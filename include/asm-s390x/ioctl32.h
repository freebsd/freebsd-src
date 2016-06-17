/*
 * include/asm-s390/ioctl32.h
 * include/asm-s390x/ioctl32.h
 *
 *         Copyright (C) 2003 IBM Corporation
 *         Author: Arnd Bergmann <arndb@de.ibm.com>
 */
#ifndef ASM_IOCTL32_H
#define ASM_IOCTL32_H

extern int sys_ioctl(unsigned int, unsigned int, unsigned long, struct file*);
typedef int (*ioctl_trans_handler_t)(unsigned int, unsigned int, unsigned long, struct file *);

#ifdef CONFIG_S390_SUPPORT

extern int
register_ioctl32_conversion(unsigned int cmd, ioctl_trans_handler_t handler);

extern void
unregister_ioctl32_conversion(unsigned int cmd);

#else

static inline int
register_ioctl32_conversion(unsigned int cmd, ioctl_trans_handler_t handler)
{
	return 0;
}

static inline void 
unregister_ioctl32_conversion(unsigned int cmd)
{
}

#endif /* CONFIG_S390_SUPPORT */
#endif /* ASM_IOCTL32_H */
