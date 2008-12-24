#
# $P4: //depot/projects/trustedbsd/openbsm/bin/audit/Makefile.am#4 $
#

if USE_NATIVE_INCLUDES
INCLUDES = -I$(top_builddir) -I$(top_srcdir)
else
INCLUDES = -I$(top_builddir) -I$(top_srcdir) -I$(top_srcdir)/sys
endif

sbin_PROGRAMS = audit
audit_LDADD = $(top_builddir)/libbsm/libbsm.la
man8_MANS = audit.8

if USE_MACH_IPC
audit_SOURCES = auditd_control_user.c audit.c
CLEANFILES = auditd_control_user.c auditd_control_user.h

auditd_control_user.c: $(top_srcdir)/bin/auditd/auditd_control.defs 
	$(MIG)	-user auditd_control_user.c -header auditd_control_user.h -server /dev/null -sheader /dev/null $(top_srcdir)/bin/auditd/auditd_control.defs
else
audit_SOURCES = audit.c
endif
