# $Id: Makefile.am,v 1.12 2000/11/15 22:51:08 assar Exp $

include $(top_srcdir)/Makefile.am.common

INCLUDES += $(INCLUDE_krb4) $(X_CFLAGS)

WFLAGS += $(WFLAGS_NOIMPLICITINT)

if HAVE_X

bin_PROGRAMS = kx
bin_SCRIPTS = rxterm rxtelnet tenletxr
libexec_PROGRAMS = kxd

else

bin_PROGRAMS = 
bin_SCRIPTS = 
libexec_PROGRAMS = 

endif

CLEANFILES = rxterm rxtelnet tenletxr

if NEED_WRITEAUTH
XauWriteAuth_c = writeauth.c
endif

kx_SOURCES = \
	kx.c \
	kx.h \
	common.c \
	context.c \
	krb4.c \
	krb5.c \
	$(XauWriteAuth_c)

EXTRA_kx_SOURCES = writeauth.c

kxd_SOURCES = \
	kxd.c \
	kx.h \
	common.c \
	context.c \
	krb4.c \
	krb5.c \
	$(XauWriteAuth_c)

EXTRA_kxd_SOURCES = writeauth.c

EXTRA_DIST = rxterm.in rxtelnet.in tenletxr.in

man_MANS = kx.1 rxtelnet.1 rxterm.1 tenletxr.1 kxd.8

rxterm: rxterm.in
	sed -e "s!%bindir%!$(bindir)!" $(srcdir)/rxterm.in > $@
	chmod +x $@

rxtelnet: rxtelnet.in
	sed -e "s!%bindir%!$(bindir)!" $(srcdir)/rxtelnet.in > $@
	chmod +x $@

tenletxr: tenletxr.in
	sed -e "s!%bindir%!$(bindir)!" $(srcdir)/tenletxr.in > $@
	chmod +x $@

LDADD = \
	$(LIB_kafs)				\
	$(LIB_krb5) \
	$(LIB_krb4)				\
	$(LIB_des)	\
	$(LIB_roken)				\
	$(X_LIBS) $(LIB_XauReadAuth) $(X_PRE_LIBS) $(X_EXTRA_LIBS)
