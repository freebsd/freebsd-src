#
# $P4: //depot/projects/trustedbsd/openbsm/libbsm/Makefile.am#5 $
#

if USE_NATIVE_INCLUDES
INCLUDES = -I$(top_builddir) -I$(top_srcdir)
else
INCLUDES = -I$(top_builddir) -I$(top_srcdir) -I$(top_srcdir)/sys
endif

lib_LTLIBRARIES = libbsm.la

libbsm_la_SOURCES =	\
	bsm_audit.c	\
	bsm_class.c	\
	bsm_control.c	\
	bsm_event.c	\
	bsm_flags.c	\
	bsm_io.c	\
	bsm_mask.c	\
	bsm_token.c	\
	bsm_user.c

if HAVE_AUDIT_SYSCALLS
libbsm_la_SOURCES +=	\
	bsm_notify.c	\
	bsm_wrappers.c
endif

man3_MANS =		\
	au_class.3	\
	au_control.3	\
	au_event.3	\
	au_free_token.3	\
	au_io.3		\
	au_mask.3	\
	au_open.3	\
	au_token.3	\
	au_user.3	\
	libbsm.3

