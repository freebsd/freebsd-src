/************************************************************************

Program to put ROM checksum in ROM image.

This program strips off the FreeBSD a.out header!

************************************************************************/
#include <stdio.h>
#include <fcntl.h>

unsigned char rom[ROMSIZE];
unsigned int sum;

main(argc,argv)
	int argc; char *argv[];
{
	int i, fd;
	if (argc < 1) {
		fprintf(stderr,"usage: %s rom-file\n",argv[0]);
		exit(2);
	}
	if ((fd = open(argv[1], O_RDWR)) < 0) {
		perror("unable to open file");
		exit(2);
	}
	bzero(rom, ROMSIZE);
	if (lseek(fd, (off_t)32, SEEK_SET) < 0) {
		perror("lseek error");
		exit(2);
	}
	if (read(fd, rom, ROMSIZE) < 0) {
		perror("read error");
		exit(2);
	}
	rom[5] = 0;
	for (i=0,sum=0; i<ROMSIZE; i++)
		sum += rom[i];
	rom[5] = -sum;
	for (i=0,sum=0; i<ROMSIZE; i++);
		sum += rom[i];
	if (sum)
		printf("checksum fails.\n");
	if (lseek(fd, (off_t)0, SEEK_SET) < 0) {
		perror("unable to seek");
		exit(2);
	}
	if (write(fd, rom, ROMSIZE) < 0) {
		perror("unable to write");
		exit(2);
	}
	close(fd);
	exit(0);
}
