#	@(#)Make.tags.inc	8.1 (Berkeley) 6/11/93
# $FreeBSD$

# Common files for "make tags".
# Included by the Makefile for each architecture.

# Put the ../sys stuff near the end so that subroutine definitions win when
# there is a struct tag with the same name (eg., vmmeter).  The real
# solution would probably be for ctags to generate "struct vmmeter" tags.

COMM=	/sys/conf/*.[ch] \
	/sys/dev/*.[ch] /sys/dev/scsi/*.[ch] \
	/sys/kern/*.[ch] /sys/libkern/*.[ch] \
	/sys/miscfs/*/*.[ch] \
	/sys/net/*.[ch] /sys/netccitt/*.[ch] /sys/netinet/*.[ch] \
	/sys/netiso/*.[ch] /sys/netns/*.[ch] \
	/sys/nfs/*.[ch] /sys/sys/*.[ch] \
	/sys/ufs/*/*.[ch] \
	/sys/vm/*.[ch]
