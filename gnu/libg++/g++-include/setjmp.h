#ifndef _setjmp_h

extern "C" {

#ifdef __setjmp_h_recursive
#include_next <setjmp.h>
#else
#define __setjmp_h_recursive
#define  setjmp  C_header_setjmp
#define  longjmp C_header_longjmp

#ifdef VMS
#include "gnu_cc_include:[000000]setjmp.h"
#else
#include_next <setjmp.h>
#endif

#undef setjmp
#undef longjmp

#define _setjmp_h 1

extern int setjmp(jmp_buf);
extern void longjmp(jmp_buf, int);

#endif
}

#endif
