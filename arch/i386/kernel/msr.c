#ident "$Id$"
/* ----------------------------------------------------------------------- *
 *   
 *   Copyright 2000 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 675 Mass Ave, Cambridge MA 02139,
 *   USA; either version 2 of the License, or (at your option) any later
 *   version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * msr.c
 *
 * x86 MSR access device
 *
 * This device is accessed by lseek() to the appropriate register number
 * and then read/write in chunks of 8 bytes.  A larger size means multiple
 * reads or writes of the same register.
 *
 * This driver uses /dev/cpu/%d/msr where %d is the minor number, and on
 * an SMP box will direct the access to CPU %d.
 */

#include <linux/module.h>
#include <linux/config.h>

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/smp.h>
#include <linux/major.h>

#include <asm/processor.h>
#include <asm/msr.h>
#include <asm/uaccess.h>
#include <asm/system.h>

/* Note: "err" is handled in a funny way below.  Otherwise one version
   of gcc or another breaks. */

static inline int wrmsr_eio(u32 reg, u32 eax, u32 edx)
{
  int err;

  asm volatile(
	       "1:	wrmsr\n"
	       "2:\n"
	       ".section .fixup,\"ax\"\n"
	       "3:	movl %4,%0\n"
	       "	jmp 2b\n"
	       ".previous\n"
	       ".section __ex_table,\"a\"\n"
	       "	.align 4\n"
	       "	.long 1b,3b\n"
	       ".previous"
	       : "=&bDS" (err)
	       : "a" (eax), "d" (edx), "c" (reg), "i" (-EIO), "0" (0));

  return err;
}

static inline int rdmsr_eio(u32 reg, u32 *eax, u32 *edx)
{
  int err;

  asm volatile(
	       "1:	rdmsr\n"
	       "2:\n"
	       ".section .fixup,\"ax\"\n"
	       "3:	movl %4,%0\n"
	       "	jmp 2b\n"
	       ".previous\n"
	       ".section __ex_table,\"a\"\n"
	       "	.align 4\n"
	       "	.long 1b,3b\n"
	       ".previous"
	       : "=&bDS" (err), "=a" (*eax), "=d" (*edx)
	       : "c" (reg), "i" (-EIO), "0" (0));

  return err;
}

#ifdef CONFIG_SMP

struct msr_command {
  int cpu;
  int err;
  u32 reg;
  u32 data[2];
};

static void msr_smp_wrmsr(void *cmd_block)
{
  struct msr_command *cmd = (struct msr_command *) cmd_block;
  
  if ( cmd->cpu == smp_processor_id() )
    cmd->err = wrmsr_eio(cmd->reg, cmd->data[0], cmd->data[1]);
}

static void msr_smp_rdmsr(void *cmd_block)
{
  struct msr_command *cmd = (struct msr_command *) cmd_block;
  
  if ( cmd->cpu == smp_processor_id() )
    cmd->err = rdmsr_eio(cmd->reg, &cmd->data[0], &cmd->data[1]);
}

static inline int do_wrmsr(int cpu, u32 reg, u32 eax, u32 edx)
{
  struct msr_command cmd;

  if ( cpu == smp_processor_id() ) {
    return wrmsr_eio(reg, eax, edx);
  } else {
    cmd.cpu = cpu;
    cmd.reg = reg;
    cmd.data[0] = eax;
    cmd.data[1] = edx;
    
    smp_call_function(msr_smp_wrmsr, &cmd, 1, 1);
    return cmd.err;
  }
}

static inline int do_rdmsr(int cpu, u32 reg, u32 *eax, u32 *edx)
{
  struct msr_command cmd;

  if ( cpu == smp_processor_id() ) {
    return rdmsr_eio(reg, eax, edx);
  } else {
    cmd.cpu = cpu;
    cmd.reg = reg;

    smp_call_function(msr_smp_rdmsr, &cmd, 1, 1);
    
    *eax = cmd.data[0];
    *edx = cmd.data[1];

    return cmd.err;
  }
}

#else /* ! CONFIG_SMP */

static inline int do_wrmsr(int cpu, u32 reg, u32 eax, u32 edx)
{
  return wrmsr_eio(reg, eax, edx);
}

static inline int do_rdmsr(int cpu, u32 reg, u32 *eax, u32 *edx)
{
  return rdmsr_eio(reg, eax, edx);
}

#endif /* ! CONFIG_SMP */

static loff_t msr_seek(struct file *file, loff_t offset, int orig)
{
  switch (orig) {
  case 0:
    file->f_pos = offset;
    return file->f_pos;
  case 1:
    file->f_pos += offset;
    return file->f_pos;
  default:
    return -EINVAL;	/* SEEK_END not supported */
  }
}

static ssize_t msr_read(struct file * file, char * buf,
			size_t count, loff_t *ppos)
{
  u32 *tmp = (u32 *)buf;
  u32 data[2];
  size_t rv;
  u32 reg = *ppos;
  int cpu = MINOR(file->f_dentry->d_inode->i_rdev);
  int err;

  if ( count % 8 )
    return -EINVAL; /* Invalid chunk size */
  
  for ( rv = 0 ; count ; count -= 8 ) {
    err = do_rdmsr(cpu, reg, &data[0], &data[1]);
    if ( err )
      return err;
    if ( copy_to_user(tmp,&data,8) )
      return -EFAULT;
    tmp += 2;
  }

  return ((char *)tmp) - buf;
}

static ssize_t msr_write(struct file * file, const char * buf,
			 size_t count, loff_t *ppos)
{
  const u32 *tmp = (const u32 *)buf;
  u32 data[2];
  size_t rv;
  u32 reg = *ppos;
  int cpu = MINOR(file->f_dentry->d_inode->i_rdev);
  int err;

  if ( count % 8 )
    return -EINVAL; /* Invalid chunk size */
  
  for ( rv = 0 ; count ; count -= 8 ) {
    if ( copy_from_user(&data,tmp,8) )
      return -EFAULT;
    err = do_wrmsr(cpu, reg, data[0], data[1]);
    if ( err )
      return err;
    tmp += 2;
  }

  return ((char *)tmp) - buf;
}

static int msr_open(struct inode *inode, struct file *file)
{
  int cpu = MINOR(file->f_dentry->d_inode->i_rdev);
  struct cpuinfo_x86 *c = &(cpu_data)[cpu];
  
  if ( !(cpu_online_map & (1UL << cpu)) )
    return -ENXIO;		/* No such CPU */
  if ( !test_bit(X86_FEATURE_MSR, &c->x86_capability) )
    return -EIO;		/* MSR not supported */
  
  return 0;
}

/*
 * File operations we support
 */
static struct file_operations msr_fops = {
  owner:	THIS_MODULE,
  llseek:	msr_seek,
  read:		msr_read,
  write:	msr_write,
  open:		msr_open,
};

int __init msr_init(void)
{
  if (register_chrdev(MSR_MAJOR, "cpu/msr", &msr_fops)) {
    printk(KERN_ERR "msr: unable to get major %d for msr\n",
	   MSR_MAJOR);
    return -EBUSY;
  }
  
  return 0;
}

void __exit msr_exit(void)
{
  unregister_chrdev(MSR_MAJOR, "cpu/msr");
}

module_init(msr_init);
module_exit(msr_exit)

EXPORT_NO_SYMBOLS;

MODULE_AUTHOR("H. Peter Anvin <hpa@zytor.com>");
MODULE_DESCRIPTION("x86 generic MSR driver");
MODULE_LICENSE("GPL");
