#ifndef _LINUX_LINKAGE_H
#define _LINUX_LINKAGE_H

#include <linux/config.h>

#ifdef __cplusplus
#define CPP_ASMLINKAGE extern "C"
#else
#define CPP_ASMLINKAGE
#endif

#if defined __i386__
#define asmlinkage CPP_ASMLINKAGE __attribute__((regparm(0)))
#elif defined __ia64__
#define asmlinkage CPP_ASMLINKAGE __attribute__((syscall_linkage))
#else
#define asmlinkage CPP_ASMLINKAGE
#endif

#define SYMBOL_NAME_STR(X) #X
#define SYMBOL_NAME(X) X
#ifdef __STDC__
#define SYMBOL_NAME_LABEL(X) X##:
#else
#define SYMBOL_NAME_LABEL(X) X/**/:
#endif

#ifdef __arm__
#define __ALIGN .align 0
#define __ALIGN_STR ".align 0"
#else
#ifdef __mc68000__
#define __ALIGN .align 4
#define __ALIGN_STR ".align 4"
#else
#ifdef __sh__
#define __ALIGN .balign 4
#define __ALIGN_STR ".balign 4"
#else
#if defined(__i386__) && defined(CONFIG_X86_ALIGNMENT_16)
#define __ALIGN .align 16,0x90
#define __ALIGN_STR ".align 16,0x90"
#else
#define __ALIGN .align 4,0x90
#define __ALIGN_STR ".align 4,0x90"
#endif
#endif /* __sh__ */
#endif /* __mc68000__ */
#endif /* __arm__ */

#ifdef __ASSEMBLY__

#define ALIGN __ALIGN
#define ALIGN_STR __ALIGN_STR

#define ENTRY(name) \
  .globl SYMBOL_NAME(name); \
  ALIGN; \
  SYMBOL_NAME_LABEL(name)

#endif

#endif
