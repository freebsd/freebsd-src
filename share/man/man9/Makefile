#	$Id: Makefile,v 1.33 1998/01/01 10:12:14 bde Exp $

MAN9=	MD5.9 \
	VFS.9 VFS_FHTOVP.9 VFS_INIT.9 VFS_MOUNT.9 VFS_QUOTACTL.9 \
	VFS_ROOT.9 VFS_START.9 VFS_STATFS.9 VFS_SYNC.9 VFS_UNMOUNT.9 \
	VFS_VGET.9 VFS_VPTOFH.9 \
	VOP_ABORTOP.9 VOP_ACCESS.9 VOP_ADVLOCK.9 VOP_ATTRIB.9 \
	VOP_BWRITE.9 VOP_CREATE.9 VOP_FSYNC.9 \
	VOP_GETPAGES.9 VOP_INACTIVE.9 VOP_IOCTL.9 VOP_LINK.9 \
	VOP_LOCK.9 VOP_LOOKUP.9 VOP_MMAP.9 VOP_OPENCLOSE.9 \
	VOP_PATHCONF.9 VOP_PRINT.9 VOP_RDWR.9 VOP_READDIR.9 \
	VOP_READLINK.9 VOP_REALLOCBLKS.9 VOP_REMOVE.9 VOP_RENAME.9 \
	VOP_STRATEGY.9 \
	at_exit.9 at_fork.9 at_shutdown.9 bios.9 boot.9 cd.9 copy.9 \
	devfs_add_devswf.9 devfs_link.9 devfs_remove_dev.9 \
	fetch.9 ifnet.9 inittodr.9 intro.9 kernacc.9 malloc.9 \
	mi_switch.9 panic.9 physio.9 psignal.9 \
	resettodr.9 rtalloc.9 rtentry.9 scsiconf.9 sd.9 sleep.9 spl.9 st.9 \
	store.9 style.9 suser.9 time.9 timeout.9 uio.9 \
	vget.9 vnode.9 vput.9 vref.9 vrele.9 vslock.9

MLINKS+=MD5.9 MD5Init.9 MD5.9 MD5Transform.9
MLINKS+=VOP_ATTRIB.9 VOP_GETATTR.9
MLINKS+=VOP_ATTRIB.9 VOP_SETATTR.9
MLINKS+=VOP_CREATE.9 VOP_MKDIR.9
MLINKS+=VOP_CREATE.9 VOP_MKNOD.9
MLINKS+=VOP_CREATE.9 VOP_SYMLINK.9
MLINKS+=VOP_GETPAGES.9 VOP_PUTPAGES.9
MLINKS+=VOP_INACTIVE.9 VOP_RECLAIM.9
MLINKS+=VOP_LOCK.9 VOP_ISLOCKED.9
MLINKS+=VOP_LOCK.9 VOP_UNLOCK.9
MLINKS+=VOP_OPENCLOSE.9 VOP_CLOSE.9
MLINKS+=VOP_OPENCLOSE.9 VOP_OPEN.9
MLINKS+=VOP_RDWR.9 VOP_READ.9
MLINKS+=VOP_RDWR.9 VOP_WRITE.9
MLINKS+=VOP_REMOVE.9 VOP_RMDIR.9
MLINKS+=at_exit.9 rm_at_exit.9
MLINKS+=at_fork.9 rm_at_fork.9
MLINKS+=at_shutdown.9 rm_at_shutdown.9
MLINKS+=copy.9 copyin.9 copy.9 copyinstr.9 copy.9 copyout.9 copy.9 copystr.9
MLINKS+=fetch.9 fubyte.9 fetch.9 fuswintr.9 fetch.9 fusword.9 fetch.9 fuword.9
MLINKS+=ifnet.9 if_data.9 ifnet.9 ifaddr.9 ifnet.9 ifqueue.9
MLINKS+=kernacc.9 useracc.9
MLINKS+=malloc.9 FREE.9 malloc.9 MALLOC.9 malloc.9 free.9
MLINKS+=mi_switch.9 cpu_switch.9
MLINKS+=psignal.9 gsignal.9 psignal.9 pgsignal.9
MLINKS+=rtalloc.9 rtalloc1.9 rtalloc.9 rtalloc_ign.9
MLINKS+=sleep.9 tsleep.9 sleep.9 wakeup.9 sleep.9 wakeup_one.9
MLINKS+=spl.9 spl0.9
MLINKS+=spl.9 splbio.9 spl.9 splclock.9 spl.9 splhigh.9 spl.9 splimp.9
MLINKS+=spl.9 splnet.9 spl.9 splsoftclock.9 spl.9 splsofttty.9
MLINKS+=spl.9 splstatclock.9 spl.9 spltty.9 spl.9 splvm.9 spl.9 splx.9
MLINKS+=store.9 subyte.9 store.9 suswintr.9 store.9 susword.9 store.9 suword.9
MLINKS+=time.9 boottime.9 time.9 mono_time.9 time.9 runtime.9
MLINKS+=timeout.9 untimeout.9
MLINKS+=vref.9 VREF.9
MLINKS+=vslock.9 vsunlock.9

.include <bsd.prog.mk>
