/*
 * linux/fs/msdos/vfatfs_syms.c
 *
 * Exported kernel symbols for the VFAT filesystem.
 * These symbols are used by dmsdos.
 */

#include <linux/module.h>
#include <linux/init.h>

#include <linux/mm.h>
#include <linux/msdos_fs.h>

DECLARE_FSTYPE_DEV(vfat_fs_type, "vfat", vfat_read_super);

EXPORT_SYMBOL(vfat_create);
EXPORT_SYMBOL(vfat_unlink);
EXPORT_SYMBOL(vfat_mkdir);
EXPORT_SYMBOL(vfat_rmdir);
EXPORT_SYMBOL(vfat_rename);
EXPORT_SYMBOL(vfat_read_super);
EXPORT_SYMBOL(vfat_lookup);

static int __init init_vfat_fs(void)
{
	return register_filesystem(&vfat_fs_type);
}

static void __exit exit_vfat_fs(void)
{
	unregister_filesystem(&vfat_fs_type);
}

module_init(init_vfat_fs)
module_exit(exit_vfat_fs)
MODULE_LICENSE("GPL");
