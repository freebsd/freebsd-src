#ifndef _ASM_PARISC_UNISTD_H_
#define _ASM_PARISC_UNISTD_H_

/*
 * This file contains the system call numbers.
 */

/*
 *   HP-UX system calls get their native numbers for binary compatibility.
 */

#define __NR_HPUX_exit                    1
#define __NR_HPUX_fork                    2
#define __NR_HPUX_read                    3
#define __NR_HPUX_write                   4
#define __NR_HPUX_open                    5
#define __NR_HPUX_close                   6
#define __NR_HPUX_wait                    7
#define __NR_HPUX_creat                   8
#define __NR_HPUX_link                    9
#define __NR_HPUX_unlink                 10
#define __NR_HPUX_execv                  11
#define __NR_HPUX_chdir                  12
#define __NR_HPUX_time                   13
#define __NR_HPUX_mknod                  14
#define __NR_HPUX_chmod                  15
#define __NR_HPUX_chown                  16
#define __NR_HPUX_break                  17
#define __NR_HPUX_lchmod                 18
#define __NR_HPUX_lseek                  19
#define __NR_HPUX_getpid                 20
#define __NR_HPUX_mount                  21
#define __NR_HPUX_umount                 22
#define __NR_HPUX_setuid                 23
#define __NR_HPUX_getuid                 24
#define __NR_HPUX_stime                  25
#define __NR_HPUX_ptrace                 26
#define __NR_HPUX_alarm                  27
#define __NR_HPUX_oldfstat               28
#define __NR_HPUX_pause                  29
#define __NR_HPUX_utime                  30
#define __NR_HPUX_stty                   31
#define __NR_HPUX_gtty                   32
#define __NR_HPUX_access                 33
#define __NR_HPUX_nice                   34
#define __NR_HPUX_ftime                  35
#define __NR_HPUX_sync                   36
#define __NR_HPUX_kill                   37
#define __NR_HPUX_stat                   38
#define __NR_HPUX_setpgrp3               39
#define __NR_HPUX_lstat                  40
#define __NR_HPUX_dup                    41
#define __NR_HPUX_pipe                   42
#define __NR_HPUX_times                  43
#define __NR_HPUX_profil                 44
#define __NR_HPUX_ki_call                45
#define __NR_HPUX_setgid                 46
#define __NR_HPUX_getgid                 47
#define __NR_HPUX_sigsys                 48
#define __NR_HPUX_reserved1              49
#define __NR_HPUX_reserved2              50
#define __NR_HPUX_acct                   51
#define __NR_HPUX_set_userthreadid       52
#define __NR_HPUX_oldlock                53
#define __NR_HPUX_ioctl                  54
#define __NR_HPUX_reboot                 55
#define __NR_HPUX_symlink                56
#define __NR_HPUX_utssys                 57
#define __NR_HPUX_readlink               58
#define __NR_HPUX_execve                 59
#define __NR_HPUX_umask                  60
#define __NR_HPUX_chroot                 61
#define __NR_HPUX_fcntl                  62
#define __NR_HPUX_ulimit                 63
#define __NR_HPUX_getpagesize            64
#define __NR_HPUX_mremap                 65
#define __NR_HPUX_vfork                  66
#define __NR_HPUX_vread                  67
#define __NR_HPUX_vwrite                 68
#define __NR_HPUX_sbrk                   69
#define __NR_HPUX_sstk                   70
#define __NR_HPUX_mmap                   71
#define __NR_HPUX_vadvise                72
#define __NR_HPUX_munmap                 73
#define __NR_HPUX_mprotect               74
#define __NR_HPUX_madvise                75
#define __NR_HPUX_vhangup                76
#define __NR_HPUX_swapoff                77
#define __NR_HPUX_mincore                78
#define __NR_HPUX_getgroups              79
#define __NR_HPUX_setgroups              80
#define __NR_HPUX_getpgrp2               81
#define __NR_HPUX_setpgrp2               82
#define __NR_HPUX_setitimer              83
#define __NR_HPUX_wait3                  84
#define __NR_HPUX_swapon                 85
#define __NR_HPUX_getitimer              86
#define __NR_HPUX_gethostname42          87
#define __NR_HPUX_sethostname42          88
#define __NR_HPUX_getdtablesize          89
#define __NR_HPUX_dup2                   90
#define __NR_HPUX_getdopt                91
#define __NR_HPUX_fstat                  92
#define __NR_HPUX_select                 93
#define __NR_HPUX_setdopt                94
#define __NR_HPUX_fsync                  95
#define __NR_HPUX_setpriority            96
#define __NR_HPUX_socket_old             97
#define __NR_HPUX_connect_old            98
#define __NR_HPUX_accept_old             99
#define __NR_HPUX_getpriority           100
#define __NR_HPUX_send_old              101
#define __NR_HPUX_recv_old              102
#define __NR_HPUX_socketaddr_old        103
#define __NR_HPUX_bind_old              104
#define __NR_HPUX_setsockopt_old        105
#define __NR_HPUX_listen_old            106
#define __NR_HPUX_vtimes_old            107
#define __NR_HPUX_sigvector             108
#define __NR_HPUX_sigblock              109
#define __NR_HPUX_siggetmask            110
#define __NR_HPUX_sigpause              111
#define __NR_HPUX_sigstack              112
#define __NR_HPUX_recvmsg_old           113
#define __NR_HPUX_sendmsg_old           114
#define __NR_HPUX_vtrace_old            115
#define __NR_HPUX_gettimeofday          116
#define __NR_HPUX_getrusage             117
#define __NR_HPUX_getsockopt_old        118
#define __NR_HPUX_resuba_old            119
#define __NR_HPUX_readv                 120
#define __NR_HPUX_writev                121
#define __NR_HPUX_settimeofday          122
#define __NR_HPUX_fchown                123
#define __NR_HPUX_fchmod                124
#define __NR_HPUX_recvfrom_old          125
#define __NR_HPUX_setresuid             126
#define __NR_HPUX_setresgid             127
#define __NR_HPUX_rename                128
#define __NR_HPUX_truncate              129
#define __NR_HPUX_ftruncate             130
#define __NR_HPUX_flock_old             131
#define __NR_HPUX_sysconf               132
#define __NR_HPUX_sendto_old            133
#define __NR_HPUX_shutdown_old          134
#define __NR_HPUX_socketpair_old        135
#define __NR_HPUX_mkdir                 136
#define __NR_HPUX_rmdir                 137
#define __NR_HPUX_utimes_old            138
#define __NR_HPUX_sigcleanup_old        139
#define __NR_HPUX_setcore               140
#define __NR_HPUX_getpeername_old       141
#define __NR_HPUX_gethostid             142
#define __NR_HPUX_sethostid             143
#define __NR_HPUX_getrlimit             144
#define __NR_HPUX_setrlimit             145
#define __NR_HPUX_killpg_old            146
#define __NR_HPUX_cachectl              147
#define __NR_HPUX_quotactl              148
#define __NR_HPUX_get_sysinfo           149
#define __NR_HPUX_getsockname_old       150
#define __NR_HPUX_privgrp               151
#define __NR_HPUX_rtprio                152
#define __NR_HPUX_plock                 153
#define __NR_HPUX_reserved3             154
#define __NR_HPUX_lockf                 155
#define __NR_HPUX_semget                156
#define __NR_HPUX_osemctl               157
#define __NR_HPUX_semop                 158
#define __NR_HPUX_msgget                159
#define __NR_HPUX_omsgctl               160
#define __NR_HPUX_msgsnd                161
#define __NR_HPUX_msgrecv               162
#define __NR_HPUX_shmget                163
#define __NR_HPUX_oshmctl               164
#define __NR_HPUX_shmat                 165
#define __NR_HPUX_shmdt                 166
#define __NR_HPUX_m68020_advise         167
/* [168,189] are for Discless/DUX */
#define __NR_HPUX_csp                   168
#define __NR_HPUX_cluster               169
#define __NR_HPUX_mkrnod                170
#define __NR_HPUX_test                  171
#define __NR_HPUX_unsp_open             172
#define __NR_HPUX_reserved4             173
#define __NR_HPUX_getcontext_old        174
#define __NR_HPUX_osetcontext           175
#define __NR_HPUX_bigio                 176
#define __NR_HPUX_pipenode              177
#define __NR_HPUX_lsync                 178
#define __NR_HPUX_getmachineid          179
#define __NR_HPUX_cnodeid               180
#define __NR_HPUX_cnodes                181
#define __NR_HPUX_swapclients           182
#define __NR_HPUX_rmt_process           183
#define __NR_HPUX_dskless_stats         184
#define __NR_HPUX_sigprocmask           185
#define __NR_HPUX_sigpending            186
#define __NR_HPUX_sigsuspend            187
#define __NR_HPUX_sigaction             188
#define __NR_HPUX_reserved5             189
#define __NR_HPUX_nfssvc                190
#define __NR_HPUX_getfh                 191
#define __NR_HPUX_getdomainname         192
#define __NR_HPUX_setdomainname         193
#define __NR_HPUX_async_daemon          194
#define __NR_HPUX_getdirentries         195
#define __NR_HPUX_statfs                196
#define __NR_HPUX_fstatfs               197
#define __NR_HPUX_vfsmount              198
#define __NR_HPUX_reserved6             199
#define __NR_HPUX_waitpid               200
/* 201 - 223 missing */
#define __NR_HPUX_sigsetreturn          224
#define __NR_HPUX_sigsetstatemask       225
/* 226 missing */
#define __NR_HPUX_cs                    227
#define __NR_HPUX_cds                   228
#define __NR_HPUX_set_no_trunc          229
#define __NR_HPUX_pathconf              230
#define __NR_HPUX_fpathconf             231
/* 232, 233 missing */
#define __NR_HPUX_nfs_fcntl             234
#define __NR_HPUX_ogetacl               235
#define __NR_HPUX_ofgetacl              236
#define __NR_HPUX_osetacl               237
#define __NR_HPUX_ofsetacl              238
#define __NR_HPUX_pstat                 239
#define __NR_HPUX_getaudid              240
#define __NR_HPUX_setaudid              241
#define __NR_HPUX_getaudproc            242
#define __NR_HPUX_setaudproc            243
#define __NR_HPUX_getevent              244
#define __NR_HPUX_setevent              245
#define __NR_HPUX_audwrite              246
#define __NR_HPUX_audswitch             247
#define __NR_HPUX_audctl                248
#define __NR_HPUX_ogetaccess            249
#define __NR_HPUX_fsctl                 250
/* 251 - 258 missing */
#define __NR_HPUX_swapfs                259
#define __NR_HPUX_fss                   260
/* 261 - 266 missing */
#define __NR_HPUX_tsync                 267
#define __NR_HPUX_getnumfds             268
#define __NR_HPUX_poll                  269
#define __NR_HPUX_getmsg                270
#define __NR_HPUX_putmsg                271
#define __NR_HPUX_fchdir                272
#define __NR_HPUX_getmount_cnt          273
#define __NR_HPUX_getmount_entry        274
#define __NR_HPUX_accept                275
#define __NR_HPUX_bind                  276
#define __NR_HPUX_connect               277
#define __NR_HPUX_getpeername           278
#define __NR_HPUX_getsockname           279
#define __NR_HPUX_getsockopt            280
#define __NR_HPUX_listen                281
#define __NR_HPUX_recv                  282
#define __NR_HPUX_recvfrom              283
#define __NR_HPUX_recvmsg               284
#define __NR_HPUX_send                  285
#define __NR_HPUX_sendmsg               286
#define __NR_HPUX_sendto                287
#define __NR_HPUX_setsockopt            288
#define __NR_HPUX_shutdown              289
#define __NR_HPUX_socket                290
#define __NR_HPUX_socketpair            291
#define __NR_HPUX_proc_open             292
#define __NR_HPUX_proc_close            293
#define __NR_HPUX_proc_send             294
#define __NR_HPUX_proc_recv             295
#define __NR_HPUX_proc_sendrecv         296
#define __NR_HPUX_proc_syscall          297
/* 298 - 311 missing */
#define __NR_HPUX_semctl                312
#define __NR_HPUX_msgctl                313
#define __NR_HPUX_shmctl                314
#define __NR_HPUX_mpctl                 315
#define __NR_HPUX_exportfs              316
#define __NR_HPUX_getpmsg               317
#define __NR_HPUX_putpmsg               318
/* 319 missing */
#define __NR_HPUX_msync                 320
#define __NR_HPUX_msleep                321
#define __NR_HPUX_mwakeup               322
#define __NR_HPUX_msem_init             323
#define __NR_HPUX_msem_remove           324
#define __NR_HPUX_adjtime               325
#define __NR_HPUX_kload                 326
#define __NR_HPUX_fattach               327
#define __NR_HPUX_fdetach               328
#define __NR_HPUX_serialize             329
#define __NR_HPUX_statvfs               330
#define __NR_HPUX_fstatvfs              331
#define __NR_HPUX_lchown                332
#define __NR_HPUX_getsid                333
#define __NR_HPUX_sysfs                 334
/* 335, 336 missing */
#define __NR_HPUX_sched_setparam        337
#define __NR_HPUX_sched_getparam        338
#define __NR_HPUX_sched_setscheduler    339
#define __NR_HPUX_sched_getscheduler    340
#define __NR_HPUX_sched_yield           341
#define __NR_HPUX_sched_get_priority_max 342
#define __NR_HPUX_sched_get_priority_min 343
#define __NR_HPUX_sched_rr_get_interval 344
#define __NR_HPUX_clock_settime         345
#define __NR_HPUX_clock_gettime         346
#define __NR_HPUX_clock_getres          347
#define __NR_HPUX_timer_create          348
#define __NR_HPUX_timer_delete          349
#define __NR_HPUX_timer_settime         350
#define __NR_HPUX_timer_gettime         351
#define __NR_HPUX_timer_getoverrun      352
#define __NR_HPUX_nanosleep             353
#define __NR_HPUX_toolbox               354
/* 355 missing */
#define __NR_HPUX_getdents              356
#define __NR_HPUX_getcontext            357
#define __NR_HPUX_sysinfo               358
#define __NR_HPUX_fcntl64               359
#define __NR_HPUX_ftruncate64           360
#define __NR_HPUX_fstat64               361
#define __NR_HPUX_getdirentries64       362
#define __NR_HPUX_getrlimit64           363
#define __NR_HPUX_lockf64               364
#define __NR_HPUX_lseek64               365
#define __NR_HPUX_lstat64               366
#define __NR_HPUX_mmap64                367
#define __NR_HPUX_setrlimit64           368
#define __NR_HPUX_stat64                369
#define __NR_HPUX_truncate64            370
#define __NR_HPUX_ulimit64              371
#define __NR_HPUX_pread                 372
#define __NR_HPUX_preadv                373
#define __NR_HPUX_pwrite                374
#define __NR_HPUX_pwritev               375
#define __NR_HPUX_pread64               376
#define __NR_HPUX_preadv64              377
#define __NR_HPUX_pwrite64              378
#define __NR_HPUX_pwritev64             379
#define __NR_HPUX_setcontext            380
#define __NR_HPUX_sigaltstack           381
#define __NR_HPUX_waitid                382
#define __NR_HPUX_setpgrp               383
#define __NR_HPUX_recvmsg2              384
#define __NR_HPUX_sendmsg2              385
#define __NR_HPUX_socket2               386
#define __NR_HPUX_socketpair2           387
#define __NR_HPUX_setregid              388
#define __NR_HPUX_lwp_create            389
#define __NR_HPUX_lwp_terminate         390
#define __NR_HPUX_lwp_wait              391
#define __NR_HPUX_lwp_suspend           392
#define __NR_HPUX_lwp_resume            393
/* 394 missing */
#define __NR_HPUX_lwp_abort_syscall     395
#define __NR_HPUX_lwp_info              396
#define __NR_HPUX_lwp_kill              397
#define __NR_HPUX_ksleep                398
#define __NR_HPUX_kwakeup               399
/* 400 missing */
#define __NR_HPUX_pstat_getlwp          401
#define __NR_HPUX_lwp_exit              402
#define __NR_HPUX_lwp_continue          403
#define __NR_HPUX_getacl                404
#define __NR_HPUX_fgetacl               405
#define __NR_HPUX_setacl                406
#define __NR_HPUX_fsetacl               407
#define __NR_HPUX_getaccess             408
#define __NR_HPUX_lwp_mutex_init        409
#define __NR_HPUX_lwp_mutex_lock_sys    410
#define __NR_HPUX_lwp_mutex_unlock      411
#define __NR_HPUX_lwp_cond_init         412
#define __NR_HPUX_lwp_cond_signal       413
#define __NR_HPUX_lwp_cond_broadcast    414
#define __NR_HPUX_lwp_cond_wait_sys     415
#define __NR_HPUX_lwp_getscheduler      416
#define __NR_HPUX_lwp_setscheduler      417
#define __NR_HPUX_lwp_getstate          418
#define __NR_HPUX_lwp_setstate          419
#define __NR_HPUX_lwp_detach            420
#define __NR_HPUX_mlock                 421
#define __NR_HPUX_munlock               422
#define __NR_HPUX_mlockall              423
#define __NR_HPUX_munlockall            424
#define __NR_HPUX_shm_open              425
#define __NR_HPUX_shm_unlink            426
#define __NR_HPUX_sigqueue              427
#define __NR_HPUX_sigwaitinfo           428
#define __NR_HPUX_sigtimedwait          429
#define __NR_HPUX_sigwait               430
#define __NR_HPUX_aio_read              431
#define __NR_HPUX_aio_write             432
#define __NR_HPUX_lio_listio            433
#define __NR_HPUX_aio_error             434
#define __NR_HPUX_aio_return            435
#define __NR_HPUX_aio_cancel            436
#define __NR_HPUX_aio_suspend           437
#define __NR_HPUX_aio_fsync             438
#define __NR_HPUX_mq_open               439
#define __NR_HPUX_mq_close              440
#define __NR_HPUX_mq_unlink             441
#define __NR_HPUX_mq_send               442
#define __NR_HPUX_mq_receive            443
#define __NR_HPUX_mq_notify             444
#define __NR_HPUX_mq_setattr            445
#define __NR_HPUX_mq_getattr            446
#define __NR_HPUX_ksem_open             447
#define __NR_HPUX_ksem_unlink           448
#define __NR_HPUX_ksem_close            449
#define __NR_HPUX_ksem_post             450
#define __NR_HPUX_ksem_wait             451
#define __NR_HPUX_ksem_read             452
#define __NR_HPUX_ksem_trywait          453
#define __NR_HPUX_lwp_rwlock_init       454
#define __NR_HPUX_lwp_rwlock_destroy    455
#define __NR_HPUX_lwp_rwlock_rdlock_sys 456
#define __NR_HPUX_lwp_rwlock_wrlock_sys 457
#define __NR_HPUX_lwp_rwlock_tryrdlock  458
#define __NR_HPUX_lwp_rwlock_trywrlock  459
#define __NR_HPUX_lwp_rwlock_unlock     460
#define __NR_HPUX_ttrace                461
#define __NR_HPUX_ttrace_wait           462
#define __NR_HPUX_lf_wire_mem           463
#define __NR_HPUX_lf_unwire_mem         464
#define __NR_HPUX_lf_send_pin_map       465
#define __NR_HPUX_lf_free_buf           466
#define __NR_HPUX_lf_wait_nq            467
#define __NR_HPUX_lf_wakeup_conn_q      468
#define __NR_HPUX_lf_unused             469
#define __NR_HPUX_lwp_sema_init         470
#define __NR_HPUX_lwp_sema_post         471
#define __NR_HPUX_lwp_sema_wait         472
#define __NR_HPUX_lwp_sema_trywait      473
#define __NR_HPUX_lwp_sema_destroy      474
#define __NR_HPUX_statvfs64             475
#define __NR_HPUX_fstatvfs64            476
#define __NR_HPUX_msh_register          477
#define __NR_HPUX_ptrace64              478
#define __NR_HPUX_sendfile              479
#define __NR_HPUX_sendpath              480
#define __NR_HPUX_sendfile64            481
#define __NR_HPUX_sendpath64            482
#define __NR_HPUX_modload               483
#define __NR_HPUX_moduload              484
#define __NR_HPUX_modpath               485
#define __NR_HPUX_getksym               486
#define __NR_HPUX_modadm                487
#define __NR_HPUX_modstat               488
#define __NR_HPUX_lwp_detached_exit     489
#define __NR_HPUX_crashconf             490
#define __NR_HPUX_siginhibit            491
#define __NR_HPUX_sigenable             492
#define __NR_HPUX_spuctl                493
#define __NR_HPUX_zerokernelsum         494
#define __NR_HPUX_nfs_kstat             495
#define __NR_HPUX_aio_read64            496
#define __NR_HPUX_aio_write64           497
#define __NR_HPUX_aio_error64           498
#define __NR_HPUX_aio_return64          499
#define __NR_HPUX_aio_cancel64          500
#define __NR_HPUX_aio_suspend64         501
#define __NR_HPUX_aio_fsync64           502
#define __NR_HPUX_lio_listio64          503
#define __NR_HPUX_recv2                 504
#define __NR_HPUX_recvfrom2             505
#define __NR_HPUX_send2                 506
#define __NR_HPUX_sendto2               507
#define __NR_HPUX_acl                   508
#define __NR_HPUX___cnx_p2p_ctl         509
#define __NR_HPUX___cnx_gsched_ctl      510
#define __NR_HPUX___cnx_pmon_ctl        511

#define __NR_HPUX_syscalls		512

/*
 * Linux system call numbers.
 *
 * Cary Coutant says that we should just use another syscall gateway
 * page to avoid clashing with the HPUX space, and I think he's right:
 * it will would keep a branch out of our syscall entry path, at the
 * very least.  If we decide to change it later, we can ``just'' tweak
 * the LINUX_GATEWAY_ADDR define at the bottom and make __NR_Linux be
 * 1024 or something.  Oh, and recompile libc. =)
 *
 * 64-bit HPUX binaries get the syscall gateway address passed in a register
 * from the kernel at startup, which seems a sane strategy.
 */

#define __NR_Linux                0
#define __NR_syscall              (__NR_Linux + 0)
#define __NR_exit                 (__NR_Linux + 1)
#define __NR_fork                 (__NR_Linux + 2)
#define __NR_read                 (__NR_Linux + 3)
#define __NR_write                (__NR_Linux + 4)
#define __NR_open                 (__NR_Linux + 5)
#define __NR_close                (__NR_Linux + 6)
#define __NR_waitpid              (__NR_Linux + 7)
#define __NR_creat                (__NR_Linux + 8)
#define __NR_link                 (__NR_Linux + 9)
#define __NR_unlink              (__NR_Linux + 10)
#define __NR_execve              (__NR_Linux + 11)
#define __NR_chdir               (__NR_Linux + 12)
#define __NR_time                (__NR_Linux + 13)
#define __NR_mknod               (__NR_Linux + 14)
#define __NR_chmod               (__NR_Linux + 15)
#define __NR_lchown              (__NR_Linux + 16)
#define __NR_socket              (__NR_Linux + 17)
#define __NR_stat                (__NR_Linux + 18)
#define __NR_lseek               (__NR_Linux + 19)
#define __NR_getpid              (__NR_Linux + 20)
#define __NR_mount               (__NR_Linux + 21)
#define __NR_bind                (__NR_Linux + 22)
#define __NR_setuid              (__NR_Linux + 23)
#define __NR_getuid              (__NR_Linux + 24)
#define __NR_stime               (__NR_Linux + 25)
#define __NR_ptrace              (__NR_Linux + 26)
#define __NR_alarm               (__NR_Linux + 27)
#define __NR_fstat               (__NR_Linux + 28)
#define __NR_pause               (__NR_Linux + 29)
#define __NR_utime               (__NR_Linux + 30)
#define __NR_connect             (__NR_Linux + 31)
#define __NR_listen              (__NR_Linux + 32)
#define __NR_access              (__NR_Linux + 33)
#define __NR_nice                (__NR_Linux + 34)
#define __NR_accept              (__NR_Linux + 35)
#define __NR_sync                (__NR_Linux + 36)
#define __NR_kill                (__NR_Linux + 37)
#define __NR_rename              (__NR_Linux + 38)
#define __NR_mkdir               (__NR_Linux + 39)
#define __NR_rmdir               (__NR_Linux + 40)
#define __NR_dup                 (__NR_Linux + 41)
#define __NR_pipe                (__NR_Linux + 42)
#define __NR_times               (__NR_Linux + 43)
#define __NR_getsockname         (__NR_Linux + 44)
#define __NR_brk                 (__NR_Linux + 45)
#define __NR_setgid              (__NR_Linux + 46)
#define __NR_getgid              (__NR_Linux + 47)
#define __NR_signal              (__NR_Linux + 48)
#define __NR_geteuid             (__NR_Linux + 49)
#define __NR_getegid             (__NR_Linux + 50)
#define __NR_acct                (__NR_Linux + 51)
#define __NR_umount2             (__NR_Linux + 52)
#define __NR_getpeername         (__NR_Linux + 53)
#define __NR_ioctl               (__NR_Linux + 54)
#define __NR_fcntl               (__NR_Linux + 55)
#define __NR_socketpair          (__NR_Linux + 56)
#define __NR_setpgid             (__NR_Linux + 57)
#define __NR_send                (__NR_Linux + 58)
#define __NR_uname               (__NR_Linux + 59)
#define __NR_umask               (__NR_Linux + 60)
#define __NR_chroot              (__NR_Linux + 61)
#define __NR_ustat               (__NR_Linux + 62)
#define __NR_dup2                (__NR_Linux + 63)
#define __NR_getppid             (__NR_Linux + 64)
#define __NR_getpgrp             (__NR_Linux + 65)
#define __NR_setsid              (__NR_Linux + 66)
#define __NR_pivot_root          (__NR_Linux + 67)
#define __NR_sgetmask            (__NR_Linux + 68)
#define __NR_ssetmask            (__NR_Linux + 69)
#define __NR_setreuid            (__NR_Linux + 70)
#define __NR_setregid            (__NR_Linux + 71)
#define __NR_mincore             (__NR_Linux + 72)
#define __NR_sigpending          (__NR_Linux + 73)
#define __NR_sethostname         (__NR_Linux + 74)
#define __NR_setrlimit           (__NR_Linux + 75)
#define __NR_getrlimit           (__NR_Linux + 76)
#define __NR_getrusage           (__NR_Linux + 77)
#define __NR_gettimeofday        (__NR_Linux + 78)
#define __NR_settimeofday        (__NR_Linux + 79)
#define __NR_getgroups           (__NR_Linux + 80)
#define __NR_setgroups           (__NR_Linux + 81)
#define __NR_sendto              (__NR_Linux + 82)
#define __NR_symlink             (__NR_Linux + 83)
#define __NR_lstat               (__NR_Linux + 84)
#define __NR_readlink            (__NR_Linux + 85)
#define __NR_uselib              (__NR_Linux + 86)
#define __NR_swapon              (__NR_Linux + 87)
#define __NR_reboot              (__NR_Linux + 88)
#define __NR_mmap2             (__NR_Linux + 89)
#define __NR_mmap                (__NR_Linux + 90)
#define __NR_munmap              (__NR_Linux + 91)
#define __NR_truncate            (__NR_Linux + 92)
#define __NR_ftruncate           (__NR_Linux + 93)
#define __NR_fchmod              (__NR_Linux + 94)
#define __NR_fchown              (__NR_Linux + 95)
#define __NR_getpriority         (__NR_Linux + 96)
#define __NR_setpriority         (__NR_Linux + 97)
#define __NR_recv                (__NR_Linux + 98)
#define __NR_statfs              (__NR_Linux + 99)
#define __NR_fstatfs            (__NR_Linux + 100)
#define __NR_stat64           (__NR_Linux + 101)
/* #define __NR_socketcall         (__NR_Linux + 102) */
#define __NR_syslog             (__NR_Linux + 103)
#define __NR_setitimer          (__NR_Linux + 104)
#define __NR_getitimer          (__NR_Linux + 105)
#define __NR_capget             (__NR_Linux + 106)
#define __NR_capset             (__NR_Linux + 107)
#define __NR_pread              (__NR_Linux + 108)
#define __NR_pwrite             (__NR_Linux + 109)
#define __NR_getcwd             (__NR_Linux + 110)
#define __NR_vhangup            (__NR_Linux + 111)
#define __NR_fstat64            (__NR_Linux + 112)
#define __NR_vfork              (__NR_Linux + 113)
#define __NR_wait4              (__NR_Linux + 114)
#define __NR_swapoff            (__NR_Linux + 115)
#define __NR_sysinfo            (__NR_Linux + 116)
#define __NR_shutdown           (__NR_Linux + 117)
#define __NR_fsync              (__NR_Linux + 118)
#define __NR_madvise            (__NR_Linux + 119)
#define __NR_clone              (__NR_Linux + 120)
#define __NR_setdomainname      (__NR_Linux + 121)
#define __NR_sendfile           (__NR_Linux + 122)
#define __NR_recvfrom           (__NR_Linux + 123)
#define __NR_adjtimex           (__NR_Linux + 124)
#define __NR_mprotect           (__NR_Linux + 125)
#define __NR_sigprocmask        (__NR_Linux + 126)
#define __NR_create_module      (__NR_Linux + 127)
#define __NR_init_module        (__NR_Linux + 128)
#define __NR_delete_module      (__NR_Linux + 129)
#define __NR_get_kernel_syms    (__NR_Linux + 130)
#define __NR_quotactl           (__NR_Linux + 131)
#define __NR_getpgid            (__NR_Linux + 132)
#define __NR_fchdir             (__NR_Linux + 133)
#define __NR_bdflush            (__NR_Linux + 134)
#define __NR_sysfs              (__NR_Linux + 135)
#define __NR_personality        (__NR_Linux + 136)
#define __NR_afs_syscall        (__NR_Linux + 137) /* Syscall for Andrew File System */
#define __NR_setfsuid           (__NR_Linux + 138)
#define __NR_setfsgid           (__NR_Linux + 139)
#define __NR__llseek            (__NR_Linux + 140)
#define __NR_getdents           (__NR_Linux + 141)
#define __NR__newselect         (__NR_Linux + 142)
#define __NR_flock              (__NR_Linux + 143)
#define __NR_msync              (__NR_Linux + 144)
#define __NR_readv              (__NR_Linux + 145)
#define __NR_writev             (__NR_Linux + 146)
#define __NR_getsid             (__NR_Linux + 147)
#define __NR_fdatasync          (__NR_Linux + 148)
#define __NR__sysctl            (__NR_Linux + 149)
#define __NR_mlock              (__NR_Linux + 150)
#define __NR_munlock            (__NR_Linux + 151)
#define __NR_mlockall           (__NR_Linux + 152)
#define __NR_munlockall         (__NR_Linux + 153)
#define __NR_sched_setparam             (__NR_Linux + 154)
#define __NR_sched_getparam             (__NR_Linux + 155)
#define __NR_sched_setscheduler         (__NR_Linux + 156)
#define __NR_sched_getscheduler         (__NR_Linux + 157)
#define __NR_sched_yield                (__NR_Linux + 158)
#define __NR_sched_get_priority_max     (__NR_Linux + 159)
#define __NR_sched_get_priority_min     (__NR_Linux + 160)
#define __NR_sched_rr_get_interval      (__NR_Linux + 161)
#define __NR_nanosleep          (__NR_Linux + 162)
#define __NR_mremap             (__NR_Linux + 163)
#define __NR_setresuid          (__NR_Linux + 164)
#define __NR_getresuid          (__NR_Linux + 165)
#define __NR_sigaltstack        (__NR_Linux + 166)
#define __NR_query_module       (__NR_Linux + 167)
#define __NR_poll               (__NR_Linux + 168)
#define __NR_nfsservctl         (__NR_Linux + 169)
#define __NR_setresgid          (__NR_Linux + 170)
#define __NR_getresgid          (__NR_Linux + 171)
#define __NR_prctl              (__NR_Linux + 172)
#define __NR_rt_sigreturn       (__NR_Linux + 173)
#define __NR_rt_sigaction       (__NR_Linux + 174)
#define __NR_rt_sigprocmask     (__NR_Linux + 175)
#define __NR_rt_sigpending      (__NR_Linux + 176)
#define __NR_rt_sigtimedwait    (__NR_Linux + 177)
#define __NR_rt_sigqueueinfo    (__NR_Linux + 178)
#define __NR_rt_sigsuspend      (__NR_Linux + 179)
#define __NR_chown              (__NR_Linux + 180)
#define __NR_setsockopt         (__NR_Linux + 181)
#define __NR_getsockopt         (__NR_Linux + 182)
#define __NR_sendmsg            (__NR_Linux + 183)
#define __NR_recvmsg            (__NR_Linux + 184)
#define __NR_semop              (__NR_Linux + 185)
#define __NR_semget             (__NR_Linux + 186)
#define __NR_semctl             (__NR_Linux + 187)
#define __NR_msgsnd             (__NR_Linux + 188)
#define __NR_msgrcv             (__NR_Linux + 189)
#define __NR_msgget             (__NR_Linux + 190)
#define __NR_msgctl             (__NR_Linux + 191)
#define __NR_shmat              (__NR_Linux + 192)
#define __NR_shmdt              (__NR_Linux + 193)
#define __NR_shmget             (__NR_Linux + 194)
#define __NR_shmctl             (__NR_Linux + 195)

#define __NR_getpmsg            (__NR_Linux + 196)      /* some people actually want streams */
#define __NR_putpmsg            (__NR_Linux + 197)      /* some people actually want streams */

#define __NR_lstat64            (__NR_Linux + 198)
#define __NR_truncate64         (__NR_Linux + 199)
#define __NR_ftruncate64        (__NR_Linux + 200)
#define __NR_getdents64         (__NR_Linux + 201)
#define __NR_fcntl64            (__NR_Linux + 202)
#define __NR_attrctl            (__NR_Linux + 203)
#define __NR_acl_get            (__NR_Linux + 204)
#define __NR_acl_set            (__NR_Linux + 205)
#define __NR_gettid             (__NR_Linux + 206)
#define __NR_readahead          (__NR_Linux + 207)
#define __NR_tkill              (__NR_Linux + 208)

#define __NR_Linux_syscalls     208

#define HPUX_GATEWAY_ADDR       0xC0000004
#define LINUX_GATEWAY_ADDR      0x100

#ifndef __ASSEMBLY__

/* The old syscall code here didn't work, and it looks like it's only used
 * by applications such as fdisk which for some reason need to produce
 * their own syscall instead of using same from libc.  The code below
 * is leveraged from glibc/sysdeps/unix/sysv/linux/hppa/sysdep.h where
 * it is essentially duplicated -- which sucks.  -PB
 */

#define SYS_ify(syscall_name)   __NR_##syscall_name

/* The system call number MUST ALWAYS be loaded in the delay slot of
   the ble instruction, or restarting system calls WILL NOT WORK.  See
   arch/parisc/kernel/signal.c - dhd, 2000-07-26 */
#define K_INLINE_SYSCALL(name, nr, args...)       ({              \
        unsigned long __sys_res;                                \
        {                                                       \
                register unsigned long __res asm("r28");        \
                K_LOAD_ARGS_##nr(args)                            \
                asm volatile(                                   \
			"ble  0x100(%%sr2, %%r0)\n\t"           \
                        " ldi %1, %%r20"                        \
                        : "=r" (__res)                          \
                        : "i" (SYS_ify(name)) K_ASM_ARGS_##nr   \
			  );                                    \
                __sys_res = __res;                              \
        }                                                       \
        if (__sys_res >= (unsigned long)-4095) {                \
		errno = -__sys_res;				\
                __sys_res = (unsigned long)-1;                 \
        }                                                       \
        __sys_res;                                              \
})

#define K_LOAD_ARGS_0()
#define K_LOAD_ARGS_1(r26)                                        \
        register unsigned long __r26 __asm__("r26") = (unsigned long)r26;       \
        K_LOAD_ARGS_0()
#define K_LOAD_ARGS_2(r26,r25)                                    \
        register unsigned long __r25 __asm__("r25") = (unsigned long)r25;       \
        K_LOAD_ARGS_1(r26)
#define K_LOAD_ARGS_3(r26,r25,r24)                                \
        register unsigned long __r24 __asm__("r24") = (unsigned long)r24;       \
        K_LOAD_ARGS_2(r26,r25)
#define K_LOAD_ARGS_4(r26,r25,r24,r23)                            \
        register unsigned long __r23 __asm__("r23") = (unsigned long)r23;       \
        K_LOAD_ARGS_3(r26,r25,r24)
#define K_LOAD_ARGS_5(r26,r25,r24,r23,r22)                        \
        register unsigned long __r22 __asm__("r22") = (unsigned long)r22;       \
        K_LOAD_ARGS_4(r26,r25,r24,r23)
#define K_LOAD_ARGS_6(r26,r25,r24,r23,r22,r21)                    \
        register unsigned long __r21 __asm__("r21") = (unsigned long)r21;       \
        K_LOAD_ARGS_5(r26,r25,r24,r23,r22)

#define K_ASM_ARGS_0
#define K_ASM_ARGS_1 , "r" (__r26)
#define K_ASM_ARGS_2 , "r" (__r26), "r" (__r25)
#define K_ASM_ARGS_3 , "r" (__r26), "r" (__r25), "r" (__r24)
#define K_ASM_ARGS_4 , "r" (__r26), "r" (__r25), "r" (__r24), "r" (__r23)
#define K_ASM_ARGS_5 , "r" (__r26), "r" (__r25), "r" (__r24), "r" (__r23), "r" (__r22)
#define K_ASM_ARGS_6 , "r" (__r26), "r" (__r25), "r" (__r24), "r" (__r23), "r" (__r22), "r" (__r21)

#define _syscall0(type,name)						      \
type name(void)								      \
{									      \
    return K_INLINE_SYSCALL(name, 0);	\
}

#define _syscall1(type,name,type1,arg1)					      \
type name(type1 arg1)							      \
{									      \
    return K_INLINE_SYSCALL(name, 1, arg1);	\
}

#define _syscall2(type,name,type1,arg1,type2,arg2)			      \
type name(type1 arg1, type2 arg2)					      \
{									      \
    return K_INLINE_SYSCALL(name, 2, arg1, arg2);	\
}

#define _syscall3(type,name,type1,arg1,type2,arg2,type3,arg3)		      \
type name(type1 arg1, type2 arg2, type3 arg3)				      \
{									      \
    return K_INLINE_SYSCALL(name, 3, arg1, arg2, arg3);	\
}

#define _syscall4(type,name,type1,arg1,type2,arg2,type3,arg3,type4,arg4)      \
type name(type1 arg1, type2 arg2, type3 arg3, type4 arg4)		      \
{									      \
    return K_INLINE_SYSCALL(name, 4, arg1, arg2, arg3, arg4);	\
}

/* select takes 5 arguments */
#define _syscall5(type,name,type1,arg1,type2,arg2,type3,arg3,type4,arg4,type5,arg5) \
type name(type1 arg1, type2 arg2, type3 arg3, type4 arg4, type5 arg5)	\
{									\
    return K_INLINE_SYSCALL(name, 5, arg1, arg2, arg3, arg4, arg5);	\
}


/* mmap & mmap2 take 6 arguments */

#define _syscall6(type,name,type1,arg1,type2,arg2,type3,arg3,type4,arg4,type5,arg5,type6,arg6) \
type name(type1 arg1, type2 arg2, type3 arg3, type4 arg4, type5 arg5, type6 arg6) \
{									\
    return K_INLINE_SYSCALL(name, 6, arg1, arg2, arg3, arg4, arg5, arg6);	\
}


#ifdef __KERNEL_SYSCALLS__

static inline int pause(void)
{
	extern int sys_pause(void);
	return sys_pause();
}

static inline int sync(void)
{
	extern int sys_sync(void);
	return sys_sync();
}

static inline pid_t setsid(void)
{
	extern int sys_setsid(void);
	return sys_setsid();
}

static inline int write(int fd, const char *buf, off_t count)
{
	extern int sys_write(int, const char *, int);
	return sys_write(fd, buf, count);
}

static inline int read(int fd, char *buf, off_t count)
{
	extern int sys_read(int, char *, int);
	return sys_read(fd, buf, count);
}

static inline off_t lseek(int fd, off_t offset, int count)
{
	extern off_t sys_lseek(int, off_t, int);
	return sys_lseek(fd, offset, count);
}

static inline int dup(int fd)
{
	extern int sys_dup(int);
	return sys_dup(fd);
}

static inline int execve(char *filename, char * argv [],
	char * envp[])
{
	extern int __execve(char *, char **, char **, struct task_struct *);
	return __execve(filename, argv, envp, current);
}

static inline int open(const char *file, int flag, int mode)
{
	extern long sys_open(const char *, int, int);
	return sys_open(file, flag, mode);
}

static inline int close(int fd)
{
	return sys_close(fd);
}

static inline int _exit(int exitcode)
{
	extern int sys_exit(int) __attribute__((noreturn));
	return sys_exit(exitcode);
}

static inline pid_t waitpid(pid_t pid, int *wait_stat, int options)
{
	return sys_wait4((int)pid, wait_stat, options, NULL);
}

static inline int delete_module(const char *name)
{
	extern int sys_delete_module(const char *name);
	return sys_delete_module(name);
}

static inline pid_t wait(int * wait_stat)
{
	return sys_wait4(-1, wait_stat, 0, NULL);
}

#endif	/* __KERNEL_SYSCALLS__ */

#endif /* __ASSEMBLY__ */

#undef STR

#endif /* _ASM_PARISC_UNISTD_H_ */
