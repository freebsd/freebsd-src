/*
 * Copyright (c) 1996 Whistle Communications
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * Whistle Communications allows free use of this software in its "as is"
 * condition.  Whistle Communications disclaims any liability of any kind for
 * any damages whatsoever resulting from the use of this software.
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD: src/sbin/i386/nextboot/nextboot.c,v 1.6 1999/08/28 00:13:06 peter Exp $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/disklabel.h>
#include <fcntl.h>
#include <err.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

struct mboot
{
	unsigned char padding[2]; /* force the longs to be long aligned */
	unsigned char bootinst[DOSPARTOFF];
	struct	dos_partition parts[4];
	unsigned short int	signature;
};
struct mboot mboot;

#define NAMEBLOCK 1 /* 2nd block */
#define BLOCKSIZE 512
#define ENABLE_MAGIC 0xfadefeed
#define DISABLE_MAGIC 0xfadefade
static int	bflag;
static int	eflag;
static int	dflag;

#define BOOT_MAGIC 0xAA55

static void
usage(void) {
	fprintf (stderr, "%s\n%s\n",
	"usage: nextboot [-b] device bootstring [bootstring] ...",
	"       nextboot {-e,-d} device");
	exit(1);
}

int
main (int argc, char** argv)
{
	int 	fd = -1;
	char namebuf[1024], *cp = namebuf;
	int	i,j;
	int	ch;
	int	part;

	bflag = 0;
	while ((ch = getopt(argc, argv, "bde")) != -1) {
        	switch(ch) {
        	case 'b':
                	bflag = 1;
                	break;
        	case 'd':
                	dflag = 1;
                	break;
        	case 'e':
                	eflag = 1;
                	break;
        	case '?':
        	default:
                	usage();
		}
     	}
	argc -= optind;
	argv += optind;

	if ( (dflag + eflag + bflag) > 1 ) {
		usage();
	}
	if (dflag + eflag){
		if(argc != 1 ) {
			usage();
		}
	} else {
		if (argc <2) {
			usage();
		}
	}
        if ((fd = open(argv[0], O_RDWR, 0)) < 0)
		errx(1, "can't open %s", argv[0]);

	argc--;
	argv++;

	/*******************************************
	 * Check that we have an MBR
	 */
	if (lseek(fd,0,0) == -1)
		err(1, "lseek");
	if (read (fd,&mboot.bootinst[0],BLOCKSIZE ) != BLOCKSIZE)
		err(1, "read0");
	if (mboot.signature != (unsigned short)BOOT_MAGIC)
		errx(1, "no fdisk part.. not touching block 1");

	/*******************************************
	 * And check that none of the partitions in it cover the name block;
	 */
	for ( part = 0; part < 4; part++) {
		if( mboot.parts[part].dp_size
		 && (mboot.parts[part].dp_start <= NAMEBLOCK)
		 && (mboot.parts[part].dp_start
			+ mboot.parts[part].dp_size > NAMEBLOCK))
			errx(1,
		"name sector lies within a Bios partition: aborting write");
	}


	/*******************************************
	 *  Now check the  name sector itself to see if it's been initialized.
	 */
	if (lseek(fd,NAMEBLOCK * BLOCKSIZE,0) == -1)
		err(1, "lseek");
	if (read(fd,namebuf,BLOCKSIZE) != BLOCKSIZE)
		err(1, "read1");
	/*******************************************
	 * check if we are just enabling or disabling
	 * Remember the flags are exclusive..
	 */
	if(!bflag) { /* don't care what's there if bflag is set */
		switch(*(unsigned long *)cp)
		{
		case	DISABLE_MAGIC:
		case	ENABLE_MAGIC:
			break;
		default:
			errx(1, "namesector not initialized, use the -b flag");
		}
	}


	/*******************************************
	 *  If the z or r flag is set, damage or restore the magic number..
	 * to disable/enable the feature
	 */
	if(dflag) {
		*(unsigned long *)cp = DISABLE_MAGIC;
	} else {
		*(unsigned long *)cp = ENABLE_MAGIC;
	}
	if ((!dflag) && (!eflag)) {
		/*******************************************
	 	*  Create a new namesector in ram 
	 	*/
		cp += 4;
		for ( i = 0 ; i < argc ; i++ ) {
			*cp++ = 'D';
			*cp++ = 'N';
			j = strlen(argv[i]);
			strncpy(cp,argv[i],j);
			cp += j;
			*cp++ = 0;
		}
		*cp++ = 0xff;
		*cp++ = 0xff;
		*cp++ = 0xff;
		namebuf[BLOCKSIZE-1] = 0; /* paranoid */
		namebuf[BLOCKSIZE] = 0xff;
	}

	/*******************************************
	 *  write it to disk.
	 */
	if (lseek(fd,NAMEBLOCK * BLOCKSIZE,0) == -1)
		err(1, "lseek");
	if(write (fd,namebuf,BLOCKSIZE ) != BLOCKSIZE)
		err(1, "write");

#if 0
	/*******************************************
	 * just to be safe/paranoid.. read it back..
	 * and print it..
	 */
	if (lseek(fd,NAMEBLOCK * BLOCKSIZE,0) == -1)
		err(1, "lseek (second)");
	read (fd,namebuf,512);
	for (i = 0;i< 16;i++) {
		for ( j = 0; j < 16; j++) {
			printf("%02x ",(unsigned char )namebuf[(i*16) + j ]);
		}
		printf("\n");
	}
#endif
	exit(0);
}
