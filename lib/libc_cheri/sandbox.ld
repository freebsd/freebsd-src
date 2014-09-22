/*-
 * Copyright (c) 2011-2014 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Linker definition for a relocatable sandbox.  Each one begins with a
 * 0x1000 byte "guard" region to catch NULL pointer dereferences, followed by
 * some amount of metadata.  The binary is linked at 0x8000.
 */

__code_base__ = 0x8000;
CAP_ALIGN = 0x20;

SECTIONS
{
	. = __code_base__;

	.text ALIGN(CAP_ALIGN): {
		crt_sb.o(.text .data .bss)
	}
	.MIPS.options ALIGN(CAP_ALIGN): {
		*(.MIPS.options)
	}
	.init : {
		KEEP (*(.init))
	} = 0
	.text ALIGN(CAP_ALIGN): {
		*(.text)
	}
	.fini ALIGN(CAP_ALIGN): {
		KEEP (*(.fini))
	} = 0
	.preinit_array ALIGN(CAP_ALIGN) : {
		PROVIDE_HIDDEN (__preinit_array_start = .);
		KEEP (*(.preinit_array))
		PROVIDE_HIDDEN (__preinit_array_end = .);
	}
	.init_array ALIGN(CAP_ALIGN) : {
		PROVIDE_HIDDEN (__init_array_start = .);
		KEEP (*(SORT(.init_array.*)))
		KEEP (*(.init_array))
		PROVIDE_HIDDEN (__init_array_end = .);
	}
	.fini_array ALIGN(CAP_ALIGN) : {
		PROVIDE_HIDDEN (__fini_array_start = .);
		KEEP (*(.fini_array))
		KEEP (*(SORT(.fini_array.*)))
		PROVIDE_HIDDEN (__fini_array_end = .);
	}
	.ctors ALIGN(CAP_ALIGN) : {
		KEEP (crtbeginC.o(.ctors))
		KEEP (*(EXCLUDE_FILE (*crtendC.o) .ctors))
		KEEP (*(SORT(.ctors.*)))
		KEEP (crtendC.o(.ctors))
	}
	.dtors ALIGN(CAP_ALIGN) : {
		KEEP (crtbeginC.o(.dtors))
		KEEP (*(EXCLUDE_FILE (*crtendC.o) .dtors))
		KEEP (*(SORT(.dtors.*)))
		KEEP (crtendC.o(.dtors))
	}
	.bss ALIGN(CAP_ALIGN): {
		*(.bss)
	}
	.data ALIGN(CAP_ALIGN): {
		*(.data)
	}
	_gp = ALIGN(CAP_ALIGN) + 0x7fc0;
	.got : {
		*(.got.plt)
		*(.got)
	}

	/*
	 * Force zero-filling of previous segments in generated image.
	 */
	sandbox_trailer : {
		QUAD(0xffffffffffffffff)
	}

	__bss_start = ADDR(.bss);
	__bss_end = ALIGN(__bss_start + SIZEOF(.bss), 0x8);
}
