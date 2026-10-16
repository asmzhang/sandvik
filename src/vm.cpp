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

#include "vm.hpp"

#include <algorithm>

#include "jni/jni.h"

#include "array.hpp"
#include "class.hpp"
#include "classbuilder.hpp"
#include "classloader.hpp"
#include "exceptions.hpp"
#include "field.hpp"
#include "frame.hpp"
#include "gc.hpp"
#include "interpreter.hpp"
#include "jni.hpp"
#include "jthread.hpp"
#include "method.hpp"
#include "monitor.hpp"
#include "object.hpp"
#include "system/logger.hpp"
#include "system/sharedlibrary.hpp"
#include "vm.hpp"

using namespace sandvik;

Vm::Vm() : _classloader(std::make_unique<ClassLoader>()), _jnienv(std::make_unique<NativeInterface>(*this)) {
	GC::getInstance().manageVm(this);
	logger.info("VM instance created.");

	ClassBuilder(*_classloader, "", "boolean").finalize();
	ClassBuilder(*_classloader, "", "byte").finalize();
	ClassBuilder(*_classloader, "", "char").finalize();
	ClassBuilder(*_classloader, "", "short").finalize();
	ClassBuilder(*_classloader, "", "int").finalize();
	ClassBuilder(*_classloader, "", "long").finalize();
	ClassBuilder(*_classloader, "", "float").finalize();
	ClassBuilder(*_classloader, "", "double").finalize();
	ClassBuilder(*_classloader, "", "void").finalize();

	// load self as a shared library to be able to resolve native methods
	loadLibrary("");
	_classloader->setVm(*this);
}

Vm::~Vm() {
	logger.debug("VM instance destroyed.");
	GC::getInstance().unmanageVm(this);
}

void Vm::loadRtByDir(const std::string& path) {
	_classloader->loadRtByDir(path);
}

void Vm::loadRtByFile(const std::string& path) {
	_classloader->loadRtByFile(path);
}

void Vm::loadDex(const std::string& path) {
	_classloader->loadDex(path);
}

void Vm::loadApk(const std::string& path) {
	_classloader->loadApk(path);
}

void Vm::addClassPath(const std::string& classpath_) {
	_classloader->addClassPath(classpath_);
}

std::string Vm::getClassPath() const {
	return _classloader->getClassPath();
}

ClassLoader& Vm::getClassLoader() const {
	return *_classloader;
}

void Vm::loadLibrary(const std::string& libName_) {
	if (!libName_.empty()) {
		const std::string requestedPath = SharedLibrary::getFullPath(libName_);
		for (const auto& loaded : _sharedlibs) {
			if (loaded->getFullPath() == requestedPath) {
				logger.fdebug("Shared library already loaded {}, skipping", requestedPath);
				return;
			}
		}
	}
	// Load the shared library
	auto lib = std::make_unique<SharedLibrary>(libName_);
	try {
		lib->load();
	} catch (const std::exception& e) {
		throw VmException("Cannot open library {} : {}", libName_, e.what());
	}
	if (lib->isLoaded()) {
		if (!libName_.empty()) {
			logger.fdebug("Loaded shared library {}", lib->getFullPath());
		}
		// Call JNI_OnLoad if it exists
		void* JNI_onLoad = lib->getAddressOfSymbol("JNI_OnLoad");
		if (JNI_onLoad) {
			// jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved)
			auto jniOnLoad = reinterpret_cast<void (*)(JavaVM*, void*)>(JNI_onLoad);
			logger.fdebug("Executing native function JNI_OnLoad@{:#x} ", (uintptr_t)jniOnLoad);
			jniOnLoad(static_cast<JavaVM*>(getJNIEnv()->getJavaVm()), nullptr);
		}
		_sharedlibs.push_back(std::move(lib));
	} else {
		throw VmException("Failed to load shared library {}", libName_);
	}
}

void* Vm::findNativeSymbol(const std::string& symbolName_) {
	for (const auto& lib : _sharedlibs) {
		void* symbol = lib->getAddressOfSymbol(symbolName_);
		if (symbol) {
			return symbol;
		}
	}
	return nullptr;
}

NativeInterface* Vm::getJNIEnv() const {
	return _jnienv.get();
}

void Vm::run() {
	auto& clazz = _classloader->getMainActivityClass();
	run(clazz, {});
}
void Vm::run(const std::string& mainClass_, const std::vector<std::string>& args_) {
	auto& clazz = _classloader->getOrLoad(mainClass_);
	run(clazz, args_);
}

void Vm::run(Class& clazz_, const std::vector<std::string>& args_) {
	logger.info("Running class: " + clazz_.getFullname());

	JThread& mainThread = newThread("main");

	// Initialize ThreadGroup for the main thread
	// The main thread needs a group before Thread.init() runs,
	// otherwise Thread.currentThread().getThreadGroup() returns null
	// and group.addUnstarted() causes a NPE.
	auto& tgClass = _classloader->getOrLoad("java.lang.ThreadGroup");
	auto mainGroup = Object::make(tgClass);
	mainGroup->setField("name", Object::make(*_classloader, "main"));
	mainThread.getThreadObject()->setField("group", mainGroup);

	uint32_t nbRegisters = 0;
	if (clazz_.hasMethod("onCreate", "(Landroid/os/Bundle;)V")) {
		// Create a new frame for the main method
		auto& method = clazz_.getMethod("onCreate", "(Landroid/os/Bundle;)V");
		mainThread.newFrame(method);
		nbRegisters = method.getNbRegisters() - 1;
	} else {
		if (clazz_.hasMethod("main", "([Ljava/lang/String;)V")) {
			// Create a new frame for the main method
			auto& method = clazz_.getMethod("main", "([Ljava/lang/String;)V");
			mainThread.newFrame(method);
			nbRegisters = method.getNbRegisters() - 1;
		} else {
			throw VmException("onCreate or main method not found in class {}", clazz_.getFullname());
		}
	}

	// Set the arguments for the main method
	const auto& stringClass = _classloader->getOrLoad("java.lang.String");
	auto args = Array::make(stringClass, (uint32_t)args_.size());
	for (size_t i = 0; i < args_.size(); ++i) {
		auto strObj = Object::make(*_classloader, args_[i]);
		args->setElement((uint32_t)i, strObj);
	}
	mainThread.currentFrame().setObjRegister(nbRegisters, args);
	try {
		mainThread.newFrame(clazz_.getMethod("<clinit>", "()V"));
	} catch (const std::exception& e) {
		logger.debug(e.what());
	}
	_isRunning.store(true);
	mainThread.run(true);
	_isRunning.store(false);
	mainThread.join();
}

ObjectRef Vm::call(Method& method_, const std::vector<ObjectRef>& args_) {
	JThread& thread = newThread("sandvik-call");

	// The worker thread needs a ThreadGroup like the main thread created in run(),
	// otherwise Thread.currentThread().getThreadGroup() returns null.
	auto& tgClass = _classloader->getOrLoad("java.lang.ThreadGroup");
	auto group = Object::make(tgClass);
	group->setField("name", Object::make(*_classloader, "sandvik-call"));
	thread.getThreadObject()->setField("group", group);

	if (args_.size() > method_.getNbArguments()) {
		throw VmException("too many arguments for method {}", method_.getName());
	}

	Frame& frame = thread.newFrame(method_);
	if (!args_.empty()) {
		// the arguments of a method live in its last registers
		const uint32_t first = method_.getNbRegisters() - static_cast<uint32_t>(args_.size());
		for (uint32_t i = 0; i < args_.size(); ++i) {
			frame.setObjRegister(first + i, args_[i]);
		}
	}

	// push the static initializer on top of the stack so that it runs first
	auto& clazz = method_.getClass();
	if (!clazz.isStaticInitialized() && clazz.hasMethod("<clinit>", "()V")) {
		try {
			thread.newFrame(clazz.getMethod("<clinit>", "()V"));
		} catch (const std::exception& e) {
			logger.debug(e.what());
		}
	}

	const std::string name = thread.getName();
	_isRunning.store(true);
	thread.run(true);
	_isRunning.store(false);
	thread.join();

	ObjectRef result = thread.getReturnObject();
	deleteThread(name);
	return result;
}

void Vm::stop() {
	logger.info("Stopping VM...");
	_isRunning.store(false);
}

JThread& Vm::newThread(const std::string& name_) {
	std::unique_lock lock(_mutex);
	_threads.emplace_back(std::make_unique<JThread>(*this, *_classloader, name_));
	return *(_threads.back());
}

JThread& Vm::newThread(ObjectRef thread_) {
	std::unique_lock lock(_mutex);
	_threads.emplace_back(std::make_unique<JThread>(*this, *_classloader, thread_));
	return *(_threads.back());
}

JThread& Vm::getThread(const std::string& name_) {
	std::unique_lock lock(_mutex);
	for (const auto& thread : _threads) {
		if (thread->getName() == name_) {
			return *thread;
		}
	}
	throw VmException("Thread with name '{}' not found", name_);
}

JThread& Vm::currentThread() const {
	std::unique_lock lock(_mutex);
	auto currentId = std::this_thread::get_id();
	for (const auto& thread : _threads) {
		if (thread->getId() == currentId) {
			return *thread;
		}
	}
	throw VmException("Current thread not found in VM");
}

void Vm::deleteThread(const std::string& name_) {
	std::unique_lock lock(_mutex);
	for (auto it = _threads.begin(); it != _threads.end(); ++it) {
		if ((*it)->getName() == name_) {
			_threads.erase(it);
			return;
		}
	}
}

std::string Vm::getProperty(const std::string& name_) const {
	auto it = _properties.find(name_);
	if (it != _properties.end()) {
		return it->second;
	}
	throw VmException("Property '{}' not found", name_);
}

void Vm::setProperty(const std::string& name_, const std::string& value_) {
	_properties[name_] = value_;
}

void Vm::visitReferences(const std::function<void(Object*)>& visitor_) const {
	{
		std::lock_guard<std::mutex> lock(_rootsMutex);
		for (Object* root : _roots) {
			if (root != nullptr) {
				visitor_(root);
			}
		}
	}
	_classloader->visitReferences(visitor_);
	for (const auto& thread : _threads) {
		thread->visitReferences(visitor_);
	}
}

void Vm::addRoot(Object* obj_) {
	if (obj_ == nullptr) {
		return;
	}
	std::lock_guard<std::mutex> lock(_rootsMutex);
	if (std::find(_roots.begin(), _roots.end(), obj_) == _roots.end()) {
		_roots.push_back(obj_);
	}
}

void Vm::removeRoot(Object* obj_) {
	if (obj_ == nullptr) {
		return;
	}
	std::lock_guard<std::mutex> lock(_rootsMutex);
	_roots.erase(std::remove(_roots.begin(), _roots.end(), obj_), _roots.end());
}

void Vm::suspend() {
	// If VM not running, nothing to do
	if (_isRunning.load() == false) {
		return;
	}
	bool all_suspended = true;
	do {
		all_suspended = true;
		// Copy pointers to threads while holding the lock, then release the lock
		std::vector<JThread*> threads_to_suspend;
		{
			std::unique_lock lock(_mutex);
			threads_to_suspend.reserve(_threads.size());
			for (const auto& thread : _threads) {
				if (thread->getState() == Thread::ThreadState::Stopped) {
					continue;
				}
				threads_to_suspend.push_back(thread.get());
			}
		}
		// Suspend each thread without holding the VM mutex to avoid deadlock with thread creation
		for (auto thread : threads_to_suspend) {
			thread->suspend();
		}
		// check if all threads are suspended
		{
			std::unique_lock lock(_mutex);
			for (const auto& thread : _threads) {
				if (thread->getState() == Thread::ThreadState::Stopped) {
					continue;
				}
				if (thread->getState() != Thread::ThreadState::Suspended) {
					all_suspended = false;
					break;
				}
			}
		}
	} while (!all_suspended);

	// take the opportunity to clean stopped threads while world is stopped
	std::unique_lock lock(_mutex);
	for (auto it = _threads.begin(); it != _threads.end(); ++it) {
		if ((*it)->getState() == Thread::ThreadState::Stopped) {
			_threads.erase(it);
			return;
		}
	}
}

void Vm::resume() {
	if (_isRunning.load() == false) {
		return;
	}
	// world is fully stopped, resume all threads
	std::unique_lock lock(_mutex);
	for (const auto& thread : _threads) {
		thread->resume();
	}
}
