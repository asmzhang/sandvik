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
 * @file sandvik_jni.cpp
 * @brief JNI entry points used when sandvik is embedded in a host JVM.
 *
 * This is the "sandvik as a library" side of the bridge described in
 * sandvik/UNIDBG_EVAL.md: a host JVM (a plain java launcher, an unidbg
 * SandvikBridge, an IDE plugin, ...) loads libsandvik and drives the VM through
 * org.sandvik.Native.
 *
 * The translation unit deliberately includes the *real* <jni.h> and only the
 * plain C embedding API: sandvik's own JNI dialect (src/jni/jni.h) must never be
 * visible here, the two define different JavaVM/JNIEnv types.
 */

#include <jni.h>

#include <string>
#include <vector>

#include "sandvik_c.h"

namespace {
	/** Read a jstring as std::string (UTF-8, empty when the reference is null). */
	std::string toStdString(JNIEnv* env_, jstring value_) {
		if (value_ == nullptr) {
			return {};
		}
		const char* chars = env_->GetStringUTFChars(value_, nullptr);
		if (chars == nullptr) {
			return {};
		}
		std::string out = chars;
		env_->ReleaseStringUTFChars(value_, chars);
		return out;
	}

	/** Build the Object[] { kind, text, number, className } returned to Java. */
	jobjectArray makeResult(JNIEnv* env_, const sv_value& value_) {
		jclass objectClass = env_->FindClass("java/lang/Object");
		jclass integerClass = env_->FindClass("java/lang/Integer");
		jclass longClass = env_->FindClass("java/lang/Long");
		if (objectClass == nullptr || integerClass == nullptr || longClass == nullptr) {
			return nullptr;
		}
		jobjectArray result = env_->NewObjectArray(4, objectClass, nullptr);
		if (result == nullptr) {
			return nullptr;
		}
		jmethodID integerValueOf = env_->GetStaticMethodID(integerClass, "valueOf", "(I)Ljava/lang/Integer;");
		jmethodID longValueOf = env_->GetStaticMethodID(longClass, "valueOf", "(J)Ljava/lang/Long;");
		if (integerValueOf == nullptr || longValueOf == nullptr) {
			return nullptr;
		}
		env_->SetObjectArrayElement(result, 0, env_->CallStaticObjectMethod(integerClass, integerValueOf, static_cast<jint>(value_.kind)));
		if (value_.string_value != nullptr) {
			env_->SetObjectArrayElement(result, 1, env_->NewStringUTF(value_.string_value));
		} else if (value_.class_name != nullptr) {
			env_->SetObjectArrayElement(result, 1, env_->NewStringUTF(value_.class_name));
		}
		switch (value_.kind) {
			case SV_KIND_INT:
			case SV_KIND_BOOL:
				env_->SetObjectArrayElement(result, 2, env_->CallStaticObjectMethod(longClass, longValueOf, static_cast<jlong>(value_.int_value)));
				break;
			case SV_KIND_LONG:
				env_->SetObjectArrayElement(result, 2, env_->CallStaticObjectMethod(longClass, longValueOf, static_cast<jlong>(value_.long_value)));
				break;
			case SV_KIND_DOUBLE:
				env_->SetObjectArrayElement(result, 2, env_->CallStaticObjectMethod(longClass, longValueOf, static_cast<jlong>(value_.double_value)));
				break;
			default:
				break;
		}
		return result;
	}
}  // namespace

extern "C" {

JNIEXPORT jlong JNICALL Java_org_sandvik_Native_create(JNIEnv* env_, jclass) {
	return reinterpret_cast<jlong>(sv_vm_create());
}

JNIEXPORT void JNICALL Java_org_sandvik_Native_destroy(JNIEnv* env_, jclass, jlong handle_) {
	sv_vm_destroy(reinterpret_cast<sv_vm*>(handle_));
}

JNIEXPORT void JNICALL Java_org_sandvik_Native_setLogLevel(JNIEnv* env_, jclass, jlong handle_, jint level_) {
	sv_vm_set_log_level(reinterpret_cast<sv_vm*>(handle_), level_);
}

JNIEXPORT void JNICALL Java_org_sandvik_Native_setGlobalLogLevel(JNIEnv* env_, jclass, jint level_) {
	sv_set_log_level(level_);
}

JNIEXPORT jint JNICALL Java_org_sandvik_Native_loadRtDir(JNIEnv* env_, jclass, jlong handle_, jstring dir_) {
	return sv_vm_load_rt_dir(reinterpret_cast<sv_vm*>(handle_), toStdString(env_, dir_).c_str());
}

JNIEXPORT jint JNICALL Java_org_sandvik_Native_loadRtFile(JNIEnv* env_, jclass, jlong handle_, jstring file_) {
	return sv_vm_load_rt_file(reinterpret_cast<sv_vm*>(handle_), toStdString(env_, file_).c_str());
}

JNIEXPORT jint JNICALL Java_org_sandvik_Native_loadDex(JNIEnv* env_, jclass, jlong handle_, jstring path_) {
	return sv_vm_load_dex(reinterpret_cast<sv_vm*>(handle_), toStdString(env_, path_).c_str());
}

JNIEXPORT jint JNICALL Java_org_sandvik_Native_loadApk(JNIEnv* env_, jclass, jlong handle_, jstring path_) {
	return sv_vm_load_apk(reinterpret_cast<sv_vm*>(handle_), toStdString(env_, path_).c_str());
}

JNIEXPORT jint JNICALL Java_org_sandvik_Native_addClassPath(JNIEnv* env_, jclass, jlong handle_, jstring path_) {
	return sv_vm_add_class_path(reinterpret_cast<sv_vm*>(handle_), toStdString(env_, path_).c_str());
}

JNIEXPORT jint JNICALL Java_org_sandvik_Native_setProperty(JNIEnv* env_, jclass, jlong handle_, jstring name_, jstring value_) {
	return sv_vm_set_property(reinterpret_cast<sv_vm*>(handle_), toStdString(env_, name_).c_str(), toStdString(env_, value_).c_str());
}

JNIEXPORT jstring JNICALL Java_org_sandvik_Native_getProperty(JNIEnv* env_, jclass, jlong handle_, jstring name_) {
	const char* value = sv_vm_get_property(reinterpret_cast<sv_vm*>(handle_), toStdString(env_, name_).c_str());
	return value != nullptr ? env_->NewStringUTF(value) : nullptr;
}

JNIEXPORT jobjectArray JNICALL Java_org_sandvik_Native_callStatic(JNIEnv* env_, jclass, jlong handle_, jstring class_, jstring method_,
                                                                 jstring signature_, jobjectArray args_) {
	sv_vm* vm = reinterpret_cast<sv_vm*>(handle_);
	sv_value value;
	value.kind = SV_KIND_NULL;
	value.int_value = 0;
	value.long_value = 0;
	value.double_value = 0.;
	value.string_value = nullptr;
	value.class_name = nullptr;

	std::vector<std::string> tokens;
	if (args_ != nullptr) {
		const jsize count = env_->GetArrayLength(args_);
		tokens.reserve(static_cast<size_t>(count));
		for (jsize i = 0; i < count; ++i) {
			auto element = reinterpret_cast<jstring>(env_->GetObjectArrayElement(args_, i));
			tokens.push_back(toStdString(env_, element));
			if (element != nullptr) {
				env_->DeleteLocalRef(element);
			}
		}
	}
	std::vector<const char*> pointers;
	pointers.reserve(tokens.size());
	for (const auto& token : tokens) {
		pointers.push_back(token.c_str());
	}

	const std::string classname = toStdString(env_, class_);
	const std::string method = toStdString(env_, method_);
	const std::string signature = toStdString(env_, signature_);
	sv_vm_call_static(vm, classname.c_str(), method.c_str(), signature.c_str(), pointers.empty() ? nullptr : pointers.data(),
	                  static_cast<int32_t>(tokens.size()), &value);
	jobjectArray result = makeResult(env_, value);
	sv_value_dispose(&value);
	return result;
}

JNIEXPORT jstring JNICALL Java_org_sandvik_Native_lastError(JNIEnv* env_, jclass, jlong handle_) {
	const char* error = sv_vm_last_error(reinterpret_cast<sv_vm*>(handle_));
	return error != nullptr ? env_->NewStringUTF(error) : nullptr;
}

}  // extern "C"
