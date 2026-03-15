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

#include "sharedlibrary.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <fmt/format.h>

#include <filesystem>
#include <sstream>

#include "env_var.hpp"

using namespace sandvik;

SharedLibrary::SharedLibrary(const std::string& name_) : _path(name_), _handle(nullptr) {
}

SharedLibrary::~SharedLibrary() {
	unload();
}

std::string SharedLibrary::getFullPath(const std::string& name_) {
	if (name_.empty()) {
		return "";
	}
	std::string ldLibraryPath = system::env::get("LD_LIBRARY_PATH");
	std::istringstream pathStream(ldLibraryPath);
	std::string path;

	while (std::getline(pathStream, path, ':')) {
		std::filesystem::path p(path);
		p /= name_;
		if (std::filesystem::exists(p)) {
			return p.string();
		}
	}

	std::filesystem::path p(name_);
	if (p.is_relative()) {
		p = std::filesystem::absolute(p);
	}
	if (std::filesystem::exists(p)) {
		return p.string();
	}

	return name_;
}

std::string SharedLibrary::getFullPath() const {
	return SharedLibrary::getFullPath(_path);
}

void SharedLibrary::load() {
#ifdef _WIN32
	if (_path.empty()) {
		_handle = (void*)GetModuleHandleA("sandvik.dll");
		if (!_handle) {
			_handle = (void*)LoadLibraryA("sandvik.dll");
		}
		if (!_handle) {
			throw std::runtime_error(fmt::format("Cannot load sandvik.dll : Error {}", GetLastError()));
		}
	} else {
		_handle = (void*)LoadLibraryA(_path.c_str());
		if (!_handle) {
			throw std::runtime_error(fmt::format("Cannot open library {} : Error {}", _path, GetLastError()));
		}
	}
#else
	if (_path.empty()) {
		_handle = dlopen(nullptr, RTLD_NOW | RTLD_LOCAL);
	} else {
		_handle = dlopen(_path.c_str(), RTLD_NOW | RTLD_LOCAL);
	}
	if (!_handle) {
		throw std::runtime_error(fmt::format("Cannot open library {} : {}", _path, dlerror()));
	}
#endif
}

void SharedLibrary::unload() {
	if (_handle) {
#ifdef _WIN32
		FreeLibrary((HMODULE)_handle);
#else
		dlclose(_handle);  // Ignoring errors in destructor (optional)
#endif
		_handle = nullptr;
	}
}

bool SharedLibrary::isLoaded() const {
	return (_handle != nullptr);
}

void* SharedLibrary::getAddressOfSymbol(const std::string& name_) {
#ifdef _WIN32
	return (void*)GetProcAddress((HMODULE)_handle, name_.c_str());
#else
	return dlsym(_handle, name_.c_str());
#endif
}
