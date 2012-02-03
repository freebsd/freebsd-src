#	$FreeBSD$

FILES=	rt2561s.fw.uu rt2561.fw.uu rt2661.fw.uu rt2860.fw.uu

rt2561s.fw.uu: rt2661_ucode.h LICENSE
	(cat rt2661_ucode.h; \
	 echo 'int main(void) { \
	     write(1, rt2561s_ucode, sizeof(rt2561s_ucode)); return 0; \
	 }') | ${CC} -o build -x c -
	(sed 's/^/# /' LICENSE; ./build | uuencode rt2561s.fw) > ${.TARGET}

rt2561.fw.uu: rt2661_ucode.h LICENSE
	(cat rt2661_ucode.h; \
	 echo 'int main(void) { \
	     write(1, rt2561_ucode, sizeof(rt2561_ucode)); return 0; \
	 }') | ${CC} -o build -x c -
	(sed 's/^/# /' LICENSE; ./build | uuencode rt2561.fw) > ${.TARGET}

rt2661.fw.uu: rt2661_ucode.h LICENSE
	(cat rt2661_ucode.h; \
	 echo 'int main(void) { \
	     write(1, rt2661_ucode, sizeof(rt2661_ucode)); return 0; \
	 }') | ${CC} -o build -x c -
	(sed 's/^/# /' LICENSE; ./build | uuencode rt2661.fw) > ${.TARGET}

rt2860.fw.uu: rt2661_ucode.h LICENSE
	(cat rt2661_ucode.h; \
	 echo 'int main(void) { \
	     write(1, rt2860_ucode, sizeof(rt2860_ucode)); return 0; \
	 }') | ${CC} -o build -x c -
	(sed 's/^/# /' LICENSE; ./build | uuencode rt2860.fw) > ${.TARGET}

clean:
	rm -f build build.c ${FILES}

.include <bsd.prog.mk>
