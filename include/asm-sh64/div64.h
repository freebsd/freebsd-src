#ifndef __ASM_SH64_DIV64_H
#define __ASM_SH64_DIV64_H

/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * include/asm-sh64/div64.h
 *
 * Copyright (C) 2000, 2001  Paolo Alberelli
 *
 */

#define do_div(n,base) ({ \
int __res; \
__res = ((unsigned long) n) % (unsigned) base; \
n = ((unsigned long) n) / (unsigned) base; \
__res; })

#endif /* __ASM_SH64_DIV64_H */
