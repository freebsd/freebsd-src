/*
 * Sysctl operations for Coda filesystem
 * Original version: (C) 1996 P. Braam and M. Callahan
 * Rewritten for Linux 2.1. (C) 1997 Carnegie Mellon University
 * 
 * Carnegie Mellon encourages users to contribute improvements to
 * the Coda project. Contact Peter Braam (coda@cs.cmu.edu).
 * 
 * CODA operation statistics
 * (c) March, 1998 Zhanyong Wan <zhanyong.wan@yale.edu>
 *
 */

#include <linux/config.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/sysctl.h>
#include <linux/swapctl.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/ctype.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>
#include <linux/utsname.h>
#define __NO_VERSION__
#include <linux/module.h>

#include <linux/coda.h>
#include <linux/coda_linux.h>
#include <linux/coda_fs_i.h>
#include <linux/coda_psdev.h>
#include <linux/coda_cache.h>
#include <linux/coda_proc.h>

static struct ctl_table_header *fs_table_header;

#define FS_CODA         1       /* Coda file system */

#define CODA_DEBUG  	 1	 /* control debugging */
#define CODA_ENTRY	 2       /* control enter/leave pattern */
#define CODA_TIMEOUT    3       /* timeout on upcalls to become intrble */
#define CODA_MC         4       /* use/do not use the access cache */
#define CODA_HARD       5       /* mount type "hard" or "soft" */
#define CODA_VFS 	 6       /* vfs statistics */
#define CODA_UPCALL 	 7       /* upcall statistics */
#define CODA_PERMISSION	 8       /* permission statistics */
#define CODA_CACHE_INV 	 9       /* cache invalidation statistics */
#define CODA_FAKE_STATFS 10	 /* don't query venus for actual cache usage */

static ctl_table coda_table[] = {
	{CODA_DEBUG, "debug", &coda_debug, sizeof(int), 0644, NULL, &proc_dointvec},
 	{CODA_MC, "accesscache", &coda_access_cache, sizeof(int), 0644, NULL, &proc_dointvec}, 
 	{CODA_TIMEOUT, "timeout", &coda_timeout, sizeof(int), 0644, NULL, &proc_dointvec},
 	{CODA_HARD, "hard", &coda_hard, sizeof(int), 0644, NULL, &proc_dointvec},
 	{CODA_VFS, "vfs_stats", NULL, 0, 0644, NULL, &do_reset_coda_vfs_stats},
 	{CODA_UPCALL, "upcall_stats", NULL, 0, 0644, NULL, &do_reset_coda_upcall_stats},
 	{CODA_PERMISSION, "permission_stats", NULL, 0, 0644, NULL, &do_reset_coda_permission_stats},
 	{CODA_CACHE_INV, "cache_inv_stats", NULL, 0, 0644, NULL, &do_reset_coda_cache_inv_stats},
 	{CODA_FAKE_STATFS, "fake_statfs", &coda_fake_statfs, sizeof(int), 0600, NULL, &proc_dointvec},
	{ 0 }
};

static ctl_table fs_table[] = {
       {FS_CODA, "coda",    NULL, 0, 0555, coda_table},
       {0}
};

struct coda_vfs_stats		coda_vfs_stat;
struct coda_permission_stats	coda_permission_stat;
struct coda_cache_inv_stats	coda_cache_inv_stat;
struct coda_upcall_stats_entry  coda_upcall_stat[CODA_NCALLS];
struct coda_upcallstats         coda_callstats;
int                             coda_upcall_timestamping = 0;

/* keep this in sync with coda.h! */
char *coda_upcall_names[] = {
	"totals      ",   /*  0 */
	"-           ",   /*  1 */
	"root        ",   /*  2 */
	"open_by_fd  ",   /*  3 */
	"open        ",   /*  4 */
	"close       ",   /*  5 */
	"ioctl       ",   /*  6 */
	"getattr     ",   /*  7 */
	"setattr     ",   /*  8 */
	"access      ",   /*  9 */
	"lookup      ",   /* 10 */
	"create      ",   /* 11 */
	"remove      ",   /* 12 */
	"link        ",   /* 13 */
	"rename      ",   /* 14 */
	"mkdir       ",   /* 15 */
	"rmdir       ",   /* 16 */
	"readdir     ",   /* 17 */
	"symlink     ",   /* 18 */
	"readlink    ",   /* 19 */
	"fsync       ",   /* 20 */
	"-           ",   /* 21 */
	"vget        ",   /* 22 */
	"signal      ",   /* 23 */
	"replace     ",   /* 24 */
	"flush       ",   /* 25 */
	"purgeuser   ",   /* 26 */
	"zapfile     ",   /* 27 */
	"zapdir      ",   /* 28 */
	"-           ",   /* 29 */
	"purgefid    ",   /* 30 */
	"open_by_path",   /* 31 */
	"resolve     ",   /* 32 */
	"reintegrate ",   /* 33 */
	"statfs      ",   /* 34 */
	"store       ",   /* 35 */
	"release     "    /* 36 */
};


void reset_coda_vfs_stats( void )
{
	memset( &coda_vfs_stat, 0, sizeof( coda_vfs_stat ) );
}

void reset_coda_upcall_stats( void )
{
	memset( &coda_upcall_stat, 0, sizeof( coda_upcall_stat ) );
}

void reset_coda_permission_stats( void )
{
	memset( &coda_permission_stat, 0, sizeof( coda_permission_stat ) );
}

void reset_coda_cache_inv_stats( void )
{
	memset( &coda_cache_inv_stat, 0, sizeof( coda_cache_inv_stat ) );
}


void do_time_stats( struct coda_upcall_stats_entry * pentry, 
		    unsigned long runtime )
{
	unsigned long time = runtime;	/* time in us */
	CDEBUG(D_SPECIAL, "time: %ld\n", time);

	if ( pentry->count == 0 ) {
		pentry->time_sum = pentry->time_squared_sum = 0;
	}
	
	pentry->count++;
	pentry->time_sum += time;
	pentry->time_squared_sum += time*time;
}



void coda_upcall_stats(int opcode, long unsigned runtime) 
{
	struct coda_upcall_stats_entry * pentry;
	
	if ( opcode < 0 || opcode > CODA_NCALLS - 1) {
		printk("Nasty opcode %d passed to coda_upcall_stats\n",
		       opcode);
		return;
	}
		
	pentry = &coda_upcall_stat[opcode];
	do_time_stats(pentry, runtime);

        /* fill in the totals */
	pentry = &coda_upcall_stat[0];
	do_time_stats(pentry, runtime);

}

unsigned long get_time_average( const struct coda_upcall_stats_entry * pentry )
{
	return ( pentry->count == 0 ) ? 0 : pentry->time_sum / pentry->count;
}

static inline unsigned long absolute( unsigned long x )
{
	return x >= 0 ? x : -x;
}

static unsigned long sqr_root( unsigned long x )
{
	unsigned long y = x, r;
	int n_bit = 0;
  
	if ( x == 0 )
		return 0;
	if ( x < 0)
		x = -x;

	while ( y ) {
		y >>= 1;
		n_bit++;
	}
  
	r = 1 << (n_bit/2);
  
	while ( 1 ) {
		r = (r + x/r)/2;
		if ( r*r <= x && x < (r+1)*(r+1) )
			break;
	}
  
	return r;
}

unsigned long get_time_std_deviation( const struct coda_upcall_stats_entry * pentry )
{
	unsigned long time_avg;
  
	if ( pentry->count <= 1 )
		return 0;
  
	time_avg = get_time_average( pentry );

	return sqr_root( (pentry->time_squared_sum / pentry->count) - 
			    time_avg * time_avg );
}

int do_reset_coda_vfs_stats( ctl_table * table, int write, struct file * filp,
			     void * buffer, size_t * lenp )
{
	if ( write ) {
		reset_coda_vfs_stats();

		filp->f_pos += *lenp;
	} else {
		*lenp = 0;
	}

	return 0;
}

int do_reset_coda_upcall_stats( ctl_table * table, int write, 
				struct file * filp, void * buffer, 
				size_t * lenp )
{
	if ( write ) {
        	if (*lenp > 0) {
			char c;
                	if (get_user(c, (char *)buffer))
			       	return -EFAULT;
                        coda_upcall_timestamping = (c == '1');
                }
		reset_coda_upcall_stats();

		filp->f_pos += *lenp;
	} else {
		*lenp = 0;
	}

	return 0;
}

int do_reset_coda_permission_stats( ctl_table * table, int write, 
				    struct file * filp, void * buffer, 
				    size_t * lenp )
{
	if ( write ) {
		reset_coda_permission_stats();

		filp->f_pos += *lenp;
	} else {
		*lenp = 0;
	}

	return 0;
}

int do_reset_coda_cache_inv_stats( ctl_table * table, int write, 
				   struct file * filp, void * buffer, 
				   size_t * lenp )
{
	if ( write ) {
		reset_coda_cache_inv_stats();

		filp->f_pos += *lenp;
	} else {
		*lenp = 0;
	}
  
	return 0;
}

int coda_vfs_stats_get_info( char * buffer, char ** start, off_t offset,
			     int length)
{
	int len=0;
	off_t begin;
	struct coda_vfs_stats * ps = & coda_vfs_stat;
  
  /* this works as long as we are below 1024 characters! */
	len += sprintf( buffer,
			"Coda VFS statistics\n"
			"===================\n\n"
			"File Operations:\n"
			"\topen\t\t%9d\n"
			"\tflush\t\t%9d\n"
			"\trelease\t\t%9d\n"
			"\tfsync\t\t%9d\n\n"
			"Dir Operations:\n"
			"\treaddir\t\t%9d\n\n"
			"Inode Operations\n"
			"\tcreate\t\t%9d\n"
			"\tlookup\t\t%9d\n"
			"\tlink\t\t%9d\n"
			"\tunlink\t\t%9d\n"
			"\tsymlink\t\t%9d\n"
			"\tmkdir\t\t%9d\n"
			"\trmdir\t\t%9d\n"
			"\trename\t\t%9d\n"
			"\tpermission\t%9d\n",

			/* file operations */
			ps->open,
			ps->flush,
			ps->release,
			ps->fsync,

			/* dir operations */
			ps->readdir,
		  
			/* inode operations */
			ps->create,
			ps->lookup,
			ps->link,
			ps->unlink,
			ps->symlink,
			ps->mkdir,
			ps->rmdir,
			ps->rename,
			ps->permission); 

	begin = offset;
	*start = buffer + begin;
	len -= begin;

	if ( len > length )
		len = length;
	if ( len < 0 )
		len = 0;

	return len;
}

int coda_upcall_stats_get_info( char * buffer, char ** start, off_t offset,
				int length)
{
	int len=0;
	int i;
	off_t begin;
	off_t pos = 0;
	char tmpbuf[80];
	int tmplen = 0;

	/* this works as long as we are below 1024 characters! */
	if ( offset < 80 ) 
		len += sprintf( buffer,"%-79s\n",	"Coda upcall statistics");
	if ( offset < 160) 
		len += sprintf( buffer + len,"%-79s\n",	"======================");
	if ( offset < 240) 
		len += sprintf( buffer + len,"%-79s\n",	"upcall              count       avg time(us)    std deviation(us)");
	if ( offset < 320) 
		len += sprintf( buffer + len,"%-79s\n",	"------              -----       ------------    -----------------");
	pos = 320; 
	for ( i = 0 ; i < CODA_NCALLS ; i++ ) {
		tmplen += sprintf(tmpbuf,"%s    %9d       %10ld      %10ld", 
				  coda_upcall_names[i],
				  coda_upcall_stat[i].count, 
				  get_time_average(&coda_upcall_stat[i]),
				  coda_upcall_stat[i].time_squared_sum);
		pos += 80;
		if ( pos < offset ) 
			continue; 
		len += sprintf(buffer + len, "%-79s\n", tmpbuf);
		if ( len >= length ) 
			break; 
	}
  
	begin = len- (pos - offset);
	*start = buffer + begin;
	len -= begin;

	if ( len > length )
		len = length;
	if ( len < 0 )
		len = 0;

	return len;
}

int coda_permission_stats_get_info( char * buffer, char ** start, off_t offset,
				    int length)
{
	int len=0;
	off_t begin;
	struct coda_permission_stats * ps = & coda_permission_stat;
  
	/* this works as long as we are below 1024 characters! */
	len += sprintf( buffer,
			"Coda permission statistics\n"
			"==========================\n\n"
			"count\t\t%9d\n"
			"hit count\t%9d\n",

			ps->count,
			ps->hit_count );
  
	begin = offset;
	*start = buffer + begin;
	len -= begin;

	if ( len > length )
		len = length;
	if ( len < 0 )
		len = 0;

	return len;
}

int coda_cache_inv_stats_get_info( char * buffer, char ** start, off_t offset,
				   int length)
{
	int len=0;
	off_t begin;
	struct coda_cache_inv_stats * ps = & coda_cache_inv_stat;
  
	/* this works as long as we are below 1024 characters! */
	len += sprintf( buffer,
			"Coda cache invalidation statistics\n"
			"==================================\n\n"
			"flush\t\t%9d\n"
			"purge user\t%9d\n"
			"zap_dir\t\t%9d\n"
			"zap_file\t%9d\n"
			"zap_vnode\t%9d\n"
			"purge_fid\t%9d\n"
			"replace\t\t%9d\n",
			ps->flush,
			ps->purge_user,
			ps->zap_dir,
			ps->zap_file,
			ps->zap_vnode,
			ps->purge_fid,
			ps->replace );
  
	begin = offset;
	*start = buffer + begin;
	len -= begin;

	if ( len > length )
		len = length;
	if ( len < 0 )
		len = 0;

	return len;
}


#ifdef CONFIG_PROC_FS

/*
 target directory structure:
   /proc/fs  (see linux/fs/proc/root.c)
   /proc/fs/coda
   /proc/fs/coda/{vfs_stats,

*/

struct proc_dir_entry* proc_fs_coda;

#endif

#define coda_proc_create(name,get_info) \
	create_proc_info_entry(name, 0, proc_fs_coda, get_info)

void coda_sysctl_init()
{
	memset(&coda_callstats, 0, sizeof(coda_callstats));
	reset_coda_vfs_stats();
	reset_coda_upcall_stats();
	reset_coda_permission_stats();
	reset_coda_cache_inv_stats();

#ifdef CONFIG_PROC_FS
	proc_fs_coda = proc_mkdir("coda", proc_root_fs);
	if (proc_fs_coda) {
		proc_fs_coda->owner = THIS_MODULE;
		coda_proc_create("vfs_stats", coda_vfs_stats_get_info);
		coda_proc_create("upcall_stats", coda_upcall_stats_get_info);
		coda_proc_create("permission_stats", coda_permission_stats_get_info);
		coda_proc_create("cache_inv_stats", coda_cache_inv_stats_get_info);
	}
#endif

#ifdef CONFIG_SYSCTL
	if ( !fs_table_header )
		fs_table_header = register_sysctl_table(fs_table, 0);
#endif 
}

void coda_sysctl_clean() 
{

#ifdef CONFIG_SYSCTL
	if ( fs_table_header ) {
		unregister_sysctl_table(fs_table_header);
		fs_table_header = 0;
	}
#endif

#if CONFIG_PROC_FS
        remove_proc_entry("cache_inv_stats", proc_fs_coda);
        remove_proc_entry("permission_stats", proc_fs_coda);
        remove_proc_entry("upcall_stats", proc_fs_coda);
        remove_proc_entry("vfs_stats", proc_fs_coda);
	remove_proc_entry("coda", proc_root_fs);
#endif 
}
