# $FreeBSD$

PROG=	iasl
SRCS=	aslcompiler.y.h aslcompilerlex.l aslcompilerparse.y \
	aslanalyze.c aslcodegen.c \
	aslcompile.c aslerror.c aslfiles.c asllength.c \
	asllisting.c aslload.c asllookup.c aslmain.c \
	aslmap.c aslopcodes.c asloperands.c aslresource.c \
	aslrestype1.c aslrestype2.c asltree.c aslutils.c \
	asltransform.c aslfold.c aslstubs.c aslopt.c
SRCS+=	adisasm.c getopt.c osunixxf.c
SRCS+=	dbfileio.c dmbuffer.c dmnames.c dmopcode.c dmobject.c \
        dmresrc.c dmresrcl.c dmresrcs.c dmutils.c dmwalk.c \
        dsopcode.c dsutils.c dswexec.c dswload.c \
	dswscope.c dswstate.c dsfield.c dsobject.c \
        exconvrt.c excreate.c exdump.c exmisc.c \
        exmutex.c exnames.c exoparg1.c exoparg2.c \
        exoparg3.c exoparg6.c exprep.c exregion.c \
        exresnte.c exresolv.c exresop.c exstore.c \
        exstoren.c exstorob.c exsystem.c exutils.c \
        nsaccess.c nsalloc.c nsdump.c nsnames.c nsobject.c \
	nsparse.c nssearch.c nsutils.c nswalk.c nsxfobj.c \
        psargs.c psopcode.c psparse.c psscope.c \
        pstree.c psutils.c pswalk.c \
        tbinstal.c tbutils.c \
	utalloc.c utcopy.c utdebug.c utdelete.c \
        utglobal.c utobject.c utmisc.c utmath.c

MAN=	iasl.8

CFLAGS+= -D_ACPI_ASL_COMPILER -I.

CFLAGS+= -D_USE_BERKELEY_YACC
LFLAGS=	-i -PAslCompiler
YFLAGS=	-d -pAslCompiler

CLEANFILES= aslcompiler.y.h aslcompilerlex.l aslcompilerparse.y

aslcompiler.y.h: aslcompilerparse.h
	cat ${.ALLSRC} > ${.TARGET}

aslcompilerlex.l: aslcompiler.l
	cat ${.ALLSRC} > ${.TARGET}

aslcompilerparse.y: aslcompiler.y
	cat ${.ALLSRC} > ${.TARGET}

.include <bsd.prog.mk>
