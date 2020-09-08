# $FreeBSD$
#
# Handle options (KERN_OPTS) for kernel module options.  This can be included earlier in a kmod Makefile
# to allow KERN_OPTS to control SRCS, etc.

.if !target(__<kmod.opts.mk>__)
__<kmod.opts.mk>__:

.include <bsd.init.mk>
# Grab all the options for a kernel build. For backwards compat, we need to
# do this after bsd.own.mk.
.include "kern.opts.mk"
.include <bsd.compiler.mk>
.include "config.mk"

.endif #  !target(__<kmod.opts.mk>__)
