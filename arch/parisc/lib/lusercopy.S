/*
 * Linux/PA-RISC Project (http://www.parisc-linux.org/)
 *
 * Assembly Language User Access Routines
 *  Copyright (C) 2000 Hewlett-Packard (John Marvin)
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2, or (at your option)
 *    any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * These routines still have plenty of room for optimization
 * (word & doubleword load/store, dual issue, store hints, etc.).
 */

/*
 * The following routines assume that space register 3 (sr3) contains
 * the space id associated with the current users address space.
 */


	.text
	
#include <asm/assembly.h>
#include <asm/errno.h>

	/*
	 * get_sr gets the appropriate space value into
	 * sr1 for kernel/user space access, depending
	 * on the flag stored in the task structure.
	 */

	.macro  get_sr
	mfctl       %cr30,%r1
	ldw         TASK_SEGMENT(%r1),%r22
	mfsp        %sr3,%r1
	or,<>       %r22,%r0,%r0
	copy        %r0,%r1
	mtsp        %r1,%sr1
	.endm

	/*
	 * unsigned long
	 * lcopy_to_user(void *to, const void *from, unsigned long n)
	 *
	 * Returns 0 for success.
	 * otherwise, returns number of bytes not transferred.
	 */

	.export lcopy_to_user,code
lcopy_to_user:
	.proc
	.callinfo NO_CALLS
	.entry
	comib,=,n   0,%r24,$lctu_done
	get_sr
$lctu_loop:
	ldbs,ma     1(%r25),%r1
	addib,<>    -1,%r24,$lctu_loop
1:      stbs,ma     %r1,1(%sr1,%r26)
$lctu_done:
	bv          %r0(%r2)
	copy        %r24,%r28
	.exit

2:      b $lctu_done
	ldo         1(%r24),%r24

	.section __ex_table,"a"
#ifdef __LP64__
	.dword      1b,(2b-1b)
#else
	.word       1b,(2b-1b)
#endif
	.previous

	.procend

	/*
	 * unsigned long
	 * lcopy_from_user(void *to, const void *from, unsigned long n)
	 *
	 * Returns 0 for success.
	 * otherwise, returns number of bytes not transferred.
	 *
	 * NOTE: This routine will also zero any bytes in the
	 *       destination that were not copied due to a fault.
	 *
	 */

	.export lcopy_from_user,code
lcopy_from_user:
	.proc
	.callinfo NO_CALLS
	.entry
	comib,=,n   0,%r24,$lcfu_done
	get_sr
$lcfu_loop:
1:      ldbs,ma     1(%sr1,%r25),%r1
	addib,<>    -1,%r24,$lcfu_loop
	stbs,ma     %r1,1(%r26)
$lcfu_done:
	bv          %r0(%r2)
	copy        %r24,%r28
	.exit

2:      copy        %r24,%r23
$lcfu_zero_loop:
	addib,<>    -1,%r23,$lcfu_zero_loop
	stbs,ma     %r0,1(%r26)
	b           $lcfu_done
	nop

	.section __ex_table,"a"
#ifdef __LP64__
	.dword      1b,(2b-1b)
#else
	.word       1b,(2b-1b)
#endif
	.previous

	.procend

	/*
	 * long lstrncpy_from_user(char *dst, const char *src, long n)
	 *
	 * Returns -EFAULT if exception before terminator,
	 *         N if the entire buffer filled,
	 *         otherwise strlen (i.e. excludes zero byte)
	 */

	.export lstrncpy_from_user,code
lstrncpy_from_user:
	.proc
	.callinfo NO_CALLS
	.entry
	comib,=     0,%r24,$lsfu_done
	copy        %r24,%r23
	get_sr
1:      ldbs,ma     1(%sr1,%r25),%r1
$lsfu_loop:
	stbs,ma     %r1,1(%r26)
	comib,=,n   0,%r1,$lsfu_done
	addib,<>,n  -1,%r24,$lsfu_loop
2:      ldbs,ma     1(%sr1,%r25),%r1
$lsfu_done:
	sub         %r23,%r24,%r28
$lsfu_exit:
	bv          %r0(%r2)
	nop
	.exit

3:      b           $lsfu_exit
	ldi         -EFAULT,%r28

	.section __ex_table,"a"
#ifdef __LP64__
	.dword      1b,(3b-1b)
	.dword      2b,(3b-2b)
#else
	.word       1b,(3b-1b)
	.word       2b,(3b-2b)
#endif
	.previous

	.procend

	/*
	 * unsigned long lclear_user(void *to, unsigned long n)
	 *
	 * Returns 0 for success.
	 * otherwise, returns number of bytes not transferred.
	 */

	.export lclear_user,code
lclear_user:
	.proc
	.callinfo NO_CALLS
	.entry
	comib,=,n   0,%r25,$lclu_done
	get_sr
$lclu_loop:
	addib,<>    -1,%r25,$lclu_loop
1:      stbs,ma     %r0,1(%sr1,%r26)

$lclu_done:
	bv          %r0(%r2)
	copy        %r25,%r28
	.exit

2:      b $lclu_done
	ldo        1(%r25),%r25

	.section __ex_table,"a"
#ifdef __LP64__
	.dword      1b,(2b-1b)
#else
	.word       1b,(2b-1b)
#endif
	.previous

	.procend

	/*
	 * long lstrnlen_user(char *s, long n)
	 *
	 * Returns 0 if exception before zero byte or reaching N,
	 *         N+1 if N would be exceeded,
	 *         else strlen + 1 (i.e. includes zero byte).
	 */

	.export lstrnlen_user,code
lstrnlen_user:
	.proc
	.callinfo NO_CALLS
	.entry
	comib,=     0,%r25,$lslen_nzero
	copy	    %r26,%r24
	get_sr
1:      ldbs,ma     1(%sr1,%r26),%r1
$lslen_loop:
	comib,=,n   0,%r1,$lslen_done
	addib,<>    -1,%r25,$lslen_loop
2:      ldbs,ma     1(%sr1,%r26),%r1
$lslen_done:
	bv          %r0(%r2)
	sub	    %r26,%r24,%r28
	.exit

$lslen_nzero:
	b           $lslen_done
	ldo         1(%r26),%r26 /* special case for N == 0 */

3:      b           $lslen_done
	copy        %r24,%r26    /* reset r26 so 0 is returned on fault */

	.section __ex_table,"a"
#ifdef __LP64__
	.dword      1b,(3b-1b)
	.dword      2b,(3b-2b)
#else
	.word       1b,(3b-1b)
	.word       2b,(3b-2b)
#endif
	.previous

	.procend

	.end
