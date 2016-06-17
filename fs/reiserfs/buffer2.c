/*
 *  Copyright 2000-2002 by Hans Reiser, licensing governed by reiserfs/README  
 */

#include <linux/config.h>
#include <linux/sched.h>
#include <linux/locks.h>
#include <linux/reiserfs_fs.h>
#include <linux/smp_lock.h>
#include <linux/kernel_stat.h>

/*
 *  wait_buffer_until_released
 *  reiserfs_bread
 */

/* when we allocate a new block (get_new_buffer, get_empty_nodes) and
   get buffer for it, it is possible that it is held by someone else
   or even by this process. In this function we wait until all other
   holders release buffer. To make sure, that current process does not
   hold we did free all buffers in tree balance structure
   (get_empty_nodes and get_nodes_for_preserving) or in path structure
   only (get_new_buffer) just before calling this */
void wait_buffer_until_released (const struct buffer_head * bh)
{
  int repeat_counter = 0;

  while (atomic_read (&(bh->b_count)) > 1) {

    if ( !(++repeat_counter % 30000000) ) {
      reiserfs_warning (NULL, "vs-3050: wait_buffer_until_released: nobody releases buffer (%b). Still waiting (%d) %cJDIRTY %cJWAIT\n",
			bh, repeat_counter, buffer_journaled(bh) ? ' ' : '!',
			buffer_journal_dirty(bh) ? ' ' : '!');
    }
    run_task_queue(&tq_disk);
    yield();
  }
  if (repeat_counter > 30000000) {
    reiserfs_warning(NULL, "vs-3051: done waiting, ignore vs-3050 messages for (%b)\n", bh) ;
  }
}

/*
 * reiserfs_bread() reads a specified block and returns the buffer that contains
 * it. It returns NULL if the block was unreadable.
 */
/* It first tries to find the block in cache, and if it cannot do so
   then it creates a new buffer and schedules I/O to read the
   block. */
/* The function is NOT SCHEDULE-SAFE! */
struct buffer_head  * reiserfs_bread (struct super_block *super, int n_block, int n_size) 
{
    struct buffer_head  *result;
    PROC_EXP( unsigned int ctx_switches = kstat.context_swtch );

    result = bread (super -> s_dev, n_block, n_size);
    PROC_INFO_INC( super, breads );
    PROC_EXP( if( kstat.context_swtch != ctx_switches ) 
	      PROC_INFO_INC( super, bread_miss ) );
    return result;
}

struct buffer_head  * journal_bread (struct super_block *s, int block)
{
	return bread (SB_JOURNAL_DEV(s), block, s->s_blocksize );
}

struct buffer_head  * journal_getblk (struct super_block *s, int block)
{
	return getblk (SB_JOURNAL_DEV(s), block, s->s_blocksize );
}

struct buffer_head  * journal_get_hash_table (struct super_block *s, int block)
{
  return get_hash_table (SB_JOURNAL_DEV(s), block, s->s_blocksize );
}
