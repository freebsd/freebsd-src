# $Id: Makefile.am,v 1.122.6.3 2003/10/14 16:13:15 joda Exp $

include $(top_srcdir)/Makefile.am.common

ACLOCAL_AMFLAGS = -I ../../cf

CLEANFILES = roken.h make-roken.c $(XHEADERS)

lib_LTLIBRARIES = libroken.la
libroken_la_LDFLAGS = -version-info 16:3:0

noinst_PROGRAMS = make-roken snprintf-test

nodist_make_roken_SOURCES = make-roken.c

check_PROGRAMS = 				\
		base64-test			\
		getaddrinfo-test		\
		parse_bytes-test		\
		parse_reply-test		\
		snprintf-test			\
		strpftime-test

TESTS = $(check_PROGRAMS)

LDADD = libroken.la $(LIB_crypt)
make_roken_LDADD = 

noinst_LTLIBRARIES = libtest.la
libtest_la_SOURCES = strftime.c strptime.c snprintf.c
libtest_la_CFLAGS = -DTEST_SNPRINTF

parse_reply_test_SOURCES = parse_reply-test.c resolve.c
parse_reply_test_CFLAGS  = -DTEST_RESOLVE

strpftime_test_SOURCES	= strpftime-test.c
strpftime_test_LDADD = libtest.la $(LDADD)
snprintf_test_SOURCES	= snprintf-test.c
snprintf_test_LDADD = libtest.la $(LDADD)
snprintf_test_CFLAGS	= -DTEST_SNPRINTF

libroken_la_SOURCES =		\
	base64.c		\
	bswap.c			\
	concat.c		\
	environment.c		\
	eread.c			\
	esetenv.c		\
	ewrite.c		\
	getaddrinfo_hostspec.c	\
	get_default_username.c	\
	get_window_size.c	\
	getarg.c		\
	getnameinfo_verified.c	\
	getprogname.c		\
	h_errno.c		\
	hostent_find_fqdn.c	\
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
	setprogname.c		\
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
	err.hin			\
	glob.hin		\
	ifaddrs.hin		\
	vis.hin	

EXTRA_DIST = roken.awk roken.h.in

libroken_la_LIBADD = @LTLIBOBJS@ $(DBLIB)

$(LTLIBOBJS) $(libroken_la_OBJECTS): roken.h $(XHEADERS)

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

man_MANS = getarg.3

SUFFIXES += .hin
.hin.h:
	cp $< $@

roken.h: make-roken$(EXEEXT)
	@./make-roken$(EXEEXT) > tmp.h ;\
	if [ -f roken.h ] && cmp -s tmp.h roken.h ; then rm -f tmp.h ; \
	else rm -f roken.h; mv tmp.h roken.h; fi

make-roken.c: roken.h.in roken.awk
	$(AWK) -f $(srcdir)/roken.awk $(srcdir)/roken.h.in > make-roken.c
