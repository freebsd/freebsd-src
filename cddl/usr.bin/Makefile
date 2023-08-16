
.include <src.opts.mk>

SUBDIR.${MK_DTRACE}+= \
	ctfconvert \
	ctfdump \
	ctfmerge

SUBDIR.${MK_TESTS}+= tests

SUBDIR.${MK_ZFS}+= \
	zinject \
	ztest \
	zstream

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
