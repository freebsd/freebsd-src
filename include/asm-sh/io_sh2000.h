/*
 * include/asm-sh/io_sh2000.h
 *
 * Copyright 2000 Stuart Menefy (stuart.menefy@st.com)
 *           2001 SUGIOKA Toshinobu (sugioka@itonet.co.jp)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * IO functions for use when we don't know what machine we are on
 */

#ifndef _ASM_SH_IO_SH2000_H
#define _ASM_SH_IO_SH2000_H

#include <asm/io_generic.h>

unsigned long sh2000_isa_port2addr(unsigned long offset);

#ifdef __WANT_IO_DEF

# define __inb			generic_inb
# define __inw			generic_inw
# define __inl			generic_inl
# define __outb			generic_outb
# define __outw			generic_outw
# define __outl			generic_outl

# define __inb_p		generic_inb_p
# define __inw_p		generic_inw
# define __inl_p		generic_inl
# define __outb_p		generic_outb_p
# define __outw_p		generic_outw
# define __outl_p		generic_outl

# define __insb			generic_insb
# define __insw			generic_insw
# define __insl			generic_insl
# define __outsb		generic_outsb
# define __outsw		generic_outsw
# define __outsl		generic_outsl

# define __readb		generic_readb
# define __readw		generic_readw
# define __readl		generic_readl
# define __writeb		generic_writeb
# define __writew		generic_writew
# define __writel		generic_writel

# define __isa_port2addr	sh2000_isa_port2addr
# define __ioremap		generic_ioremap
# define __iounmap		generic_iounmap

#endif

#endif /* _ASM_SH_IO_SH2000_H */
