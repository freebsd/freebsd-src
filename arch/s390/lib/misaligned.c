/*
 *  arch/s390/lib/misaligned.c
 *    S390 misalignment panic stubs
 *
 *  S390 version
 *    Copyright (C) 2001 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Martin Schwidefsky (schwidefsky@de.ibm.com).
 *
 * xchg wants to panic if the pointer is not aligned. To avoid multiplying
 * the panic message over and over again, the panic is done in the helper
 * functions __misaligned_u32 and __misaligned_u16.
 */

#include <linux/module.h> 
#include <linux/kernel.h>

void __misaligned_u16(void)
{
	panic("misaligned (__u16 *) in __xchg\n");
}

void __misaligned_u32(void)
{
	panic("misaligned (__u32 *) in __xchg\n");
}

EXPORT_SYMBOL(__misaligned_u16);
EXPORT_SYMBOL(__misaligned_u32);

