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

_PRIVATELIBS=	\
		atf_c \
		atf_cxx \
		bsdstat \
		devdctl \
		event \
		heimipcc \
		heimipcs \
		ldns \
		sqlite3 \
		ssh \
		ucl \
		unbound \
		zstd

_INTERNALLIBS=	\
		amu \
		bfd \
		binutils \
		bsnmptools \
		cron \
		elftc \
		fifolog \
		gdb \
		iberty \
		ipf \
		lpr \
		netbsd \
		ntp \
		ntpevent \
		opcodes \
		openbsd \
		opts \
		parse \
		pe \
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
		${LOCAL_LIBRARIES} \
		80211 \
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
		c_cheri \
		c_nosyscalls \
		c_pic \
		calendar \
		cam \
		casper \
		cheri \
		cheri_support \
		cheri_syscalls \
		cap_dns \
		cap_grp \
		cap_pwd \
		cap_random \
		cap_sysctl \
		com_err \
		compiler_rt \
		crypt \
		crypto \
		css \
		ctf \
		curl \
		cuse \
		cxxrt \
		devctl \
		devdctl \
		devinfo \
		devstat \
		dialog \
		dom \
		dpv \
		dtrace \
		dwarf \
		edit \
		efivar \
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
		helloworld \
		heimbase \
		heimntlm \
		heimsqlite \
		hubbub \
		hx509 \
		ifconfig \
		ipsec \
		jail \
		jpeg \
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
		malloc_simple \
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
		nsbmp \
		nsfb \
		nsgif \
		nv \
		nvpair \
		opie \
		pam \
		panel \
		panelw \
		parserutils \
		pcap \
		pcsclite \
		pjdlog \
		pmc \
		png \
		png_sb \
		proc \
		procstat \
		pthread \
		radius \
		readline \
		roken \
		rosprite \
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
		svgtiny \
		syscalls \
		sysdecode \
		tacplus \
		termcap \
		termcapw \
		ufs \
		ugidfw \
		ulog \
		umem \
		usb \
		usbhid \
		util \
		uutil \
		vmmapi \
		wapcaplet \
		wind \
		wrap \
		xo \
		y \
		ypclnt \
		z \
		zfs_core \
		zfs \
		zpool \

.if ${MK_BLACKLIST} != "no"
_LIBRARIES+= \
		blacklist \

.endif

.if ${MK_OFED} != "no"
_LIBRARIES+= \
		cxgb4 \
		ibcm \
		ibcommon \
		ibmad \
		ibsdp \
		ibumad \
		ibverbs \
		mlx4 \
		mthca \
		opensm \
		osmcomp \
		osmvendor \
		rdmacm \

.endif

# Each library's LIBADD needs to be duplicated here for static linkage of
# 2nd+ order consumers.  Auto-generating this would be better.
_DP_80211=	sbuf bsdxml
_DP_archive=	z bz2 lzma bsdxml
.if ${MK_BLACKLIST} != "no"
_DP_blacklist+=	pthread
.endif
.if ${MK_OPENSSL} != "no"
_DP_archive+=	crypto
.else
_DP_archive+=	md
.endif
_DP_sqlite3=	pthread
_DP_ssl=	crypto
_DP_ssh=	crypto crypt z
.if ${MK_LDNS} != "no"
_DP_ssh+=	ldns
.endif
_DP_edit=	ncursesw
.if ${MK_OPENSSL} != "no"
_DP_bsnmp=	crypto
.endif
_DP_geom=	bsdxml sbuf
_DP_cam=	sbuf
_DP_kvm=	elf
_DP_casper=	nv
_DP_cap_dns=	nv
_DP_cap_grp=	nv
_DP_cap_pwd=	nv
_DP_cap_random=	nv
_DP_cap_sysctl=	nv
_DP_pjdlog=	util
_DP_png=	z
_DP_opie=	md
_DP_usb=	pthread
_DP_unbound=	ssl crypto pthread
_DP_rt=	pthread
.if ${MK_OPENSSL} == "no"
_DP_radius=	md
.else
_DP_radius=	crypto
.endif
_DP_rtld_db=	elf procstat
_DP_procstat=	kvm util elf
.if ${MK_CXX} == "yes" && !defined(LIBCHERI)
.if ${MK_LIBCPLUSPLUS} != "no"
_DP_proc=	cxxrt
.else
_DP_proc=	supcplusplus
.endif
.endif
.if ${MK_CDDL} != "no"
_DP_proc+=	ctf
.endif
_DP_proc+=	elf procstat rtld_db util
_DP_mp=	crypto
_DP_memstat=	kvm
_DP_magic=	z
_DP_mt=		sbuf bsdxml
_DP_ldns=	crypto
.if ${MK_OPENSSL} != "no"
_DP_fetch=	ssl crypto
.else
_DP_fetch=	md
.endif
_DP_execinfo=	elf
_DP_dwarf=	elf
_DP_dpv=	dialog figpar util ncursesw
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
_DP_readline=	ncursesw
_DP_roken=	crypt
_DP_kadm5clnt=	com_err krb5 roken
_DP_kadm5srv=	com_err hdb krb5 roken
_DP_heimntlm=	crypto com_err krb5 roken
_DP_hx509=	asn1 com_err crypto roken wind
_DP_hdb=	asn1 com_err krb5 roken sqlite3
_DP_asn1=	com_err roken
_DP_kdc=	roken hdb hx509 krb5 heimntlm asn1 crypto
_DP_wind=	com_err roken
_DP_heimbase=	pthread
_DP_heimipcc=	heimbase roken pthread
_DP_heimipcs=	heimbase roken pthread
_DP_kafs5=	asn1 krb5 roken
_DP_krb5+=	asn1 com_err crypt crypto hx509 roken wind heimbase heimipcc
_DP_gssapi_krb5+=	gssapi krb5 crypto roken asn1 com_err
_DP_lzma=	pthread
_DP_ucl=	m
_DP_vmmapi=	util
_DP_ctf=	z
_DP_dtrace=	ctf elf proc pthread rtld_db
_DP_xo=		util
# The libc dependencies are not strictly needed but are defined to make the
# assert happy.
_DP_c=		compiler_rt
_DP_c_nosyscalls=		compiler_rt
.if ${MK_SSP} != "no"
_DP_c+=		ssp_nonshared
_DP_c_nosyscalls+=		ssp_nonshared
.endif
_DP_stdthreads=	pthread
_DP_tacplus=	md
_DP_panel=	ncurses
_DP_panelw=	ncursesw
_DP_rpcsec_gss=	gssapi
_DP_smb=	kiconv
_DP_ulog=	md
_DP_fifolog=	z
_DP_ipf=	kvm
_DP_zfs=	md pthread umem util uutil m nvpair avl bsdxml geom nvpair z \
		zfs_core
_DP_zfs_core=	nvpair
_DP_zpool=	md pthread z nvpair avl umem
.if ${MK_OFED} != "no"
_DP_cxgb4=	ibverbs pthread
_DP_ibcm=	ibverbs
_DP_ibmad=	ibcommon ibumad
_DP_ibumad=	ibcommon
_DP_mlx4=	ibverbs pthread
_DP_mthca=	ibverbs pthread
_DP_opensm=	pthread
_DP_osmcomp=	pthread
_DP_osmvendor=	ibumad opensm osmcomp pthread
_DP_rdmacm=	ibverbs
.endif

_DP_helloworld=	cheri

# Define special cases
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
# Add in all dependencies for static linkage.
.if defined(_DP_${_l}) && (${_INTERNALLIBS:M${_l}} || \
    (defined(NO_SHARED) && (${NO_SHARED} != "no" && ${NO_SHARED} != "NO")))
.for _d in ${_DP_${_l}}
DPADD_${_l}+=	${DPADD_${_d}}
LDADD_${_l}+=	${LDADD_${_d}}
.endfor
.endif
.endfor

# These are special cases where the library is broken and anything that uses
# it needs to add more dependencies.  Broken usually means that it has a
# cyclic dependency and cannot link its own dependencies.  This is bad, please
# fix the library instead.
# Unless the library itself is broken then the proper place to define
# dependencies is _DP_* above.

# libatf-c++ exposes libatf-c abi hence we need to explicit link to atf_c for
# atf_cxx
DPADD_atf_cxx+=	${DPADD_atf_c}
LDADD_atf_cxx+=	${LDADD_atf_c}

# Detect LDADD/DPADD that should be LIBADD, before modifying LDADD here.
_BADLDADD=
.for _l in ${LDADD:M-l*:N-l*/*:C,^-l,,}
.if ${_LIBRARIES:M${_l}} && !${_PRIVATELIBS:M${_l}}
_BADLDADD+=	${_l}
.endif
.endfor
.if !empty(_BADLDADD)
.error ${.CURDIR}: These libraries should be LIBADD+=foo rather than DPADD/LDADD+=-lfoo: ${_BADLDADD}
.endif

.for _l in ${LIBADD}
DPADD+=		${DPADD_${_l}}
LDADD+=		${LDADD_${_l}}
.endfor

_LIB_OBJTOP?=	${OBJTOP}

# INTERNALLIB definitions.
LIBELFTCDIR=	${_LIB_OBJTOP}/lib/libelftc
LIBELFTC?=	${LIBELFTCDIR}/libelftc.a

LIBPEDIR=	${_LIB_OBJTOP}/lib/libpe
LIBPE?=		${LIBPEDIR}/libpe.a

LIBREADLINEDIR=	${_LIB_OBJTOP}/gnu/lib/libreadline/readline
LIBREADLINE?=	${LIBREADLINEDIR}/libreadline.a

LIBOPENBSDDIR=	${_LIB_OBJTOP}/lib/libopenbsd
LIBOPENBSD?=	${LIBOPENBSDDIR}/libopenbsd.a

LIBSMDIR=	${_LIB_OBJTOP}/lib/libsm
LIBSM?=		${LIBSMDIR}/libsm.a

LIBSMDBDIR=	${_LIB_OBJTOP}/lib/libsmdb
LIBSMDB?=	${LIBSMDBDIR}/libsmdb.a

LIBSMUTILDIR=	${_LIB_OBJTOP}/lib/libsmutil
LIBSMUTIL?=	${LIBSMDBDIR}/libsmutil.a

LIBNETBSDDIR?=	${_LIB_OBJTOP}/lib/libnetbsd
LIBNETBSD?=	${LIBNETBSDDIR}/libnetbsd.a

LIBVERSDIR?=	${_LIB_OBJTOP}/kerberos5/lib/libvers
LIBVERS?=	${LIBVERSDIR}/libvers.a

LIBSLDIR=	${_LIB_OBJTOP}/kerberos5/lib/libsl
LIBSL?=		${LIBSLDIR}/libsl.a

LIBIPFDIR=	${_LIB_OBJTOP}/sbin/ipf/libipf
LIBIPF?=	${LIBIPFDIR}/libipf.a

LIBTELNETDIR=	${_LIB_OBJTOP}/lib/libtelnet
LIBTELNET?=	${LIBTELNETDIR}/libtelnet.a

LIBCRONDIR=	${_LIB_OBJTOP}/usr.sbin/cron/lib
LIBCRON?=	${LIBCRONDIR}/libcron.a

LIBNTPDIR=	${_LIB_OBJTOP}/usr.sbin/ntp/libntp
LIBNTP?=	${LIBNTPDIR}/libntp.a

LIBNTPEVENTDIR=	${_LIB_OBJTOP}/usr.sbin/ntp/libntpevent
LIBNTPEVENT?=	${LIBNTPEVENTDIR}/libntpevent.a

LIBOPTSDIR=	${_LIB_OBJTOP}/usr.sbin/ntp/libopts
LIBOPTS?=	${LIBOPTSDIR}/libopts.a

LIBPARSEDIR=	${_LIB_OBJTOP}/usr.sbin/ntp/libparse
LIBPARSE?=	${LIBPARSEDIR}/libparse.a

LIBLPRDIR=	${_LIB_OBJTOP}/usr.sbin/lpr/common_source
LIBLPR?=	${LIBOPTSDIR}/liblpr.a

LIBFIFOLOGDIR=	${_LIB_OBJTOP}/usr.sbin/fifolog/lib
LIBFIFOLOG?=	${LIBOPTSDIR}/libfifolog.a

LIBBSNMPTOOLSDIR=	${_LIB_OBJTOP}/usr.sbin/bsnmpd/tools/libbsnmptools
LIBBSNMPTOOLS?=	${LIBBSNMPTOOLSDIR}/libbsnmptools.a

LIBAMUDIR=	${_LIB_OBJTOP}/usr.sbin/amd/libamu
LIBAMU?=	${LIBAMUDIR}/libamu/libamu.a

LIBBFDDIR=	${_LIB_OBJTOP}/gnu/usr.bin/binutils/libbfd
LIBBFD?=	${LIBBFDDIR}/libbfd.a

LIBBINUTILSDIR=	${_LIB_OBJTOP}/gnu/usr.bin/binutils/libbinutils
LIBBINUTILS?=	${LIBBINUTILSDIR}/libbinutils.a

LIBGDBDIR=	${_LIB_OBJTOP}/gnu/usr.bin/gdb/libgdb
LIBGDB?=	${LIBGDBDIR}/libgdb.a

LIBIBERTYDIR=	${_LIB_OBJTOP}/gnu/usr.bin/binutils/libiberty
LIBIBERTY?=	${LIBIBERTYDIR}/libiberty.a

LIBOPCODESDIR=	${_LIB_OBJTOP}/gnu/usr.bin/binutils/libopcodes
LIBOPCODES?=	${LIBOPCODESDIR}/libopcodes.a

# Define a directory for each library.  This is useful for adding -L in when
# not using a --sysroot or for meta mode bootstrapping when there is no
# Makefile.depend.  These are sorted by directory.
LIBAVLDIR=	${_LIB_OBJTOP}/cddl/lib/libavl
LIBCTFDIR=	${_LIB_OBJTOP}/cddl/lib/libctf
LIBDTRACEDIR=	${_LIB_OBJTOP}/cddl/lib/libdtrace
LIBNVPAIRDIR=	${_LIB_OBJTOP}/cddl/lib/libnvpair
LIBUMEMDIR=	${_LIB_OBJTOP}/cddl/lib/libumem
LIBUUTILDIR=	${_LIB_OBJTOP}/cddl/lib/libuutil
LIBZFSDIR=	${_LIB_OBJTOP}/cddl/lib/libzfs
LIBZFS_COREDIR=	${_LIB_OBJTOP}/cddl/lib/libzfs_core
LIBZPOOLDIR=	${_LIB_OBJTOP}/cddl/lib/libzpool
LIBCXGB4DIR=	${_LIB_OBJTOP}/contrib/ofed/usr.lib/libcxgb4
LIBIBCMDIR=	${_LIB_OBJTOP}/contrib/ofed/usr.lib/libibcm
LIBIBCOMMONDIR=	${_LIB_OBJTOP}/contrib/ofed/usr.lib/libibcommon
LIBIBMADDIR=	${_LIB_OBJTOP}/contrib/ofed/usr.lib/libibmad
LIBIBUMADDIR=	${_LIB_OBJTOP}/contrib/ofed/usr.lib/libibumad
LIBIBVERBSDIR=	${_LIB_OBJTOP}/contrib/ofed/usr.lib/libibverbs
LIBMLX4DIR=	${_LIB_OBJTOP}/contrib/ofed/usr.lib/libmlx4
LIBMTHCADIR=	${_LIB_OBJTOP}/contrib/ofed/usr.lib/libmthca
LIBOPENSMDIR=	${_LIB_OBJTOP}/contrib/ofed/usr.lib/libopensm
LIBOSMCOMPDIR=	${_LIB_OBJTOP}/contrib/ofed/usr.lib/libosmcomp
LIBOSMVENDORDIR=	${_LIB_OBJTOP}/contrib/ofed/usr.lib/libosmvendor
LIBRDMACMDIR=	${_LIB_OBJTOP}/contrib/ofed/usr.lib/librdmacm
LIBIBSDPDIR=	${_LIB_OBJTOP}/contrib/ofed/usr.lib/libsdp
LIBDIALOGDIR=	${_LIB_OBJTOP}/gnu/lib/libdialog
LIBGCOVDIR=	${_LIB_OBJTOP}/gnu/lib/libgcov
LIBGOMPDIR=	${_LIB_OBJTOP}/gnu/lib/libgomp
LIBGNUREGEXDIR=	${_LIB_OBJTOP}/gnu/lib/libregex
LIBSSPDIR=	${_LIB_OBJTOP}/gnu/lib/libssp
LIBSSP_NONSHAREDDIR=	${_LIB_OBJTOP}/gnu/lib/libssp/libssp_nonshared
LIBSUPCPLUSPLUSDIR=	${_LIB_OBJTOP}/gnu/lib/libsupc++
LIBASN1DIR=	${_LIB_OBJTOP}/kerberos5/lib/libasn1
LIBGSSAPI_KRB5DIR=	${_LIB_OBJTOP}/kerberos5/lib/libgssapi_krb5
LIBGSSAPI_NTLMDIR=	${_LIB_OBJTOP}/kerberos5/lib/libgssapi_ntlm
LIBGSSAPI_SPNEGODIR=	${_LIB_OBJTOP}/kerberos5/lib/libgssapi_spnego
LIBHDBDIR=	${_LIB_OBJTOP}/kerberos5/lib/libhdb
LIBHEIMBASEDIR=	${_LIB_OBJTOP}/kerberos5/lib/libheimbase
LIBHEIMIPCCDIR=	${_LIB_OBJTOP}/kerberos5/lib/libheimipcc
LIBHEIMIPCSDIR=	${_LIB_OBJTOP}/kerberos5/lib/libheimipcs
LIBHEIMNTLMDIR=	${_LIB_OBJTOP}/kerberos5/lib/libheimntlm
LIBHX509DIR=	${_LIB_OBJTOP}/kerberos5/lib/libhx509
LIBKADM5CLNTDIR=	${_LIB_OBJTOP}/kerberos5/lib/libkadm5clnt
LIBKADM5SRVDIR=	${_LIB_OBJTOP}/kerberos5/lib/libkadm5srv
LIBKAFS5DIR=	${_LIB_OBJTOP}/kerberos5/lib/libkafs5
LIBKDCDIR=	${_LIB_OBJTOP}/kerberos5/lib/libkdc
LIBKRB5DIR=	${_LIB_OBJTOP}/kerberos5/lib/libkrb5
LIBROKENDIR=	${_LIB_OBJTOP}/kerberos5/lib/libroken
LIBWINDDIR=	${_LIB_OBJTOP}/kerberos5/lib/libwind
LIBATF_CDIR=	${_LIB_OBJTOP}/lib/atf/libatf-c
LIBATF_CXXDIR=	${_LIB_OBJTOP}/lib/atf/libatf-c++
LIBALIASDIR=	${_LIB_OBJTOP}/lib/libalias/libalias
LIBBLACKLISTDIR=	${_LIB_OBJTOP}/lib/libblacklist
LIBBLOCKSRUNTIMEDIR=	${_LIB_OBJTOP}/lib/libblocksruntime
LIBBSNMPDIR=	${_LIB_OBJTOP}/lib/libbsnmp/libbsnmp
LIBCASPERDIR=	${_LIB_OBJTOP}/lib/libcasper/libcasper
LIBCAP_DNSDIR=	${_LIB_OBJTOP}/lib/libcasper/services/cap_dns
LIBCAP_GRPDIR=	${_LIB_OBJTOP}/lib/libcasper/services/cap_grp
LIBCAP_PWDDIR=	${_LIB_OBJTOP}/lib/libcasper/services/cap_pwd
LIBCAP_RANDOMDIR=	${_LIB_OBJTOP}/lib/libcasper/services/cap_random
LIBCAP_SYSCTLDIR=	${_LIB_OBJTOP}/lib/libcasper/services/cap_sysctl
LIBBSDXMLDIR=	${_LIB_OBJTOP}/lib/libexpat
LIBKVMDIR=	${_LIB_OBJTOP}/lib/libkvm
LIBPTHREADDIR=	${_LIB_OBJTOP}/lib/libthr
LIBMDIR=	${_LIB_OBJTOP}/lib/msun
LIBFORMDIR=	${_LIB_OBJTOP}/lib/ncurses/form
LIBFORMLIBWDIR=	${_LIB_OBJTOP}/lib/ncurses/formw
LIBMENUDIR=	${_LIB_OBJTOP}/lib/ncurses/menu
LIBMENULIBWDIR=	${_LIB_OBJTOP}/lib/ncurses/menuw
LIBNCURSESDIR=	${_LIB_OBJTOP}/lib/ncurses/ncurses
LIBNCURSESWDIR=	${_LIB_OBJTOP}/lib/ncurses/ncursesw
LIBPANELDIR=	${_LIB_OBJTOP}/lib/ncurses/panel
LIBPANELWDIR=	${_LIB_OBJTOP}/lib/ncurses/panelw
LIBCRYPTODIR=	${_LIB_OBJTOP}/secure/lib/libcrypto
LIBSSHDIR=	${_LIB_OBJTOP}/secure/lib/libssh
LIBSSLDIR=	${_LIB_OBJTOP}/secure/lib/libssl
LIBTEKENDIR=	${_LIB_OBJTOP}/sys/teken/libteken
LIBEGACYDIR=	${_LIB_OBJTOP}/tools/build
LIBLNDIR=	${_LIB_OBJTOP}/usr.bin/lex/lib

LIBTERMCAPDIR=	${LIBNCURSESDIR}
LIBTERMCAPWDIR=	${LIBNCURSESWDIR}

# Default other library directories to lib/libNAME.
.for lib in ${_LIBRARIES}
LIB${lib:tu}DIR?=	${_LIB_OBJTOP}/lib/lib${lib}
.endfor

# Validate that listed LIBADD are valid.
.for _l in ${LIBADD}
.if empty(_LIBRARIES:M${_l})
_BADLIBADD+= ${_l}
.endif
.endfor
.if !empty(_BADLIBADD)
.error ${.CURDIR}: Invalid LIBADD used which may need to be added to ${_this:T}: ${_BADLIBADD}
.endif

# Sanity check that libraries are defined here properly when building them.
.if defined(LIB) && ${_LIBRARIES:M${LIB}} != ""
.if !empty(LIBADD) && \
    (!defined(_DP_${LIB}) || ${LIBADD:O:u} != ${_DP_${LIB}:O:u})
.error ${.CURDIR}: Missing or incorrect _DP_${LIB} entry in ${_this:T}.  Should match LIBADD for ${LIB} ('${LIBADD}' vs '${_DP_${LIB}}')
.endif
# Note that OBJTOP is not yet defined here but for the purpose of the check
# it is fine as it resolves to the SRC directory.
.if !defined(LIB${LIB:tu}DIR) || !exists(${SRCTOP}/${LIB${LIB:tu}DIR:S,^${_LIB_OBJTOP}/,,})
.error ${.CURDIR}: Missing or incorrect value for LIB${LIB:tu}DIR in ${_this:T}: ${LIB${LIB:tu}DIR:S,^${_LIB_OBJTOP}/,,}
.endif
.if ${_INTERNALLIBS:M${LIB}} != "" && !defined(LIB${LIB:tu})
.error ${.CURDIR}: Missing value for LIB${LIB:tu} in ${_this:T}.  Likely should be: LIB${LIB:tu}?= $${LIB${LIB:tu}DIR}/lib${LIB}.a
.endif
.endif

.endif	# !target(__<src.libnames.mk>__)
