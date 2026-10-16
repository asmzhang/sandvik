#pragma once
/*
 * compat/sys/types.h -- uid_t/gid_t for mingw-w64.
 *
 * mingw-w64's <sys/types.h> defines neither uid_t nor gid_t, but the Android
 * natives in android/12.0.0/natives/libcore/io/Linux.cpp use uid_t
 * (getpwuid(static_cast<uid_t>(uid))). Give them the width Linux/Android uses
 * on 64-bit targets, so Java's StructPasswd.java sees the same numbers.
 *
 * Everything else is forwarded to the toolchain's <sys/types.h> unchanged.
 */
#include_next <sys/types.h>

#if defined(_WIN32) && !defined(SANDVIK_COMPAT_SYS_TYPES_IDS)
#define SANDVIK_COMPAT_SYS_TYPES_IDS 1

#ifndef _UID_T_DEFINED
typedef unsigned int uid_t;
#define _UID_T_DEFINED
#endif

#ifndef _GID_T_DEFINED
typedef unsigned int gid_t;
#define _GID_T_DEFINED
#endif

#endif /* _WIN32 && !SANDVIK_COMPAT_SYS_TYPES_IDS */
