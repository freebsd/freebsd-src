//===------------------------- __libunwind_config.h -----------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef ____LIBUNWIND_CONFIG_H__
#define ____LIBUNWIND_CONFIG_H__

#if defined(__arm__) && !defined(__USING_SJLJ_EXCEPTIONS__) && \
    !defined(__ARM_DWARF_EH__)
#define _LIBUNWIND_ARM_EHABI 1
#else
#define _LIBUNWIND_ARM_EHABI 0
#endif

#if defined(_LIBUNWIND_IS_NATIVE_ONLY)
# if defined(__i386__)
#  define _LIBUNWIND_TARGET_I386 1
#  define _LIBUNWIND_CONTEXT_SIZE 8
#  define _LIBUNWIND_CURSOR_SIZE 19
#  define _LIBUNWIND_MAX_REGISTER 9
# elif defined(__x86_64__)
#  define _LIBUNWIND_TARGET_X86_64 1
#  define _LIBUNWIND_CONTEXT_SIZE 21
#  define _LIBUNWIND_CURSOR_SIZE 33
#  define _LIBUNWIND_MAX_REGISTER 17
# elif defined(__ppc__)
#  define _LIBUNWIND_TARGET_PPC 1
#  define _LIBUNWIND_CONTEXT_SIZE 117
#  define _LIBUNWIND_CURSOR_SIZE 128
#  define _LIBUNWIND_MAX_REGISTER 113
# elif defined(__aarch64__)
#  define _LIBUNWIND_TARGET_AARCH64 1
#  define _LIBUNWIND_CONTEXT_SIZE 66
#  define _LIBUNWIND_CURSOR_SIZE 78
#  define _LIBUNWIND_MAX_REGISTER 96
# elif defined(__arm__)
#  define _LIBUNWIND_TARGET_ARM 1
#  define _LIBUNWIND_CONTEXT_SIZE 60
#  define _LIBUNWIND_CURSOR_SIZE 67
#  define _LIBUNWIND_MAX_REGISTER 96
# elif defined(__or1k__)
#  define _LIBUNWIND_TARGET_OR1K 1
#  define _LIBUNWIND_CONTEXT_SIZE 16
#  define _LIBUNWIND_CURSOR_SIZE 28
#  define _LIBUNWIND_MAX_REGISTER 32
# elif defined(__riscv)
#  define _LIBUNWIND_TARGET_RISCV 1
#  define _LIBUNWIND_CONTEXT_SIZE 64
#  define _LIBUNWIND_CURSOR_SIZE 76
#  define _LIBUNWIND_MAX_REGISTER 96
# elif defined(__mips__)
#  if defined(_ABIO32) && _MIPS_SIM == _ABIO32
#    define _LIBUNWIND_TARGET_MIPS_O32 1
#    if defined(__mips_hard_float)
#      define _LIBUNWIND_CONTEXT_SIZE 50
#      define _LIBUNWIND_CURSOR_SIZE 61
#    else
#      define _LIBUNWIND_CONTEXT_SIZE 18
#      define _LIBUNWIND_CURSOR_SIZE 29
#    endif
#  elif defined(_ABIN32) && _MIPS_SIM == _ABIN32
#    define _LIBUNWIND_TARGET_MIPS_NEWABI 1
#    if defined(__mips_hard_float)
#      define _LIBUNWIND_CONTEXT_SIZE 67
#      define _LIBUNWIND_CURSOR_SIZE 78
#    else
#      define _LIBUNWIND_CONTEXT_SIZE 35
#      define _LIBUNWIND_CURSOR_SIZE 46
#    endif
#  elif defined(_ABI64) && _MIPS_SIM == _ABI64
#    define _LIBUNWIND_TARGET_MIPS_NEWABI 1
#    if defined(__mips_hard_float)
#      define _LIBUNWIND_CONTEXT_SIZE 67
#      define _LIBUNWIND_CURSOR_SIZE 79
#    else
#      define _LIBUNWIND_CONTEXT_SIZE 35
#      define _LIBUNWIND_CURSOR_SIZE 47
#    endif
#  else
#    error "Unsupported MIPS ABI and/or environment"
#  endif
#  define _LIBUNWIND_MAX_REGISTER 66
# else
#  error "Unsupported architecture."
# endif
#else // !_LIBUNWIND_IS_NATIVE_ONLY
# define _LIBUNWIND_TARGET_I386 1
# define _LIBUNWIND_TARGET_X86_64 1
# define _LIBUNWIND_TARGET_PPC 1
# define _LIBUNWIND_TARGET_AARCH64 1
# define _LIBUNWIND_TARGET_ARM 1
# define _LIBUNWIND_TARGET_OR1K 1
# define _LIBUNWIND_TARGET_MIPS_O32 1
# define _LIBUNWIND_TARGET_MIPS_NEWABI 1
# define _LIBUNWIND_CONTEXT_SIZE 128
# define _LIBUNWIND_CURSOR_SIZE 140
# define _LIBUNWIND_MAX_REGISTER 120
#endif // _LIBUNWIND_IS_NATIVE_ONLY

#endif // ____LIBUNWIND_CONFIG_H__
