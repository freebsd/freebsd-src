/*
 * linux/fs/fat/fatfs_syms.c
 *
 * Exported kernel symbols for the low-level FAT-based fs support.
 *
 */

#include <linux/module.h>
#include <linux/init.h>

#include <linux/mm.h>
#include <linux/msdos_fs.h>
#include <linux/fat_cvf.h>

EXPORT_SYMBOL(fat_new_dir);
EXPORT_SYMBOL(fat_get_block);
EXPORT_SYMBOL(fat_clear_inode);
EXPORT_SYMBOL(fat_date_unix2dos);
EXPORT_SYMBOL(fat_delete_inode);
EXPORT_SYMBOL(fat__get_entry);
EXPORT_SYMBOL(fat_mark_buffer_dirty);
EXPORT_SYMBOL(fat_notify_change);
EXPORT_SYMBOL(fat_put_super);
EXPORT_SYMBOL(fat_attach);
EXPORT_SYMBOL(fat_detach);
EXPORT_SYMBOL(fat_build_inode);
EXPORT_SYMBOL(fat_read_super);
EXPORT_SYMBOL(fat_search_long);
EXPORT_SYMBOL(fat_readdir);
EXPORT_SYMBOL(fat_scan);
EXPORT_SYMBOL(fat_statfs);
EXPORT_SYMBOL(fat_write_inode);
EXPORT_SYMBOL(register_cvf_format);
EXPORT_SYMBOL(unregister_cvf_format);
EXPORT_SYMBOL(fat_get_cluster);
EXPORT_SYMBOL(fat_dir_ioctl);
EXPORT_SYMBOL(fat_add_entries);
EXPORT_SYMBOL(fat_dir_empty);
EXPORT_SYMBOL(fat_truncate);
EXPORT_SYMBOL(fat_brelse);

static int __init init_fat_fs(void)
{
	fat_hash_init();
	return 0;
}

module_init(init_fat_fs)
