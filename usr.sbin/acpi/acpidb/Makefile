# $FreeBSD$

PROG=	acpidb
SRCS=	acpidb.c

# components/debugger
SRCS+=	dbcmds.c dbconvert.c dbdisply.c dbexec.c dbfileio.c	\
	dbhistry.c dbinput.c dbmethod.c dbnames.c dbstats.c	\
	dbutils.c dbxface.c

# components/disassembler
SRCS+=	dmbuffer.c dmdeferred.c dmnames.c dmobject.c dmopcode.c	\
	dmresrc.c dmresrcl.c dmresrcl2.c dmresrcs.c dmutils.c	\
	dmwalk.c

# components/dispatcher
SRCS+=	dsargs.c dscontrol.c dsfield.c dsinit.c dsmethod.c	\
	dsmthdat.c dsobject.c dsopcode.c dsutils.c dswexec.c	\
	dswload.c dswload2.c dswscope.c dswstate.c

# components/events
SRCS+=	evevent.c evglock.c evgpe.c evgpeblk.c evgpeinit.c	\
	evgpeutil.c evhandler.c evmisc.c evregion.c evrgnini.c	\
	evsci.c evxface.c evxfevnt.c evxfregn.c

# components/executer
SRCS+=	exconfig.c exconvrt.c excreate.c exdebug.c exdump.c	\
	exfield.c exfldio.c exmisc.c exmutex.c exnames.c	\
	exoparg1.c exoparg2.c exoparg3.c exoparg6.c exprep.c	\
	exregion.c exresnte.c exresolv.c exresop.c exstore.c	\
	exstoren.c exstorob.c exsystem.c exutils.c

# components/hardware
SRCS+=	hwacpi.c hwesleep.c hwgpe.c hwpci.c hwregs.c hwsleep.c	\
	hwvalid.c hwxface.c hwxfsleep.c

# components/namespace
SRCS+=	nsaccess.c nsalloc.c nsarguments.c nsconvert.c nsdump.c	\
	nseval.c nsinit.c nsload.c nsnames.c nsobject.c		\
	nsparse.c nspredef.c nsprepkg.c nsrepair.c nsrepair2.c	\
	nssearch.c nsutils.c nswalk.c nsxfeval.c nsxfname.c	\
	nsxfobj.c

# components/parser
SRCS+=	psargs.c psloop.c psobject.c psopcode.c psopinfo.c	\
	psparse.c psscope.c pstree.c psutils.c pswalk.c		\
	psxface.c

# components/os_specific/service_layers
SRCS+=	osunixxf.c

# components/resources
SRCS+=	rsaddr.c rscalc.c rscreate.c rsdump.c rsdumpinfo.c	\
	rsinfo.c rsio.c rsirq.c rslist.c rsmemory.c rsmisc.c	\
	rsserial.c rsutils.c rsxface.c

# components/tables
SRCS+=	tbfadt.c tbfind.c tbinstal.c tbutils.c tbxface.c	\
	tbxfload.c

# components/utilities
SRCS+=	utaddress.c utalloc.c utcache.c utcopy.c utdebug.c	\
	utdecode.c utdelete.c uteval.c utexcep.c utglobal.c	\
	utids.c	utinit.c utlock.c utmath.c utmisc.c utmutex.c	\
	utobject.c utosi.c utownerid.c utpredef.c utresrc.c	\
	utstate.c utstring.c uttrack.c utxface.c utxferror.c	\
	utxfinit.c

MAN=	acpidb.8
WARNS?=	2

CFLAGS+= -DACPI_EXEC_APP -fno-strict-aliasing
DPADD=	${LIBPTHREAD}
LDADD=	-lpthread

.include <bsd.prog.mk>
