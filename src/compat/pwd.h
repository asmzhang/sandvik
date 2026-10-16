#pragma once
/*
 * compat/pwd.h -- mingw-w64 has no <pwd.h>.
 *
 * Only libcore.io.UnixFileSystem includes it, and it never calls
 * getpwuid()/getgrgid(), so declaring the structs is enough to keep the POSIX
 * include set compiling on Windows. The id fields are plain unsigned int
 * because mingw-w64's <sys/types.h> defines neither uid_t nor gid_t.
 */
#ifdef _WIN32

struct passwd {
	char* pw_name;
	char* pw_passwd;
	unsigned int pw_uid;
	unsigned int pw_gid;
	char* pw_gecos;
	char* pw_dir;
	char* pw_shell;
};

struct group {
	char* gr_name;
	char* gr_passwd;
	unsigned int gr_gid;
	char** gr_mem;
};

/*
 * libcore.io.Linux.getpwuid() calls this with a uid_t; the field widths above
 * match (see compat/sys/types.h for the uid_t/gid_t typedefs). The definition
 * lives in src/compat/posix_win32.cpp and answers with a synthetic entry.
 */
#ifdef __cplusplus
extern "C" {
#endif

struct passwd* getpwuid(unsigned int uid);

#ifdef __cplusplus
}
#endif

#endif /* _WIN32 */
