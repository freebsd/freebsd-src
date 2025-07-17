SUBDIR = alpm amdpm amdsmb ichiic ichsmb intpm ismt nfsmb viapm lpbb pcf

.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "amd64"
SUBDIR += imcsmb
.endif
.if ${MACHINE_CPUARCH} == "riscv"
SUBDIR += iicoc
.endif
.if ${MACHINE_CPUARCH} == "aarch64"
SUBDIR += geni_iic
.endif

.include <bsd.subdir.mk>
