#	from: @(#)Makefile	7.3 (Berkeley) 6/9/91
# $FreeBSD: src/sys/i386/Makefile,v 1.5.2.1 1999/08/29 16:04:30 peter Exp $

# Makefile for i386 tags file

all:
	@echo "make tags or links only"

TI386=	${.OBJDIR}/../i386/tags
SI386=	${.CURDIR}/../i386/i386/*.[ch] ${.CURDIR}/../i386/include/*.h \
	${.CURDIR}/../i386/isa/*.[ch]
AI386=	${.CURDIR}/../i386/i386/*.s

# Directories in which to place i386 tags links
DI386=	eisa isa include

tags:
	-ctags -dtf ${TI386} ${COMM} ${SI386}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${AI386} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3 \1 /^\2(\3\4$$/;" \
		>> ${TI386}
	sort -o ${TI386} ${TI386}

#XXX This doesn't work because ${.OBJDIR}/$i doesn't exist
#
#links:
#	-for i in ${DI386}; do \
#	    (cd ${.CURDIR}/$$i && rm -f ${.OBJDIR}/tags; \
#	    ln -s ${.OBJDIR}/../tags ${.OBJDIR}/tags) \
#	done
