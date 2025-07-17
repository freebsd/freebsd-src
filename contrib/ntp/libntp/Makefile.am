NULL=
BUILT_SOURCES =
CLEANFILES =
DISTCLEANFILES =
noinst_LIBRARIES = libntp.a @MAKE_LIBNTPSIM@
EXTRA_LIBRARIES = libntpsim.a

noinst_HEADERS =				\
	lib/isc/task_p.h			\
	lib/isc/include/isc/app.h		\
	lib/isc/include/isc/assertions.h	\
	lib/isc/include/isc/backtrace.h		\
	lib/isc/include/isc/bind9.h		\
	lib/isc/include/isc/buffer.h		\
	lib/isc/include/isc/boolean.h		\
	lib/isc/include/isc/error.h		\
	lib/isc/include/isc/event.h		\
	lib/isc/include/isc/eventclass.h	\
	lib/isc/include/isc/file.h		\
	lib/isc/include/isc/formatcheck.h	\
	lib/isc/include/isc/hash.h		\
	lib/isc/include/isc/interfaceiter.h	\
	lib/isc/include/isc/lang.h		\
	lib/isc/include/isc/lib.h		\
	lib/isc/include/isc/list.h		\
	lib/isc/include/isc/log.h		\
	lib/isc/include/isc/magic.h		\
	lib/isc/include/isc/md5.h		\
	lib/isc/include/isc/msgs.h		\
	lib/isc/include/isc/msgcat.h		\
	lib/isc/include/isc/namespace.h		\
	lib/isc/include/isc/netscope.h		\
	lib/isc/include/isc/netaddr.h		\
	lib/isc/include/isc/ondestroy.h		\
	lib/isc/include/isc/platform.h		\
	lib/isc/include/isc/print.h		\
	lib/isc/include/isc/random.h		\
	lib/isc/include/isc/region.h		\
	lib/isc/include/isc/result.h		\
	lib/isc/include/isc/resultclass.h	\
	lib/isc/include/isc/sha1.h		\
	lib/isc/include/isc/sockaddr.h		\
	lib/isc/include/isc/socket.h		\
	lib/isc/include/isc/stdio.h		\
	lib/isc/include/isc/string.h		\
	lib/isc/include/isc/task.h		\
	lib/isc/include/isc/timer.h		\
	lib/isc/include/isc/types.h		\
	lib/isc/include/isc/util.h		\
	lib/isc/include/isc/xml.h		\
	lib/isc/unix/errno2result.h		\
	lib/isc/unix/include/isc/dir.h		\
	lib/isc/unix/include/isc/int.h		\
	lib/isc/unix/include/isc/net.h		\
	lib/isc/unix/include/isc/offset.h	\
	lib/isc/unix/include/isc/stat.h		\
	lib/isc/unix/include/isc/stdtime.h	\
	lib/isc/unix/include/isc/strerror.h	\
	lib/isc/unix/include/isc/time.h		\
	lib/isc/win32/include/isc/condition.h	\
	lib/isc/win32/include/isc/dir.h		\
	lib/isc/win32/include/isc/int.h		\
	lib/isc/win32/include/isc/ipv6.h	\
	lib/isc/win32/include/isc/mutex.h	\
	lib/isc/win32/include/isc/net.h		\
	lib/isc/win32/include/isc/offset.h	\
	lib/isc/win32/include/isc/once.h	\
	lib/isc/win32/include/isc/platform.h	\
	lib/isc/win32/include/isc/stat.h	\
	lib/isc/win32/include/isc/stdtime.h	\
	lib/isc/win32/include/isc/strerror.h	\
	lib/isc/win32/include/isc/thread.h	\
	lib/isc/win32/include/isc/time.h	\
	lib/isc/win32/include/isc/win32os.h	\
	$(NULL)


# ntp_crypto_rnd.c includes libevent headers for use when
# neither OpenSSL nor arc4random_buf are available.  Those
# include event2/event-config.h which is built, so if we
# are building our libevent tearoff, build it before this.

if BUILD_LIBEVENT
BUILT_SOURCES 	+= libevent_first
CLEANFILES 	+= libevent_first

libevent_first:
	cd $(top_builddir)/sntp/libevent && $(MAKE) $(AM_MAKEFLAGS)
	echo stamp > $@
endif


libisc_SRCS =							\
	lib/isc/assertions.c					\
	lib/isc/buffer.c					\
	lib/isc/backtrace-emptytbl.c				\
	lib/isc/backtrace.c					\
	lib/isc/unix/dir.c					\
	lib/isc/error.c						\
	lib/isc/unix/errno2result.c				\
	lib/isc/event.c						\
	lib/isc/unix/file.c					\
	lib/isc/inet_ntop.c					\
	lib/isc/inet_pton.c					\
	lib/isc/unix/interfaceiter.c				\
	lib/isc/lib.c						\
	lib/isc/log.c						\
	lib/isc/md5.c						\
	lib/isc/nls/msgcat.c					\
	lib/isc/unix/net.c					\
	lib/isc/netaddr.c					\
	lib/isc/netscope.c					\
	lib/isc/ondestroy.c					\
	lib/isc/random.c					\
	lib/isc/result.c					\
	lib/isc/unix/stdio.c					\
	lib/isc/unix/stdtime.c					\
	lib/isc/unix/strerror.c					\
	lib/isc/task.c						\
	lib/isc/unix/time.c					\
	lib/isc/sha1.c						\
	lib/isc/sockaddr.c					\
	lib/isc/tsmemcmp.c					\
	$(NULL)

libisc_XTRA =						\
	lib/isc/pthreads/condition.c			\
	lib/isc/pthreads/mutex.c			\
	lib/isc/pthreads/thread.c			\
	lib/isc/nothreads/condition.c			\
	lib/isc/nothreads/thread.c			\
	$(NULL)

if PTHREADS
libisc_SRCS += 						\
	lib/isc/pthreads/condition.c			\
	lib/isc/pthreads/mutex.c			\
	lib/isc/pthreads/thread.c			\
	$(NULL)
else
libisc_SRCS += 						\
	lib/isc/nothreads/condition.c			\
	lib/isc/nothreads/thread.c			\
	$(NULL)
endif

libntp_a_SRCS =						\
	a_md5encrypt.c					\
	adjtime.c					\
	atoint.c					\
	atolfp.c					\
	atouint.c					\
	audio.c						\
	authkeys.c					\
	authreadkeys.c					\
	authusekey.c					\
	bsd_strerror.c					\
	buftvtots.c					\
	caljulian.c					\
	caltontp.c					\
	calyearstart.c					\
	clocktime.c					\
	clocktypes.c					\
	decodenetnum.c					\
	dofptoa.c					\
	dolfptoa.c					\
	emalloc.c					\
	findconfig.c					\
	getopt.c					\
	hextoint.c					\
	hextolfp.c					\
	humandate.c					\
	icom.c						\
	iosignal.c					\
	is_ip_address.c					\
	lib_strbuf.c					\
	libssl_compat.c					\
	machines.c					\
	mktime.c					\
	modetoa.c					\
	mstolfp.c					\
	msyslog.c					\
	netof.c						\
	ntp_calendar.c					\
	ntp_calgps.c					\
	ntp_crypto_rnd.c				\
	ntp_intres.c					\
	ntp_libopts.c					\
	ntp_lineedit.c					\
	ntp_random.c					\
	ntp_realpath.c					\
	ntp_rfc2553.c					\
	ntp_worker.c					\
	numtoa.c					\
	octtoint.c					\
	prettydate.c					\
	refidsmear.c					\
	recvbuff.c					\
	refnumtoa.c					\
	snprintf.c					\
	socket.c					\
	socktoa.c					\
	socktohost.c					\
	ssl_init.c					\
	statestr.c					\
	strdup.c					\
	strl_obsd.c					\
	syssignal.c					\
	timespecops.c					\
	timetoa.c					\
	timevalops.c					\
	timexsup.c					\
	uglydate.c					\
	vint64ops.c					\
	work_fork.c					\
	work_thread.c					\
	xsbprintf.c					\
	ymd2yd.c					\
	$(libisc_SRCS)					\
	$(NULL)

libntp_a_SOURCES = systime.c $(libntp_a_SRCS)
libntpsim_a_SOURCES = systime_s.c $(libntp_a_SRCS)

EXTRA_libntp_a_SOURCES = 				\
	adjtimex.c					\
	$(libisc_XTRA)					\
	$(NULL)

AM_CFLAGS = $(CFLAGS_NTP)
AM_CFLAGS += $(NTP_HARD_CFLAGS)

AM_CPPFLAGS  = $(NTP_INCS)
AM_CPPFLAGS += $(LIBOPTS_CFLAGS)
AM_CPPFLAGS += $(CPPFLAGS_LIBEVENT)
AM_CPPFLAGS += $(CPPFLAGS_NTP)
AM_CPPFLAGS += $(NTP_HARD_CPPFLAGS)

AM_LDFLAGS = $(NTP_HARD_LDFLAGS)

EXTRA_DIST = 					\
	README					\
	lib/isc/nothreads/include		\
	lib/isc/pthreads/include		\
	lib/isc/unix/ifiter_getifaddrs.c	\
	lib/isc/unix/ifiter_ioctl.c		\
	lib/isc/unix/ifiter_sysctl.c		\
	lib/isc/win32/condition.c		\
	lib/isc/win32/interfaceiter.c		\
	lib/isc/win32/net.c			\
	lib/isc/win32/once.c			\
	lib/isc/win32/stdtime.c			\
	lib/isc/win32/strerror.c		\
	lib/isc/win32/thread.c			\
	lib/isc/win32/time.c			\
	lib/isc/win32/win32os.c			\
	$(NULL)

include $(top_srcdir)/depsver.mf
include $(top_srcdir)/includes.mf
