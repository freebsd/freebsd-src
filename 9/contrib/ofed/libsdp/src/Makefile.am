lib_LTLIBRARIES = libsdp.la $(LIB_SDP_SYS)
if WANT_LIBSDP_SYS
LIB_SDP_SYS = libsdp_sys.la
endif

AM_CFLAGS = -Wall -DSYSCONFDIR=\"$(sysconfdir)\"

libsdp_la_SOURCES = log.c match.c port.c config_parser.c config_scanner.c
libsdp_la_LDFLAGS = -version-info 1 -ldl -lc
if SOLARIS_BUILD
libsdp_la_LDFLAGS += -lsocket
endif
libsdp_sys_la_SOURCES = socket.c
libsdp_sys_la_LDFLAGS = -version-info 1

EXTRA_DIST = libsdp.h linux/sdp_inet.h config_parser.h

config_parser.c: @MAINTAINER_MODE_TRUE@ $(srcdir)/config_parser.y
	$(YACC) -y -plibsdp_yy -d $(srcdir)/config_parser.y
	mv y.tab.c config_parser.c
	mv y.tab.h config_parser.h
	cp -f config_parser.c config_parser.h $(srcdir)/

config_scanner.c: @MAINTAINER_MODE_TRUE@ $(srcdir)/config_scanner.l
	$(LEX) -Plibsdp_yy $(srcdir)/config_scanner.l 
	mv lex.libsdp_yy.c config_scanner.c
	cp -f config_scanner.c $(srcdir)/config_scanner.c

nobase_include_HEADERS = linux/sdp_inet.h
