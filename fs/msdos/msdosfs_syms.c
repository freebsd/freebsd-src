/*
 * linux/fs/msdos/msdosfs_syms.c
 *
 * Exported kernel symbols for the MS-DOS filesystem.
 * These symbols are used by umsdos.
 */

#include <linux/module.h>

#include <linux/mm.h>
#include <linux/msdos_fs.h>
#include <linux/init.h>

/*
 * Support for umsdos fs
 *
 * These symbols are _always_ exported, in case someone
 * wants to install the umsdos module later.
 */
EXPORT_SYMBOL(msdos_create);
EXPORT_SYMBOL(msdos_lookup);
EXPORT_SYMBOL(msdos_mkdir);
EXPORT_SYMBOL(msdos_rename);
EXPORT_SYMBOL(msdos_rmdir);
EXPORT_SYMBOL(msdos_unlink);
EXPORT_SYMBOL(msdos_read_super);
EXPORT_SYMBOL(msdos_put_super);

static DECLARE_FSTYPE_DEV(msdos_fs_type, "msdos", msdos_read_super);

static int __init init_msdos_fs(void)
{
	return register_filesystem(&msdos_fs_type);
}

static void __exit exit_msdos_fs(void)
{
	unregister_filesystem(&msdos_fs_type);
}

module_init(init_msdos_fs)
module_exit(exit_msdos_fs)
MODULE_LICENSE("GPL");
