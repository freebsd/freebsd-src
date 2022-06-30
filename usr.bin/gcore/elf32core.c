/* $FreeBSD$ */
#ifndef __LP64__
#error "this file must be compiled for LP64."
#endif

#define __ELF_WORD_SIZE 32
#define _MACHINE_ELF_WANT_32BIT
#define	_WANT_LWPINFO32

#include <sys/procfs.h>

#define	ELFCORE_COMPAT_32	1
#include "elfcore.c"

