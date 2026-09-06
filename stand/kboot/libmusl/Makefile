# This is a narrow musl networking subset, not a standalone libc.  It keeps the
# Linux networking ABI/types and selected resolver/socket code, while local
# shims route allocation, errno, and syscalls through the kboot/libsa runtime.

NO_LIBSA_CFLAGS=	yes

.include <bsd.init.mk>

LIB=	musl
WARNS?=	4

MUSL_ALLTYPES_H=	musl/bits/alltypes.h
MUSL_SYSCALL_H=	musl/bits/syscall.h
DPSRCS+=	${MUSL_ALLTYPES_H} ${MUSL_SYSCALL_H}
CLEANFILES+=	${MUSL_ALLTYPES_H} ${MUSL_SYSCALL_H}

MUSL_CFLAGS=	-D_XOPEN_SOURCE=700 \
		-include ${.CURDIR}/musl_symbol_renames.h \
		-I${MUSLDIR}/arch/${MUSL_ARCH} \
		-I${MUSLDIR}/arch/generic \
		-I${MUSLDIR}/src/include \
		-I${MUSLDIR}/src/internal \
		-I${.OBJDIR}/musl \
		-I${MUSLDIR}/include \
		-I${MUSLDIR}/src/network

MUSL_PATHS= \
	src/fcntl \
	src/internal \
	src/linux \
	src/misc \
	src/network \
	src/select \
	src/stdlib \
	src/ctype \
	src/time \
	src/unistd

.for _dir in ${MUSL_PATHS}
.PATH:	${MUSLDIR}/${_dir}
.endfor

MUSL_UPSTREAM_SRCS= \
		bind.c \
		clock_gettime.c \
		close.c \
		connect.c \
		dn_expand.c \
		dns_parse.c \
		fcntl.c \
		gai_strerror.c \
		getentropy.c \
		getrandom.c \
		getsockname.c \
		getsockopt.c \
		inet_aton.c \
		inet_pton.c \
		if_nametoindex.c \
		ioctl.c \
		isalnum.c \
		lookup_ipliteral.c \
		poll.c \
		qsort.c \
		qsort_nr.c \
		recv.c \
		recvfrom.c \
		recvmsg.c \
		res_msend.c \
		res_mkquery.c \
		setsockopt.c \
		send.c \
		sendmsg.c \
		sendto.c \
		socket.c \
		syscall_ret.c

MUSL_WRAPPER_SRCS= \
		freeaddrinfo_wrapper.c \
		getaddrinfo_wrapper.c \
		lookup_name_wrapper.c

MUSL_STUB_SRCS= \
		lock_stub.c \
		pthread_stub.c \
		resolvconf_stub.c \
		vdso_stub.c

MUSL_LOCAL_SRCS= \
		calloc.c \
		errno.c \
		free.c \
		lctrans_stub.c \
		musl_compat.c \
		syscall_cp.c

MUSL_SRCS=	${MUSL_UPSTREAM_SRCS} \
		${MUSL_WRAPPER_SRCS} \
		${MUSL_STUB_SRCS} \
		${MUSL_LOCAL_SRCS}

SRCS=	${MUSL_SRCS}

.for _src in ${MUSL_SRCS}
CFLAGS.${_src}+=	${MUSL_CFLAGS}
CFLAGS.${_src}+=	-Udouble -Ufloat
.endfor

# Generate musl arch headers in the object tree.
${MUSL_ALLTYPES_H}: ${MUSLDIR}/arch/${MUSL_ARCH}/bits/alltypes.h.in \
    ${MUSLDIR}/include/alltypes.h.in \
    ${MUSLDIR}/tools/mkalltypes.sed
	@mkdir -p ${.TARGET:H}
	sed -f ${MUSLDIR}/tools/mkalltypes.sed \
	    ${MUSLDIR}/arch/${MUSL_ARCH}/bits/alltypes.h.in \
	    ${MUSLDIR}/include/alltypes.h.in > ${.TARGET}

${MUSL_SYSCALL_H}: ${MUSLDIR}/arch/${MUSL_ARCH}/bits/syscall.h.in
	@mkdir -p ${.TARGET:H}
	cp ${.ALLSRC} ${.TARGET}
	sed -n -e s/__NR_/SYS_/p < ${.ALLSRC} >> ${.TARGET}

.include <bsd.lib.mk>
