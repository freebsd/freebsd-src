# $Id: Makefile.am,v 1.96 2001/05/16 23:57:10 assar Exp $

AUTOMAKE_OPTIONS = foreign no-dependencies

AM_CFLAGS += $(WFLAGS)

## ACLOCAL = @ACLOCAL@ -I cf
ACLOCAL_AMFLAGS = -I ../../cf

CLEANFILES = roken.h make-roken.c $(XHEADERS)

lib_LTLIBRARIES = libroken.la
libroken_la_LDFLAGS = -version-info 12:0:3

noinst_PROGRAMS = make-roken

nodist_make_roken_SOURCES = make-roken.c

check_PROGRAMS = parse_bytes-test \
		strpftime-test \
		getaddrinfo-test

TESTS = $(check_PROGRAMS)

LIB_crypt = @LIB_crypt@

common_LDADD = libroken.la $(LIB_crypt)

strpftime_test_SOURCES = strpftime-test.c strftime.c strptime.c snprintf.c
##snprintf_test_SOURCES  = snprintf-test.c snprintf.c
##snprintf_test_LDADD    = $(common_LDADD) -lm
getaddrinfo_test_LDADD = $(common_LDADD)
parse_bytes_test_LDADD = $(common_LDADD)

libroken_la_SOURCES =		\
	base64.c		\
	bswap.c			\
	concat.c		\
	emalloc.c		\
	environment.c		\
	eread.c			\
	erealloc.c		\
	esetenv.c		\
	estrdup.c		\
	ewrite.c		\
	getaddrinfo_hostspec.c	\
	get_default_username.c	\
	get_window_size.c	\
	getarg.c		\
	getnameinfo_verified.c	\
	issuid.c		\
	k_getpwnam.c		\
	k_getpwuid.c		\
	mini_inetd.c		\
	net_read.c		\
	net_write.c		\
	parse_bytes.c		\
	parse_time.c		\
	parse_units.c		\
	resolve.c		\
	roken_gethostby.c	\
	rtbl.c			\
	rtbl.h			\
	signal.c		\
	simple_exec.c		\
	snprintf.c		\
	socket.c		\
	strcollect.c		\
	timeval.c		\
	tm2time.c		\
	unvis.c			\
	verify.c		\
	vis.c			\
	vis.h			\
	warnerr.c		\
	write_pid.c		\
	xdbm.h

EXTRA_libroken_la_SOURCES =	\
	chown.c			\
	copyhostent.c		\
	daemon.c		\
	err.c			\
	err.hin			\
	errx.c			\
	fchown.c		\
	flock.c			\
	fnmatch.c		\
	fnmatch.hin		\
	freeaddrinfo.c		\
	freehostent.c		\
	gai_strerror.c		\
	getaddrinfo.c		\
	getdtablesize.c		\
	getegid.c		\
	geteuid.c		\
	getgid.c		\
	gethostname.c		\
	getifaddrs.c		\
	getipnodebyaddr.c	\
	getipnodebyname.c	\
	getopt.c		\
	gettimeofday.c		\
	getuid.c		\
	getusershell.c		\
	glob.hin		\
	hstrerror.c		\
	ifaddrs.hin		\
	inet_aton.c		\
	inet_ntop.c		\
	inet_pton.c		\
	initgroups.c		\
	innetgr.c		\
	iruserok.c		\
	lstat.c			\
	memmove.c		\
	mkstemp.c		\
	putenv.c		\
	rcmd.c			\
	readv.c			\
	recvmsg.c		\
	sendmsg.c		\
	setegid.c		\
	setenv.c		\
	seteuid.c		\
	strcasecmp.c		\
	strdup.c		\
	strerror.c		\
	strftime.c		\
	strlcat.c		\
	strlcpy.c		\
	strlwr.c		\
	strncasecmp.c		\
	strndup.c		\
	strnlen.c		\
	strptime.c		\
	strsep.c		\
	strsep_copy.c		\
	strtok_r.c		\
	strupr.c		\
	swab.c			\
	unsetenv.c		\
	verr.c			\
	verrx.c			\
	vsyslog.c		\
	vwarn.c			\
	vwarnx.c		\
	warn.c			\
	warnx.c			\
	writev.c

EXTRA_DIST = roken.awk roken.h.in

libroken_la_LIBADD = @LTLIBOBJS@

$(LTLIBOBJS) $(libroken_la_OBJECTS): $(include_HEADERS) roken.h

BUILT_SOURCES = make-roken.c roken.h

if have_err_h
err_h =
else
err_h = err.h
endif

if have_fnmatch_h
fnmatch_h =
else
fnmatch_h = fnmatch.h
endif

if have_glob_h
glob_h =
else
glob_h = glob.h
endif

if have_ifaddrs_h
ifaddrs_h =
else
ifaddrs_h = ifaddrs.h
endif

if have_vis_h
vis_h = 
else
vis_h = vis.h
endif

## these are controlled by configure
XHEADERS = $(err_h) $(fnmatch_h) $(glob_h) $(ifaddrs_h) $(vis_h)

include_HEADERS = 				\
	base64.h				\
	getarg.h				\
	parse_bytes.h 				\
	parse_time.h 				\
	parse_units.h				\
	resolve.h 				\
	roken-common.h 				\
	rtbl.h 					\
	xdbm.h					\
	$(XHEADERS) 

nodist_include_HEADERS = roken.h


SUFFIXES += .hin
.hin.h:
	cp $< $@

roken.h: make-roken$(EXEEXT)
	@./make-roken$(EXEEXT) > tmp.h ;\
	if [ -f roken.h ] && cmp -s tmp.h roken.h ; then rm -f tmp.h ; \
	else rm -f roken.h; mv tmp.h roken.h; fi

make-roken.c: roken.h.in roken.awk
	$(AWK) -f $(srcdir)/roken.awk $(srcdir)/roken.h.in > make-roken.c
