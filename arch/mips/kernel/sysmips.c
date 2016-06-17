/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1995, 1996, 1997, 2000, 2001 by Ralf Baechle
 * Copyright (C) 2001 MIPS Technologies, Inc.
 */
#include <linux/errno.h>
#include <linux/linkage.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/utsname.h>

#include <asm/cachectl.h>
#include <asm/pgalloc.h>
#include <asm/sysmips.h>
#include <asm/uaccess.h>

extern asmlinkage void syscall_trace(void);

/*
 * How long a hostname can we get from user space?
 *  -EFAULT if invalid area or too long
 *  0 if ok
 *  >0 EFAULT after xx bytes
 */
static inline int
get_max_hostname(unsigned long address)
{
	struct vm_area_struct * vma;

	vma = find_vma(current->mm, address);
	if (!vma || vma->vm_start > address || !(vma->vm_flags & VM_READ))
		return -EFAULT;
	address = vma->vm_end - address;
	if (address > PAGE_SIZE)
		return 0;
	if (vma->vm_next && vma->vm_next->vm_start == vma->vm_end &&
	   (vma->vm_next->vm_flags & VM_READ))
		return 0;
	return address;
}

asmlinkage int
_sys_sysmips(int cmd, int arg1, int arg2, int arg3)
{
	char	*name;
	int	tmp, len, retval;

	switch(cmd) {
	case SETNAME: {
		char nodename[__NEW_UTS_LEN + 1];

		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;

		name = (char *) arg1;

		len = strncpy_from_user(nodename, name, sizeof(nodename));
		if (len < 0)
			return -EFAULT;

		down_write(&uts_sem);
		strncpy(system_utsname.nodename, nodename, len);
		system_utsname.nodename[len] = '\0';
		up_write(&uts_sem);
		return 0;
	}

	case MIPS_ATOMIC_SET:
		printk(KERN_CRIT "How did I get here?\n");
		retval = -EINVAL;
		goto out;

	case MIPS_FIXADE:
		tmp = current->thread.mflags & ~3;
		current->thread.mflags = tmp | (arg1 & 3);
		retval = 0;
		goto out;

	case FLUSH_CACHE:
		__flush_cache_all();
		retval = 0;
		goto out;

	case MIPS_RDNVRAM:
		retval = -EIO;
		goto out;

	default:
		retval = -EINVAL;
		goto out;
	}

out:
	return retval;
}

/*
 * No implemented yet ...
 */
asmlinkage int
sys_cachectl(char *addr, int nbytes, int op)
{
	return -ENOSYS;
}

asmlinkage int sys_pause(void)
{
	current->state = TASK_INTERRUPTIBLE;
	schedule();
	return -ERESTARTNOHAND;
}
