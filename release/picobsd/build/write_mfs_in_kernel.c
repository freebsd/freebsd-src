/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD$
 *
 * This program patches a filesystem into a kernel made with MD_ROOT
 * option.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <ufs/ffs/fs.h>

static int force = 0;	/* don't check for zeros, may corrupt kernel */

int
main(int argc, char **argv)
{
	unsigned char *buf_kernel, *buf_fs, *p,*q, *prog;
	int fd_kernel, fd_fs, ch, errs=0;
	struct stat st_kernel, st_fs;
	u_long l;

	prog= *argv;
	while ((ch = getopt(argc, argv, "f")) != EOF)
		switch(ch) {                                                       
		case 'f':                                                          
			force = 1 - force;
			break;   
		default:
			errs++;
		}
	argc -= optind;
	argv += optind;

	if (errs || argc != 2) {
		fprintf(stderr,"Usage:\n\t%s [-f] kernel fs\n", prog);
		exit(2);
	}
	--argv;	/* original prog did not use getopt(3) */
	fd_kernel = open(argv[1],O_RDWR);
	if (fd_kernel < 0) { perror(argv[1]); exit(2); }
	fstat(fd_kernel,&st_kernel);
	fd_fs = open(argv[2],O_RDONLY);
	if (fd_fs < 0) { perror(argv[2]); exit(2); }
	fstat(fd_fs,&st_fs);
	buf_kernel = malloc(st_kernel.st_size);
	if (!buf_kernel) { perror("malloc"); exit(2); }
	buf_fs = malloc(st_fs.st_size);
	if (!buf_fs) { perror("malloc"); exit(2); }
	if (st_kernel.st_size != read(fd_kernel,buf_kernel,st_kernel.st_size))
		{ perror(argv[1]); exit(2); }
	if (st_fs.st_size != read(fd_fs,buf_fs,st_fs.st_size))
		{ perror(argv[2]); exit(2); }
	for(l=0,p=buf_kernel; l < st_kernel.st_size - st_fs.st_size ; l++,p++ )
		if(*p == 'M' && !strcmp(p,"MFS Filesystem goes here"))
			goto found;
	fprintf(stderr,"MFS filesystem signature not found in %s\n",argv[1]);
	exit(1);
found:
	if (!force)
		for(l=0,q= p + SBOFF; l < st_fs.st_size - SBOFF ; l++,q++ )
			if (*q)
				goto fail;
	memcpy(p+SBOFF,buf_fs+SBOFF,st_fs.st_size-SBOFF);
	lseek(fd_kernel,0L,SEEK_SET);
	if (st_kernel.st_size != write(fd_kernel,buf_kernel,st_kernel.st_size))
		{ perror(argv[1]); exit(2); }
	exit(0);
fail:
	l += SBOFF;
	fprintf(stderr,"Obstruction in kernel after %ld bytes (%ld Kbyte)\n",
		l, l/1024);
	fprintf(stderr,"Filesystem is %ld bytes (%ld Kbyte)\n",
		(u_long)st_fs.st_size, (u_long)st_fs.st_size/1024);
	exit(1);
}

/*
 * I added a '-f' option to force writing the image into the kernel, even when
 * there is already data (i.e. not zero) in the written area. This is useful
 * to rewrite a changed MD-image. Beware: If the written image is larger than
 * the space reserved in the kernel (with option MD_ROOT) then
 * THIS WILL CORRUPT THE KERNEL!
 *
 */
