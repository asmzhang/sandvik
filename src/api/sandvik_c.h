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
 * Objects that are not strings cross the boundary as sv_handle references: resolve
 * their class with sv_vm_resolve_class(), create strings with sv_vm_new_string(),
 * pass handles as sv_arg with SV_ARG_HANDLE and read them back from
 * sv_value.handle. Handles are stable, so an object that leaves the VM and comes
 * back keeps its identity.
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
	int64_t handle;        /**< SV_KIND_STRING / SV_KIND_OBJECT: handle of that object (0 when none) */
} sv_value;

/** @brief A reference to an object or a class living inside the VM.
 *
 * 0 is the null handle and is accepted wherever a handle is expected. Handles are
 * stable: while a handle is alive, one VM object always maps to one handle, so an
 * object that crosses the boundary and comes back keeps its identity (this is what
 * a host JNI bridge has to preserve). A live handle also **pins** its object: the
 * VM's garbage collector cannot see raw ObjectRef values held outside the VM, so
 * every handle is registered as a root and sv_vm_release_handle() drops that root
 * (which lets the collector reclaim the object once nothing else references it).
 */
typedef int64_t sv_handle;

/** @brief Kind of an sv_arg. */
typedef enum sv_arg_kind {
	SV_ARG_TOKEN = 0, /**< typed token in sv_arg.token */
	SV_ARG_HANDLE = 1 /**< VM handle in sv_arg.handle */
} sv_arg_kind;

/** @brief One argument of sv_vm_call_static_handle(). */
typedef struct sv_arg {
	int32_t kind;      /**< one of sv_arg_kind */
	const char* token; /**< SV_ARG_TOKEN: "s:text", "i:42", "l:42", "d:1.5", "n:" */
	sv_handle handle;  /**< SV_ARG_HANDLE: handle from sv_vm_new_string()/sv_vm_call_static(); 0 passes null */
} sv_arg;

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

/** @brief Resolve a class by name (dotted or slashed) without calling it. @return class handle, or 0 on failure. */
sv_handle sv_vm_resolve_class(sv_vm* vm_, const char* class_name_);

/** @brief Create a java.lang.String inside the VM. @return object handle, or 0 on failure. */
sv_handle sv_vm_new_string(sv_vm* vm_, const char* utf8_);

/**
 * @brief Call a static method of a class handle, mixing tokens and VM handles.
 * @param class_ class handle from sv_vm_resolve_class() (an object handle denoting a Class also works)
 * @param out_ filled with the return value; an object return also carries its handle in out_->handle
 * @return 0 on success, negative on failure (see sv_vm_last_error)
 */
int32_t sv_vm_call_static_handle(sv_vm* vm_, sv_handle class_, const char* method_, const char* signature_,
                                 const sv_arg* args_, int32_t argc_, sv_value* out_);

/** @brief Class name of a handle (dotted for classes/objects), or NULL. Owned by the VM. */
const char* sv_vm_object_class(sv_vm* vm_, sv_handle handle_);

/** @brief Text of a string handle, the toString() of any other object, or NULL. Owned by the VM. */
const char* sv_vm_object_string(sv_vm* vm_, sv_handle handle_);

/** @brief 1 when the handle denotes a java.lang.String. */
int32_t sv_vm_object_is_string(sv_vm* vm_, sv_handle handle_);

/** @brief 1 for the null handle or for a null object. */
int32_t sv_vm_object_is_null(sv_vm* vm_, sv_handle handle_);

/** @brief 1 when the handle denotes a Class object (or a resolved class). */
int32_t sv_vm_object_is_class(sv_vm* vm_, sv_handle handle_);

/** @brief 1 when the handle denotes an array. */
int32_t sv_vm_object_is_array(sv_vm* vm_, sv_handle handle_);

/** @brief Length of an array handle, or -1 when it is not an array. */
int32_t sv_vm_object_array_length(sv_vm* vm_, sv_handle handle_);

/** @brief Element of a one dimensional array handle as a handle, or 0. */
sv_handle sv_vm_object_element(sv_vm* vm_, sv_handle handle_, int32_t index_);

/** @brief Number of live handles (diagnostics: an identity preserving bridge keeps it flat). */
int32_t sv_vm_handle_count(sv_vm* vm_);

/** @brief Drop a handle and the GC root it holds. */
void sv_vm_release_handle(sv_vm* vm_, sv_handle handle_);

/** @brief Start the collector thread (idempotent).
 *
 * The collector thread is already started when the VM is constructed (its
 * constructor calls GC::manageVm, which runs the thread on the first VM), so this
 * is an explicit guarantee for hosts that want to be certain before asking for a
 * cycle rather than a switch that turns collection on.
 * @return 0 on success, negative on failure
 */
int32_t sv_gc_start(void);

/** @brief Ask the running collector for one cycle (asynchronous). @return 0, or negative on failure. */
int32_t sv_gc_request_collect(void);

/** @brief Number of collection cycles performed so far. */
uint64_t sv_gc_cycles(void);

/** @brief Number of objects currently owned by the collector. */
uint64_t sv_gc_tracked_count(void);

/** @brief Diagnostics: allocate a String that nothing references.
 *
 * Gives the next collection a provably unreachable object to sweep, which lets a
 * test show that a collection really happened and that rooted handles survived it.
 * @return 0 on success, negative on failure
 */
int32_t sv_gc_probe_unrooted_string(sv_vm* vm_, const char* utf8_);

/** @brief Release the memory owned by a sv_value (safe to call twice). */
void sv_value_dispose(sv_value* value_);

/** @brief Last error message of the VM, or NULL. Owned by the VM. */
const char* sv_vm_last_error(sv_vm* vm);

#ifdef __cplusplus
}
#endif

#endif /* SANDVIK_EMBED_API_H */
