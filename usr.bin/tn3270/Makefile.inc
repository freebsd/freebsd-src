# $Header: /a/cvs/386BSD/src/usr.bin/tn3270/Makefile.inc,v 1.1 1993/06/29 11:39:53 nate Exp $

CFLAGS += -DTERMCAP -DSRCRT -DKLUDGELINEMODE -DUSE_TERMIO -DTN3270
KBD = unix.kbd

.include "${.CURDIR}/../../Makefile.inc"
