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
 * cpuid.c
 *
 * x86 CPUID access device
 *
 * This device is accessed by lseek() to the appropriate CPUID level
 * and then read in chunks of 16 bytes.  A larger size means multiple
 * reads of consecutive levels.
 *
 * This driver uses /dev/cpu/%d/cpuid where %d is the minor number, and on
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

#ifdef CONFIG_SMP

struct cpuid_command {
  int cpu;
  u32 reg;
  u32 *data;
};

static void cpuid_smp_cpuid(void *cmd_block)
{
  struct cpuid_command *cmd = (struct cpuid_command *) cmd_block;
  
  if ( cmd->cpu == smp_processor_id() )
    cpuid(cmd->reg, &cmd->data[0], &cmd->data[1], &cmd->data[2], &cmd->data[3]);
}

static inline void do_cpuid(int cpu, u32 reg, u32 *data)
{
  struct cpuid_command cmd;
  
  if ( cpu == smp_processor_id() ) {
    cpuid(reg, &data[0], &data[1], &data[2], &data[3]);
  } else {
    cmd.cpu  = cpu;
    cmd.reg  = reg;
    cmd.data = data;
    
    smp_call_function(cpuid_smp_cpuid, &cmd, 1, 1);
  }
}
#else /* ! CONFIG_SMP */

static inline void do_cpuid(int cpu, u32 reg, u32 *data)
{
  cpuid(reg, &data[0], &data[1], &data[2], &data[3]);
}

#endif /* ! CONFIG_SMP */

static loff_t cpuid_seek(struct file *file, loff_t offset, int orig)
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

static ssize_t cpuid_read(struct file * file, char * buf,
			size_t count, loff_t *ppos)
{
  u32 *tmp = (u32 *)buf;
  u32 data[4];
  size_t rv;
  u32 reg = *ppos;
  int cpu = MINOR(file->f_dentry->d_inode->i_rdev);
  
  if ( count % 16 )
    return -EINVAL; /* Invalid chunk size */
  
  for ( rv = 0 ; count ; count -= 16 ) {
    do_cpuid(cpu, reg, data);
    if ( copy_to_user(tmp,&data,16) )
      return -EFAULT;
    tmp += 4;
    *ppos = reg++;
  }
  
  return ((char *)tmp) - buf;
}

static int cpuid_open(struct inode *inode, struct file *file)
{
  int cpu = MINOR(file->f_dentry->d_inode->i_rdev);
  struct cpuinfo_x86 *c = &(cpu_data)[cpu];

  if ( !(cpu_online_map & (1UL << cpu)) )
    return -ENXIO;		/* No such CPU */
  if ( c->cpuid_level < 0 )
    return -EIO;		/* CPUID not supported */
  
  return 0;
}

/*
 * File operations we support
 */
static struct file_operations cpuid_fops = {
  owner:	THIS_MODULE,
  llseek:	cpuid_seek,
  read:		cpuid_read,
  open:		cpuid_open,
};

int __init cpuid_init(void)
{
  if (register_chrdev(CPUID_MAJOR, "cpu/cpuid", &cpuid_fops)) {
    printk(KERN_ERR "cpuid: unable to get major %d for cpuid\n",
	   CPUID_MAJOR);
    return -EBUSY;
  }

  return 0;
}

void __exit cpuid_exit(void)
{
  unregister_chrdev(CPUID_MAJOR, "cpu/cpuid");
}

module_init(cpuid_init);
module_exit(cpuid_exit)

EXPORT_NO_SYMBOLS;

MODULE_AUTHOR("H. Peter Anvin <hpa@zytor.com>");
MODULE_DESCRIPTION("x86 generic CPUID driver");
MODULE_LICENSE("GPL");
