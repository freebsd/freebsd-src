/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1999,2001-2003 Silicon Graphics, Inc.  All Rights Reserved.
 *
 * Module to export the system's Firmware Interface Tables, including
 * PROM revision numbers, in /proc
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/sn/simulator.h>

/* to lookup nasids */
#include <asm/sn/sn_cpuid.h>

MODULE_DESCRIPTION("PROM version reporting for /proc");
MODULE_AUTHOR("Chad Talbott");
MODULE_LICENSE("GPL");

#undef DEBUG_PROMINFO

#define TRACE_PROMINFO

#if defined(DEBUG_PROMINFO)
#  define DPRINTK(x...) printk(KERN_DEBUG x)
#else
#  define DPRINTK(x...)
#endif

#if defined(TRACE_PROMINFO) && defined(DEBUG_PROMINFO)
#  if defined(__GNUC__)
#    define TRACE()	printk(KERN_DEBUG "%s:%d:%s\n", \
			       __FILE__, __LINE__, __FUNCTION__)
#  else
#    define TRACE()	printk(KERN_DEBUG "%s:%d\n", __LINE__, __FILE__)
#  endif
#else
#  define TRACE()
#endif

/* Sub-regions determined by bits in Node Offset */
#define	LB_PROM_SPACE		0x0000000700000000ul /* Local LB PROM */

#define FIT_SIGNATURE		0x2020205f5449465ful
/* Standard Intel FIT entry types */
#define FIT_ENTRY_FIT_HEADER	0x00	/* FIT header entry */
#define FIT_ENTRY_PAL_B		0x01	/* PAL_B entry */
/* Entries 0x02 through 0x0D reserved by Intel */
#define FIT_ENTRY_PAL_A_PROC	0x0E	/* Processor-specific PAL_A entry */
#define FIT_ENTRY_PAL_A		0x0F	/* PAL_A entry, same as... */
#define FIT_ENTRY_PAL_A_GEN	0x0F	/* ...Generic PAL_A entry */
#define FIT_ENTRY_UNUSED	0x7F	/* Unused (reserved by Intel?) */
/* OEM-defined entries range from 0x10 to 0x7E. */
#define FIT_ENTRY_SAL_A		0x10	/* SAL_A entry */
#define FIT_ENTRY_SAL_B		0x11	/* SAL_B entry */
#define FIT_ENTRY_SALRUNTIME	0x12	/* SAL runtime entry */
#define FIT_ENTRY_EFI		0x1F	/* EFI entry */
#define FIT_ENTRY_FPSWA		0x20	/* embedded fpswa entry */
#define FIT_ENTRY_VMLINUX	0x21	/* embedded vmlinux entry */

#define FIT_MAJOR_SHIFT	(32 + 8)
#define FIT_MAJOR_MASK	((1 << 8) - 1)
#define FIT_MINOR_SHIFT	32
#define FIT_MINOR_MASK	((1 << 8) - 1)

#define FIT_MAJOR(q)	\
	((unsigned) ((q) >> FIT_MAJOR_SHIFT) & FIT_MAJOR_MASK)
#define FIT_MINOR(q)	\
	((unsigned) ((q) >> FIT_MINOR_SHIFT) & FIT_MINOR_MASK)

#define FIT_TYPE_SHIFT	(32 + 16)
#define FIT_TYPE_MASK	((1 << 7) - 1)

#define FIT_TYPE(q)	\
	((unsigned) ((q) >> FIT_TYPE_SHIFT) & FIT_TYPE_MASK)

#define FIT_ENTRY(type, maj, min, size)					\
	((((unsigned long)(maj) & FIT_MAJOR_MASK) << FIT_MAJOR_SHIFT) |	\
	 (((unsigned long)(min) & FIT_MINOR_MASK) << FIT_MINOR_SHIFT) |	\
	 (((unsigned long)(type) & FIT_TYPE_MASK) << FIT_TYPE_SHIFT) |	\
	 (size))

struct fit_type_map_t {
	unsigned char	type;
	const char	*name;
};

static const struct fit_type_map_t fit_entry_types[] = {
	{ FIT_ENTRY_FIT_HEADER, "FIT Header" },
	{ FIT_ENTRY_PAL_A_GEN,  "Generic PAL_A" },
	{ FIT_ENTRY_PAL_A_PROC, "Processor-specific PAL_A" },
	{ FIT_ENTRY_PAL_A,      "PAL_A" },
	{ FIT_ENTRY_PAL_B,      "PAL_B" },
	{ FIT_ENTRY_SAL_A,      "SAL_A" },
	{ FIT_ENTRY_SAL_B,      "SAL_B" },
	{ FIT_ENTRY_SALRUNTIME, "SAL runtime" },
	{ FIT_ENTRY_EFI,	"EFI" },
	{ FIT_ENTRY_VMLINUX,    "Embedded Linux" },
	{ FIT_ENTRY_FPSWA,      "Embedded FPSWA" },
	{ FIT_ENTRY_UNUSED,     "Unused" },
	{ 0xff,                 "Error" },
};

static const char *
fit_type_name(unsigned char type)
{
	struct fit_type_map_t const*mapp;

	for (mapp = fit_entry_types; mapp->type != 0xff; mapp++)
		if (type == mapp->type)
			return mapp->name;

	if ((type > FIT_ENTRY_PAL_A) && (type < FIT_ENTRY_UNUSED))
		return "OEM type";
	if ((type > FIT_ENTRY_PAL_B) && (type < FIT_ENTRY_PAL_A))
		return "Reserved";

	return "Unknown type";
}

/* These two routines read the FIT table directly from the FLASH PROM
 * on a specific node.  The PROM can only be accessed using aligned 64
 * bit reads, so we do that and then shift and mask the result to get
 * at each field.
 */
static int
dump_fit_entry(char *page, unsigned long *fentry)
{
	unsigned long q1, q2;
	unsigned type;

	TRACE();

	q1 = readq(fentry);
	q2 = readq(fentry + 1);
	type = FIT_TYPE(q2);
	return sprintf(page, "%02x %-25s %x.%02x %016lx %u\n",
		       type,
		       fit_type_name(type),
		       FIT_MAJOR(q2), FIT_MINOR(q2),
		       q1,
		       /* mult by sixteen to get size in bytes */
		       (unsigned)q2 * 16);
}

/* We assume that the fit table will be small enough that we can print
 * the whole thing into one page.  (This is true for our default 16kB
 * pages -- each entry is about 60 chars wide when printed.)  I read
 * somewhere that the maximum size of the FIT is 128 entries, so we're
 * OK except for 4kB pages (and no one is going to do that on SN
 * anyway).
 */
static int
dump_fit(char *page, unsigned long *fit)
{
	unsigned long qw;
	int nentries;
	int fentry;
	char *p;

	TRACE();

	DPRINTK("dumping fit from %p\n", (void *)fit);

	qw = readq(fit);
	DPRINTK("FIT signature: %016lx (%.8s)\n", qw, (char *)&qw);
	if (qw != FIT_SIGNATURE)
		printk(KERN_WARNING "Unrecognized FIT signature");

	qw = readq(fit + 1);
	nentries = (unsigned)qw;
	DPRINTK("number of fit entries: %u\n", nentries);
	/* check that we won't overflow the page -- see comment above */
	BUG_ON(nentries * 60 > PAGE_SIZE);

	p = page;
	for (fentry = 0; fentry < nentries; fentry++)
		/* each FIT entry is two 64 bit words */
		p += dump_fit_entry(p, fit + 2 * fentry);

	return p - page;
}

static int
dump_version(char *page, unsigned long *fit)
{
	int nentries;
	int fentry;
	unsigned long qw;

	TRACE();

	nentries = (unsigned)readq(fit + 1);
	BUG_ON(nentries * 60 > PAGE_SIZE);

	for (fentry = 0; fentry < nentries; fentry++) {
		qw = readq(fit + 2 * fentry + 1);
		if (FIT_TYPE(qw) == FIT_ENTRY_SAL_A)
			return sprintf(page, "%x.%02x\n",
				       FIT_MAJOR(qw), FIT_MINOR(qw));
	}
	return 0;
}

/* same as in proc_misc.c */
static int
proc_calc_metrics(char *page, char **start, off_t off, int count, int *eof,
		  int len)
{
	if (len <= off+count) *eof = 1;
	*start = page + off;
	len -= off;
	if (len>count) len = count;
	if (len<0) len = 0;
	return len;
}

static int
read_version_entry(char *page, char **start, off_t off, int count, int *eof,
		   void *data)
{
	int len = 0;

	MOD_INC_USE_COUNT;
	/* data holds the pointer to this node's FIT */
	len = dump_version(page, (unsigned long *)data);
	len = proc_calc_metrics(page, start, off, count, eof, len);
	MOD_DEC_USE_COUNT;
	return len;
}

static int
read_fit_entry(char *page, char **start, off_t off, int count, int *eof,
	       void *data)
{
	int len = 0;

	MOD_INC_USE_COUNT;
	/* data holds the pointer to this node's FIT */
	len = dump_fit(page, (unsigned long *)data);
	len = proc_calc_metrics(page, start, off, count, eof, len);
	MOD_DEC_USE_COUNT;

	return len;
}

/* this is a fake FIT that's used on the medusa simulator which
 * doesn't usually run a complete PROM. 
 */
#ifdef CONFIG_IA64_SGI_SN_SIM
static unsigned long fakefit[] = {
	/* this is all we need to satisfy the code below */
	FIT_SIGNATURE,
	FIT_ENTRY(FIT_ENTRY_FIT_HEADER, 0x02, 0x60, 2),
	/* dump something arbitrary for
	 * /proc/sgi_prominfo/nodeX/version */
	0xbadbeef00fa3ef17ul,
	FIT_ENTRY(FIT_ENTRY_SAL_A, 0, 0x99, 0x100)
};	
#endif

static unsigned long *
lookup_fit(int nasid)
{
	unsigned long *fitp;
	unsigned long fit_paddr;
	unsigned long *fit_vaddr;

#ifdef CONFIG_IA64_SGI_SN_SIM
	if (IS_RUNNING_ON_SIMULATOR())
		return fakefit;
#endif

	fitp = (void *)GLOBAL_MMR_ADDR(nasid, LB_PROM_SPACE - 32);
	DPRINTK("pointer to fit at %p\n", (void *)fitp);
	fit_paddr = readq(fitp);
	DPRINTK("fit pointer contains %lx\n", fit_paddr);
	/* snag just the node-relative offset */
	fit_paddr &= ~0ul >> (63-35);
	/* the pointer to the FIT is relative to IA-64 compatibility
	 * space.  However, the PROM is mapped at a different offset
	 * in MMR space (both local and global)
	 */
	fit_paddr += 0x700000000;
	fit_vaddr = (void *)GLOBAL_MMR_ADDR(nasid, fit_paddr);
	DPRINTK("fit at %p\n", (void *)fit_vaddr);
	return fit_vaddr;
}

/* module entry points */
int __init prominfo_init(void);
void __exit prominfo_exit(void);

module_init(prominfo_init);
module_exit(prominfo_exit);

static struct proc_dir_entry **proc_entries;
static struct proc_dir_entry *sgi_prominfo_entry;

#define NODE_NAME_LEN 11

int __init
prominfo_init(void)
{
	struct proc_dir_entry **entp;
	cnodeid_t cnodeid;
	nasid_t nasid;
	char name[NODE_NAME_LEN];

	if (!ia64_platform_is("sn2"))
		return 0;

	TRACE();

	DPRINTK("running on cpu %d\n", smp_processor_id());
	DPRINTK("numnodes %d\n", numnodes);

	proc_entries = kmalloc(numnodes * sizeof(struct proc_dir_entry *),
			       GFP_KERNEL);

	sgi_prominfo_entry = proc_mkdir("sgi_prominfo", NULL);

	for (cnodeid = 0, entp = proc_entries;
	     cnodeid < numnodes;
	     cnodeid++, entp++) {
		sprintf(name, "node%d", cnodeid);
		*entp = proc_mkdir(name, sgi_prominfo_entry);
		nasid = cnodeid_to_nasid(cnodeid);
		create_proc_read_entry(
			"fit", 0, *entp, read_fit_entry,
			lookup_fit(nasid));
		create_proc_read_entry(
			"version", 0, *entp, read_version_entry,
			lookup_fit(nasid));
	}

	return 0;
}

void __exit
prominfo_exit(void)
{
	struct proc_dir_entry **entp;
	unsigned cnodeid;
	char name[NODE_NAME_LEN];

	TRACE();

	for (cnodeid = 0, entp = proc_entries;
	     cnodeid < numnodes;
	     cnodeid++, entp++) {
		remove_proc_entry("fit", *entp);
		remove_proc_entry("version", *entp);
		sprintf(name, "node%d", cnodeid);
		remove_proc_entry(name, sgi_prominfo_entry);
	}
	remove_proc_entry("sgi_prominfo", NULL);
	kfree(proc_entries);
}
