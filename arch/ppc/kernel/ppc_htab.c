/*
 * PowerPC hash table management proc entry.  Will show information
 * about the current hash table and will allow changes to it.
 *
 * Written by Cort Dougan (cort@cs.nmt.edu)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/config.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/stat.h>
#include <linux/sysctl.h>
#include <linux/ctype.h>
#include <linux/threads.h>

#include <asm/uaccess.h>
#include <asm/bitops.h>
#include <asm/mmu.h>
#include <asm/processor.h>
#include <asm/residual.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/cputable.h>
#include <asm/system.h>

static ssize_t ppc_htab_read(struct file * file, char * buf,
			     size_t count, loff_t *ppos);
static ssize_t ppc_htab_write(struct file * file, const char * buffer,
			      size_t count, loff_t *ppos);
static long long ppc_htab_lseek(struct file * file, loff_t offset, int orig);
int proc_dol2crvec(ctl_table *table, int write, struct file *filp,
		  void *buffer, size_t *lenp);

extern PTE *Hash, *Hash_end;
extern unsigned long Hash_size, Hash_mask;
extern unsigned long _SDR1;
extern unsigned long htab_reloads;
extern unsigned long htab_preloads;
extern unsigned long htab_evicts;
extern unsigned long pte_misses;
extern unsigned long pte_errors;
extern unsigned int primary_pteg_full;
extern unsigned int htab_hash_searches;

/* these will go into processor.h when I'm done debugging -- Cort */
#define MMCR0 952
#define MMCR0_PMC1_CYCLES (0x1<<7)
#define MMCR0_PMC1_ICACHEMISS (0x5<<7)
#define MMCR0_PMC1_DTLB (0x6<<7)
#define MMCR0_PMC2_DCACHEMISS (0x6)
#define MMCR0_PMC2_CYCLES (0x1)
#define MMCR0_PMC2_ITLB (0x7)
#define MMCR0_PMC2_LOADMISSTIME (0x5)

#define PMC1 953
#define PMC2 954

struct file_operations ppc_htab_operations = {
        llseek:         ppc_htab_lseek,
        read:           ppc_htab_read,
        write:          ppc_htab_write,
};

static char *pmc1_lookup(unsigned long mmcr0)
{
	switch ( mmcr0 & (0x7f<<7) )
	{
	case 0x0:
		return "none";
	case MMCR0_PMC1_CYCLES:
		return "cycles";
	case MMCR0_PMC1_ICACHEMISS:
		return "ic miss";
	case MMCR0_PMC1_DTLB:
		return "dtlb miss";
	default:
		return "unknown";
	}
}

static char *pmc2_lookup(unsigned long mmcr0)
{
	switch ( mmcr0 & 0x3f )
	{
	case 0x0:
		return "none";
	case MMCR0_PMC2_CYCLES:
		return "cycles";
	case MMCR0_PMC2_DCACHEMISS:
		return "dc miss";
	case MMCR0_PMC2_ITLB:
		return "itlb miss";
	case MMCR0_PMC2_LOADMISSTIME:
		return "load miss time";
	default:
		return "unknown";
	}
}

/*
 * print some useful info about the hash table.  This function
 * is _REALLY_ slow (see the nested for loops below) but nothing
 * in here should be really timing critical. -- Cort
 */
static ssize_t ppc_htab_read(struct file * file, char * buf,
			     size_t count, loff_t *ppos)
{
	unsigned long mmcr0 = 0, pmc1 = 0, pmc2 = 0;
	int n = 0;
#if defined(CONFIG_PPC_STD_MMU) && !defined(CONFIG_PPC64BRIDGE)
	int valid;
	unsigned int kptes = 0, uptes = 0, zombie_ptes = 0;
	PTE *ptr;
	struct task_struct *p;
#endif /* CONFIG_PPC_STD_MMU */
	char buffer[512];

	if (count < 0)
		return -EINVAL;

	if (cur_cpu_spec[0]->cpu_features & CPU_FTR_604_PERF_MON) {
		asm volatile ("mfspr %0,952 \n\t"
		    "mfspr %1,953 \n\t"
		    "mfspr %2,954 \n\t"
		    : "=r" (mmcr0), "=r" (pmc1), "=r" (pmc2) );
		n += sprintf( buffer + n,
			      "604 Performance Monitoring\n"
			      "MMCR0\t\t: %08lx %s%s ",
			      mmcr0,
			      ( mmcr0>>28 & 0x2 ) ? "(user mode counted)" : "",
			      ( mmcr0>>28 & 0x4 ) ? "(kernel mode counted)" : "");
		n += sprintf( buffer + n,
			      "\nPMC1\t\t: %08lx (%s)\n"
			      "PMC2\t\t: %08lx (%s)\n",
			      pmc1, pmc1_lookup(mmcr0),
			      pmc2, pmc2_lookup(mmcr0));
	}

#ifdef CONFIG_PPC_STD_MMU
	/* if we don't have a htab */
	if ( Hash_size == 0 )
	{
		n += sprintf( buffer + n, "No Hash Table used\n");
		goto return_string;
	}

#ifndef CONFIG_PPC64BRIDGE
	for ( ptr = Hash ; ptr < Hash_end ; ptr++)
	{
		unsigned int ctx, mctx, vsid;

		if (!ptr->v)
			continue;
		/* make sure someone is using this context/vsid */
		/* first undo the esid skew */
		vsid = ptr->vsid;
		mctx = ((vsid - (vsid & 0xf) * 0x111) >> 4) & 0xfffff;
		if (mctx == 0) {
			kptes++;
			continue;
		}
		/* now undo the context skew; 801921 * 897 == 1 mod 2^20 */
		ctx = (mctx * 801921) & 0xfffff;
		valid = 0;
		for_each_task(p) {
			if (p->mm != NULL && ctx == p->mm->context) {
				valid = 1;
				uptes++;
				break;
			}
		}
		if (!valid)
			zombie_ptes++;
	}
#endif

	n += sprintf( buffer + n,
		      "PTE Hash Table Information\n"
		      "Size\t\t: %luKb\n"
		      "Buckets\t\t: %lu\n"
 		      "Address\t\t: %08lx\n"
		      "Entries\t\t: %lu\n"
#ifndef CONFIG_PPC64BRIDGE
		      "User ptes\t: %u\n"
		      "Kernel ptes\t: %u\n"
		      "Zombies\t\t: %u\n"
		      "Percent full\t: %lu%%\n"
#endif
                      , (unsigned long)(Hash_size>>10),
		      (Hash_size/(sizeof(PTE)*8)),
		      (unsigned long)Hash,
		      Hash_size/sizeof(PTE)
#ifndef CONFIG_PPC64BRIDGE
                      , uptes,
		      kptes,
		      zombie_ptes,
		      ((kptes+uptes)*100) / (Hash_size/sizeof(PTE))
#endif
		);

	n += sprintf( buffer + n,
		      "Reloads\t\t: %lu\n"
		      "Preloads\t: %lu\n"
		      "Searches\t: %u\n"
		      "Overflows\t: %u\n"
		      "Evicts\t\t: %lu\n",
		      htab_reloads, htab_preloads, htab_hash_searches,
		      primary_pteg_full, htab_evicts);
return_string:
#endif /* CONFIG_PPC_STD_MMU */

	n += sprintf( buffer + n,
		      "Non-error misses: %lu\n"
		      "Error misses\t: %lu\n",
		      pte_misses, pte_errors);
	if (*ppos >= strlen(buffer))
		return 0;
	if (n > strlen(buffer) - *ppos)
		n = strlen(buffer) - *ppos;
	if (n > count)
		n = count;
	copy_to_user(buf, buffer + *ppos, n);
	*ppos += n;
	return n;
}

/*
 * Allow user to define performance counters and resize the hash table
 */
static ssize_t ppc_htab_write(struct file * file, const char * buffer,
			      size_t count, loff_t *ppos)
{
#ifdef CONFIG_PPC_STD_MMU
	unsigned long tmp;
	if ( current->uid != 0 )
		return -EACCES;
	/* don't set the htab size for now */
	if ( !strncmp( buffer, "size ", 5) )
		return -EBUSY;

	/* turn off performance monitoring */
	if ( !strncmp( buffer, "off", 3) )
	{
		if (cur_cpu_spec[0]->cpu_features & CPU_FTR_604_PERF_MON) {
			asm volatile ("mtspr %0, %3 \n\t"
			    "mtspr %1, %3 \n\t"
			    "mtspr %2, %3 \n\t"
			    :: "i" (MMCR0), "i" (PMC1), "i" (PMC2), "r" (0));
		}
	}

	if ( !strncmp( buffer, "reset", 5) )
	{
		if (cur_cpu_spec[0]->cpu_features & CPU_FTR_604_PERF_MON) {
			/* reset PMC1 and PMC2 */
			asm volatile (
				"mtspr 953, %0 \n\t"
				"mtspr 954, %0 \n\t"
				:: "r" (0));
		}
		htab_reloads = 0;
		htab_evicts = 0;
		pte_misses = 0;
		pte_errors = 0;
	}

	if ( !strncmp( buffer, "user", 4) )
	{
		if (cur_cpu_spec[0]->cpu_features & CPU_FTR_604_PERF_MON) {
			/* setup mmcr0 and clear the correct pmc */
			asm("mfspr %0,%1\n\t"  : "=r" (tmp) : "i" (MMCR0));
			tmp &= ~(0x60000000);
			tmp |= 0x20000000;
			asm volatile (
				"mtspr %1,%0 \n\t"    /* set new mccr0 */
				"mtspr %3,%4 \n\t"    /* reset the pmc */
				"mtspr %5,%4 \n\t"    /* reset the pmc2 */
				:: "r" (tmp), "i" (MMCR0), "i" (0),
				"i" (PMC1),  "r" (0), "i"(PMC2) );
		}
	}

	if ( !strncmp( buffer, "kernel", 6) )
	{
		if (cur_cpu_spec[0]->cpu_features & CPU_FTR_604_PERF_MON) {
			/* setup mmcr0 and clear the correct pmc */
			asm("mfspr %0,%1\n\t"  : "=r" (tmp) : "i" (MMCR0));
			tmp &= ~(0x60000000);
			tmp |= 0x40000000;
			asm volatile (
				"mtspr %1,%0 \n\t"    /* set new mccr0 */
				"mtspr %3,%4 \n\t"    /* reset the pmc */
				"mtspr %5,%4 \n\t"    /* reset the pmc2 */
				:: "r" (tmp), "i" (MMCR0), "i" (0),
				"i" (PMC1),  "r" (0), "i"(PMC2) );
		}
	}

	/* PMC1 values */
	if ( !strncmp( buffer, "dtlb", 4) )
	{
		if (cur_cpu_spec[0]->cpu_features & CPU_FTR_604_PERF_MON) {
			/* setup mmcr0 and clear the correct pmc */
			asm("mfspr %0,%1\n\t"  : "=r" (tmp) : "i" (MMCR0));
			tmp &= ~(0x7f<<7);
			tmp |= MMCR0_PMC1_DTLB;
			asm volatile (
				"mtspr %1,%0 \n\t"    /* set new mccr0 */
				"mtspr %3,%4 \n\t"    /* reset the pmc */
				:: "r" (tmp), "i" (MMCR0), "i" (MMCR0_PMC1_DTLB),
				"i" (PMC1),  "r" (0) );
		}
	}

	if ( !strncmp( buffer, "ic miss", 7) )
	{
		if (cur_cpu_spec[0]->cpu_features & CPU_FTR_604_PERF_MON) {
			/* setup mmcr0 and clear the correct pmc */
			asm("mfspr %0,%1\n\t"  : "=r" (tmp) : "i" (MMCR0));
			tmp &= ~(0x7f<<7);
			tmp |= MMCR0_PMC1_ICACHEMISS;
			asm volatile (
				"mtspr %1,%0 \n\t"    /* set new mccr0 */
				"mtspr %3,%4 \n\t"    /* reset the pmc */
				:: "r" (tmp), "i" (MMCR0),
				"i" (MMCR0_PMC1_ICACHEMISS), "i" (PMC1),  "r" (0));
		}
	}

	/* PMC2 values */
	if ( !strncmp( buffer, "load miss time", 14) )
	{
		if (cur_cpu_spec[0]->cpu_features & CPU_FTR_604_PERF_MON) {
			/* setup mmcr0 and clear the correct pmc */
		       asm volatile(
			       "mfspr %0,%1\n\t"     /* get current mccr0 */
			       "rlwinm %0,%0,0,0,31-6\n\t"  /* clear bits [26-31] */
			       "ori   %0,%0,%2 \n\t" /* or in mmcr0 settings */
			       "mtspr %1,%0 \n\t"    /* set new mccr0 */
			       "mtspr %3,%4 \n\t"    /* reset the pmc */
			       : "=r" (tmp)
			       : "i" (MMCR0), "i" (MMCR0_PMC2_LOADMISSTIME),
			       "i" (PMC2),  "r" (0) );
		}
	}

	if ( !strncmp( buffer, "itlb", 4) )
	{
		if (cur_cpu_spec[0]->cpu_features & CPU_FTR_604_PERF_MON) {
			/* setup mmcr0 and clear the correct pmc */
		       asm volatile(
			       "mfspr %0,%1\n\t"     /* get current mccr0 */
			       "rlwinm %0,%0,0,0,31-6\n\t"  /* clear bits [26-31] */
			       "ori   %0,%0,%2 \n\t" /* or in mmcr0 settings */
			       "mtspr %1,%0 \n\t"    /* set new mccr0 */
			       "mtspr %3,%4 \n\t"    /* reset the pmc */
			       : "=r" (tmp)
			       : "i" (MMCR0), "i" (MMCR0_PMC2_ITLB),
			       "i" (PMC2),  "r" (0) );
		}
	}

	if ( !strncmp( buffer, "dc miss", 7) )
	{
		if (cur_cpu_spec[0]->cpu_features & CPU_FTR_604_PERF_MON) {
			/* setup mmcr0 and clear the correct pmc */
		       asm volatile(
			       "mfspr %0,%1\n\t"     /* get current mccr0 */
			       "rlwinm %0,%0,0,0,31-6\n\t"  /* clear bits [26-31] */
			       "ori   %0,%0,%2 \n\t" /* or in mmcr0 settings */
			       "mtspr %1,%0 \n\t"    /* set new mccr0 */
			       "mtspr %3,%4 \n\t"    /* reset the pmc */
			       : "=r" (tmp)
			       : "i" (MMCR0), "i" (MMCR0_PMC2_DCACHEMISS),
			       "i" (PMC2),  "r" (0) );
		}
	}


	return count;

#if 0 /* resizing htab is a bit difficult right now -- Cort */
	unsigned long size;
	extern void reset_SDR1(void);

	/* only know how to set size right now */
	if ( strncmp( buffer, "size ", 5) )
		return -EINVAL;

	size = simple_strtoul( &buffer[5], NULL, 10 );

	/* only allow to shrink */
	if ( size >= Hash_size>>10 )
		return -EINVAL;

	/* minimum size of htab */
	if ( size < 64 )
		return -EINVAL;

	/* make sure it's a multiple of 64k */
	if ( size % 64 )
		return -EINVAL;

	printk("Hash table resize to %luk\n", size);
	/*
	 * We need to rehash all kernel entries for the new htab size.
	 * Kernel only since we do a flush_tlb_all().  Since it's kernel
	 * we only need to bother with vsids 0-15.  To avoid problems of
	 * clobbering un-rehashed values we put the htab at a new spot
	 * and put everything there.
	 * -- Cort
	 */
	Hash_size = size<<10;
	Hash_mask = (Hash_size >> 6) - 1;
        _SDR1 = __pa(Hash) | (Hash_mask >> 10);
	flush_tlb_all();

	reset_SDR1();
#endif
	return count;
#else /* CONFIG_PPC_STD_MMU */
	return 0;
#endif /* CONFIG_PPC_STD_MMU */
}


static long long
ppc_htab_lseek(struct file * file, loff_t offset, int orig)
{
    switch (orig) {
    case 0:
	file->f_pos = offset;
	return(file->f_pos);
    case 1:
	file->f_pos += offset;
	return(file->f_pos);
    case 2:
	return(-EINVAL);
    default:
	return(-EINVAL);
    }
}

#define TMPBUFLEN 512
int proc_dol2crvec(ctl_table *table, int write, struct file *filp,
		  void *buffer, size_t *lenp)
{
	int vleft, first=1, len, left, val;
	char buf[TMPBUFLEN], *p;
	static const char *sizestrings[4] = {
		"2MB", "256KB", "512KB", "1MB"
	};
	static const char *clockstrings[8] = {
		"clock disabled", "+1 clock", "+1.5 clock", "+3.5 clock",
		"+2 clock", "+2.5 clock", "+3 clock", "+4 clock"
	};
	static const char *typestrings[4] = {
		"flow-through burst SRAM", "reserved SRAM",
		"pipelined burst SRAM", "pipelined late-write SRAM"
	};
	static const char *holdstrings[4] = {
		"0.5", "1.0", "(reserved2)", "(reserved3)"
	};

	if (!(cur_cpu_spec[0]->cpu_features & CPU_FTR_L2CR))
		return -EFAULT;

	if ( /*!table->maxlen ||*/ (filp->f_pos && !write)) {
		*lenp = 0;
		return 0;
	}

	vleft = table->maxlen / sizeof(int);
	left = *lenp;

	for (; left /*&& vleft--*/; first=0) {
		if (write) {
			while (left) {
				char c;
				if(get_user(c,(char *) buffer))
					return -EFAULT;
				if (!isspace(c))
					break;
				left--;
				((char *) buffer)++;
			}
			if (!left)
				break;
			len = left;
			if (len > TMPBUFLEN-1)
				len = TMPBUFLEN-1;
			if(copy_from_user(buf, buffer, len))
				return -EFAULT;
			buf[len] = 0;
			p = buf;
			if (*p < '0' || *p > '9')
				break;
			val = simple_strtoul(p, &p, 0);
			len = p-buf;
			if ((len < left) && *p && !isspace(*p))
				break;
			buffer += len;
			left -= len;
			_set_L2CR(val);
		} else {
			int is750fx = cur_cpu_spec[0]->cpu_features & CPU_FTR_750FX;
			p = buf;
			if (!first)
				*p++ = '\t';
			val = _get_L2CR();
			p += sprintf(p, "0x%08x: ", val);
			p += sprintf(p, " L2 %s, ", (val >> 31) & 1 ? "enabled" :
				     	"disabled");
			if (!(val>>30&1))
				p += sprintf(p, "no ");
			if (is750fx)
				p += sprintf(p, "ECC checkstop");
			else
				p += sprintf(p, "parity");

			/* 75x & 74x0 have different L2CR than 745x */
			if (!(cur_cpu_spec[0]->cpu_features &
						CPU_FTR_SPEC7450)) {
				if (!is750fx) {
					p += sprintf(p, ", %s",
						     sizestrings[(val >> 28) & 3]);
					p += sprintf(p, ", %s",
						     clockstrings[(val >> 25) & 7]);
					p += sprintf(p, ", %s",
						     typestrings[(val >> 23) & 3]);
				}
				p += sprintf(p, "%s", (val>>22)&1 ?
					     ", data only" : "");
				if (!is750fx) {
					p += sprintf(p, "%s", (val>>20)&1 ?
						     ", ZZ enabled": "");
				}
				p += sprintf(p, ", %s", (val>>19)&1 ?
					"write-through" : "copy-back");
				p += sprintf(p, "%s", (val>>18)&1 ?
					", testing" : "");
				if (!is750fx) {
					p += sprintf(p, ", %sns hold",
						     holdstrings[(val>>16)&3]);
					p += sprintf(p, "%s", (val>>15)&1 ?
						     ", DLL slow" : "");
					p += sprintf(p, "%s", (val>>14)&1 ?
						     ", diff clock" :"");
					p += sprintf(p, "%s", (val>>13)&1 ?
						     ", DLL bypass" :"");
				} else {
					if ((val>>11)&1)
						p += sprintf(p, ", lock way 0");
					if ((val>>10)&1)
						p += sprintf(p, ", lock way 1");
					if ((val>>9)&1)
						p += sprintf(p, ", Snoop Hit in Locked Line Error Enabled");
				}
			} else { /* 745x */
				p += sprintf(p, ", %sinstn only", (val>>20)&1 ?
					"" : "no ");
				p += sprintf(p, ", %sdata only", (val>>16)&1 ?
					"" : "no ");
				p += sprintf(p, ", %s replacement",
					(val>>12)&1 ?  "secondary" : "default");
			}

			p += sprintf(p,"\n");

			len = strlen(buf);
			if (len > left)
				len = left;
			if(copy_to_user(buffer, buf, len))
				return -EFAULT;
			left -= len;
			buffer += len;
			break;
		}
	}

	if (!write && !first && left) {
		if(put_user('\n', (char *) buffer))
			return -EFAULT;
		left--, buffer++;
	}
	if (write) {
		p = (char *) buffer;
		while (left) {
			char c;
			if(get_user(c, p++))
				return -EFAULT;
			if (!isspace(c))
				break;
			left--;
		}
	}
	if (write && first)
		return -EINVAL;
	*lenp -= left;
	filp->f_pos += *lenp;
	return 0;
}

int proc_dol3crvec(ctl_table *table, int write, struct file *filp,
		  void *buffer, size_t *lenp)
{
	int vleft, first=1, len, left, val;
	char buf[TMPBUFLEN], *p;
	static const char *clockstrings[8] = {
		"+6 clock", "reserved(1)", "+2 clock", "+2.5 clock",
		"+3 clock", "+3.5 clock", "+4 clock", "+5 clock"
	};
	static const char *clocksampstrings[4] = {
		"2 clock", "3 clock", "4 clock", "5 clock"
	};
	static const char *pclocksampstrings[8] = {
		"0 P-clock", "1 P-clock", "2 P-clock", "3 P-clock",
		"4 P-clock", "5 P-clock", "reserved(6)", "reserved(7)"
	};
	static const char *typestrings[4] = {
		"MSUG2 DDR SRAM",
		"Pipelined synchronous late-write SRAM",
		"Reserved", "PB2 SRAM"
	};

	if (!(cur_cpu_spec[0]->cpu_features & CPU_FTR_L3CR))
		return -EFAULT;
	if (write)
		return -EFAULT;

	if (filp->f_pos && !write) {
		*lenp = 0;
		return 0;
	}

	vleft = table->maxlen / sizeof(int);
	left = *lenp;

	for (; left; first=0) {
		p = buf;
		if (!first)
			*p++ = '\t';
		val = _get_L3CR();
		p += sprintf(p, "0x%08x: ", val);
		p += sprintf(p, " L3 %s", (val >> 31) & 1 ? "enabled" :
			     	"disabled");
		p += sprintf(p, ", %sdata parity", (val>>30)&1 ? "" :
				"no ");
		p += sprintf(p, ", %saddr parity", (val>>29)&1 ? "" :
				"no ");
		p += sprintf(p, ", %s", (val>>28)&1 ? "2MB" : "1MB");
		p += sprintf(p, ", clocks %s", (val>>27)&1 ? "enabled" :
				"disabled");
		p += sprintf(p, ", %s", clockstrings[(val >> 23) & 7]);
		p += sprintf(p, ", %sinstn only", (val>>22)&1 ? "" :
				"no ");
		p += sprintf(p, ", %ssample point override",
				(val>>18)&1 ? "" : "no ");
		p += sprintf(p, ", %s sample point",
				clocksampstrings[(val>>16)&3]);
		p += sprintf(p, ", %s sample point",
				pclocksampstrings[(val>>13)&7]);
		p += sprintf(p, ", %s replacement", (val>>12)&1 ?
				"secondary" : "default");
		p += sprintf(p, ", %s", typestrings[(val >> 8) & 3]);
		p += sprintf(p, ", %sclock cntl", (val>>7)&1 ? "" :
				"no ");
		p += sprintf(p, ", %sdata only", (val>>6)&1 ? "" :
				"no ");
		p += sprintf(p, ", private mem %s", (val>>2)&1 ?
				"enabled" : "disabled");
		p += sprintf(p, ", %sprivate mem", val&1 ? "2MB " :
				"1MB ");
		p += sprintf(p,"\n");

		len = strlen(buf);
		if (len > left)
			len = left;
		if(copy_to_user(buffer, buf, len))
			return -EFAULT;
		left -= len;
		buffer += len;
		break;
	}

	if (!write && !first && left) {
		if(put_user('\n', (char *) buffer))
			return -EFAULT;
		left--, buffer++;
	}
	*lenp -= left;
	filp->f_pos += *lenp;
	return 0;
}
