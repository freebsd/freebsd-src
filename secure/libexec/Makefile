
.include <src.opts.mk>

SUBDIR=
.if ${MK_OPENSSH} != "no"
SUBDIR+=sftp-server ssh-keysign ssh-pkcs11-helper ssh-sk-helper
.endif

SUBDIR.${MK_TESTS}+= tests

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
