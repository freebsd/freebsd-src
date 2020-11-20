# $NetBSD: objdir-writable.mk,v 1.4 2020/11/14 07:36:00 sjg Exp $

# test checking for writable objdir

RO_OBJDIR?= ${TMPDIR:U/tmp}/roobj

.if make(do-objdir)
# this should succeed
.OBJDIR: ${RO_OBJDIR}

do-objdir:
.else
all: no-objdir ro-objdir explicit-objdir

# make it now
x!= echo; mkdir -p ${RO_OBJDIR};  chmod 555 ${RO_OBJDIR}

.END: rm-objdir
rm-objdir:
	@rmdir ${RO_OBJDIR}

no-objdir:
	@MAKEOBJDIR=${RO_OBJDIR} ${.MAKE} -r -f /dev/null -C /tmp -V .OBJDIR

ro-objdir:
	@MAKEOBJDIR=${RO_OBJDIR} ${.MAKE} -r -f /dev/null -C /tmp -V .OBJDIR MAKE_OBJDIR_CHECK_WRITABLE=no

explicit-objdir:
	@MAKEOBJDIR=/tmp ${.MAKE} -r -f ${MAKEFILE:tA} -C /tmp do-objdir -V .OBJDIR
.endif

