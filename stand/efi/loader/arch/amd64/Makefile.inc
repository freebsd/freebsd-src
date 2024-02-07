
.PATH:	${BOOTSRC}/i386/libi386
SRCS+=	nullconsole.c \
	comconsole.c \
	spinconsole.c

.if ${DO32:U0} == 1 && ${MK_LOADER_IA32} != "no"
LOADER:=	${LOADER}-ia32
PROG=	${LOADER}.sym

CFLAGS+=	-I${.CURDIR}/arch/amd64/ia32
CFLAGS+=	-I${EFISRC}/include/i386

SRCS+= amd64_tramp32.S \
	start32.S \
	elf64_freebsd32.c \

NEWVERSWHAT=	"EFI loader" amd64-ia32
LDSCRIPT=	${.CURDIR}/../loader/arch/amd64/ia32/i386.ldscript
.if ${LOADER_INTERP} == ${LOADER_DEFAULT_INTERP}
LINKS=		${BINDIR}/${LOADER}.efi ${BINDIR}/loader-ia32.efi
.endif

.PATH: ${.CURDIR}/../loader/arch/amd64/ia32

LIBEFI32=		${BOOTOBJ}/efi/libefi32/libefi.a

EFI_TARGET=	efi-app-ia32
DPADD=		${LDR_INTERP32} ${LIBEFI32} ${LIBSA32} ${LDSCRIPT}
LDADD=		${LDR_INTERP32} ${LIBEFI32} ${LIBSA32}
.else
SRCS+=	amd64_tramp.S \
	start.S \
	elf64_freebsd.c \
	trap.c \
	multiboot2.c \
	exc.S

CFLAGS+=	-fPIC
LDFLAGS+=	-Wl,-znocombreloc

.if ${MK_LOADER_IA32} != "no"
# Make the 32-bit loader before we start
.BEGIN: loader32
loader32: .PHONY
	${MAKE} -C ${.CURDIR} MAKEOBJDIR=${.OBJDIR}/ia32 DO32=1 ${.TARGETS}
.endif
.endif
