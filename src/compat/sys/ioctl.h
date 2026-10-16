#pragma once
/*
 * compat/sys/ioctl.h — mingw-w64 (and llvm-mingw) ship no <sys/ioctl.h>.
 *
 * The only consumer in the tree is java.io.FileInputStream.available0(),
 * which asks for FIONREAD and already falls back to
 * lseek(SEEK_END) - lseek(SEEK_CUR) when the call fails. Reporting failure
 * therefore keeps the Java semantics intact on Windows.
 */
#ifdef _WIN32
#ifndef FIONREAD
#define FIONREAD 0x541B /* Linux value; never issued here */
#endif /* FIONREAD */

inline int ioctl(int /*fd*/, unsigned long /*request*/, ...) {
	return -1;
}
#endif /* _WIN32 */
