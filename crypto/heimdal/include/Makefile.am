# $Id: Makefile.am,v 1.32 2002/05/24 15:36:21 joda Exp $

include $(top_srcdir)/Makefile.am.common

SUBDIRS = kadm5

noinst_PROGRAMS = bits
CHECK_LOCAL =

INCLUDES += -DHOST=\"$(CANONICAL_HOST)\"

include_HEADERS = krb5-types.h

krb5-types.h: bits$(EXEEXT)
	./bits$(EXEEXT) krb5-types.h

CLEANFILES =		\
	asn1.h		\
	asn1_err.h	\
	base64.h	\
	com_err.h	\
	com_right.h	\
	der.h		\
	des.h		\
	editline.h	\
	err.h		\
	getarg.h	\
	glob.h		\
	gssapi.h	\
	hdb.h		\
	hdb_asn1.h	\
	hdb_err.h	\
	heim_err.h	\
	kafs.h		\
	krb5-protos.h	\
	krb5-private.h	\
	krb5-types.h	\
	krb5.h		\
	krb5_err.h	\
	md4.h		\
	md5.h		\
	rc4.h		\
	otp.h		\
	parse_time.h	\
	parse_units.h	\
	resolve.h	\
	roken-common.h	\
	roken.h		\
	sha.h		\
	sl.h		\
	xdbm.h
