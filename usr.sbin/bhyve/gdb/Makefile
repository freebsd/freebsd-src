PACKAGE=	bhyve
FILESDIR=	${SHAREDIR}/bhyve/gdb

FILES+=		target.xml

.if ${MACHINE_ARCH} == "amd64"
XMLARCH=	i386:x86-64
FILES+=		amd64.xml
.endif

.if !make(install*)
target.xml: .PHONY
	@echo "<?xml version=\"1.0\"?>" > ${.TARGET}
	@echo "<!DOCTYPE target SYSTEM \"gdb-target.dtd\">" >> ${.TARGET}
	@echo "<target>" >> ${.TARGET}
	@echo "  <architecture>${XMLARCH}</architecture>" >> ${.TARGET}
.for file in ${FILES:Ntarget.xml}
	@echo "  <xi:include href=\"${file}\"/>" >> ${.TARGET}
.endfor
	@echo "</target>" >> ${.TARGET}
.endif

.include <bsd.prog.mk>
