/*
 * coda_statis.h
 * 
 * CODA operation statistics
 *
 * (c) March, 1998
 * by Michihiro Kuramochi, Zhenyu Xia and Zhanyong Wan
 * zhanyong.wan@yale.edu
 *
 */

#ifndef _CODA_PROC_H
#define _CODA_PROC_H

void coda_sysctl_init(void);
void coda_sysctl_clean(void);
void coda_upcall_stats(int opcode, unsigned long jiffies);

#include <linux/sysctl.h>
#include <linux/coda_fs_i.h>
#include <linux/coda.h>

/* these four files are presented to show the result of the statistics:
 *
 *	/proc/fs/coda/vfs_stats
 *		      upcall_stats
 *		      permission_stats
 *		      cache_inv_stats
 *
 * these four files are presented to reset the statistics to 0:
 *
 *	/proc/sys/coda/vfs_stats
 *		       upcall_stats
 *		       permission_stats
 *		       cache_inv_stats
 */

/* VFS operation statistics */
struct coda_vfs_stats 
{
	/* file operations */
	int open;
	int flush;
	int release;
	int fsync;

	/* dir operations */
	int readdir;
  
	/* inode operations */
	int create;
	int lookup;
	int link;
	int unlink;
	int symlink;
	int mkdir;
	int rmdir;
	int rename;
	int permission;

	/* symlink operatoins*/
	int follow_link;
	int readlink;
};

struct coda_upcall_stats_entry 
{
  int count;
  unsigned long time_sum;
  unsigned long time_squared_sum;
};



/* cache hits for permissions statistics */
struct coda_permission_stats 
{
	int count;
	int hit_count;
};

/* cache invalidation statistics */
struct coda_cache_inv_stats
{
	int flush;
	int purge_user;
	int zap_dir;
	int zap_file;
	int zap_vnode;
	int purge_fid;
	int replace;
};

/* these global variables hold the actual statistics data */
extern struct coda_vfs_stats		coda_vfs_stat;
extern struct coda_permission_stats	coda_permission_stat;
extern struct coda_cache_inv_stats	coda_cache_inv_stat;
extern int				coda_upcall_timestamping;

/* reset statistics to 0 */
void reset_coda_vfs_stats( void );
void reset_coda_upcall_stats( void );
void reset_coda_permission_stats( void );
void reset_coda_cache_inv_stats( void );

/* some utitlities to make it easier for you to do statistics for time */
void do_time_stats( struct coda_upcall_stats_entry * pentry, 
		    unsigned long jiffy );
/*
double get_time_average( const struct coda_upcall_stats_entry * pentry );
double get_time_std_deviation( const struct coda_upcall_stats_entry * pentry );
*/
unsigned long get_time_average( const struct coda_upcall_stats_entry * pentry );
unsigned long get_time_std_deviation( const struct coda_upcall_stats_entry * pentry );

/* like coda_dointvec, these functions are to be registered in the ctl_table
 * data structure for /proc/sys/... files 
 */
int do_reset_coda_vfs_stats( ctl_table * table, int write, struct file * filp,
			     void * buffer, size_t * lenp );
int do_reset_coda_upcall_stats( ctl_table * table, int write, 
				struct file * filp, void * buffer, 
				size_t * lenp );
int do_reset_coda_permission_stats( ctl_table * table, int write, 
				    struct file * filp, void * buffer, 
				    size_t * lenp );
int do_reset_coda_cache_inv_stats( ctl_table * table, int write, 
				   struct file * filp, void * buffer, 
				   size_t * lenp );

/* these functions are called to form the content of /proc/fs/coda/... files */
int coda_vfs_stats_get_info( char * buffer, char ** start, off_t offset,
			     int length);
int coda_upcall_stats_get_info( char * buffer, char ** start, off_t offset,
				int length);
int coda_permission_stats_get_info( char * buffer, char ** start, off_t offset,
				    int length);
int coda_cache_inv_stats_get_info( char * buffer, char ** start, off_t offset,
				   int length);


#endif /* _CODA_PROC_H */
