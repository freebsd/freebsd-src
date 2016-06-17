#ifndef _ASM_SEGMENT_H
#define _ASM_SEGMENT_H

#define __KERNEL_CS	0x10
#define __KERNEL_DS	0x18

#define __KERNEL32_CS   0x38

/* 
 * we cannot use the same code segment descriptor for user and kernel
 * even not in the long flat model, because of different DPL /kkeil 
 * GDT layout to get 64bit syscall right (sysret hardcodes gdt offsets) 
 */

#define __USER32_CS   0x23   /* 4*8+3 */ 
#define __USER_DS     0x2b   /* 5*8+3 */ 
#define __USER_CS     0x33   /* 6*8+3 */ 
#define __USER32_DS	__USER_DS 
#define __KERNEL_COMPAT32_CS 0x08

#endif
