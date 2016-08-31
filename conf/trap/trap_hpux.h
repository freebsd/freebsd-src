/* $srcdir/conf/trap/trap_hpux.h */
extern int mount_hpux(MTYPE_TYPE type, const char *dir, int flags, caddr_t data);
#define MOUNT_TRAP(type, mnt, flags, mnt_data) 	mount_hpux(type, mnt->mnt_dir, flags, mnt_data)

/*
 * HPUX 9.x does is not even consistently inconsistent with itself.
 * It defines an integer mount type for PCFS, but not a string type as
 * with all other mount types.
 *
 * XXX: remove this ugly hack when HPUX 9.0 is defunct.
 */
#if MOUNT_TYPE_PCFS == MOUNT_PC
# undef MOUNT_TYPE_PCFS
# define MOUNT_TYPE_PCFS "pcfs"
#endif /* MOUNT_TYPE_PCFS == MOUNT_PC */
