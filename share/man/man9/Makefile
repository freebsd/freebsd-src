# $FreeBSD$

MAN=	CONDSPLASSERT.9 DECLARE_MODULE.9 DELAY.9 DEV_MODULE.9 KASSERT.9 MD5.9 \
	MODULE_DEPEND.9 MODULE_VERSION.9 SPLASSERT.9 \
	VFS.9 VFS_FHTOVP.9 VFS_INIT.9 VFS_MOUNT.9 VFS_QUOTACTL.9 \
	VFS_ROOT.9 VFS_START.9 VFS_STATFS.9 VFS_SYNC.9 VFS_UNMOUNT.9 \
	VFS_VGET.9 VFS_VPTOFH.9 VFS_CHECKEXP.9 \
	VOP_ACCESS.9 VOP_ADVLOCK.9 VOP_ATTRIB.9 \
	VOP_BWRITE.9 VOP_CREATE.9 VOP_FSYNC.9 VOP_LEASE.9 \
	VOP_GETPAGES.9 VOP_GETVOBJECT.9 VOP_INACTIVE.9 VOP_IOCTL.9 VOP_LINK.9 \
	VOP_LOCK.9 VOP_LOOKUP.9 VOP_OPENCLOSE.9 \
	VOP_PATHCONF.9 VOP_PRINT.9 VOP_RDWR.9 VOP_READDIR.9 \
	VOP_READLINK.9 VOP_REALLOCBLKS.9 VOP_REMOVE.9 VOP_RENAME.9 \
	VOP_STRATEGY.9 \
	accf_data.9 accf_http.9 at_exit.9 at_fork.9 atomic.9 \
	bios.9 boot.9 buf.9 \
	cd.9 condvar.9 copy.9 critical_enter.9 \
	devfs_add_devswf.9 devfs_link.9 devfs_remove_dev.9 devstat.9 \
	devtoname.9 \
	fetch.9 \
	get_cyclecounter.9 \
	ifnet.9 inittodr.9 intro.9 ithread.9 \
	kernacc.9 kthread.9 \
	malloc.9 make_dev.9 mbuf.9 mbchain.9 mdchain.9 \
	microseq.9 microtime.9 microuptime.9 \
	mi_switch.9 module.9 mutex.9 \
	namei.9 \
	panic.9 pfil.9 physio.9 posix4.9 psignal.9 \
	random.9 resettodr.9 rtalloc.9 rtentry.9 runqueue.9 \
	scheduler.9 sleep.9 sleepqueue.9 spl.9 store.9 style.9 suser.9 swi.9 \
	sx.9 time.9 timeout.9 tvtohz.9 \
	uio.9 \
	vcount.9 vget.9 vnode.9 vput.9 vref.9 vrele.9 vslock.9

MAN+=	device.9 device_add_child.9 device_delete_child.9 device_enable.9 \
	device_find_child.9 device_get_children.9 \
	device_get_devclass.9 device_get_driver.9 \
	device_get_ivars.9 device_get_softc.9 device_get_state.9 \
	device_get_unit.9 device_probe_and_attach.9 device_quiet.9 \
	device_set_desc.9 device_set_flags.9 driver.9 \
	devclass.9 devclass_add_driver.9 devclass_find.9 \
	devclass_get_device.9 devclass_get_devices.9 devclass_get_maxunit.9 \
	devclass_get_name.9 devclass_get_softc.9 \
	DEVICE_ATTACH.9 DEVICE_DETACH.9 DEVICE_IDENTIFY.9 DEVICE_PROBE.9 \
	DEVICE_SHUTDOWN.9 \
	DRIVER_MODULE.9 BUS_PRINT_CHILD.9 BUS_SETUP_INTR.9 \
	BUS_READ_IVAR.9 \
	bus_generic_attach.9 bus_generic_detach.9 bus_generic_map_intr.9 \
	bus_generic_print_child.9 bus_generic_read_ivar.9 \
	bus_generic_shutdown.9 \
	bus_alloc_resource.9 bus_release_resource.9 \
	VOP_ACLCHECK.9 VOP_GETACL.9 VOP_GETEXTATTR.9 VOP_SETACL.9 \
	VOP_SETEXTATTR.9 acl.9 extattr.9 kobj.9 taskqueue.9 accept_filter.9 \
	sbuf.9 resource_int_value.9 resource_query_string.9 \
	zone.9 \
	sysctl_add_oid.9 sysctl_ctx_init.9

MLINKS+=MD5.9 MD5Init.9 MD5.9 MD5Transform.9
MLINKS+=VOP_ATTRIB.9 VOP_GETATTR.9
MLINKS+=VOP_ATTRIB.9 VOP_SETATTR.9
MLINKS+=VOP_CREATE.9 VOP_MKDIR.9
MLINKS+=VOP_CREATE.9 VOP_MKNOD.9
MLINKS+=VOP_CREATE.9 VOP_SYMLINK.9
MLINKS+=VOP_GETPAGES.9 VOP_PUTPAGES.9
MLINKS+=VOP_GETVOBJECT.9 VOP_CREATEVOBJECT.9
MLINKS+=VOP_GETVOBJECT.9 VOP_DESTROYVOBJECT.9
MLINKS+=VOP_INACTIVE.9 VOP_RECLAIM.9
MLINKS+=VOP_LOCK.9 VOP_ISLOCKED.9
MLINKS+=VOP_LOCK.9 VOP_UNLOCK.9
MLINKS+=VOP_LOCK.9 vn_lock.9
MLINKS+=VOP_OPENCLOSE.9 VOP_CLOSE.9
MLINKS+=VOP_OPENCLOSE.9 VOP_OPEN.9
MLINKS+=VOP_RDWR.9 VOP_READ.9
MLINKS+=VOP_RDWR.9 VOP_WRITE.9
MLINKS+=VOP_REMOVE.9 VOP_RMDIR.9
MLINKS+=atomic.9 atomic_add.9
MLINKS+=atomic.9 atomic_clear.9
MLINKS+=atomic.9 atomic_cmpset.9
MLINKS+=atomic.9 atomic_load.9
MLINKS+=atomic.9 atomic_readandclear.9
MLINKS+=atomic.9 atomic_set.9
MLINKS+=atomic.9 atomic_subtract.9
MLINKS+=atomic.9 atomic_store.9
MLINKS+=at_exit.9 rm_at_exit.9
MLINKS+=at_fork.9 rm_at_fork.9
MLINKS+=condvar.9 cv_init.9
MLINKS+=condvar.9 cv_destroy.9
MLINKS+=condvar.9 cv_wait.9
MLINKS+=condvar.9 cv_wait_sig.9
MLINKS+=condvar.9 cv_timedwait.9
MLINKS+=condvar.9 cv_timedwait_sig.9
MLINKS+=condvar.9 cv_signal.9
MLINKS+=condvar.9 cv_broadcast.9
MLINKS+=condvar.9 cv_waitq_remove.9
MLINKS+=condvar.9 cv_waitq_empty.9
MLINKS+=condvar.9 cv_wmesg.9
MLINKS+=copy.9 copyin.9 copy.9 copyinstr.9 copy.9 copyout.9 copy.9 copystr.9
MLINKS+=critical_enter.9 critical_exit.9
MLINKS+=devfs_link.9 devfs_makelink.9
MLINKS+=devstat.9 devicestat.9 devstat.9 devstat_add_entry.9 
MLINKS+=devstat.9 devstat_remove_entry.9 devstat.9 devstat_start_transaction.9
MLINKS+=devstat.9 devstat_end_transaction.9
MLINKS+=fetch.9 fubyte.9 fetch.9 fuswintr.9 fetch.9 fusword.9 fetch.9 fuword.9
MLINKS+=ifnet.9 if_data.9 ifnet.9 ifaddr.9 ifnet.9 ifqueue.9
MLINKS+=ithread.9 ithread_add_handler.9 ithread.9 ithread_create.9
MLINKS+=ithread.9 ithread_destroy.9 ithread.9 ithread_priority.9
MLINKS+=ithread.9 ithread_remove_handler.9 ithread.9 ithread_schedule.9
MLINKS+=kernacc.9 useracc.9
MLINKS+=kthread.9 kproc_start.9 kthread.9 kproc_shutdown.9
MLINKS+=kthread.9 kthread_create.9 kthread.9 kthread_exit.9
MLINKS+=kthread.9 kthread_resume.9 kthread.9 kthread_suspend.9
MLINKS+=kthread.9 kthread_suspend_check.9
MLINKS+=make_dev.9 destroy_dev.9
MLINKS+=malloc.9 FREE.9 malloc.9 MALLOC.9 malloc.9 free.9
MLINKS+=mi_switch.9 cpu_switch.9 mi_switch.9 cpu_throw.9
MLINKS+=namei.9 NDINIT.9
MLINKS+=namei.9 NDFREE.9
MLINKS+=pfil.9 pfil_hook_get.9 pfil.9 pfil_add_hook.9 pfil.9 pfil_remove_hook.9
MLINKS+=posix4.9 p1003_1b.9
MLINKS+=psignal.9 gsignal.9 psignal.9 pgsignal.9
MLINKS+=random.9 srandom.9 random.9 arc4random.9 random.9 read_random.9
MLINKS+=rtalloc.9 rtalloc1.9 rtalloc.9 rtalloc_ign.9
MLINKS+=runqueue.9 chooseproc.9 runqueue.9 procrunnable.9
MLINKS+=runqueue.9 remrunqueue.9 runqueue.9 setrunqueue.9
MLINKS+=scheduler.9 curpriority_cmp.9 scheduler.9 maybe_resched.9
MLINKS+=scheduler.9 resetpriority.9 scheduler.9 roundrobin.9
MLINKS+=scheduler.9 roundrobin_interval.9 scheduler.9 sched_setup.9
MLINKS+=scheduler.9 schedclock.9 scheduler.9 schedcpu.9
MLINKS+=scheduler.9 setrunnable.9 scheduler.9 updatepri.9
MLINKS+=sleep.9 tsleep.9 sleep.9 wakeup.9 sleep.9 wakeup_one.9
MLINKS+=sleep.9 asleep.9 sleep.9 await.9 sleep.9 msleep.9
MLINKS+=sleepqueue.9 endtsleep.9 sleepqueue.9 sleepinit.9
MLINKS+=sleepqueue.9 unsleep.9
MLINKS+=buf.9 bp.9
MLINKS+=spl.9 spl0.9
MLINKS+=spl.9 splbio.9 spl.9 splclock.9 spl.9 splhigh.9 spl.9 splimp.9
MLINKS+=spl.9 splnet.9 spl.9 splsoftclock.9 spl.9 splsofttty.9
MLINKS+=spl.9 splstatclock.9 spl.9 spltty.9 spl.9 splvm.9 spl.9 splx.9
MLINKS+=store.9 subyte.9 store.9 suswintr.9 store.9 susword.9 store.9 suword.9
MLINKS+=swi.9 swi_sched.9 swi.9 swi_add.9
MLINKS+=sx.9 sx_init.9
MLINKS+=sx.9 sx_destroy.9
MLINKS+=sx.9 sx_slock.9
MLINKS+=sx.9 sx_xlock.9
MLINKS+=sx.9 sx_sunlock.9
MLINKS+=sx.9 sx_xunlock.9
MLINKS+=time.9 boottime.9 time.9 mono_time.9 time.9 runtime.9
MLINKS+=timeout.9 untimeout.9
MLINKS+=timeout.9 callout_handle_init.9 timeout.9 callout_init.9
MLINKS+=timeout.9 callout_stop.9 timeout.9 callout_reset.9
MLINKS+=uio.9 uiomove.9
MLINKS+=vref.9 VREF.9
MLINKS+=vslock.9 vsunlock.9

MLINKS+=device_add_child.9 device_add_child_ordered.9
MLINKS+=device_enable.9 device_disable.9
MLINKS+=device_enable.9 device_is_enabled.9
MLINKS+=device_get_state.9 device_busy.9
MLINKS+=device_get_state.9 device_unbusy.9
MLINKS+=device_get_state.9 device_is_alive.9
MLINKS+=device_quiet.9 device_verbose.9
MLINKS+=device_quiet.9 device_is_quiet.9
MLINKS+=device_set_desc.9 device_get_desc.9
MLINKS+=device_set_desc.9 device_set_desc_copy.9
MLINKS+=device_set_flags.9 device_get_flags.9
MLINKS+=devclass_add_driver.9 devclass_delete_driver.9
MLINKS+=devclass_add_driver.9 devclass_find_driver.9
MLINKS+=BUS_READ_IVAR.9 BUS_WRITE_IVAR.9
MLINKS+=BUS_SETUP_INTR.9 bus_setup_intr.9
MLINKS+=BUS_SETUP_INTR.9 BUS_TEARDOWN_INTR.9
MLINKS+=BUS_SETUP_INTR.9 bus_teardown_intr.9
MLINKS+=bus_generic_read_ivar.9 bus_generic_write_ivar.9

MLINKS+=mbchain.9 mb_init.9
MLINKS+=mbchain.9 mb_initm.9
MLINKS+=mbchain.9 mb_done.9
MLINKS+=mbchain.9 mb_detach.9
MLINKS+=mbchain.9 mb_fixhdr.9
MLINKS+=mbchain.9 mb_reserve.9
MLINKS+=mbchain.9 mb_put_uint8.9
MLINKS+=mbchain.9 mb_put_uint16be.9
MLINKS+=mbchain.9 mb_put_uint16le.9
MLINKS+=mbchain.9 mb_put_uint32be.9
MLINKS+=mbchain.9 mb_put_uint32le.9
MLINKS+=mbchain.9 mb_put_int64be.9
MLINKS+=mbchain.9 mb_put_int64le.9
MLINKS+=mbchain.9 mb_put_mem.9
MLINKS+=mbchain.9 mb_put_mbuf.9
MLINKS+=mbchain.9 mb_put_uio.9

MLINKS+=mdchain.9 md_initm.9
MLINKS+=mdchain.9 md_done.9
MLINKS+=mdchain.9 md_append_record.9
MLINKS+=mdchain.9 md_next_record.9
MLINKS+=mdchain.9 md_get_uint8.9
MLINKS+=mdchain.9 md_get_uint16.9
MLINKS+=mdchain.9 md_get_uint16be.9
MLINKS+=mdchain.9 md_get_uint16le.9
MLINKS+=mdchain.9 md_get_uint32.9
MLINKS+=mdchain.9 md_get_uint32be.9
MLINKS+=mdchain.9 md_get_uint32le.9
MLINKS+=mdchain.9 md_get_int64.9
MLINKS+=mdchain.9 md_get_int64be.9
MLINKS+=mdchain.9 md_get_int64le.9
MLINKS+=mdchain.9 md_get_mem.9
MLINKS+=mdchain.9 md_get_mbuf.9
MLINKS+=mdchain.9 md_get_uio.9

MLINKS+=microtime.9 getmicrotime.9 microtime.9 nanotime.9
MLINKS+=microtime.9 getnanotime.9
MLINKS+=microuptime.9 getmicrouptime.9 microuptime.9 nanouptime.9
MLINKS+=microuptime.9 getnanouptime.9

MLINKS+=mutex.9 mtx_init.9
MLINKS+=mutex.9 mtx_enter.9
MLINKS+=mutex.9 mtx_try_enter.9
MLINKS+=mutex.9 mtx_exit.9
MLINKS+=mutex.9 mtx_destroy.9
MLINKS+=mutex.9 mtx_owned.9
MLINKS+=mutex.9 mtx_assert.9

MLINKS+=kobj.9 kobj_class_compile.9
MLINKS+=kobj.9 kobj_class_compile_static.9
MLINKS+=kobj.9 kobj_class_free.9
MLINKS+=kobj.9 kobj_create.9
MLINKS+=kobj.9 kobj_init.9
MLINKS+=kobj.9 kobj_delete.9
MLINKS+=kobj.9 DEFINE_CLASS.9

MLINKS+=taskqueue.9 taskqueue_create.9
MLINKS+=taskqueue.9 taskqueue_free.9
MLINKS+=taskqueue.9 taskqueue_find.9
MLINKS+=taskqueue.9 taskqueue_enqueue.9
MLINKS+=taskqueue.9 taskqueue_run.9
MLINKS+=taskqueue.9 TASK_INIT.9
MLINKS+=taskqueue.9 TASKQUEUE_DECLARE.9
MLINKS+=taskqueue.9 TASKQUEUE_DEFINE.9

MLINKS+=resource_int_value.9 resource_long_value.9
MLINKS+=resource_int_value.9 resource_string_value.9
MLINKS+=resource_query_string.9 resource_query_name.9
MLINKS+=resource_query_string.9 resource_query_unit.9

MLINKS+=suser.9 suser_xxx.9

MLINKS+=sysctl_add_oid.9 sysctl_remove_oid.9
MLINKS+=sysctl_add_oid.9 SYSCTL_ADD_OID.9
MLINKS+=sysctl_add_oid.9 SYSCTL_ADD_NODE.9
MLINKS+=sysctl_add_oid.9 SYSCTL_ADD_STRING.9
MLINKS+=sysctl_add_oid.9 SYSCTL_ADD_INT.9
MLINKS+=sysctl_add_oid.9 SYSCTL_ADD_UINT.9
MLINKS+=sysctl_add_oid.9 SYSCTL_ADD_LONG.9
MLINKS+=sysctl_add_oid.9 SYSCTL_ADD_ULONG.9
MLINKS+=sysctl_add_oid.9 SYSCTL_ADD_OPAQUE.9
MLINKS+=sysctl_add_oid.9 SYSCTL_ADD_STRUCT.9
MLINKS+=sysctl_add_oid.9 SYSCTL_ADD_PROC.9
MLINKS+=sysctl_ctx_init.9 sysctl_ctx_free.9
MLINKS+=sysctl_ctx_init.9 sysctl_ctx_entry_add.9
MLINKS+=sysctl_ctx_init.9 sysctl_ctx_entry_del.9
MLINKS+=sysctl_ctx_init.9 sysctl_ctx_entry_find.9

MLINKS+=sbuf.9 sbuf_new.9
MLINKS+=sbuf.9 sbuf_clear.9
MLINKS+=sbuf.9 sbuf_setpos.9
MLINKS+=sbuf.9 sbuf_cat.9
MLINKS+=sbuf.9 sbuf_cpy.9
MLINKS+=sbuf.9 sbuf_printf.9
MLINKS+=sbuf.9 sbuf_putc.9
MLINKS+=sbuf.9 sbuf_overflowed.9
MLINKS+=sbuf.9 sbuf_finish.9
MLINKS+=sbuf.9 sbuf_data.9
MLINKS+=sbuf.9 sbuf_len.9
MLINKS+=sbuf.9 sbuf_delete.9

MLINKS+=vcount.9 count_dev.9

MLINKS+=zone.9 zalloc.9
MLINKS+=zone.9 zfree.9
MLINKS+=zone.9 zinit.9
MLINKS+=zone.9 zinitna.9
MLINKS+=zone.9 zbootinit.9

.include <bsd.prog.mk>
