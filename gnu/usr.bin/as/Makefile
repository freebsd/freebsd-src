#	from: @(#)Makefile	6.1 (Berkeley) 3/3/91
#	$Id: Makefile,v 1.2 1993/11/03 00:50:49 paul Exp $

.include "config/Makefile.$(MACHINE)"

.if !defined (gas_hosttype)
gas_hosttype=$(MACHINE)
.endif
.if !defined (gas_target)
gas_target=$(MACHINE)
.endif
.if !defined (gas_objformat)
gas_objformat=aout
.endif

.if exists(${.CURDIR}/obj)
ADDINCLUDE=-I${.CURDIR}/obj
.endif

PROG=		as
SRCS+=		app.c as.c atof-generic.c bignum-copy.c \
		cond.c expr.c flo-const.c flo-copy.c flonum-mult.c \
		frags.c hash.c hex-value.c input-file.c input-scrub.c \
		listing.c messages.c obstack.c output-file.c read.c subsegs.c \
		symbols.c version.c write.c xmalloc.c xrealloc.c \
		obj-$(gas_objformat).c
CFLAGS+=	-I$(.CURDIR) ${ADDINCLUDE} -I$(.CURDIR)/config \
		-DPIC -DOLD_GAS -DSIGTY=void -Derror=as_fatal \
		-DSUB_SEGMENT_ALIGN=4
#LDADD+=		-lgnumalloc
#DPADD+=		/usr/lib/libgnumalloc.a

CONF_HEADERS=	targ-cpu.h obj-format.h host.h targ-env.h

.PATH: $(.CURDIR)/config

beforedepend ${PROG}: ${CONF_HEADERS}

targ-cpu.h: Makefile config/Makefile.$(MACHINE)
	@cmp -s $(.CURDIR)/config/tc-$(gas_target).h targ-cpu.h || \
	    ( echo "updating ${.TARGET}..." ; /bin/rm -f targ-cpu.h ; \
	    cp $(.CURDIR)/config/tc-$(gas_target).h targ-cpu.h )

obj-format.h: Makefile config/Makefile.$(MACHINE)
	@cmp -s $(.CURDIR)/config/obj-$(gas_objformat).h obj-format.h || \
	    ( echo "updating ${.TARGET}..." ; /bin/rm -f obj-format.h ; \
	    cp $(.CURDIR)/config/obj-$(gas_objformat).h obj-format.h )

.if exists ($(.CURDIR)/config/ho-$(gas_hosttype).h)
config_hostfile=	$(.CURDIR)/config/ho-$(gas_hosttype).h
.else
config_hostfile=	$(.CURDIR)/config/ho-generic.h
.endif

host.h: Makefile config/Makefile.$(MACHINE)
	@cmp -s $(config_hostfile) host.h || \
	    ( echo "updating ${.TARGET}..." ; /bin/rm -f host.h ; \
	    cp $(config_hostfile) host.h )

.if exists ($(.CURDIR)/config/te-$(MACHINE).h)
config_targenvfile=	$(.CURDIR)/config/te-$(MACHINE).h
.else
config_targenvfile=	$(.CURDIR)/config/te-generic.h
.endif

targ-env.h: Makefile config/Makefile.$(MACHINE)
	@cmp -s $(config_targenvfile) targ-env.h || \
	    ( echo "updating ${.TARGET}..." ; /bin/rm -f targ-env.h ; \
	    cp $(config_targenvfile) targ-env.h )

CLEANFILES+= ${CONF_HEADERS}

.include <bsd.prog.mk>
