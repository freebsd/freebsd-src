# $FreeBSD$
#
PROG=	mkdosfs

CFLAGS+= -Wall

###################################################################
#
# Everything below is solely intented for maintenance.
# As you can see, it requires as86/ld86 from the ``bcc'' package.
#
# For this reason, the bootcode.h target puts the result into
# ${.CURDIR}

AS86=		as86
LD86=		ld86
AS86FLAGS=	-0
LD86FLAGS=	-0 -s

CLEANFILES+=	*.obj *.bin *.com
.SUFFIXES: .asm .obj .bin .com

.asm.obj:
	${AS86} ${AS86FLAGS} -o ${.TARGET} ${.IMPSRC}

.obj.bin:
	${LD86} ${LD86FLAGS} -T 0x7c00 -o ${.PREFIX}.tmp ${.IMPSRC}
	dd bs=32 skip=1 of=${.TARGET} if=${.PREFIX}.tmp
	rm -f ${.PREFIX}.tmp

# .com file is just for testing
.obj.com:
	${LD86} ${LD86FLAGS} -T 0x100 -o ${.PREFIX}.tmp ${.IMPSRC}
	dd bs=32 skip=1 of=${.TARGET} if=${.PREFIX}.tmp
	rm -f ${.PREFIX}.tmp

## Do NOT depend this on bootcode.bin unless you've installed the
## bcc package!
bootcode.h: ## bootcode.bin
	@echo converting bootcode.bin into bootcode.h...
	@perl -e 'if(read(STDIN,$$buf,512)<512) {		\
			die "Read error on .bin file\n";	\
		}						\
		@arr = unpack("C*",$$buf);			\
		print "#ifndef BOOTCODE_H\n";			\
		print "#define BOOTCODE_H 1\n\n";		\
		print "/*\n * This file has been generated\n";	\
		print " * automatically.  Do not edit.\n */\n\n"; \
		print "static unsigned char bootcode[512] = {\n"; \
		for($$i=0; $$i<512; $$i++) {			\
			printf "0x%02x, ",$$arr[$$i];		\
			if($$i % 12 == 11) {print "\n";}	\
		}						\
		print "};\n\n";					\
		print "#endif /* BOOTCODE_H */\n";' 		\
	< bootcode.bin > ${.CURDIR}/bootcode.h

.include <bsd.prog.mk>

