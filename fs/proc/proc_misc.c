/*
 *  linux/fs/proc/proc_misc.c
 *
 *  linux/fs/proc/array.c
 *  Copyright (C) 1992  by Linus Torvalds
 *  based on ideas by Darren Senn
 *
 *  This used to be the part of array.c. See the rest of history and credits
 *  there. I took this into a separate file and switched the thing to generic
 *  proc_file_inode_operations, leaving in array.c only per-process stuff.
 *  Inumbers allocation made dynamic (via create_proc_entry()).  AV, May 1999.
 *
 * Changes:
 * Fulton Green      :  Encapsulated position metric calculations.
 *			<kernel@FultonGreen.com>
 */

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/kernel_stat.h>
#include <linux/tty.h>
#include <linux/string.h>
#include <linux/mman.h>
#include <linux/proc_fs.h>
#include <linux/ioport.h>
#include <linux/config.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/swap.h>
#include <linux/slab.h>
#include <linux/smp.h>
#include <linux/signal.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/smp_lock.h>
#include <linux/seq_file.h>
#include <linux/sysrq.h>

#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/io.h>

#define LOAD_INT(x) ((x) >> FSHIFT)
#define LOAD_FRAC(x) LOAD_INT(((x) & (FIXED_1-1)) * 100)
/*
 * Warning: stuff below (imported functions) assumes that its output will fit
 * into one page. For some of those functions it may be wrong. Moreover, we
 * have a way to deal with that gracefully. Right now I used straightforward
 * wrappers, but this needs further analysis wrt potential overflows.
 */
extern int get_hardware_list(char *);
extern int get_stram_list(char *);
#ifdef CONFIG_MODULES
extern int get_module_list(char *);
#endif
extern int get_device_list(char *);
extern int get_filesystem_list(char *);
extern int get_exec_domain_list(char *);
#ifndef CONFIG_X86
extern int get_irq_list(char *);
#endif
extern int get_dma_list(char *);
extern int get_locks_status (char *, char **, off_t, int);
extern int get_swaparea_info (char *);
#ifdef CONFIG_SGI_DS1286
extern int get_ds1286_status(char *);
#endif

void proc_sprintf(char *page, off_t *off, int *lenp, const char *format, ...)
{
	int len = *lenp;
	va_list args;

	/* try to only print whole lines */
	if (len > PAGE_SIZE-512)
		return;

	va_start(args, format);
	len += vsnprintf(page + len, PAGE_SIZE-len, format, args);
	va_end(args);

	if (len <= *off) {
		*off -= len;
		len = 0;
	}

	*lenp = len;
}

static int proc_calc_metrics(char *page, char **start, off_t off,
				 int count, int *eof, int len)
{
	if (len <= off+count) *eof = 1;
	*start = page + off;
	len -= off;
	if (len>count) len = count;
	if (len<0) len = 0;
	return len;
}

static int loadavg_read_proc(char *page, char **start, off_t off,
				 int count, int *eof, void *data)
{
	int a, b, c;
	int len;

	a = avenrun[0] + (FIXED_1/200);
	b = avenrun[1] + (FIXED_1/200);
	c = avenrun[2] + (FIXED_1/200);
	len = sprintf(page,"%d.%02d %d.%02d %d.%02d %d/%d %d\n",
		LOAD_INT(a), LOAD_FRAC(a),
		LOAD_INT(b), LOAD_FRAC(b),
		LOAD_INT(c), LOAD_FRAC(c),
		nr_running, nr_threads, last_pid);
	return proc_calc_metrics(page, start, off, count, eof, len);
}

static int uptime_read_proc(char *page, char **start, off_t off,
				 int count, int *eof, void *data)
{
	unsigned long uptime;
	unsigned long idle;
	int len;

	uptime = jiffies;
	idle = init_tasks[0]->times.tms_utime + init_tasks[0]->times.tms_stime;

	/* The formula for the fraction parts really is ((t * 100) / HZ) % 100, but
	   that would overflow about every five days at HZ == 100.
	   Therefore the identity a = (a / b) * b + a % b is used so that it is
	   calculated as (((t / HZ) * 100) + ((t % HZ) * 100) / HZ) % 100.
	   The part in front of the '+' always evaluates as 0 (mod 100). All divisions
	   in the above formulas are truncating. For HZ being a power of 10, the
	   calculations simplify to the version in the #else part (if the printf
	   format is adapted to the same number of digits as zeroes in HZ.
	 */
#if HZ!=100
	len = sprintf(page,"%lu.%02lu %lu.%02lu\n",
		uptime / HZ,
		(((uptime % HZ) * 100) / HZ) % 100,
		idle / HZ,
		(((idle % HZ) * 100) / HZ) % 100);
#else
	len = sprintf(page,"%lu.%02lu %lu.%02lu\n",
		uptime / HZ,
		uptime % HZ,
		idle / HZ,
		idle % HZ);
#endif
	return proc_calc_metrics(page, start, off, count, eof, len);
}

static int meminfo_read_proc(char *page, char **start, off_t off,
				 int count, int *eof, void *data)
{
	struct sysinfo i;
	int len;
	int pg_size ;

/*
 * display in kilobytes.
 */
#define K(x) ((x) << (PAGE_SHIFT - 10))
#define B(x) ((unsigned long long)(x) << PAGE_SHIFT)
	si_meminfo(&i);
	si_swapinfo(&i);
	pg_size = page_cache_size - i.bufferram;

	len = sprintf(page, "        total:    used:    free:  shared: buffers:  cached:\n"
		"Mem:  %8Lu %8Lu %8Lu %8Lu %8Lu %8Lu\n"
		"Swap: %8Lu %8Lu %8Lu\n",
		B(i.totalram), B(i.totalram-i.freeram), B(i.freeram),
		B(i.sharedram), B(i.bufferram),
		B(pg_size), B(i.totalswap),
		B(i.totalswap-i.freeswap), B(i.freeswap));
	/*
	 * Tagged format, for easy grepping and expansion.
	 * The above will go away eventually, once the tools
	 * have been updated.
	 */
	len += sprintf(page+len,
		"MemTotal:     %8lu kB\n"
		"MemFree:      %8lu kB\n"
		"MemShared:    %8lu kB\n"
		"Buffers:      %8lu kB\n"
		"Cached:       %8lu kB\n"
		"SwapCached:   %8lu kB\n"
		"Active:       %8u kB\n"
		"Inactive:     %8u kB\n"
		"HighTotal:    %8lu kB\n"
		"HighFree:     %8lu kB\n"
		"LowTotal:     %8lu kB\n"
		"LowFree:      %8lu kB\n"
		"SwapTotal:    %8lu kB\n"
		"SwapFree:     %8lu kB\n",
		K(i.totalram),
		K(i.freeram),
		K(i.sharedram),
		K(i.bufferram),
		K(pg_size - swapper_space.nrpages),
		K(swapper_space.nrpages),
		K(nr_active_pages),
		K(nr_inactive_pages),
		K(i.totalhigh),
		K(i.freehigh),
		K(i.totalram-i.totalhigh),
		K(i.freeram-i.freehigh),
		K(i.totalswap),
		K(i.freeswap));

	return proc_calc_metrics(page, start, off, count, eof, len);
#undef B
#undef K
}

static int version_read_proc(char *page, char **start, off_t off,
				 int count, int *eof, void *data)
{
	extern char *linux_banner;
	int len;

	strcpy(page, linux_banner);
	len = strlen(page);
	return proc_calc_metrics(page, start, off, count, eof, len);
}

extern struct seq_operations cpuinfo_op;
static int cpuinfo_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &cpuinfo_op);
}
static struct file_operations proc_cpuinfo_operations = {
	open:		cpuinfo_open,
	read:		seq_read,
	llseek:		seq_lseek,
	release:	seq_release,
};

#ifdef CONFIG_PROC_HARDWARE
static int hardware_read_proc(char *page, char **start, off_t off,
				 int count, int *eof, void *data)
{
	int len = get_hardware_list(page);
	return proc_calc_metrics(page, start, off, count, eof, len);
}
#endif

#ifdef CONFIG_STRAM_PROC
static int stram_read_proc(char *page, char **start, off_t off,
				 int count, int *eof, void *data)
{
	int len = get_stram_list(page);
	return proc_calc_metrics(page, start, off, count, eof, len);
}
#endif

extern struct seq_operations partitions_op;
static int partitions_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &partitions_op);
}
static struct file_operations proc_partitions_operations = {
	open:		partitions_open,
	read:		seq_read,
	llseek:		seq_lseek,
	release:	seq_release,
};

#ifdef CONFIG_MODULES
static int modules_read_proc(char *page, char **start, off_t off,
				 int count, int *eof, void *data)
{
	int len = get_module_list(page);
	return proc_calc_metrics(page, start, off, count, eof, len);
}

extern struct seq_operations ksyms_op;
static int ksyms_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &ksyms_op);
}
static struct file_operations proc_ksyms_operations = {
	open:		ksyms_open,
	read:		seq_read,
	llseek:		seq_lseek,
	release:	seq_release,
};
#endif

extern struct seq_operations slabinfo_op;
extern ssize_t slabinfo_write(struct file *, const char *, size_t, loff_t *);
static int slabinfo_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &slabinfo_op);
}
static struct file_operations proc_slabinfo_operations = {
	open:		slabinfo_open,
	read:		seq_read,
	write:		slabinfo_write,
	llseek:		seq_lseek,
	release:	seq_release,
};

static int kstat_read_proc(char *page, char **start, off_t off,
				 int count, int *eof, void *data)
{
	int i, len = 0;
	extern unsigned long total_forks;
	unsigned long jif = jiffies;
	unsigned int sum = 0, user = 0, nice = 0, system = 0;
	int major, disk;

	for (i = 0 ; i < smp_num_cpus; i++) {
		int cpu = cpu_logical_map(i), j;

		user += kstat.per_cpu_user[cpu];
		nice += kstat.per_cpu_nice[cpu];
		system += kstat.per_cpu_system[cpu];
#if !defined(CONFIG_ARCH_S390)
		for (j = 0 ; j < NR_IRQS ; j++)
			sum += kstat.irqs[cpu][j];
#endif
	}

	proc_sprintf(page, &off, &len,
		      "cpu  %u %u %u %lu\n", user, nice, system,
		      jif * smp_num_cpus - (user + nice + system));
	for (i = 0 ; i < smp_num_cpus; i++)
		proc_sprintf(page, &off, &len,
			"cpu%d %u %u %u %lu\n",
			i,
			kstat.per_cpu_user[cpu_logical_map(i)],
			kstat.per_cpu_nice[cpu_logical_map(i)],
			kstat.per_cpu_system[cpu_logical_map(i)],
			jif - (  kstat.per_cpu_user[cpu_logical_map(i)] \
				   + kstat.per_cpu_nice[cpu_logical_map(i)] \
				   + kstat.per_cpu_system[cpu_logical_map(i)]));
	proc_sprintf(page, &off, &len,
		"page %u %u\n"
		"swap %u %u\n"
		"intr %u",
			kstat.pgpgin >> 1,
			kstat.pgpgout >> 1,
			kstat.pswpin,
			kstat.pswpout,
			sum
	);
#if !defined(CONFIG_ARCH_S390) && !defined(CONFIG_ALPHA)
	for (i = 0 ; i < NR_IRQS ; i++)
		proc_sprintf(page, &off, &len,
			     " %u", kstat_irqs(i));
#endif

	proc_sprintf(page, &off, &len, "\ndisk_io: ");

	for (major = 0; major < DK_MAX_MAJOR; major++) {
		for (disk = 0; disk < DK_MAX_DISK; disk++) {
			int active = kstat.dk_drive[major][disk] +
				kstat.dk_drive_rblk[major][disk] +
				kstat.dk_drive_wblk[major][disk];
			if (active)
				proc_sprintf(page, &off, &len,
					"(%u,%u):(%u,%u,%u,%u,%u) ",
					major, disk,
					kstat.dk_drive[major][disk],
					kstat.dk_drive_rio[major][disk],
					kstat.dk_drive_rblk[major][disk],
					kstat.dk_drive_wio[major][disk],
					kstat.dk_drive_wblk[major][disk]
			);
		}
	}

	proc_sprintf(page, &off, &len,
		"\nctxt %u\n"
		"btime %lu\n"
		"processes %lu\n",
		kstat.context_swtch,
		xtime.tv_sec - jif / HZ,
		total_forks);

	return proc_calc_metrics(page, start, off, count, eof, len);
}

static int devices_read_proc(char *page, char **start, off_t off,
				 int count, int *eof, void *data)
{
	int len = get_device_list(page);
	return proc_calc_metrics(page, start, off, count, eof, len);
}

#ifndef CONFIG_X86
#if !defined(CONFIG_ARCH_S390)
static int interrupts_read_proc(char *page, char **start, off_t off,
				 int count, int *eof, void *data)
{
	int len = get_irq_list(page);
	return proc_calc_metrics(page, start, off, count, eof, len);
}
#endif

#else /* !CONFIG_X86 */

extern int show_interrupts(struct seq_file *p, void *v);
static int interrupts_open(struct inode *inode, struct file *file)
{
	unsigned size = PAGE_SIZE * (1 + smp_num_cpus / 8);
	char *buf = kmalloc(size, GFP_KERNEL);
	struct seq_file *m;
	int res;

	if (!buf)
		return -ENOMEM;
	res = single_open(file, show_interrupts, NULL);
	if (!res) {
		m = file->private_data;
		m->buf = buf;
		m->size = size;
	} else
		kfree(buf);
	return res;
}
static struct file_operations proc_interrupts_operations = {
	.open		= interrupts_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};
#endif /* !CONFIG_X86 */

extern struct file_operations proc_ioports_operations;
extern struct file_operations proc_iomem_operations;

static int filesystems_read_proc(char *page, char **start, off_t off,
				 int count, int *eof, void *data)
{
	int len = get_filesystem_list(page);
	return proc_calc_metrics(page, start, off, count, eof, len);
}

static int dma_read_proc(char *page, char **start, off_t off,
				 int count, int *eof, void *data)
{
	int len = get_dma_list(page);
	return proc_calc_metrics(page, start, off, count, eof, len);
}

static int cmdline_read_proc(char *page, char **start, off_t off,
				 int count, int *eof, void *data)
{
	extern char saved_command_line[];
	int len = 0;

	proc_sprintf(page, &off, &len, "%s\n", saved_command_line);
	return proc_calc_metrics(page, start, off, count, eof, len);
}

#ifdef CONFIG_SGI_DS1286
static int ds1286_read_proc(char *page, char **start, off_t off,
				 int count, int *eof, void *data)
{
	int len = get_ds1286_status(page);
	return proc_calc_metrics(page, start, off, count, eof, len);
}
#endif

static int locks_read_proc(char *page, char **start, off_t off,
				 int count, int *eof, void *data)
{
	int len;
	lock_kernel();
	len = get_locks_status(page, start, off, count);
	unlock_kernel();
	if (len < count) *eof = 1;
	return len;
}

static int execdomains_read_proc(char *page, char **start, off_t off,
				 int count, int *eof, void *data)
{
	int len = get_exec_domain_list(page);
	return proc_calc_metrics(page, start, off, count, eof, len);
}

static int swaps_read_proc(char *page, char **start, off_t off,
				 int count, int *eof, void *data)
{
	int len = get_swaparea_info(page);
	return proc_calc_metrics(page, start, off, count, eof, len);
}

/*
 * This function accesses profiling information. The returned data is
 * binary: the sampling step and the actual contents of the profile
 * buffer. Use of the program readprofile is recommended in order to
 * get meaningful info out of these data.
 */
static ssize_t read_profile(struct file *file, char *buf,
			    size_t count, loff_t *ppos)
{
	unsigned long p = *ppos;
	ssize_t read;
	char * pnt;
	unsigned int sample_step = 1 << prof_shift;

	if (p >= (prof_len+1)*sizeof(unsigned int))
		return 0;
	if (count > (prof_len+1)*sizeof(unsigned int) - p)
		count = (prof_len+1)*sizeof(unsigned int) - p;
	read = 0;

	while (p < sizeof(unsigned int) && count > 0) {
		put_user(*((char *)(&sample_step)+p),buf);
		buf++; p++; count--; read++;
	}
	pnt = (char *)prof_buffer + p - sizeof(unsigned int);
	if (copy_to_user(buf,(void *)pnt,count))
		return -EFAULT;
	read += count;
	*ppos += read;
	return read;
}

/*
 * Writing to /proc/profile resets the counters
 *
 * Writing a 'profiling multiplier' value into it also re-sets the profiling
 * interrupt frequency, on architectures that support this.
 */
static ssize_t write_profile(struct file * file, const char * buf,
			     size_t count, loff_t *ppos)
{
#ifdef CONFIG_SMP
	extern int setup_profiling_timer (unsigned int multiplier);

	if (count==sizeof(int)) {
		unsigned int multiplier;

		if (copy_from_user(&multiplier, buf, sizeof(int)))
			return -EFAULT;

		if (setup_profiling_timer(multiplier))
			return -EINVAL;
	}
#endif

	memset(prof_buffer, 0, prof_len * sizeof(*prof_buffer));
	return count;
}

static struct file_operations proc_profile_operations = {
	read:		read_profile,
	write:		write_profile,
};

#ifdef CONFIG_MAGIC_SYSRQ
/*
 * writing 'C' to /proc/sysrq-trigger is like sysrq-C
 */
static ssize_t write_sysrq_trigger(struct file *file, const char *buf,
				     size_t count, loff_t *ppos)
{
	if (count) {
		char c;

		if (get_user(c, buf))
			return -EFAULT;
		handle_sysrq(c, NULL, NULL, NULL);
	}
	return count;
}

static struct file_operations proc_sysrq_trigger_operations = {
	.write		= write_sysrq_trigger,
};
#endif

struct proc_dir_entry *proc_root_kcore;

static void create_seq_entry(char *name, mode_t mode, struct file_operations *f)
{
	struct proc_dir_entry *entry;
	entry = create_proc_entry(name, mode, NULL);
	if (entry)
		entry->proc_fops = f;
}

void __init proc_misc_init(void)
{
	struct proc_dir_entry *entry;
	static struct {
		char *name;
		int (*read_proc)(char*,char**,off_t,int,int*,void*);
	} *p, simple_ones[] = {
		{"loadavg",     loadavg_read_proc},
		{"uptime",	uptime_read_proc},
		{"meminfo",	meminfo_read_proc},
		{"version",	version_read_proc},
#ifdef CONFIG_PROC_HARDWARE
		{"hardware",	hardware_read_proc},
#endif
#ifdef CONFIG_STRAM_PROC
		{"stram",	stram_read_proc},
#endif
#ifdef CONFIG_MODULES
		{"modules",	modules_read_proc},
#endif
		{"stat",	kstat_read_proc},
		{"devices",	devices_read_proc},
#if !defined(CONFIG_ARCH_S390) && !defined(CONFIG_X86)
		{"interrupts",	interrupts_read_proc},
#endif
		{"filesystems",	filesystems_read_proc},
		{"dma",		dma_read_proc},
		{"cmdline",	cmdline_read_proc},
#ifdef CONFIG_SGI_DS1286
		{"rtc",		ds1286_read_proc},
#endif
		{"locks",	locks_read_proc},
		{"swaps",	swaps_read_proc},
		{"execdomains",	execdomains_read_proc},
		{NULL,}
	};
	for (p = simple_ones; p->name; p++)
		create_proc_read_entry(p->name, 0, NULL, p->read_proc, NULL);

	proc_symlink("mounts", NULL, "self/mounts");

	/* And now for trickier ones */
	entry = create_proc_entry("kmsg", S_IRUSR, &proc_root);
	if (entry)
		entry->proc_fops = &proc_kmsg_operations;
	create_seq_entry("cpuinfo", 0, &proc_cpuinfo_operations);
#if defined(CONFIG_X86)
	create_seq_entry("interrupts", 0, &proc_interrupts_operations);
#endif
	create_seq_entry("ioports", 0, &proc_ioports_operations);
	create_seq_entry("iomem", 0, &proc_iomem_operations);
	create_seq_entry("partitions", 0, &proc_partitions_operations);
	create_seq_entry("slabinfo",S_IWUSR|S_IRUGO,&proc_slabinfo_operations);
#ifdef CONFIG_MODULES
	create_seq_entry("ksyms", 0, &proc_ksyms_operations);
#endif
	proc_root_kcore = create_proc_entry("kcore", S_IRUSR, NULL);
	if (proc_root_kcore) {
		proc_root_kcore->proc_fops = &proc_kcore_operations;
		proc_root_kcore->size =
				(size_t)high_memory - PAGE_OFFSET + PAGE_SIZE;
	}
	if (prof_shift) {
		entry = create_proc_entry("profile", S_IWUSR | S_IRUGO, NULL);
		if (entry) {
			entry->proc_fops = &proc_profile_operations;
			entry->size = (1+prof_len) * sizeof(unsigned int);
		}
	}
#ifdef CONFIG_MAGIC_SYSRQ
	entry = create_proc_entry("sysrq-trigger", S_IWUSR, NULL);
	if (entry)
		entry->proc_fops = &proc_sysrq_trigger_operations;
#endif
#ifdef CONFIG_PPC32
	{
		extern struct file_operations ppc_htab_operations;
		entry = create_proc_entry("ppc_htab", S_IRUGO|S_IWUSR, NULL);
		if (entry)
			entry->proc_fops = &ppc_htab_operations;
	}
#endif
}
