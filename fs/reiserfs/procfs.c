/* -*- linux-c -*- */
/* fs/reiserfs/procfs.c */
/*
 * Copyright 2000-2002 by Hans Reiser, licensing governed by reiserfs/README
 */

/* proc info support a la one created by Sizif@Botik.RU for PGC */

/* $Id: procfs.c,v 1.1.8.2 2001/07/15 17:08:42 god Exp $ */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <linux/reiserfs_fs.h>
#include <linux/reiserfs_fs_sb.h>
#include <linux/smp_lock.h>
#include <linux/locks.h>
#include <linux/init.h>
#include <linux/proc_fs.h>

#if defined( REISERFS_PROC_INFO )

/*
 * LOCKING:
 *
 * We rely on new Alexander Viro's super-block locking.
 *
 */

static struct super_block *procinfo_prologue( kdev_t dev )
{
	struct super_block *result;

	/* get super-block by device */
	result = get_super( dev );
	if( result != NULL ) {
		if( !reiserfs_is_super( result ) ) {
			printk( KERN_DEBUG "reiserfs: procfs-52: "
				"non-reiserfs super found\n" );
			drop_super( result );
			result = NULL;
		}
	} else
		printk( KERN_DEBUG "reiserfs: procfs-74: "
			"race between procinfo and umount\n" );
	return result;
}

int procinfo_epilogue( struct super_block *super )
{
	drop_super( super );
	return 0;
}

int reiserfs_proc_tail( int len, char *buffer, char **start, 
			off_t offset, int count, int *eof )
{
	/* this is black procfs magic */
	if( offset >= len ) {
		*start = buffer;
		*eof = 1;
		return 0;
	}
	*start = buffer + offset;
	if( ( len -= offset ) > count ) {
		return count;
	}
	*eof = 1;
	return len;
}

int reiserfs_version_in_proc( char *buffer, char **start, off_t offset,
			      int count, int *eof, void *data )
{
	int len = 0;
	struct super_block *sb;
	char *format;
    
	sb = procinfo_prologue( ( kdev_t ) ( long ) data );
	if( sb == NULL )
		return -ENOENT;
	if ( sb->u.reiserfs_sb.s_properties & (1 << REISERFS_3_6) ) {
		format = "3.6";
	} else if ( sb->u.reiserfs_sb.s_properties & (1 << REISERFS_3_5) ) {
		format = "3.5";
	} else {
		format = "unknown";
	}
	len += sprintf( &buffer[ len ], "%s format\twith checks %s\n",
			format,
#if defined( CONFIG_REISERFS_CHECK )
			"on"
#else
			"off"
#endif
		);
	procinfo_epilogue( sb );
	return reiserfs_proc_tail( len, buffer, start, offset, count, eof );
}

int reiserfs_global_version_in_proc( char *buffer, char **start, off_t offset,
				     int count, int *eof, void *data )
{
	int len = 0;
	return reiserfs_proc_tail( len, buffer, start, offset, count, eof );
}

#define SF( x ) ( r -> x )
#define SFP( x ) SF( s_proc_info_data.x )
#define SFPL( x ) SFP( x[ level ] )
#define SFPF( x ) SFP( scan_bitmap.x )
#define SFPJ( x ) SFP( journal.x )

#define D2C( x ) le16_to_cpu( x )
#define D4C( x ) le32_to_cpu( x )
#define DF( x ) D2C( rs -> s_v1.x )
#define DFL( x ) D4C( rs -> s_v1.x )
#define DP( x ) D2C( rs -> x )
#define DPL( x ) D4C( rs -> x )

#define objectid_map( s, rs ) (old_format_only (s) ?				\
                         (__u32 *)((struct reiserfs_super_block_v1 *)rs + 1) :	\
			 (__u32 *)(rs + 1))
#define MAP( i ) D4C( objectid_map( sb, rs )[ i ] )

#define DJF( x ) le32_to_cpu( rs -> x )
#define DJV( x ) le32_to_cpu( s_v1 -> x )
#define DJP( x ) le32_to_cpu( jp -> x ) 
#define JF( x ) ( r -> s_journal -> x )

int reiserfs_super_in_proc( char *buffer, char **start, off_t offset,
			    int count, int *eof, void *data )
{
	struct super_block *sb;
	struct reiserfs_sb_info *r;
	int len = 0;
    
	sb = procinfo_prologue( ( kdev_t ) ( long ) data );
	if( sb == NULL )
		return -ENOENT;
	r = &sb->u.reiserfs_sb;
	len += sprintf( &buffer[ len ], 
			"state: \t%s\n"
			"mount options: \t%s%s%s%s%s%s%s%s%s%s%s%s\n"
			"gen. counter: \t%i\n"
			"s_kmallocs: \t%i\n"
			"s_disk_reads: \t%i\n"
			"s_disk_writes: \t%i\n"
			"s_fix_nodes: \t%i\n"
			"s_do_balance: \t%i\n"
			"s_unneeded_left_neighbor: \t%i\n"
			"s_good_search_by_key_reada: \t%i\n"
			"s_bmaps: \t%i\n"
			"s_bmaps_without_search: \t%i\n"
			"s_direct2indirect: \t%i\n"
			"s_indirect2direct: \t%i\n"
			"\n"
			"max_hash_collisions: \t%i\n"

			"breads: \t%lu\n"
			"bread_misses: \t%lu\n"

			"search_by_key: \t%lu\n"
			"search_by_key_fs_changed: \t%lu\n"
			"search_by_key_restarted: \t%lu\n"
			
			"insert_item_restarted: \t%lu\n"
			"paste_into_item_restarted: \t%lu\n"
			"cut_from_item_restarted: \t%lu\n"
			"delete_solid_item_restarted: \t%lu\n"
			"delete_item_restarted: \t%lu\n"

			"leaked_oid: \t%lu\n"
			"leaves_removable: \t%lu\n",

			SF( s_mount_state ) == REISERFS_VALID_FS ?
			"REISERFS_VALID_FS" : "REISERFS_ERROR_FS",
			reiserfs_r5_hash( sb ) ? "FORCE_R5 " : "",
			reiserfs_rupasov_hash( sb ) ? "FORCE_RUPASOV " : "",
			reiserfs_tea_hash( sb ) ? "FORCE_TEA " : "",
			reiserfs_hash_detect( sb ) ? "DETECT_HASH " : "",
			reiserfs_no_border( sb ) ? "NO_BORDER " : "BORDER ",
			reiserfs_no_unhashed_relocation( sb ) ? "NO_UNHASHED_RELOCATION " : "",
			reiserfs_hashed_relocation( sb ) ? "UNHASHED_RELOCATION " : "",
			reiserfs_test4( sb ) ? "TEST4 " : "",
			have_large_tails( sb ) ? "TAILS " : have_small_tails(sb)?"SMALL_TAILS ":"NO_TAILS ",
			replay_only( sb ) ? "REPLAY_ONLY " : "",
			reiserfs_dont_log( sb ) ? "DONT_LOG " : "LOG ",
			convert_reiserfs( sb ) ? "CONV " : "",

			atomic_read( &r -> s_generation_counter ),
			SF( s_kmallocs ),
			SF( s_disk_reads ),
			SF( s_disk_writes ),
			SF( s_fix_nodes ),
			SF( s_do_balance ),
			SF( s_unneeded_left_neighbor ),
			SF( s_good_search_by_key_reada ),
			SF( s_bmaps ),
			SF( s_bmaps_without_search ),
			SF( s_direct2indirect ),
			SF( s_indirect2direct ),
			SFP( max_hash_collisions ),
			SFP( breads ),
			SFP( bread_miss ),
			SFP( search_by_key ),
			SFP( search_by_key_fs_changed ),
			SFP( search_by_key_restarted ),

			SFP( insert_item_restarted ),
			SFP( paste_into_item_restarted ),
			SFP( cut_from_item_restarted ),
			SFP( delete_solid_item_restarted ),
			SFP( delete_item_restarted ),

			SFP( leaked_oid ),
			SFP( leaves_removable ) );

	procinfo_epilogue( sb );
	return reiserfs_proc_tail( len, buffer, start, offset, count, eof );
}

int reiserfs_per_level_in_proc( char *buffer, char **start, off_t offset,
				int count, int *eof, void *data )
{
	struct super_block *sb;
	struct reiserfs_sb_info *r;
	int len = 0;
	int level;
	
	sb = procinfo_prologue( ( kdev_t ) ( long ) data );
	if( sb == NULL )
		return -ENOENT;
	r = &sb->u.reiserfs_sb;

	len += sprintf( &buffer[ len ],
			"level\t"
			"     balances"
			" [sbk:  reads"
			"   fs_changed"
			"   restarted]"
			"   free space"
			"        items"
			"   can_remove"
			"         lnum"
			"         rnum"
			"       lbytes"
			"       rbytes"
			"     get_neig"
			" get_neig_res"
			"  need_l_neig"
			"  need_r_neig"
			"\n"
			
		);

	for( level = 0 ; level < MAX_HEIGHT ; ++ level ) {
		if( len > PAGE_SIZE - 240 ) {
			len += sprintf( &buffer[ len ], "... and more\n" );
			break;
		}
		len += sprintf( &buffer[ len ], 
				"%i\t"
				" %12lu"
				" %12lu"
				" %12lu"
				" %12lu"
				" %12lu"
				" %12lu"
				" %12lu"
				" %12li"
				" %12li"
				" %12li"
				" %12li"
				" %12lu"
				" %12lu"
				" %12lu"
				" %12lu"
				"\n",
				level, 
				SFPL( balance_at ),
				SFPL( sbk_read_at ),
				SFPL( sbk_fs_changed ),
				SFPL( sbk_restarted ),
				SFPL( free_at ),
				SFPL( items_at ),
				SFPL( can_node_be_removed ),
				SFPL( lnum ),
				SFPL( rnum ),
				SFPL( lbytes ),
				SFPL( rbytes ),
				SFPL( get_neighbors ),
				SFPL( get_neighbors_restart ),
				SFPL( need_l_neighbor ),
				SFPL( need_r_neighbor )
			);
	}

	procinfo_epilogue( sb );
	return reiserfs_proc_tail( len, buffer, start, offset, count, eof );
}

int reiserfs_bitmap_in_proc( char *buffer, char **start, off_t offset,
			     int count, int *eof, void *data )
{
	struct super_block *sb;
	struct reiserfs_sb_info *r = &sb->u.reiserfs_sb;
	int len = 0;
    
	sb = procinfo_prologue( ( kdev_t ) ( long ) data );
	if( sb == NULL )
		return -ENOENT;
	r = &sb->u.reiserfs_sb;

	len += sprintf( &buffer[ len ], "free_block: %lu\n"
			"  scan_bitmap:"
			"          wait"
			"          bmap"
			"         retry"
			"        stolen"
			"  journal_hint"
			"journal_nohint"
			"\n"
			" %14lu"
			" %14lu"
			" %14lu"
			" %14lu"
			" %14lu"
			" %14lu"
			" %14lu"
			"\n",
			SFP( free_block ),
			SFPF( call ), 
			SFPF( wait ), 
			SFPF( bmap ),
			SFPF( retry ),
			SFPF( stolen ),
			SFPF( in_journal_hint ),
			SFPF( in_journal_nohint ) );

	procinfo_epilogue( sb );
	return reiserfs_proc_tail( len, buffer, start, offset, count, eof );
}

int reiserfs_on_disk_super_in_proc( char *buffer, char **start, off_t offset,
				    int count, int *eof, void *data )
{
	struct super_block *sb;
	struct reiserfs_sb_info *sb_info;
	struct reiserfs_super_block *rs;
	int hash_code;
	int len = 0;
    
	sb = procinfo_prologue( ( kdev_t ) ( long ) data );
	if( sb == NULL )
		return -ENOENT;
	sb_info = &sb->u.reiserfs_sb;
	rs = sb_info -> s_rs;
	hash_code = DFL( s_hash_function_code );

	len += sprintf( &buffer[ len ], 
			"block_count: \t%i\n"
			"free_blocks: \t%i\n"
			"root_block: \t%i\n"
			"blocksize: \t%i\n"
			"oid_maxsize: \t%i\n"
			"oid_cursize: \t%i\n"
			"umount_state: \t%i\n"
			"magic: \t%10.10s\n"
			"fs_state: \t%i\n"
			"hash: \t%s\n"
			"tree_height: \t%i\n"
			"bmap_nr: \t%i\n"
			"version: \t%i\n"
			"reserved_for_journal: \t%i\n"
			"inode_generation: \t%i\n"
			"flags: \t%x[%s]\n",

			DFL( s_block_count ),
			DFL( s_free_blocks ),
			DFL( s_root_block ),
			DF( s_blocksize ),
			DF( s_oid_maxsize ),
			DF( s_oid_cursize ),
			DF( s_umount_state ),
			rs -> s_v1.s_magic,
			DF( s_fs_state ),
			hash_code == TEA_HASH ? "tea" :
			( hash_code == YURA_HASH ) ? "rupasov" :
			( hash_code == R5_HASH ) ? "r5" :
			( hash_code == UNSET_HASH ) ? "unset" : "unknown",
			DF( s_tree_height ),
			DF( s_bmap_nr ),
			DF( s_version ),
			DF( s_reserved_for_journal ),
			DPL( s_inode_generation ),
			DPL( s_flags ),
			(DPL( s_flags ) & reiserfs_attrs_cleared 
			? "attrs_cleared" : "" ));

	procinfo_epilogue( sb );
	return reiserfs_proc_tail( len, buffer, start, offset, count, eof );
}

int reiserfs_oidmap_in_proc( char *buffer, char **start, off_t offset,
			     int count, int *eof, void *data )
{
	struct super_block *sb;
	struct reiserfs_sb_info *sb_info;
	struct reiserfs_super_block *rs;
	int i;
	unsigned int mapsize;
	unsigned long total_used;
	int len = 0;
	int exact;
    
	sb = procinfo_prologue( ( kdev_t ) ( long ) data );
	if( sb == NULL )
		return -ENOENT;
	sb_info = &sb->u.reiserfs_sb;
	rs = sb_info -> s_rs;
	mapsize = le16_to_cpu( rs -> s_v1.s_oid_cursize );
	total_used = 0;

	for( i = 0 ; i < mapsize ; ++i ) {
		__u32 right;

		right = ( i == mapsize - 1 ) ? MAX_KEY_OBJECTID : MAP( i + 1 );
		len += sprintf( &buffer[ len ], "%s: [ %x .. %x )\n",
				( i & 1 ) ? "free" : "used", MAP( i ), right );
		if( ! ( i & 1 ) ) {
			total_used += right - MAP( i );
		}
		if( len > PAGE_SIZE - 100 ) {
			len += sprintf( &buffer[ len ], "... and more\n" );
			break;
		}
	}
#if defined( REISERFS_USE_OIDMAPF )
	if( sb_info -> oidmap.use_file && ( sb_info -> oidmap.mapf != NULL ) ) {
		loff_t size;

		size = sb_info -> oidmap.mapf -> f_dentry -> d_inode -> i_size;
		total_used += size / sizeof( reiserfs_oidinterval_d_t );
		exact = 1;
	} else
#endif
	{
		exact = ( i == mapsize );
	}
	len += sprintf( &buffer[ len ], "total: \t%i [%i/%i] used: %lu [%s]\n", 
			i, 
			mapsize, le16_to_cpu( rs -> s_v1.s_oid_maxsize ),
			total_used, exact ? "exact" : "estimation" );

	procinfo_epilogue( sb );
	return reiserfs_proc_tail( len, buffer, start, offset, count, eof );
}

int reiserfs_journal_in_proc( char *buffer, char **start, off_t offset,
			      int count, int *eof, void *data )
{
	struct super_block *sb;
	struct reiserfs_sb_info *r;
	struct reiserfs_super_block *rs;
	struct journal_params *jp;	
	int len = 0;
    
	sb = procinfo_prologue( ( kdev_t ) ( long ) data );
	if( sb == NULL )
		return -ENOENT;
	r = &sb->u.reiserfs_sb;
	rs = r -> s_rs;
	jp = &rs->s_v1.s_journal;

	len += sprintf( &buffer[ len ], 
			/* on-disk fields */
 			"jp_journal_1st_block: \t%i\n"
 			"jp_journal_dev: \t%s[%x]\n"
 			"jp_journal_size: \t%i\n"
 			"jp_journal_trans_max: \t%i\n"
 			"jp_journal_magic: \t%i\n"
 			"jp_journal_max_batch: \t%i\n"
 			"jp_journal_max_commit_age: \t%i\n"
			"jp_journal_max_trans_age: \t%i\n"
			/* incore fields */
			"j_1st_reserved_block: \t%i\n" 
			"j_state: \t%li\n"			
			"j_trans_id: \t%lu\n"
			"j_mount_id: \t%lu\n"
			"j_start: \t%lu\n"
			"j_len: \t%lu\n"
			"j_len_alloc: \t%lu\n"
			"j_wcount: \t%i\n"
			"j_bcount: \t%lu\n"
			"j_first_unflushed_offset: \t%lu\n"
			"j_last_flush_trans_id: \t%lu\n"
			"j_trans_start_time: \t%li\n"
			"j_journal_list_index: \t%i\n"
			"j_list_bitmap_index: \t%i\n"
			"j_must_wait: \t%i\n"
			"j_next_full_flush: \t%i\n"
			"j_next_async_flush: \t%i\n"
			"j_cnode_used: \t%i\n"
			"j_cnode_free: \t%i\n"
			"\n"
			/* reiserfs_proc_info_data_t.journal fields */
			"in_journal: \t%12lu\n"
			"in_journal_bitmap: \t%12lu\n"
			"in_journal_reusable: \t%12lu\n"
			"lock_journal: \t%12lu\n"
			"lock_journal_wait: \t%12lu\n"
			"journal_begin: \t%12lu\n"
			"journal_relock_writers: \t%12lu\n"
			"journal_relock_wcount: \t%12lu\n"
			"mark_dirty: \t%12lu\n"
			"mark_dirty_already: \t%12lu\n"
			"mark_dirty_notjournal: \t%12lu\n"
			"restore_prepared: \t%12lu\n"
			"prepare: \t%12lu\n"
			"prepare_retry: \t%12lu\n",

                        DJP( jp_journal_1st_block ),
                        DJP( jp_journal_dev ) == 0 ? "none" : bdevname(to_kdev_t(DJP( jp_journal_dev ))),
                        DJP( jp_journal_dev ),
                        DJP( jp_journal_size ),
                        DJP( jp_journal_trans_max ),
                        DJP( jp_journal_magic ),
                        DJP( jp_journal_max_batch ),
                        DJP( jp_journal_max_commit_age ),
                        DJP( jp_journal_max_trans_age ),

 			JF( j_1st_reserved_block ),
			JF( j_state ),			
			JF( j_trans_id ),
			JF( j_mount_id ),
			JF( j_start ),
			JF( j_len ),
			JF( j_len_alloc ),
			atomic_read( & r -> s_journal -> j_wcount ),
			JF( j_bcount ),
			JF( j_first_unflushed_offset ),
			JF( j_last_flush_trans_id ),
			JF( j_trans_start_time ),
			JF( j_journal_list_index ),
			JF( j_list_bitmap_index ),
			JF( j_must_wait ),
			JF( j_next_full_flush ),
			JF( j_next_async_flush ),
			JF( j_cnode_used ),
			JF( j_cnode_free ),

			SFPJ( in_journal ),
			SFPJ( in_journal_bitmap ),
			SFPJ( in_journal_reusable ),
			SFPJ( lock_journal ),
			SFPJ( lock_journal_wait ),
			SFPJ( journal_being ),
			SFPJ( journal_relock_writers ),
			SFPJ( journal_relock_wcount ),
			SFPJ( mark_dirty ),
			SFPJ( mark_dirty_already ),
			SFPJ( mark_dirty_notjournal ),
			SFPJ( restore_prepared ),
			SFPJ( prepare ),
			SFPJ( prepare_retry )
		);

	procinfo_epilogue( sb );
	return reiserfs_proc_tail( len, buffer, start, offset, count, eof );
}


static struct proc_dir_entry *proc_info_root = NULL;
static const char *proc_info_root_name = "fs/reiserfs";

int reiserfs_proc_info_init( struct super_block *sb )
{
	spin_lock_init( & __PINFO( sb ).lock );
	sb->u.reiserfs_sb.procdir = proc_mkdir( bdevname( sb -> s_dev ), 
						proc_info_root );
	if( sb->u.reiserfs_sb.procdir ) {
		sb->u.reiserfs_sb.procdir -> owner = THIS_MODULE;
		return 0;
	}
	reiserfs_warning( sb, "reiserfs: cannot create /proc/%s/%s\n",
			  proc_info_root_name, bdevname( sb -> s_dev ) );
	return 1;
}


int reiserfs_proc_info_done( struct super_block *sb )
{
	spin_lock( & __PINFO( sb ).lock );
	__PINFO( sb ).exiting = 1;
	spin_unlock( & __PINFO( sb ).lock );
	if ( proc_info_root ) {
		remove_proc_entry( bdevname( sb -> s_dev ), proc_info_root );
		sb->u.reiserfs_sb.procdir = NULL;
	}
	return 0;
}

/* Create /proc/fs/reiserfs/DEV/name and attach read procedure @func
   to it.  Other parts of reiserfs use this function to make their
   per-device statistics available via /proc */

struct proc_dir_entry *reiserfs_proc_register( struct super_block *sb, 
					       char *name, read_proc_t *func )
{
	return ( sb->u.reiserfs_sb.procdir ) ? create_proc_read_entry
		( name, 0, sb->u.reiserfs_sb.procdir, func, 
		  ( void * ) ( long ) sb -> s_dev ) : NULL;
}

void reiserfs_proc_unregister( struct super_block *sb, const char *name )
{
	remove_proc_entry( name, sb->u.reiserfs_sb.procdir );
}

struct proc_dir_entry *reiserfs_proc_register_global( char *name, 
						      read_proc_t *func )
{
	return ( proc_info_root ) ? create_proc_read_entry( name, 0, 
							    proc_info_root, 
							    func, NULL ) : NULL;
}

void reiserfs_proc_unregister_global( const char *name )
{
	remove_proc_entry( name, proc_info_root );
}

int reiserfs_proc_info_global_init( void )
{
	if( proc_info_root == NULL ) {
		proc_info_root = proc_mkdir( proc_info_root_name, 0 );
		if( proc_info_root ) {
			proc_info_root -> owner = THIS_MODULE;
		} else {
			reiserfs_warning( NULL, "reiserfs: cannot create /proc/%s\n",
					  proc_info_root_name );
			return 1;
		}
	}
	return 0;
}

int reiserfs_proc_info_global_done( void )
{
	if ( proc_info_root != NULL ) {
		proc_info_root = NULL;
		remove_proc_entry( proc_info_root_name, 0 );
	}
	return 0;
}

/* REISERFS_PROC_INFO */
#else

int reiserfs_proc_info_init( struct super_block *sb ) { return 0; }
int reiserfs_proc_info_done( struct super_block *sb ) { return 0; }

struct proc_dir_entry *reiserfs_proc_register( struct super_block *sb, 
					       char *name, 
					       read_proc_t *func ) 
{ return NULL; }

void reiserfs_proc_unregister( struct super_block *sb, const char *name ) 
{;}

struct proc_dir_entry *reiserfs_proc_register_global( char *name, 
						      read_proc_t *func )
{ return NULL; }

void reiserfs_proc_unregister_global( const char *name ) {;}

int reiserfs_proc_info_global_init( void ) { return 0; }
int reiserfs_proc_info_global_done( void ) { return 0; }

int reiserfs_global_version_in_proc( char *buffer, char **start, 
				     off_t offset,
				     int count, int *eof, void *data )
{ return 0; }

int reiserfs_version_in_proc( char *buffer, char **start, off_t offset,
			      int count, int *eof, void *data )
{ return 0; }

int reiserfs_super_in_proc( char *buffer, char **start, off_t offset,
			    int count, int *eof, void *data )
{ return 0; }

int reiserfs_per_level_in_proc( char *buffer, char **start, off_t offset,
				int count, int *eof, void *data )
{ return 0; }

int reiserfs_bitmap_in_proc( char *buffer, char **start, off_t offset,
			     int count, int *eof, void *data )
{ return 0; }

int reiserfs_on_disk_super_in_proc( char *buffer, char **start, off_t offset,
				    int count, int *eof, void *data )
{ return 0; }

int reiserfs_oidmap_in_proc( char *buffer, char **start, off_t offset,
			     int count, int *eof, void *data )
{ return 0; }

int reiserfs_journal_in_proc( char *buffer, char **start, off_t offset,
			      int count, int *eof, void *data )
{ return 0; }

/* REISERFS_PROC_INFO */
#endif

/*
 * $Log: procfs.c,v $
 * Revision 1.1.8.2  2001/07/15 17:08:42  god
 *  . use get_super() in procfs.c
 *  . remove remove_save_link() from reiserfs_do_truncate()
 *
 * I accept terms and conditions stated in the Legal Agreement
 * (available at http://www.namesys.com/legalese.html)
 *
 * Revision 1.1.8.1  2001/07/11 16:48:50  god
 * proc info support
 *
 * I accept terms and conditions stated in the Legal Agreement
 * (available at http://www.namesys.com/legalese.html)
 *
 */

/* 
 * Make Linus happy.
 * Local variables:
 * c-indentation-style: "K&R"
 * mode-name: "LC"
 * c-basic-offset: 8
 * tab-width: 8
 * End:
 */
