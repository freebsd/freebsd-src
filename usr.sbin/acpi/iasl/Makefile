# $FreeBSD$

PROG=	iasl
SRCS+=	aslcompilerparse.c aslcompilerlex.c aslanalyze.c aslcodegen.c \
	aslcompile.c aslerror.c aslfiles.c asllength.c \
	asllisting.c aslload.c asllookup.c aslmain.c \
	aslmap.c aslopcodes.c asloperands.c aslresource.c \
	aslrestype1.c aslrestype2.c asltree.c aslutils.c \
	asltransform.c aslfold.c aslstubs.c aslopt.c
SRCS+=	adisasm.c getopt.c osunixxf-xxx.c
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
YFLAGS+= -v -d -pAslCompiler
LFLAGS+= -i

.if $(YACC) == "bison"
YFLAGS+= -y
.else
CFLAGS+= -D_USE_BERKELEY_YACC
.endif

CLEANFILES= y.output y.tab.c y.tab.h aslcompiler.y.h \
	aslcompilerparse.c aslcompilerlex.c iasl

aslcompilerparse.c: aslcompiler.y
	${YACC} ${YFLAGS} ${ACPICA_DIR}/compiler/aslcompiler.y
	cp y.tab.c aslcompilerparse.c
	cp y.tab.h aslcompiler.y.h

aslcompilerlex.c: aslcompiler.l
	${LEX} ${LFLAGS} -PAslCompiler -oaslcompilerlex.c \
		${ACPICA_DIR}/compiler/aslcompiler.l

# The following band-aid is also present in ../acpidb/Makefile
CLEANFILES+= osunixxf-xxx.c
osunixxf-xxx.c: osunixxf.c
	cat ${.ALLSRC} | sed -e 's/UINT32 *length/ACPI_SIZE length/g' -e 's/UINT32 *size/ACPI_SIZE size/g' -e 's/(UINT32) where/(uintptr_t)where/g' > ${.TARGET}

.include <bsd.prog.mk>
