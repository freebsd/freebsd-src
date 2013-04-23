# $FreeBSD$

PROG=	iasl
SRCS=	adfile.c adisasm.c adwalk.c

# common
SRCS+=	ahpredef.c dmextern.c dmrestag.c dmtable.c dmtbdump.c	\
	dmtbinfo.c getopt.c

# compiler
SRCS+=	aslanalyze.c aslbtypes.c aslcodegen.c aslcompile.c	\
	aslcompiler.y.h aslcompilerlex.c aslcompilerparse.c	\
	aslerror.c aslfileio.c aslfiles.c aslfold.c aslhex.c	\
	asllength.c asllisting.c asllistsup.c aslload.c		\
	asllookup.c aslmain.c aslmap.c aslmethod.c aslnamesp.c	\
	asloffset.c aslopcodes.c asloperands.c aslopt.c		\
	aslpredef.c aslprepkg.c aslresource.c aslrestype1.c	\
	aslrestype1i.c aslrestype2.c aslrestype2d.c		\
	aslrestype2e.c aslrestype2q.c aslrestype2s.c		\
	aslrestype2w.c aslstartup.c aslstubs.c asltransform.c	\
	asltree.c aslutils.c asluuid.c aslwalks.c aslxref.c	\
	dtcompile.c dtexpress.c dtfield.c dtio.c dtparser.y.h	\
	dtparserlex.c dtparserparse.c dtsubtable.c dttable.c	\
	dttemplate.c dtutils.c prexpress.c prmacros.c		\
	prparser.y.h prparserlex.c prparserparse.c prscan.c	\
	prutils.c

# components/debugger
SRCS+=	dbfileio.c

# components/disassembler
SRCS+=	dmbuffer.c dmdeferred.c dmnames.c dmopcode.c dmresrc.c	\
	dmresrcl.c dmresrcl2.c dmresrcs.c dmutils.c dmwalk.c

# components/dispatcher
SRCS+=	dsargs.c dscontrol.c dsfield.c dsobject.c dsopcode.c	\
	dsutils.c dswexec.c dswload.c dswload2.c dswscope.c	\
	dswstate.c

# components/executer
SRCS+=	exconvrt.c excreate.c exdump.c exmisc.c exmutex.c	\
	exnames.c exoparg1.c exoparg2.c exoparg3.c exoparg6.c	\
	exprep.c exresnte.c exresolv.c exresop.c exstore.c	\
	exstoren.c exstorob.c exsystem.c exutils.c

# components/parser
SRCS+=	psargs.c psloop.c psobject.c psopcode.c psopinfo.c	\
	psparse.c psscope.c pstree.c psutils.c pswalk.c

# components/namespace
SRCS+=	nsaccess.c nsalloc.c nsdump.c nsnames.c nsobject.c	\
	nsparse.c nssearch.c nsutils.c nswalk.c

# components/tables
SRCS+=	tbfadt.c tbinstal.c tbutils.c tbxface.c

# components/utilities
SRCS+=	utaddress.c utalloc.c utcache.c utcopy.c utdebug.c	\
	utdecode.c utdelete.c utexcep.c utglobal.c utinit.c	\
	utlock.c utmath.c utmisc.c utmutex.c utobject.c		\
	utownerid.c utpredef.c utresrc.c utstate.c utstring.c	\
	utxface.c utxferror.c

# os_specific/service_layers
SRCS+=	osunixxf.c

WARNS?=	2

MAN=	iasl.8

CFLAGS+= -DACPI_ASL_COMPILER -I.
LFLAGS= -i -s
YFLAGS= -d

CLEANFILES= aslcompiler.y.h aslcompilerlex.c aslcompilerparse.c	\
	aslcompilerparse.h dtparser.y.h dtparserlex.c		\
	dtparserparse.c dtparserparse.h prparser.y.h		\
	prparserlex.c prparserparse.c prparserparse.h

aslcompilerlex.c: aslcompiler.l aslsupport.l
	${LEX} ${LFLAGS} -PAslCompiler -o${.TARGET} \
	    ${ACPICA_DIR}/compiler/aslcompiler.l

.ORDER: aslcompilerparse.c aslcompilerparse.h
aslcompilerparse.c aslcompilerparse.h: aslcompiler.y
	${YACC} ${YFLAGS} -pAslCompiler -oaslcompilerparse.c ${.ALLSRC}

aslcompiler.y.h: aslcompilerparse.h
	ln -f ${.ALLSRC} ${.TARGET}

dtparserlex.c: dtparser.l
	${LEX} ${LFLAGS} -PDtParser -o${.TARGET} ${.ALLSRC}

.ORDER: dtparserparse.c dtparserparse.h
dtparserparse.c dtparserparse.h: dtparser.y
	${YACC} ${YFLAGS} -pDtParser -odtparserparse.c ${.ALLSRC}

dtparser.y.h: dtparserparse.h
	ln -f ${.ALLSRC} ${.TARGET}

prparserlex.c: prparser.l
	${LEX} ${LFLAGS} -PPrParser -o${.TARGET} ${.ALLSRC}

.ORDER: prparserparse.c prparserparse.h
prparserparse.c prparserparse.h: prparser.y
	${YACC} ${YFLAGS} -pPrParser -oprparserparse.c ${.ALLSRC}

prparser.y.h: prparserparse.h
	ln -f ${.ALLSRC} ${.TARGET}

.include <bsd.prog.mk>
