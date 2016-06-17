/*
 * linux/kernel/info.c
 *
 * Copyright (C) 1992 Darren Senn
 */

/* This implements the sysinfo() system call */

#include <linux/mm.h>
#include <linux/unistd.h>
#include <linux/swap.h>
#include <linux/smp_lock.h>

#include <asm/uaccess.h>

asmlinkage long sys_sysinfo(struct sysinfo *info)
{
	struct sysinfo val;

	memset((char *)&val, 0, sizeof(struct sysinfo));

	cli();
	val.uptime = jiffies / HZ;

	val.loads[0] = avenrun[0] << (SI_LOAD_SHIFT - FSHIFT);
	val.loads[1] = avenrun[1] << (SI_LOAD_SHIFT - FSHIFT);
	val.loads[2] = avenrun[2] << (SI_LOAD_SHIFT - FSHIFT);

	val.procs = nr_threads-1;
	sti();

	si_meminfo(&val);
	si_swapinfo(&val);

	{
		unsigned long mem_total, sav_total;
		unsigned int mem_unit, bitcount;

		/* If the sum of all the available memory (i.e. ram + swap)
		 * is less than can be stored in a 32 bit unsigned long then
		 * we can be binary compatible with 2.2.x kernels.  If not,
		 * well, in that case 2.2.x was broken anyways...
		 *
		 *  -Erik Andersen <andersee@debian.org> */

		mem_total = val.totalram + val.totalswap;
		if (mem_total < val.totalram || mem_total < val.totalswap)
			goto out;
		bitcount = 0;
		mem_unit = val.mem_unit;
		while (mem_unit > 1) {
			bitcount++;
			mem_unit >>= 1;
			sav_total = mem_total;
			mem_total <<= 1;
			if (mem_total < sav_total)
				goto out;
		}

		/* If mem_total did not overflow, multiply all memory values by
		 * val.mem_unit and set it to 1.  This leaves things compatible
		 * with 2.2.x, and also retains compatibility with earlier 2.4.x
		 * kernels...  */

		val.mem_unit = 1;
		val.totalram <<= bitcount;
		val.freeram <<= bitcount;
		val.sharedram <<= bitcount;
		val.bufferram <<= bitcount;
		val.totalswap <<= bitcount;
		val.freeswap <<= bitcount;
		val.totalhigh <<= bitcount;
		val.freehigh <<= bitcount;
	}
out:
	if (copy_to_user(info, &val, sizeof(struct sysinfo)))
		return -EFAULT;
	return 0;
}
