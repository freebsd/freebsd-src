/*
 * Herein lies all the functions/variables that are "exported" for linkage
 * with dynamically loaded kernel modules.
 *			Jon.
 *
 * - Stacked module support and unified symbol table added (June 1994)
 * - External symbol table support added (December 1994)
 * - Versions on symbols added (December 1994)
 *   by Bjorn Ekwall <bj0rn@blox.se>
 */

#include <linux/config.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/blkdev.h>
#include <linux/cdrom.h>
#include <linux/kernel_stat.h>
#include <linux/vmalloc.h>
#include <linux/sys.h>
#include <linux/utsname.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/serial.h>
#include <linux/locks.h>
#include <linux/delay.h>
#include <linux/random.h>
#include <linux/reboot.h>
#include <linux/pagemap.h>
#include <linux/sysctl.h>
#include <linux/hdreg.h>
#include <linux/skbuff.h>
#include <linux/genhd.h>
#include <linux/blkpg.h>
#include <linux/swap.h>
#include <linux/ctype.h>
#include <linux/file.h>
#include <linux/iobuf.h>
#include <linux/console.h>
#include <linux/poll.h>
#include <linux/mmzone.h>
#include <linux/mm.h>
#include <linux/capability.h>
#include <linux/highuid.h>
#include <linux/brlock.h>
#include <linux/fs.h>
#include <linux/tty.h>
#include <linux/in6.h>
#include <linux/completion.h>
#include <linux/seq_file.h>
#include <linux/dnotify.h>
#include <linux/crc32.h>
#include <linux/firmware.h>
#include <asm/checksum.h>

#if defined(CONFIG_PROC_FS)
#include <linux/proc_fs.h>
#endif
#ifdef CONFIG_KMOD
#include <linux/kmod.h>
#endif

extern void set_device_ro(kdev_t dev,int flag);

extern void *sys_call_table;

extern struct timezone sys_tz;
extern int request_dma(unsigned int dmanr, char * deviceID);
extern void free_dma(unsigned int dmanr);
extern spinlock_t dma_spin_lock;
extern int panic_timeout;

#ifdef CONFIG_MODVERSIONS
const struct module_symbol __export_Using_Versions
__attribute__((section("__ksymtab"))) = {
	1 /* Version version */, "Using_Versions"
};
#endif


EXPORT_SYMBOL(inter_module_register);
EXPORT_SYMBOL(inter_module_unregister);
EXPORT_SYMBOL(inter_module_get);
EXPORT_SYMBOL(inter_module_get_request);
EXPORT_SYMBOL(inter_module_put);
EXPORT_SYMBOL(try_inc_mod_count);

/* process memory management */
EXPORT_SYMBOL(do_mmap_pgoff);
EXPORT_SYMBOL(do_munmap);
EXPORT_SYMBOL(do_brk);
EXPORT_SYMBOL(exit_mm);
EXPORT_SYMBOL(exit_files);
EXPORT_SYMBOL(exit_fs);
EXPORT_SYMBOL(exit_sighand);

/* internal kernel memory management */
EXPORT_SYMBOL(_alloc_pages);
EXPORT_SYMBOL(__alloc_pages);
EXPORT_SYMBOL(alloc_pages_node);
EXPORT_SYMBOL(__get_free_pages);
EXPORT_SYMBOL(get_zeroed_page);
EXPORT_SYMBOL(__free_pages);
EXPORT_SYMBOL(free_pages);
EXPORT_SYMBOL(num_physpages);
EXPORT_SYMBOL(kmem_find_general_cachep);
EXPORT_SYMBOL(kmem_cache_create);
EXPORT_SYMBOL(kmem_cache_destroy);
EXPORT_SYMBOL(kmem_cache_shrink);
EXPORT_SYMBOL(kmem_cache_alloc);
EXPORT_SYMBOL(kmem_cache_free);
EXPORT_SYMBOL(kmem_cache_size);
EXPORT_SYMBOL(kmalloc);
EXPORT_SYMBOL(kfree);
EXPORT_SYMBOL(vfree);
EXPORT_SYMBOL(__vmalloc);
EXPORT_SYMBOL(vmap);
EXPORT_SYMBOL(vmalloc_to_page);
EXPORT_SYMBOL(mem_map);
EXPORT_SYMBOL(remap_page_range);
EXPORT_SYMBOL(max_mapnr);
EXPORT_SYMBOL(high_memory);
EXPORT_SYMBOL(vmtruncate);
EXPORT_SYMBOL(find_vma);
EXPORT_SYMBOL(get_unmapped_area);
EXPORT_SYMBOL(init_mm);
#ifdef CONFIG_HIGHMEM
EXPORT_SYMBOL(kmap_high);
EXPORT_SYMBOL(kunmap_high);
EXPORT_SYMBOL(highmem_start_page);
EXPORT_SYMBOL(create_bounce);
EXPORT_SYMBOL(kmap_prot);
EXPORT_SYMBOL(kmap_pte);
#endif

/* filesystem internal functions */
EXPORT_SYMBOL(def_blk_fops);
EXPORT_SYMBOL(update_atime);
EXPORT_SYMBOL(get_fs_type);
EXPORT_SYMBOL(get_super);
EXPORT_SYMBOL(drop_super);
EXPORT_SYMBOL(getname);
EXPORT_SYMBOL(names_cachep);
EXPORT_SYMBOL(fput);
EXPORT_SYMBOL(fget);
EXPORT_SYMBOL(igrab);
EXPORT_SYMBOL(iunique);
EXPORT_SYMBOL(ilookup);
EXPORT_SYMBOL(iget4_locked);
EXPORT_SYMBOL(unlock_new_inode);
EXPORT_SYMBOL(iput);
EXPORT_SYMBOL(inode_init_once);
EXPORT_SYMBOL(__inode_init_once);
EXPORT_SYMBOL(force_delete);
EXPORT_SYMBOL(follow_up);
EXPORT_SYMBOL(follow_down);
EXPORT_SYMBOL(lookup_mnt);
EXPORT_SYMBOL(path_init);
EXPORT_SYMBOL(path_walk);
EXPORT_SYMBOL(path_lookup);
EXPORT_SYMBOL(path_release);
EXPORT_SYMBOL(__user_walk);
EXPORT_SYMBOL(lookup_one_len);
EXPORT_SYMBOL(lookup_hash);
EXPORT_SYMBOL(sys_close);
EXPORT_SYMBOL(dcache_lock);
EXPORT_SYMBOL(d_alloc_root);
EXPORT_SYMBOL(d_delete);
EXPORT_SYMBOL(dget_locked);
EXPORT_SYMBOL(d_validate);
EXPORT_SYMBOL(d_rehash);
EXPORT_SYMBOL(d_invalidate);	/* May be it will be better in dcache.h? */
EXPORT_SYMBOL(d_move);
EXPORT_SYMBOL(d_instantiate);
EXPORT_SYMBOL(d_alloc);
EXPORT_SYMBOL(d_lookup);
EXPORT_SYMBOL(__d_path);
EXPORT_SYMBOL(mark_buffer_dirty);
EXPORT_SYMBOL(set_buffer_async_io); /* for reiserfs_writepage */
EXPORT_SYMBOL(end_buffer_io_async);
EXPORT_SYMBOL(__mark_buffer_dirty);
EXPORT_SYMBOL(__mark_inode_dirty);
EXPORT_SYMBOL(fd_install);
EXPORT_SYMBOL(get_empty_filp);
EXPORT_SYMBOL(init_private_file);
EXPORT_SYMBOL(filp_open);
EXPORT_SYMBOL(filp_close);
EXPORT_SYMBOL(put_filp);
EXPORT_SYMBOL(files_lock);
EXPORT_SYMBOL(check_disk_change);
EXPORT_SYMBOL(__invalidate_buffers);
EXPORT_SYMBOL(invalidate_bdev);
EXPORT_SYMBOL(invalidate_inodes);
EXPORT_SYMBOL(invalidate_device);
EXPORT_SYMBOL(invalidate_inode_pages);
EXPORT_SYMBOL(truncate_inode_pages);
EXPORT_SYMBOL(fsync_dev);
EXPORT_SYMBOL(fsync_no_super);
EXPORT_SYMBOL(permission);
EXPORT_SYMBOL(vfs_permission);
EXPORT_SYMBOL(inode_setattr);
EXPORT_SYMBOL(inode_change_ok);
EXPORT_SYMBOL(write_inode_now);
EXPORT_SYMBOL(notify_change);
EXPORT_SYMBOL(set_blocksize);
EXPORT_SYMBOL(sb_set_blocksize);
EXPORT_SYMBOL(sb_min_blocksize);
EXPORT_SYMBOL(getblk);
EXPORT_SYMBOL(cdget);
EXPORT_SYMBOL(cdput);
EXPORT_SYMBOL(bdget);
EXPORT_SYMBOL(bdput);
EXPORT_SYMBOL(bread);
EXPORT_SYMBOL(__brelse);
EXPORT_SYMBOL(__bforget);
EXPORT_SYMBOL(ll_rw_block);
EXPORT_SYMBOL(submit_bh);
EXPORT_SYMBOL(unlock_buffer);
EXPORT_SYMBOL(__wait_on_buffer);
EXPORT_SYMBOL(___wait_on_page);
EXPORT_SYMBOL(generic_direct_IO);
EXPORT_SYMBOL(discard_bh_page);
EXPORT_SYMBOL(block_write_full_page);
EXPORT_SYMBOL(block_read_full_page);
EXPORT_SYMBOL(block_prepare_write);
EXPORT_SYMBOL(block_sync_page);
EXPORT_SYMBOL(generic_cont_expand);
EXPORT_SYMBOL(cont_prepare_write);
EXPORT_SYMBOL(generic_commit_write);
EXPORT_SYMBOL(block_truncate_page);
EXPORT_SYMBOL(generic_block_bmap);
EXPORT_SYMBOL(generic_file_read);
EXPORT_SYMBOL(do_generic_file_read);
EXPORT_SYMBOL(do_generic_file_write);
EXPORT_SYMBOL(do_generic_direct_read);
EXPORT_SYMBOL(do_generic_direct_write);
EXPORT_SYMBOL(generic_file_write);
EXPORT_SYMBOL(generic_file_mmap);
EXPORT_SYMBOL(generic_ro_fops);
EXPORT_SYMBOL(generic_buffer_fdatasync);
EXPORT_SYMBOL(page_hash_bits);
EXPORT_SYMBOL(page_hash_table);
EXPORT_SYMBOL(file_lock_list);
EXPORT_SYMBOL(locks_init_lock);
EXPORT_SYMBOL(locks_copy_lock);
EXPORT_SYMBOL(posix_lock_file);
EXPORT_SYMBOL(posix_test_lock);
EXPORT_SYMBOL(posix_block_lock);
EXPORT_SYMBOL(posix_unblock_lock);
EXPORT_SYMBOL(posix_locks_deadlock);
EXPORT_SYMBOL(locks_mandatory_area);
EXPORT_SYMBOL(dput);
EXPORT_SYMBOL(have_submounts);
EXPORT_SYMBOL(d_find_alias);
EXPORT_SYMBOL(d_prune_aliases);
EXPORT_SYMBOL(prune_dcache);
EXPORT_SYMBOL(shrink_dcache_sb);
EXPORT_SYMBOL(shrink_dcache_parent);
EXPORT_SYMBOL(find_inode_number);
EXPORT_SYMBOL(is_subdir);
EXPORT_SYMBOL(get_unused_fd);
EXPORT_SYMBOL(put_unused_fd);
EXPORT_SYMBOL(vfs_create);
EXPORT_SYMBOL(vfs_mkdir);
EXPORT_SYMBOL(vfs_mknod);
EXPORT_SYMBOL(vfs_symlink);
EXPORT_SYMBOL(vfs_link);
EXPORT_SYMBOL(vfs_rmdir);
EXPORT_SYMBOL(vfs_unlink);
EXPORT_SYMBOL(vfs_rename);
EXPORT_SYMBOL(vfs_statfs);
EXPORT_SYMBOL(generic_read_dir);
EXPORT_SYMBOL(generic_file_llseek);
EXPORT_SYMBOL(no_llseek);
EXPORT_SYMBOL(__pollwait);
EXPORT_SYMBOL(poll_freewait);
EXPORT_SYMBOL(ROOT_DEV);
EXPORT_SYMBOL(__find_get_page);
EXPORT_SYMBOL(__find_lock_page);
EXPORT_SYMBOL(find_trylock_page);
EXPORT_SYMBOL(find_or_create_page);
EXPORT_SYMBOL(grab_cache_page_nowait);
EXPORT_SYMBOL(read_cache_page);
EXPORT_SYMBOL(set_page_dirty);
EXPORT_SYMBOL(mark_page_accessed);
EXPORT_SYMBOL(vfs_readlink);
EXPORT_SYMBOL(vfs_follow_link);
EXPORT_SYMBOL(page_readlink);
EXPORT_SYMBOL(page_follow_link);
EXPORT_SYMBOL(page_symlink_inode_operations);
EXPORT_SYMBOL(block_symlink);
EXPORT_SYMBOL(vfs_readdir);
EXPORT_SYMBOL(__get_lease);
EXPORT_SYMBOL(lease_get_mtime);
EXPORT_SYMBOL(lock_may_read);
EXPORT_SYMBOL(lock_may_write);
EXPORT_SYMBOL(dcache_dir_open);
EXPORT_SYMBOL(dcache_dir_close);
EXPORT_SYMBOL(dcache_dir_lseek);
EXPORT_SYMBOL(dcache_dir_fsync);
EXPORT_SYMBOL(dcache_readdir);
EXPORT_SYMBOL(dcache_dir_ops);

/* for stackable file systems (lofs, wrapfs, cryptfs, etc.) */
EXPORT_SYMBOL(default_llseek);
EXPORT_SYMBOL(dentry_open);
EXPORT_SYMBOL(filemap_nopage);
EXPORT_SYMBOL(filemap_sync);
EXPORT_SYMBOL(filemap_fdatawrite);
EXPORT_SYMBOL(filemap_fdatasync);
EXPORT_SYMBOL(filemap_fdatawait);
EXPORT_SYMBOL(lock_page);
EXPORT_SYMBOL(unlock_page);
EXPORT_SYMBOL(wakeup_page_waiters);

/* device registration */
EXPORT_SYMBOL(register_chrdev);
EXPORT_SYMBOL(unregister_chrdev);
EXPORT_SYMBOL(register_blkdev);
EXPORT_SYMBOL(unregister_blkdev);
EXPORT_SYMBOL(tty_register_driver);
EXPORT_SYMBOL(tty_unregister_driver);
EXPORT_SYMBOL(tty_std_termios);

/* block device driver support */
EXPORT_SYMBOL(blksize_size);
EXPORT_SYMBOL(hardsect_size);
EXPORT_SYMBOL(blk_size);
EXPORT_SYMBOL(blk_dev);
EXPORT_SYMBOL(is_read_only);
EXPORT_SYMBOL(set_device_ro);
EXPORT_SYMBOL(bmap);
EXPORT_SYMBOL(sync_dev);
EXPORT_SYMBOL(devfs_register_partitions);
EXPORT_SYMBOL(blkdev_open);
EXPORT_SYMBOL(blkdev_get);
EXPORT_SYMBOL(blkdev_put);
EXPORT_SYMBOL(ioctl_by_bdev);
EXPORT_SYMBOL(grok_partitions);
EXPORT_SYMBOL(register_disk);
EXPORT_SYMBOL(tq_disk);
EXPORT_SYMBOL(init_buffer);
EXPORT_SYMBOL(refile_buffer);
EXPORT_SYMBOL(max_sectors);
EXPORT_SYMBOL(max_readahead);

/* tty routines */
EXPORT_SYMBOL(tty_hangup);
EXPORT_SYMBOL(tty_wait_until_sent);
EXPORT_SYMBOL(tty_check_change);
EXPORT_SYMBOL(tty_hung_up_p);
EXPORT_SYMBOL(tty_flip_buffer_push);
EXPORT_SYMBOL(tty_get_baud_rate);
EXPORT_SYMBOL(do_SAK);

/* filesystem registration */
EXPORT_SYMBOL(register_filesystem);
EXPORT_SYMBOL(unregister_filesystem);
EXPORT_SYMBOL(kern_mount);
EXPORT_SYMBOL(__mntput);
EXPORT_SYMBOL(may_umount);

/* executable format registration */
EXPORT_SYMBOL(register_binfmt);
EXPORT_SYMBOL(unregister_binfmt);
EXPORT_SYMBOL(search_binary_handler);
EXPORT_SYMBOL(prepare_binprm);
EXPORT_SYMBOL(compute_creds);
EXPORT_SYMBOL(remove_arg_zero);
EXPORT_SYMBOL(set_binfmt);

/* sysctl table registration */
EXPORT_SYMBOL(register_sysctl_table);
EXPORT_SYMBOL(unregister_sysctl_table);
EXPORT_SYMBOL(sysctl_string);
EXPORT_SYMBOL(sysctl_intvec);
EXPORT_SYMBOL(sysctl_jiffies);
EXPORT_SYMBOL(proc_dostring);
EXPORT_SYMBOL(proc_dointvec);
EXPORT_SYMBOL(proc_dointvec_jiffies);
EXPORT_SYMBOL(proc_dointvec_minmax);
EXPORT_SYMBOL(proc_doulongvec_ms_jiffies_minmax);
EXPORT_SYMBOL(proc_doulongvec_minmax);

/* interrupt handling */
EXPORT_SYMBOL(add_timer);
EXPORT_SYMBOL(del_timer);
EXPORT_SYMBOL(request_irq);
EXPORT_SYMBOL(free_irq);
#if !defined(CONFIG_IA64)	/* irq_stat is part of struct cpuinfo_ia64 */
EXPORT_SYMBOL(irq_stat);
#endif

/* waitqueue handling */
EXPORT_SYMBOL(add_wait_queue);
EXPORT_SYMBOL(add_wait_queue_exclusive);
EXPORT_SYMBOL(remove_wait_queue);

/* completion handling */
EXPORT_SYMBOL(wait_for_completion);
EXPORT_SYMBOL(complete);

/* The notion of irq probe/assignment is foreign to S/390 */

#if !defined(CONFIG_ARCH_S390)
EXPORT_SYMBOL(probe_irq_on);
EXPORT_SYMBOL(probe_irq_off);
#endif

#ifdef CONFIG_SMP
EXPORT_SYMBOL(del_timer_sync);
#endif
EXPORT_SYMBOL(mod_timer);
EXPORT_SYMBOL(tq_timer);
EXPORT_SYMBOL(tq_immediate);

#ifdef CONFIG_SMP
/* Various random spinlocks we want to export */
EXPORT_SYMBOL(tqueue_lock);

/* Big-Reader lock implementation */
EXPORT_SYMBOL(__brlock_array);
#ifndef __BRLOCK_USE_ATOMICS
EXPORT_SYMBOL(__br_write_locks);
#endif
EXPORT_SYMBOL(__br_write_lock);
EXPORT_SYMBOL(__br_write_unlock);
#endif

/* Kiobufs */
EXPORT_SYMBOL(alloc_kiovec);
EXPORT_SYMBOL(free_kiovec);
EXPORT_SYMBOL(expand_kiobuf);

EXPORT_SYMBOL(map_user_kiobuf);
EXPORT_SYMBOL(unmap_kiobuf);
EXPORT_SYMBOL(lock_kiovec);
EXPORT_SYMBOL(unlock_kiovec);
EXPORT_SYMBOL(brw_kiovec);
EXPORT_SYMBOL(kiobuf_wait_for_io);

/* dma handling */
EXPORT_SYMBOL(request_dma);
EXPORT_SYMBOL(free_dma);
EXPORT_SYMBOL(dma_spin_lock);
#ifdef HAVE_DISABLE_HLT
EXPORT_SYMBOL(disable_hlt);
EXPORT_SYMBOL(enable_hlt);
#endif

/* resource handling */
EXPORT_SYMBOL(request_resource);
EXPORT_SYMBOL(release_resource);
EXPORT_SYMBOL(allocate_resource);
EXPORT_SYMBOL(check_resource);
EXPORT_SYMBOL(__request_region);
EXPORT_SYMBOL(__check_region);
EXPORT_SYMBOL(__release_region);
EXPORT_SYMBOL(ioport_resource);
EXPORT_SYMBOL(iomem_resource);

/* process management */
EXPORT_SYMBOL(complete_and_exit);
EXPORT_SYMBOL(__wake_up);
EXPORT_SYMBOL(__wake_up_sync);
EXPORT_SYMBOL(wake_up_process);
EXPORT_SYMBOL(sleep_on);
EXPORT_SYMBOL(sleep_on_timeout);
EXPORT_SYMBOL(interruptible_sleep_on);
EXPORT_SYMBOL(interruptible_sleep_on_timeout);
EXPORT_SYMBOL(schedule);
EXPORT_SYMBOL(schedule_timeout);
#if CONFIG_SMP
EXPORT_SYMBOL(set_cpus_allowed);
#endif
EXPORT_SYMBOL(yield);
EXPORT_SYMBOL(__cond_resched);
EXPORT_SYMBOL(jiffies);
EXPORT_SYMBOL(xtime);
EXPORT_SYMBOL(do_gettimeofday);
EXPORT_SYMBOL(do_settimeofday);

#if !defined(__ia64__)
EXPORT_SYMBOL(loops_per_jiffy);
#endif

EXPORT_SYMBOL(kstat);
EXPORT_SYMBOL(nr_running);

/* misc */
EXPORT_SYMBOL(panic);
EXPORT_SYMBOL(panic_notifier_list);
EXPORT_SYMBOL(panic_timeout);
EXPORT_SYMBOL(__out_of_line_bug);
EXPORT_SYMBOL(sprintf);
EXPORT_SYMBOL(snprintf);
EXPORT_SYMBOL(sscanf);
EXPORT_SYMBOL(vsprintf);
EXPORT_SYMBOL(vsnprintf);
EXPORT_SYMBOL(vsscanf);
EXPORT_SYMBOL(kdevname);
EXPORT_SYMBOL(bdevname);
EXPORT_SYMBOL(cdevname);
EXPORT_SYMBOL(simple_strtol);
EXPORT_SYMBOL(simple_strtoul);
EXPORT_SYMBOL(simple_strtoull);
EXPORT_SYMBOL(system_utsname);	/* UTS data */
EXPORT_SYMBOL(uts_sem);		/* UTS semaphore */
#ifndef __mips__
EXPORT_SYMBOL(sys_call_table);
#endif
EXPORT_SYMBOL(machine_restart);
EXPORT_SYMBOL(machine_halt);
EXPORT_SYMBOL(machine_power_off);
EXPORT_SYMBOL(_ctype);
EXPORT_SYMBOL(secure_tcp_sequence_number);
EXPORT_SYMBOL(get_random_bytes);
EXPORT_SYMBOL(securebits);
EXPORT_SYMBOL(cap_bset);
EXPORT_SYMBOL(reparent_to_init);
EXPORT_SYMBOL(daemonize);
EXPORT_SYMBOL(csum_partial); /* for networking and md */
EXPORT_SYMBOL(seq_escape);
EXPORT_SYMBOL(seq_printf);
EXPORT_SYMBOL(seq_open);
EXPORT_SYMBOL(seq_release);
EXPORT_SYMBOL(seq_read);
EXPORT_SYMBOL(seq_lseek);
EXPORT_SYMBOL(single_open);
EXPORT_SYMBOL(single_release);
EXPORT_SYMBOL(seq_release_private);

/* Program loader interfaces */
EXPORT_SYMBOL(setup_arg_pages);
EXPORT_SYMBOL(copy_strings_kernel);
EXPORT_SYMBOL(do_execve);
EXPORT_SYMBOL(flush_old_exec);
EXPORT_SYMBOL(kernel_read);
EXPORT_SYMBOL(open_exec);

/* Miscellaneous access points */
EXPORT_SYMBOL(si_meminfo);

/* Added to make file system as module */
EXPORT_SYMBOL(sys_tz);
EXPORT_SYMBOL(file_fsync);
EXPORT_SYMBOL(fsync_buffers_list);
EXPORT_SYMBOL(clear_inode);
EXPORT_SYMBOL(___strtok);
EXPORT_SYMBOL(init_special_inode);
EXPORT_SYMBOL(read_ahead);
EXPORT_SYMBOL(get_hash_table);
EXPORT_SYMBOL(new_inode);
EXPORT_SYMBOL(insert_inode_hash);
EXPORT_SYMBOL(remove_inode_hash);
EXPORT_SYMBOL(buffer_insert_list);
EXPORT_SYMBOL(make_bad_inode);
EXPORT_SYMBOL(is_bad_inode);
EXPORT_SYMBOL(event);
EXPORT_SYMBOL(brw_page);
EXPORT_SYMBOL(__inode_dir_notify);

#ifdef CONFIG_UID16
EXPORT_SYMBOL(overflowuid);
EXPORT_SYMBOL(overflowgid);
#endif
EXPORT_SYMBOL(fs_overflowuid);
EXPORT_SYMBOL(fs_overflowgid);

/* all busmice */
EXPORT_SYMBOL(fasync_helper);
EXPORT_SYMBOL(kill_fasync);

EXPORT_SYMBOL(disk_name);	/* for md.c */

/* binfmt_aout */
EXPORT_SYMBOL(get_write_access);

/* library functions */
EXPORT_SYMBOL(strnicmp);
EXPORT_SYMBOL(strspn);
EXPORT_SYMBOL(strsep);

#ifdef CONFIG_CRC32
EXPORT_SYMBOL(crc32_le);
EXPORT_SYMBOL(crc32_be);
EXPORT_SYMBOL(bitreverse);
#endif

#ifdef CONFIG_FW_LOADER
EXPORT_SYMBOL(release_firmware);
EXPORT_SYMBOL(request_firmware);
EXPORT_SYMBOL(request_firmware_nowait);
EXPORT_SYMBOL(register_firmware);
#endif

/* software interrupts */
EXPORT_SYMBOL(tasklet_hi_vec);
EXPORT_SYMBOL(tasklet_vec);
EXPORT_SYMBOL(bh_task_vec);
EXPORT_SYMBOL(init_bh);
EXPORT_SYMBOL(remove_bh);
EXPORT_SYMBOL(tasklet_init);
EXPORT_SYMBOL(tasklet_kill);
EXPORT_SYMBOL(__run_task_queue);
EXPORT_SYMBOL(do_softirq);
EXPORT_SYMBOL(raise_softirq);
EXPORT_SYMBOL(cpu_raise_softirq);
EXPORT_SYMBOL(__tasklet_schedule);
EXPORT_SYMBOL(__tasklet_hi_schedule);

/* init task, for moving kthread roots - ought to export a function ?? */

EXPORT_SYMBOL(init_task_union);

EXPORT_SYMBOL(tasklist_lock);
EXPORT_SYMBOL(pidhash);
EXPORT_SYMBOL(unshare_files);

/* debug */
EXPORT_SYMBOL(dump_stack);

/* To match ksyms with System.map */
extern const char _end[];
EXPORT_SYMBOL(_end);
