#
# $Id: Makefile,v 1.1 1996/10/03 23:00:09 peter Exp $
#

GPPDIR=	${.CURDIR}/../../../contrib/libg++

.PATH:	${GPPDIR}/libg++/src ${GPPDIR}/libg++/genclass

all:		genclass

CLEANFILES+=	genclass

PROTODIR=	/usr/share/libg++
VERSION!=	sed -n -e '/^VERSION/s/VERSION *= *\(.*\)/\1/p' \
			< ${GPPDIR}/libg++/Makefile.in

#genclass prototypes
GEN=	AVLMap.ccP AVLMap.hP AVLSet.ccP AVLSet.hP AVec.ccP AVec.hP \
	BSTSet.ccP BSTSet.hP Bag.ccP Bag.hP CHBag.ccP CHBag.hP \
	CHMap.ccP CHMap.hP CHNode.ccP CHNode.hP CHSet.ccP CHSet.hP \
	DLDeque.ccP DLDeque.hP DLList.ccP DLList.hP Deque.ccP Deque.hP \
	FPQueue.ccP FPQueue.hP FPStack.ccP FPStack.hP FPlex.ccP FPlex.hP \
	List.ccP List.hP MPlex.ccP MPlex.hP Map.ccP Map.hP \
	OSLBag.ccP OSLBag.hP OSLSet.ccP OSLSet.hP OXPBag.ccP OXPBag.hP \
	OXPSet.ccP OXPSet.hP PHPQ.ccP PHPQ.hP PQ.ccP PQ.hP \
	PSList.hP PVec.hP Plex.ccP Plex.hP Queue.ccP Queue.hP \
	RAVLMap.ccP RAVLMap.hP RPlex.ccP RPlex.hP SLBag.ccP SLBag.hP \
	SLList.ccP SLList.hP SLQueue.ccP SLQueue.hP SLSet.ccP SLSet.hP \
	SLStack.ccP SLStack.hP Set.ccP Set.hP SkipBag.ccP SkipBag.hP \
	SkipMap.ccP SkipMap.hP SkipSet.ccP SkipSet.hP SplayBag.ccP SplayBag.hP \
	SplayMap.ccP SplayMap.hP SplayNode.ccP SplayNode.hP \
	SplayPQ.ccP SplayPQ.hP SplaySet.ccP SplaySet.hP Stack.ccP Stack.hP \
	VHBag.ccP VHBag.hP VHMap.ccP VHMap.hP VHSet.ccP VHSet.hP \
	VOHSet.ccP VOHSet.hP VQueue.ccP VQueue.hP VStack.ccP VStack.hP \
	Vec.ccP Vec.hP XPBag.ccP XPBag.hP XPDeque.ccP XPDeque.hP \
	XPPQ.ccP XPPQ.hP XPQueue.ccP XPQueue.hP XPSet.ccP XPSet.hP \
	XPStack.ccP XPStack.hP XPlex.ccP XPlex.hP \
	defs.hP intSList.hP intVec.hP


genclass:	genclass.sh
	sed -e 's@^PROTODIR=.*$$@PROTODIR=$$\{PROTODIR-${PROTODIR}\}@' \
	    -e 's/<VERSION>/${VERSION}/' < ${.ALLSRC} > genclass

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
		${GEN:S;^;${GPPDIR}/libg++/src/gen/;} \
		${DESTDIR}${PROTODIR}
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} genclass \
		${DESTDIR}${BINDIR}

.include <bsd.prog.mk>
