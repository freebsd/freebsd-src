# $FreeBSD$

PROG=	iasl
SRCS=	adfile.c adisasm.c adwalk.c
SRCS+=	osunixxf.c

# common
SRCS+=	dmextern.c dmrestag.c dmtable.c dmtbdump.c dmtbinfo.c	\
	getopt.c

# compiler
SRCS+=	aslanalyze.c aslcodegen.c aslcompile.c aslcompiler.y.h	\
	aslcompilerlex.l aslcompilerparse.y aslerror.c		\
	aslfiles.c aslfold.c asllength.c asllisting.c		\
	aslload.c asllookup.c aslmain.c aslmap.c aslopcodes.c	\
	asloperands.c aslopt.c aslpredef.c aslresource.c	\
	aslrestype1.c aslrestype1i.c aslrestype2.c		\
	aslrestype2d.c aslrestype2e.c aslrestype2q.c		\
	aslrestype2w.c aslstartup.c aslstubs.c asltransform.c	\
	asltree.c aslutils.c

# debugger
SRCS+=	dbfileio.c

# disassembler
SRCS+=	dmbuffer.c dmnames.c dmobject.c dmopcode.c dmresrc.c	\
	dmresrcl.c dmresrcs.c dmutils.c dmwalk.c

# interpreter/dispatcher
SRCS+=	dsfield.c dsobject.c dsopcode.c dsutils.c dswexec.c	\
	dswload.c dswscope.c dswstate.c

# interpreter/executer
SRCS+=	exconvrt.c excreate.c exdump.c exmisc.c exmutex.c	\
	exnames.c exoparg1.c exoparg2.c exoparg3.c exoparg6.c	\
	exprep.c exregion.c exresnte.c exresolv.c exresop.c	\
	exstore.c exstoren.c exstorob.c exsystem.c exutils.c

# interpreter/parser
SRCS+=	psargs.c psloop.c psopcode.c psparse.c psscope.c	\
	pstree.c psutils.c pswalk.c

# namespace
SRCS+=	nsaccess.c nsalloc.c nsdump.c nsnames.c nsobject.c	\
	nsparse.c nssearch.c nsutils.c nswalk.c nsxfobj.c

# tables
SRCS+=	tbfadt.c tbinstal.c tbutils.c tbxface.c

# utilities
SRCS+=	utalloc.c utcache.c utcopy.c utdebug.c utdelete.c	\
	utglobal.c utinit.c utlock.c utmath.c utmisc.c		\
	utmutex.c utobject.c utresrc.c utstate.c utxface.c

WARNS?=	2

MAN=	iasl.8

CFLAGS+= -DACPI_ASL_COMPILER -I.
CFLAGS+= -D_USE_BERKELEY_YACC
LFLAGS= -i -PAslCompiler
YFLAGS= -d -pAslCompiler
DPADD=	${LIBPTHREAD}
LDADD=	-lpthread

CLEANFILES= aslcompiler.y.h aslcompilerlex.l aslcompilerparse.y

aslcompiler.y.h: aslcompilerparse.h
	cat ${.ALLSRC} > ${.TARGET}

aslcompilerlex.l: aslcompiler.l
	cat ${.ALLSRC} > ${.TARGET}

aslcompilerparse.y: aslcompiler.y
	cat ${.ALLSRC} > ${.TARGET}

.include <bsd.prog.mk>
