# $NetBSD: objdir-writable.mk,v 1.7 2022/02/09 21:24:29 rillig Exp $

# test checking for writable objdir

TMPDIR?= /tmp
RO_OBJDIR?= ${TMPDIR}/roobj

.if make(do-objdir)
# this should succeed
.OBJDIR: ${RO_OBJDIR}

do-objdir:
.else
all: no-objdir ro-objdir explicit-objdir

# make it now
_!=	mkdir -p ${RO_OBJDIR}
_!=	chmod 555 ${RO_OBJDIR}

.END: rm-objdir
rm-objdir:
	@rmdir ${RO_OBJDIR}

no-objdir:
	@MAKEOBJDIR=${RO_OBJDIR} ${.MAKE} -r -f /dev/null -C ${TMPDIR} -V .OBJDIR

ro-objdir:
	@MAKEOBJDIR=${RO_OBJDIR} ${.MAKE} -r -f /dev/null -C ${TMPDIR} -V .OBJDIR MAKE_OBJDIR_CHECK_WRITABLE=no

explicit-objdir:
	@MAKEOBJDIR=${TMPDIR} ${.MAKE} -r -f ${MAKEFILE:tA} -C ${TMPDIR} do-objdir -V .OBJDIR
.endif
