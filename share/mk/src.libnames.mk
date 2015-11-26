# $FreeBSD$
#
# The include file <src.libnames.mk> define library names suitable
# for INTERNALLIB and PRIVATELIB definition

.if !target(__<bsd.init.mk>__)
.error src.libnames.mk cannot be included directly.
.endif

.if !target(__<src.libnames.mk>__)
__<src.libnames.mk>__:

.include <src.opts.mk>

.if ${.OBJDIR:S,${.CURDIR},,} != ${.OBJDIR}
ROOTOBJDIR=	${.OBJDIR:S,${.CURDIR},,}${SRCTOP}
.elif defined(OBJTOP) && ${.OBJDIR:M${OBJTOP}*} != ""
ROOTOBJDIR=	${OBJTOP}
.endif

_PRIVATELIBS=	\
		atf_c \
		atf_cxx \
		bsdstat \
		event \
		heimipcc \
		heimipcs \
		ldns \
		sqlite3 \
		ssh \
		ucl \
		unbound

_INTERNALLIBS=	\
		amu \
		bsnmptools \
		cron \
		elftc \
		fifolog \
		ipf \
		lpr \
		netbsd \
		ntp \
		ntpevent \
		openbsd \
		opts \
		parse \
		readline \
		sl \
		sm \
		smdb \
		smutil \
		telnet \
		vers

_LIBRARIES=	\
		${_PRIVATELIBS} \
		${_INTERNALLIBS} \
		alias \
		archive \
		asn1 \
		auditd \
		avl \
		begemot \
		bluetooth \
		bsdxml \
		bsm \
		bsnmp \
		bz2 \
		c \
		c_pic \
		calendar \
		cam \
		capsicum \
		casper \
		com_err \
		compiler_rt \
		crypt \
		crypto \
		ctf \
		cuse \
		cxxrt \
		devctl \
		devinfo \
		devstat \
		dialog \
		dpv \
		dtrace \
		dwarf \
		edit \
		elf \
		execinfo \
		fetch \
		figpar \
		geom \
		gnuregex \
		gpio \
		gssapi \
		gssapi_krb5 \
		hdb \
		heimbase \
		heimntlm \
		heimsqlite \
		hx509 \
		ipsec \
		jail \
		kadm5clnt \
		kadm5srv \
		kafs5 \
		kdc \
		kiconv \
		krb5 \
		kvm \
		l \
		lzma \
		m \
		magic \
		md \
		memstat \
		mp \
		mt \
		nandfs \
		ncurses \
		ncursesw \
		netgraph \
		ngatm \
		nv \
		opie \
		pam \
		panel \
		panelw \
		pcap \
		pcsclite \
		pjdlog \
		pmc \
		proc \
		procstat \
		pthread \
		radius \
		readline \
		roken \
		rpcsec_gss \
		rpcsvc \
		rt \
		rtld_db \
		sbuf \
		sdp \
		sm \
		smb \
		ssl \
		ssp_nonshared \
		stdthreads \
		supcplusplus \
		tacplus \
		termcapw \
		ufs \
		ugidfw \
		ulog \
		usb \
		usbhid \
		util \
		vmmapi \
		wind \
		wrap \
		xo \
		y \
		ypclnt \
		z

_DP_archive=	z bz2 lzma bsdxml
.if ${MK_OPENSSL} != "no"
_DP_archive+=	crypto
.else
_DP_archive+=	md
.endif
_DP_ssl=	crypto
_DP_ssh=	crypto crypt
.if ${MK_LDNS} != "no"
_DP_ssh+=	ldns z
.endif
_DP_edit=	ncursesw
.if ${MK_OPENSSL} != "no"
_DP_bsnmp=	crypto
.endif
_DP_geom=	bsdxml sbuf
_DP_cam=	sbuf
_DP_casper=	capsicum nv pjdlog
_DP_capsicum=	nv
_DP_pjdlog=	util
_DP_opie=	md
_DP_usb=	pthread
_DP_unbound=	pthread
_DP_rt=	pthread
.if ${MK_OPENSSL} == "no"
_DP_radius=	md
.else
_DP_radius=	crypto
.endif
_DP_procstat=	kvm util elf
.if ${MK_CXX} == "yes"
.if ${MK_LIBCPLUSPLUS} != "no"
_DP_proc=	cxxrt
.else
_DP_proc=	supcplusplus
.endif
.endif
.if ${MK_CDDL} != "no"
_DP_proc+=	ctf
.endif
_DP_mp=	crypto
_DP_memstat=	kvm
_DP_magic=	z
_DP_mt=		bsdxml
_DP_ldns=	crypto
.if ${MK_OPENSSL} != "no"
_DP_fetch=	ssl crypto
.else
_DP_fetch=	md
.endif
_DP_execinfo=	elf
_DP_dwarf=	elf
_DP_dpv=	dialog figpar util
_DP_dialog=	ncursesw m
_DP_cuse=	pthread
_DP_atf_cxx=	atf_c
_DP_devstat=	kvm
_DP_pam=	radius tacplus opie md util
.if ${MK_KERBEROS} != "no"
_DP_pam+=	krb5
.endif
.if ${MK_OPENSSH} != "no"
_DP_pam+=	ssh
.endif
.if ${MK_NIS} != "no"
_DP_pam+=	ypclnt
.endif
_DP_krb5+=	asn1 com_err crypt crypto hx509 roken wind heimbase heimipcc \
		pthread
_DP_gssapi_krb5+=	gssapi krb5 crypto roken asn1 com_err
_DP_lzma=	pthread
_DP_ucl=	m
_DP_vmmapi=	util
_DP_ctf=	z
_DP_proc=	rtld_db util
_DP_dtrace=	rtld_db pthread
_DP_xo=		util

# Define spacial cases
LDADD_supcplusplus=	-lsupc++
LIBATF_C=	${DESTDIR}${LIBDIR}/libprivateatf-c.a
LIBATF_CXX=	${DESTDIR}${LIBDIR}/libprivateatf-c++.a
LDADD_atf_c=	-lprivateatf-c
LDADD_atf_cxx=	-lprivateatf-c++

.for _l in ${_PRIVATELIBS}
LIB${_l:tu}?=	${DESTDIR}${LIBDIR}/libprivate${_l}.a
.endfor

.for _l in ${_LIBRARIES}
.if ${_INTERNALLIBS:M${_l}}
LDADD_${_l}_L+=		-L${LIB${_l:tu}DIR}
.endif
DPADD_${_l}?=	${LIB${_l:tu}}
.if ${_PRIVATELIBS:M${_l}}
LDADD_${_l}?=	-lprivate${_l}
.else
LDADD_${_l}?=	${LDADD_${_l}_L} -l${_l}
.endif
.if defined(_DP_${_l}) && defined(NO_SHARED) && (${NO_SHARED} != "no" && ${NO_SHARED} != "NO")
.for _d in ${_DP_${_l}}
DPADD_${_l}+=	${DPADD_${_d}}
LDADD_${_l}+=	${LDADD_${_d}}
.endfor
.endif
.endfor

DPADD_atf_cxx+=	${DPADD_atf_c}
LDADD_atf_cxx+=	${LDADD_atf_c}

DPADD_sqlite3+=	${DPADD_pthread}
LDADD_sqlite3+=	${LDADD_pthread}

DPADD_fifolog+=	${DPADD_z}
LDADD_fifolog+=	${LDADD_z}

DPADD_ipf+=	${DPADD_kvm}
LDADD_ipf+=	${LDADD_kvm}

DPADD_mt+=	${DPADD_sbuf}
LDADD_mt+=	${LDADD_sbuf}

DPADD_dtrace+=	${DPADD_ctf} ${DPADD_elf} ${DPADD_proc}
LDADD_dtrace+=	${LDADD_ctf} ${LDADD_elf} ${LDADD_proc}

# The following depends on libraries which are using pthread
DPADD_hdb+=	${DPADD_pthread}
LDADD_hdb+=	${LDADD_pthread}
DPADD_kadm5srv+=	${DPADD_pthread}
LDADD_kadm5srv+=	${LDADD_pthread}
DPADD_krb5+=	${DPADD_pthread}
LDADD_krb5+=	${LDADD_pthread}
DPADD_gssapi_krb5+=	${DPADD_pthread}
LDADD_gssapi_krb5+=	${LDADD_pthread}

.for _l in ${LIBADD}
DPADD+=		${DPADD_${_l}:Umissing-dpadd_${_l}}
LDADD+=		${LDADD_${_l}}
.endfor

.if defined(DPADD) && ${DPADD:Mmissing-dpadd_*}
.error Missing ${DPADD:Mmissing-dpadd_*:S/missing-dpadd_//:S/^/DPADD_/} variable add "${DPADD:Mmissing-dpadd_*:S/missing-dpadd_//}" to _LIBRARIES, _INTERNALLIBS, or _PRIVATELIBS and define "${DPADD:Mmissing-dpadd_*:S/missing-dpadd_//:S/^/LIB/:tu}".
.endif

LIBELFTCDIR=	${ROOTOBJDIR}/lib/libelftc
LIBELFTC?=	${LIBELFTCDIR}/libelftc.a

LIBREADLINEDIR=	${ROOTOBJDIR}/gnu/lib/libreadline/readline
LIBREADLINE?=	${LIBREADLINEDIR}/libreadline.a

LIBOPENBSDDIR=	${ROOTOBJDIR}/lib/libopenbsd
LIBOPENBSD?=	${LIBOPENBSDDIR}/libopenbsd.a

LIBSMDIR=	${ROOTOBJDIR}/lib/libsm
LIBSM?=		${LIBSMDIR}/libsm.a

LIBSMDBDIR=	${ROOTOBJDIR}/lib/libsmdb
LIBSMDB?=	${LIBSMDBDIR}/libsmdb.a

LIBSMUTILDIR=	${ROOTOBJDIR}/lib/libsmutil
LIBSMUTIL?=	${LIBSMDBDIR}/libsmutil.a

LIBNETBSDDIR?=	${ROOTOBJDIR}/lib/libnetbsd
LIBNETBSD?=	${LIBNETBSDDIR}/libnetbsd.a

LIBVERSDIR?=	${ROOTOBJDIR}/kerberos5/lib/libvers
LIBVERS?=	${LIBVERSDIR}/libvers.a

LIBSLDIR=	${ROOTOBJDIR}/kerberos5/lib/libsl
LIBSL?=		${LIBSLDIR}/libsl.a

LIBIPFDIR=	${ROOTOBJDIR}/sbin/ipf/libipf
LIBIPF?=	${LIBIPFDIR}/libipf.a

LIBTELNETDIR=	${ROOTOBJDIR}/lib/libtelnet
LIBTELNET?=	${LIBTELNETDIR}/libtelnet.a

LIBCRONDIR=	${ROOTOBJDIR}/usr.sbin/cron/lib
LIBCRON?=	${LIBCRONDIR}/libcron.a

LIBNTPDIR=	${ROOTOBJDIR}/usr.sbin/ntp/libntp
LIBNTP?=	${LIBNTPDIR}/libntp.a

LIBNTPEVENTDIR=	${ROOTOBJDIR}/usr.sbin/ntp/libntpevent
LIBNTPEVENT?=	${LIBNTPEVENTDIR}/libntpevent.a

LIBOPTSDIR=	${ROOTOBJDIR}/usr.sbin/ntp/libopts
LIBOTPS?=	${LIBOPTSDIR}/libopts.a

LIBPARSEDIR=	${ROOTOBJDIR}/usr.sbin/ntp/libparse
LIBPARSE?=	${LIBPARSEDIR}/libparse.a

LIBLPRDIR=	${ROOTOBJDIR}/usr.sbin/lpr/common_source
LIBLPR?=	${LIBOPTSDIR}/liblpr.a

LIBFIFOLOGDIR=	${ROOTOBJDIR}/usr.sbin/fifolog/lib
LIBFIFOLOG?=	${LIBOPTSDIR}/libfifolog.a

LIBBSNMPTOOLSDIR=	${ROOTOBJDIR}/usr.sbin/bsnmpd/tools/libbsnmptools
LIBBSNMPTOOLS?=	${LIBBSNMPTOOLSDIR}/libbsnmptools.a

LIBAMUDIR=	${ROOTOBJDIR}/usr.sbin/amd/libamu
LIBAMU?=	${LIBAMUDIR}/libamu/libamu.a

# Define a directory for each library.  This is useful for adding -L in when
# not using a --sysroot or for meta mode bootstrapping when there is no
# Makefile.depend.  These are sorted by directory.
LIBAVLDIR=	${ROOTOBJDIR}/cddl/lib/libavl
LIBCTFDIR=	${ROOTOBJDIR}/cddl/lib/libctf
LIBDTRACEDIR=	${ROOTOBJDIR}/cddl/lib/libdtrace
LIBNVPAIRDIR=	${ROOTOBJDIR}/cddl/lib/libnvpair
LIBUMEMDIR=	${ROOTOBJDIR}/cddl/lib/libumem
LIBUUTILDIR=	${ROOTOBJDIR}/cddl/lib/libuutil
LIBZFSDIR=	${ROOTOBJDIR}/cddl/lib/libzfs
LIBZFS_COREDIR=	${ROOTOBJDIR}/cddl/lib/libzfs_core
LIBZPOOLDIR=	${ROOTOBJDIR}/cddl/lib/libzpool
LIBDIALOGDIR=	${ROOTOBJDIR}/gnu/lib/libdialog
LIBGCOVDIR=	${ROOTOBJDIR}/gnu/lib/libgcov
LIBGOMPDIR=	${ROOTOBJDIR}/gnu/lib/libgomp
LIBGNUREGEXDIR=	${ROOTOBJDIR}/gnu/lib/libregex
LIBSSPDIR=	${ROOTOBJDIR}/gnu/lib/libssp
LIBSSP_NONSHAREDDIR=	${ROOTOBJDIR}/gnu/lib/libssp/libssp_nonshared
LIBSUPCPLUSPLUSDIR=	${ROOTOBJDIR}/gnu/lib/libsupc++
LIBASN1DIR=	${ROOTOBJDIR}/kerberos5/lib/libasn1
LIBGSSAPI_KRB5DIR=	${ROOTOBJDIR}/kerberos5/lib/libgssapi_krb5
LIBGSSAPI_NTLMDIR=	${ROOTOBJDIR}/kerberos5/lib/libgssapi_ntlm
LIBGSSAPI_SPNEGODIR=	${ROOTOBJDIR}/kerberos5/lib/libgssapi_spnego
LIBHDBDIR=	${ROOTOBJDIR}/kerberos5/lib/libhdb
LIBHEIMBASEDIR=	${ROOTOBJDIR}/kerberos5/lib/libheimbase
LIBHEIMIPCCDIR=	${ROOTOBJDIR}/kerberos5/lib/libheimipcc
LIBHEIMIPCSDIR=	${ROOTOBJDIR}/kerberos5/lib/libheimipcs
LIBHEIMNTLMDIR=	${ROOTOBJDIR}/kerberos5/lib/libheimntlm
LIBHX509DIR=	${ROOTOBJDIR}/kerberos5/lib/libhx509
LIBKADM5CLNTDIR=	${ROOTOBJDIR}/kerberos5/lib/libkadm5clnt
LIBKADM5SRVDIR=	${ROOTOBJDIR}/kerberos5/lib/libkadm5srv
LIBKAFS5DIR=	${ROOTOBJDIR}/kerberos5/lib/libkafs5
LIBKDCDIR=	${ROOTOBJDIR}/kerberos5/lib/libkdc
LIBKRB5DIR=	${ROOTOBJDIR}/kerberos5/lib/libkrb5
LIBROKENDIR=	${ROOTOBJDIR}/kerberos5/lib/libroken
LIBWINDDIR=	${ROOTOBJDIR}/kerberos5/lib/libwind
LIBALIASDIR=	${ROOTOBJDIR}/lib/libalias/libalias
LIBBLOCKSRUNTIMEDIR=	${ROOTOBJDIR}/lib/libblocksruntime
LIBBSNMPDIR=	${ROOTOBJDIR}/lib/libbsnmp/libbsnmp
LIBBSDXMLDIR=	${ROOTOBJDIR}/lib/libexpat
LIBKVMDIR=	${ROOTOBJDIR}/lib/libkvm
LIBPTHREADDIR=	${ROOTOBJDIR}/lib/libthr
LIBMDIR=	${ROOTOBJDIR}/lib/msun
LIBFORMDIR=	${ROOTOBJDIR}/lib/ncurses/form
LIBFORMLIBWDIR=	${ROOTOBJDIR}/lib/ncurses/formw
LIBMENUDIR=	${ROOTOBJDIR}/lib/ncurses/menu
LIBMENULIBWDIR=	${ROOTOBJDIR}/lib/ncurses/menuw
LIBTERMCAPDIR=	${ROOTOBJDIR}/lib/ncurses/ncurses
LIBTERMCAPWDIR=	${ROOTOBJDIR}/lib/ncurses/ncursesw
LIBPANELDIR=	${ROOTOBJDIR}/lib/ncurses/panel
LIBPANELWDIR=	${ROOTOBJDIR}/lib/ncurses/panelw
LIBCRYPTODIR=	${ROOTOBJDIR}/secure/lib/libcrypto
LIBSSHDIR=	${ROOTOBJDIR}/secure/lib/libssh
LIBSSLDIR=	${ROOTOBJDIR}/secure/lib/libssl
LIBTEKENDIR=	${ROOTOBJDIR}/sys/teken/libteken
LIBEGACYDIR=	${ROOTOBJDIR}/tools/build
LIBLNDIR=	${ROOTOBJDIR}/usr.bin/lex/lib

# Default other library directories to lib/libNAME.
.for lib in ${_LIBRARIES}
LIB${lib:tu}DIR?=	${ROOTOBJDIR}/lib/lib${lib}
.endfor

.endif	# !target(__<src.libnames.mk>__)
