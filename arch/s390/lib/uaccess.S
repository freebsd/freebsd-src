/*
 *  arch/s390/lib/uaccess.S
 *    __copy_{from|to}_user functions.
 *
 *  s390
 *    Copyright (C) 2000,2002 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Authors(s): Martin Schwidefsky (schwidefsky@de.ibm.com)
 *
 *  These functions have standard call interface
 */

#include <asm/lowcore.h>

        .text
        .align 4
        .globl __copy_from_user_asm
__copy_from_user_asm:
	lr	%r5,%r3
	sacf	512
0:	mvcle	%r2,%r4,0
	jo	0b
1:	sacf	0
	lr	%r2,%r5
	br	%r14
2:	lhi	%r1,-4096
	lr	%r3,%r4
	slr	%r3,%r1      # %r3 = %r4 + 4096
	nr	%r3,%r1      # %r3 = (%r4 + 4096) & -4096
	slr	%r3,%r4      # %r3 = #bytes to next user page boundary
	clr	%r5,%r3      # copy crosses next page boundary ?
	jnh	1b           # no, this page fauled
	# The page after the current user page might have faulted.
	# We cant't find out which page because the program check handler
	# might have callled schedule, destroying all lowcore information.
	# We retry with the shortened length.
3:	mvcle	%r2,%r4,0
	jo	3b
	j	1b
        .section __ex_table,"a"
	.long	0b,2b
	.long	3b,1b
        .previous

        .align 4
        .text
        .globl __copy_to_user_asm
__copy_to_user_asm:
	lr	%r5,%r3
	sacf	512
0:	mvcle	%r4,%r2,0
	jo	0b
1:	sacf	0
	lr	%r2,%r3
	br	%r14
2:	lhi	%r1,-4096
	lr	%r5,%r4
	slr	%r5,%r1      # %r5 = %r4 + 4096
	nr	%r5,%r1      # %r5 = (%r4 + 4096) & -4096
	slr	%r5,%r4      # %r5 = #bytes to next user page boundary
	clr	%r3,%r5      # copy crosses next page boundary ?
	jnh	1b           # no, the current page fauled
	# The page after the current user page might have faulted.
	# We cant't find out which page because the program check handler
	# might have callled schedule, destroying all lowcore information.
	# We retry with the shortened length.
3:	mvcle	%r4,%r2,0
	jo	3b
	j	1b
        .section __ex_table,"a"
	.long	0b,2b
	.long	3b,1b
        .previous

        .align 4
        .text
        .globl __clear_user_asm
__clear_user_asm:
	lr	%r4,%r2
	lr	%r5,%r3
	sr	%r2,%r2
	sr	%r3,%r3
	sacf	512
0:	mvcle	%r4,%r2,0
	jo	0b
1:	sacf	0
	br	%r14
2:	lr	%r2,%r5
	lhi	%r1,-4096
	slr	%r5,%r1      # %r5 = %r4 + 4096
	nr	%r5,%r1      # %r5 = (%r4 + 4096) & -4096
	slr	%r5,%r4      # %r5 = #bytes to next user page boundary
	clr	%r2,%r5      # copy crosses next page boundary ?
	jnh	1b           # no, the current page fauled
	# The page after the current user page might have faulted.
	# We cant't find out which page because the program check handler
	# might have callled schedule, destroying all lowcore information.
	# We retry with the shortened length.
	slr	%r2,%r5
3:	mvcle	%r4,%r2,0
	jo	3b
	j	1b
4:	alr	%r2,%r5
	j	1b
        .section __ex_table,"a"
	.long	0b,2b
        .long	3b,4b
        .previous

