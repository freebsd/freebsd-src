/*
 */

/* zfs_config.h.  Generated from zfs_config.h.in by configure.  */
/* zfs_config.h.in.  Generated from configure.ac by autoheader.  */

/* Define to 1 if translation of program messages to the user's native
   language is requested. */
/* #undef ENABLE_NLS */

/* __assign_str() has one arg */
/* #undef HAVE_1ARG_ASSIGN_STR */

/* lookup_bdev() wants 1 arg */
/* #undef HAVE_1ARG_LOOKUP_BDEV */

/* kernel has access_ok with 'type' parameter */
/* #undef HAVE_ACCESS_OK_TYPE */

/* add_disk() returns int */
/* #undef HAVE_ADD_DISK_RET */

/* Define if host toolchain supports AES */
#define HAVE_AES 1

/* Define if you have [rt] */
#define HAVE_AIO_H 1

#ifdef __amd64__
#ifndef RESCUE
/* Define if host toolchain supports AVX */
#define HAVE_AVX 1
#endif

/* Define if host toolchain supports AVX2 */
#define HAVE_AVX2 1

/* Define if host toolchain supports AVX512BW */
#define HAVE_AVX512BW 1

/* Define if host toolchain supports AVX512CD */
#define HAVE_AVX512CD 1

/* Define if host toolchain supports AVX512DQ */
#define HAVE_AVX512DQ 1

/* Define if host toolchain supports AVX512ER */
#define HAVE_AVX512ER 1

/* Define if host toolchain supports AVX512F */
#define HAVE_AVX512F 1

/* Define if host toolchain supports AVX512IFMA */
#define HAVE_AVX512IFMA 1

/* Define if host toolchain supports AVX512PF */
#define HAVE_AVX512PF 1

/* Define if host toolchain supports AVX512VBMI */
#define HAVE_AVX512VBMI 1

/* Define if host toolchain supports AVX512VL */
#define HAVE_AVX512VL 1
#endif

/* backtrace() is available */
/* #undef HAVE_BACKTRACE */

/* bdevname() is available */
/* #undef HAVE_BDEVNAME */

/* bdev_check_media_change() exists */
/* #undef HAVE_BDEV_CHECK_MEDIA_CHANGE */

/* bdev_file_open_by_path() exists */
/* #undef HAVE_BDEV_FILE_OPEN_BY_PATH */

/* bdev_*_io_acct() available */
/* #undef HAVE_BDEV_IO_ACCT_63 */

/* bdev_*_io_acct() available */
/* #undef HAVE_BDEV_IO_ACCT_OLD */

/* bdev_kobj() exists */
/* #undef HAVE_BDEV_KOBJ */

/* bdev_max_discard_sectors() is available */
/* #undef HAVE_BDEV_MAX_DISCARD_SECTORS */

/* bdev_max_secure_erase_sectors() is available */
/* #undef HAVE_BDEV_MAX_SECURE_ERASE_SECTORS */

/* bdev_nr_bytes() is available */
/* #undef HAVE_BDEV_NR_BYTES */

/* bdev_open_by_path() exists */
/* #undef HAVE_BDEV_OPEN_BY_PATH */

/* bdev_release() exists */
/* #undef HAVE_BDEV_RELEASE */

/* block_device_operations->submit_bio() returns void */
/* #undef HAVE_BDEV_SUBMIT_BIO_RETURNS_VOID */

/* bdev_whole() is available */
/* #undef HAVE_BDEV_WHOLE */

/* bio_alloc() takes 4 arguments */
/* #undef HAVE_BIO_ALLOC_4ARG */

/* bio->bi_bdev->bd_disk exists */
/* #undef HAVE_BIO_BDEV_DISK */

/* bio_*_io_acct() available */
/* #undef HAVE_BIO_IO_ACCT */

/* bio_max_segs() is implemented */
/* #undef HAVE_BIO_MAX_SEGS */

/* bio_set_dev() GPL-only */
/* #undef HAVE_BIO_SET_DEV_GPL_ONLY */

/* bio_set_dev() is a macro */
/* #undef HAVE_BIO_SET_DEV_MACRO */

/* bio_set_op_attrs is available */
/* #undef HAVE_BIO_SET_OP_ATTRS */

/* blkdev_get_by_path() exists and takes 4 args */
/* #undef HAVE_BLKDEV_GET_BY_PATH_4ARG */

/* blkdev_get_by_path() handles ERESTARTSYS */
/* #undef HAVE_BLKDEV_GET_ERESTARTSYS */

/* __blkdev_issue_discard(flags) is available */
/* #undef HAVE_BLKDEV_ISSUE_DISCARD_ASYNC_FLAGS */

/* __blkdev_issue_discard() is available */
/* #undef HAVE_BLKDEV_ISSUE_DISCARD_ASYNC_NOFLAGS */

/* blkdev_issue_discard(flags) is available */
/* #undef HAVE_BLKDEV_ISSUE_DISCARD_FLAGS */

/* blkdev_issue_discard() is available */
/* #undef HAVE_BLKDEV_ISSUE_DISCARD_NOFLAGS */

/* blkdev_issue_secure_erase() is available */
/* #undef HAVE_BLKDEV_ISSUE_SECURE_ERASE */

/* blkdev_put() exists */
/* #undef HAVE_BLKDEV_PUT */

/* blkdev_put() accepts void* as arg 2 */
/* #undef HAVE_BLKDEV_PUT_HOLDER */

/* struct queue_limits has a features field */
/* #undef HAVE_BLKDEV_QUEUE_LIMITS_FEATURES */

/* blkdev_reread_part() exists */
/* #undef HAVE_BLKDEV_REREAD_PART */

/* blkg_tryget() is available */
/* #undef HAVE_BLKG_TRYGET */

/* blkg_tryget() GPL-only */
/* #undef HAVE_BLKG_TRYGET_GPL_ONLY */

/* blk_alloc_disk() exists */
/* #undef HAVE_BLK_ALLOC_DISK */

/* blk_alloc_disk() exists and takes 2 args */
/* #undef HAVE_BLK_ALLOC_DISK_2ARG */

/* blk_alloc_queue() expects request function */
/* #undef HAVE_BLK_ALLOC_QUEUE_REQUEST_FN */

/* blk_alloc_queue_rh() expects request function */
/* #undef HAVE_BLK_ALLOC_QUEUE_REQUEST_FN_RH */

/* blk_cleanup_disk() exists */
/* #undef HAVE_BLK_CLEANUP_DISK */

/* blk_mode_t is defined */
/* #undef HAVE_BLK_MODE_T */

/* block multiqueue hardware context is cached in struct request */
/* #undef HAVE_BLK_MQ_RQ_HCTX */

/* blk queue backing_dev_info is dynamic */
/* #undef HAVE_BLK_QUEUE_BDI_DYNAMIC */

/* blk_queue_discard() is available */
/* #undef HAVE_BLK_QUEUE_DISCARD */

/* backing_dev_info is available through queue gendisk */
/* #undef HAVE_BLK_QUEUE_DISK_BDI */

/* blk_queue_secure_erase() is available */
/* #undef HAVE_BLK_QUEUE_SECURE_ERASE */

/* blk_queue_update_readahead() exists */
/* #undef HAVE_BLK_QUEUE_UPDATE_READAHEAD */

/* BLK_STS_RESV_CONFLICT is defined */
/* #undef HAVE_BLK_STS_RESV_CONFLICT */

/* Define if release() in block_device_operations takes 1 arg */
/* #undef HAVE_BLOCK_DEVICE_OPERATIONS_RELEASE_1ARG */

/* Define if revalidate_disk() in block_device_operations */
/* #undef HAVE_BLOCK_DEVICE_OPERATIONS_REVALIDATE_DISK */

/* check_disk_change() exists */
/* #undef HAVE_CHECK_DISK_CHANGE */

/* copy_splice_read exists */
/* #undef HAVE_COPY_SPLICE_READ */

/* cpu_has_feature() is GPL-only */
/* #undef HAVE_CPU_HAS_FEATURE_GPL_ONLY */

/* Define if the GNU dcgettext() function is already present or preinstalled.
   */
/* #undef HAVE_DCGETTEXT */

/* DECLARE_EVENT_CLASS() is available */
/* #undef HAVE_DECLARE_EVENT_CLASS */

/* dequeue_signal() takes a task argument */
/* #undef HAVE_DEQUEUE_SIGNAL_3ARG_TASK */

/* dequeue_signal() takes 4 arguments */
/* #undef HAVE_DEQUEUE_SIGNAL_4ARG */

/* lookup_bdev() wants dev_t arg */
/* #undef HAVE_DEVT_LOOKUP_BDEV */

/* disk_check_media_change() exists */
/* #undef HAVE_DISK_CHECK_MEDIA_CHANGE */

/* disk_*_io_acct() available */
/* #undef HAVE_DISK_IO_ACCT */

/* disk_update_readahead() exists */
/* #undef HAVE_DISK_UPDATE_READAHEAD */

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if you have the 'execvpe' function. */
#define HAVE_EXECVPE 1

/* fault_in_iov_iter_readable() is available */
/* #undef HAVE_FAULT_IN_IOV_ITER_READABLE */

/* file->f_version exists */
/* #undef HAVE_FILE_F_VERSION */

/* flush_dcache_page() is GPL-only */
/* #undef HAVE_FLUSH_DCACHE_PAGE_GPL_ONLY */

/* Define if compiler supports -Wformat-overflow */
/* #undef HAVE_FORMAT_OVERFLOW */

/* fsync_bdev() is declared in include/blkdev.h */
/* #undef HAVE_FSYNC_BDEV */

/* yes */
/* #undef HAVE_GENERIC_FADVISE */

/* generic_fillattr requires struct mnt_idmap* */
/* #undef HAVE_GENERIC_FILLATTR_IDMAP */

/* generic_fillattr requires struct mnt_idmap* and u32 request_mask */
/* #undef HAVE_GENERIC_FILLATTR_IDMAP_REQMASK */

/* generic_fillattr requires struct user_namespace* */
/* #undef HAVE_GENERIC_FILLATTR_USERNS */

/* generic_*_io_acct() 4 arg available */
/* #undef HAVE_GENERIC_IO_ACCT_4ARG */

/* GENHD_FL_EXT_DEVT flag is available */
/* #undef HAVE_GENHD_FL_EXT_DEVT */

/* GENHD_FL_NO_PART flag is available */
/* #undef HAVE_GENHD_FL_NO_PART */

/* Define if the GNU gettext() function is already present or preinstalled. */
/* #undef HAVE_GETTEXT */

/* Define to 1 if you have the 'gettid' function. */
/* #undef HAVE_GETTID */

/* iops->get_acl() exists */
/* #undef HAVE_GET_ACL */

/* iops->get_acl() takes rcu */
/* #undef HAVE_GET_ACL_RCU */

/* has iops->get_inode_acl() */
/* #undef HAVE_GET_INODE_ACL */

/* iattr->ia_vfsuid and iattr->ia_vfsgid exist */
/* #undef HAVE_IATTR_VFSID */

/* Define if you have the iconv() function and it works. */
#define HAVE_ICONV 1

/* iops->getattr() takes struct mnt_idmap* */
/* #undef HAVE_IDMAP_IOPS_GETATTR */

/* iops->setattr() takes struct mnt_idmap* */
/* #undef HAVE_IDMAP_IOPS_SETATTR */

/* APIs for idmapped mount are present */
/* #undef HAVE_IDMAP_MNT_API */

/* mnt_idmap does not have user_namespace */
/* #undef HAVE_IDMAP_NO_USERNS */

/* Define if compiler supports -Wimplicit-fallthrough */
/* #undef HAVE_IMPLICIT_FALLTHROUGH */

/* Define if compiler supports -Winfinite-recursion */
/* #undef HAVE_INFINITE_RECURSION */

/* inode_get_atime() exists in linux/fs.h */
/* #undef HAVE_INODE_GET_ATIME */

/* inode_get_ctime() exists in linux/fs.h */
/* #undef HAVE_INODE_GET_CTIME */

/* inode_get_mtime() exists in linux/fs.h */
/* #undef HAVE_INODE_GET_MTIME */

/* inode_owner_or_capable() exists */
/* #undef HAVE_INODE_OWNER_OR_CAPABLE */

/* inode_owner_or_capable() takes mnt_idmap */
/* #undef HAVE_INODE_OWNER_OR_CAPABLE_IDMAP */

/* inode_owner_or_capable() takes user_ns */
/* #undef HAVE_INODE_OWNER_OR_CAPABLE_USERNS */

/* inode_set_atime_to_ts() exists in linux/fs.h */
/* #undef HAVE_INODE_SET_ATIME_TO_TS */

/* inode_set_ctime_to_ts() exists in linux/fs.h */
/* #undef HAVE_INODE_SET_CTIME_TO_TS */

/* inode_set_mtime_to_ts() exists in linux/fs.h */
/* #undef HAVE_INODE_SET_MTIME_TO_TS */

/* timestamp_truncate() exists */
/* #undef HAVE_INODE_TIMESTAMP_TRUNCATE */

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* iops->create() takes struct mnt_idmap* */
/* #undef HAVE_IOPS_CREATE_IDMAP */

/* iops->create() takes struct user_namespace* */
/* #undef HAVE_IOPS_CREATE_USERNS */

/* iops->mkdir() takes struct mnt_idmap* */
/* #undef HAVE_IOPS_MKDIR_IDMAP */

/* iops->mkdir() takes struct user_namespace* */
/* #undef HAVE_IOPS_MKDIR_USERNS */

/* iops->mknod() takes struct mnt_idmap* */
/* #undef HAVE_IOPS_MKNOD_IDMAP */

/* iops->mknod() takes struct user_namespace* */
/* #undef HAVE_IOPS_MKNOD_USERNS */

/* iops->permission() takes struct mnt_idmap* */
/* #undef HAVE_IOPS_PERMISSION_IDMAP */

/* iops->permission() takes struct user_namespace* */
/* #undef HAVE_IOPS_PERMISSION_USERNS */

/* iops->rename() takes struct mnt_idmap* */
/* #undef HAVE_IOPS_RENAME_IDMAP */

/* iops->rename() takes struct user_namespace* */
/* #undef HAVE_IOPS_RENAME_USERNS */

/* iops->symlink() takes struct mnt_idmap* */
/* #undef HAVE_IOPS_SYMLINK_IDMAP */

/* iops->symlink() takes struct user_namespace* */
/* #undef HAVE_IOPS_SYMLINK_USERNS */

/* iov_iter_get_pages() is available */
/* #undef HAVE_IOV_ITER_GET_PAGES */

/* iov_iter_get_pages2() is available */
/* #undef HAVE_IOV_ITER_GET_PAGES2 */

/* iov_iter_type() is available */
/* #undef HAVE_IOV_ITER_TYPE */

/* Define to 1 if you have the 'issetugid' function. */
#define HAVE_ISSETUGID 1

/* iter_iov() is available */
/* #undef HAVE_ITER_IOV */

/* kernel has kernel_fpu_* functions */
/* #undef HAVE_KERNEL_FPU */

/* kernel has asm/fpu/api.h */
/* #undef HAVE_KERNEL_FPU_API_HEADER */

/* kernel fpu internal */
/* #undef HAVE_KERNEL_FPU_INTERNAL */

/* kernel has asm/fpu/internal.h */
/* #undef HAVE_KERNEL_FPU_INTERNAL_HEADER */

/* Define if compiler supports -Winfinite-recursion */
/* #undef HAVE_KERNEL_INFINITE_RECURSION */

/* kernel defines intptr_t */
/* #undef HAVE_KERNEL_INTPTR_T */

/* kernel has kernel_neon_* functions */
/* #undef HAVE_KERNEL_NEON */

/* kernel does stack verification */
/* #undef HAVE_KERNEL_OBJTOOL */

/* kernel has linux/objtool.h */
/* #undef HAVE_KERNEL_OBJTOOL_HEADER */

/* strlcpy() exists */
/* #undef HAVE_KERNEL_STRLCPY */

/* kernel has kmap_local_page */
/* #undef HAVE_KMAP_LOCAL_PAGE */

/* Define if you have [aio] */
/* #undef HAVE_LIBAIO */

/* Define if you have [blkid] */
/* #undef HAVE_LIBBLKID */

/* Define if you have [crypto] */
#define HAVE_LIBCRYPTO 1

/* Define if you have [tirpc] */
/* #undef HAVE_LIBTIRPC */

/* Define if you have [udev] */
/* #undef HAVE_LIBUDEV */

/* Define if you have [unwind] */
/* #undef HAVE_LIBUNWIND */

/* libunwind has unw_get_elf_filename */
/* #undef HAVE_LIBUNWIND_ELF */

/* Define if you have [uuid] */
/* #undef HAVE_LIBUUID */

/* building against unsupported kernel version */
/* #undef HAVE_LINUX_EXPERIMENTAL */

/* makedev() is declared in sys/mkdev.h */
/* #undef HAVE_MAKEDEV_IN_MKDEV */

/* makedev() is declared in sys/sysmacros.h */
/* #undef HAVE_MAKEDEV_IN_SYSMACROS */

/* Noting that make_request_fn() returns blk_qc_t */
/* #undef HAVE_MAKE_REQUEST_FN_RET_QC */

/* Define to 1 if you have the 'mlockall' function. */
#define HAVE_MLOCKALL 1

/* PG_error flag is available */
/* #undef HAVE_MM_PAGE_FLAG_ERROR */

/* page_mapping() is available */
/* #undef HAVE_MM_PAGE_MAPPING */

/* page_size() is available */
/* #undef HAVE_MM_PAGE_SIZE */

/* Define if host toolchain supports MOVBE */
#define HAVE_MOVBE 1

/* folio_wait_bit() exists */
/* #undef HAVE_PAGEMAP_FOLIO_WAIT_BIT */

/* part_to_dev() exists */
/* #undef HAVE_PART_TO_DEV */

/* iops->getattr() takes a path */
/* #undef HAVE_PATH_IOPS_GETATTR */

/* Define if host toolchain supports PCLMULQDQ */
#define HAVE_PCLMULQDQ 1

/* proc_handler ctl_table arg is const */
/* #undef HAVE_PROC_HANDLER_CTL_TABLE_CONST */

/* proc_ops structure exists */
/* #undef HAVE_PROC_OPS_STRUCT */

/* If available, contains the Python version number currently in use. */
#define HAVE_PYTHON "3.7"

/* qat is enabled and existed */
/* #undef HAVE_QAT */

/* struct reclaim_state has reclaimed */
/* #undef HAVE_RECLAIM_STATE_RECLAIMED */

/* register_shrinker is vararg */
/* #undef HAVE_REGISTER_SHRINKER_VARARG */

/* register_sysctl_sz exists */
/* #undef HAVE_REGISTER_SYSCTL_SZ */

/* register_sysctl_table exists */
/* #undef HAVE_REGISTER_SYSCTL_TABLE */

/* iops->rename() wants flags */
/* #undef HAVE_RENAME_WANTS_FLAGS */

/* revalidate_disk() is available */
/* #undef HAVE_REVALIDATE_DISK */

/* revalidate_disk_size() is available */
/* #undef HAVE_REVALIDATE_DISK_SIZE */

/* Define to 1 if you have the <security/pam_modules.h> header file. */
#define HAVE_SECURITY_PAM_MODULES_H 1

/* setattr_prepare() accepts mnt_idmap */
/* #undef HAVE_SETATTR_PREPARE_IDMAP */

/* setattr_prepare() is available, doesn't accept user_namespace */
/* #undef HAVE_SETATTR_PREPARE_NO_USERNS */

/* setattr_prepare() accepts user_namespace */
/* #undef HAVE_SETATTR_PREPARE_USERNS */

/* iops->set_acl() takes 4 args, arg1 is struct mnt_idmap * */
/* #undef HAVE_SET_ACL_IDMAP_DENTRY */

/* iops->set_acl() takes 4 args */
/* #undef HAVE_SET_ACL_USERNS */

/* iops->set_acl() takes 4 args, arg2 is struct dentry * */
/* #undef HAVE_SET_ACL_USERNS_DENTRY_ARG2 */

/* shrinker_register exists */
/* #undef HAVE_SHRINKER_REGISTER */

/* kernel_siginfo_t exists */
/* #undef HAVE_SIGINFO */

#if defined(__amd64__) || defined(__i386__)
/* Define if host toolchain supports SSE */
#define HAVE_SSE 1

/* Define if host toolchain supports SSE2 */
#define HAVE_SSE2 1

/* Define if host toolchain supports SSE3 */
#define HAVE_SSE3 1

/* Define if host toolchain supports SSE4.1 */
#define HAVE_SSE4_1 1

/* Define if host toolchain supports SSE4.2 */
#define HAVE_SSE4_2 1

/* Define if host toolchain supports SSSE3 */
#define HAVE_SSSE3 1
#endif

/* STACK_FRAME_NON_STANDARD is defined */
/* #undef HAVE_STACK_FRAME_NON_STANDARD */

/* standalone <linux/stdarg.h> exists */
/* #undef HAVE_STANDALONE_LINUX_STDARG */

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdio.h> header file. */
#define HAVE_STDIO_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the 'strlcat' function. */
#define HAVE_STRLCAT 1

/* Define to 1 if you have the 'strlcpy' function. */
#define HAVE_STRLCPY 1

/* submit_bio is member of struct block_device_operations */
/* #undef HAVE_SUBMIT_BIO_IN_BLOCK_DEVICE_OPERATIONS */

/* have super_block s_shrink */
/* #undef HAVE_SUPER_BLOCK_S_SHRINK */

/* have super_block s_shrink pointer */
/* #undef HAVE_SUPER_BLOCK_S_SHRINK_PTR */

/* sync_blockdev() is declared in include/blkdev.h */
/* #undef HAVE_SYNC_BLOCKDEV */

/* struct kobj_type has default_groups */
/* #undef HAVE_SYSFS_DEFAULT_GROUPS */

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* i_op->tmpfile() uses old dentry signature */
/* #undef HAVE_TMPFILE_DENTRY */

/* i_op->tmpfile() has mnt_idmap */
/* #undef HAVE_TMPFILE_IDMAP */

/* i_op->tmpfile() has userns */
/* #undef HAVE_TMPFILE_USERNS */

/* totalhigh_pages() exists */
/* #undef HAVE_TOTALHIGH_PAGES */

/* kernel has totalram_pages() */
/* #undef HAVE_TOTALRAM_PAGES_FUNC */

/* Define to 1 if you have the 'udev_device_get_is_initialized' function. */
/* #undef HAVE_UDEV_DEVICE_GET_IS_INITIALIZED */

/* kernel has __kernel_fpu_* functions */
/* #undef HAVE_UNDERSCORE_KERNEL_FPU */

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* iops->getattr() takes struct user_namespace* */
/* #undef HAVE_USERNS_IOPS_GETATTR */

/* iops->setattr() takes struct user_namespace* */
/* #undef HAVE_USERNS_IOPS_SETATTR */

/* fops->clone_file_range() is available */
/* #undef HAVE_VFS_CLONE_FILE_RANGE */

/* fops->dedupe_file_range() is available */
/* #undef HAVE_VFS_DEDUPE_FILE_RANGE */

/* aops->direct_IO() uses iov_iter without rw */
/* #undef HAVE_VFS_DIRECT_IO_ITER */

/* aops->direct_IO() uses iov_iter with offset */
/* #undef HAVE_VFS_DIRECT_IO_ITER_OFFSET */

/* filemap_dirty_folio exists */
/* #undef HAVE_VFS_FILEMAP_DIRTY_FOLIO */

/* generic_copy_file_range() is available */
/* #undef HAVE_VFS_GENERIC_COPY_FILE_RANGE */

/* invalidate_folio exists */
/* #undef HAVE_VFS_INVALIDATE_FOLIO */

/* All required iov_iter interfaces are available */
/* #undef HAVE_VFS_IOV_ITER */

/* address_space_operations->readpages exists */
/* #undef HAVE_VFS_READPAGES */

/* read_folio exists */
/* #undef HAVE_VFS_READ_FOLIO */

/* release_folio exists */
/* #undef HAVE_VFS_RELEASE_FOLIO */

/* fops->remap_file_range() is available */
/* #undef HAVE_VFS_REMAP_FILE_RANGE */

/* __set_page_dirty_nobuffers exists */
/* #undef HAVE_VFS_SET_PAGE_DIRTY_NOBUFFERS */

/* splice_copy_file_range() is available */
/* #undef HAVE_VFS_SPLICE_COPY_FILE_RANGE */

/* __vmalloc page flags exists */
/* #undef HAVE_VMALLOC_PAGE_KERNEL */

/* int (*writepage_t)() takes struct folio* */
/* #undef HAVE_WRITEPAGE_T_FOLIO */

/* xattr_handler->get() wants dentry and inode and flags */
/* #undef HAVE_XATTR_GET_DENTRY_INODE_FLAGS */

/* xattr_handler->set() wants both dentry and inode */
/* #undef HAVE_XATTR_SET_DENTRY_INODE */

/* xattr_handler->set() takes mnt_idmap */
/* #undef HAVE_XATTR_SET_IDMAP */

/* xattr_handler->set() takes user_namespace */
/* #undef HAVE_XATTR_SET_USERNS */

/* Define if host toolchain supports XSAVE */
#define HAVE_XSAVE 1

/* Define if host toolchain supports XSAVEOPT */
#define HAVE_XSAVEOPT 1

/* Define if host toolchain supports XSAVES */
#define HAVE_XSAVES 1

/* ZERO_PAGE() is GPL-only */
/* #undef HAVE_ZERO_PAGE_GPL_ONLY */

/* Define if you have [z] */
#define HAVE_ZLIB 1

/* kernel exports FPU functions */
/* #undef KERNEL_EXPORTS_X86_FPU */

/* TBD: fetch(3) support */
#if 0
/* whether the chosen libfetch is to be loaded at run-time */
#define LIBFETCH_DYNAMIC 1

/* libfetch is fetch(3) */
#define LIBFETCH_IS_FETCH 1

/* libfetch is libcurl */
#define LIBFETCH_IS_LIBCURL 0

/* soname of chosen libfetch */
#define LIBFETCH_SONAME "libfetch.so.6"
#endif

/* Define to the sub-directory where libtool stores uninstalled libraries. */
#define LT_OBJDIR ".libs/"

/* make_request_fn() return type */
/* #undef MAKE_REQUEST_FN_RET */

/* using complete_and_exit() instead */
/* #undef SPL_KTHREAD_COMPLETE_AND_EXIT */

/* Defined for legacy compatibility. */
#define SPL_META_ALIAS ZFS_META_ALIAS

/* Defined for legacy compatibility. */
#define SPL_META_RELEASE ZFS_META_RELEASE

/* Defined for legacy compatibility. */
#define SPL_META_VERSION ZFS_META_VERSION

/* pde_data() is PDE_DATA() */
/* #undef SPL_PDE_DATA */

/* Define to 1 if all of the C89 standard headers exist (not just the ones
   required in a freestanding environment). This macro is provided for
   backward compatibility; new code need not use it. */
#define SYSTEM_FREEBSD 1

/* True if ZFS is to be compiled for a Linux system */
/* #undef SYSTEM_LINUX */

/* Version number of package */
/* #undef ZFS_DEBUG */

/* /dev/zfs minor */
/* #undef ZFS_DEVICE_MINOR */

/* Define the project alias string. */
#define ZFS_META_ALIAS "zfs-2.3.99-31-FreeBSD_gb2f6de7b5"

/* Define the project author. */
#define ZFS_META_AUTHOR "OpenZFS"

/* Define the project release date. */
/* #undef ZFS_META_DATA */

/* Define the maximum compatible kernel version. */
#define ZFS_META_KVER_MAX "6.11"

/* Define the minimum compatible kernel version. */
#define ZFS_META_KVER_MIN "4.18"

/* Define the project license. */
#define ZFS_META_LICENSE "CDDL"

/* Define the libtool library 'age' version information. */
/* #undef ZFS_META_LT_AGE */

/* Define the libtool library 'current' version information. */
/* #undef ZFS_META_LT_CURRENT */

/* Define the libtool library 'revision' version information. */
/* #undef ZFS_META_LT_REVISION */

/* Define the project name. */
#define ZFS_META_NAME "zfs"

/* Define the project release. */
#define ZFS_META_RELEASE "31-FreeBSD_gb2f6de7b5"

/* Define the project version. */
#define ZFS_META_VERSION "2.3.99"

/* count is located in percpu_ref.data */
/* #undef ZFS_PERCPU_REF_COUNT_IN_DATA */
