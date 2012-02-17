#
# $P4: //depot/projects/trustedbsd/openbsm/bin/auditd/Makefile.am#1 $
#

INCLUDES = -I$(top_srcdir)

sbin_PROGRAMS = auditd
auditd_SOURCES = audit_warn.c auditd.c
auditd_LDADD = $(top_builddir)/libbsm/libbsm.la
man8_MANS = auditd.8
