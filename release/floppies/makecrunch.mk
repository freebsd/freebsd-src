

SYSINSTALL=../../../../release/sysinstall/sysinstall
NOCRYPT?=yes

all: ${SYSINSTALL} crunch

${SYSINSTALL}:
	@echo "you need to make sysinstall first"
	false

crunch:
	-crunchgen ${.CURDIR}/crunch.conf
	${MAKE} -f crunch.mk all NOCRYP=${NOCRYPT} \
	    "CFLAGS=${CFLAGS} -DCRUNCHED_BINARY" 

clean:
	rm  -f *.o *.stub *.lo *_stub.c *.mk \
		crunch.cache \
		crunch.mk \
		crunch.c \
		crunch \
		.tmp_*

install: 
	@echo " No idea what to do to install yet"

.include <bsd.prog.mk>
