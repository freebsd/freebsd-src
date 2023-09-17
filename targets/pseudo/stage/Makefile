
# this makefile gets hooked into everyone's dependencies so that
# distrib-dirs can be done in STAGE_OBJTOP
all:

.if ${.MAKE.OS} == "FreeBSD"
# mtree makes a lot of noise if we are not root, 
# we don't need to see it.
stage-distrib-dirs: .META ${META_DEPS}
	mkdir -p ${STAGE_OBJTOP}
	INSTALL="sh ${SRCTOP}/tools/install.sh" ${.MAKE} -C ${SRCTOP}/etc \
	    distrib-dirs -DNO_FSCHG -DWITH_TESTS DESTDIR=${STAGE_OBJTOP}

.else
std_dirs= \
	/bin \
	/etc \
	/sbin \
	/usr/bin \
	/usr/include/sys \
	/usr/lib \
	/usr/sbin \

stage-distrib-dirs: .META
	mkdir -p ${std_dirs:@d@${STAGE_OBJTOP}$d@}

.endif

.include <bsd.prog.mk>

.if ${.MAKE.LEVEL} > 0 && ${MK_STAGING} == "yes"
all: stage-distrib-dirs
.endif
