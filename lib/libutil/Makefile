#	@(#)Makefile	8.1 (Berkeley) 6/4/93
#	$FreeBSD$

SHLIBDIR?= /lib

.include <bsd.own.mk>

LIB=	util
SHLIB_MAJOR= 9

SRCS=	_secure_path.c auth.c expand_number.c flopen.c fparseln.c gr_util.c \
	hexdump.c humanize_number.c kinfo_getfile.c kinfo_getvmmap.c kld.c \
	login_auth.c login_cap.c \
	login_class.c login_crypt.c login_ok.c login_times.c login_tty.c \
	pidfile.c property.c pty.c pw_util.c quotafile.c realhostname.c \
	stub.c trimdomain.c uucplock.c
INCS=	libutil.h login_cap.h

CFLAGS+= -DLIBC_SCCS

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+= -DINET6
.endif

CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../libc/gen/

MAN+=	kld.3 login_auth.3 login_tty.3 pty.3 \
	login_cap.3 login_class.3 login_times.3 login_ok.3 \
	_secure_path.3 uucplock.3 property.3 auth.3 realhostname.3 \
	realhostname_sa.3 trimdomain.3 fparseln.3 humanize_number.3 \
	pidfile.3 flopen.3 expand_number.3 hexdump.3 \
	kinfo_getfile.3 kinfo_getvmmap.3 quotafile.3
MAN+=	login.conf.5 auth.conf.5
MLINKS+= kld.3 kld_isloaded.3 kld.3 kld_load.3
MLINKS+= property.3 properties_read.3  property.3 properties_free.3
MLINKS+= property.3 property_find.3
MLINKS+= auth.3 auth_getval.3
MLINKS+= pty.3 openpty.3  pty.3 forkpty.3
MLINKS+=login_cap.3 login_getclassbyname.3 login_cap.3 login_close.3 \
	login_cap.3 login_getclass.3 login_cap.3 login_getuserclass.3 \
	login_cap.3 login_getcapstr.3 login_cap.3 login_getcaplist.3 \
	login_cap.3 login_getstyle.3 login_cap.3 login_getcaptime.3 \
	login_cap.3 login_getcapnum.3 login_cap.3 login_getcapsize.3 \
	login_cap.3 login_getcapbool.3 login_cap.3 login_getpath.3 \
	login_cap.3 login_getpwclass.3 login_cap.3 login_setcryptfmt.3
MLINKS+=login_class.3 setusercontext.3 login_class.3 setclasscontext.3 \
	login_class.3 setclassenvironment.3 login_class.3 setclassresources.3
MLINKS+=login_times.3 parse_lt.3 login_times.3 in_ltm.3 \
	login_times.3 in_lt.3 login_times.3 in_ltms.3 \
	login_times.3 in_lts.3
MLINKS+=login_ok.3 auth_ttyok.3 login_ok.3 auth_hostok.3 \
	login_ok.3 auth_timeok.3
MLINKS+=login_auth.3 auth_checknologin.3 login_auth.3 auth_cat.3
MLINKS+=uucplock.3 uu_lock.3 uucplock.3 uu_lock_txfr.3 \
	uucplock.3 uu_unlock.3 uucplock.3 uu_lockerr.3
MLINKS+=pidfile.3 pidfile_open.3 \
	pidfile.3 pidfile_write.3 \
	pidfile.3 pidfile_close.3 \
	pidfile.3 pidfile_remove.3
MLINKS+=quotafile.3 quota_open.3 \
	quotafile.3 quota_fsname.3 \
	quotafile.3 quota_qfname.3 \
	quotafile.3 quota_statfs.3 \
	quotafile.3 quota_read.3 \
	quotafile.3 quota_write_limits.3 \
	quotafile.3 quota_write_usage.3 \
	quotafile.3 quota_close.3

.include <bsd.lib.mk>
