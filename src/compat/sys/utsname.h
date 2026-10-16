#pragma once
/*
 * compat/sys/utsname.h — mingw-w64 has no <sys/utsname.h>.
 *
 * Only libcore.io.UnixFileSystem includes it and it never calls uname(),
 * so the struct alone is enough for the include to compile on Windows.
 */
#ifdef _WIN32
#ifndef _UTSNAME_LENGTH
#define _UTSNAME_LENGTH 65
#endif

struct utsname {
	char sysname[_UTSNAME_LENGTH];
	char nodename[_UTSNAME_LENGTH];
	char release[_UTSNAME_LENGTH];
	char version[_UTSNAME_LENGTH];
	char machine[_UTSNAME_LENGTH];
};

#ifdef __cplusplus
extern "C" {
#endif

int uname(struct utsname* buf);

#ifdef __cplusplus
}
#endif

#endif /* _WIN32 */
