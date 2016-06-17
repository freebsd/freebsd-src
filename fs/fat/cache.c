/*
 *  linux/fs/fat/cache.c
 *
 *  Written 1992,1993 by Werner Almesberger
 *
 *  Mar 1999. AV. Changed cache, so that it uses the starting cluster instead
 *	of inode number.
 *  May 1999. AV. Fixed the bogosity with FAT32 (read "FAT28"). Fscking lusers.
 */

#include <linux/msdos_fs.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/stat.h>
#include <linux/fat_cvf.h>

#if 0
#  define PRINTK(x) printk x
#else
#  define PRINTK(x)
#endif

static struct fat_cache *fat_cache,cache[FAT_CACHE];
static spinlock_t fat_cache_lock = SPIN_LOCK_UNLOCKED;

/* Returns the this'th FAT entry, -1 if it is an end-of-file entry. If
   new_value is != -1, that FAT entry is replaced by it. */

int fat_access(struct super_block *sb,int nr,int new_value)
{
	return MSDOS_SB(sb)->cvf_format->fat_access(sb,nr,new_value);
}

int fat_bmap(struct inode *inode,int sector)
{
	return MSDOS_SB(inode->i_sb)->cvf_format->cvf_bmap(inode,sector);
}

int default_fat_access(struct super_block *sb,int nr,int new_value)
{
	struct buffer_head *bh, *bh2, *c_bh, *c_bh2;
	unsigned char *p_first, *p_last;
	int copy, first, last, next, b;

	if ((unsigned) (nr-2) >= MSDOS_SB(sb)->clusters)
		return 0;
	if (MSDOS_SB(sb)->fat_bits == 32) {
		first = last = nr*4;
	} else if (MSDOS_SB(sb)->fat_bits == 16) {
		first = last = nr*2;
	} else {
		first = nr*3/2;
		last = first+1;
	}
	b = MSDOS_SB(sb)->fat_start + (first >> sb->s_blocksize_bits);
	if (!(bh = fat_bread(sb, b))) {
		printk("bread in fat_access failed\n");
		return 0;
	}
	if ((first >> sb->s_blocksize_bits) == (last >> sb->s_blocksize_bits)) {
		bh2 = bh;
	} else {
		if (!(bh2 = fat_bread(sb, b+1))) {
			fat_brelse(sb, bh);
			printk("2nd bread in fat_access failed\n");
			return 0;
		}
	}
	if (MSDOS_SB(sb)->fat_bits == 32) {
		p_first = p_last = NULL; /* GCC needs that stuff */
		next = CF_LE_L(((__u32 *) bh->b_data)[(first &
		    (sb->s_blocksize - 1)) >> 2]);
		/* Fscking Microsoft marketing department. Their "32" is 28. */
		next &= 0xfffffff;
		if (next >= 0xffffff7) next = -1;
		PRINTK(("fat_bread: 0x%x, nr=0x%x, first=0x%x, next=0x%x\n", b, nr, first, next));

	} else if (MSDOS_SB(sb)->fat_bits == 16) {
		p_first = p_last = NULL; /* GCC needs that stuff */
		next = CF_LE_W(((__u16 *) bh->b_data)[(first &
		    (sb->s_blocksize - 1)) >> 1]);
		if (next >= 0xfff7) next = -1;
	} else {
		p_first = &((__u8 *)bh->b_data)[first & (sb->s_blocksize - 1)];
		p_last = &((__u8 *)bh2->b_data)[(first + 1) & (sb->s_blocksize - 1)];
		if (nr & 1) next = ((*p_first >> 4) | (*p_last << 4)) & 0xfff;
		else next = (*p_first+(*p_last << 8)) & 0xfff;
		if (next >= 0xff7) next = -1;
	}
	if (new_value != -1) {
		if (MSDOS_SB(sb)->fat_bits == 32) {
			((__u32 *)bh->b_data)[(first & (sb->s_blocksize - 1)) >> 2]
				= CT_LE_L(new_value);
		} else if (MSDOS_SB(sb)->fat_bits == 16) {
			((__u16 *)bh->b_data)[(first & (sb->s_blocksize - 1)) >> 1]
				= CT_LE_W(new_value);
		} else {
			if (nr & 1) {
				*p_first = (*p_first & 0xf) | (new_value << 4);
				*p_last = new_value >> 4;
			}
			else {
				*p_first = new_value & 0xff;
				*p_last = (*p_last & 0xf0) | (new_value >> 8);
			}
			fat_mark_buffer_dirty(sb, bh2);
		}
		fat_mark_buffer_dirty(sb, bh);
		for (copy = 1; copy < MSDOS_SB(sb)->fats; copy++) {
			b = MSDOS_SB(sb)->fat_start + (first >> sb->s_blocksize_bits)
				+ MSDOS_SB(sb)->fat_length * copy;
			if (!(c_bh = fat_bread(sb, b)))
				break;
			if (bh != bh2) {
				if (!(c_bh2 = fat_bread(sb, b+1))) {
					fat_brelse(sb, c_bh);
					break;
				}
				memcpy(c_bh2->b_data, bh2->b_data, sb->s_blocksize);
				fat_mark_buffer_dirty(sb, c_bh2);
				fat_brelse(sb, c_bh2);
			}
			memcpy(c_bh->b_data, bh->b_data, sb->s_blocksize);
			fat_mark_buffer_dirty(sb, c_bh);
			fat_brelse(sb, c_bh);
		}
	}
	fat_brelse(sb, bh);
	if (bh != bh2)
		fat_brelse(sb, bh2);
	return next;
}

void fat_cache_init(void)
{
	static int initialized = 0;
	int count;

	spin_lock(&fat_cache_lock);
	if (initialized) {
		spin_unlock(&fat_cache_lock);
		return;
	}
	fat_cache = &cache[0];
	for (count = 0; count < FAT_CACHE; count++) {
		cache[count].device = 0;
		cache[count].next = count == FAT_CACHE-1 ? NULL :
		    &cache[count+1];
	}
	initialized = 1;
	spin_unlock(&fat_cache_lock);
}


void fat_cache_lookup(struct inode *inode,int cluster,int *f_clu,int *d_clu)
{
	struct fat_cache *walk;
	int first = MSDOS_I(inode)->i_start;

	if (!first)
		return;
	spin_lock(&fat_cache_lock);
	for (walk = fat_cache; walk; walk = walk->next)
		if (inode->i_dev == walk->device
		    && walk->start_cluster == first
		    && walk->file_cluster <= cluster
		    && walk->file_cluster > *f_clu) {
			*d_clu = walk->disk_cluster;
#ifdef DEBUG
printk("cache hit: %d (%d)\n",walk->file_cluster,*d_clu);
#endif
			if ((*f_clu = walk->file_cluster) == cluster) { 
				spin_unlock(&fat_cache_lock);
				return;
			}
		}
	spin_unlock(&fat_cache_lock);
#ifdef DEBUG
printk("cache miss\n");
#endif
}


#ifdef DEBUG
static void list_cache(void)
{
	struct fat_cache *walk;

	for (walk = fat_cache; walk; walk = walk->next) {
		if (walk->device)
			printk("<%s,%d>(%d,%d) ", kdevname(walk->device),
			       walk->start_cluster, walk->file_cluster,
			       walk->disk_cluster);
		else printk("-- ");
	}
	printk("\n");
}
#endif


void fat_cache_add(struct inode *inode,int f_clu,int d_clu)
{
	struct fat_cache *walk,*last;
	int first = MSDOS_I(inode)->i_start;

	last = NULL;
	spin_lock(&fat_cache_lock);
	for (walk = fat_cache; walk->next; walk = (last = walk)->next)
		if (inode->i_dev == walk->device
		    && walk->start_cluster == first
		    && walk->file_cluster == f_clu) {
			if (walk->disk_cluster != d_clu) {
				printk("FAT cache corruption inode=%ld\n",
					inode->i_ino);
				spin_unlock(&fat_cache_lock);
				fat_cache_inval_inode(inode);
				return;
			}
			/* update LRU */
			if (last == NULL) {
				spin_unlock(&fat_cache_lock);
				return;
			}
			last->next = walk->next;
			walk->next = fat_cache;
			fat_cache = walk;
#ifdef DEBUG
list_cache();
#endif
			spin_unlock(&fat_cache_lock);
			return;
		}
	walk->device = inode->i_dev;
	walk->start_cluster = first;
	walk->file_cluster = f_clu;
	walk->disk_cluster = d_clu;
	last->next = NULL;
	walk->next = fat_cache;
	fat_cache = walk;
	spin_unlock(&fat_cache_lock);
#ifdef DEBUG
list_cache();
#endif
}


/* Cache invalidation occurs rarely, thus the LRU chain is not updated. It
   fixes itself after a while. */

void fat_cache_inval_inode(struct inode *inode)
{
	struct fat_cache *walk;
	int first = MSDOS_I(inode)->i_start;

	spin_lock(&fat_cache_lock);
	for (walk = fat_cache; walk; walk = walk->next)
		if (walk->device == inode->i_dev
		    && walk->start_cluster == first)
			walk->device = 0;
	spin_unlock(&fat_cache_lock);
}


void fat_cache_inval_dev(kdev_t device)
{
	struct fat_cache *walk;

	spin_lock(&fat_cache_lock);
	for (walk = fat_cache; walk; walk = walk->next)
		if (walk->device == device)
			walk->device = 0;
	spin_unlock(&fat_cache_lock);
}


int fat_get_cluster(struct inode *inode,int cluster)
{
	int nr,count;

	if (!(nr = MSDOS_I(inode)->i_start)) return 0;
	if (!cluster) return nr;
	count = 0;
	for (fat_cache_lookup(inode,cluster,&count,&nr); count < cluster;
	    count++) {
		if ((nr = fat_access(inode->i_sb,nr,-1)) == -1) return 0;
		if (!nr) return 0;
	}
	fat_cache_add(inode,cluster,nr);
	return nr;
}

int default_fat_bmap(struct inode *inode,int sector)
{
	struct super_block *sb = inode->i_sb;
	struct msdos_sb_info *sbi = MSDOS_SB(sb);
	int cluster, offset, last_block;

	if ((sbi->fat_bits != 32) &&
	    (inode->i_ino == MSDOS_ROOT_INO || (S_ISDIR(inode->i_mode) &&
	     !MSDOS_I(inode)->i_start))) {
		if (sector >= sbi->dir_entries >> sbi->dir_per_block_bits)
			return 0;
		return sector + sbi->dir_start;
	}
	last_block = (MSDOS_I(inode)->mmu_private + (sb->s_blocksize - 1))
		>> sb->s_blocksize_bits;
	if (sector >= last_block)
		return 0;

	cluster = sector / sbi->cluster_size;
	offset  = sector % sbi->cluster_size;
	if (!(cluster = fat_get_cluster(inode, cluster)))
		return 0;

	return (cluster - 2) * sbi->cluster_size + sbi->data_start + offset;
}


/* Free all clusters after the skip'th cluster. Doesn't use the cache,
   because this way we get an additional sanity check. */

int fat_free(struct inode *inode,int skip)
{
	int nr,last;

	if (!(nr = MSDOS_I(inode)->i_start)) return 0;
	last = 0;
	while (skip--) {
		last = nr;
		if ((nr = fat_access(inode->i_sb,nr,-1)) == -1) return 0;
		if (!nr) {
			printk("fat_free: skipped EOF\n");
			return -EIO;
		}
	}
	if (last) {
		fat_access(inode->i_sb,last,EOF_FAT(inode->i_sb));
		fat_cache_inval_inode(inode);
	} else {
		fat_cache_inval_inode(inode);
		MSDOS_I(inode)->i_start = 0;
		MSDOS_I(inode)->i_logstart = 0;
		mark_inode_dirty(inode);
	}
	lock_fat(inode->i_sb);
	while (nr != -1) {
		if (!(nr = fat_access(inode->i_sb,nr,0))) {
			fat_fs_panic(inode->i_sb,"fat_free: deleting beyond EOF");
			break;
		}
		if (MSDOS_SB(inode->i_sb)->free_clusters != -1) {
			MSDOS_SB(inode->i_sb)->free_clusters++;
			if (MSDOS_SB(inode->i_sb)->fat_bits == 32) {
				fat_clusters_flush(inode->i_sb);
			}
		}
		inode->i_blocks -= (1 << MSDOS_SB(inode->i_sb)->cluster_bits) / 512;
	}
	unlock_fat(inode->i_sb);
	return 0;
}
