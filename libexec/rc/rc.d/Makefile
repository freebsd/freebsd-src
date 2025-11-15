.include <src.opts.mk>

CONFDIR=	/etc/rc.d
CONFGROUPS=	CONFS
CONFSPACKAGE=	rc

# Files which are always installed and go in the -rc package.
CONFS=	DAEMON \
	FILESYSTEMS \
	LOGIN \
	NETWORKING \
	SERVERS \
	adjkerntz \
	bgfsck \
	bridge \
	cfumass \
	cleanvar \
	cleartmp \
	ddb \
	defaultroute \
	devfs \
	dmesg \
	dumpon \
	fsck \
	growfs \
	growfs_fstab \
	hostid \
	hostid_save \
	hostname \
	iovctl \
	ip6addrctl \
	ipsec \
	kld \
	kldxref \
	ldconfig \
	linux \
	local \
	localpkg \
	mixer \
	motd \
	mountcritlocal \
	mountcritremote \
	mountlate \
	mdconfig \
	mdconfig2 \
	msgs \
	netif \
	netoptions \
	netwait \
	noshutdown \
	os-release \
	pwcheck \
	quota \
	random \
	rarpd \
	rctl \
	root \
	routing \
	rpcbind \
	rtadvd \
	rtsold \
	savecore \
	securelevel \
	serial \
	static_arp \
	static_ndp \
	stf \
	swap \
	swaplate \
	sysctl \
	sysctl_lastload \
	sysvipc \
	tmp \
	ugidfw \
	var \
	var_run \
	watchdogd

# Groups for files which don't go in -rc, or which depend on src.conf knobs.

.if ${MK_ACCT} != "no" || ${MK_UTMPX} != "no"
CONFGROUPS+=			ACCT
ACCTPACKAGE=			acct
.if ${MK_ACCT} != "no"
ACCT=		accounting
.endif
.if ${MK_UTMPX} != "no"
ACCT+=		utx
.endif
.endif

CONFGROUPS.${MK_ACPI}+=		ACPI
ACPIPACKAGE=			acpi
ACPI=		power_profile

CONFGROUPS.${MK_APM}+=		APM
APMPACKAGE=			apm
APM=		apm
.if ${MACHINE} == "i386"
APM+=		apmd
.endif

CONFGROUPS.${MK_AUDIT}+=	AUDIT
AUDITPACKAGE=			audit
AUDIT=		auditd \
		auditdistd

CONFGROUPS.${MK_AUTOFS}+=	AUTOFS
AUTOFSPACKAGE=			autofs
AUTOFS=		automount \
		automountd \
		autounmountd

CONFGROUPS.${MK_BLOCKLIST}+=	BLOCKLIST
BLOCKLISTPACKAGE=		blocklist
BLOCKLIST=	blacklistd \
		blocklistd

CONFGROUPS.${MK_BLUETOOTH}+=	BLUETOOTH
BLUETOOTHPACKAGE=		bluetooth
BLUETOOTH=	bluetooth \
		bthidd \
		hcsecd \
		rfcomm_pppd_server \
		sdpd \
		ubthidhci

CONFGROUPS.${MK_BOOTPARAMD}+=	BOOTPARAMD
BOOTPARAMD=	bootparams

CONFGROUPS.${MK_BSNMP}+=	BSNMP
BSNMPPACKAGE=			bsnmp
BSNMP=		bsnmpd

CONFGROUPS.${MK_CCD}+=		CCD
CCDPACKAGE=			ccdconfig
CCD=		ccd

CONFGROUPS+=			DEVD
DEVDPACKAGE=			devd
DEVD=		devd

CONFGROUPS+=			DEVMATCH
DEVMATCHPACKAGE=		devmatch
DEVMATCH=	devmatch

CONFGROUPS+=			DHCLIENT
DHCLIENTPACKAGE=		dhclient
DHCLIENT=	dhclient

CONFGROUPS+=			CRON
CRONPACKAGE=			cron
CRON=		cron

CONFGROUPS+=			CTL
CTLPACKAGE=			ctl
CTL=		ctld

CONFGROUPS+=			GEOM
GEOMPACKAGE=			geom
GEOM=		geli \
		geli2 \
		gptboot

CONFGROUPS+=			GGATED
GGATEDPACKAGE=			ggate
GGATED=		ggated

CONFGROUPS.${MK_KERBEROS_SUPPORT}+=GSSD
GSSDPACKAGE=			gssd
GSSD=		gssd

CONFGROUPS.${MK_HAST}+=		HAST
HASTPACKAGE=			hast
HAST=		hastd

CONFGROUPS.${MK_INETD}+=	INETD
INETDPACKAGE=			inetd
INETD=		inetd

CONFGROUPS.${MK_IPFILTER}+=	IPF
IPFPACKAGE=			ipf
IPF=		ipfilter \
		ipfs \
		ipmon \
		ipnat \
		ippool

CONFGROUPS.${MK_IPFW}+=		IPFW
IPFWPACKAGE=			ipfw
IPFW=		ipfw \
		dnctl
.if ${MK_NETGRAPH} != "no"
IPFW+=		ipfw_netflow
.endif

CONFGROUPS.${MK_ISCSI}+=	ISCSI
ISCSIPACKAGE=			iscsi
ISCSI=		iscsictl \
		iscsid

# natd is only built when ipfw is built
CONFGROUPS.${MK_IPFW}+=		NATD
NATDPACKAGE=			natd
NATD=		natd

CONFGROUPS.${MK_JAIL}+=		JAIL
JAILPACKAGE=			jail
JAIL=		jail

CONFGROUPS.${MK_LPR}+=		LP
LPPACKAGE=			lp
LP=		lpd

CONFGROUPS+=			NEWSYSLOG
NEWSYSLOGPACKAGE=		newsyslog
NEWSYSLOG=	newsyslog

CONFGROUPS+=			NFS
NFSPACKAGE=			nfs
NFS=		lockd \
		mountd \
		nfscbd \
		nfsclient \
		nfsd \
		nfsuserd \
		statd

CONFGROUPS.${MK_NIS}+=		NIS
NISPACKAGE=			yp
NIS=		ypbind \
		ypldap \
		yppasswdd \
		ypserv \
		ypset \
		ypupdated \
		ypxfrd \
		nisdomain

CONFGROUPS.${MK_NS_CACHING}+=	NSCD
NSCD=		nscd

CONFGROUPS.${MK_NTP}+=		NTP
NTPPACKAGE=			ntp
NTP=		ntpd \
		ntpdate

CONFGROUPS.${MK_NUAGEINIT}+=	NUAGEINIT
NUAGEINITPACKAGE=		nuageinit
NUAGEINIT=	nuageinit \
		nuageinit_post_net \
		nuageinit_user_data_script

CONFGROUPS.${MK_OFED_EXTRA}+=	OPENSM
OPENSM=	opensm

CONFGROUPS.${MK_PF}+=		PF
PFPACKAGE=			pf
PF=		pf \
		pflog \
		pfsync \
		ftp-proxy

CONFGROUPS+=			POWERD
POWERDPACKAGE=			powerd
POWERD=		powerd

CONFGROUPS.${MK_PPP}+=		PPP
PPPPACKAGE=			ppp
PPP=		ppp

CONFGROUPS+=			PPPOED
PPPOEDPACKAGE=			ppp
PPPOED=		pppoed

CONFGROUPS+=			SYSLOGD
SYSLOGDPACKAGE=			syslogd
SYSLOGD=	syslogd

CONFGROUPS+=			RCMDS
RCMDSPACKAGE=			rcmds
RCMDS=		rwho

CONFGROUPS+=			RESOLVCONF
RESOLVCONFPACKAGE=		resolvconf
RESOLVCONF=	resolv

CONFGROUPS.${MK_SENDMAIL}+=	SENDMAIL
SENDMAILPACKAGE=		sendmail
SENDMAIL=	sendmail

CONFGROUPS.${MK_OPENSSH}+=	SSH
SSHPACKAGE=			ssh
SSH=		sshd

CONFGROUPS.${MK_UNBOUND}+=	UNBOUND
UNBOUNDPACKAGE=			local-unbound
UNBOUND=	local_unbound

CONFGROUPS.${MK_VI}+=		VI
VIPACKAGE=			vi
VI=		virecover

CONFGROUPS.${MK_CUSE}+=		VOSS
VOSSPACKAGE=			sound
VOSS=		virtual_oss

CONFGROUPS.${MK_WIRELESS}+=	HOSTAPD
HOSTAPDPACKAGE=			hostapd
HOSTAPD=	hostapd

CONFGROUPS.${MK_WIRELESS}+=	WPA
WPAPACKAGE=			wpa
WPA=		wpa_supplicant

CONFGROUPS.${MK_ZFS}+=		ZFS
ZFSPACKAGE=			zfs
ZFS=		zfs \
		zfsbe \
		zfsd \
		zfskeys \
		zpool \
		zpoolreguid \
		zpoolupgrade \
		zvol

CONFGROUPS.${MK_LEGACY_CONSOLE}+=SYSCONS
SYSCONSPACKAGE=			console-tools
SYSCONS=	moused \
		msconvd \
		syscons

.if ${MK_KERBEROS} != "no"
.if ${MK_MITKRB5} == "no"

# Heimdal rc scripts
CONFGROUPS+=	HEIMDAL
HEIMDAL=	ipropd_master \
		ipropd_slave \
		kadmind \
		kdc \
		kfd \
		kpasswdd
HEIMDALPACKAGE=	kerberos

DIRS+=		VAR_HEMIDAL
VAR_HEMIDAL=	/var/heimdal
VAR_HEMIDAL_MODE=	700

.else	# ${MK_MITKRB5} != "no"

# MIT KRB5 rc scripts
CONFGROUPS+=	KRB5
KRB5=		kadmind \
		kdc
KRB5PACKAGE=	kerberos-kdc

.endif	# ${MK_MITKRB5}
.endif	# ${MK_KERBEROS}

.if ${MK_OPENSSL} != "no" && ${MK_OPENSSL_KTLS} != "no"
CONFGROUPS+=			KTLS
KTLS=	tlsclntd \
	tlsservd
.endif

.if ${MK_INET6} != "no" || ${MK_ROUTED} != "no"
CONFGROUPS+=	RIP
RIPPACKAGE=	rip

.if ${MK_INET6} != "no"
RIP+=		route6d
.endif
.if ${MK_ROUTED} != "no"
RIP+=		routed
.endif
.endif

.for fg in ${CONFGROUPS} ${CONFGROUPS.yes}
${fg}MODE?=	${BINMODE}
${fg}PACKAGE?=	rc
.endfor

.include <bsd.prog.mk>
