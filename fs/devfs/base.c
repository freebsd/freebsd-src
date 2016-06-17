/*  devfs (Device FileSystem) driver.

    Copyright (C) 1998-2002  Richard Gooch

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    Richard Gooch may be reached by email at  rgooch@atnf.csiro.au
    The postal address is:
      Richard Gooch, c/o ATNF, P. O. Box 76, Epping, N.S.W., 2121, Australia.

    ChangeLog

    19980110   Richard Gooch <rgooch@atnf.csiro.au>
               Original version.
  v0.1
    19980111   Richard Gooch <rgooch@atnf.csiro.au>
               Created per-fs inode table rather than using inode->u.generic_ip
  v0.2
    19980111   Richard Gooch <rgooch@atnf.csiro.au>
               Created .epoch inode which has a ctime of 0.
	       Fixed loss of named pipes when dentries lost.
	       Fixed loss of inode data when devfs_register() follows mknod().
  v0.3
    19980111   Richard Gooch <rgooch@atnf.csiro.au>
               Fix for when compiling with CONFIG_KERNELD.
    19980112   Richard Gooch <rgooch@atnf.csiro.au>
               Fix for readdir() which sometimes didn't show entries.
	       Added <<tolerant>> option to <devfs_register>.
  v0.4
    19980113   Richard Gooch <rgooch@atnf.csiro.au>
               Created <devfs_fill_file> function.
  v0.5
    19980115   Richard Gooch <rgooch@atnf.csiro.au>
               Added subdirectory support. Major restructuring.
    19980116   Richard Gooch <rgooch@atnf.csiro.au>
               Fixed <find_by_dev> to not search major=0,minor=0.
	       Added symlink support.
  v0.6
    19980120   Richard Gooch <rgooch@atnf.csiro.au>
               Created <devfs_mk_dir> function and support directory unregister
    19980120   Richard Gooch <rgooch@atnf.csiro.au>
               Auto-ownership uses real uid/gid rather than effective uid/gid.
  v0.7
    19980121   Richard Gooch <rgooch@atnf.csiro.au>
               Supported creation of sockets.
  v0.8
    19980122   Richard Gooch <rgooch@atnf.csiro.au>
               Added DEVFS_FL_HIDE_UNREG flag.
	       Interface change to <devfs_mk_symlink>.
               Created <devfs_symlink> to support symlink(2).
  v0.9
    19980123   Richard Gooch <rgooch@atnf.csiro.au>
               Added check to <devfs_fill_file> to check inode is in devfs.
	       Added optional traversal of symlinks.
  v0.10
    19980124   Richard Gooch <rgooch@atnf.csiro.au>
               Created <devfs_get_flags> and <devfs_set_flags>.
  v0.11
    19980125   C. Scott Ananian <cananian@alumni.princeton.edu>
               Created <devfs_find_handle>.
    19980125   Richard Gooch <rgooch@atnf.csiro.au>
               Allow removal of symlinks.
  v0.12
    19980125   Richard Gooch <rgooch@atnf.csiro.au>
               Created <devfs_set_symlink_destination>.
    19980126   Richard Gooch <rgooch@atnf.csiro.au>
               Moved DEVFS_SUPER_MAGIC into header file.
	       Added DEVFS_FL_HIDE flag.
	       Created <devfs_get_maj_min>.
	       Created <devfs_get_handle_from_inode>.
	       Fixed minor bug in <find_by_dev>.
    19980127   Richard Gooch <rgooch@atnf.csiro.au>
	       Changed interface to <find_by_dev>, <find_entry>,
	       <devfs_unregister>, <devfs_fill_file> and <devfs_find_handle>.
	       Fixed inode times when symlink created with symlink(2).
  v0.13
    19980129   C. Scott Ananian <cananian@alumni.princeton.edu>
               Exported <devfs_set_symlink_destination>, <devfs_get_maj_min>
	       and <devfs_get_handle_from_inode>.
    19980129   Richard Gooch <rgooch@atnf.csiro.au>
	       Created <devfs_unlink> to support unlink(2).
  v0.14
    19980129   Richard Gooch <rgooch@atnf.csiro.au>
	       Fixed kerneld support for entries in devfs subdirectories.
    19980130   Richard Gooch <rgooch@atnf.csiro.au>
	       Bugfixes in <call_kerneld>.
  v0.15
    19980207   Richard Gooch <rgooch@atnf.csiro.au>
	       Call kerneld when looking up unregistered entries.
  v0.16
    19980326   Richard Gooch <rgooch@atnf.csiro.au>
	       Modified interface to <devfs_find_handle> for symlink traversal.
  v0.17
    19980331   Richard Gooch <rgooch@atnf.csiro.au>
	       Fixed persistence bug with device numbers for manually created
	       device files.
	       Fixed problem with recreating symlinks with different content.
  v0.18
    19980401   Richard Gooch <rgooch@atnf.csiro.au>
	       Changed to CONFIG_KMOD.
	       Hide entries which are manually unlinked.
	       Always invalidate devfs dentry cache when registering entries.
	       Created <devfs_rmdir> to support rmdir(2).
	       Ensure directories created by <devfs_mk_dir> are visible.
  v0.19
    19980402   Richard Gooch <rgooch@atnf.csiro.au>
	       Invalidate devfs dentry cache when making directories.
	       Invalidate devfs dentry cache when removing entries.
	       Fixed persistence bug with fifos.
  v0.20
    19980421   Richard Gooch <rgooch@atnf.csiro.au>
	       Print process command when debugging kerneld/kmod.
	       Added debugging for register/unregister/change operations.
    19980422   Richard Gooch <rgooch@atnf.csiro.au>
	       Added "devfs=" boot options.
  v0.21
    19980426   Richard Gooch <rgooch@atnf.csiro.au>
	       No longer lock/unlock superblock in <devfs_put_super>.
	       Drop negative dentries when they are released.
	       Manage dcache more efficiently.
  v0.22
    19980427   Richard Gooch <rgooch@atnf.csiro.au>
	       Added DEVFS_FL_AUTO_DEVNUM flag.
  v0.23
    19980430   Richard Gooch <rgooch@atnf.csiro.au>
	       No longer set unnecessary methods.
  v0.24
    19980504   Richard Gooch <rgooch@atnf.csiro.au>
	       Added PID display to <call_kerneld> debugging message.
	       Added "after" debugging message to <call_kerneld>.
    19980519   Richard Gooch <rgooch@atnf.csiro.au>
	       Added "diread" and "diwrite" boot options.
    19980520   Richard Gooch <rgooch@atnf.csiro.au>
	       Fixed persistence problem with permissions.
  v0.25
    19980602   Richard Gooch <rgooch@atnf.csiro.au>
	       Support legacy device nodes.
	       Fixed bug where recreated inodes were hidden.
  v0.26
    19980602   Richard Gooch <rgooch@atnf.csiro.au>
	       Improved debugging in <get_vfs_inode>.
    19980607   Richard Gooch <rgooch@atnf.csiro.au>
	       No longer free old dentries in <devfs_mk_dir>.
	       Free all dentries for a given entry when deleting inodes.
  v0.27
    19980627   Richard Gooch <rgooch@atnf.csiro.au>
	       Limit auto-device numbering to majors 128 to 239.
  v0.28
    19980629   Richard Gooch <rgooch@atnf.csiro.au>
	       Fixed inode times persistence problem.
  v0.29
    19980704   Richard Gooch <rgooch@atnf.csiro.au>
	       Fixed spelling in <devfs_readlink> debug.
	       Fixed bug in <devfs_setup> parsing "dilookup".
  v0.30
    19980705   Richard Gooch <rgooch@atnf.csiro.au>
	       Fixed devfs inode leak when manually recreating inodes.
	       Fixed permission persistence problem when recreating inodes.
  v0.31
    19980727   Richard Gooch <rgooch@atnf.csiro.au>
	       Removed harmless "unused variable" compiler warning.
	       Fixed modes for manually recreated device nodes.
  v0.32
    19980728   Richard Gooch <rgooch@atnf.csiro.au>
	       Added NULL devfs inode warning in <devfs_read_inode>.
	       Force all inode nlink values to 1.
  v0.33
    19980730   Richard Gooch <rgooch@atnf.csiro.au>
	       Added "dimknod" boot option.
	       Set inode nlink to 0 when freeing dentries.
	       Fixed modes for manually recreated symlinks.
  v0.34
    19980802   Richard Gooch <rgooch@atnf.csiro.au>
	       Fixed bugs in recreated directories and symlinks.
  v0.35
    19980806   Richard Gooch <rgooch@atnf.csiro.au>
	       Fixed bugs in recreated device nodes.
    19980807   Richard Gooch <rgooch@atnf.csiro.au>
	       Fixed bug in currently unused <devfs_get_handle_from_inode>.
	       Defined new <devfs_handle_t> type.
	       Improved debugging when getting entries.
	       Fixed bug where directories could be emptied.
  v0.36
    19980809   Richard Gooch <rgooch@atnf.csiro.au>
	       Replaced dummy .epoch inode with .devfsd character device.
    19980810   Richard Gooch <rgooch@atnf.csiro.au>
	       Implemented devfsd protocol revision 0.
  v0.37
    19980819   Richard Gooch <rgooch@atnf.csiro.au>
	       Added soothing message to warning in <devfs_d_iput>.
  v0.38
    19980829   Richard Gooch <rgooch@atnf.csiro.au>
	       Use GCC extensions for structure initialisations.
	       Implemented async open notification.
	       Incremented devfsd protocol revision to 1.
  v0.39
    19980908   Richard Gooch <rgooch@atnf.csiro.au>
	       Moved async open notification to end of <devfs_open>.
  v0.40
    19980910   Richard Gooch <rgooch@atnf.csiro.au>
	       Prepended "/dev/" to module load request.
	       Renamed <call_kerneld> to <call_kmod>.
  v0.41
    19980910   Richard Gooch <rgooch@atnf.csiro.au>
	       Fixed typo "AYSNC" -> "ASYNC".
  v0.42
    19980910   Richard Gooch <rgooch@atnf.csiro.au>
	       Added open flag for files.
  v0.43
    19980927   Richard Gooch <rgooch@atnf.csiro.au>
	       Set i_blocks=0 and i_blksize=1024 in <devfs_read_inode>.
  v0.44
    19981005   Richard Gooch <rgooch@atnf.csiro.au>
	       Added test for empty <<name>> in <devfs_find_handle>.
	       Renamed <generate_path> to <devfs_generate_path> and published.
  v0.45
    19981006   Richard Gooch <rgooch@atnf.csiro.au>
	       Created <devfs_get_fops>.
  v0.46
    19981007   Richard Gooch <rgooch@atnf.csiro.au>
	       Limit auto-device numbering to majors 144 to 239.
  v0.47
    19981010   Richard Gooch <rgooch@atnf.csiro.au>
	       Updated <devfs_follow_link> for VFS change in 2.1.125.
  v0.48
    19981022   Richard Gooch <rgooch@atnf.csiro.au>
	       Created DEVFS_ FL_COMPAT flag.
  v0.49
    19981023   Richard Gooch <rgooch@atnf.csiro.au>
	       Created "nocompat" boot option.
  v0.50
    19981025   Richard Gooch <rgooch@atnf.csiro.au>
	       Replaced "mount" boot option with "nomount".
  v0.51
    19981110   Richard Gooch <rgooch@atnf.csiro.au>
	       Created "only" boot option.
  v0.52
    19981112   Richard Gooch <rgooch@atnf.csiro.au>
	       Added DEVFS_FL_REMOVABLE flag.
  v0.53
    19981114   Richard Gooch <rgooch@atnf.csiro.au>
	       Only call <scan_dir_for_removable> on first call to
	       <devfs_readdir>.
  v0.54
    19981205   Richard Gooch <rgooch@atnf.csiro.au>
	       Updated <devfs_rmdir> for VFS change in 2.1.131.
  v0.55
    19981218   Richard Gooch <rgooch@atnf.csiro.au>
	       Created <devfs_mk_compat>.
    19981220   Richard Gooch <rgooch@atnf.csiro.au>
	       Check for partitions on removable media in <devfs_lookup>.
  v0.56
    19990118   Richard Gooch <rgooch@atnf.csiro.au>
	       Added support for registering regular files.
	       Created <devfs_set_file_size>.
	       Update devfs inodes from entries if not changed through FS.
  v0.57
    19990124   Richard Gooch <rgooch@atnf.csiro.au>
	       Fixed <devfs_fill_file> to only initialise temporary inodes.
	       Trap for NULL fops in <devfs_register>.
	       Return -ENODEV in <devfs_fill_file> for non-driver inodes.
  v0.58
    19990126   Richard Gooch <rgooch@atnf.csiro.au>
	       Switched from PATH_MAX to DEVFS_PATHLEN.
  v0.59
    19990127   Richard Gooch <rgooch@atnf.csiro.au>
	       Created "nottycompat" boot option.
  v0.60
    19990318   Richard Gooch <rgooch@atnf.csiro.au>
	       Fixed <devfsd_read> to not overrun event buffer.
  v0.61
    19990329   Richard Gooch <rgooch@atnf.csiro.au>
	       Created <devfs_auto_unregister>.
  v0.62
    19990330   Richard Gooch <rgooch@atnf.csiro.au>
	       Don't return unregistred entries in <devfs_find_handle>.
	       Panic in <devfs_unregister> if entry unregistered.
    19990401   Richard Gooch <rgooch@atnf.csiro.au>
	       Don't panic in <devfs_auto_unregister> for duplicates.
  v0.63
    19990402   Richard Gooch <rgooch@atnf.csiro.au>
	       Don't unregister already unregistered entries in <unregister>.
  v0.64
    19990510   Richard Gooch <rgooch@atnf.csiro.au>
	       Disable warning messages when unable to read partition table for
	       removable media.
  v0.65
    19990512   Richard Gooch <rgooch@atnf.csiro.au>
	       Updated <devfs_lookup> for VFS change in 2.3.1-pre1.
	       Created "oops-on-panic" boot option.
	       Improved debugging in <devfs_register> and <devfs_unregister>.
  v0.66
    19990519   Richard Gooch <rgooch@atnf.csiro.au>
	       Added documentation for some functions.
    19990525   Richard Gooch <rgooch@atnf.csiro.au>
	       Removed "oops-on-panic" boot option: now always Oops.
  v0.67
    19990531   Richard Gooch <rgooch@atnf.csiro.au>
	       Improved debugging in <devfs_register>.
  v0.68
    19990604   Richard Gooch <rgooch@atnf.csiro.au>
	       Added "diunlink" and "nokmod" boot options.
	       Removed superfluous warning message in <devfs_d_iput>.
  v0.69
    19990611   Richard Gooch <rgooch@atnf.csiro.au>
	       Took account of change to <d_alloc_root>.
  v0.70
    19990614   Richard Gooch <rgooch@atnf.csiro.au>
	       Created separate event queue for each mounted devfs.
	       Removed <devfs_invalidate_dcache>.
	       Created new ioctl()s.
	       Incremented devfsd protocol revision to 3.
	       Fixed bug when re-creating directories: contents were lost.
	       Block access to inodes until devfsd updates permissions.
    19990615   Richard Gooch <rgooch@atnf.csiro.au>
	       Support 2.2.x kernels.
  v0.71
    19990623   Richard Gooch <rgooch@atnf.csiro.au>
	       Switched to sending process uid/gid to devfsd.
	       Renamed <call_kmod> to <try_modload>.
	       Added DEVFSD_NOTIFY_LOOKUP event.
    19990624   Richard Gooch <rgooch@atnf.csiro.au>
	       Added DEVFSD_NOTIFY_CHANGE event.
	       Incremented devfsd protocol revision to 4.
  v0.72
    19990713   Richard Gooch <rgooch@atnf.csiro.au>
	       Return EISDIR rather than EINVAL for read(2) on directories.
  v0.73
    19990809   Richard Gooch <rgooch@atnf.csiro.au>
	       Changed <devfs_setup> to new __init scheme.
  v0.74
    19990901   Richard Gooch <rgooch@atnf.csiro.au>
	       Changed remaining function declarations to new __init scheme.
  v0.75
    19991013   Richard Gooch <rgooch@atnf.csiro.au>
	       Created <devfs_get_info>, <devfs_set_info>,
	       <devfs_get_first_child> and <devfs_get_next_sibling>.
	       Added <<dir>> parameter to <devfs_register>, <devfs_mk_compat>,
	       <devfs_mk_dir> and <devfs_find_handle>.
	       Work sponsored by SGI.
  v0.76
    19991017   Richard Gooch <rgooch@atnf.csiro.au>
	       Allow multiple unregistrations.
	       Work sponsored by SGI.
  v0.77
    19991026   Richard Gooch <rgooch@atnf.csiro.au>
	       Added major and minor number to devfsd protocol.
	       Incremented devfsd protocol revision to 5.
	       Work sponsored by SGI.
  v0.78
    19991030   Richard Gooch <rgooch@atnf.csiro.au>
	       Support info pointer for all devfs entry types.
	       Added <<info>> parameter to <devfs_mk_dir> and
	       <devfs_mk_symlink>.
	       Work sponsored by SGI.
  v0.79
    19991031   Richard Gooch <rgooch@atnf.csiro.au>
	       Support "../" when searching devfs namespace.
	       Work sponsored by SGI.
  v0.80
    19991101   Richard Gooch <rgooch@atnf.csiro.au>
	       Created <devfs_get_unregister_slave>.
	       Work sponsored by SGI.
  v0.81
    19991103   Richard Gooch <rgooch@atnf.csiro.au>
	       Exported <devfs_get_parent>.
	       Work sponsored by SGI.
  v0.82
    19991104   Richard Gooch <rgooch@atnf.csiro.au>
               Removed unused <devfs_set_symlink_destination>.
    19991105   Richard Gooch <rgooch@atnf.csiro.au>
               Do not hide entries from devfsd or children.
	       Removed DEVFS_ FL_TTY_COMPAT flag.
	       Removed "nottycompat" boot option.
	       Removed <devfs_mk_compat>.
	       Work sponsored by SGI.
  v0.83
    19991107   Richard Gooch <rgooch@atnf.csiro.au>
	       Added DEVFS_FL_WAIT flag.
	       Work sponsored by SGI.
  v0.84
    19991107   Richard Gooch <rgooch@atnf.csiro.au>
	       Support new "disc" naming scheme in <get_removable_partition>.
	       Allow NULL fops in <devfs_register>.
	       Work sponsored by SGI.
  v0.85
    19991110   Richard Gooch <rgooch@atnf.csiro.au>
	       Fall back to major table if NULL fops given to <devfs_register>.
	       Work sponsored by SGI.
  v0.86
    19991204   Richard Gooch <rgooch@atnf.csiro.au>
	       Support fifos when unregistering.
	       Work sponsored by SGI.
  v0.87
    19991209   Richard Gooch <rgooch@atnf.csiro.au>
	       Removed obsolete DEVFS_ FL_COMPAT and DEVFS_ FL_TOLERANT flags.
	       Work sponsored by SGI.
  v0.88
    19991214   Richard Gooch <rgooch@atnf.csiro.au>
	       Removed kmod support.
	       Work sponsored by SGI.
  v0.89
    19991216   Richard Gooch <rgooch@atnf.csiro.au>
	       Improved debugging in <get_vfs_inode>.
	       Ensure dentries created by devfsd will be cleaned up.
	       Work sponsored by SGI.
  v0.90
    19991223   Richard Gooch <rgooch@atnf.csiro.au>
	       Created <devfs_get_name>.
	       Work sponsored by SGI.
  v0.91
    20000203   Richard Gooch <rgooch@atnf.csiro.au>
	       Ported to kernel 2.3.42.
	       Removed <devfs_fill_file>.
	       Work sponsored by SGI.
  v0.92
    20000306   Richard Gooch <rgooch@atnf.csiro.au>
	       Added DEVFS_ FL_NO_PERSISTENCE flag.
	       Removed unnecessary call to <update_devfs_inode_from_entry> in
	       <devfs_readdir>.
	       Work sponsored by SGI.
  v0.93
    20000413   Richard Gooch <rgooch@atnf.csiro.au>
	       Set inode->i_size to correct size for symlinks.
    20000414   Richard Gooch <rgooch@atnf.csiro.au>
	       Only give lookup() method to directories to comply with new VFS
	       assumptions.
	       Work sponsored by SGI.
    20000415   Richard Gooch <rgooch@atnf.csiro.au>
	       Remove unnecessary tests in symlink methods.
	       Don't kill existing block ops in <devfs_read_inode>.
	       Work sponsored by SGI.
  v0.94
    20000424   Richard Gooch <rgooch@atnf.csiro.au>
	       Don't create missing directories in <devfs_find_handle>.
	       Work sponsored by SGI.
  v0.95
    20000430   Richard Gooch <rgooch@atnf.csiro.au>
	       Added CONFIG_DEVFS_MOUNT.
	       Work sponsored by SGI.
  v0.96
    20000608   Richard Gooch <rgooch@atnf.csiro.au>
	       Disabled multi-mount capability (use VFS bindings instead).
	       Work sponsored by SGI.
  v0.97
    20000610   Richard Gooch <rgooch@atnf.csiro.au>
	       Switched to FS_SINGLE to disable multi-mounts.
    20000612   Richard Gooch <rgooch@atnf.csiro.au>
	       Removed module support.
	       Removed multi-mount code.
	       Removed compatibility macros: VFS has changed too much.
	       Work sponsored by SGI.
  v0.98
    20000614   Richard Gooch <rgooch@atnf.csiro.au>
	       Merged devfs inode into devfs entry.
	       Work sponsored by SGI.
  v0.99
    20000619   Richard Gooch <rgooch@atnf.csiro.au>
	       Removed dead code in <devfs_register> which used to call
	       <free_dentries>.
	       Work sponsored by SGI.
  v0.100
    20000621   Richard Gooch <rgooch@atnf.csiro.au>
	       Changed interface to <devfs_register>.
	       Work sponsored by SGI.
  v0.101
    20000622   Richard Gooch <rgooch@atnf.csiro.au>
	       Simplified interface to <devfs_mk_symlink> and <devfs_mk_dir>.
	       Simplified interface to <devfs_find_handle>.
	       Work sponsored by SGI.
  v0.102
    20010519   Richard Gooch <rgooch@atnf.csiro.au>
	       Ensure <devfs_generate_path> terminates string for root entry.
	       Exported <devfs_get_name> to modules.
    20010520   Richard Gooch <rgooch@atnf.csiro.au>
	       Make <devfs_mk_symlink> send events to devfsd.
	       Cleaned up option processing in <devfs_setup>.
    20010521   Richard Gooch <rgooch@atnf.csiro.au>
	       Fixed bugs in handling symlinks: could leak or cause Oops.
    20010522   Richard Gooch <rgooch@atnf.csiro.au>
	       Cleaned up directory handling by separating fops.
  v0.103
    20010601   Richard Gooch <rgooch@atnf.csiro.au>
	       Fixed handling of inverted options in <devfs_setup>.
  v0.104
    20010604   Richard Gooch <rgooch@atnf.csiro.au>
	       Adjusted <try_modload> to account for <devfs_generate_path> fix.
  v0.105
    20010617   Richard Gooch <rgooch@atnf.csiro.au>
	       Answered question posed by Al Viro and removed his comments.
	       Moved setting of registered flag after other fields are changed.
	       Fixed race between <devfsd_close> and <devfsd_notify_one>.
	       Global VFS changes added bogus BKL to <devfsd_close>: removed.
	       Widened locking in <devfs_readlink> and <devfs_follow_link>.
	       Replaced <devfsd_read> stack usage with <devfsd_ioctl> kmalloc.
	       Simplified locking in <devfsd_ioctl> and fixed memory leak.
  v0.106
    20010709   Richard Gooch <rgooch@atnf.csiro.au>
	       Removed broken devnum allocation and use <devfs_alloc_devnum>.
	       Fixed old devnum leak by calling new <devfs_dealloc_devnum>.
  v0.107
    20010712   Richard Gooch <rgooch@atnf.csiro.au>
	       Fixed bug in <devfs_setup> which could hang boot process.
  v0.108
    20010730   Richard Gooch <rgooch@atnf.csiro.au>
	       Added DEVFSD_NOTIFY_DELETE event.
    20010801   Richard Gooch <rgooch@atnf.csiro.au>
	       Removed #include <asm/segment.h>.
  v0.109
    20010807   Richard Gooch <rgooch@atnf.csiro.au>
	       Fixed inode table races by removing it and using
	       inode->u.generic_ip instead.
	       Moved <devfs_read_inode> into <get_vfs_inode>.
	       Moved <devfs_write_inode> into <devfs_notify_change>.
  v0.110
    20010808   Richard Gooch <rgooch@atnf.csiro.au>
	       Fixed race in <devfs_do_symlink> for uni-processor.
  v0.111
    20010818   Richard Gooch <rgooch@atnf.csiro.au>
	       Removed remnant of multi-mount support in <devfs_mknod>.
               Removed unused DEVFS_FL_SHOW_UNREG flag.
  v0.112
    20010820   Richard Gooch <rgooch@atnf.csiro.au>
	       Removed nlink field from struct devfs_inode.
  v0.113
    20010823   Richard Gooch <rgooch@atnf.csiro.au>
	       Replaced BKL with global rwsem to protect symlink data (quick
	       and dirty hack).
  v0.114
    20010827   Richard Gooch <rgooch@atnf.csiro.au>
	       Replaced global rwsem for symlink with per-link refcount.
  v0.115
    20010919   Richard Gooch <rgooch@atnf.csiro.au>
	       Set inode->i_mapping->a_ops for block nodes in <get_vfs_inode>.
  v0.116
    20011008   Richard Gooch <rgooch@atnf.csiro.au>
	       Fixed overrun in <devfs_link> by removing function (not needed).
    20011009   Richard Gooch <rgooch@atnf.csiro.au>
	       Fixed buffer underrun in <try_modload>.
    20011029   Richard Gooch <rgooch@atnf.csiro.au>
	       Fixed race in <devfsd_ioctl> when setting event mask.
    20011114   Richard Gooch <rgooch@atnf.csiro.au>
	       First release of new locking code.
  v1.0
    20011117   Richard Gooch <rgooch@atnf.csiro.au>
	       Discard temporary buffer, now use "%s" for dentry names.
    20011118   Richard Gooch <rgooch@atnf.csiro.au>
	       Don't generate path in <try_modload>: use fake entry instead.
	       Use "existing" directory in <_devfs_make_parent_for_leaf>.
    20011122   Richard Gooch <rgooch@atnf.csiro.au>
	       Use slab cache rather than fixed buffer for devfsd events.
  v1.1
    20011125   Richard Gooch <rgooch@atnf.csiro.au>
	       Send DEVFSD_NOTIFY_REGISTERED events in <devfs_mk_dir>.
    20011127   Richard Gooch <rgooch@atnf.csiro.au>
	       Fixed locking bug in <devfs_d_revalidate_wait> due to typo.
	       Do not send CREATE, CHANGE, ASYNC_OPEN or DELETE events from
	       devfsd or children.
  v1.2
    20011202   Richard Gooch <rgooch@atnf.csiro.au>
	       Fixed bug in <devfsd_read>: was dereferencing freed pointer.
  v1.3
    20011203   Richard Gooch <rgooch@atnf.csiro.au>
	       Fixed bug in <devfsd_close>: was dereferencing freed pointer.
	       Added process group check for devfsd privileges.
  v1.4
    20011204   Richard Gooch <rgooch@atnf.csiro.au>
	       Use SLAB_ATOMIC in <devfsd_notify_de> from <devfs_d_delete>.
  v1.5
    20011211   Richard Gooch <rgooch@atnf.csiro.au>
	       Return old entry in <devfs_mk_dir> for 2.4.x kernels.
    20011212   Richard Gooch <rgooch@atnf.csiro.au>
	       Increment refcount on module in <check_disc_changed>.
    20011215   Richard Gooch <rgooch@atnf.csiro.au>
	       Created <devfs_get_handle> and exported <devfs_put>.
	       Increment refcount on module in <devfs_get_ops>.
	       Created <devfs_put_ops>.
  v1.6
    20011216   Richard Gooch <rgooch@atnf.csiro.au>
	       Added poisoning to <devfs_put>.
	       Improved debugging messages.
  v1.7
    20011221   Richard Gooch <rgooch@atnf.csiro.au>
	       Corrected (made useful) debugging message in <unregister>.
	       Moved <kmem_cache_create> in <mount_devfs_fs> to <init_devfs_fs>
    20011224   Richard Gooch <rgooch@atnf.csiro.au>
	       Added magic number to guard against scribbling drivers.
    20011226   Richard Gooch <rgooch@atnf.csiro.au>
	       Only return old entry in <devfs_mk_dir> if a directory.
	       Defined macros for error and debug messages.
  v1.8
    20020113   Richard Gooch <rgooch@atnf.csiro.au>
	       Fixed (rare, old) race in <devfs_lookup>.
  v1.9
    20020120   Richard Gooch <rgooch@atnf.csiro.au>
	       Fixed deadlock bug in <devfs_d_revalidate_wait>.
	       Tag VFS deletable in <devfs_mk_symlink> if handle ignored.
  v1.10
    20020129   Richard Gooch <rgooch@atnf.csiro.au>
	       Added KERN_* to remaining messages.
	       Cleaned up declaration of <stat_read>.
  v1.11
    20020219   Richard Gooch <rgooch@atnf.csiro.au>
	       Changed <devfs_rmdir> to allow later additions if not yet empty.
  v1.12
    20020514   Richard Gooch <rgooch@atnf.csiro.au>
	       Added BKL to <devfs_open> because drivers still need it.
	       Protected <scan_dir_for_removable> and <get_removable_partition>
	       from changing directory contents.
  v1.12a
    20020721   Richard Gooch <rgooch@atnf.csiro.au>
	       Switched to ISO C structure field initialisers.
	       Switch to set_current_state() and move before add_wait_queue().
    20020722   Richard Gooch <rgooch@atnf.csiro.au>
	       Fixed devfs entry leak in <devfs_readdir> when *readdir fails.
  v1.12b
    20020818   Richard Gooch <rgooch@atnf.csiro.au>
	       Fixed module unload race in <devfs_open>.
  v1.12c
*/
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/timer.h>
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/wait.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/ctype.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/devfs_fs.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/smp_lock.h>
#include <linux/smp.h>
#include <linux/version.h>
#include <linux/rwsem.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/processor.h>
#include <asm/system.h>
#include <asm/pgtable.h>
#include <asm/bitops.h>
#include <asm/atomic.h>

#define DEVFS_VERSION            "1.12c (20020818)"

#define DEVFS_NAME "devfs"

#define FIRST_INODE 1

#define STRING_LENGTH 256
#define FAKE_BLOCK_SIZE 1024
#define POISON_PTR ( *(void **) poison_array )
#define MAGIC_VALUE 0x327db823

#ifndef TRUE
#  define TRUE 1
#  define FALSE 0
#endif

#define MODE_DIR (S_IFDIR | S_IWUSR | S_IRUGO | S_IXUGO)

#define IS_HIDDEN(de) ( (de)->hide && !is_devfsd_or_child(fs_info) )

#define DEBUG_NONE         0x0000000
#define DEBUG_MODULE_LOAD  0x0000001
#define DEBUG_REGISTER     0x0000002
#define DEBUG_UNREGISTER   0x0000004
#define DEBUG_FREE         0x0000008
#define DEBUG_SET_FLAGS    0x0000010
#define DEBUG_S_READ       0x0000100        /*  Break  */
#define DEBUG_I_LOOKUP     0x0001000        /*  Break  */
#define DEBUG_I_CREATE     0x0002000
#define DEBUG_I_GET        0x0004000
#define DEBUG_I_CHANGE     0x0008000
#define DEBUG_I_UNLINK     0x0010000
#define DEBUG_I_RLINK      0x0020000
#define DEBUG_I_FLINK      0x0040000
#define DEBUG_I_MKNOD      0x0080000
#define DEBUG_F_READDIR    0x0100000        /*  Break  */
#define DEBUG_D_DELETE     0x1000000        /*  Break  */
#define DEBUG_D_RELEASE    0x2000000
#define DEBUG_D_IPUT       0x4000000
#define DEBUG_ALL          0xfffffff
#define DEBUG_DISABLED     DEBUG_NONE

#define OPTION_NONE             0x00
#define OPTION_MOUNT            0x01
#define OPTION_ONLY             0x02

#define PRINTK(format, args...) \
   {printk (KERN_ERR "%s" format, __FUNCTION__ , ## args);}

#define OOPS(format, args...) \
   {printk (KERN_CRIT "%s" format, __FUNCTION__ , ## args); \
    printk ("Forcing Oops\n"); \
    BUG();}

#ifdef CONFIG_DEVFS_DEBUG
#  define VERIFY_ENTRY(de) \
   {if ((de) && (de)->magic_number != MAGIC_VALUE) \
        OOPS ("(%p): bad magic value: %x\n", (de), (de)->magic_number);}
#  define WRITE_ENTRY_MAGIC(de,magic) (de)->magic_number = (magic)
#  define DPRINTK(flag, format, args...) \
   {if (devfs_debug & flag) \
	printk (KERN_INFO "%s" format, __FUNCTION__ , ## args);}
#else
#  define VERIFY_ENTRY(de)
#  define WRITE_ENTRY_MAGIC(de,magic)
#  define DPRINTK(flag, format, args...)
#endif


struct directory_type
{
    rwlock_t lock;                   /*  Lock for searching(R)/updating(W)   */
    struct devfs_entry *first;
    struct devfs_entry *last;
    unsigned short num_removable;    /*  Lock for writing but not reading    */
    unsigned char no_more_additions:1;
};

struct file_type
{
    unsigned long size;
};

struct device_type
{
    unsigned short major;
    unsigned short minor;
};

struct fcb_type  /*  File, char, block type  */
{
    void *ops;
    union 
    {
	struct file_type file;
	struct device_type device;
    }
    u;
    unsigned char auto_owner:1;
    unsigned char aopen_notify:1;
    unsigned char removable:1;  /*  Belongs in device_type, but save space   */
    unsigned char open:1;       /*  Not entirely correct                     */
    unsigned char autogen:1;    /*  Belongs in device_type, but save space   */
};

struct symlink_type
{
    unsigned int length;         /*  Not including the NULL-termimator       */
    char *linkname;              /*  This is NULL-terminated                 */
};

struct devfs_inode     /*  This structure is for "persistent" inode storage  */
{
    struct dentry *dentry;
    time_t atime;
    time_t mtime;
    time_t ctime;
    unsigned int ino;            /*  Inode number as seen in the VFS         */
    uid_t uid;
    gid_t gid;
};

struct devfs_entry
{
#ifdef CONFIG_DEVFS_DEBUG
    unsigned int magic_number;
#endif
    void *info;
    atomic_t refcount;           /*  When this drops to zero, it's unused    */
    union 
    {
	struct directory_type dir;
	struct fcb_type fcb;
	struct symlink_type symlink;
	const char *name;        /*  Only used for (mode == 0)               */
    }
    u;
    struct devfs_entry *prev;    /*  Previous entry in the parent directory  */
    struct devfs_entry *next;    /*  Next entry in the parent directory      */
    struct devfs_entry *parent;  /*  The parent directory                    */
    struct devfs_entry *slave;   /*  Another entry to unregister             */
    struct devfs_inode inode;
    umode_t mode;
    unsigned short namelen;      /*  I think 64k+ filenames are a way off... */
    unsigned char hide:1;
    unsigned char vfs_deletable:1;/*  Whether the VFS may delete the entry   */
    char name[1];                /*  This is just a dummy: the allocated array
				     is bigger. This is NULL-terminated      */
};

/*  The root of the device tree  */
static struct devfs_entry *root_entry;

struct devfsd_buf_entry
{
    struct devfs_entry *de;      /*  The name is generated with this         */
    unsigned short type;         /*  The type of event                       */
    umode_t mode;
    uid_t uid;
    gid_t gid;
    struct devfsd_buf_entry *next;
};

struct fs_info                  /*  This structure is for the mounted devfs  */
{
    struct super_block *sb;
    spinlock_t devfsd_buffer_lock;  /*  Lock when inserting/deleting events  */
    struct devfsd_buf_entry *devfsd_first_event;
    struct devfsd_buf_entry *devfsd_last_event;
    volatile int devfsd_sleeping;
    volatile struct task_struct *devfsd_task;
    volatile pid_t devfsd_pgrp;
    volatile struct file *devfsd_file;
    struct devfsd_notify_struct *devfsd_info;
    volatile unsigned long devfsd_event_mask;
    atomic_t devfsd_overrun_count;
    wait_queue_head_t devfsd_wait_queue;      /*  Wake devfsd on input       */
    wait_queue_head_t revalidate_wait_queue;  /*  Wake when devfsd sleeps    */
};

static struct fs_info fs_info = {devfsd_buffer_lock: SPIN_LOCK_UNLOCKED};
static kmem_cache_t *devfsd_buf_cache;
#ifdef CONFIG_DEVFS_DEBUG
static unsigned int devfs_debug_init __initdata = DEBUG_NONE;
static unsigned int devfs_debug = DEBUG_NONE;
static spinlock_t stat_lock = SPIN_LOCK_UNLOCKED;
static unsigned int stat_num_entries;
static unsigned int stat_num_bytes;
#endif
static unsigned char poison_array[8] =
    {0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a};

#ifdef CONFIG_DEVFS_MOUNT
static unsigned int boot_options = OPTION_MOUNT;
#else
static unsigned int boot_options = OPTION_NONE;
#endif

/*  Forward function declarations  */
static devfs_handle_t _devfs_walk_path (struct devfs_entry *dir,
					const char *name, int namelen,
					int traverse_symlink);
static ssize_t devfsd_read (struct file *file, char *buf, size_t len,
			    loff_t *ppos);
static int devfsd_ioctl (struct inode *inode, struct file *file,
			 unsigned int cmd, unsigned long arg);
static int devfsd_close (struct inode *inode, struct file *file);
#ifdef CONFIG_DEVFS_DEBUG
static ssize_t stat_read (struct file *file, char *buf, size_t len,
			  loff_t *ppos);
static struct file_operations stat_fops =
{
    .read    = stat_read,
};
#endif


/*  Devfs daemon file operations  */
static struct file_operations devfsd_fops =
{
    .read    = devfsd_read,
    .ioctl   = devfsd_ioctl,
    .release = devfsd_close,
};


/*  Support functions follow  */


/**
 *	devfs_get - Get a reference to a devfs entry.
 *	@de:  The devfs entry.
 */

static struct devfs_entry *devfs_get (struct devfs_entry *de)
{
    VERIFY_ENTRY (de);
    if (de) atomic_inc (&de->refcount);
    return de;
}   /*  End Function devfs_get  */

/**
 *	devfs_put - Put (release) a reference to a devfs entry.
 *	@de:  The handle to the devfs entry.
 */

void devfs_put (devfs_handle_t de)
{
    if (!de) return;
    VERIFY_ENTRY (de);
    if (de->info == POISON_PTR) OOPS ("(%p): poisoned pointer\n", de);
    if ( !atomic_dec_and_test (&de->refcount) ) return;
    if (de == root_entry) OOPS ("(%p): root entry being freed\n", de);
    DPRINTK (DEBUG_FREE, "(%s): de: %p, parent: %p \"%s\"\n",
	     de->name, de, de->parent,
	     de->parent ? de->parent->name : "no parent");
    if ( S_ISLNK (de->mode) ) kfree (de->u.symlink.linkname);
    if ( ( S_ISCHR (de->mode) || S_ISBLK (de->mode) ) && de->u.fcb.autogen )
    {
	devfs_dealloc_devnum ( S_ISCHR (de->mode) ? DEVFS_SPECIAL_CHR :
			       DEVFS_SPECIAL_BLK,
			       mk_kdev (de->u.fcb.u.device.major,
					de->u.fcb.u.device.minor) );
    }
    WRITE_ENTRY_MAGIC (de, 0);
#ifdef CONFIG_DEVFS_DEBUG
    spin_lock (&stat_lock);
    --stat_num_entries;
    stat_num_bytes -= sizeof *de + de->namelen;
    if ( S_ISLNK (de->mode) ) stat_num_bytes -= de->u.symlink.length + 1;
    spin_unlock (&stat_lock);
#endif
    de->info = POISON_PTR;
    kfree (de);
}   /*  End Function devfs_put  */

/**
 *	_devfs_search_dir - Search for a devfs entry in a directory.
 *	@dir:  The directory to search.
 *	@name:  The name of the entry to search for.
 *	@namelen:  The number of characters in @name.
 *
 *  Search for a devfs entry in a directory and returns a pointer to the entry
 *   on success, else %NULL. The directory must be locked already.
 *   An implicit devfs_get() is performed on the returned entry.
 */

static struct devfs_entry *_devfs_search_dir (struct devfs_entry *dir,
					      const char *name,
					      unsigned int namelen)
{
    struct devfs_entry *curr;

    if ( !S_ISDIR (dir->mode) )
    {
	PRINTK ("(%s): not a directory\n", dir->name);
	return NULL;
    }
    for (curr = dir->u.dir.first; curr != NULL; curr = curr->next)
    {
	if (curr->namelen != namelen) continue;
	if (memcmp (curr->name, name, namelen) == 0) break;
	/*  Not found: try the next one  */
    }
    return devfs_get (curr);
}   /*  End Function _devfs_search_dir  */


/**
 *	_devfs_alloc_entry - Allocate a devfs entry.
 *	@name:  The name of the entry.
 *	@namelen:  The number of characters in @name.
 *
 *  Allocate a devfs entry and returns a pointer to the entry on success, else
 *   %NULL.
 */

static struct devfs_entry *_devfs_alloc_entry (const char *name,
					       unsigned int namelen,
					       umode_t mode)
{
    struct devfs_entry *new;
    static unsigned long inode_counter = FIRST_INODE;
    static spinlock_t counter_lock = SPIN_LOCK_UNLOCKED;

    if ( name && (namelen < 1) ) namelen = strlen (name);
    if ( ( new = kmalloc (sizeof *new + namelen, GFP_KERNEL) ) == NULL )
	return NULL;
    memset (new, 0, sizeof *new + namelen);  /*  Will set '\0' on name  */
    new->mode = mode;
    if ( S_ISDIR (mode) ) rwlock_init (&new->u.dir.lock);
    atomic_set (&new->refcount, 1);
    spin_lock (&counter_lock);
    new->inode.ino = inode_counter++;
    spin_unlock (&counter_lock);
    if (name) memcpy (new->name, name, namelen);
    new->namelen = namelen;
    WRITE_ENTRY_MAGIC (new, MAGIC_VALUE);
#ifdef CONFIG_DEVFS_DEBUG
    spin_lock (&stat_lock);
    ++stat_num_entries;
    stat_num_bytes += sizeof *new + namelen;
    spin_unlock (&stat_lock);
#endif
    return new;
}   /*  End Function _devfs_alloc_entry  */


/**
 *	_devfs_append_entry - Append a devfs entry to a directory's child list.
 *	@dir:  The directory to add to.
 *	@de:  The devfs entry to append.
 *	@removable: If TRUE, increment the count of removable devices for %dir.
 *	@old_de: If an existing entry exists, it will be written here. This may
 *		 be %NULL. An implicit devfs_get() is performed on this entry.
 *
 *  Append a devfs entry to a directory's list of children, checking first to
 *   see if an entry of the same name exists. The directory will be locked.
 *   The value 0 is returned on success, else a negative error code.
 *   On failure, an implicit devfs_put() is performed on %de.
 */

static int _devfs_append_entry (devfs_handle_t dir, devfs_handle_t de,
				int removable, devfs_handle_t *old_de)
{
    int retval;

    if (old_de) *old_de = NULL;
    if ( !S_ISDIR (dir->mode) )
    {
	PRINTK ("(%s): dir: \"%s\" is not a directory\n", de->name, dir->name);
	devfs_put (de);
	return -ENOTDIR;
    }
    write_lock (&dir->u.dir.lock);
    if (dir->u.dir.no_more_additions) retval = -ENOENT;
    else
    {
	struct devfs_entry *old;

	old = _devfs_search_dir (dir, de->name, de->namelen);
	if (old_de) *old_de = old;
	else devfs_put (old);
	if (old == NULL)
	{
	    de->parent = dir;
	    de->prev = dir->u.dir.last;
	    /*  Append to the directory's list of children  */
	    if (dir->u.dir.first == NULL) dir->u.dir.first = de;
	    else dir->u.dir.last->next = de;
	    dir->u.dir.last = de;
	    if (removable) ++dir->u.dir.num_removable;
	    retval = 0;
	}
	else retval = -EEXIST;
    }
    write_unlock (&dir->u.dir.lock);
    if (retval) devfs_put (de);
    return retval;
}   /*  End Function _devfs_append_entry  */


/**
 *	_devfs_get_root_entry - Get the root devfs entry.
 *
 *	Returns the root devfs entry on success, else %NULL.
 */

static struct devfs_entry *_devfs_get_root_entry (void)
{
    kdev_t devnum;
    struct devfs_entry *new;
    static spinlock_t root_lock = SPIN_LOCK_UNLOCKED;

    /*  Always ensure the root is created  */
    if (root_entry) return root_entry;
    if ( ( new = _devfs_alloc_entry (NULL, 0,MODE_DIR) ) == NULL ) return NULL;
    spin_lock (&root_lock);
    if (root_entry)
    {
	spin_unlock (&root_lock);
	devfs_put (new);
	return (root_entry);
    }
    root_entry = new;
    spin_unlock (&root_lock);
    /*  And create the entry for ".devfsd"  */
    if ( ( new = _devfs_alloc_entry (".devfsd", 0, S_IFCHR |S_IRUSR |S_IWUSR) )
	 == NULL ) return NULL;
    devnum = devfs_alloc_devnum (DEVFS_SPECIAL_CHR);
    new->u.fcb.u.device.major = major (devnum);
    new->u.fcb.u.device.minor = minor (devnum);
    new->u.fcb.ops = &devfsd_fops;
    _devfs_append_entry (root_entry, new, FALSE, NULL);
#ifdef CONFIG_DEVFS_DEBUG
    if ( ( new = _devfs_alloc_entry (".stat", 0, S_IFCHR | S_IRUGO | S_IWUGO) )
	 == NULL ) return NULL;
    devnum = devfs_alloc_devnum (DEVFS_SPECIAL_CHR);
    new->u.fcb.u.device.major = major (devnum);
    new->u.fcb.u.device.minor = minor (devnum);
    new->u.fcb.ops = &stat_fops;
    _devfs_append_entry (root_entry, new, FALSE, NULL);
#endif
    return root_entry;
}   /*  End Function _devfs_get_root_entry  */


/**
 *	_devfs_descend - Descend down a tree using the next component name.
 *	@dir:  The directory to search.
 *	@name:  The component name to search for.
 *	@namelen:  The length of %name.
 *	@next_pos:  The position of the next '/' or '\0' is written here.
 *
 *  Descend into a directory, searching for a component. This function forms
 *   the core of a tree-walking algorithm. The directory will be locked.
 *   The devfs entry corresponding to the component is returned. If there is
 *   no matching entry, %NULL is returned.
 *   An implicit devfs_get() is performed on the returned entry.
 */

static struct devfs_entry *_devfs_descend (struct devfs_entry *dir,
					   const char *name, int namelen,
					   int *next_pos)
{
    const char *stop, *ptr;
    struct devfs_entry *entry;

    if ( (namelen >= 3) && (strncmp (name, "../", 3) == 0) )
    {   /*  Special-case going to parent directory  */
	*next_pos = 3;
	return devfs_get (dir->parent);
    }
    stop = name + namelen;
    /*  Search for a possible '/'  */
    for (ptr = name; (ptr < stop) && (*ptr != '/'); ++ptr);
    *next_pos = ptr - name;
    read_lock (&dir->u.dir.lock);
    entry = _devfs_search_dir (dir, name, *next_pos);
    read_unlock (&dir->u.dir.lock);
    return entry;
}   /*  End Function _devfs_descend  */


static devfs_handle_t _devfs_make_parent_for_leaf (struct devfs_entry *dir,
						   const char *name,
						   int namelen, int *leaf_pos)
{
    int next_pos = 0;

    if (dir == NULL) dir = _devfs_get_root_entry ();
    if (dir == NULL) return NULL;
    devfs_get (dir);
    /*  Search for possible trailing component and ignore it  */
    for (--namelen; (namelen > 0) && (name[namelen] != '/'); --namelen);
    *leaf_pos = (name[namelen] == '/') ? (namelen + 1) : 0;
    for (; namelen > 0; name += next_pos, namelen -= next_pos)
    {
	struct devfs_entry *de, *old;

	if ( ( de = _devfs_descend (dir, name, namelen, &next_pos) ) == NULL )
	{
	    de = _devfs_alloc_entry (name, next_pos, MODE_DIR);
	    devfs_get (de);
	    if ( !de || _devfs_append_entry (dir, de, FALSE, &old) )
	    {
		devfs_put (de);
		if ( !old || !S_ISDIR (old->mode) )
		{
		    devfs_put (old);
		    devfs_put (dir);
		    return NULL;
		}
		de = old;  /*  Use the existing directory  */
	    }
	}
	if (de == dir->parent)
	{
	    devfs_put (dir);
	    devfs_put (de);
	    return NULL;
	}
	devfs_put (dir);
	dir = de;
	if (name[next_pos] == '/') ++next_pos;
    }
    return dir;
}   /*  End Function _devfs_make_parent_for_leaf  */


static devfs_handle_t _devfs_prepare_leaf (devfs_handle_t *dir,
					   const char *name, umode_t mode)
{
    int namelen, leaf_pos;
    struct devfs_entry *de;

    namelen = strlen (name);
    if ( ( *dir = _devfs_make_parent_for_leaf (*dir, name, namelen,
					       &leaf_pos) ) == NULL )
    {
	PRINTK ("(%s): could not create parent path\n", name);
	return NULL;
    }
    if ( ( de = _devfs_alloc_entry (name + leaf_pos, namelen - leaf_pos,mode) )
	 == NULL )
    {
	PRINTK ("(%s): could not allocate entry\n", name);
	devfs_put (*dir);
	return NULL;
    }
    return de;
}   /*  End Function _devfs_prepare_leaf  */


static devfs_handle_t _devfs_walk_path (struct devfs_entry *dir,
					const char *name, int namelen,
					int traverse_symlink)
{
    int next_pos = 0;

    if (dir == NULL) dir = _devfs_get_root_entry ();
    if (dir == NULL) return NULL;
    devfs_get (dir);
    for (; namelen > 0; name += next_pos, namelen -= next_pos)
    {
	struct devfs_entry *de, *link;

	if ( ( de = _devfs_descend (dir, name, namelen, &next_pos) ) == NULL )
	{
	    devfs_put (dir);
	    return NULL;
	}
	if (S_ISLNK (de->mode) && traverse_symlink)
	{   /*  Need to follow the link: this is a stack chomper  */
	    link = _devfs_walk_path (dir, de->u.symlink.linkname,
				     de->u.symlink.length, TRUE);
	    devfs_put (de);
	    if (!link)
	    {
		devfs_put (dir);
		return NULL;
	    }
	    de = link;
	}
	devfs_put (dir);
	dir = de;
	if (name[next_pos] == '/') ++next_pos;
    }
    return dir;
}   /*  End Function _devfs_walk_path  */


/**
 *	_devfs_find_by_dev - Find a devfs entry in a directory.
 *	@dir: The directory where to search
 *	@major: The major number to search for.
 *	@minor: The minor number to search for.
 *	@type: The type of special file to search for. This may be either
 *		%DEVFS_SPECIAL_CHR or %DEVFS_SPECIAL_BLK.
 *
 *	Returns the devfs_entry pointer on success, else %NULL. An implicit
 *	devfs_get() is performed.
 */

static struct devfs_entry *_devfs_find_by_dev (struct devfs_entry *dir,
					       unsigned int major,
					       unsigned int minor, char type)
{
    struct devfs_entry *entry, *de;

    devfs_get (dir);
    if (dir == NULL) return NULL;
    if ( !S_ISDIR (dir->mode) )
    {
	PRINTK ("(%p): not a directory\n", dir);
	devfs_put (dir);
	return NULL;
    }
    /*  First search files in this directory  */
    read_lock (&dir->u.dir.lock);
    for (entry = dir->u.dir.first; entry != NULL; entry = entry->next)
    {
	if ( !S_ISCHR (entry->mode) && !S_ISBLK (entry->mode) ) continue;
	if ( S_ISCHR (entry->mode) && (type != DEVFS_SPECIAL_CHR) ) continue;
	if ( S_ISBLK (entry->mode) && (type != DEVFS_SPECIAL_BLK) ) continue;
	if ( (entry->u.fcb.u.device.major == major) &&
	     (entry->u.fcb.u.device.minor == minor) )
	{
	    devfs_get (entry);
	    read_unlock (&dir->u.dir.lock);
	    devfs_put (dir);
	    return entry;
	}
	/*  Not found: try the next one  */
    }
    /*  Now recursively search the subdirectories: this is a stack chomper  */
    for (entry = dir->u.dir.first; entry != NULL; entry = entry->next)
    {
	if ( !S_ISDIR (entry->mode) ) continue;
	de = _devfs_find_by_dev (entry, major, minor, type);
	if (de)
	{
	    read_unlock (&dir->u.dir.lock);
	    devfs_put (dir);
	    return de;
	}
    }
    read_unlock (&dir->u.dir.lock);
    devfs_put (dir);
    return NULL;
}   /*  End Function _devfs_find_by_dev  */


/**
 *	_devfs_find_entry - Find a devfs entry.
 *	@dir: The handle to the parent devfs directory entry. If this is %NULL the
 *		name is relative to the root of the devfs.
 *	@name: The name of the entry. This may be %NULL.
 *	@major: The major number. This is used if lookup by @name fails.
 *	@minor: The minor number. This is used if lookup by @name fails.
 *		NOTE: If @major and @minor are both 0, searching by major and minor
 *		numbers is disabled.
 *	@type: The type of special file to search for. This may be either
 *		%DEVFS_SPECIAL_CHR or %DEVFS_SPECIAL_BLK.
 *	@traverse_symlink: If %TRUE then symbolic links are traversed.
 *
 *	Returns the devfs_entry pointer on success, else %NULL. An implicit
 *	devfs_get() is performed.
 */

static struct devfs_entry *_devfs_find_entry (devfs_handle_t dir,
					      const char *name,
					      unsigned int major,
					      unsigned int minor,
					      char type, int traverse_symlink)
{
    struct devfs_entry *entry;

    if (name != NULL)
    {
	unsigned int namelen = strlen (name);

	if (name[0] == '/')
	{
	    /*  Skip leading pathname component  */
	    if (namelen < 2)
	    {
		PRINTK ("(%s): too short\n", name);
		return NULL;
	    }
	    for (++name, --namelen; (*name != '/') && (namelen > 0);
		 ++name, --namelen);
	    if (namelen < 2)
	    {
		PRINTK ("(%s): too short\n", name);
		return NULL;
	    }
	    ++name;
	    --namelen;
	}
	entry = _devfs_walk_path (dir, name, namelen, traverse_symlink);
	if (entry != NULL) return entry;
    }
    /*  Have to search by major and minor: slow  */
    if ( (major == 0) && (minor == 0) ) return NULL;
    return _devfs_find_by_dev (root_entry, major, minor, type);
}   /*  End Function _devfs_find_entry  */

static struct devfs_entry *get_devfs_entry_from_vfs_inode (struct inode *inode)
{
    if (inode == NULL) return NULL;
    VERIFY_ENTRY ( (struct devfs_entry *) inode->u.generic_ip );
    return inode->u.generic_ip;
}   /*  End Function get_devfs_entry_from_vfs_inode  */


/**
 *	free_dentry - Free the dentry for a device entry and invalidate inode.
 *	@de: The entry.
 *
 *	This must only be called after the entry has been unhooked from it's
 *	 parent directory.
 */

static void free_dentry (struct devfs_entry *de)
{
    struct dentry *dentry = de->inode.dentry;

    if (!dentry) return;
    spin_lock (&dcache_lock);
    dget_locked (dentry);
    spin_unlock (&dcache_lock);
    /*  Forcefully remove the inode  */
    if (dentry->d_inode != NULL) dentry->d_inode->i_nlink = 0;
    d_drop (dentry);
    dput (dentry);
}   /*  End Function free_dentry  */


/**
 *	is_devfsd_or_child - Test if the current process is devfsd or one of its children.
 *	@fs_info: The filesystem information.
 *
 *	Returns %TRUE if devfsd or child, else %FALSE.
 */

static int is_devfsd_or_child (struct fs_info *fs_info)
{
    struct task_struct *p;

    if (current == fs_info->devfsd_task) return (TRUE);
    if (current->pgrp == fs_info->devfsd_pgrp) return (TRUE);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,1)
    for (p = current->p_opptr; p != &init_task; p = p->p_opptr)
    {
	if (p == fs_info->devfsd_task) return (TRUE);
    }
#endif
    return (FALSE);
}   /*  End Function is_devfsd_or_child  */


/**
 *	devfsd_queue_empty - Test if devfsd has work pending in its event queue.
 *	@fs_info: The filesystem information.
 *
 *	Returns %TRUE if the queue is empty, else %FALSE.
 */

static inline int devfsd_queue_empty (struct fs_info *fs_info)
{
    return (fs_info->devfsd_last_event) ? FALSE : TRUE;
}   /*  End Function devfsd_queue_empty  */


/**
 *	wait_for_devfsd_finished - Wait for devfsd to finish processing its event queue.
 *	@fs_info: The filesystem information.
 *
 *	Returns %TRUE if no more waiting will be required, else %FALSE.
 */

static int wait_for_devfsd_finished (struct fs_info *fs_info)
{
    DECLARE_WAITQUEUE (wait, current);

    if (fs_info->devfsd_task == NULL) return (TRUE);
    if (devfsd_queue_empty (fs_info) && fs_info->devfsd_sleeping) return TRUE;
    if ( is_devfsd_or_child (fs_info) ) return (FALSE);
    set_current_state (TASK_UNINTERRUPTIBLE);
    add_wait_queue (&fs_info->revalidate_wait_queue, &wait);
    if (!devfsd_queue_empty (fs_info) || !fs_info->devfsd_sleeping)
	if (fs_info->devfsd_task) schedule ();
    remove_wait_queue (&fs_info->revalidate_wait_queue, &wait);
    __set_current_state (TASK_RUNNING);
    return (TRUE);
}   /*  End Function wait_for_devfsd_finished  */


/**
 *	devfsd_notify_de - Notify the devfsd daemon of a change.
 *	@de: The devfs entry that has changed. This and all parent entries will
 *            have their reference counts incremented if the event was queued.
 *	@type: The type of change.
 *	@mode: The mode of the entry.
 *	@uid: The user ID.
 *	@gid: The group ID.
 *	@fs_info: The filesystem info.
 *
 *	Returns %TRUE if an event was queued and devfsd woken up, else %FALSE.
 */

static int devfsd_notify_de (struct devfs_entry *de,
			     unsigned short type, umode_t mode,
			     uid_t uid, gid_t gid, struct fs_info *fs_info,
			     int atomic)
{
    struct devfsd_buf_entry *entry;
    struct devfs_entry *curr;

    if ( !( fs_info->devfsd_event_mask & (1 << type) ) ) return (FALSE);
    if ( ( entry = kmem_cache_alloc (devfsd_buf_cache,
				     atomic ? SLAB_ATOMIC : SLAB_KERNEL) )
	 == NULL )
    {
	atomic_inc (&fs_info->devfsd_overrun_count);
	return (FALSE);
    }
    for (curr = de; curr != NULL; curr = curr->parent) devfs_get (curr);
    entry->de = de;
    entry->type = type;
    entry->mode = mode;
    entry->uid = uid;
    entry->gid = gid;
    entry->next = NULL;
    spin_lock (&fs_info->devfsd_buffer_lock);
    if (!fs_info->devfsd_first_event) fs_info->devfsd_first_event = entry;
    if (fs_info->devfsd_last_event) fs_info->devfsd_last_event->next = entry;
    fs_info->devfsd_last_event = entry;
    spin_unlock (&fs_info->devfsd_buffer_lock);
    wake_up_interruptible (&fs_info->devfsd_wait_queue);
    return (TRUE);
}   /*  End Function devfsd_notify_de  */


/**
 *	devfsd_notify - Notify the devfsd daemon of a change.
 *	@de: The devfs entry that has changed.
 *	@type: The type of change event.
 *	@wait: If TRUE, the function waits for the daemon to finish processing
 *		the event.
 */

static void devfsd_notify (struct devfs_entry *de,unsigned short type,int wait)
{
    if (devfsd_notify_de (de, type, de->mode, current->euid,
			  current->egid, &fs_info, 0) && wait)
	wait_for_devfsd_finished (&fs_info);
}   /*  End Function devfsd_notify  */


/**
 *	devfs_register - Register a device entry.
 *	@dir: The handle to the parent devfs directory entry. If this is %NULL the
 *		new name is relative to the root of the devfs.
 *	@name: The name of the entry.
 *	@flags: A set of bitwise-ORed flags (DEVFS_FL_*).
 *	@major: The major number. Not needed for regular files.
 *	@minor: The minor number. Not needed for regular files.
 *	@mode: The default file mode.
 *	@ops: The &file_operations or &block_device_operations structure.
 *		This must not be externally deallocated.
 *	@info: An arbitrary pointer which will be written to the @private_data
 *		field of the &file structure passed to the device driver. You can set
 *		this to whatever you like, and change it once the file is opened (the next
 *		file opened will not see this change).
 *
 *	Returns a handle which may later be used in a call to devfs_unregister().
 *	On failure %NULL is returned.
 */

devfs_handle_t devfs_register (devfs_handle_t dir, const char *name,
			       unsigned int flags,
			       unsigned int major, unsigned int minor,
			       umode_t mode, void *ops, void *info)
{
    char devtype = S_ISCHR (mode) ? DEVFS_SPECIAL_CHR : DEVFS_SPECIAL_BLK;
    int err;
    kdev_t devnum = NODEV;
    struct devfs_entry *de;

    if (name == NULL)
    {
	PRINTK ("(): NULL name pointer\n");
	return NULL;
    }
    if (ops == NULL)
    {
	if ( S_ISBLK (mode) ) ops = (void *) get_blkfops (major);
	if (ops == NULL)
	{
	    PRINTK ("(%s): NULL ops pointer\n", name);
	    return NULL;
	}
	PRINTK ("(%s): NULL ops, got %p from major table\n", name, ops);
    }
    if ( S_ISDIR (mode) )
    {
	PRINTK ("(%s): creating directories is not allowed\n", name);
	return NULL;
    }
    if ( S_ISLNK (mode) )
    {
	PRINTK ("(%s): creating symlinks is not allowed\n", name);
	return NULL;
    }
    if ( ( S_ISCHR (mode) || S_ISBLK (mode) ) &&
	 (flags & DEVFS_FL_AUTO_DEVNUM) )
    {
	if ( kdev_none ( devnum = devfs_alloc_devnum (devtype) ) )
	{
	    PRINTK ("(%s): exhausted %s device numbers\n",
		    name, S_ISCHR (mode) ? "char" : "block");
	    return NULL;
	}
	major = major (devnum);
	minor = minor (devnum);
    }
    if ( ( de = _devfs_prepare_leaf (&dir, name, mode) ) == NULL )
    {
	PRINTK ("(%s): could not prepare leaf\n", name);
	if ( !kdev_none (devnum) ) devfs_dealloc_devnum (devtype, devnum);
	return NULL;
    }
    if ( S_ISCHR (mode) || S_ISBLK (mode) )
    {
	de->u.fcb.u.device.major = major;
	de->u.fcb.u.device.minor = minor;
	de->u.fcb.autogen = kdev_none (devnum) ? FALSE : TRUE;
    }
    else if ( !S_ISREG (mode) )
    {
	PRINTK ("(%s): illegal mode: %x\n", name, mode);
	devfs_put (de);
	devfs_put (dir);
	return (NULL);
    }
    de->info = info;
    if (flags & DEVFS_FL_CURRENT_OWNER)
    {
	de->inode.uid = current->uid;
	de->inode.gid = current->gid;
    }
    else
    {
	de->inode.uid = 0;
	de->inode.gid = 0;
    }
    de->u.fcb.ops = ops;
    de->u.fcb.auto_owner = (flags & DEVFS_FL_AUTO_OWNER) ? TRUE : FALSE;
    de->u.fcb.aopen_notify = (flags & DEVFS_FL_AOPEN_NOTIFY) ? TRUE : FALSE;
    de->hide = (flags & DEVFS_FL_HIDE) ? TRUE : FALSE;
    if (flags & DEVFS_FL_REMOVABLE) de->u.fcb.removable = TRUE;
    if ( ( err = _devfs_append_entry (dir, de, de->u.fcb.removable, NULL) )
	 != 0 )
    {
	PRINTK ("(%s): could not append to parent, err: %d\n", name, err);
	devfs_put (dir);
	if ( !kdev_none (devnum) ) devfs_dealloc_devnum (devtype, devnum);
	return NULL;
    }
    DPRINTK (DEBUG_REGISTER, "(%s): de: %p dir: %p \"%s\"  pp: %p\n",
	     name, de, dir, dir->name, dir->parent);
    devfsd_notify (de, DEVFSD_NOTIFY_REGISTERED, flags & DEVFS_FL_WAIT);
    devfs_put (dir);
    return de;
}   /*  End Function devfs_register  */


/**
 *	_devfs_unhook - Unhook a device entry from its parents list
 *	@de: The entry to unhook.
 *
 *	Returns %TRUE if the entry was unhooked, else %FALSE if it was
 *		previously unhooked.
 *	The caller must have a write lock on the parent directory.
 */

static int _devfs_unhook (struct devfs_entry *de)
{
    struct devfs_entry *parent;

    if ( !de || (de->prev == de) ) return FALSE;
    parent = de->parent;
    if (de->prev == NULL) parent->u.dir.first = de->next;
    else de->prev->next = de->next;
    if (de->next == NULL) parent->u.dir.last = de->prev;
    else de->next->prev = de->prev;
    de->prev = de;          /*  Indicate we're unhooked                      */
    de->next = NULL;        /*  Force early termination for <devfs_readdir>  */
    if ( ( S_ISREG (de->mode) || S_ISCHR (de->mode) || S_ISBLK (de->mode) ) &&
	 de->u.fcb.removable )
	--parent->u.dir.num_removable;
    return TRUE;
}   /*  End Function _devfs_unhook  */


/**
 *	_devfs_unregister - Unregister a device entry from it's parent.
 *	@dir: The parent directory.
 *	@de: The entry to unregister.
 *
 *	The caller must have a write lock on the parent directory, which is
 *	unlocked by this function.
 */

static void _devfs_unregister (struct devfs_entry *dir, struct devfs_entry *de)
{
    int unhooked = _devfs_unhook (de);

    write_unlock (&dir->u.dir.lock);
    if (!unhooked) return;
    devfs_get (dir);
    devfs_unregister (de->slave);  /*  Let it handle the locking  */
    devfsd_notify (de, DEVFSD_NOTIFY_UNREGISTERED, 0);
    free_dentry (de);
    devfs_put (dir);
    if ( !S_ISDIR (de->mode) ) return;
    while (TRUE)  /*  Recursively unregister: this is a stack chomper  */
    {
	struct devfs_entry *child;

	write_lock (&de->u.dir.lock);
	de->u.dir.no_more_additions = TRUE;
	child = de->u.dir.first;
	VERIFY_ENTRY (child);
	_devfs_unregister (de, child);
	if (!child) break;
	DPRINTK (DEBUG_UNREGISTER, "(%s): child: %p  refcount: %d\n",
		 child->name, child, atomic_read (&child->refcount) );
	devfs_put (child);
    }
}   /*  End Function _devfs_unregister  */


/**
 *	devfs_unregister - Unregister a device entry.
 *	@de: A handle previously created by devfs_register() or returned from
 *		devfs_get_handle(). If this is %NULL the routine does nothing.
 */

void devfs_unregister (devfs_handle_t de)
{
    VERIFY_ENTRY (de);
    if ( (de == NULL) || (de->parent == NULL) ) return;
    DPRINTK (DEBUG_UNREGISTER, "(%s): de: %p  refcount: %d\n",
	     de->name, de, atomic_read (&de->refcount) );
    write_lock (&de->parent->u.dir.lock);
    _devfs_unregister (de->parent, de);
    devfs_put (de);
}   /*  End Function devfs_unregister  */

static int devfs_do_symlink (devfs_handle_t dir, const char *name,
			     unsigned int flags, const char *link,
			     devfs_handle_t *handle, void *info)
{
    int err;
    unsigned int linklength;
    char *newlink;
    struct devfs_entry *de;

    if (handle != NULL) *handle = NULL;
    if (name == NULL)
    {
	PRINTK ("(): NULL name pointer\n");
	return -EINVAL;
    }
    if (link == NULL)
    {
	PRINTK ("(%s): NULL link pointer\n", name);
	return -EINVAL;
    }
    linklength = strlen (link);
    if ( ( newlink = kmalloc (linklength + 1, GFP_KERNEL) ) == NULL )
	return -ENOMEM;
    memcpy (newlink, link, linklength);
    newlink[linklength] = '\0';
    if ( ( de = _devfs_prepare_leaf (&dir, name, S_IFLNK | S_IRUGO | S_IXUGO) )
	 == NULL )
    {
	PRINTK ("(%s): could not prepare leaf\n", name);
	kfree (newlink);
	return -ENOTDIR;
    }
    de->info = info;
    de->hide = (flags & DEVFS_FL_HIDE) ? TRUE : FALSE;
    de->u.symlink.linkname = newlink;
    de->u.symlink.length = linklength;
    if ( ( err = _devfs_append_entry (dir, de, FALSE, NULL) ) != 0 )
    {
	PRINTK ("(%s): could not append to parent, err: %d\n", name, err);
	devfs_put (dir);
	return err;
    }
    devfs_put (dir);
#ifdef CONFIG_DEVFS_DEBUG
    spin_lock (&stat_lock);
    stat_num_bytes += linklength + 1;
    spin_unlock (&stat_lock);
#endif
    if (handle != NULL) *handle = de;
    return 0;
}   /*  End Function devfs_do_symlink  */


/**
 *	devfs_mk_symlink Create a symbolic link in the devfs namespace.
 *	@dir: The handle to the parent devfs directory entry. If this is %NULL the
 *		new name is relative to the root of the devfs.
 *	@name: The name of the entry.
 *	@flags: A set of bitwise-ORed flags (DEVFS_FL_*).
 *	@link: The destination name.
 *	@handle: The handle to the symlink entry is written here. This may be %NULL.
 *	@info: An arbitrary pointer which will be associated with the entry.
 *
 *	Returns 0 on success, else a negative error code is returned.
 */

int devfs_mk_symlink (devfs_handle_t dir, const char *name, unsigned int flags,
		      const char *link, devfs_handle_t *handle, void *info)
{
    int err;
    devfs_handle_t de;

    if (handle != NULL) *handle = NULL;
    DPRINTK (DEBUG_REGISTER, "(%s)\n", name);
    err = devfs_do_symlink (dir, name, flags, link, &de, info);
    if (err) return err;
    if (handle == NULL) de->vfs_deletable = TRUE;
    else *handle = de;
    devfsd_notify (de, DEVFSD_NOTIFY_REGISTERED, flags & DEVFS_FL_WAIT);
    return 0;
}   /*  End Function devfs_mk_symlink  */


/**
 *	devfs_mk_dir - Create a directory in the devfs namespace.
 *	@dir: The handle to the parent devfs directory entry. If this is %NULL the
 *		new name is relative to the root of the devfs.
 *	@name: The name of the entry.
 *	@info: An arbitrary pointer which will be associated with the entry.
 *
 *	Use of this function is optional. The devfs_register() function
 *	will automatically create intermediate directories as needed. This function
 *	is provided for efficiency reasons, as it provides a handle to a directory.
 *	Returns a handle which may later be used in a call to devfs_unregister().
 *	On failure %NULL is returned.
 */

devfs_handle_t devfs_mk_dir (devfs_handle_t dir, const char *name, void *info)
{
    int err;
    struct devfs_entry *de, *old;

    if (name == NULL)
    {
	PRINTK ("(): NULL name pointer\n");
	return NULL;
    }
    if ( ( de = _devfs_prepare_leaf (&dir, name, MODE_DIR) ) == NULL )
    {
	PRINTK ("(%s): could not prepare leaf\n", name);
	return NULL;
    }
    de->info = info;
    if ( ( err = _devfs_append_entry (dir, de, FALSE, &old) ) != 0 )
    {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,1)
	if ( old && S_ISDIR (old->mode) )
	{
	    PRINTK ("(%s): using old entry in dir: %p \"%s\"\n",
		    name, dir, dir->name);
	    old->vfs_deletable = FALSE;
	    devfs_put (dir);
	    return old;
	}
#endif
	PRINTK ("(%s): could not append to dir: %p \"%s\", err: %d\n",
		name, dir, dir->name, err);
	devfs_put (old);
	devfs_put (dir);
	return NULL;
    }
    DPRINTK (DEBUG_REGISTER, "(%s): de: %p dir: %p \"%s\"\n",
	     name, de, dir, dir->name);
    devfsd_notify (de, DEVFSD_NOTIFY_REGISTERED, 0);
    devfs_put (dir);
    return de;
}   /*  End Function devfs_mk_dir  */


/**
 *	devfs_get_handle - Find the handle of a devfs entry.
 *	@dir: The handle to the parent devfs directory entry. If this is %NULL the
 *		name is relative to the root of the devfs.
 *	@name: The name of the entry.
 *	@major: The major number. This is used if @name is %NULL.
 *	@minor: The minor number. This is used if @name is %NULL.
 *	@type: The type of special file to search for. This may be either
 *		%DEVFS_SPECIAL_CHR or %DEVFS_SPECIAL_BLK.
 *	@traverse_symlinks: If %TRUE then symlink entries in the devfs namespace are
 *		traversed. Symlinks pointing out of the devfs namespace will cause a
 *		failure. Symlink traversal consumes stack space.
 *
 *	Returns a handle which may later be used in a call to
 *	devfs_unregister(), devfs_get_flags(), or devfs_set_flags(). A
 *	subsequent devfs_put() is required to decrement the refcount.
 *	On failure %NULL is returned.
 */

devfs_handle_t devfs_get_handle (devfs_handle_t dir, const char *name,
				 unsigned int major, unsigned int minor,
				 char type, int traverse_symlinks)
{
    if ( (name != NULL) && (name[0] == '\0') ) name = NULL;
    return _devfs_find_entry (dir, name, major, minor, type,traverse_symlinks);
}   /*  End Function devfs_get_handle  */


/*  Compatibility function. Will be removed in sometime in 2.5  */

devfs_handle_t devfs_find_handle (devfs_handle_t dir, const char *name,
				  unsigned int major, unsigned int minor,
				  char type, int traverse_symlinks)
{
    devfs_handle_t de;

    de = devfs_get_handle (dir, name, major, minor, type, traverse_symlinks);
    devfs_put (de);
    return de;
}   /*  End Function devfs_find_handle  */


/**
 *	devfs_get_flags - Get the flags for a devfs entry.
 *	@de: The handle to the device entry.
 *	@flags: The flags are written here.
 *
 *	Returns 0 on success, else a negative error code.
 */

int devfs_get_flags (devfs_handle_t de, unsigned int *flags)
{
    unsigned int fl = 0;

    if (de == NULL) return -EINVAL;
    VERIFY_ENTRY (de);
    if (de->hide) fl |= DEVFS_FL_HIDE;
    if ( S_ISCHR (de->mode) || S_ISBLK (de->mode) || S_ISREG (de->mode) )
    {
	if (de->u.fcb.auto_owner) fl |= DEVFS_FL_AUTO_OWNER;
	if (de->u.fcb.aopen_notify) fl |= DEVFS_FL_AOPEN_NOTIFY;
	if (de->u.fcb.removable) fl |= DEVFS_FL_REMOVABLE;
    }
    *flags = fl;
    return 0;
}   /*  End Function devfs_get_flags  */


/*
 *	devfs_set_flags - Set the flags for a devfs entry.
 *	@de: The handle to the device entry.
 *	@flags: The flags to set. Unset flags are cleared.
 *
 *	Returns 0 on success, else a negative error code.
 */

int devfs_set_flags (devfs_handle_t de, unsigned int flags)
{
    if (de == NULL) return -EINVAL;
    VERIFY_ENTRY (de);
    DPRINTK (DEBUG_SET_FLAGS, "(%s): flags: %x\n", de->name, flags);
    de->hide = (flags & DEVFS_FL_HIDE) ? TRUE : FALSE;
    if ( S_ISCHR (de->mode) || S_ISBLK (de->mode) || S_ISREG (de->mode) )
    {
	de->u.fcb.auto_owner = (flags & DEVFS_FL_AUTO_OWNER) ? TRUE : FALSE;
	de->u.fcb.aopen_notify = (flags & DEVFS_FL_AOPEN_NOTIFY) ? TRUE:FALSE;
    }
    return 0;
}   /*  End Function devfs_set_flags  */


/**
 *	devfs_get_maj_min - Get the major and minor numbers for a devfs entry.
 *	@de: The handle to the device entry.
 *	@major: The major number is written here. This may be %NULL.
 *	@minor: The minor number is written here. This may be %NULL.
 *
 *	Returns 0 on success, else a negative error code.
 */

int devfs_get_maj_min (devfs_handle_t de, unsigned int *major,
		       unsigned int *minor)
{
    if (de == NULL) return -EINVAL;
    VERIFY_ENTRY (de);
    if ( S_ISDIR (de->mode) ) return -EISDIR;
    if ( !S_ISCHR (de->mode) && !S_ISBLK (de->mode) ) return -EINVAL;
    if (major != NULL) *major = de->u.fcb.u.device.major;
    if (minor != NULL) *minor = de->u.fcb.u.device.minor;
    return 0;
}   /*  End Function devfs_get_maj_min  */


/**
 *	devfs_get_handle_from_inode - Get the devfs handle for a VFS inode.
 *	@inode: The VFS inode.
 *
 *	Returns the devfs handle on success, else %NULL.
 */

devfs_handle_t devfs_get_handle_from_inode (struct inode *inode)
{
    if (!inode || !inode->i_sb) return NULL;
    if (inode->i_sb->s_magic != DEVFS_SUPER_MAGIC) return NULL;
    return get_devfs_entry_from_vfs_inode (inode);
}   /*  End Function devfs_get_handle_from_inode  */


/**
 *	devfs_generate_path - Generate a pathname for an entry, relative to the devfs root.
 *	@de: The devfs entry.
 *	@path: The buffer to write the pathname to. The pathname and '\0'
 *		terminator will be written at the end of the buffer.
 *	@buflen: The length of the buffer.
 *
 *	Returns the offset in the buffer where the pathname starts on success,
 *	else a negative error code.
 */

int devfs_generate_path (devfs_handle_t de, char *path, int buflen)
{
    int pos;
#define NAMEOF(de) ( (de)->mode ? (de)->name : (de)->u.name )

    if (de == NULL) return -EINVAL;
    VERIFY_ENTRY (de);
    if (de->namelen >= buflen) return -ENAMETOOLONG; /*  Must be first       */
    path[buflen - 1] = '\0';
    if (de->parent == NULL) return buflen - 1;       /*  Don't prepend root  */
    pos = buflen - de->namelen - 1;
    memcpy (path + pos, NAMEOF (de), de->namelen);
    for (de = de->parent; de->parent != NULL; de = de->parent)
    {
	if (pos - de->namelen - 1 < 0) return -ENAMETOOLONG;
	path[--pos] = '/';
	pos -= de->namelen;
	memcpy (path + pos, NAMEOF (de), de->namelen);
    }
    return pos;
}   /*  End Function devfs_generate_path  */


/**
 *	devfs_get_ops - Get the device operations for a devfs entry.
 *	@de: The handle to the device entry.
 *
 *	Returns a pointer to the device operations on success, else NULL.
 *	The use count for the module owning the operations will be incremented.
 */

void *devfs_get_ops (devfs_handle_t de)
{
    struct module *owner;

    if (de == NULL) return NULL;
    VERIFY_ENTRY (de);
    if ( !S_ISCHR (de->mode) && !S_ISBLK (de->mode) && !S_ISREG (de->mode) )
	return NULL;
    if (de->u.fcb.ops == NULL) return NULL;
    read_lock (&de->parent->u.dir.lock);  /*  Prevent module from unloading  */
    if (de->next == de) owner = NULL;     /*  Ops pointer is already stale   */
    else if ( S_ISCHR (de->mode) || S_ISREG (de->mode) )
	owner = ( (struct file_operations *) de->u.fcb.ops )->owner;
    else owner = ( (struct block_device_operations *) de->u.fcb.ops )->owner;
    if ( (de->next == de) || !try_inc_mod_count (owner) )
    {   /*  Entry is already unhooked or module is unloading  */
	read_unlock (&de->parent->u.dir.lock);
	return NULL;
    }
    read_unlock (&de->parent->u.dir.lock);  /*  Module can continue unloading*/
    return de->u.fcb.ops;
}   /*  End Function devfs_get_ops  */


/**
 *	devfs_put_ops - Put the device operations for a devfs entry.
 *	@de: The handle to the device entry.
 *
 *	The use count for the module owning the operations will be decremented.
 */

void devfs_put_ops (devfs_handle_t de)
{
    struct module *owner;

    if (de == NULL) return;
    VERIFY_ENTRY (de);
    if ( !S_ISCHR (de->mode) && !S_ISBLK (de->mode) && !S_ISREG (de->mode) )
	return;
    if (de->u.fcb.ops == NULL) return;
    if ( S_ISCHR (de->mode) || S_ISREG (de->mode) )
	owner = ( (struct file_operations *) de->u.fcb.ops )->owner;
    else owner = ( (struct block_device_operations *) de->u.fcb.ops )->owner;
    if (owner) __MOD_DEC_USE_COUNT (owner);
}   /*  End Function devfs_put_ops  */


/**
 *	devfs_set_file_size - Set the file size for a devfs regular file.
 *	@de: The handle to the device entry.
 *	@size: The new file size.
 *
 *	Returns 0 on success, else a negative error code.
 */

int devfs_set_file_size (devfs_handle_t de, unsigned long size)
{
    if (de == NULL) return -EINVAL;
    VERIFY_ENTRY (de);
    if ( !S_ISREG (de->mode) ) return -EINVAL;
    if (de->u.fcb.u.file.size == size) return 0;
    de->u.fcb.u.file.size = size;
    if (de->inode.dentry == NULL) return 0;
    if (de->inode.dentry->d_inode == NULL) return 0;
    de->inode.dentry->d_inode->i_size = size;
    return 0;
}   /*  End Function devfs_set_file_size  */


/**
 *	devfs_get_info - Get the info pointer written to private_data of @de upon open.
 *	@de: The handle to the device entry.
 *
 *	Returns the info pointer.
 */
void *devfs_get_info (devfs_handle_t de)
{
    if (de == NULL) return NULL;
    VERIFY_ENTRY (de);
    return de->info;
}   /*  End Function devfs_get_info  */


/**
 *	devfs_set_info - Set the info pointer written to private_data upon open.
 *	@de: The handle to the device entry.
 *	@info: pointer to the data
 *
 *	Returns 0 on success, else a negative error code.
 */
int devfs_set_info (devfs_handle_t de, void *info)
{
    if (de == NULL) return -EINVAL;
    VERIFY_ENTRY (de);
    de->info = info;
    return 0;
}   /*  End Function devfs_set_info  */


/**
 *	devfs_get_parent - Get the parent device entry.
 *	@de: The handle to the device entry.
 *
 *	Returns the parent device entry if it exists, else %NULL.
 */
devfs_handle_t devfs_get_parent (devfs_handle_t de)
{
    if (de == NULL) return NULL;
    VERIFY_ENTRY (de);
    return de->parent;
}   /*  End Function devfs_get_parent  */


/**
 *	devfs_get_first_child - Get the first leaf node in a directory.
 *	@de: The handle to the device entry.
 *
 *	Returns the leaf node device entry if it exists, else %NULL.
 */

devfs_handle_t devfs_get_first_child (devfs_handle_t de)
{
    if (de == NULL) return NULL;
    VERIFY_ENTRY (de);
    if ( !S_ISDIR (de->mode) ) return NULL;
    return de->u.dir.first;
}   /*  End Function devfs_get_first_child  */


/**
 *	devfs_get_next_sibling - Get the next sibling leaf node. for a device entry.
 *	@de: The handle to the device entry.
 *
 *	Returns the leaf node device entry if it exists, else %NULL.
 */

devfs_handle_t devfs_get_next_sibling (devfs_handle_t de)
{
    if (de == NULL) return NULL;
    VERIFY_ENTRY (de);
    return de->next;
}   /*  End Function devfs_get_next_sibling  */


/**
 *	devfs_auto_unregister - Configure a devfs entry to be automatically unregistered.
 *	@master: The master devfs entry. Only one slave may be registered.
 *	@slave: The devfs entry which will be automatically unregistered when the
 *		master entry is unregistered. It is illegal to call devfs_unregister()
 *		on this entry.
 */

void devfs_auto_unregister (devfs_handle_t master, devfs_handle_t slave)
{
    if (master == NULL) return;
    VERIFY_ENTRY (master);
    VERIFY_ENTRY (slave);
    if (master->slave != NULL)
    {
	/*  Because of the dumbness of the layers above, ignore duplicates  */
	if (master->slave == slave) return;
	PRINTK ("(%s): only one slave allowed\n", master->name);
	OOPS ("():  old slave: \"%s\"  new slave: \"%s\"\n",
	      master->slave->name, slave->name);
    }
    master->slave = slave;
}   /*  End Function devfs_auto_unregister  */


/**
 *	devfs_get_unregister_slave - Get the slave entry which will be automatically unregistered.
 *	@master: The master devfs entry.
 *
 *	Returns the slave which will be unregistered when @master is unregistered.
 */

devfs_handle_t devfs_get_unregister_slave (devfs_handle_t master)
{
    if (master == NULL) return NULL;
    VERIFY_ENTRY (master);
    return master->slave;
}   /*  End Function devfs_get_unregister_slave  */


/**
 *	devfs_get_name - Get the name for a device entry in its parent directory.
 *	@de: The handle to the device entry.
 *	@namelen: The length of the name is written here. This may be %NULL.
 *
 *	Returns the name on success, else %NULL.
 */

const char *devfs_get_name (devfs_handle_t de, unsigned int *namelen)
{
    if (de == NULL) return NULL;
    VERIFY_ENTRY (de);
    if (namelen != NULL) *namelen = de->namelen;
    return de->name;
}   /*  End Function devfs_get_name  */


/**
 *	devfs_register_chrdev - Optionally register a conventional character driver.
 *	@major: The major number for the driver.
 *	@name: The name of the driver (as seen in /proc/devices).
 *	@fops: The &file_operations structure pointer.
 *
 *	This function will register a character driver provided the "devfs=only"
 *	option was not provided at boot time.
 *	Returns 0 on success, else a negative error code on failure.
 */

int devfs_register_chrdev (unsigned int major, const char *name,
			   struct file_operations *fops)
{
    if (boot_options & OPTION_ONLY) return 0;
    return register_chrdev (major, name, fops);
}   /*  End Function devfs_register_chrdev  */


/**
 *	devfs_register_blkdev - Optionally register a conventional block driver.
 *	@major: The major number for the driver.
 *	@name: The name of the driver (as seen in /proc/devices).
 *	@bdops: The &block_device_operations structure pointer.
 *
 *	This function will register a block driver provided the "devfs=only"
 *	option was not provided at boot time.
 *	Returns 0 on success, else a negative error code on failure.
 */

int devfs_register_blkdev (unsigned int major, const char *name,
			   struct block_device_operations *bdops)
{
    if (boot_options & OPTION_ONLY) return 0;
    return register_blkdev (major, name, bdops);
}   /*  End Function devfs_register_blkdev  */


/**
 *	devfs_unregister_chrdev - Optionally unregister a conventional character driver.
 *	@major: The major number for the driver.
 *	@name: The name of the driver (as seen in /proc/devices).
 *
 *	This function will unregister a character driver provided the "devfs=only"
 *	option was not provided at boot time.
 *	Returns 0 on success, else a negative error code on failure.
 */

int devfs_unregister_chrdev (unsigned int major, const char *name)
{
    if (boot_options & OPTION_ONLY) return 0;
    return unregister_chrdev (major, name);
}   /*  End Function devfs_unregister_chrdev  */


/**
 *	devfs_unregister_blkdev - Optionally unregister a conventional block driver.
 *	@major: The major number for the driver.
 *	@name: The name of the driver (as seen in /proc/devices).
 *
 *	This function will unregister a block driver provided the "devfs=only"
 *	option was not provided at boot time.
 *	Returns 0 on success, else a negative error code on failure.
 */

int devfs_unregister_blkdev (unsigned int major, const char *name)
{
    if (boot_options & OPTION_ONLY) return 0;
    return unregister_blkdev (major, name);
}   /*  End Function devfs_unregister_blkdev  */

/**
 *	devfs_setup - Process kernel boot options.
 *	@str: The boot options after the "devfs=".
 */

static int __init devfs_setup (char *str)
{
    static struct
    {
	char *name;
	unsigned int mask;
	unsigned int *opt;
    } devfs_options_tab[] __initdata =
    {
#ifdef CONFIG_DEVFS_DEBUG
	{"dall",      DEBUG_ALL,          &devfs_debug_init},
	{"dmod",      DEBUG_MODULE_LOAD,  &devfs_debug_init},
	{"dreg",      DEBUG_REGISTER,     &devfs_debug_init},
	{"dunreg",    DEBUG_UNREGISTER,   &devfs_debug_init},
	{"dfree",     DEBUG_FREE,         &devfs_debug_init},
	{"diget",     DEBUG_I_GET,        &devfs_debug_init},
	{"dchange",   DEBUG_SET_FLAGS,    &devfs_debug_init},
	{"dsread",    DEBUG_S_READ,       &devfs_debug_init},
	{"dichange",  DEBUG_I_CHANGE,     &devfs_debug_init},
	{"dimknod",   DEBUG_I_MKNOD,      &devfs_debug_init},
	{"dilookup",  DEBUG_I_LOOKUP,     &devfs_debug_init},
	{"diunlink",  DEBUG_I_UNLINK,     &devfs_debug_init},
#endif  /*  CONFIG_DEVFS_DEBUG  */
	{"only",      OPTION_ONLY,        &boot_options},
	{"mount",     OPTION_MOUNT,       &boot_options},
	{NULL,        0,                  NULL}
    };

    while ( (*str != '\0') && !isspace (*str) )
    {
	int i, found = 0, invert = 0;

	if (strncmp (str, "no", 2) == 0)
	{
	    invert = 1;
	    str += 2;
	}
	for (i = 0; devfs_options_tab[i].name != NULL; i++)
	{
	    int len = strlen (devfs_options_tab[i].name);

	    if (strncmp (str, devfs_options_tab[i].name, len) == 0)
	    {
		if (invert)
		    *devfs_options_tab[i].opt &= ~devfs_options_tab[i].mask;
		else
		    *devfs_options_tab[i].opt |= devfs_options_tab[i].mask;
		str += len;
		found = 1;
		break;
	    }
	}
	if (!found) return 0;       /*  No match         */
	if (*str != ',') return 0;  /*  No more options  */
	++str;
    }
    return 1;
}   /*  End Function devfs_setup  */

__setup("devfs=", devfs_setup);

EXPORT_SYMBOL(devfs_put);
EXPORT_SYMBOL(devfs_register);
EXPORT_SYMBOL(devfs_unregister);
EXPORT_SYMBOL(devfs_mk_symlink);
EXPORT_SYMBOL(devfs_mk_dir);
EXPORT_SYMBOL(devfs_get_handle);
EXPORT_SYMBOL(devfs_find_handle);
EXPORT_SYMBOL(devfs_get_flags);
EXPORT_SYMBOL(devfs_set_flags);
EXPORT_SYMBOL(devfs_get_maj_min);
EXPORT_SYMBOL(devfs_get_handle_from_inode);
EXPORT_SYMBOL(devfs_generate_path);
EXPORT_SYMBOL(devfs_get_ops);
EXPORT_SYMBOL(devfs_set_file_size);
EXPORT_SYMBOL(devfs_get_info);
EXPORT_SYMBOL(devfs_set_info);
EXPORT_SYMBOL(devfs_get_parent);
EXPORT_SYMBOL(devfs_get_first_child);
EXPORT_SYMBOL(devfs_get_next_sibling);
EXPORT_SYMBOL(devfs_auto_unregister);
EXPORT_SYMBOL(devfs_get_unregister_slave);
EXPORT_SYMBOL(devfs_get_name);
EXPORT_SYMBOL(devfs_register_chrdev);
EXPORT_SYMBOL(devfs_register_blkdev);
EXPORT_SYMBOL(devfs_unregister_chrdev);
EXPORT_SYMBOL(devfs_unregister_blkdev);


/**
 *	try_modload - Notify devfsd of an inode lookup by a non-devfsd process.
 *	@parent: The parent devfs entry.
 *	@fs_info: The filesystem info.
 *	@name: The device name.
 *	@namelen: The number of characters in @name.
 *	@buf: A working area that will be used. This must not go out of scope
 *            until devfsd is idle again.
 *
 *	Returns 0 on success (event was queued), else a negative error code.
 */

static int try_modload (struct devfs_entry *parent, struct fs_info *fs_info,
			const char *name, unsigned namelen,
			struct devfs_entry *buf)
{
    if ( !( fs_info->devfsd_event_mask & (1 << DEVFSD_NOTIFY_LOOKUP) ) )
	return -ENOENT;
    if ( is_devfsd_or_child (fs_info) ) return -ENOENT;
    memset (buf, 0, sizeof *buf);
    atomic_set (&buf->refcount, 1);
    buf->parent = parent;
    buf->namelen = namelen;
    buf->u.name = name;
    WRITE_ENTRY_MAGIC (buf, MAGIC_VALUE);
    if ( !devfsd_notify_de (buf, DEVFSD_NOTIFY_LOOKUP, 0,
			    current->euid, current->egid, fs_info, 0) )
	return -ENOENT;
    /*  Possible success: event has been queued  */
    return 0;
}   /*  End Function try_modload  */


/**
 *	check_disc_changed - Check if a removable disc was changed.
 *	@de: The device.
 *
 *	Returns 1 if the media was changed, else 0.
 *
 *	This function may block, and may indirectly cause the parent directory
 *	contents to be changed due to partition re-reading.
 */

static int check_disc_changed (struct devfs_entry *de)
{
    int tmp;
    int retval = 0;
    kdev_t dev = mk_kdev (de->u.fcb.u.device.major, de->u.fcb.u.device.minor);
    struct block_device_operations *bdops;
    extern int warn_no_part;

    if ( !S_ISBLK (de->mode) ) return 0;
    bdops = devfs_get_ops (de);
    if (!bdops) return 0;
    if (bdops->check_media_change == NULL) goto out;
    if ( !bdops->check_media_change (dev) ) goto out;
    retval = 1;
    printk (KERN_DEBUG "VFS: Disk change detected on device %s\n",
	     kdevname (dev) );
    if ( invalidate_device (dev, 0) )
	printk (KERN_WARNING "VFS: busy inodes on changed media..\n");
    /*  Ugly hack to disable messages about unable to read partition table  */
    tmp = warn_no_part;
    warn_no_part = 0;
    if (bdops->revalidate) bdops->revalidate (dev);
    warn_no_part = tmp;
out:
    devfs_put_ops (de);
    return retval;
}   /*  End Function check_disc_changed  */


/**
 *	scan_dir_for_removable - Scan a directory for removable media devices and check media.
 *	@dir: The directory.
 *
 *	This function may block, and may indirectly cause the directory
 *	contents to be changed due to partition re-reading. The directory will
 *	be locked for reading.
 */

static void scan_dir_for_removable (struct devfs_entry *dir)
{
    struct devfs_entry *de;

    read_lock (&dir->u.dir.lock);
    if (dir->u.dir.num_removable < 1) de = NULL;
    else
    {
	for (de = dir->u.dir.first; de != NULL; de = de->next)
	{
	    if (S_ISBLK (de->mode) && de->u.fcb.removable) break;
	}
	devfs_get (de);
    }
    read_unlock (&dir->u.dir.lock);
    if (de) check_disc_changed (de);
    devfs_put (de);
}   /*  End Function scan_dir_for_removable  */

/**
 *	get_removable_partition - Get removable media partition.
 *	@dir: The parent directory.
 *	@name: The name of the entry.
 *	@namelen: The number of characters in <<name>>.
 *
 *	Returns 1 if the media was changed, else 0.
 *
 *	This function may block, and may indirectly cause the directory
 *	contents to be changed due to partition re-reading. The directory must
 *	be locked for reading upon entry, and will be unlocked upon exit.
 */

static int get_removable_partition (struct devfs_entry *dir, const char *name,
				    unsigned int namelen)
{
    int retval;
    struct devfs_entry *de;

    if (dir->u.dir.num_removable < 1)
    {
	read_unlock (&dir->u.dir.lock);
	return 0;
    }
    for (de = dir->u.dir.first; de != NULL; de = de->next)
    {
	if (!S_ISBLK (de->mode) || !de->u.fcb.removable) continue;
	if (strcmp (de->name, "disc") == 0) break;
	/*  Support for names where the partition is appended to the disc name
	 */
	if (de->namelen >= namelen) continue;
	if (strncmp (de->name, name, de->namelen) == 0) break;
    }
    devfs_get (de);
    read_unlock (&dir->u.dir.lock);
    retval = de ? check_disc_changed (de) : 0;
    devfs_put (de);
    return retval;
}   /*  End Function get_removable_partition  */


/*  Superblock operations follow  */

static struct inode_operations devfs_iops;
static struct inode_operations devfs_dir_iops;
static struct file_operations devfs_fops;
static struct file_operations devfs_dir_fops;
static struct inode_operations devfs_symlink_iops;

static int devfs_notify_change (struct dentry *dentry, struct iattr *iattr)
{
    int retval;
    struct devfs_entry *de;
    struct inode *inode = dentry->d_inode;
    struct fs_info *fs_info = inode->i_sb->u.generic_sbp;

    de = get_devfs_entry_from_vfs_inode (inode);
    if (de == NULL) return -ENODEV;
    retval = inode_change_ok (inode, iattr);
    if (retval != 0) return retval;
    retval = inode_setattr (inode, iattr);
    if (retval != 0) return retval;
    DPRINTK (DEBUG_I_CHANGE, "(%d): VFS inode: %p  devfs_entry: %p\n",
	     (int) inode->i_ino, inode, de);
    DPRINTK (DEBUG_I_CHANGE, "():   mode: 0%o  uid: %d  gid: %d\n",
	     (int) inode->i_mode, (int) inode->i_uid, (int) inode->i_gid);
    /*  Inode is not on hash chains, thus must save permissions here rather
	than in a write_inode() method  */
    if ( ( !S_ISREG (inode->i_mode) && !S_ISCHR (inode->i_mode) &&
	   !S_ISBLK (inode->i_mode) ) || !de->u.fcb.auto_owner )
    {
	de->mode = inode->i_mode;
	de->inode.uid = inode->i_uid;
	de->inode.gid = inode->i_gid;
    }
    de->inode.atime = inode->i_atime;
    de->inode.mtime = inode->i_mtime;
    de->inode.ctime = inode->i_ctime;
    if ( ( iattr->ia_valid & (ATTR_MODE | ATTR_UID | ATTR_GID) ) &&
	 !is_devfsd_or_child (fs_info) )
	devfsd_notify_de (de, DEVFSD_NOTIFY_CHANGE, inode->i_mode,
			  inode->i_uid, inode->i_gid, fs_info, 0);
    return 0;
}   /*  End Function devfs_notify_change  */

static int devfs_statfs (struct super_block *sb, struct statfs *buf)
{
    buf->f_type = DEVFS_SUPER_MAGIC;
    buf->f_bsize = FAKE_BLOCK_SIZE;
    buf->f_bfree = 0;
    buf->f_bavail = 0;
    buf->f_ffree = 0;
    buf->f_namelen = NAME_MAX;
    return 0;
}   /*  End Function devfs_statfs  */

static void devfs_clear_inode (struct inode *inode)
{
    if ( S_ISBLK (inode->i_mode) ) bdput (inode->i_bdev);
}   /*  End Function devfs_clear_inode  */

static struct super_operations devfs_sops =
{ 
    .put_inode     = force_delete,
    .clear_inode   = devfs_clear_inode,
    .statfs        = devfs_statfs,
};


/**
 *	_devfs_get_vfs_inode - Get a VFS inode.
 *	@sb: The super block.
 *	@de: The devfs inode.
 *	@dentry: The dentry to register with the devfs inode.
 *
 *	Returns the inode on success, else %NULL. An implicit devfs_get() is
 *       performed if the inode is created.
 */

static struct inode *_devfs_get_vfs_inode (struct super_block *sb,
					   struct devfs_entry *de,
					   struct dentry *dentry)
{
    int is_fcb = FALSE;
    struct inode *inode;

    if (de->prev == de) return NULL;  /*  Quick check to see if unhooked  */
    if ( ( inode = new_inode (sb) ) == NULL )
    {
	PRINTK ("(%s): new_inode() failed, de: %p\n", de->name, de);
	return NULL;
    }
    if (de->parent)
    {
	read_lock (&de->parent->u.dir.lock);
	if (de->prev != de) de->inode.dentry = dentry; /*      Not unhooked  */
	read_unlock (&de->parent->u.dir.lock);
    }
    else de->inode.dentry = dentry;             /*  Root: no locking needed  */
    if (de->inode.dentry != dentry)
    {   /*  Must have been unhooked  */
	iput (inode);
	return NULL;
    }
    inode->u.generic_ip = devfs_get (de);
    inode->i_ino = de->inode.ino;
    DPRINTK (DEBUG_I_GET, "(%d): VFS inode: %p  devfs_entry: %p\n",
	     (int) inode->i_ino, inode, de);
    inode->i_blocks = 0;
    inode->i_blksize = FAKE_BLOCK_SIZE;
    inode->i_op = &devfs_iops;
    inode->i_fop = &devfs_fops;
    inode->i_rdev = NODEV;
    if ( S_ISCHR (de->mode) )
    {
	inode->i_rdev = mk_kdev (de->u.fcb.u.device.major,
				 de->u.fcb.u.device.minor);
	inode->i_cdev = cdget ( kdev_t_to_nr (inode->i_rdev) );
	is_fcb = TRUE;
    }
    else if ( S_ISBLK (de->mode) )
    {
	inode->i_rdev = mk_kdev (de->u.fcb.u.device.major,
				 de->u.fcb.u.device.minor);
	if (bd_acquire (inode) == 0)
	{
	    if (!inode->i_bdev->bd_op && de->u.fcb.ops)
		inode->i_bdev->bd_op = de->u.fcb.ops;
	}
	else PRINTK ("(%d): no block device from bdget()\n",(int)inode->i_ino);
	is_fcb = TRUE;
    }
    else if ( S_ISFIFO (de->mode) ) inode->i_fop = &def_fifo_fops;
    else if ( S_ISREG (de->mode) )
    {
	inode->i_size = de->u.fcb.u.file.size;
	is_fcb = TRUE;
    }
    else if ( S_ISDIR (de->mode) )
    {
	inode->i_op = &devfs_dir_iops;
    	inode->i_fop = &devfs_dir_fops;
    }
    else if ( S_ISLNK (de->mode) )
    {
	inode->i_op = &devfs_symlink_iops;
	inode->i_size = de->u.symlink.length;
    }
    if (is_fcb && de->u.fcb.auto_owner)
	inode->i_mode = (de->mode & S_IFMT) | S_IRUGO | S_IWUGO;
    else inode->i_mode = de->mode;
    inode->i_uid = de->inode.uid;
    inode->i_gid = de->inode.gid;
    inode->i_atime = de->inode.atime;
    inode->i_mtime = de->inode.mtime;
    inode->i_ctime = de->inode.ctime;
    DPRINTK (DEBUG_I_GET, "():   mode: 0%o  uid: %d  gid: %d\n",
	     (int) inode->i_mode, (int) inode->i_uid, (int) inode->i_gid);
    return inode;
}   /*  End Function _devfs_get_vfs_inode  */


/*  File operations for device entries follow  */

static int devfs_readdir (struct file *file, void *dirent, filldir_t filldir)
{
    int err, count;
    int stored = 0;
    struct fs_info *fs_info;
    struct devfs_entry *parent, *de, *next = NULL;
    struct inode *inode = file->f_dentry->d_inode;

    fs_info = inode->i_sb->u.generic_sbp;
    parent = get_devfs_entry_from_vfs_inode (file->f_dentry->d_inode);
    if ( (long) file->f_pos < 0 ) return -EINVAL;
    DPRINTK (DEBUG_F_READDIR, "(%s): fs_info: %p  pos: %ld\n",
	     parent->name, fs_info, (long) file->f_pos);
    switch ( (long) file->f_pos )
    {
      case 0:
	scan_dir_for_removable (parent);
	err = (*filldir) (dirent, "..", 2, file->f_pos,
			  file->f_dentry->d_parent->d_inode->i_ino, DT_DIR);
	if (err == -EINVAL) break;
	if (err < 0) return err;
	file->f_pos++;
	++stored;
	/*  Fall through  */
      case 1:
	err = (*filldir) (dirent, ".", 1, file->f_pos, inode->i_ino, DT_DIR);
	if (err == -EINVAL) break;
	if (err < 0) return err;
	file->f_pos++;
	++stored;
	/*  Fall through  */
      default:
	/*  Skip entries  */
	count = file->f_pos - 2;
	read_lock (&parent->u.dir.lock);
	for (de = parent->u.dir.first; de && (count > 0); de = de->next)
	    if ( !IS_HIDDEN (de) ) --count;
	devfs_get (de);
	read_unlock (&parent->u.dir.lock);
	/*  Now add all remaining entries  */
	while (de)
	{
	    if ( IS_HIDDEN (de) ) err = 0;
	    else
	    {
		err = (*filldir) (dirent, de->name, de->namelen,
				  file->f_pos, de->inode.ino, de->mode >> 12);
		if (err < 0) devfs_put (de);
		else
		{
		    file->f_pos++;
		    ++stored;
		}
	    }
	    if (err == -EINVAL) break;
	    if (err < 0) return err;
	    read_lock (&parent->u.dir.lock);
	    next = devfs_get (de->next);
	    read_unlock (&parent->u.dir.lock);
	    devfs_put (de);
	    de = next;
	}
	break;
    }
    return stored;
}   /*  End Function devfs_readdir  */

static int devfs_open (struct inode *inode, struct file *file)
{
    int err;
    struct fcb_type *df;
    struct devfs_entry *de;
    struct fs_info *fs_info = inode->i_sb->u.generic_sbp;
    void *ops;

    de = get_devfs_entry_from_vfs_inode (inode);
    if (de == NULL) return -ENODEV;
    if ( S_ISDIR (de->mode) ) return 0;
    df = &de->u.fcb;
    file->private_data = de->info;
    ops = devfs_get_ops (de);  /*  Now have module refcount  */
    if ( S_ISBLK (inode->i_mode) )
    {
	file->f_op = &def_blk_fops;
	if (ops) inode->i_bdev->bd_op = ops;
	err = def_blk_fops.open (inode, file); /* This bumps module refcount */
	devfs_put_ops (de);                    /* So drop my refcount        */
    }
    else
    {
	file->f_op = ops;
	if (file->f_op)
	{
	    lock_kernel ();
	    err = file->f_op->open ? (*file->f_op->open) (inode, file) : 0;
	    unlock_kernel ();
	}
	else
	{   /*  Fallback to legacy scheme (I don't have a module refcount)  */
	    if ( S_ISCHR (inode->i_mode) ) err = chrdev_open (inode, file);
	    else err = -ENODEV;
	}
    }
    if (err < 0) return err;
    /*  Open was successful  */
    if (df->open) return 0;
    df->open = TRUE;  /*  This is the first open  */
    if (df->auto_owner)
    {
	/*  Change the ownership/protection to what driver specified  */
	inode->i_mode = de->mode;
	inode->i_uid = current->euid;
	inode->i_gid = current->egid;
    }
    if ( df->aopen_notify && !is_devfsd_or_child (fs_info) )
	devfsd_notify_de (de, DEVFSD_NOTIFY_ASYNC_OPEN, inode->i_mode,
			  current->euid, current->egid, fs_info, 0);
    return 0;
}   /*  End Function devfs_open  */

static struct file_operations devfs_fops =
{
    .open    = devfs_open,
};

static struct file_operations devfs_dir_fops =
{
    .read    = generic_read_dir,
    .readdir = devfs_readdir,
    .open    = devfs_open,
};


/*  Dentry operations for device entries follow  */


/**
 *	devfs_d_release - Callback for when a dentry is freed.
 *	@dentry: The dentry.
 */

static void devfs_d_release (struct dentry *dentry)
{
    DPRINTK (DEBUG_D_RELEASE, "(%p): inode: %p\n", dentry, dentry->d_inode);
}   /*  End Function devfs_d_release  */

/**
 *	devfs_d_iput - Callback for when a dentry loses its inode.
 *	@dentry: The dentry.
 *	@inode:	The inode.
 */

static void devfs_d_iput (struct dentry *dentry, struct inode *inode)
{
    struct devfs_entry *de;

    de = get_devfs_entry_from_vfs_inode (inode);
    DPRINTK (DEBUG_D_IPUT,"(%s): dentry: %p inode: %p de: %p de->dentry: %p\n",
	     de->name, dentry, inode, de, de->inode.dentry);
    if ( de->inode.dentry && (de->inode.dentry != dentry) )
	OOPS ("(%s): de: %p dentry: %p de->dentry: %p\n",
	      de->name, de, dentry, de->inode.dentry);
    de->inode.dentry = NULL;
    iput (inode);
    devfs_put (de);
}   /*  End Function devfs_d_iput  */

static int devfs_d_delete (struct dentry *dentry);

static struct dentry_operations devfs_dops =
{
    .d_delete     = devfs_d_delete,
    .d_release    = devfs_d_release,
    .d_iput       = devfs_d_iput,
};

static int devfs_d_revalidate_wait (struct dentry *dentry, int flags);

static struct dentry_operations devfs_wait_dops =
{
    .d_delete     = devfs_d_delete,
    .d_release    = devfs_d_release,
    .d_iput       = devfs_d_iput,
    .d_revalidate = devfs_d_revalidate_wait,
};

/**
 *	devfs_d_delete - Callback for when all files for a dentry are closed.
 *	@dentry: The dentry.
 */

static int devfs_d_delete (struct dentry *dentry)
{
    struct inode *inode = dentry->d_inode;
    struct devfs_entry *de;
    struct fs_info *fs_info;

    if (dentry->d_op == &devfs_wait_dops) dentry->d_op = &devfs_dops;
    /*  Unhash dentry if negative (has no inode)  */
    if (inode == NULL)
    {
	DPRINTK (DEBUG_D_DELETE, "(%p): dropping negative dentry\n", dentry);
	return 1;
    }
    fs_info = inode->i_sb->u.generic_sbp;
    de = get_devfs_entry_from_vfs_inode (inode);
    DPRINTK (DEBUG_D_DELETE, "(%p): inode: %p  devfs_entry: %p\n",
	     dentry, inode, de);
    if (de == NULL) return 0;
    if ( !S_ISCHR (de->mode) && !S_ISBLK (de->mode) && !S_ISREG (de->mode) )
	return 0;
    if (!de->u.fcb.open) return 0;
    de->u.fcb.open = FALSE;
    if (de->u.fcb.aopen_notify)
	devfsd_notify_de (de, DEVFSD_NOTIFY_CLOSE, inode->i_mode,
			  current->euid, current->egid, fs_info, 1);
    if (!de->u.fcb.auto_owner) return 0;
    /*  Change the ownership/protection back  */
    inode->i_mode = (de->mode & S_IFMT) | S_IRUGO | S_IWUGO;
    inode->i_uid = de->inode.uid;
    inode->i_gid = de->inode.gid;
    return 0;
}   /*  End Function devfs_d_delete  */

struct devfs_lookup_struct
{
    devfs_handle_t de;
    wait_queue_head_t wait_queue;
};

static int devfs_d_revalidate_wait (struct dentry *dentry, int flags)
{
    struct inode *dir = dentry->d_parent->d_inode;
    struct fs_info *fs_info = dir->i_sb->u.generic_sbp;
    devfs_handle_t parent = get_devfs_entry_from_vfs_inode (dir);
    struct devfs_lookup_struct *lookup_info = dentry->d_fsdata;
    DECLARE_WAITQUEUE (wait, current);

    if ( is_devfsd_or_child (fs_info) )
    {
	devfs_handle_t de = lookup_info->de;
	struct inode *inode;

	DPRINTK (DEBUG_I_LOOKUP,
		 "(%s): dentry: %p inode: %p de: %p by: \"%s\"\n",
		 dentry->d_name.name, dentry, dentry->d_inode, de,
		 current->comm);
	if (dentry->d_inode) return 1;
	if (de == NULL)
	{
	    read_lock (&parent->u.dir.lock);
	    de = _devfs_search_dir (parent, dentry->d_name.name,
				    dentry->d_name.len);
	    read_unlock (&parent->u.dir.lock);
	    if (de == NULL) return 1;
	    lookup_info->de = de;
	}
	/*  Create an inode, now that the driver information is available  */
	inode = _devfs_get_vfs_inode (dir->i_sb, de, dentry);
	if (!inode) return 1;
	DPRINTK (DEBUG_I_LOOKUP,
		 "(%s): new VFS inode(%u): %p de: %p by: \"%s\"\n",
		 de->name, de->inode.ino, inode, de, current->comm);
	d_instantiate (dentry, inode);
	return 1;
    }
    if (lookup_info == NULL) return 1;  /*  Early termination  */
    read_lock (&parent->u.dir.lock);
    if (dentry->d_fsdata)
    {
	set_current_state (TASK_UNINTERRUPTIBLE);
	add_wait_queue (&lookup_info->wait_queue, &wait);
	read_unlock (&parent->u.dir.lock);
	schedule ();
    }
    else read_unlock (&parent->u.dir.lock);
    return 1;
}   /*  End Function devfs_d_revalidate_wait  */


/*  Inode operations for device entries follow  */

static struct dentry *devfs_lookup (struct inode *dir, struct dentry *dentry)
{
    struct devfs_entry tmp;  /*  Must stay in scope until devfsd idle again  */
    struct devfs_lookup_struct lookup_info;
    struct fs_info *fs_info = dir->i_sb->u.generic_sbp;
    struct devfs_entry *parent, *de;
    struct inode *inode;
    struct dentry *retval = NULL;

    /*  Set up the dentry operations before anything else, to ensure cleaning
	up on any error  */
    dentry->d_op = &devfs_dops;
    /*  First try to get the devfs entry for this directory  */
    parent = get_devfs_entry_from_vfs_inode (dir);
    DPRINTK (DEBUG_I_LOOKUP, "(%s): dentry: %p parent: %p by: \"%s\"\n",
	     dentry->d_name.name, dentry, parent, current->comm);
    if (parent == NULL) return ERR_PTR (-ENOENT);
    read_lock (&parent->u.dir.lock);
    de = _devfs_search_dir (parent, dentry->d_name.name, dentry->d_name.len);
    if (de) read_unlock (&parent->u.dir.lock);
    else
    {   /*  Try re-reading the partition (media may have changed)  */
	if ( get_removable_partition (parent, dentry->d_name.name,
				      dentry->d_name.len) )  /*  Unlocks  */
	{   /*  Media did change  */
	    read_lock (&parent->u.dir.lock);
	    de = _devfs_search_dir (parent, dentry->d_name.name,
				    dentry->d_name.len);
	    read_unlock (&parent->u.dir.lock);
	}
    }
    lookup_info.de = de;
    init_waitqueue_head (&lookup_info.wait_queue);
    dentry->d_fsdata = &lookup_info;
    if (de == NULL)
    {   /*  Try with devfsd. For any kind of failure, leave a negative dentry
	    so someone else can deal with it (in the case where the sysadmin
	    does a mknod()). It's important to do this before hashing the
	    dentry, so that the devfsd queue is filled before revalidates
	    can start  */
	if (try_modload (parent, fs_info,
			 dentry->d_name.name, dentry->d_name.len, &tmp) < 0)
	{   /*  Lookup event was not queued to devfsd  */
	    d_add (dentry, NULL);
	    return NULL;
	}
    }
    dentry->d_op = &devfs_wait_dops;
    d_add (dentry, NULL);  /*  Open the floodgates  */
    /*  Unlock directory semaphore, which will release any waiters. They
	will get the hashed dentry, and may be forced to wait for
	revalidation  */
    up (&dir->i_sem);
    wait_for_devfsd_finished (fs_info);  /*  If I'm not devfsd, must wait  */
    down (&dir->i_sem);      /*  Grab it again because them's the rules  */
    de = lookup_info.de;
    /*  If someone else has been so kind as to make the inode, we go home
	early  */
    if (dentry->d_inode) goto out;
    if (de == NULL)
    {
	read_lock (&parent->u.dir.lock);
	de = _devfs_search_dir (parent, dentry->d_name.name,
				dentry->d_name.len);
	read_unlock (&parent->u.dir.lock);
	if (de == NULL) goto out;
	/*  OK, there's an entry now, but no VFS inode yet  */
    }
    /*  Create an inode, now that the driver information is available  */
    inode = _devfs_get_vfs_inode (dir->i_sb, de, dentry);
    if (!inode)
    {
	retval = ERR_PTR (-ENOMEM);
	goto out;
    }
    DPRINTK (DEBUG_I_LOOKUP, "(%s): new VFS inode(%u): %p de: %p by: \"%s\"\n",
	     de->name, de->inode.ino, inode, de, current->comm);
    d_instantiate (dentry, inode);
out:
    dentry->d_op = &devfs_dops;
    dentry->d_fsdata = NULL;
    write_lock (&parent->u.dir.lock);
    wake_up (&lookup_info.wait_queue);
    write_unlock (&parent->u.dir.lock);
    devfs_put (de);
    return retval;
}   /*  End Function devfs_lookup  */

static int devfs_unlink (struct inode *dir, struct dentry *dentry)
{
    int unhooked;
    struct devfs_entry *de;
    struct inode *inode = dentry->d_inode;
    struct fs_info *fs_info = dir->i_sb->u.generic_sbp;

    de = get_devfs_entry_from_vfs_inode (inode);
    DPRINTK (DEBUG_I_UNLINK, "(%s): de: %p\n", dentry->d_name.name, de);
    if (de == NULL) return -ENOENT;
    if (!de->vfs_deletable) return -EPERM;
    write_lock (&de->parent->u.dir.lock);
    unhooked = _devfs_unhook (de);
    write_unlock (&de->parent->u.dir.lock);
    if (!unhooked) return -ENOENT;
    if ( !is_devfsd_or_child (fs_info) )
	devfsd_notify_de (de, DEVFSD_NOTIFY_DELETE, inode->i_mode,
			  inode->i_uid, inode->i_gid, fs_info, 0);
    free_dentry (de);
    devfs_put (de);
    return 0;
}   /*  End Function devfs_unlink  */

static int devfs_symlink (struct inode *dir, struct dentry *dentry,
			  const char *symname)
{
    int err;
    struct fs_info *fs_info = dir->i_sb->u.generic_sbp;
    struct devfs_entry *parent, *de;
    struct inode *inode;

    /*  First try to get the devfs entry for this directory  */
    parent = get_devfs_entry_from_vfs_inode (dir);
    if (parent == NULL) return -ENOENT;
    err = devfs_do_symlink (parent, dentry->d_name.name, DEVFS_FL_NONE,
			    symname, &de, NULL);
    DPRINTK (DEBUG_DISABLED, "(%s): errcode from <devfs_do_symlink>: %d\n",
	     dentry->d_name.name, err);
    if (err < 0) return err;
    de->vfs_deletable = TRUE;
    de->inode.uid = current->euid;
    de->inode.gid = current->egid;
    de->inode.atime = CURRENT_TIME;
    de->inode.mtime = CURRENT_TIME;
    de->inode.ctime = CURRENT_TIME;
    if ( ( inode = _devfs_get_vfs_inode (dir->i_sb, de, dentry) ) == NULL )
	return -ENOMEM;
    DPRINTK (DEBUG_DISABLED, "(%s): new VFS inode(%u): %p  dentry: %p\n",
	     dentry->d_name.name, de->inode.ino, inode, dentry);
    d_instantiate (dentry, inode);
    if ( !is_devfsd_or_child (fs_info) )
	devfsd_notify_de (de, DEVFSD_NOTIFY_CREATE, inode->i_mode,
			  inode->i_uid, inode->i_gid, fs_info, 0);
    return 0;
}   /*  End Function devfs_symlink  */

static int devfs_mkdir (struct inode *dir, struct dentry *dentry, int mode)
{
    int err;
    struct fs_info *fs_info = dir->i_sb->u.generic_sbp;
    struct devfs_entry *parent, *de;
    struct inode *inode;

    mode = (mode & ~S_IFMT) | S_IFDIR;  /*  VFS doesn't pass S_IFMT part  */
    parent = get_devfs_entry_from_vfs_inode (dir);
    if (parent == NULL) return -ENOENT;
    de = _devfs_alloc_entry (dentry->d_name.name, dentry->d_name.len, mode);
    if (!de) return -ENOMEM;
    de->vfs_deletable = TRUE;
    if ( ( err = _devfs_append_entry (parent, de, FALSE, NULL) ) != 0 )
	return err;
    de->inode.uid = current->euid;
    de->inode.gid = current->egid;
    de->inode.atime = CURRENT_TIME;
    de->inode.mtime = CURRENT_TIME;
    de->inode.ctime = CURRENT_TIME;
    if ( ( inode = _devfs_get_vfs_inode (dir->i_sb, de, dentry) ) == NULL )
	return -ENOMEM;
    DPRINTK (DEBUG_DISABLED, "(%s): new VFS inode(%u): %p  dentry: %p\n",
	     dentry->d_name.name, de->inode.ino, inode, dentry);
    d_instantiate (dentry, inode);
    if ( !is_devfsd_or_child (fs_info) )
	devfsd_notify_de (de, DEVFSD_NOTIFY_CREATE, inode->i_mode,
			  inode->i_uid, inode->i_gid, fs_info, 0);
    return 0;
}   /*  End Function devfs_mkdir  */

static int devfs_rmdir (struct inode *dir, struct dentry *dentry)
{
    int err = 0;
    struct devfs_entry *de;
    struct fs_info *fs_info = dir->i_sb->u.generic_sbp;
    struct inode *inode = dentry->d_inode;

    if (dir->i_sb->u.generic_sbp != inode->i_sb->u.generic_sbp) return -EINVAL;
    de = get_devfs_entry_from_vfs_inode (inode);
    if (de == NULL) return -ENOENT;
    if ( !S_ISDIR (de->mode) ) return -ENOTDIR;
    if (!de->vfs_deletable) return -EPERM;
    /*  First ensure the directory is empty and will stay that way  */
    write_lock (&de->u.dir.lock);
    if (de->u.dir.first) err = -ENOTEMPTY;
    else de->u.dir.no_more_additions = TRUE;
    write_unlock (&de->u.dir.lock);
    if (err) return err;
    /*  Now unhook the directory from it's parent  */
    write_lock (&de->parent->u.dir.lock);
    if ( !_devfs_unhook (de) ) err = -ENOENT;
    write_unlock (&de->parent->u.dir.lock);
    if (err) return err;
    if ( !is_devfsd_or_child (fs_info) )
	devfsd_notify_de (de, DEVFSD_NOTIFY_DELETE, inode->i_mode,
			  inode->i_uid, inode->i_gid, fs_info, 0);
    free_dentry (de);
    devfs_put (de);
    return 0;
}   /*  End Function devfs_rmdir  */

static int devfs_mknod (struct inode *dir, struct dentry *dentry, int mode,
			int rdev)
{
    int err;
    struct fs_info *fs_info = dir->i_sb->u.generic_sbp;
    struct devfs_entry *parent, *de;
    struct inode *inode;

    DPRINTK (DEBUG_I_MKNOD, "(%s): mode: 0%o  dev: %d\n",
	     dentry->d_name.name, mode, rdev);
    parent = get_devfs_entry_from_vfs_inode (dir);
    if (parent == NULL) return -ENOENT;
    de = _devfs_alloc_entry (dentry->d_name.name, dentry->d_name.len, mode);
    if (!de) return -ENOMEM;
    de->vfs_deletable = TRUE;
    if ( S_ISBLK (mode) || S_ISCHR (mode) )
    {
	de->u.fcb.u.device.major = MAJOR (rdev);
	de->u.fcb.u.device.minor = MINOR (rdev);
    }
    if ( ( err = _devfs_append_entry (parent, de, FALSE, NULL) ) != 0 )
	return err;
    de->inode.uid = current->euid;
    de->inode.gid = current->egid;
    de->inode.atime = CURRENT_TIME;
    de->inode.mtime = CURRENT_TIME;
    de->inode.ctime = CURRENT_TIME;
    if ( ( inode = _devfs_get_vfs_inode (dir->i_sb, de, dentry) ) == NULL )
	return -ENOMEM;
    DPRINTK (DEBUG_I_MKNOD, ":   new VFS inode(%u): %p  dentry: %p\n",
	     de->inode.ino, inode, dentry);
    d_instantiate (dentry, inode);
    if ( !is_devfsd_or_child (fs_info) )
	devfsd_notify_de (de, DEVFSD_NOTIFY_CREATE, inode->i_mode,
			  inode->i_uid, inode->i_gid, fs_info, 0);
    return 0;
}   /*  End Function devfs_mknod  */

static int devfs_readlink (struct dentry *dentry, char *buffer, int buflen)
{
    int err;
    struct devfs_entry *de;

    de = get_devfs_entry_from_vfs_inode (dentry->d_inode);
    if (!de) return -ENODEV;
    err = vfs_readlink (dentry, buffer, buflen, de->u.symlink.linkname);
    return err;
}   /*  End Function devfs_readlink  */

static int devfs_follow_link (struct dentry *dentry, struct nameidata *nd)
{
    int err;
    struct devfs_entry *de;

    de = get_devfs_entry_from_vfs_inode (dentry->d_inode);
    if (!de) return -ENODEV;
    err = vfs_follow_link (nd, de->u.symlink.linkname);
    return err;
}   /*  End Function devfs_follow_link  */

static struct inode_operations devfs_iops =
{
    .setattr        = devfs_notify_change,
};

static struct inode_operations devfs_dir_iops =
{
    .lookup         = devfs_lookup,
    .unlink         = devfs_unlink,
    .symlink        = devfs_symlink,
    .mkdir          = devfs_mkdir,
    .rmdir          = devfs_rmdir,
    .mknod          = devfs_mknod,
    .setattr        = devfs_notify_change,
};

static struct inode_operations devfs_symlink_iops =
{
    .readlink       = devfs_readlink,
    .follow_link    = devfs_follow_link,
    .setattr        = devfs_notify_change,
};

static struct super_block *devfs_read_super (struct super_block *sb,
					     void *data, int silent)
{
    struct inode *root_inode = NULL;

    if (_devfs_get_root_entry () == NULL) goto out_no_root;
    atomic_set (&fs_info.devfsd_overrun_count, 0);
    init_waitqueue_head (&fs_info.devfsd_wait_queue);
    init_waitqueue_head (&fs_info.revalidate_wait_queue);
    fs_info.sb = sb;
    sb->u.generic_sbp = &fs_info;
    sb->s_blocksize = 1024;
    sb->s_blocksize_bits = 10;
    sb->s_magic = DEVFS_SUPER_MAGIC;
    sb->s_op = &devfs_sops;
    if ( ( root_inode = _devfs_get_vfs_inode (sb, root_entry, NULL) ) == NULL )
	goto out_no_root;
    sb->s_root = d_alloc_root (root_inode);
    if (!sb->s_root) goto out_no_root;
    DPRINTK (DEBUG_S_READ, "(): made devfs ptr: %p\n", sb->u.generic_sbp);
    return sb;

out_no_root:
    PRINTK ("(): get root inode failed\n");
    if (root_inode) iput (root_inode);
    return NULL;
}   /*  End Function devfs_read_super  */


static DECLARE_FSTYPE (devfs_fs_type, DEVFS_NAME, devfs_read_super, FS_SINGLE);


/*  File operations for devfsd follow  */

static ssize_t devfsd_read (struct file *file, char *buf, size_t len,
			    loff_t *ppos)
{
    int done = FALSE;
    int ival;
    loff_t pos, devname_offset, tlen, rpos;
    devfs_handle_t de;
    struct devfsd_buf_entry *entry;
    struct fs_info *fs_info = file->f_dentry->d_inode->i_sb->u.generic_sbp;
    struct devfsd_notify_struct *info = fs_info->devfsd_info;
    DECLARE_WAITQUEUE (wait, current);

    /*  Can't seek (pread) on this device  */
    if (ppos != &file->f_pos) return -ESPIPE;
    /*  Verify the task has grabbed the queue  */
    if (fs_info->devfsd_task != current) return -EPERM;
    info->major = 0;
    info->minor = 0;
    /*  Block for a new entry  */
    set_current_state (TASK_INTERRUPTIBLE);
    add_wait_queue (&fs_info->devfsd_wait_queue, &wait);
    while ( devfsd_queue_empty (fs_info) )
    {
	fs_info->devfsd_sleeping = TRUE;
	wake_up (&fs_info->revalidate_wait_queue);
	schedule ();
	fs_info->devfsd_sleeping = FALSE;
	if ( signal_pending (current) )
	{
	    remove_wait_queue (&fs_info->devfsd_wait_queue, &wait);
	    __set_current_state (TASK_RUNNING);
	    return -EINTR;
	}
	set_current_state (TASK_INTERRUPTIBLE);
    }
    remove_wait_queue (&fs_info->devfsd_wait_queue, &wait);
    __set_current_state (TASK_RUNNING);
    /*  Now play with the data  */
    ival = atomic_read (&fs_info->devfsd_overrun_count);
    info->overrun_count = ival;
    entry = fs_info->devfsd_first_event;
    info->type = entry->type;
    info->mode = entry->mode;
    info->uid = entry->uid;
    info->gid = entry->gid;
    de = entry->de;
    if ( S_ISCHR (de->mode) || S_ISBLK (de->mode) )
    {
	info->major = de->u.fcb.u.device.major;
	info->minor = de->u.fcb.u.device.minor;
    }
    pos = devfs_generate_path (de, info->devname, DEVFS_PATHLEN);
    if (pos < 0) return pos;
    info->namelen = DEVFS_PATHLEN - pos - 1;
    if (info->mode == 0) info->mode = de->mode;
    devname_offset = info->devname - (char *) info;
    rpos = *ppos;
    if (rpos < devname_offset)
    {
	/*  Copy parts of the header  */
	tlen = devname_offset - rpos;
	if (tlen > len) tlen = len;
	if ( copy_to_user (buf, (char *) info + rpos, tlen) )
	{
	    return -EFAULT;
	}
	rpos += tlen;
	buf += tlen;
	len -= tlen;
    }
    if ( (rpos >= devname_offset) && (len > 0) )
    {
	/*  Copy the name  */
	tlen = info->namelen + 1;
	if (tlen > len) tlen = len;
	else done = TRUE;
	if ( copy_to_user (buf, info->devname + pos + rpos - devname_offset,
			   tlen) )
	{
	    return -EFAULT;
	}
	rpos += tlen;
    }
    tlen = rpos - *ppos;
    if (done)
    {
	devfs_handle_t parent;

	spin_lock (&fs_info->devfsd_buffer_lock);
	fs_info->devfsd_first_event = entry->next;
	if (entry->next == NULL) fs_info->devfsd_last_event = NULL;
	spin_unlock (&fs_info->devfsd_buffer_lock);
	for (; de != NULL; de = parent)
	{
	    parent = de->parent;
	    devfs_put (de);
	}
	kmem_cache_free (devfsd_buf_cache, entry);
	if (ival > 0) atomic_sub (ival, &fs_info->devfsd_overrun_count);
	*ppos = 0;
    }
    else *ppos = rpos;
    return tlen;
}   /*  End Function devfsd_read  */

static int devfsd_ioctl (struct inode *inode, struct file *file,
			 unsigned int cmd, unsigned long arg)
{
    int ival;
    struct fs_info *fs_info = inode->i_sb->u.generic_sbp;

    switch (cmd)
    {
      case DEVFSDIOC_GET_PROTO_REV:
	ival = DEVFSD_PROTOCOL_REVISION_KERNEL;
	if ( copy_to_user ( (void *)arg, &ival, sizeof ival ) ) return -EFAULT;
	break;
      case DEVFSDIOC_SET_EVENT_MASK:
	/*  Ensure only one reader has access to the queue. This scheme will
	    work even if the global kernel lock were to be removed, because it
	    doesn't matter who gets in first, as long as only one gets it  */
	if (fs_info->devfsd_task == NULL)
	{
	    static spinlock_t lock = SPIN_LOCK_UNLOCKED;

	    if ( !spin_trylock (&lock) ) return -EBUSY;
	    if (fs_info->devfsd_task != NULL)
	    {   /*  We lost the race...  */
		spin_unlock (&lock);
		return -EBUSY;
	    }
	    fs_info->devfsd_task = current;
	    spin_unlock (&lock);
	    fs_info->devfsd_pgrp = (current->pgrp == current->pid) ?
		current->pgrp : 0;
	    fs_info->devfsd_file = file;
	    fs_info->devfsd_info = kmalloc (sizeof *fs_info->devfsd_info,
					    GFP_KERNEL);
	    if (!fs_info->devfsd_info)
	    {
		devfsd_close (inode, file);
		return -ENOMEM;
	    }
	}
	else if (fs_info->devfsd_task != current) return -EBUSY;
	fs_info->devfsd_event_mask = arg;  /*  Let the masses come forth  */
	break;
      case DEVFSDIOC_RELEASE_EVENT_QUEUE:
	if (fs_info->devfsd_file != file) return -EPERM;
	return devfsd_close (inode, file);
	/*break;*/
#ifdef CONFIG_DEVFS_DEBUG
      case DEVFSDIOC_SET_DEBUG_MASK:
	if ( copy_from_user (&ival, (void *) arg, sizeof ival) )return -EFAULT;
	devfs_debug = ival;
	break;
#endif
      default:
	return -ENOIOCTLCMD;
    }
    return 0;
}   /*  End Function devfsd_ioctl  */

static int devfsd_close (struct inode *inode, struct file *file)
{
    struct devfsd_buf_entry *entry, *next;
    struct fs_info *fs_info = inode->i_sb->u.generic_sbp;

    if (fs_info->devfsd_file != file) return 0;
    fs_info->devfsd_event_mask = 0;
    fs_info->devfsd_file = NULL;
    spin_lock (&fs_info->devfsd_buffer_lock);
    entry = fs_info->devfsd_first_event;
    fs_info->devfsd_first_event = NULL;
    fs_info->devfsd_last_event = NULL;
    if (fs_info->devfsd_info)
    {
	kfree (fs_info->devfsd_info);
	fs_info->devfsd_info = NULL;
    }
    spin_unlock (&fs_info->devfsd_buffer_lock);
    fs_info->devfsd_pgrp = 0;
    fs_info->devfsd_task = NULL;
    wake_up (&fs_info->revalidate_wait_queue);
    for (; entry; entry = next)
    {
	next = entry->next;
	kmem_cache_free (devfsd_buf_cache, entry);
    }
    return 0;
}   /*  End Function devfsd_close  */

#ifdef CONFIG_DEVFS_DEBUG
static ssize_t stat_read (struct file *file, char *buf, size_t len,
			  loff_t *ppos)
{
    ssize_t num;
    char txt[80];

    num = sprintf (txt, "Number of entries: %u  number of bytes: %u\n",
		   stat_num_entries, stat_num_bytes) + 1;
    /*  Can't seek (pread) on this device  */
    if (ppos != &file->f_pos) return -ESPIPE;
    if (*ppos >= num) return 0;
    if (*ppos + len > num) len = num - *ppos;
    if ( copy_to_user (buf, txt + *ppos, len) ) return -EFAULT;
    *ppos += len;
    return len;
}   /*  End Function stat_read  */
#endif


static int __init init_devfs_fs (void)
{
    int err;

    printk (KERN_INFO "%s: v%s Richard Gooch (rgooch@atnf.csiro.au)\n",
	    DEVFS_NAME, DEVFS_VERSION);
    devfsd_buf_cache = kmem_cache_create ("devfsd_event",
					  sizeof (struct devfsd_buf_entry),
					  0, 0, NULL, NULL);
    if (!devfsd_buf_cache) OOPS ("(): unable to allocate event slab\n");
#ifdef CONFIG_DEVFS_DEBUG
    devfs_debug = devfs_debug_init;
    printk (KERN_INFO "%s: devfs_debug: 0x%0x\n", DEVFS_NAME, devfs_debug);
#endif
    printk (KERN_INFO "%s: boot_options: 0x%0x\n", DEVFS_NAME, boot_options);
    err = register_filesystem (&devfs_fs_type);
    if (!err)
    {
	struct vfsmount *devfs_mnt = kern_mount (&devfs_fs_type);
	err = PTR_ERR (devfs_mnt);
	if ( !IS_ERR (devfs_mnt) ) err = 0;
    }
    return err;
}   /*  End Function init_devfs_fs  */

void __init mount_devfs_fs (void)
{
    int err;

    if ( !(boot_options & OPTION_MOUNT) ) return;
    err = do_mount ("none", "/dev", "devfs", 0, NULL);
    if (err == 0) printk (KERN_INFO "Mounted devfs on /dev\n");
    else PRINTK ("(): unable to mount devfs, err: %d\n", err);
}   /*  End Function mount_devfs_fs  */

module_init(init_devfs_fs)
