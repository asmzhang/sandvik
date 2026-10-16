#pragma once
/*
 * compat/unicode/urename.h — ICU's symbol-renaming header.
 *
 * java.lang.Character includes it alongside <unicode/uchar.h>. When a real
 * ICU is available the real header is used; otherwise the compatibility
 * <unicode/uchar.h> in this directory already provides the few symbols the
 * VM needs, so there is nothing to rename.
 */
#if defined(__has_include_next)
#if __has_include_next(<unicode/urename.h>)
#include_next <unicode/urename.h>
#endif
#endif
