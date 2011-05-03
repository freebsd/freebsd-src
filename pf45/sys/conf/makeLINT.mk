# $FreeBSD$

all:
	@echo "make LINT only"

clean:
	rm -f LINT
.if ${TARGET} == "amd64" || ${TARGET} == "i386"
	rm -f LINT-VIMAGE
.endif

NOTES=	../../conf/NOTES NOTES
LINT: ${NOTES} ../../conf/makeLINT.sed
	cat ${NOTES} | sed -E -n -f ../../conf/makeLINT.sed > ${.TARGET}
.if ${TARGET} == "amd64" || ${TARGET} == "i386"
	echo "include ${.TARGET}"	>  ${.TARGET}-VIMAGE
	echo "ident ${.TARGET}-VIMAGE"	>> ${.TARGET}-VIMAGE
	echo "options VIMAGE"		>> ${.TARGET}-VIMAGE
	echo "include ${.TARGET}"	>  ${.TARGET}-NOINET
	echo "ident ${.TARGET}-NOINET"	>> ${.TARGET}-NOINET
	echo 'makeoptions MKMODULESENV+="WITHOUT_INET_SUPPORT="'  >> ${.TARGET}-NOINET
	echo "nooptions INET"		>> ${.TARGET}-NOINET
	echo "nodevice gre"		>> ${.TARGET}-NOINET
	echo "include ${.TARGET}"	>  ${.TARGET}-NOINET6
	echo "ident ${.TARGET}-NOINET6"	>> ${.TARGET}-NOINET6
	echo "nooptions INET6"		>> ${.TARGET}-NOINET6
	echo "include ${.TARGET}"	>  ${.TARGET}-NOIP
	echo "ident ${.TARGET}-NOIP"	>> ${.TARGET}-NOIP
	echo 'makeoptions MKMODULESENV+="WITHOUT_INET_SUPPORT="'  >> ${.TARGET}-NOIP
	echo 'makeoptions MKMODULESENV+="WITHOUT_INET6_SUPPORT="' >> ${.TARGET}-NOIP
	echo "nooptions INET"		>> ${.TARGET}-NOIP
	echo "nooptions INET6"		>> ${.TARGET}-NOIP
	echo "nodevice age"		>> ${.TARGET}-NOIP
	echo "nodevice alc"		>> ${.TARGET}-NOIP
	echo "nodevice ale"		>> ${.TARGET}-NOIP
	echo "nodevice bxe"		>> ${.TARGET}-NOIP
	echo "nodevice em"		>> ${.TARGET}-NOIP
	echo "nodevice fxp"		>> ${.TARGET}-NOIP
	echo "nodevice igb"		>> ${.TARGET}-NOIP
	echo "nodevice jme"		>> ${.TARGET}-NOIP
	echo "nodevice msk"		>> ${.TARGET}-NOIP
	echo "nodevice mxge"		>> ${.TARGET}-NOIP
	echo "nodevice sge"		>> ${.TARGET}-NOIP
	echo "nodevice sk"		>> ${.TARGET}-NOIP
	echo "nodevice txp"		>> ${.TARGET}-NOIP
	echo "nodevice vxge"		>> ${.TARGET}-NOIP
.endif
.if ${TARGET} == "powerpc" || ${TARGET} == "mips"
	echo "machine	${TARGET} ${TARGET_ARCH}" >> ${.TARGET}
.endif
