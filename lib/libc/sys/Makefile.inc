# libc-specific portion of the system call interface

.PATH: ${LIBC_SRCTOP}/sys

# Most of the implementation is shared with libsys:
.include "${LIBSYS_SRCTOP}/Makefile.sys"

# emit empty assembly stubs for syscalls in dynamic libc
SHARED_CFLAGS+=	-D'_SYSCALL_BODY(name)='

SYM_MAPS+=	${LIBC_SRCTOP}/sys/Symbol.map

# Add the interposer wrappers
SRCS+=	${INTERPOSED:S/$/.c/}

# Pseudo system calls implemented atop other interfaces.
SRCS+= \
	POSIX2x_Fork.c \
	brk.c \
	closefrom.c \
	compat-stub.c \
	creat.c \
	getdents.c \
	lockf.c \
	lstat.c \
	mknod.c \
	pipe.c \
	recv.c \
	recvmmsg.c \
	send.c \
	sendmmsg.c \
	shm_open.c \
	stat.c \
	vadvise.c \
	wait.c \
	wait3.c \
	waitid.c \
	waitpid.c
