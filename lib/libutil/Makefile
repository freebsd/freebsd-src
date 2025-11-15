SHLIBDIR?= /lib

.include <src.opts.mk>

PACKAGE=	runtime

.PATH: ${SRCTOP}/lib/libc/gen # for pw_scan.c

LIB=	util
SHLIB_MAJOR= 10

SRCS=	_secure_path.c cpuset.c expand_number.c flopen.c fparseln.c \
	ftime.c getlocalbase.c gr_util.c \
	hexdump.c humanize_number.c kinfo_getfile.c \
	kinfo_getallproc.c kinfo_getproc.c kinfo_getvmmap.c \
	kinfo_getvmobject.c kld.c \
	login_auth.c login_cap.c \
	login_class.c login_crypt.c login_ok.c login_times.c login_tty.c \
	mntopts.c \
	pidfile.c property.c pty.c pw_scan.c pw_util.c quotafile.c \
	realhostname.c stub.c trimdomain.c uucplock.c
INCS=	libutil.h login_cap.h mntopts.h

CFLAGS+= -DNO__SCCSID

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+= -DINET6
.endif

CFLAGS+= -I${.CURDIR} -I${SRCTOP}/lib/libc/gen/

VERSION_DEF=	${SRCTOP}/lib/libc/Versions.def
SYMBOL_MAPS=	${.CURDIR}/Symbol.map

MAN+=	cpuset.3
MLINKS+=cpuset.3 cpuset_parselist.3
MLINKS+=cpuset.3 domainset_parselist.3
MAN+=	expand_number.3
MAN+=	flopen.3
MLINKS+=flopen.3 flopenat.3
MAN+=	fparseln.3
MAN+=	ftime.3
MAN+=	getlocalbase.3
MAN+=	hexdump.3
MAN+=	humanize_number.3
MAN+=	kinfo_getallproc.3
MAN+=	kinfo_getfile.3
MAN+=	kinfo_getproc.3
MAN+=	kinfo_getvmmap.3
MAN+=	kinfo_getvmobject.3
MAN+=	kld.3
MLINKS+=kld.3 kld_isloaded.3
MLINKS+=kld.3 kld_load.3
MAN+=	login_auth.3
MLINKS+=login_auth.3 auth_cat.3
MLINKS+=login_auth.3 auth_checknologin.3
MAN+=	login_cap.3
MLINKS+=login_cap.3 login_close.3
MLINKS+=login_cap.3 login_getcapbool.3
MLINKS+=login_cap.3 login_getcapenum.3
MLINKS+=login_cap.3 login_getcaplist.3
MLINKS+=login_cap.3 login_getcapnum.3
MLINKS+=login_cap.3 login_getcapsize.3
MLINKS+=login_cap.3 login_getcapstr.3
MLINKS+=login_cap.3 login_getcaptime.3
MLINKS+=login_cap.3 login_getclass.3
MLINKS+=login_cap.3 login_getclassbyname.3
MLINKS+=login_cap.3 login_getpath.3
MLINKS+=login_cap.3 login_getpwclass.3
MLINKS+=login_cap.3 login_getstyle.3
MLINKS+=login_cap.3 login_getuserclass.3
MLINKS+=login_cap.3 login_setcryptfmt.3
MAN+=	login_class.3
MLINKS+=login_class.3 setclasscontext.3
MLINKS+=login_class.3 setclasscpumask.3
MLINKS+=login_class.3 setclassenvironment.3
MLINKS+=login_class.3 setclassresources.3
MLINKS+=login_class.3 setusercontext.3
MAN+=	login_ok.3
MLINKS+=login_ok.3 auth_hostok.3
MLINKS+=login_ok.3 auth_timeok.3
MLINKS+=login_ok.3 auth_ttyok.3
MAN+=	login_times.3
MLINKS+=login_times.3 in_lt.3
MLINKS+=login_times.3 in_ltm.3
MLINKS+=login_times.3 in_ltms.3
MLINKS+=login_times.3 in_lts.3
MLINKS+=login_times.3 parse_lt.3
MAN+=	login_tty.3
MAN+=	mntopts.3
MLINKS+=mntopts.3 build_iovec.3
MLINKS+=mntopts.3 build_iovec_argf.3
MLINKS+=mntopts.3 checkpath.3
MLINKS+=mntopts.3 chkdoreload.3
MLINKS+=mntopts.3 free_iovec.3
MLINKS+=mntopts.3 getmntopts.3
MLINKS+=mntopts.3 getmntpoint.3
MLINKS+=mntopts.3 rmslashes.3
MAN+=	pidfile.3
MLINKS+=pidfile.3 pidfile_close.3
MLINKS+=pidfile.3 pidfile_fileno.3
MLINKS+=pidfile.3 pidfile_open.3
MLINKS+=pidfile.3 pidfile_remove.3
MLINKS+=pidfile.3 pidfile_signal.3
MLINKS+=pidfile.3 pidfile_write.3
MAN+=	property.3
MLINKS+=property.3 property_find.3
MLINKS+=property.3 properties_free.3
MLINKS+=property.3 properties_read.3
MAN+=	pty.3
MLINKS+=pty.3 forkpty.3
MLINKS+=pty.3 openpty.3
MAN+=	pw_util.3
MLINKS+=pw_util.3 pw_copy.3
MLINKS+=pw_util.3 pw_dup.3
MLINKS+=pw_util.3 pw_edit.3
MLINKS+=pw_util.3 pw_equal.3
MLINKS+=pw_util.3 pw_fini.3
MLINKS+=pw_util.3 pw_init.3
MLINKS+=pw_util.3 pw_initpwd.3
MLINKS+=pw_util.3 pw_make.3
MLINKS+=pw_util.3 pw_make_v7.3
MLINKS+=pw_util.3 pw_mkdb.3
MLINKS+=pw_util.3 pw_lock.3
MLINKS+=pw_util.3 pw_scan.3
MLINKS+=pw_util.3 pw_tempname.3
MLINKS+=pw_util.3 pw_tmp.3
MAN+=	quotafile.3
MLINKS+=quotafile.3 quota_check_path.3
MLINKS+=quotafile.3 quota_close.3
MLINKS+=quotafile.3 quota_convert.3
MLINKS+=quotafile.3 quota_fsname.3
MLINKS+=quotafile.3 quota_maxid.3
MLINKS+=quotafile.3 quota_off.3
MLINKS+=quotafile.3 quota_on.3
MLINKS+=quotafile.3 quota_open.3
MLINKS+=quotafile.3 quota_qfname.3
MLINKS+=quotafile.3 quota_read.3
MLINKS+=quotafile.3 quota_write_limits.3
MLINKS+=quotafile.3 quota_write_usage.3
MAN+=	realhostname.3
MAN+=	realhostname_sa.3
MAN+=	_secure_path.3
MAN+=	trimdomain.3
MAN+=	uucplock.3
MLINKS+=uucplock.3 uu_lock.3
MLINKS+=uucplock.3 uu_lock_txfr.3
MLINKS+=uucplock.3 uu_lockerr.3
MLINKS+=uucplock.3 uu_unlock.3

MANNODEV=	login.conf.5

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.lib.mk>
