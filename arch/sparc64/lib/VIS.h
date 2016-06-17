/* $Id: VIS.h,v 1.4 1999/05/25 16:52:50 jj Exp $
 * VIS.h: High speed copy/clear operations utilizing the UltraSparc
 *        Visual Instruction Set.
 *
 * Copyright (C) 1997 David S. Miller (davem@caip.rutgers.edu)
 * Copyright (C) 1996, 1997, 1999 Jakub Jelinek (jj@ultra.linux.cz)
 */

	/* VIS code can be used for numerous copy/set operation variants.
	 * It can be made to work in the kernel, one single instance,
	 * for all of memcpy, copy_to_user, and copy_from_user by setting
	 * the ASI src/dest globals correctly.  Furthermore it can
	 * be used for kernel-->kernel page copies as well, a hook label
	 * is put in here just for this purpose.
	 *
	 * For userland, compiling this without __KERNEL__ defined makes
	 * it work just fine as a generic libc bcopy and memcpy.
	 * If for userland it is compiled with a 32bit gcc (but you need
	 * -Wa,-Av9a), the code will just rely on lower 32bits of
	 * IEU registers, if you compile it with 64bit gcc (ie. define
	 * __sparc_v9__), the code will use full 64bit.
	 */

#ifndef __VIS_H
#define __VIS_H
	 
#ifdef __KERNEL__
#include <asm/head.h>
#include <asm/asi.h>
#else
#define ASI_AIUS		0x11 /* Secondary, user				*/
#define ASI_BLK_AIUS		0x71 /* Secondary, user, blk ld/st		*/
#define ASI_P			0x80 /* Primary, implicit			*/
#define ASI_S			0x81 /* Secondary, implicit			*/
#define ASI_BLK_COMMIT_P	0xe0 /* Primary, blk store commit		*/
#define ASI_BLK_COMMIT_S	0xe1 /* Secondary, blk store commit		*/
#define ASI_BLK_P		0xf0 /* Primary, blk ld/st			*/
#define ASI_BLK_S		0xf1 /* Secondary, blk ld/st			*/
#define FPRS_FEF		0x04
#endif

	/* I'm telling you, they really did this chip right.
	 * Perhaps the SunSoft folks should visit some of the
	 * people in Sun Microelectronics and start some brain
	 * cell exchange program...
	 */
#define ASI_BLK_XOR		(ASI_P ^ ASI_BLK_P)
	/* Well, things get more hairy if we use ASI_AIUS as
	 * USER_DS and ASI_P as KERNEL_DS, we'd reach
	 * commit block stores this way which is not what we want...
	 */
	/* ASI_P->ASI_BLK_P && ASI_AIUS->ASI_BLK_AIUS transitions can be done
	 * as blkasi = asi | ASI_BLK_OR
	 */
#define ASI_BLK_OR		(ASI_BLK_P & ~ASI_P)
	/* Transition back from ASI_BLK_P->ASI_P && ASI_BLK_AIUS->ASI_AIUS is
	 * more complicated:
	 * asi = blkasi ^ (blkasi >> 3) ^ ASI_BLK_XOR1
	 */
#define ASI_BLK_XOR1		(ASI_BLK_P ^ (ASI_BLK_P >> 3) ^ ASI_P)

#define	asi_src			%o3
#define asi_dest		%o4

#ifdef __KERNEL__
#define ASI_SETSRC_BLK		wr	asi_src, 0, %asi;
#define ASI_SETSRC_NOBLK	wr	asi_src, 0, %asi;
#define ASI_SETDST_BLK		wr	asi_dest, 0, %asi;
#define ASI_SETDST_NOBLK	wr	asi_dest, 0, %asi;
#define ASIBLK			%asi
#define ASINORMAL		%asi
#define LDUB			lduba
#define LDUH			lduha
#define LDUW			lduwa
#define LDX			ldxa
#define LDD			ldda
#define LDDF			ldda
#define LDBLK			ldda
#define STB			stba
#define STH			stha
#define STW			stwa
#define STD			stda
#define STX			stxa
#define STDF			stda
#define STBLK			stda
#else
#define ASI_SETSRC_BLK
#define ASI_SETSRC_NOBLK
#define ASI_SETDST_BLK
#define ASI_SETDST_NOBLK
#define ASI_SETDST_SPECIAL
#define ASIBLK			%asi
#define ASINORMAL
#define LDUB			ldub
#define LDUH			lduh
#define LDUW			lduw
#define LDD			ldd
#define LDX			ldx
#define LDDF			ldd
#define LDBLK			ldda
#define STB			stb
#define STH			sth
#define STW			stw
#define STD			std
#define STX			stx
#define STDF			std
#define STBLK			stda
#endif

#ifdef __KERNEL__

#define REGS_64BIT

#else

#ifndef REGS_64BIT
#ifdef __sparc_v9__
#define REGS_64BIT
#endif
#endif

#endif

#ifndef REGS_64BIT
#define	xcc	icc
#endif

#endif
