# $Id: Makefile.am,v 1.68 2002/03/10 23:41:33 assar Exp $

include $(top_srcdir)/Makefile.am.common

YFLAGS = -d

lib_LTLIBRARIES = libasn1.la
libasn1_la_LDFLAGS = -version-info 6:0:0

libasn1_la_LIBADD = @LIB_com_err@

BUILT_SOURCES =			\
	$(gen_files:.x=.c)	\
	asn1_err.h		\
	asn1_err.c

gen_files =					\
	asn1_APOptions.x			\
	asn1_AP_REP.x				\
	asn1_AP_REQ.x				\
	asn1_AS_REP.x				\
	asn1_AS_REQ.x				\
	asn1_Authenticator.x			\
	asn1_AuthorizationData.x		\
	asn1_CKSUMTYPE.x			\
	asn1_Checksum.x				\
	asn1_ENCTYPE.x				\
	asn1_ETYPE_INFO.x			\
	asn1_ETYPE_INFO_ENTRY.x			\
	asn1_EncAPRepPart.x			\
	asn1_EncASRepPart.x			\
	asn1_EncKDCRepPart.x			\
	asn1_EncKrbCredPart.x			\
	asn1_EncKrbPrivPart.x			\
	asn1_EncTGSRepPart.x			\
	asn1_EncTicketPart.x			\
	asn1_EncryptedData.x			\
	asn1_EncryptionKey.x			\
	asn1_HostAddress.x			\
	asn1_HostAddresses.x			\
	asn1_KDCOptions.x			\
	asn1_KDC_REP.x				\
	asn1_KDC_REQ.x				\
	asn1_KDC_REQ_BODY.x			\
	asn1_KRB_CRED.x				\
	asn1_KRB_ERROR.x			\
	asn1_KRB_PRIV.x				\
	asn1_KRB_SAFE.x				\
	asn1_KRB_SAFE_BODY.x			\
	asn1_KerberosTime.x			\
	asn1_KrbCredInfo.x			\
	asn1_LastReq.x				\
	asn1_LR_TYPE.x				\
	asn1_MESSAGE_TYPE.x			\
	asn1_METHOD_DATA.x			\
	asn1_NAME_TYPE.x			\
	asn1_PADATA_TYPE.x			\
	asn1_PA_DATA.x				\
	asn1_PA_ENC_TS_ENC.x			\
	asn1_Principal.x			\
	asn1_PrincipalName.x			\
	asn1_Realm.x				\
	asn1_TGS_REP.x				\
	asn1_TGS_REQ.x				\
	asn1_Ticket.x				\
	asn1_TicketFlags.x			\
	asn1_TransitedEncoding.x		\
	asn1_UNSIGNED.x


noinst_PROGRAMS = asn1_compile asn1_print
check_PROGRAMS = check-der
TESTS = check-der

asn1_compile_SOURCES = 				\
	gen.c					\
	gen_copy.c				\
	gen_decode.c				\
	gen_encode.c				\
	gen_free.c				\
	gen_glue.c				\
	gen_length.c				\
	hash.c					\
	lex.l					\
	main.c					\
	parse.y					\
	symbol.c

libasn1_la_SOURCES =				\
	der_get.c				\
	der_put.c				\
	der_free.c				\
	der_length.c				\
	der_copy.c				\
	timegm.c				\
	$(BUILT_SOURCES)

asn1_compile_LDADD = \
	$(LIB_roken) $(LEXLIB)

check_der_LDADD = \
	libasn1.la \
	$(LIB_roken)

asn1_print_LDADD = $(check_der_LDADD)

CLEANFILES = lex.c parse.c parse.h krb5_asn1.h $(BUILT_SOURCES) \
	$(gen_files) asn1_files

include_HEADERS = krb5_asn1.h asn1_err.h der.h

$(asn1_compile_OBJECTS): parse.h parse.c

$(gen_files) krb5_asn1.h: asn1_files

asn1_files: asn1_compile$(EXEEXT) $(srcdir)/k5.asn1
	./asn1_compile$(EXEEXT) $(srcdir)/k5.asn1 krb5_asn1

$(libasn1_la_OBJECTS): krb5_asn1.h asn1_err.h

$(asn1_print_OBJECTS): krb5_asn1.h

EXTRA_DIST = asn1_err.et
