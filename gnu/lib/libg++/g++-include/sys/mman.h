#ifndef __libgxx_sys_mman_h

extern "C" {
#ifdef __sys_mman_h_recursive
#include_next <sys/mman.h>
#else
#define __sys_mman_h_recursive
#include_next <sys/mman.h>

#define __libgxx_sys_mman_h 1
#endif
}

#endif
