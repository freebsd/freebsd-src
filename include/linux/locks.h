#ifndef _LINUX_LOCKS_H
#define _LINUX_LOCKS_H

#ifndef _LINUX_MM_H
#include <linux/mm.h>
#endif
#ifndef _LINUX_PAGEMAP_H
#include <linux/pagemap.h>
#endif

/*
 * Buffer cache locking - note that interrupts may only unlock, not
 * lock buffers.
 */
extern void __wait_on_buffer(struct buffer_head *);

static inline void wait_on_buffer(struct buffer_head * bh)
{
	if (test_bit(BH_Lock, &bh->b_state))
		__wait_on_buffer(bh);
}

static inline void lock_buffer(struct buffer_head * bh)
{
	while (test_and_set_bit(BH_Lock, &bh->b_state))
		__wait_on_buffer(bh);
}

extern void FASTCALL(unlock_buffer(struct buffer_head *bh));

/*
 * super-block locking. Again, interrupts may only unlock
 * a super-block (although even this isn't done right now.
 * nfs may need it).
 */

static inline void lock_super(struct super_block * sb)
{
	down(&sb->s_lock);
}

static inline void unlock_super(struct super_block * sb)
{
	up(&sb->s_lock);
}

#endif /* _LINUX_LOCKS_H */

