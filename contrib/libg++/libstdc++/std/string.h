// Main header for the -*- C++ -*- string classes.

#ifndef __STRING__
#define __STRING__

#include <std/bastring.h>

extern "C++" {
typedef basic_string <char, string_char_traits <char> > string;
// typedef basic_string <wchar_t, string_char_traits <wchar_t> > wstring;
} // extern "C++"

#endif
