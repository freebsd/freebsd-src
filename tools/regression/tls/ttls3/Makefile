# $FreeBSD$

all: ttls3

LDFLAGS=-shared -Bsymbolic --allow-shlib-undefined
CFLAGS+= -L${.CURDIR}/../../../../lib/libc -lc -lthr
CFLAGS+= -Wl,--rpath=${.CURDIR}/../../../../lib/libc
CFLAGS+= -Wl,--rpath=${.OBJDIR}
CFLAGS+= -Wl,--dynamic-linker=${.CURDIR}/../../../../libexec/rtld-elf/ld-elf.so.1

tls-lib: elftls.S
	gcc -c -o elftls.o elftls.S
	ld $(LDFLAGS) elftls.o -soname libtls.so.1 -o libtls.so.1
	ln -sf libtls.so.1 libtls.so

tls-test-lib: tls-lib tls-test-lib.c
	gcc -c -o tls-test.o tls-test-lib.c
	ld $(LDFLAGS) tls-test.o libtls.so.1 -rpath=${.OBJDIR} -soname libtls-test.so.1 -o libtls-test.so.1

ttls3: tls-test-lib tls-test.c
	gcc $(CFLAGS) -rdynamic -o ttls3 tls-test.c

clean:
	rm -f *.o libtls.so* libtls-test.so* ttls3
