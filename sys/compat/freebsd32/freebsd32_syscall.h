/*
 * System call numbers.
 *
 * DO NOT EDIT-- this file is automatically @generated.
 */

#define	FREEBSD32_SYS_syscall	0
#define	FREEBSD32_SYS_exit	1
#define	FREEBSD32_SYS_fork	2
#define	FREEBSD32_SYS_read	3
#define	FREEBSD32_SYS_write	4
#define	FREEBSD32_SYS_open	5
#define	FREEBSD32_SYS_close	6
#define	FREEBSD32_SYS_freebsd32_wait4	7
				/* 8 is old creat */
#define	FREEBSD32_SYS_link	9
#define	FREEBSD32_SYS_unlink	10
				/* 11 is obsolete execv */
#define	FREEBSD32_SYS_chdir	12
#define	FREEBSD32_SYS_fchdir	13
#define	FREEBSD32_SYS_freebsd11_mknod	14
#define	FREEBSD32_SYS_chmod	15
#define	FREEBSD32_SYS_chown	16
#define	FREEBSD32_SYS_break	17
				/* 18 is freebsd4 freebsd32_getfsstat */
				/* 19 is old freebsd32_lseek */
#define	FREEBSD32_SYS_getpid	20
#define	FREEBSD32_SYS_mount	21
#define	FREEBSD32_SYS_unmount	22
#define	FREEBSD32_SYS_setuid	23
#define	FREEBSD32_SYS_getuid	24
#define	FREEBSD32_SYS_geteuid	25
#define	FREEBSD32_SYS_freebsd32_ptrace	26
#define	FREEBSD32_SYS_freebsd32_recvmsg	27
#define	FREEBSD32_SYS_freebsd32_sendmsg	28
#define	FREEBSD32_SYS_recvfrom	29
#define	FREEBSD32_SYS_accept	30
#define	FREEBSD32_SYS_getpeername	31
#define	FREEBSD32_SYS_getsockname	32
#define	FREEBSD32_SYS_access	33
#define	FREEBSD32_SYS_chflags	34
#define	FREEBSD32_SYS_fchflags	35
#define	FREEBSD32_SYS_sync	36
#define	FREEBSD32_SYS_kill	37
				/* 38 is old freebsd32_stat */
#define	FREEBSD32_SYS_getppid	39
				/* 40 is old freebsd32_lstat */
#define	FREEBSD32_SYS_dup	41
#define	FREEBSD32_SYS_freebsd10_pipe	42
#define	FREEBSD32_SYS_getegid	43
#define	FREEBSD32_SYS_profil	44
#define	FREEBSD32_SYS_ktrace	45
				/* 46 is old freebsd32_sigaction */
#define	FREEBSD32_SYS_getgid	47
				/* 48 is old sigprocmask */
#define	FREEBSD32_SYS_getlogin	49
#define	FREEBSD32_SYS_setlogin	50
#define	FREEBSD32_SYS_acct	51
				/* 52 is old sigpending */
#define	FREEBSD32_SYS_freebsd32_sigaltstack	53
#define	FREEBSD32_SYS_freebsd32_ioctl	54
#define	FREEBSD32_SYS_reboot	55
#define	FREEBSD32_SYS_revoke	56
#define	FREEBSD32_SYS_symlink	57
#define	FREEBSD32_SYS_readlink	58
#define	FREEBSD32_SYS_freebsd32_execve	59
#define	FREEBSD32_SYS_umask	60
#define	FREEBSD32_SYS_chroot	61
				/* 62 is old freebsd32_fstat */
				/* 63 is obsolete getkerninfo */
				/* 64 is old getpagesize */
#define	FREEBSD32_SYS_msync	65
#define	FREEBSD32_SYS_vfork	66
				/* 67 is obsolete vread */
				/* 68 is obsolete vwrite */
				/* 69 is obsolete sbrk */
				/* 70 is obsolete sstk */
				/* 71 is old freebsd32_mmap */
#define	FREEBSD32_SYS_freebsd11_vadvise	72
#define	FREEBSD32_SYS_munmap	73
#define	FREEBSD32_SYS_freebsd32_mprotect	74
#define	FREEBSD32_SYS_madvise	75
				/* 76 is obsolete vhangup */
				/* 77 is obsolete vlimit */
#define	FREEBSD32_SYS_mincore	78
#define	FREEBSD32_SYS_getgroups	79
#define	FREEBSD32_SYS_setgroups	80
#define	FREEBSD32_SYS_getpgrp	81
#define	FREEBSD32_SYS_setpgid	82
#define	FREEBSD32_SYS_freebsd32_setitimer	83
				/* 84 is old wait */
#define	FREEBSD32_SYS_swapon	85
#define	FREEBSD32_SYS_freebsd32_getitimer	86
				/* 87 is old gethostname */
				/* 88 is old sethostname */
#define	FREEBSD32_SYS_getdtablesize	89
#define	FREEBSD32_SYS_dup2	90
#define	FREEBSD32_SYS_freebsd32_fcntl	92
#define	FREEBSD32_SYS_freebsd32_select	93
#define	FREEBSD32_SYS_fsync	95
#define	FREEBSD32_SYS_setpriority	96
#define	FREEBSD32_SYS_socket	97
#define	FREEBSD32_SYS_connect	98
				/* 99 is old accept */
#define	FREEBSD32_SYS_getpriority	100
				/* 101 is old send */
				/* 102 is old recv */
				/* 103 is old freebsd32_sigreturn */
#define	FREEBSD32_SYS_bind	104
#define	FREEBSD32_SYS_setsockopt	105
#define	FREEBSD32_SYS_listen	106
				/* 107 is obsolete vtimes */
				/* 108 is old freebsd32_sigvec */
				/* 109 is old sigblock */
				/* 110 is old sigsetmask */
				/* 111 is old sigsuspend */
				/* 112 is old freebsd32_sigstack */
				/* 113 is old freebsd32_recvmsg */
				/* 114 is old freebsd32_sendmsg */
				/* 115 is obsolete vtrace */
#define	FREEBSD32_SYS_freebsd32_gettimeofday	116
#define	FREEBSD32_SYS_freebsd32_getrusage	117
#define	FREEBSD32_SYS_getsockopt	118
#define	FREEBSD32_SYS_freebsd32_readv	120
#define	FREEBSD32_SYS_freebsd32_writev	121
#define	FREEBSD32_SYS_freebsd32_settimeofday	122
#define	FREEBSD32_SYS_fchown	123
#define	FREEBSD32_SYS_fchmod	124
				/* 125 is old recvfrom */
#define	FREEBSD32_SYS_setreuid	126
#define	FREEBSD32_SYS_setregid	127
#define	FREEBSD32_SYS_rename	128
				/* 129 is old freebsd32_truncate */
				/* 130 is old freebsd32_ftruncate */
#define	FREEBSD32_SYS_flock	131
#define	FREEBSD32_SYS_mkfifo	132
#define	FREEBSD32_SYS_sendto	133
#define	FREEBSD32_SYS_shutdown	134
#define	FREEBSD32_SYS_socketpair	135
#define	FREEBSD32_SYS_mkdir	136
#define	FREEBSD32_SYS_rmdir	137
#define	FREEBSD32_SYS_freebsd32_utimes	138
				/* 139 is obsolete freebsd32_sigreturn */
#define	FREEBSD32_SYS_freebsd32_adjtime	140
				/* 141 is old getpeername */
				/* 142 is old gethostid */
				/* 143 is old freebsd32_sethostid */
				/* 144 is old getrlimit */
				/* 145 is old setrlimit */
				/* 146 is old killpg */
#define	FREEBSD32_SYS_setsid	147
#define	FREEBSD32_SYS_quotactl	148
				/* 149 is old quota */
				/* 150 is old getsockname */
				/* 156 is old freebsd32_getdirentries */
				/* 157 is freebsd4 freebsd32_statfs */
				/* 158 is freebsd4 freebsd32_fstatfs */
#define	FREEBSD32_SYS_getfh	161
				/* 162 is freebsd4 getdomainname */
				/* 163 is freebsd4 setdomainname */
				/* 164 is freebsd4 uname */
#define	FREEBSD32_SYS_freebsd32_sysarch	165
#define	FREEBSD32_SYS_rtprio	166
#define	FREEBSD32_SYS_freebsd32_semsys	169
#define	FREEBSD32_SYS_freebsd32_msgsys	170
#define	FREEBSD32_SYS_freebsd32_shmsys	171
				/* 173 is freebsd6 freebsd32_pread */
				/* 174 is freebsd6 freebsd32_pwrite */
#define	FREEBSD32_SYS_setfib	175
#define	FREEBSD32_SYS_freebsd32_ntp_adjtime	176
#define	FREEBSD32_SYS_setgid	181
#define	FREEBSD32_SYS_setegid	182
#define	FREEBSD32_SYS_seteuid	183
				/* 184 is obsolete lfs_bmapv */
				/* 185 is obsolete lfs_markv */
				/* 186 is obsolete lfs_segclean */
				/* 187 is obsolete lfs_segwait */
#define	FREEBSD32_SYS_freebsd11_freebsd32_stat	188
#define	FREEBSD32_SYS_freebsd11_freebsd32_fstat	189
#define	FREEBSD32_SYS_freebsd11_freebsd32_lstat	190
#define	FREEBSD32_SYS_pathconf	191
#define	FREEBSD32_SYS_fpathconf	192
#define	FREEBSD32_SYS_getrlimit	194
#define	FREEBSD32_SYS_setrlimit	195
#define	FREEBSD32_SYS_freebsd11_freebsd32_getdirentries	196
				/* 197 is freebsd6 freebsd32_mmap */
#define	FREEBSD32_SYS___syscall	198
				/* 199 is freebsd6 freebsd32_lseek */
				/* 200 is freebsd6 freebsd32_truncate */
				/* 201 is freebsd6 freebsd32_ftruncate */
#define	FREEBSD32_SYS_freebsd32___sysctl	202
#define	FREEBSD32_SYS_mlock	203
#define	FREEBSD32_SYS_munlock	204
#define	FREEBSD32_SYS_undelete	205
#define	FREEBSD32_SYS_freebsd32_futimes	206
#define	FREEBSD32_SYS_getpgid	207
#define	FREEBSD32_SYS_poll	209
#define	FREEBSD32_SYS_freebsd7_freebsd32___semctl	220
#define	FREEBSD32_SYS_semget	221
#define	FREEBSD32_SYS_semop	222
				/* 223 is obsolete semconfig */
#define	FREEBSD32_SYS_freebsd7_freebsd32_msgctl	224
#define	FREEBSD32_SYS_msgget	225
#define	FREEBSD32_SYS_freebsd32_msgsnd	226
#define	FREEBSD32_SYS_freebsd32_msgrcv	227
#define	FREEBSD32_SYS_shmat	228
#define	FREEBSD32_SYS_freebsd7_freebsd32_shmctl	229
#define	FREEBSD32_SYS_shmdt	230
#define	FREEBSD32_SYS_shmget	231
#define	FREEBSD32_SYS_freebsd32_clock_gettime	232
#define	FREEBSD32_SYS_freebsd32_clock_settime	233
#define	FREEBSD32_SYS_freebsd32_clock_getres	234
#define	FREEBSD32_SYS_freebsd32_ktimer_create	235
#define	FREEBSD32_SYS_ktimer_delete	236
#define	FREEBSD32_SYS_freebsd32_ktimer_settime	237
#define	FREEBSD32_SYS_freebsd32_ktimer_gettime	238
#define	FREEBSD32_SYS_ktimer_getoverrun	239
#define	FREEBSD32_SYS_freebsd32_nanosleep	240
#define	FREEBSD32_SYS_ffclock_getcounter	241
#define	FREEBSD32_SYS_freebsd32_ffclock_setestimate	242
#define	FREEBSD32_SYS_freebsd32_ffclock_getestimate	243
#define	FREEBSD32_SYS_freebsd32_clock_nanosleep	244
#define	FREEBSD32_SYS_freebsd32_clock_getcpuclockid2	247
#define	FREEBSD32_SYS_minherit	250
#define	FREEBSD32_SYS_rfork	251
				/* 252 is obsolete openbsd_poll */
#define	FREEBSD32_SYS_issetugid	253
#define	FREEBSD32_SYS_lchown	254
#define	FREEBSD32_SYS_freebsd32_aio_read	255
#define	FREEBSD32_SYS_freebsd32_aio_write	256
#define	FREEBSD32_SYS_freebsd32_lio_listio	257
#define	FREEBSD32_SYS_freebsd11_getdents	272
#define	FREEBSD32_SYS_lchmod	274
				/* 275 is obsolete netbsd_lchown */
#define	FREEBSD32_SYS_freebsd32_lutimes	276
				/* 277 is obsolete netbsd_msync */
#define	FREEBSD32_SYS_freebsd11_freebsd32_nstat	278
#define	FREEBSD32_SYS_freebsd11_freebsd32_nfstat	279
#define	FREEBSD32_SYS_freebsd11_freebsd32_nlstat	280
#define	FREEBSD32_SYS_freebsd32_preadv	289
#define	FREEBSD32_SYS_freebsd32_pwritev	290
				/* 297 is freebsd4 freebsd32_fhstatfs */
#define	FREEBSD32_SYS_fhopen	298
#define	FREEBSD32_SYS_freebsd11_freebsd32_fhstat	299
#define	FREEBSD32_SYS_modnext	300
#define	FREEBSD32_SYS_freebsd32_modstat	301
#define	FREEBSD32_SYS_modfnext	302
#define	FREEBSD32_SYS_modfind	303
#define	FREEBSD32_SYS_kldload	304
#define	FREEBSD32_SYS_kldunload	305
#define	FREEBSD32_SYS_kldfind	306
#define	FREEBSD32_SYS_kldnext	307
#define	FREEBSD32_SYS_freebsd32_kldstat	308
#define	FREEBSD32_SYS_kldfirstmod	309
#define	FREEBSD32_SYS_getsid	310
#define	FREEBSD32_SYS_setresuid	311
#define	FREEBSD32_SYS_setresgid	312
				/* 313 is obsolete signanosleep */
#define	FREEBSD32_SYS_freebsd32_aio_return	314
#define	FREEBSD32_SYS_freebsd32_aio_suspend	315
#define	FREEBSD32_SYS_aio_cancel	316
#define	FREEBSD32_SYS_freebsd32_aio_error	317
				/* 318 is freebsd6 freebsd32_aio_read */
				/* 319 is freebsd6 freebsd32_aio_write */
				/* 320 is freebsd6 freebsd32_lio_listio */
#define	FREEBSD32_SYS_yield	321
				/* 322 is obsolete thr_sleep */
				/* 323 is obsolete thr_wakeup */
#define	FREEBSD32_SYS_mlockall	324
#define	FREEBSD32_SYS_munlockall	325
#define	FREEBSD32_SYS___getcwd	326
#define	FREEBSD32_SYS_sched_setparam	327
#define	FREEBSD32_SYS_sched_getparam	328
#define	FREEBSD32_SYS_sched_setscheduler	329
#define	FREEBSD32_SYS_sched_getscheduler	330
#define	FREEBSD32_SYS_sched_yield	331
#define	FREEBSD32_SYS_sched_get_priority_max	332
#define	FREEBSD32_SYS_sched_get_priority_min	333
#define	FREEBSD32_SYS_freebsd32_sched_rr_get_interval	334
#define	FREEBSD32_SYS_utrace	335
				/* 336 is freebsd4 freebsd32_sendfile */
#define	FREEBSD32_SYS_freebsd32_jail	338
#define	FREEBSD32_SYS_sigprocmask	340
#define	FREEBSD32_SYS_sigsuspend	341
				/* 342 is freebsd4 freebsd32_sigaction */
#define	FREEBSD32_SYS_sigpending	343
				/* 344 is freebsd4 freebsd32_sigreturn */
#define	FREEBSD32_SYS_freebsd32_sigtimedwait	345
#define	FREEBSD32_SYS_freebsd32_sigwaitinfo	346
#define	FREEBSD32_SYS___acl_get_file	347
#define	FREEBSD32_SYS___acl_set_file	348
#define	FREEBSD32_SYS___acl_get_fd	349
#define	FREEBSD32_SYS___acl_set_fd	350
#define	FREEBSD32_SYS___acl_delete_file	351
#define	FREEBSD32_SYS___acl_delete_fd	352
#define	FREEBSD32_SYS___acl_aclcheck_file	353
#define	FREEBSD32_SYS___acl_aclcheck_fd	354
#define	FREEBSD32_SYS_extattrctl	355
#define	FREEBSD32_SYS_extattr_set_file	356
#define	FREEBSD32_SYS_extattr_get_file	357
#define	FREEBSD32_SYS_extattr_delete_file	358
#define	FREEBSD32_SYS_freebsd32_aio_waitcomplete	359
#define	FREEBSD32_SYS_getresuid	360
#define	FREEBSD32_SYS_getresgid	361
#define	FREEBSD32_SYS_kqueue	362
#define	FREEBSD32_SYS_freebsd11_freebsd32_kevent	363
				/* 364 is obsolete __cap_get_proc */
				/* 365 is obsolete __cap_set_proc */
				/* 366 is obsolete __cap_get_fd */
				/* 367 is obsolete __cap_get_file */
				/* 368 is obsolete __cap_set_fd */
				/* 369 is obsolete __cap_set_file */
#define	FREEBSD32_SYS_extattr_set_fd	371
#define	FREEBSD32_SYS_extattr_get_fd	372
#define	FREEBSD32_SYS_extattr_delete_fd	373
#define	FREEBSD32_SYS___setugid	374
				/* 375 is obsolete nfsclnt */
#define	FREEBSD32_SYS_eaccess	376
#define	FREEBSD32_SYS_freebsd32_nmount	378
				/* 379 is obsolete kse_exit */
				/* 380 is obsolete kse_wakeup */
				/* 381 is obsolete kse_create */
				/* 382 is obsolete kse_thr_interrupt */
				/* 383 is obsolete kse_release */
#define	FREEBSD32_SYS_kenv	390
#define	FREEBSD32_SYS_lchflags	391
#define	FREEBSD32_SYS_uuidgen	392
#define	FREEBSD32_SYS_freebsd32_sendfile	393
#define	FREEBSD32_SYS_mac_syscall	394
#define	FREEBSD32_SYS_freebsd11_freebsd32_getfsstat	395
#define	FREEBSD32_SYS_freebsd11_statfs	396
#define	FREEBSD32_SYS_freebsd11_fstatfs	397
#define	FREEBSD32_SYS_freebsd11_fhstatfs	398
#define	FREEBSD32_SYS_ksem_close	400
#define	FREEBSD32_SYS_ksem_post	401
#define	FREEBSD32_SYS_ksem_wait	402
#define	FREEBSD32_SYS_ksem_trywait	403
#define	FREEBSD32_SYS_freebsd32_ksem_init	404
#define	FREEBSD32_SYS_freebsd32_ksem_open	405
#define	FREEBSD32_SYS_ksem_unlink	406
#define	FREEBSD32_SYS_ksem_getvalue	407
#define	FREEBSD32_SYS_ksem_destroy	408
#define	FREEBSD32_SYS_extattr_set_link	412
#define	FREEBSD32_SYS_extattr_get_link	413
#define	FREEBSD32_SYS_extattr_delete_link	414
#define	FREEBSD32_SYS_freebsd32_sigaction	416
#define	FREEBSD32_SYS_freebsd32_sigreturn	417
#define	FREEBSD32_SYS_freebsd32_getcontext	421
#define	FREEBSD32_SYS_freebsd32_setcontext	422
#define	FREEBSD32_SYS_freebsd32_swapcontext	423
#define	FREEBSD32_SYS_freebsd13_swapoff	424
#define	FREEBSD32_SYS___acl_get_link	425
#define	FREEBSD32_SYS___acl_set_link	426
#define	FREEBSD32_SYS___acl_delete_link	427
#define	FREEBSD32_SYS___acl_aclcheck_link	428
#define	FREEBSD32_SYS_sigwait	429
#define	FREEBSD32_SYS_thr_exit	431
#define	FREEBSD32_SYS_thr_self	432
#define	FREEBSD32_SYS_thr_kill	433
#define	FREEBSD32_SYS_freebsd10_freebsd32__umtx_lock	434
#define	FREEBSD32_SYS_freebsd10_freebsd32__umtx_unlock	435
#define	FREEBSD32_SYS_jail_attach	436
#define	FREEBSD32_SYS_extattr_list_fd	437
#define	FREEBSD32_SYS_extattr_list_file	438
#define	FREEBSD32_SYS_extattr_list_link	439
				/* 440 is obsolete kse_switchin */
#define	FREEBSD32_SYS_freebsd32_ksem_timedwait	441
#define	FREEBSD32_SYS_freebsd32_thr_suspend	442
#define	FREEBSD32_SYS_thr_wake	443
#define	FREEBSD32_SYS_kldunloadf	444
#define	FREEBSD32_SYS_audit	445
#define	FREEBSD32_SYS_auditon	446
#define	FREEBSD32_SYS_getauid	447
#define	FREEBSD32_SYS_setauid	448
#define	FREEBSD32_SYS_getaudit	449
#define	FREEBSD32_SYS_setaudit	450
#define	FREEBSD32_SYS_getaudit_addr	451
#define	FREEBSD32_SYS_setaudit_addr	452
#define	FREEBSD32_SYS_auditctl	453
#define	FREEBSD32_SYS_freebsd32__umtx_op	454
#define	FREEBSD32_SYS_freebsd32_thr_new	455
#define	FREEBSD32_SYS_freebsd32_sigqueue	456
#define	FREEBSD32_SYS_freebsd32_kmq_open	457
#define	FREEBSD32_SYS_freebsd32_kmq_setattr	458
#define	FREEBSD32_SYS_freebsd32_kmq_timedreceive	459
#define	FREEBSD32_SYS_freebsd32_kmq_timedsend	460
#define	FREEBSD32_SYS_freebsd32_kmq_notify	461
#define	FREEBSD32_SYS_kmq_unlink	462
#define	FREEBSD32_SYS_freebsd32_abort2	463
#define	FREEBSD32_SYS_thr_set_name	464
#define	FREEBSD32_SYS_freebsd32_aio_fsync	465
#define	FREEBSD32_SYS_rtprio_thread	466
#define	FREEBSD32_SYS_sctp_peeloff	471
#define	FREEBSD32_SYS_sctp_generic_sendmsg	472
#define	FREEBSD32_SYS_sctp_generic_sendmsg_iov	473
#define	FREEBSD32_SYS_sctp_generic_recvmsg	474
#define	FREEBSD32_SYS_freebsd32_pread	475
#define	FREEBSD32_SYS_freebsd32_pwrite	476
#define	FREEBSD32_SYS_freebsd32_mmap	477
#define	FREEBSD32_SYS_freebsd32_lseek	478
#define	FREEBSD32_SYS_freebsd32_truncate	479
#define	FREEBSD32_SYS_freebsd32_ftruncate	480
#define	FREEBSD32_SYS_thr_kill2	481
#define	FREEBSD32_SYS_freebsd12_shm_open	482
#define	FREEBSD32_SYS_shm_unlink	483
#define	FREEBSD32_SYS_cpuset	484
#define	FREEBSD32_SYS_freebsd32_cpuset_setid	485
#define	FREEBSD32_SYS_freebsd32_cpuset_getid	486
#define	FREEBSD32_SYS_freebsd32_cpuset_getaffinity	487
#define	FREEBSD32_SYS_freebsd32_cpuset_setaffinity	488
#define	FREEBSD32_SYS_faccessat	489
#define	FREEBSD32_SYS_fchmodat	490
#define	FREEBSD32_SYS_fchownat	491
#define	FREEBSD32_SYS_freebsd32_fexecve	492
#define	FREEBSD32_SYS_freebsd11_freebsd32_fstatat	493
#define	FREEBSD32_SYS_freebsd32_futimesat	494
#define	FREEBSD32_SYS_linkat	495
#define	FREEBSD32_SYS_mkdirat	496
#define	FREEBSD32_SYS_mkfifoat	497
#define	FREEBSD32_SYS_freebsd11_mknodat	498
#define	FREEBSD32_SYS_openat	499
#define	FREEBSD32_SYS_readlinkat	500
#define	FREEBSD32_SYS_renameat	501
#define	FREEBSD32_SYS_symlinkat	502
#define	FREEBSD32_SYS_unlinkat	503
#define	FREEBSD32_SYS_posix_openpt	504
				/* 505 is obsolete kgssapi */
#define	FREEBSD32_SYS_freebsd32_jail_get	506
#define	FREEBSD32_SYS_freebsd32_jail_set	507
#define	FREEBSD32_SYS_jail_remove	508
#define	FREEBSD32_SYS_freebsd12_closefrom	509
#define	FREEBSD32_SYS_freebsd32___semctl	510
#define	FREEBSD32_SYS_freebsd32_msgctl	511
#define	FREEBSD32_SYS_freebsd32_shmctl	512
#define	FREEBSD32_SYS_lpathconf	513
				/* 514 is obsolete cap_new */
#define	FREEBSD32_SYS___cap_rights_get	515
#define	FREEBSD32_SYS_cap_enter	516
#define	FREEBSD32_SYS_cap_getmode	517
#define	FREEBSD32_SYS_pdfork	518
#define	FREEBSD32_SYS_pdkill	519
#define	FREEBSD32_SYS_pdgetpid	520
#define	FREEBSD32_SYS_freebsd32_pselect	522
#define	FREEBSD32_SYS_getloginclass	523
#define	FREEBSD32_SYS_setloginclass	524
#define	FREEBSD32_SYS_rctl_get_racct	525
#define	FREEBSD32_SYS_rctl_get_rules	526
#define	FREEBSD32_SYS_rctl_get_limits	527
#define	FREEBSD32_SYS_rctl_add_rule	528
#define	FREEBSD32_SYS_rctl_remove_rule	529
#define	FREEBSD32_SYS_freebsd32_posix_fallocate	530
#define	FREEBSD32_SYS_freebsd32_posix_fadvise	531
#define	FREEBSD32_SYS_freebsd32_wait6	532
#define	FREEBSD32_SYS_cap_rights_limit	533
#define	FREEBSD32_SYS_freebsd32_cap_ioctls_limit	534
#define	FREEBSD32_SYS_freebsd32_cap_ioctls_get	535
#define	FREEBSD32_SYS_cap_fcntls_limit	536
#define	FREEBSD32_SYS_cap_fcntls_get	537
#define	FREEBSD32_SYS_bindat	538
#define	FREEBSD32_SYS_connectat	539
#define	FREEBSD32_SYS_chflagsat	540
#define	FREEBSD32_SYS_accept4	541
#define	FREEBSD32_SYS_pipe2	542
#define	FREEBSD32_SYS_freebsd32_aio_mlock	543
#define	FREEBSD32_SYS_freebsd32_procctl	544
#define	FREEBSD32_SYS_freebsd32_ppoll	545
#define	FREEBSD32_SYS_freebsd32_futimens	546
#define	FREEBSD32_SYS_freebsd32_utimensat	547
				/* 548 is obsolete numa_getaffinity */
				/* 549 is obsolete numa_setaffinity */
#define	FREEBSD32_SYS_fdatasync	550
#define	FREEBSD32_SYS_freebsd32_fstat	551
#define	FREEBSD32_SYS_freebsd32_fstatat	552
#define	FREEBSD32_SYS_freebsd32_fhstat	553
#define	FREEBSD32_SYS_getdirentries	554
#define	FREEBSD32_SYS_statfs	555
#define	FREEBSD32_SYS_fstatfs	556
#define	FREEBSD32_SYS_freebsd32_getfsstat	557
#define	FREEBSD32_SYS_fhstatfs	558
#define	FREEBSD32_SYS_freebsd32_mknodat	559
#define	FREEBSD32_SYS_freebsd32_kevent	560
#define	FREEBSD32_SYS_freebsd32_cpuset_getdomain	561
#define	FREEBSD32_SYS_freebsd32_cpuset_setdomain	562
#define	FREEBSD32_SYS_getrandom	563
#define	FREEBSD32_SYS_getfhat	564
#define	FREEBSD32_SYS_fhlink	565
#define	FREEBSD32_SYS_fhlinkat	566
#define	FREEBSD32_SYS_fhreadlink	567
#define	FREEBSD32_SYS_funlinkat	568
#define	FREEBSD32_SYS_copy_file_range	569
#define	FREEBSD32_SYS_freebsd32___sysctlbyname	570
#define	FREEBSD32_SYS_shm_open2	571
#define	FREEBSD32_SYS_shm_rename	572
#define	FREEBSD32_SYS_sigfastblock	573
#define	FREEBSD32_SYS___realpathat	574
#define	FREEBSD32_SYS_close_range	575
#define	FREEBSD32_SYS_rpctls_syscall	576
#define	FREEBSD32_SYS___specialfd	577
#define	FREEBSD32_SYS_freebsd32_aio_writev	578
#define	FREEBSD32_SYS_freebsd32_aio_readv	579
#define	FREEBSD32_SYS_fspacectl	580
#define	FREEBSD32_SYS_sched_getcpu	581
#define	FREEBSD32_SYS_swapoff	582
#define	FREEBSD32_SYS_kqueuex	583
#define	FREEBSD32_SYS_membarrier	584
#define	FREEBSD32_SYS_timerfd_create	585
#define	FREEBSD32_SYS_freebsd32_timerfd_gettime	586
#define	FREEBSD32_SYS_freebsd32_timerfd_settime	587
#define	FREEBSD32_SYS_kcmp	588
#define	FREEBSD32_SYS_getrlimitusage	589
#define	FREEBSD32_SYS_fchroot	590
#define	FREEBSD32_SYS_freebsd32_setcred	591
#define	FREEBSD32_SYS_exterrctl	592
#define	FREEBSD32_SYS_inotify_add_watch_at	593
#define	FREEBSD32_SYS_inotify_rm_watch	594
#define	FREEBSD32_SYS_MAXSYSCALL	595
