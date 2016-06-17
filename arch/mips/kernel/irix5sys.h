/*
 * irix5sys.h: 32-bit IRIX5 ABI system call table.
 *
 * Copyright (C) 1996 David S. Miller (dm@engr.sgi.com)
 */

/* This file is being included twice - once to build a list of all
 * syscalls and once to build a table of how many arguments each syscall
 * accepts.  Syscalls that receive a pointer to the saved registers are
 * marked as having zero arguments.
 */

/* Keys:
 *         V == Valid and should work as expected for most cases.
 *        HV == Half Valid, some things will work, some likely will not
 *        IV == InValid, certainly will not work at all yet
 *        ?V == ?'ably Valid, I have not done enough looking into it
 *        DC == Don't Care, a rats ass we couldn't give
 */

SYS(sys_syscall, 0)			/* 1000  sysindir()	       V*/
SYS(sys_exit, 1)			/* 1001  exit()		       V*/
SYS(sys_fork, 0)			/* 1002  fork()		       V*/
SYS(sys_read, 3)			/* 1003  read()		       V*/
SYS(sys_write, 3)			/* 1004  write()	       V*/
SYS(sys_open, 3)			/* 1005  open()		       V*/
SYS(sys_close, 1)			/* 1006  close()	       V*/
SYS(irix_unimp, 0)			/* 1007  (XXX IRIX 4 wait)     V*/
SYS(sys_creat, 2)			/* 1008  creat()	       V*/
SYS(sys_link, 2)			/* 1009  link()		       V*/
SYS(sys_unlink, 1)			/* 1010  unlink()	       V*/
SYS(irix_exec, 0)			/* 1011  exec()		       V*/
SYS(sys_chdir, 1)			/* 1012  chdir()	       V*/
SYS(irix_gtime, 0)			/* 1013  time()		       V*/
SYS(irix_unimp, 0)			/* 1014  (XXX IRIX 4 mknod)    V*/
SYS(sys_chmod, 2)			/* 1015  chmod()	       V*/
SYS(sys_chown, 3)			/* 1016  chown()	       V*/
SYS(irix_brk, 1)			/* 1017  break()	       V*/
SYS(irix_unimp, 0)			/* 1018  (XXX IRIX 4 stat)     V*/
SYS(sys_lseek, 3)			/* 1019  lseek()     XXX64bit HV*/
SYS(irix_getpid, 0)			/* 1020  getpid()	       V*/
SYS(irix_mount, 6)			/* 1021  mount()	      IV*/
SYS(sys_umount, 1)			/* 1022  umount()	       V*/
SYS(sys_setuid, 1)			/* 1023  setuid()	       V*/
SYS(irix_getuid, 0)			/* 1024  getuid()	       V*/
SYS(irix_stime, 1)			/* 1025  stime()	       V*/
SYS(irix_unimp, 4)			/* 1026  XXX ptrace()	      IV*/
SYS(irix_alarm, 1)			/* 1027  alarm()	       V*/
SYS(irix_unimp, 0)			/* 1028  (XXX IRIX 4 fstat)    V*/
SYS(irix_pause, 0)			/* 1029  pause()	       V*/
SYS(sys_utime, 2)			/* 1030  utime()	       V*/
SYS(irix_unimp, 0)			/* 1031  nuthin'	       V*/
SYS(irix_unimp, 0)			/* 1032  nobody home man...    V*/
SYS(sys_access, 2)			/* 1033  access()	       V*/
SYS(sys_nice, 1)			/* 1034  nice()		       V*/
SYS(irix_statfs, 2)			/* 1035  statfs()	       V*/
SYS(sys_sync, 0)			/* 1036  sync()		       V*/
SYS(sys_kill, 2)			/* 1037  kill()		       V*/
SYS(irix_fstatfs, 2)			/* 1038  fstatfs()	       V*/
SYS(irix_setpgrp, 1)			/* 1039  setpgrp()	       V*/
SYS(irix_syssgi, 0)			/* 1040  syssgi()	      HV*/
SYS(sys_dup, 1)				/* 1041  dup()		       V*/
SYS(sys_pipe, 0)			/* 1042  pipe()		       V*/
SYS(irix_times, 1)			/* 1043  times()	       V*/
SYS(irix_unimp, 0)			/* 1044  XXX profil()	      IV*/
SYS(irix_unimp, 0)			/* 1045  XXX lock()	      IV*/
SYS(sys_setgid, 1)			/* 1046  setgid()	       V*/
SYS(irix_getgid, 0)			/* 1047  getgid()	       V*/
SYS(irix_unimp, 0)			/* 1048  (XXX IRIX 4 ssig)     V*/
SYS(irix_msgsys, 6)			/* 1049  sys_msgsys	       V*/
SYS(sys_sysmips, 4)			/* 1050  sysmips()	      HV*/
SYS(irix_unimp, 0)			/* 1051	 XXX sysacct()	      IV*/
SYS(irix_shmsys, 5)			/* 1052  sys_shmsys	       V*/
SYS(irix_semsys, 0)			/* 1053  sys_semsys	       V*/
SYS(irix_ioctl, 3)			/* 1054  ioctl()	      HV*/
SYS(irix_uadmin, 0)			/* 1055  XXX sys_uadmin()     HC*/
SYS(irix_sysmp, 0)			/* 1056  sysmp()	      HV*/
SYS(irix_utssys, 4)			/* 1057  sys_utssys()	      HV*/
SYS(irix_unimp, 0)			/* 1058  nada enchilada	       V*/
SYS(irix_exece, 0)			/* 1059  exece()	       V*/
SYS(sys_umask, 1)			/* 1060  umask()	       V*/
SYS(sys_chroot, 1)			/* 1061  chroot()	       V*/
SYS(irix_fcntl, 3)			/* 1062  fcntl()	      ?V*/
SYS(irix_ulimit, 2)			/* 1063  ulimit()	      HV*/
SYS(irix_unimp, 0)			/* 1064  XXX AFS shit	      DC*/
SYS(irix_unimp, 0)			/* 1065  XXX AFS shit	      DC*/
SYS(irix_unimp, 0)			/* 1066  XXX AFS shit	      DC*/
SYS(irix_unimp, 0)			/* 1067  XXX AFS shit	      DC*/
SYS(irix_unimp, 0)			/* 1068  XXX AFS shit	      DC*/
SYS(irix_unimp, 0)			/* 1069  XXX AFS shit	      DC*/
SYS(irix_unimp, 0)			/* 1070  XXX AFS shit	      DC*/
SYS(irix_unimp, 0)			/* 1071  XXX AFS shit	      DC*/
SYS(irix_unimp, 0)			/* 1072  XXX AFS shit	      DC*/
SYS(irix_unimp, 0)			/* 1073  XXX AFS shit	      DC*/
SYS(irix_unimp, 0)			/* 1074  nuttin'	       V*/
SYS(irix_unimp, 0)			/* 1075  XXX sys_getrlimit64()IV*/
SYS(irix_unimp, 0)			/* 1076  XXX sys_setrlimit64()IV*/
SYS(sys_nanosleep, 2)			/* 1077  nanosleep()	       V*/
SYS(irix_lseek64, 5)			/* 1078  lseek64()	      ?V*/
SYS(sys_rmdir, 1)			/* 1079  rmdir()	       V*/
SYS(sys_mkdir, 2)			/* 1080  mkdir()	       V*/
SYS(sys_getdents, 3)			/* 1081  getdents()	       V*/
SYS(irix_sginap, 1)			/* 1082  sys_sginap()	       V*/
SYS(irix_sgikopt, 3)			/* 1083  sys_sgikopt()	      DC*/
SYS(sys_sysfs, 3)			/* 1084  sysfs()	      ?V*/
SYS(irix_unimp, 0)			/* 1085  XXX sys_getmsg()     DC*/
SYS(irix_unimp, 0)			/* 1086  XXX sys_putmsg()     DC*/
SYS(sys_poll, 3)			/* 1087  poll()	               V*/
SYS(irix_sigreturn, 0)			/* 1088  sigreturn()	      ?V*/
SYS(sys_accept, 3)			/* 1089  accept()	       V*/
SYS(sys_bind, 3)			/* 1090  bind()		       V*/
SYS(sys_connect, 3)			/* 1091  connect()	       V*/
SYS(irix_gethostid, 0)			/* 1092  sys_gethostid()      ?V*/
SYS(sys_getpeername, 3)			/* 1093  getpeername()	       V*/
SYS(sys_getsockname, 3)			/* 1094  getsockname()	       V*/
SYS(sys_getsockopt, 5)			/* 1095  getsockopt()	       V*/
SYS(sys_listen, 2)			/* 1096  listen()	       V*/
SYS(sys_recv, 4)			/* 1097  recv()		       V*/
SYS(sys_recvfrom, 6)			/* 1098  recvfrom()	       V*/
SYS(sys_recvmsg, 3)			/* 1099  recvmsg()	       V*/
SYS(sys_select, 5)			/* 1100  select()	       V*/
SYS(sys_send, 4)			/* 1101  send()		       V*/
SYS(sys_sendmsg, 3)			/* 1102  sendmsg()	       V*/
SYS(sys_sendto, 6)			/* 1103  sendto()	       V*/
SYS(irix_sethostid, 1)			/* 1104  sys_sethostid()      ?V*/
SYS(sys_setsockopt, 5)			/* 1105  setsockopt()	       V*/
SYS(sys_shutdown, 2)			/* 1106  shutdown()	      ?V*/
SYS(irix_socket, 3)			/* 1107  socket()	       V*/
SYS(sys_gethostname, 2)			/* 1108  sys_gethostname()    ?V*/
SYS(sys_sethostname, 2)			/* 1109  sethostname()	      ?V*/
SYS(irix_getdomainname, 2)		/* 1110  sys_getdomainname()  ?V*/
SYS(sys_setdomainname, 2)		/* 1111  setdomainname()      ?V*/
SYS(sys_truncate, 2)			/* 1112  truncate()	       V*/
SYS(sys_ftruncate, 2)			/* 1113  ftruncate()	       V*/
SYS(sys_rename, 2)			/* 1114  rename()	       V*/
SYS(sys_symlink, 2)			/* 1115  symlink()	       V*/
SYS(sys_readlink, 3)			/* 1116  readlink()	       V*/
SYS(irix_unimp, 0)			/* 1117  XXX IRIX 4 lstat()   DC*/
SYS(irix_unimp, 0)			/* 1118  nothin'	       V*/
SYS(irix_unimp, 0)			/* 1119  XXX nfs_svc()	      DC*/
SYS(irix_unimp, 0)			/* 1120  XXX nfs_getfh()      DC*/
SYS(irix_unimp, 0)			/* 1121  XXX async_daemon()   DC*/
SYS(irix_unimp, 0)			/* 1122  XXX exportfs()	      DC*/
SYS(sys_setregid, 2)			/* 1123  setregid()	       V*/
SYS(sys_setreuid, 2)			/* 1124  setreuid()	       V*/
SYS(sys_getitimer, 2)			/* 1125  getitimer()	       V*/
SYS(sys_setitimer, 3)			/* 1126  setitimer()	       V*/
SYS(irix_unimp, 1)			/* 1127  XXX adjtime() 	      IV*/
SYS(irix_gettimeofday, 1)		/* 1128  gettimeofday()	       V*/
SYS(irix_unimp, 0)			/* 1129  XXX sproc()	      IV*/
SYS(irix_prctl, 0)			/* 1130  prctl()	      HV*/
SYS(irix_unimp, 0)			/* 1131  XXX procblk()	      IV*/
SYS(irix_unimp, 0)			/* 1132  XXX sprocsp()	      IV*/
SYS(irix_unimp, 0)			/* 1133  XXX sgigsc()	      IV*/
SYS(irix_mmap32, 6)			/* 1134  mmap()	   XXXflags?  ?V*/
SYS(sys_munmap, 2)			/* 1135  munmap()	       V*/
SYS(sys_mprotect, 3)			/* 1136  mprotect()	       V*/
SYS(sys_msync, 4)			/* 1137  msync()	       V*/
SYS(irix_madvise, 3)			/* 1138  madvise()	      DC*/
SYS(irix_pagelock, 3)			/* 1139  pagelock()	      IV*/
SYS(irix_getpagesize, 0)		/* 1140  getpagesize()         V*/
SYS(irix_quotactl, 0)			/* 1141  quotactl()	       V*/
SYS(irix_unimp, 0)			/* 1142  nobody home man       V*/
SYS(sys_getpgid, 1)			/* 1143  BSD getpgrp()	       V*/
SYS(irix_BSDsetpgrp, 2)			/* 1143  BSD setpgrp()	       V*/
SYS(sys_vhangup, 0)			/* 1144  vhangup()	       V*/
SYS(sys_fsync, 1)			/* 1145  fsync()	       V*/
SYS(sys_fchdir, 1)			/* 1146  fchdir()	       V*/
SYS(sys_getrlimit, 2)			/* 1147  getrlimit()	      ?V*/
SYS(sys_setrlimit, 2)			/* 1148  setrlimit()	      ?V*/
SYS(sys_cacheflush, 3)			/* 1150  cacheflush()	      HV*/
SYS(sys_cachectl, 3)			/* 1151  cachectl()	      HV*/
SYS(sys_fchown, 3)			/* 1152  fchown()	      ?V*/
SYS(sys_fchmod, 2)			/* 1153  fchmod()	      ?V*/
SYS(irix_unimp, 0)			/* 1154  XXX IRIX 4 wait3()    V*/
SYS(sys_socketpair, 4)			/* 1155  socketpair()	       V*/
SYS(irix_systeminfo, 3)			/* 1156  systeminfo()	      IV*/
SYS(irix_uname, 1)			/* 1157  uname()	      IV*/
SYS(irix_xstat, 3)			/* 1158  xstat()	       V*/
SYS(irix_lxstat, 3)			/* 1159  lxstat()	       V*/
SYS(irix_fxstat, 3)			/* 1160  fxstat()	       V*/
SYS(irix_xmknod, 0)			/* 1161  xmknod()	      ?V*/
SYS(irix_sigaction, 4)			/* 1162  sigaction()	      ?V*/
SYS(irix_sigpending, 1)			/* 1163  sigpending()	      ?V*/
SYS(irix_sigprocmask, 3)		/* 1164  sigprocmask()	      ?V*/
SYS(irix_sigsuspend, 0)			/* 1165  sigsuspend()	      ?V*/
SYS(irix_sigpoll_sys, 3)		/* 1166  sigpoll_sys()	      IV*/
SYS(irix_swapctl, 2)			/* 1167  swapctl()	      IV*/
SYS(irix_getcontext, 0)			/* 1168  getcontext()	      HV*/
SYS(irix_setcontext, 0)			/* 1169  setcontext()	      HV*/
SYS(irix_waitsys, 5)			/* 1170  waitsys()	      IV*/
SYS(irix_sigstack, 2)			/* 1171  sigstack()	      HV*/
SYS(irix_sigaltstack, 2)		/* 1172  sigaltstack()	      HV*/
SYS(irix_sigsendset, 2)			/* 1173  sigsendset()	      IV*/
SYS(irix_statvfs, 2)			/* 1174  statvfs()	       V*/
SYS(irix_fstatvfs, 2)			/* 1175  fstatvfs()	       V*/
SYS(irix_unimp, 0)			/* 1176  XXX getpmsg()	      DC*/
SYS(irix_unimp, 0)			/* 1177  XXX putpmsg()	      DC*/
SYS(sys_lchown, 3)			/* 1178  lchown()	       V*/
SYS(irix_priocntl, 0)			/* 1179  priocntl()	      DC*/
SYS(irix_sigqueue, 4)			/* 1180  sigqueue()	      IV*/
SYS(sys_readv, 3)			/* 1181  readv()	       V*/
SYS(sys_writev, 3)			/* 1182  writev()	       V*/
SYS(irix_truncate64, 4)			/* 1183  truncate64() XX32bit HV*/
SYS(irix_ftruncate64, 4)		/* 1184  ftruncate64()XX32bit HV*/
SYS(irix_mmap64, 0)			/* 1185  mmap64()     XX32bit HV*/
SYS(irix_dmi, 0)			/* 1186  dmi()		      DC*/
SYS(irix_pread, 6)			/* 1187  pread()	      IV*/
SYS(irix_pwrite, 6)			/* 1188  pwrite()	      IV*/
SYS(sys_fsync, 1)			/* 1189  fdatasync()  XXPOSIX HV*/
SYS(irix_sgifastpath, 7)		/* 1190  sgifastpath() WHEEE  IV*/
SYS(irix_unimp, 0)			/* 1191  XXX attr_get()	      DC*/
SYS(irix_unimp, 0)			/* 1192  XXX attr_getf()      DC*/
SYS(irix_unimp, 0)			/* 1193  XXX attr_set()	      DC*/
SYS(irix_unimp, 0)			/* 1194  XXX attr_setf()      DC*/
SYS(irix_unimp, 0)			/* 1195  XXX attr_remove()    DC*/
SYS(irix_unimp, 0)			/* 1196  XXX attr_removef()   DC*/
SYS(irix_unimp, 0)			/* 1197  XXX attr_list()      DC*/
SYS(irix_unimp, 0)			/* 1198  XXX attr_listf()     DC*/
SYS(irix_unimp, 0)			/* 1199  XXX attr_multi()     DC*/
SYS(irix_unimp, 0)			/* 1200  XXX attr_multif()    DC*/
SYS(irix_statvfs64, 2)			/* 1201  statvfs64()	       V*/
SYS(irix_fstatvfs64, 2)			/* 1202  fstatvfs64()	       V*/
SYS(irix_getmountid, 2)			/* 1203  getmountid()XXXfsids HV*/
SYS(irix_nsproc, 5)			/* 1204  nsproc()	      IV*/
SYS(irix_getdents64, 3)			/* 1205  getdents64()	      HV*/
SYS(irix_unimp, 0)			/* 1206  XXX DFS garbage      DC*/
SYS(irix_ngetdents, 4)			/* 1207  ngetdents() XXXeop   HV*/
SYS(irix_ngetdents64, 4)		/* 1208  ngetdents64() XXXeop HV*/
SYS(irix_unimp, 0)			/* 1209  nothin'	       V*/
SYS(irix_unimp, 0)			/* 1210  XXX pidsprocsp()	*/
SYS(irix_unimp, 0)			/* 1211  XXX rexec()		*/
SYS(irix_unimp, 0)			/* 1212  XXX timer_create()	*/
SYS(irix_unimp, 0)			/* 1213  XXX timer_delete()	*/
SYS(irix_unimp, 0)			/* 1214  XXX timer_settime()	*/
SYS(irix_unimp, 0)			/* 1215  XXX timer_gettime()	*/
SYS(irix_unimp, 0)			/* 1216  XXX timer_setoverrun()	*/
SYS(sys_sched_rr_get_interval, 2)	/* 1217  sched_rr_get_interval()V*/
SYS(sys_sched_yield, 0)			/* 1218  sched_yield()	       V*/
SYS(sys_sched_getscheduler, 1)		/* 1219  sched_getscheduler()  V*/
SYS(sys_sched_setscheduler, 3)		/* 1220  sched_setscheduler()  V*/
SYS(sys_sched_getparam, 2)		/* 1221  sched_getparam()      V*/
SYS(sys_sched_setparam, 2)		/* 1222  sched_setparam()      V*/
SYS(irix_unimp, 0)			/* 1223  XXX usync_cntl()	*/
SYS(irix_unimp, 0)			/* 1224  XXX psema_cntl()	*/
SYS(irix_unimp, 0)			/* 1225  XXX restartreturn()	*/

/* Just to pad things out nicely. */
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)
SYS(irix_unimp, 0)

/* YEEEEEEEEEEEEEEEEEE!!!! */
