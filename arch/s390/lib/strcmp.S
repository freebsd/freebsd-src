/*
 *  arch/s390/lib/strcmp.S
 *    S390 strcmp routine
 *
 *  S390 version
 *    Copyright (C) 1999 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Martin Schwidefsky (schwidefsky@de.ibm.com),
 */

/*
 * R2 = address of compare string
 * R3 = address of test string
 */
        .globl   strcmp
strcmp:
        SR      0,0
        SR      1,1
        CLST    2,3
        JO      .-4
        JE      strcmp_equal
        IC      0,0(3)
        IC      1,0(2)
        SR      1,0
strcmp_equal:
        LR      2,1
        BR      14
        
