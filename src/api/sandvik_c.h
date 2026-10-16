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
 * @file sandvik_c.h
 * @brief Embedding API of sandvik (plain C, no C++ types, no JNI types).
 *
 * The header is the only contract an embedding host (JNI bridge, C application,
 * another runtime) needs: it hides the whole C++ VM behind an opaque handle.
 * It intentionally avoids including any sandvik internal header so that a host
 * can compile it with a completely different notion of "JNI".
 *
 * Typical use:
 * @code
 *   sv_vm* vm = sv_vm_create();
 *   if (sv_vm_load_rt_dir(vm, "/path/to/android/12.0.0/bin") != 0) { ... }
 *   if (sv_vm_load_dex(vm, "classes.dex") != 0) { ... }
 *   sv_value out;
 *   const char* args[] = { "s:world" };
 *   if (sv_vm_call_static(vm, "BridgeHello", "hello",
 *                         "(Ljava/lang/String;)Ljava/lang/String;", args, 1, &out) == 0) {
 *       printf("%s\n", out.string_value);
 *       sv_value_dispose(&out);
 *   }
 *   sv_vm_destroy(vm);
 * @endcode
 *
 * Arguments are passed as typed tokens:
 *   - "s:<text>"  java.lang.String  (a token without a known prefix is a string)
 *   - "i:<int>"   int
 *   - "l:<long>"  long
 *   - "d:<x>"     double
 *   - "n:"        null
 * Only java.lang.String arguments and int/long/double/void returns are marshalled
 * today: any other object crosses the boundary as an opaque reference (kind
 * SV_KIND_OBJECT with its class descriptor).
 */

#ifndef SANDVIK_EMBED_API_H
#define SANDVIK_EMBED_API_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Opaque handle to a sandvik VM instance. */
typedef struct sv_vm sv_vm;

/** @brief Kind of a value returned by sv_vm_call_static(). */
typedef enum sv_value_kind {
	SV_KIND_NULL = 0,   /**< null (or a void method) */
	SV_KIND_STRING = 1, /**< java.lang.String, see string_value */
	SV_KIND_INT = 2,    /**< int, see int_value */
	SV_KIND_LONG = 3,   /**< long, see long_value */
	SV_KIND_DOUBLE = 4, /**< double, see double_value */
	SV_KIND_BOOL = 5,   /**< boolean, see int_value */
	SV_KIND_OBJECT = 6  /**< any other object, see class_name */
} sv_value_kind;

/** @brief A value exchanged with the VM. Dispose it with sv_value_dispose(). */
typedef struct sv_value {
	int32_t kind;          /**< one of sv_value_kind */
	int32_t int_value;     /**< SV_KIND_INT / SV_KIND_BOOL */
	int64_t long_value;    /**< SV_KIND_LONG */
	double double_value;   /**< SV_KIND_DOUBLE */
	char* string_value;    /**< SV_KIND_STRING, malloc'ed UTF-8, may be NULL */
	char* class_name;      /**< SV_KIND_OBJECT class descriptor, malloc'ed, may be NULL */
} sv_value;

/** @brief Create a VM instance. @return handle, or NULL when it cannot be created. */
sv_vm* sv_vm_create(void);

/** @brief Destroy a VM instance created by sv_vm_create(). NULL is accepted. */
void sv_vm_destroy(sv_vm* vm);

/** @brief Change the log level of the VM (0 = none, 1 = fatal, 2 = error, 3 = warn, 4 = info, 5 = debug). */
void sv_vm_set_log_level(sv_vm* vm, int32_t level);

/** @brief Change the log level of the process-wide sandvik logger.
 *
 * The VM logs a banner while it is being constructed, so a host that wants a
 * silent library has to call this one *before* sv_vm_create().
 */
void sv_set_log_level(int32_t level);

/** @brief Load the Android runtime classes from a directory of jar/dex files. @return 0 on success. */
int32_t sv_vm_load_rt_dir(sv_vm* vm, const char* dir);

/** @brief Load the Android runtime classes from a single jar/dex file. @return 0 on success. */
int32_t sv_vm_load_rt_file(sv_vm* vm, const char* file);

/** @brief Load a dex file. @return 0 on success. */
int32_t sv_vm_load_dex(sv_vm* vm, const char* path);

/** @brief Load an apk file. @return 0 on success. */
int32_t sv_vm_load_apk(sv_vm* vm, const char* path);

/** @brief Append an entry to the class path. @return 0 on success. */
int32_t sv_vm_add_class_path(sv_vm* vm, const char* path);

/** @brief Set a system property seen by the VM (System.getProperty). @return 0 on success. */
int32_t sv_vm_set_property(sv_vm* vm, const char* name, const char* value);

/** @brief Read a system property of the VM. @return value owned by the VM, valid until the next call, or NULL. */
const char* sv_vm_get_property(sv_vm* vm, const char* name);

/**
 * @brief Call a static method and return its value.
 * @param vm_ handle
 * @param class_name_ class name, dotted or slashed form ("BridgeHello", "com/x/Y")
 * @param method_ method name
 * @param signature_ JNI signature of the method ("(Ljava/lang/String;)Ljava/lang/String;")
 * @param args_ typed argument tokens (see the file comment), may be NULL when argc_ is 0
 * @param argc_ number of arguments
 * @param out_ filled with the return value, dispose it with sv_value_dispose()
 * @return 0 on success, negative on failure (see sv_vm_last_error)
 */
int32_t sv_vm_call_static(sv_vm* vm_, const char* class_name_, const char* method_, const char* signature_,
                          const char* const* args_, int32_t argc_, sv_value* out_);

/** @brief Release the memory owned by a sv_value (safe to call twice). */
void sv_value_dispose(sv_value* value_);

/** @brief Last error message of the VM, or NULL. Owned by the VM. */
const char* sv_vm_last_error(sv_vm* vm);

#ifdef __cplusplus
}
#endif

#endif /* SANDVIK_EMBED_API_H */
