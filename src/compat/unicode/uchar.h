#pragma once
/*
 * compat/unicode/uchar.h — ICU is not shipped with llvm-mingw, but
 * java.lang.Character only needs u_isalpha()/u_isalnum() (and UChar32).
 *
 * If a real ICU header is reachable (Linux builds, where the waf build links
 * libicuuc) it is used unchanged; otherwise fall back to the C library for
 * ASCII and treat non-ASCII code points as letters/digits, which matches the
 * CJK case the VM is normally exercised with.
 */
#if defined(__has_include_next)
#if __has_include_next(<unicode/uchar.h>)
#include_next <unicode/uchar.h>
#define SANDVIK_COMPAT_HAVE_ICU_UCHAR 1
#endif
#endif

#ifndef SANDVIK_COMPAT_HAVE_ICU_UCHAR
#include <cctype>
#include <cstdint>

#ifndef UChar32
typedef int32_t UChar32;
#endif

inline bool u_isalpha(UChar32 c) {
	if (c < 0 || c > 0x10FFFF) {
		return false;
	}
	if (c < 0x80) {
		return std::isalpha(static_cast<unsigned char>(c)) != 0;
	}
	return true; /* non-ASCII: approximated, see header comment */
}

inline bool u_isalnum(UChar32 c) {
	if (c < 0 || c > 0x10FFFF) {
		return false;
	}
	if (c < 0x80) {
		return std::isalnum(static_cast<unsigned char>(c)) != 0;
	}
	return true; /* non-ASCII: approximated, see header comment */
}
#endif /* SANDVIK_COMPAT_HAVE_ICU_UCHAR */
