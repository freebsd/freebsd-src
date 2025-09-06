.include <bsd.init.mk>

LIB=	acpi

.if ${MACHINE} == "amd64"
CFLAGS+=	-DACPI_SINGLE_THREADED \
		-DACPI_USE_NATIVE_RSDP_POINTER \
		-DACPI_USE_NATIVE_MEMORY_MAPPING \
		-DACPI_REDUCED_HARDWARE \
		-DACPI_DEBUG_OUTPUT

.PATH: ${.CURDIR}/../libefi
.PATH: ${.CURDIR}/amd64/Osd
.PATH: ${.CURDIR}/../contrib/dev/acpica/os_specific/service_layers
.PATH: ${SYSDIR}/contrib/dev/acpica/os_specific/service_layers
.PATH: ${SYSDIR}/contrib/dev/acpica/components/tables
.PATH: ${SYSDIR}/contrib/dev/acpica/components/namespace
.PATH: ${SYSDIR}/contrib/dev/acpica/components/utilities
.PATH: ${SYSDIR}/contrib/dev/acpica/components/dispatcher
.PATH: ${SYSDIR}/contrib/dev/acpica/components/executer
.PATH: ${SYSDIR}/contrib/dev/acpica/components/hardware
.PATH: ${SYSDIR}/contrib/dev/acpica/components/events
.PATH: ${SYSDIR}/contrib/dev/acpica/components/parser

SRCS+=	libefi.c OsdMemory.c osunixxf.c utinit.c utglobal.c utresrc.c utlock.c \
	utownerid.c utcopy.c uthex.c utmath.c utstrsuppt.c utstrtoul64.c \
	utpredef.c uterror.c uteval.c utids.c utbuffer.c utmisc.c utalloc.c \
	utxferror.c utexcep.c utaddress.c utascii.c utcache.c utcksum.c \
	utdebug.c utdecode.c utdelete.c utmutex.c utobject.c utosi.c \
	utstring.c utxfinit.c utnonansi.c utstate.c tbdata.c tbxfload.c \
	tbfadt.c tbinstal.c tbprint.c tbutils.c tbxface.c tbxfroot.c \
	tbfind.c nsxfobj.c nsxfeval.c nsinit.c nsparse.c nsload.c \
	nsrepair2.c nsconvert.c nsrepair.c nsprepkg.c nsarguments.c nseval.c \
	nsxfname.c nsdump.c nswalk.c nssearch.c nsnames.c nsutils.c \
	nsaccess.c nsalloc.c nsobject.c nspredef.c dsinit.c dsfield.c \
	dspkginit.c dsopcode.c dsdebug.c dsmethod.c dscontrol.c dsobject.c \
	dsutils.c dswexec.c dswload2.c dswload.c dsmthdat.c dswstate.c \
	dsargs.c dswscope.c exregion.c exfldio.c exserial.c exstorob.c \
	exstoren.c exdebug.c exmutex.c exconcat.c exmisc.c exconfig.c \
	excreate.c exoparg6.c exoparg3.c exoparg2.c exoparg1.c exdump.c \
	exprep.c exresop.c exsystem.c exnames.c exresolv.c exstore.c \
	exfield.c exconvrt.c exresnte.c exutils.c extrace.c hwpci.c \
	hwregs.c hwxface.c hwvalid.c hwacpi.c evmisc.c evregion.c evrgnini.c \
	evxface.c evevent.c evxfevnt.c evhandler.c evxfregn.c psobject.c \
	psloop.c pstree.c pswalk.c psopinfo.c psscope.c psutils.c psargs.c \
	psparse.c psxface.c psopcode.c

CFLAGS+=	-I${SYSDIR}/contrib/dev/acpica/include
CFLAGS+=	-I${EFISRC}/include
CFLAGS+=	-I${EFISRC}/acpica/include
CFLAGS+=	-I${EFISRC}/include/${MACHINE_ARCH}
.endif # amd64

.include <bsd.lib.mk>
