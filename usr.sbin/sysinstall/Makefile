PROG=	sysinstall
NOMAN=	yes
CLEANFILES+=	makedevs.c rtermcap dumpnlist

.PATH: ${.CURDIR}/../disklabel ${.CURDIR}/../../usr.bin/cksum

SRCS=	anonFTP.c apache.c attr.c cdrom.c command.c config.c devices.c \
	disks.c dispatch.c dist.c dmenu.c doc.c dos.c floppy.c ftp.c \
	ftp_strat.c globals.c index.c install.c installUpgrade.c \
	label.c lndir.c main.c makedevs.c media.c menus.c misc.c \
	msg.c network.c nfs.c options.c package.c samba.c system.c \
	tape.c tcpip.c termcap.c ufs.c variable.c wizard.c \
	uc_eisa.c uc_isa.c uc_kmem.c uc_list.c uc_main.c uc_pci.c \
	uc_scsi.c

CFLAGS+= -Wall -I${.CURDIR}/../../gnu/lib/libdialog -I${.OBJDIR} -I/sys
CFLAGS+= -DUC_PRIVATE -DKERN_NO_SYMBOLS -DSAVE_USERCONFIG

#CFLAGS+=	-DLOAD_CONFIG_FILE=\"install.cfg\"

DPADD=	${LIBDIALOG} ${LIBNCURSES} ${LIBMYTINFO} ${LIBUTIL} ${LIBDISK}
LDADD=	-ldialog -lncurses -lmytinfo -lutil -ldisk


makedevs.c:	Makefile rtermcap
	rm -f makedevs.tmp
	echo '#include <sys/types.h>' > makedevs.tmp
	./rtermcap cons25 | \
		file2c 'const char termcap_cons25[] = {' ',0};' \
		>> makedevs.tmp
	./rtermcap cons25-m | \
		file2c 'const char termcap_cons25_m[] = {' ',0};' \
		>> makedevs.tmp
	./rtermcap cons25r | \
		file2c 'const char termcap_cons25r[] = {' ',0};' \
		>> makedevs.tmp
	./rtermcap cons25r-m | \
		file2c 'const char termcap_cons25r_m[] = {' ',0};' \
		>> makedevs.tmp
	./rtermcap cons25l1 | \
		file2c 'const char termcap_cons25l1[] = {' ',0};' \
		>> makedevs.tmp
	./rtermcap cons25l1-m | \
		file2c 'const char termcap_cons25l1_m[] = {' ',0};' \
		>> makedevs.tmp
	./rtermcap vt100 | \
		file2c 'const char termcap_vt100[] = {' ',0};' \
		>> makedevs.tmp
	mv makedevs.tmp makedevs.c

rtermcap:	${.CURDIR}/rtermcap.c
	${CC} -o rtermcap ${.CURDIR}/rtermcap.c -ltermcap

testftp:       ftp.c
	cc -o testftp -I../libdisk -DSTANDALONE_FTP ftp.c

.include <bsd.prog.mk>
