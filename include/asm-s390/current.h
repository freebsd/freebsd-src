/*
 *  include/asm-s390/current.h
 *
 *  S390 version
 *    Copyright (C) 1999 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Martin Schwidefsky (schwidefsky@de.ibm.com)
 *
 *  Derived from "include/asm-i386/current.h"
 */

#ifndef _S390_CURRENT_H
#define _S390_CURRENT_H

#ifdef __KERNEL__

struct task_struct;

static inline struct task_struct * get_current(void)
{
        struct task_struct *current;
        __asm__("lhi   %0,-8192\n\t"
                "al    %0,0xc40"
                : "=&r" (current) : : "cc" );
        return current;
 }

#define current get_current()

#endif

#endif /* !(_S390_CURRENT_H) */
