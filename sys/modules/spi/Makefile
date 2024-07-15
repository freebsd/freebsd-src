SUBDIR = \
	../spigen \
	at45d \
	${_atopcase} \
	mx25l \
	spibus \
	
.if ${MACHINE_CPUARCH} == "i386" || ${MACHINE_CPUARCH} == "amd64"
_atopcase=atopcase
.endif

.include <bsd.subdir.mk>
