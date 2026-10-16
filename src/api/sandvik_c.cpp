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
#include <string>
#include <vector>

#include "class.hpp"
#include "classloader.hpp"
#include "exceptions.hpp"
#include "method.hpp"
#include "object.hpp"
#include "system/logger.hpp"
#include "vm.hpp"

/** @brief Opaque VM handle exposed to C hosts. */
struct sv_vm {
	std::unique_ptr<sandvik::Vm> vm;
	std::string last_error;
};

namespace {
	using sandvik::ClassLoader;
	using sandvik::Object;
	using sandvik::ObjectRef;

	/** Convert a slashed class name to the dotted form used by the class loader. */
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

	/** Fill out_ from the value returned by the method. */
	void marshalReturn(ObjectRef result_, char descriptor_, sv_value* out_) {
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
				return;
			}
			const std::string classname = result_->getClass().getFullname();
			if (classname == "java.lang.Object" && result_->toString().empty()) {
				out_->kind = SV_KIND_NULL;
				return;
			}
			out_->kind = SV_KIND_OBJECT;
			out_->class_name = dupString(classname);
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
		marshalReturn(result, returnDescriptor(signature), out_);
		vm_->last_error.clear();
		return 0;
	} catch (const std::exception& e) {
		vm_->last_error = e.what();
		out_->kind = SV_KIND_NULL;
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
