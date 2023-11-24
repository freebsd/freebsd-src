
SUBDIR = alpm amdpm amdsmb ichiic ichsmb intpm ismt nfsmb viapm lpbb pcf

.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "amd64"
SUBDIR += imcsmb
.endif
.if ${MACHINE_CPUARCH} == "riscv"
SUBDIR += iicoc
.endif

.include <bsd.subdir.mk>
