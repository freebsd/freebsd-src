# Originally from	$NetBSD: Makefile,v 1.21 1997/10/26 22:08:38 lukem Exp $
#
# Notes:
# - We don't use the libc strerror/sys_errlist because the string table is
#   quite large.
#

.include <bsd.init.mk>

LIBSA_CPUARCH?=${MACHINE_CPUARCH}

LIB?=		sa

# standalone components and stuff we have modified locally
SRCS+=	gzguts.h zutil.h __main.c abort.c assert.c bcd.c environment.c \
	features.c getopt.c gets.c globals.c \
	hexdump.c nvstore.c pager.c panic.c printf.c strdup.c strerror.c \
	random.c sbrk.c tslog.c twiddle.c zalloc.c zalloc_malloc.c

# private (pruned) versions of libc string functions
SRCS+=	strcasecmp.c

.PATH: ${LIBCSRC}/net

SRCS+= ntoh.c

# string functions from libc
.PATH: ${LIBCSRC}/string
SRCS+=	bcmp.c bcopy.c bzero.c ffs.c fls.c \
	memccpy.c memchr.c memcmp.c memcpy.c memmove.c memset.c \
	strcat.c strchr.c strchrnul.c strcmp.c strcpy.c stpcpy.c stpncpy.c \
	strcspn.c strlcat.c strlcpy.c strlen.c strncat.c strncmp.c strncpy.c \
	strnlen.c strpbrk.c strrchr.c strsep.c strspn.c strstr.c strtok.c swab.c

# stdlib functions from libc
.PATH: ${LIBCSRC}/stdlib
SRCS+=	abs.c strtol.c strtoll.c strtoul.c strtoull.c

# common boot code
.PATH:	${SYSDIR}/kern
SRCS+=	subr_boot.c

.if ${MACHINE_CPUARCH} == "arm"
.PATH: ${LIBCSRC}/arm/gen

# Do not generate movt/movw, because the relocation fixup for them does not
# translate to the -Bsymbolic -pie format required by self_reloc() in loader(8).
# Also, the fpu is not available in a standalone environment.
CFLAGS.clang+=	-mno-movt
CFLAGS.clang+=	-mfpu=none

.PATH: ${SRCTOP}/contrib/llvm-project/compiler-rt/lib/builtins/arm/
SRCS+=	aeabi_idivmod.S aeabi_ldivmod.S aeabi_uidivmod.S aeabi_uldivmod.S
SRCS+=	aeabi_memcmp.S aeabi_memcpy.S aeabi_memmove.S aeabi_memset.S
.endif

.if ${MACHINE_CPUARCH} == "aarch64" || ${MACHINE_CPUARCH} == "riscv"
.PATH: ${LIBCSRC}/${MACHINE_CPUARCH}/gen
.endif

# Compiler support functions
.PATH: ${SRCTOP}/contrib/llvm-project/compiler-rt/lib/builtins/
# __clzsi2 and ctzsi2 for various builtin functions
SRCS+=	clzsi2.c ctzsi2.c
# Divide and modulus functions called by the compiler
SRCS+=	divmoddi4.c  divmodsi4.c  divdi3.c  divsi3.c  moddi3.c  modsi3.c
SRCS+=	udivmoddi4.c udivmodsi4.c udivdi3.c udivsi3.c umoddi3.c umodsi3.c
SRCS+=	ashldi3.c ashrdi3.c lshrdi3.c

.if ${MACHINE_CPUARCH:Namd64:Ni386} == ""
.PATH: ${SASRC}/x86
SRCS+= hypervisor.c
.endif

.if ${MACHINE_CPUARCH} == "powerpc"
SRCS+=	syncicache.c
.endif

# uuid functions from libc
.PATH: ${LIBCSRC}/uuid
SRCS+= uuid_create_nil.c uuid_equal.c uuid_from_string.c uuid_is_nil.c uuid_to_string.c

# _setjmp/_longjmp
.PATH: ${SASRC}/${LIBSA_CPUARCH}
SRCS+=	_setjmp.S

# decompression functionality from libbz2
# NOTE: to actually test this functionality after libbz2 upgrade compile
# loader(8) with LOADER_BZIP2_SUPPORT defined
.PATH: ${SRCTOP}/contrib/bzip2
.for i in bzlib.c crctable.c decompress.c huffman.c randtable.c
CFLAGS.${i}+= -DBZ_NO_STDIO -DBZ_NO_COMPRESS
SRCS+=${i}
.endfor

# decompression functionality from zlib
# https://github.com/madler/zlib/issues/633 documents why we suppress deprecated
# prototype warnings.
.PATH: ${SRCTOP}/sys/contrib/zlib
ZLIB_CFLAGS=-DHAVE_MEMCPY -I${SRCTOP}/sys/contrib/zlib ${NO_WDEPRECATED_NON_PROTOTYPE}
.for i in adler32.c crc32.c infback.c inffast.c inflate.c inftrees.c zutil.c
CFLAGS.${i}+=${ZLIB_CFLAGS}
SRCS+=	${i}
.endfor

# lz4 decompression functionality
.PATH: ${SRCTOP}/sys/cddl/contrib/opensolaris/common/lz4
SRCS+=	lz4.c
CFLAGS.lz4.c+= -I${SRCTOP}/sys/cddl/contrib/opensolaris/common/lz4

# io routines
SRCS+=	closeall.c dev.c ioctl.c nullfs.c stat.c mount.c \
	fstat.c close.c lseek.c open.c read.c write.c readdir.c preload.c

# SMBios routines
SRCS+=	smbios.c
.if !defined(BOOT_HIDE_SERIAL_NUMBERS)
# Export serial numbers, UUID, and asset tag from loader.
CFLAGS.smbios.c+= -DSMBIOS_SERIAL_NUMBERS
.if defined(BOOT_LITTLE_ENDIAN_UUID)
# Use little-endian UUID format as defined in SMBIOS 2.6.
CFLAGS.smbios.c+= -DSMBIOS_LITTLE_ENDIAN_UUID
.elif defined(BOOT_NETWORK_ENDIAN_UUID)
# Use network-endian UUID format for backward compatibility.
CFLAGS.smbios.c+= -DSMBIOS_NETWORK_ENDIAN_UUID
.endif
.endif

# network routines
SRCS+=	arp.c ether.c ip.c inet_ntoa.c in_cksum.c net.c udp.c netif.c rpc.c

# network info services:
SRCS+=	bootp.c rarp.c bootparam.c

# boot filesystems
SRCS+=	ufs.c nfs.c cd9660.c tftp.c gzipfs.c bzipfs.c
SRCS+=	dosfs.c ext2fs.c
SRCS+=	splitfs.c
SRCS+=	pkgfs.c

# Time support
SRCS+=	time.c

# kernel ufs support
.PATH: ${SRCTOP}/sys/ufs/ffs
SRCS+=ffs_subr.c ffs_tables.c

#
# i386 has a constrained space for its /boot/loader, so compile out the
# extensive messages diagnosing bad superblocks. i386 doesn't support UEFI
# booting, so doing it always makes sense natively there. When we compile
# for 32-bit on amd64, LIBSA_CPUARCH is also i386 and we use libsa32 only
# for the BIOS /boot/loader which has the same constraints.
#
.if ${LIBSA_CPUARCH} == "i386"
CFLAGS.ffs_subr.c+= -DSTANDALONE_SMALL
.endif

CFLAGS.gzipfs.c+= ${ZLIB_CFLAGS}
CFLAGS.pkgfs.c+= ${ZLIB_CFLAGS}
CFLAGS.bzipfs.c+= -I${SRCTOP}/contrib/bzip2  -DBZ_NO_STDIO -DBZ_NO_COMPRESS

# explicit_bzero and calculate_crc32c
.PATH: ${SYSDIR}/libkern
SRCS+=  explicit_bzero.c crc32_libkern.c

# Bits from GELI reused for some random seeding cases
.PATH: ${SYSDIR}/geom/eli
SRCS+=	g_eli_hmac.c pkcs5v2.c

# Maybe GELI
.if ${MK_LOADER_GELI} == "yes"
.include "${SASRC}/geli/Makefile.inc"
.endif

.if ${MK_LOADER_VERIEXEC} == "yes" && ${MK_BEARSSL} == "yes"
# XXX Note that these pollutes CFLAGS in a way that's not easy to fix
.include "${SRCTOP}/lib/libbearssl/Makefile.libsa.inc"
.include "${SRCTOP}/lib/libsecureboot/Makefile.libsa.inc"
.endif

# Maybe ZFS
.if ${MK_LOADER_ZFS} == "yes"
.include "${SASRC}/zfs/Makefile.inc"
.endif

# Crypto hashing functions
# sha256 and sha512 from sys/crypto
.PATH: ${SYSDIR}/crypto/sha2
SRCS+=		sha256c.c sha512c.c

# md5 from the kernel
.PATH: ${SYSDIR}/kern
SRCS+=		md5c.c

.if ${DO32:U0} == 0
MAN=libsa.3
.endif

# Create a subset of includes that are safe, as well as adjusting those that aren't
# The lists may drive people nuts, but they are explicitly opt-in
FAKE_DIRS=xlocale arpa ssp
SAFE_INCS=a.out.h assert.h elf.h inttypes.h limits.h nlist.h setjmp.h stddef.h stdbool.h string.h strings.h time.h unistd.h uuid.h
STAND_H_INC=ctype.h fcntl.h signal.h stdio.h stdlib.h
OTHER_INC=stdarg.h errno.h stdint.h

beforedepend:
	mkdir -p ${FAKE_DIRS}; \
	for i in ${SAFE_INCS}; do \
		ln -sf ${SRCTOP}/include/$$i $$i; \
	done; \
	ln -sf ${SYSDIR}/${MACHINE}/include/stdarg.h stdarg.h; \
	ln -sf ${SYSDIR}/sys/errno.h errno.h; \
	ln -sf ${SYSDIR}/sys/stdint.h stdint.h; \
	ln -sf ${SRCTOP}/include/arpa/inet.h arpa/inet.h; \
	ln -sf ${SRCTOP}/include/arpa/tftp.h arpa/tftp.h; \
	ln -sf ${SRCTOP}/include/ssp/ssp.h ssp/ssp.h; \
	for i in _time.h _strings.h _string.h; do \
		[ -f xlocale/$$i ] || :> xlocale/$$i; \
	done; \
	for i in ${STAND_H_INC}; do \
		ln -sf ${SASRC}/stand.h $$i; \
	done
CLEANDIRS+=${FAKE_DIRS}
CLEANFILES+= ${SAFE_INCS} ${STAND_H_INC} ${OTHER_INC}

.include <bsd.lib.mk>
