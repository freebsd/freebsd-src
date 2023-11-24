ACFLAGS= -target armv7-unknown-freebsd${OS_REVISION} -nostdlib -Wl,-e -Wl,main -static -mhwdiv=arm

swp_test_impl: swp_test_impl.S
	${CC} ${ACFLAGS} -o ${.TARGET} ${.ALLSRC}
