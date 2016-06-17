/*
 *  arch/s390/kernel/strncpy.S
 *    S390 strncpy routine
 *
 *  S390 version
 *    Copyright (C) 1999 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Martin Schwidefsky (schwidefsky@de.ibm.com),
 */

/*
 * R2 = address of destination
 * R3 = address of source string
 * R4 = max number of bytes to copy
 */
        .globl   strncpy
strncpy:
        LR      1,2            # don't touch address in R2
	LTR     4,4
        JZ      strncpy_exit   # 0 bytes -> nothing to do
	SR      0,0
strncpy_loop:
        ICM     0,1,0(3)       # ICM sets the cc, IC does not
	LA      3,1(3)
        STC     0,0(1)
	LA      1,1(1)
        JZ      strncpy_exit   # ICM inserted a 0x00
        BRCT    4,strncpy_loop # R4 -= 1, jump to strncpy_loop if >  0
strncpy_exit:
        BR      14

