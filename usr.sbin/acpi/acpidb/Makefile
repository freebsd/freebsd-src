# $FreeBSD$

PROG=	acpidb
SRCS+=	acpidb.c
SRCS+=	osunixxf.c
SRCS+=	dbcmds.c dbdisply.c dbexec.c dbfileio.c \
	dbhistry.c dbinput.c dbstats.c dbutils.c \
	dbxface.c dmbuffer.c dmnames.c dmobject.c \
	dmopcode.c dmresrc.c dmresrcl.c dmresrcs.c \
	dmutils.c dmwalk.c dsfield.c dsinit.c \
	dsmethod.c dsmthdat.c dsobject.c dsopcode.c \
	dsutils.c dswexec.c dswload.c dswscope.c dswstate.c \
	evevent.c evgpe.c evgpeblk.c evmisc.c \
	evregion.c evrgnini.c evsci.c evxface.c \
	evxfevnt.c evxfregn.c exconfig.c exconvrt.c \
	excreate.c exdump.c exfield.c exfldio.c \
	exmisc.c exmutex.c exnames.c exoparg1.c \
	exoparg2.c exoparg3.c exoparg6.c exprep.c \
	exregion.c exresnte.c exresolv.c exresop.c \
	exstore.c exstoren.c exstorob.c exsystem.c exutils.c \
	hwacpi.c hwgpe.c hwregs.c \
	nsaccess.c nsalloc.c nsdump.c nseval.c \
	nsinit.c nsload.c nsnames.c nsobject.c \
	nsparse.c nssearch.c nsutils.c nswalk.c \
	nsxfeval.c nsxfname.c nsxfobj.c \
	psargs.c psopcode.c psparse.c psscope.c \
	pstree.c psutils.c pswalk.c psxface.c \
	rsaddr.c rscalc.c rscreate.c rsdump.c \
	rsio.c rsirq.c rslist.c rsmemory.c \
	rsmisc.c rsutils.c rsxface.c \
	tbconvrt.c tbget.c tbgetall.c tbinstal.c \
	tbrsdt.c tbutils.c tbxface.c tbxfroot.c \
	utalloc.c utcopy.c utdebug.c utdelete.c \
	uteval.c utglobal.c utinit.c utmath.c \
	utmisc.c utobject.c utxface.c

MAN=	acpidb.8

CFLAGS+= -DACPI_APPLICATION -DACPI_DEBUG_OUTPUT -DACPI_DEBUGGER \
	-DACPI_DISASSEMBLER

.include <bsd.prog.mk>
