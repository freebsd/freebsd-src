# $Id: Makefile.am,v 1.54 1999/12/03 04:04:13 assar Exp $

include $(top_srcdir)/Makefile.am.common

CLEANFILES = roken.h make-roken.c print_version.h

lib_LTLIBRARIES = libroken.la
libroken_la_LDFLAGS = -version-info 3:2:0

noinst_PROGRAMS = make-roken make-print-version

check_PROGRAMS = parse_bytes-test strpftime-test getaddrinfo-test
TESTS = $(check_PROGRAMS)

getaddrinfo_test_LDADD = libroken.la
parse_bytes_test_LDADD = libroken.la
strpftime_test_LDADD = strftime.o strptime.o

if KRB4
if KRB5
## need to link with des here; otherwise, if krb4 is shared the link
## will fail with unresolved references
make_print_version_LDADD += $(LIB_krb4) -ldes
endif
endif

libroken_la_SOURCES =		\
	base64.c		\
	concat.c		\
	emalloc.c		\
	eread.c			\
	erealloc.c		\
	estrdup.c		\
	ewrite.c		\
	get_default_username.c	\
	get_window_size.c	\
	getarg.c		\
	inaddr2str.c		\
	issuid.c		\
	k_getpwnam.c		\
	k_getpwuid.c		\
	mini_inetd.c		\
	net_read.c		\
	net_write.c		\
	parse_bytes.c		\
	parse_time.c		\
	parse_units.c		\
	print_version.c		\
	resolve.c		\
	roken_gethostby.c	\
	signal.c		\
	simple_exec.c		\
	snprintf.c		\
	socket.c		\
	tm2time.c		\
	verify.c		\
	warnerr.c		\
	xdbm.h

EXTRA_libroken_la_SOURCES =	\
	chown.c			\
	copyhostent.c		\
	daemon.c		\
	err.c			\
	err.h			\
	errx.c			\
	fchown.c		\
	flock.c			\
	fnmatch.c		\
	fnmatch.h		\
	freeaddrinfo.c		\
	freehostent.c		\
	gai_strerror.c		\
	getaddrinfo.c		\
	getdtablesize.c		\
	getegid.c		\
	geteuid.c		\
	getgid.c		\
	gethostname.c		\
	getipnodebyaddr.c	\
	getipnodebyname.c	\
	getnameinfo.c		\
	getopt.c		\
	gettimeofday.c		\
	getuid.c		\
	getusershell.c		\
	glob.h			\
	hstrerror.c		\
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

EXTRA_DIST = resource.h roken.awk roken.def roken.dsp roken.h.in \
	roken.mak roken.rc



libroken_la_LIBADD = @LTLIBOBJS@

$(LTLIBOBJS) $(libroken_la_OBJECTS): roken.h

include_HEADERS = $(err_h) base64.h getarg.h \
	parse_bytes.h parse_time.h parse_units.h \
	resolve.h roken.h roken-common.h

build_HEADERZ = $(err_h) $(fnmatch_h) $(glob_h) xdbm.h

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

roken.h: make-roken
	@./make-roken > tmp.h ;\
	if [ -f roken.h ] && cmp -s tmp.h roken.h ; then rm -f tmp.h ; \
	else rm -f roken.h; mv tmp.h roken.h; fi

make-roken.c: roken.h.in roken.awk
	$(AWK) -f $(srcdir)/roken.awk $(srcdir)/roken.h.in > make-roken.c

print_version.lo: print_version.h

print_version.h: make-print-version
	./make-print-version print_version.h

make-print-version.o: $(top_builddir)/include/version.h
