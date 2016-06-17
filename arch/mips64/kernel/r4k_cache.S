/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1995 - 1999 Ralf Baechle
 * Copyright (C) 1999 Silicon Graphics, Inc.
 *
 * Cache error handler
 */
#include <asm/asm.h>
#include <asm/regdef.h>
#include <asm/mipsregs.h>
#include <asm/stackframe.h>

/*
 * Game over.  Go to the button.  Press gently.  Swear where allowed by
 * legislation.
 */
	LEAF(except_vec2_generic)
	/* Famous last words: unreached */
	mfc0	a1,CP0_ERROREPC
	PRINT("Cache error exception: c0_errorepc == %08x\n")
1:
	j	1b
	 nop
	END(except_vec2_generic)
