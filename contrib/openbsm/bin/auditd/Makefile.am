#
# $P4: //depot/projects/trustedbsd/openbsm/bin/auditd/Makefile.am#4 $
#

if USE_NATIVE_INCLUDES
INCLUDES = -I$(top_builddir) -I$(top_srcdir)
else
INCLUDES = -I$(top_builddir) -I$(top_srcdir) -I$(top_srcdir)/sys
endif

sbin_PROGRAMS = auditd
auditd_LDADD = $(top_builddir)/libbsm/libbsm.la
man8_MANS = auditd.8

if USE_MACH_IPC
auditd_SOURCES = auditd_control_server.c audit_triggers_server.c audit_warn.c auditd.c
CLEANFILES = auditd_control_server.c auditd_control_server.h audit_triggers_server.c audit_triggers_server.h

auditd_control_server.c: auditd_control.defs
	$(MIG) -user /dev/null -header /dev/null -server auditd_control_server.c -sheader auditd_control_server.h $(top_srcdir)/bin/auditd/auditd_control.defs

audit_triggers_server.c: audit_triggers.defs
	$(MIG) -user /dev/null -header /dev/null -server audit_triggers_server.c -sheader audit_triggers_server.h $(top_srcdir)/bin/auditd/audit_triggers.defs
else
auditd_SOURCES = audit_warn.c auditd.c
endif
