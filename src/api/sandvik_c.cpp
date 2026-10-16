/*
 * This file is part of Sandvik project.
 * Copyright (C) 2025 Christophe Duvernois
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

/**
 * @file sandvik_c.cpp
 * @brief Implementation of the embedding API declared in sandvik_c.h.
 *
 * This translation unit is the only place where the C API meets the C++ VM, so
 * no other file has to care about ObjectRef/Method/Vm lifetime. It must not
 * include a real <jni.h>: sandvik has its own JNI dialect (src/jni/jni.h) and
 * mixing the two in one translation unit would clash on JavaVM/JNIEnv.
 */

#include "sandvik_c.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "array.hpp"
#include "class.hpp"
#include "classloader.hpp"
#include "exceptions.hpp"
#include "gc.hpp"
#include "method.hpp"
#include "object.hpp"
#include "system/logger.hpp"
#include "vm.hpp"

/** @brief Opaque VM handle exposed to C hosts. */
struct sv_vm {
	std::unique_ptr<sandvik::Vm> vm;
	std::string last_error;
	/** handle table: one id per VM object (identity preserving) */
	std::unordered_map<void*, int64_t> refs_by_ptr;
	std::unordered_map<int64_t, void*> refs_by_id;
	/** what a handle addresses: 0 = object, 1 = resolved Class, 2 = java.lang.Class object */
	std::unordered_map<int64_t, int32_t> refs_kind;
	int64_t next_ref = 1;
	/** last string handed out by sv_vm_object_class()/sv_vm_object_string() */
	std::string scratch;
};

namespace {
	using sandvik::ClassLoader;
	using sandvik::Object;
	using sandvik::ObjectRef;

	/** Kinds stored in sv_vm::refs_kind. */
	constexpr int32_t KIND_OBJECT = 0;
	constexpr int32_t KIND_CLASS = 1;
	constexpr int32_t KIND_CLASS_OBJECT = 2;

	/** Intern a VM pointer, reusing the existing id so identity survives a round trip.
	 *
	 * Object handles are pinned as GC roots: the collector only walks the class
	 * loader, the threads and the VM root set, so an object reachable solely
	 * through this table would be swept and the handle would dangle.
	 */
	sv_handle internRef(sv_vm* vm_, void* ptr_, int32_t kind_) {
		if (vm_ == nullptr || ptr_ == nullptr) {
			return 0;
		}
		const auto known = vm_->refs_by_ptr.find(ptr_);
		if (known != vm_->refs_by_ptr.end()) {
			return known->second;
		}
		const int64_t id = vm_->next_ref++;
		vm_->refs_by_ptr[ptr_] = id;
		vm_->refs_by_id[id] = ptr_;
		vm_->refs_kind[id] = kind_;
		if (kind_ != KIND_CLASS) {
			vm_->vm->addRoot(static_cast<Object*>(ptr_));
		}
		return id;
	}

	/** Object behind a handle, or nullptr (a resolved Class addresses no object). */
	Object* objectFromHandle(sv_vm* vm_, sv_handle handle_) {
		if (vm_ == nullptr || handle_ == 0) {
			return nullptr;
		}
		const auto it = vm_->refs_by_id.find(handle_);
		if (it == vm_->refs_by_id.end()) {
			return nullptr;
		}
		const auto kind = vm_->refs_kind.find(handle_);
		if (kind != vm_->refs_kind.end() && kind->second == KIND_CLASS) {
			return nullptr;
		}
		return static_cast<Object*>(it->second);
	}

	/** Class behind a handle: a resolved class, or the class denoted by a Class object. */
	sandvik::Class* classFromHandle(sv_vm* vm_, sv_handle handle_) {
		if (vm_ == nullptr || handle_ == 0) {
			return nullptr;
		}
		const auto it = vm_->refs_by_id.find(handle_);
		if (it == vm_->refs_by_id.end()) {
			return nullptr;
		}
		const auto kind = vm_->refs_kind.find(handle_);
		const int32_t kindValue = kind == vm_->refs_kind.end() ? KIND_OBJECT : kind->second;
		if (kindValue == KIND_CLASS) {
			return static_cast<sandvik::Class*>(it->second);
		}
		if (kindValue == KIND_CLASS_OBJECT) {
			// ConstClassObject::getClassType() is the class the object denotes.
			auto* obj = static_cast<Object*>(it->second);
			return obj != nullptr ? const_cast<sandvik::Class*>(&obj->getClassType()) : nullptr;
		}
		// Array::isClass() is true as well (arrays carry a class type), so an
		// array must never be mistaken for a java.lang.Class object.
		auto* obj = static_cast<Object*>(it->second);
		if (obj != nullptr && obj->isClass() && !obj->isArray()) {
			return const_cast<sandvik::Class*>(&obj->getClassType());
		}
		return nullptr;
	}

	/** Kind of a freshly interned object pointer. */
	int32_t kindOfObject(const Object* obj_) {
		if (obj_ == nullptr) {
			return KIND_OBJECT;
		}
		return (obj_->isClass() && !obj_->isArray()) ? KIND_CLASS_OBJECT : KIND_OBJECT;
	}


	/**
	 * Convert a slashed class name to the dotted form used by the class loader.
	 */
	std::string dotted(const char* name_) {
		std::string out = name_ != nullptr ? name_ : "";
		for (auto& c : out) {
			if (c == '/') {
				c = '.';
			}
		}
		return out;
	}

	/** Return descriptor of a JNI signature ("(I)Ljava/lang/String;" -> 'L'). */
	char returnDescriptor(const std::string& signature_) {
		const auto pos = signature_.rfind(')');
		if (pos == std::string::npos || pos + 1 >= signature_.size()) {
			return 'V';
		}
		return signature_[pos + 1];
	}

	char* dupString(const std::string& text_) {
		char* out = static_cast<char*>(std::malloc(text_.size() + 1));
		if (out == nullptr) {
			return nullptr;
		}
		std::memcpy(out, text_.c_str(), text_.size() + 1);
		return out;
	}

	/** Materialize one argument token (see sandvik_c.h for the syntax). */
	ObjectRef makeArgument(ClassLoader& classloader_, const std::string& token_) {
		if (token_ == "n:" || token_ == "null") {
			return Object::makeNull();
		}
		if (token_.rfind("s:", 0) == 0) {
			return Object::make(classloader_, token_.substr(2));
		}
		if (token_.rfind("i:", 0) == 0) {
			const auto value = static_cast<int32_t>(std::stol(token_.substr(2)));
			return Object::make(static_cast<uint64_t>(static_cast<uint32_t>(value)));
		}
		if (token_.rfind("l:", 0) == 0) {
			return Object::make(static_cast<uint64_t>(std::stoll(token_.substr(2))));
		}
		if (token_.rfind("d:", 0) == 0) {
			const double value = std::stod(token_.substr(2));
			uint64_t bits = 0;
			std::memcpy(&bits, &value, sizeof(bits));
			return Object::make(bits);
		}
		return Object::make(classloader_, token_);
	}

	/** Materialize one sv_arg: either a typed token or a handle of the VM. */
	ObjectRef makeArg(sv_vm* vm_, ClassLoader& classloader_, const sv_arg& arg_) {
		if (arg_.kind != SV_ARG_HANDLE) {
			return makeArgument(classloader_, arg_.token != nullptr ? arg_.token : "n:");
		}
		if (arg_.handle == 0) {
			return Object::makeNull();
		}
		const auto it = vm_->refs_by_id.find(arg_.handle);
		if (it == vm_->refs_by_id.end()) {
			throw std::runtime_error("unknown handle passed as argument");
		}
		const auto kind = vm_->refs_kind.find(arg_.handle);
		if (kind != vm_->refs_kind.end() && kind->second == KIND_CLASS) {
			return Object::makeConstClass(classloader_, *static_cast<sandvik::Class*>(it->second));
		}
		return static_cast<Object*>(it->second);
	}

	/** Fill out_ from the value returned by the method. */
	void marshalReturn(sv_vm* vm_, ObjectRef result_, char descriptor_, sv_value* out_) {
		switch (descriptor_) {
			case 'V':
				out_->kind = SV_KIND_NULL;
				return;
			case 'Z':
			case 'B':
			case 'C':
			case 'S':
			case 'I':
				out_->kind = descriptor_ == 'Z' ? SV_KIND_BOOL : SV_KIND_INT;
				out_->int_value = result_ != nullptr ? result_->getValue() : 0;
				return;
			case 'J':
				out_->kind = SV_KIND_LONG;
				out_->long_value = result_ != nullptr ? result_->getLongValue() : 0;
				return;
			case 'F': {
				out_->kind = SV_KIND_DOUBLE;
				const uint32_t bits = result_ != nullptr ? static_cast<uint32_t>(result_->getValue()) : 0;
				float value = 0.F;
				std::memcpy(&value, &bits, sizeof(value));
				out_->double_value = static_cast<double>(value);
				return;
			}
			case 'D': {
				out_->kind = SV_KIND_DOUBLE;
				const uint64_t bits = result_ != nullptr ? static_cast<uint64_t>(result_->getLongValue()) : 0;
				double value = 0.;
				std::memcpy(&value, &bits, sizeof(value));
				out_->double_value = value;
				return;
			}
			default:
				break;
		}
		// object (or array) return
		if (result_ == nullptr) {
			out_->kind = SV_KIND_NULL;
			return;
		}
		try {
			if (result_->isString()) {
				out_->kind = SV_KIND_STRING;
				out_->string_value = dupString(result_->str());
				out_->handle = internRef(vm_, result_, false);
				return;
			}
			const std::string classname = result_->getClass().getFullname();
			if (classname == "java.lang.Object" && result_->toString().empty()) {
				out_->kind = SV_KIND_NULL;
				return;
			}
			out_->kind = SV_KIND_OBJECT;
			out_->class_name = dupString(classname);
			// Array::isClass() is true as well, so only a non-array object can be
			// interned as a class handle.
			out_->handle = internRef(vm_, result_, result_->isClass() && !result_->isArray());
		} catch (const std::exception&) {
			out_->kind = SV_KIND_NULL;
		}
	}
}  // namespace

extern "C" {

sv_vm* sv_vm_create(void) {
	try {
		auto handle = std::make_unique<sv_vm>();
		handle->vm = std::make_unique<sandvik::Vm>();
		return handle.release();
	} catch (const std::exception&) {
		return nullptr;
	}
}

void sv_vm_destroy(sv_vm* vm_) {
	delete vm_;
}

static void apply_log_level(int32_t level_) {
	switch (level_) {
		case 0:
			logger.setLevel(sandvik::Logger::LogLevel::NONE);
			break;
		case 3:
			logger.setLevel(sandvik::Logger::LogLevel::WARNING);
			break;
		case 4:
			logger.setLevel(sandvik::Logger::LogLevel::INFO);
			break;
		case 5:
			logger.setLevel(sandvik::Logger::LogLevel::DEBUG);
			break;
		default:
			logger.setLevel(sandvik::Logger::LogLevel::ERROR);
			break;
	}
}

void sv_vm_set_log_level(sv_vm* vm_, int32_t level_) {
	if (vm_ == nullptr) {
		return;
	}
	apply_log_level(level_);
}

void sv_set_log_level(int32_t level_) {
	apply_log_level(level_);
}

int32_t sv_vm_load_rt_dir(sv_vm* vm_, const char* dir_) {
	if (vm_ == nullptr || dir_ == nullptr) {
		return -1;
	}
	try {
		vm_->vm->loadRtByDir(dir_);
		return 0;
	} catch (const std::exception& e) {
		vm_->last_error = e.what();
		return -1;
	}
}

int32_t sv_vm_load_rt_file(sv_vm* vm_, const char* file_) {
	if (vm_ == nullptr || file_ == nullptr) {
		return -1;
	}
	try {
		vm_->vm->loadRtByFile(file_);
		return 0;
	} catch (const std::exception& e) {
		vm_->last_error = e.what();
		return -1;
	}
}

int32_t sv_vm_load_dex(sv_vm* vm_, const char* path_) {
	if (vm_ == nullptr || path_ == nullptr) {
		return -1;
	}
	try {
		vm_->vm->loadDex(path_);
		return 0;
	} catch (const std::exception& e) {
		vm_->last_error = e.what();
		return -1;
	}
}

int32_t sv_vm_load_apk(sv_vm* vm_, const char* path_) {
	if (vm_ == nullptr || path_ == nullptr) {
		return -1;
	}
	try {
		vm_->vm->loadApk(path_);
		return 0;
	} catch (const std::exception& e) {
		vm_->last_error = e.what();
		return -1;
	}
}

int32_t sv_vm_add_class_path(sv_vm* vm_, const char* path_) {
	if (vm_ == nullptr || path_ == nullptr) {
		return -1;
	}
	try {
		vm_->vm->addClassPath(path_);
		return 0;
	} catch (const std::exception& e) {
		vm_->last_error = e.what();
		return -1;
	}
}

int32_t sv_vm_set_property(sv_vm* vm_, const char* name_, const char* value_) {
	if (vm_ == nullptr || name_ == nullptr || value_ == nullptr) {
		return -1;
	}
	try {
		vm_->vm->setProperty(name_, value_);
		return 0;
	} catch (const std::exception& e) {
		vm_->last_error = e.what();
		return -1;
	}
}

const char* sv_vm_get_property(sv_vm* vm_, const char* name_) {
	if (vm_ == nullptr || name_ == nullptr) {
		return nullptr;
	}
	try {
		vm_->last_error = vm_->vm->getProperty(name_);
		return vm_->last_error.c_str();
	} catch (const std::exception& e) {
		vm_->last_error = e.what();
		return nullptr;
	}
}

int32_t sv_vm_call_static(sv_vm* vm_, const char* class_name_, const char* method_, const char* signature_,
                          const char* const* args_, int32_t argc_, sv_value* out_) {
	if (vm_ == nullptr || class_name_ == nullptr || method_ == nullptr || signature_ == nullptr || out_ == nullptr || argc_ < 0) {
		return -1;
	}
	std::memset(out_, 0, sizeof(*out_));
	try {
		auto& classloader = vm_->vm->getClassLoader();
		const std::string classname = dotted(class_name_);
		const std::string name = method_;
		const std::string signature = signature_;
		auto& clazz = classloader.getOrLoad(classname);
		if (!clazz.hasMethod(name, signature)) {
			vm_->last_error = "method not found: " + classname + "." + name + signature;
			return -1;
		}
		auto& method = clazz.getMethod(name, signature);
		std::vector<ObjectRef> args;
		args.reserve(static_cast<size_t>(argc_));
		for (int32_t i = 0; i < argc_; ++i) {
			if (args_ == nullptr || args_[i] == nullptr) {
				args.push_back(Object::makeNull());
				continue;
			}
			args.push_back(makeArgument(classloader, args_[i]));
		}
		ObjectRef result = vm_->vm->call(method, args);
		marshalReturn(vm_, result, returnDescriptor(signature), out_);
		vm_->last_error.clear();
		return 0;
	} catch (const std::exception& e) {
		vm_->last_error = e.what();
		out_->kind = SV_KIND_NULL;
		return -1;
	}
}

sv_handle sv_vm_resolve_class(sv_vm* vm_, const char* class_name_) {
	if (vm_ == nullptr || class_name_ == nullptr) {
		return 0;
	}
	try {
		auto& clazz = vm_->vm->getClassLoader().getOrLoad(dotted(class_name_));
		vm_->last_error.clear();
		return internRef(vm_, &clazz, KIND_CLASS);
	} catch (const std::exception& e) {
		vm_->last_error = e.what();
		return 0;
	}
}

sv_handle sv_vm_new_string(sv_vm* vm_, const char* utf8_) {
	if (vm_ == nullptr || utf8_ == nullptr) {
		return 0;
	}
	try {
		auto ref = Object::make(vm_->vm->getClassLoader(), utf8_);
		vm_->last_error.clear();
		return internRef(vm_, ref, kindOfObject(ref));
	} catch (const std::exception& e) {
		vm_->last_error = e.what();
		return 0;
	}
}

int32_t sv_vm_call_static_handle(sv_vm* vm_, sv_handle class_, const char* method_, const char* signature_,
                                 const sv_arg* args_, int32_t argc_, sv_value* out_) {
	if (vm_ == nullptr || method_ == nullptr || signature_ == nullptr || out_ == nullptr || argc_ < 0) {
		return -1;
	}
	std::memset(out_, 0, sizeof(*out_));
	try {
		auto* clazz = classFromHandle(vm_, class_);
		if (clazz == nullptr) {
			vm_->last_error = "invalid class handle";
			return -1;
		}
		const std::string name = method_;
		const std::string signature = signature_;
		if (!clazz->hasMethod(name, signature)) {
			vm_->last_error = "method not found: " + clazz->getName() + "." + name + signature;
			return -1;
		}
		auto& method = clazz->getMethod(name, signature);
		auto& classloader = vm_->vm->getClassLoader();
		std::vector<ObjectRef> args;
		args.reserve(static_cast<size_t>(argc_));
		for (int32_t i = 0; i < argc_; ++i) {
			args.push_back(makeArg(vm_, classloader, args_[i]));
		}
		ObjectRef result = vm_->vm->call(method, args);
		marshalReturn(vm_, result, returnDescriptor(signature), out_);
		vm_->last_error.clear();
		return 0;
	} catch (const std::exception& e) {
		vm_->last_error = e.what();
		out_->kind = SV_KIND_NULL;
		return -1;
	}
}

const char* sv_vm_object_class(sv_vm* vm_, sv_handle handle_) {
	if (vm_ == nullptr) {
		return nullptr;
	}
	try {
		const auto kind = vm_->refs_kind.find(handle_);
		if (kind != vm_->refs_kind.end() && kind->second == KIND_CLASS) {
			auto* clazz = classFromHandle(vm_, handle_);
			if (clazz == nullptr) {
				return nullptr;
			}
			vm_->scratch = clazz->getFullname();
		} else {
			auto* obj = objectFromHandle(vm_, handle_);
			if (obj == nullptr) {
				return nullptr;
			}
			vm_->scratch = obj->getClass().getFullname();
		}
		return vm_->scratch.c_str();
	} catch (const std::exception& e) {
		vm_->last_error = e.what();
		return nullptr;
	}
}

const char* sv_vm_object_string(sv_vm* vm_, sv_handle handle_) {
	if (vm_ == nullptr || handle_ == 0) {
		return nullptr;
	}
	try {
		auto* obj = objectFromHandle(vm_, handle_);
		if (obj == nullptr) {
			auto* clazz = classFromHandle(vm_, handle_);
			if (clazz == nullptr) {
				return nullptr;
			}
			vm_->scratch = clazz->getFullname();
			return vm_->scratch.c_str();
		}
		vm_->scratch = obj->isString() ? obj->str() : obj->toString();
		return vm_->scratch.c_str();
	} catch (const std::exception& e) {
		vm_->last_error = e.what();
		return nullptr;
	}
}

int32_t sv_vm_object_is_string(sv_vm* vm_, sv_handle handle_) {
	auto* obj = objectFromHandle(vm_, handle_);
	return (obj != nullptr && obj->isString()) ? 1 : 0;
}

int32_t sv_vm_object_is_null(sv_vm* vm_, sv_handle handle_) {
	if (vm_ == nullptr || handle_ == 0) {
		return 1;
	}
	auto* obj = objectFromHandle(vm_, handle_);
	return (obj != nullptr && obj->isNull()) ? 1 : 0;
}

int32_t sv_vm_object_is_class(sv_vm* vm_, sv_handle handle_) {
	return classFromHandle(vm_, handle_) != nullptr ? 1 : 0;
}

int32_t sv_vm_object_is_array(sv_vm* vm_, sv_handle handle_) {
	auto* obj = objectFromHandle(vm_, handle_);
	return (obj != nullptr && obj->isArray()) ? 1 : 0;
}

int32_t sv_vm_object_array_length(sv_vm* vm_, sv_handle handle_) {
	auto* obj = objectFromHandle(vm_, handle_);
	if (obj == nullptr || !obj->isArray()) {
		return -1;
	}
	try {
		return static_cast<int32_t>(obj->getArrayLength());
	} catch (const std::exception& e) {
		vm_->last_error = e.what();
		return -1;
	}
}

sv_handle sv_vm_object_element(sv_vm* vm_, sv_handle handle_, int32_t index_) {
	if (vm_ == nullptr || index_ < 0) {
		return 0;
	}
	try {
		auto* array = dynamic_cast<sandvik::Array*>(objectFromHandle(vm_, handle_));
		if (array == nullptr) {
			vm_->last_error = "handle is not an array";
			return 0;
		}
		auto element = array->getElement(static_cast<uint32_t>(index_));
		vm_->last_error.clear();
		// an element that is itself an array must stay an object handle, and a
		// Class object must not be interned as a resolved Class (type confusion)
		return internRef(vm_, element, kindOfObject(element));
	} catch (const std::exception& e) {
		vm_->last_error = e.what();
		return 0;
	}
}

int32_t sv_vm_handle_count(sv_vm* vm_) {
	if (vm_ == nullptr) {
		return 0;
	}
	return static_cast<int32_t>(vm_->refs_by_id.size());
}

void sv_vm_release_handle(sv_vm* vm_, sv_handle handle_) {
	if (vm_ == nullptr || handle_ == 0) {
		return;
	}
	const auto it = vm_->refs_by_id.find(handle_);
	if (it == vm_->refs_by_id.end()) {
		return;
	}
	const auto kind = vm_->refs_kind.find(handle_);
	if (kind != vm_->refs_kind.end() && kind->second != KIND_CLASS && it->second != nullptr) {
		// drop the GC root this handle was holding
		vm_->vm->removeRoot(static_cast<Object*>(it->second));
	}
	vm_->refs_by_ptr.erase(it->second);
	vm_->refs_by_id.erase(it);
	vm_->refs_kind.erase(handle_);
}

int32_t sv_gc_start(void) {
	// The Vm constructor already starts the collector thread (GC::manageVm runs it
	// for the first VM), and it only sweeps when asked: this entry is an explicit,
	// idempotent guarantee for a host that wants collections before requesting one.
	static std::mutex startMutex;
	std::lock_guard<std::mutex> lock(startMutex);
	static bool started = false;
	if (started) {
		return 0;
	}
	try {
		sandvik::GC::getInstance().run(false);
		started = true;
		return 0;
	} catch (const std::exception&) {
		return -1;
	}
}

int32_t sv_gc_request_collect(void) {
	try {
		sandvik::GC::getInstance().requestCollect();
		return 0;
	} catch (const std::exception&) {
		return -1;
	}
}

uint64_t sv_gc_cycles(void) {
	return sandvik::GC::getInstance().getGcCycles();
}

uint64_t sv_gc_tracked_count(void) {
	return sandvik::GC::getInstance().getTrackedObjectCount();
}

int32_t sv_gc_probe_unrooted_string(sv_vm* vm_, const char* utf8_) {
	if (vm_ == nullptr || utf8_ == nullptr) {
		return -1;
	}
	try {
		// Diagnostic hook: allocate a String that no handle and no VM structure
		// references, so the next collection has a provably unreachable object to
		// sweep. Used by the bridge tests to show the collector really runs.
		ObjectRef probe = Object::make(vm_->vm->getClassLoader(), utf8_);
		(void)probe;
		return 0;
	} catch (const std::exception& e) {
		vm_->last_error = e.what();
		return -1;
	}
}

void sv_value_dispose(sv_value* value_) {
	if (value_ == nullptr) {
		return;
	}
	std::free(value_->string_value);
	std::free(value_->class_name);
	std::memset(value_, 0, sizeof(*value_));
}

const char* sv_vm_last_error(sv_vm* vm_) {
	if (vm_ == nullptr || vm_->last_error.empty()) {
		return nullptr;
	}
	return vm_->last_error.c_str();
}

}  // extern "C"
