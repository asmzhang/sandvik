#pragma once
/*
 * compat/sys/mman.h -- mingw-w64 has no <sys/mman.h>.
 *
 * Only src/system/os_constants.hpp includes it, and only for the PROT_, MAP_ and
 * MS_ values it republishes to Java; no Windows build calls mmap().
 * Values are the Linux/Android ones.
 */
#if defined(_WIN32) && !defined(SANDVIK_COMPAT_SYS_MMAN)
#define SANDVIK_COMPAT_SYS_MMAN 1

#ifndef PROT_NONE
#define PROT_NONE 0x0
#endif
#ifndef PROT_READ
#define PROT_READ 0x1
#endif
#ifndef PROT_WRITE
#define PROT_WRITE 0x2
#endif
#ifndef PROT_EXEC
#define PROT_EXEC 0x4
#endif

#ifndef MAP_SHARED
#define MAP_SHARED 0x01
#endif
#ifndef MAP_PRIVATE
#define MAP_PRIVATE 0x02
#endif
#ifndef MAP_FIXED
#define MAP_FIXED 0x10
#endif
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS 0x20
#endif
#ifndef MAP_POPULATE
#define MAP_POPULATE 0x8000
#endif

#ifndef MS_ASYNC
#define MS_ASYNC 1
#endif
#ifndef MS_INVALIDATE
#define MS_INVALIDATE 2
#endif
#ifndef MS_SYNC
#define MS_SYNC 4
#endif

#ifndef MAP_FAILED
#define MAP_FAILED ((void*)-1)
#endif

#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset);
int munmap(void* addr, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* _WIN32 && !SANDVIK_COMPAT_SYS_MMAN */
