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

#include "rtld.hpp"

#include <string>
#include <vector>

#include "class.hpp"
#include "dex.hpp"
#include "exceptions.hpp"
#include "system/logger.hpp"
#include "system/zip.hpp"

#include "resource_paths.h"

using namespace sandvik;

/** Constructor: Loads the JAR file */
void rtld::load(const std::string& path_, std::vector<std::unique_ptr<Dex>>& dexs_) {
	auto zip = std::make_unique<ZipReader>();
	if (path_.empty()) {
		// 使用直接配置的资源路径，而不是嵌入式资源
		std::string resource_path = get_sanddirt_dex_jar_path();
		if (!ZipReader::isValidArchive(resource_path)) {
			throw VmException("Invalid internal RT file: {}", resource_path);
		}
		zip->open(resource_path);
	} else {
		if (!ZipReader::isValidArchive(path_)) {
			throw VmException("Invalid RT file: {}", path_);
		}
		zip->open(path_);
	}

	// load all *.dex files
	for (const auto& file : zip->getList()) {
		if (file.size() >= 4 && file.ends_with(".dex")) {
			uint64_t size = 0;
			auto buffer = zip->extractToMemory(file, size);
			if (!buffer) {
				throw VmException("Failed to extract {}", file);
			}
			std::vector<uint8_t> dexBuffer(buffer.get(), buffer.get() + size);
			dexs_.push_back(std::make_unique<Dex>(dexBuffer, path_.empty() ? "<sandvik>" : path_));
		}
	}
	zip->close();
}