/*
 * posix_win32.cpp -- Windows implementations of the POSIX calls the Android
 * natives need.
 *
 * android/12.0.0/natives/libcore/io/Linux.cpp is built on Windows too (it is
 * the Java-side implementation of libcore.io.Linux), but mingw-w64 provides no
 * <pwd.h>, <sys/utsname.h>, <sys/mman.h> or <sys/poll.h> and no setpgid().
 * The compat/ headers declare them; this file defines them.
 *
 * Semantics are chosen to match what Android's Java code does with the result:
 *  - getuid()/getgid() report 0 everywhere (see compat/unistd.h), so getpwuid()
 *    answers with a synthetic entry for the "sandvik" user.
 *  - uname() reports the emulated platform (Linux/aarch64), not the Windows
 *    host: sandvik runs Android dex files, and Java code branches on these
 *    values.
 *  - mmap()/munmap() are real mappings, not errno-free no-ops: the VM maps dex
 *    and jar images, and a failing mmap must surface as an IOException on the
 *    Java side.
 *  - setpgid() is a no-op (there is no process group to join on Windows) and
 *    listen() reports ENOSYS: the AF_UNIX endpoints libcore sets up are not
 *    reachable from the CRT fd layer on Windows.
 *
 * poll() lives in posix_win32_poll.cpp instead: it needs winsock, and winsock's
 * listen(SOCKET, int) would clash with the POSIX listen() shim defined here.
 *
 * Nothing in this file is compiled on POSIX hosts.
 */

#if defined(_WIN32)

// Keep winsock.h (and its listen()) out of this translation unit -- see above.
#define WIN32_LEAN_AND_MEAN

#include <cerrno>
#include <cstring>
#include <string>

#include <windows.h>

#include <io.h>
#include <sys/stat.h>

#include <pwd.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>

extern "C" {

struct passwd* getpwuid(unsigned int uid) {
	static struct passwd entry;
	static char name[] = "sandvik";
	static char passwd[] = "x";
	static char gecos[] = "sandvik";
	static char dir[] = "/data";
	static char shell[] = "/system/bin/sh";

	entry.pw_name = name;
	entry.pw_passwd = passwd;
	entry.pw_uid = uid;
	entry.pw_gid = uid;
	entry.pw_gecos = gecos;
	entry.pw_dir = dir;
	entry.pw_shell = shell;
	return &entry;
}

int uname(struct utsname* buf) {
	if (buf == nullptr) {
		errno = EFAULT;
		return -1;
	}
	std::memset(buf, 0, sizeof(*buf));
	auto copy = [](char* dst, const char* src) {
		std::strncpy(dst, src, _UTSNAME_LENGTH - 1);
		dst[_UTSNAME_LENGTH - 1] = '\0';
	};
	copy(buf->sysname, "Linux");
	copy(buf->nodename, "localhost");
	copy(buf->release, "5.10.0-sandvik");
	copy(buf->version, "#1 SMP PREEMPT sandvik");
	copy(buf->machine, "aarch64");
	return 0;
}

int setpgid(int pid, int pgid) {
	(void)pid;
	(void)pgid;
	return 0;
}

static DWORD mmapProtection(int prot) {
	if ((prot & PROT_EXEC) != 0) {
		return ((prot & PROT_WRITE) != 0) ? PAGE_EXECUTE_READWRITE : PAGE_EXECUTE_READ;
	}
	if ((prot & PROT_WRITE) != 0) {
		return PAGE_READWRITE;
	}
	return PAGE_READONLY;
}

void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset) {
	if (length == 0) {
		errno = EINVAL;
		return MAP_FAILED;
	}

	const DWORD protection = mmapProtection(prot);

	// Anonymous mappings (and the fd == -1 case) are plain committed memory.
	if (((flags & MAP_ANONYMOUS) != 0) || fd < 0) {
		void* result = VirtualAlloc(addr, length, MEM_COMMIT | MEM_RESERVE, protection);
		if (result == nullptr) {
			errno = ENOMEM;
			return MAP_FAILED;
		}
		return result;
	}

	auto handle = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
	if (handle == INVALID_HANDLE_VALUE) {
		errno = EBADF;
		return MAP_FAILED;
	}

	HANDLE mapping = CreateFileMappingW(handle, nullptr, protection, 0, 0, nullptr);
	if (mapping == nullptr) {
		errno = ENOMEM;
		return MAP_FAILED;
	}

	DWORD access = ((prot & PROT_EXEC) != 0) ? FILE_MAP_EXECUTE : FILE_MAP_READ;
	if ((prot & PROT_WRITE) != 0) {
		access |= FILE_MAP_WRITE;
	}

	const auto high = static_cast<DWORD>(static_cast<uint64_t>(offset) >> 32);
	const auto low = static_cast<DWORD>(static_cast<uint64_t>(offset) & 0xffffffffu);
	void* result = MapViewOfFile(mapping, access, high, low, length);
	CloseHandle(mapping);
	if (result == nullptr) {
		errno = ENOMEM;
		return MAP_FAILED;
	}
	return result;
}

int munmap(void* addr, size_t length) {
	(void)length;
	if (addr == nullptr) {
		errno = EINVAL;
		return -1;
	}
	MEMORY_BASIC_INFORMATION info;
	if (VirtualQuery(addr, &info, sizeof(info)) == 0) {
		errno = EINVAL;
		return -1;
	}
	if (info.Type == MEM_MAPPED) {
		if (UnmapViewOfFile(addr) == 0) {
			errno = EINVAL;
			return -1;
		}
		return 0;
	}
	if (VirtualFree(addr, 0, MEM_RELEASE) == 0) {
		errno = EINVAL;
		return -1;
	}
	return 0;
}

int listen(int fd, int backlog) {
	(void)fd;
	(void)backlog;
	errno = ENOSYS;
	return -1;
}

}  // extern "C"

#endif /* _WIN32 */
