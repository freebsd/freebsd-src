/*
 * test.c -- a simple utility for testing audio I/O
 *
 * (C) Luigi Rizzo 1997
 *
 * This code mmaps the io descriptor, then every second dumps the
 * relevant data structures.
 *
 * call it as "test unit" where unit is the unit number you want
 * to see displayed.
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/select.h>

caddr_t r, w, d;

#include </sys/i386/isa/snd/sound.h>

void
print_d(u_long *p, int unit)
{
    snddev_info *d;
    int i;
    for (i=0; i<2000; i++) {
	d = (snddev_info *)&(p[i]);
	if ( d->magic == MAGIC(unit) )
	    break;
    }
    if (i == 2000) {
	printf("desc not found\n");
	return;
    }
    printf("device type %d name %s\n", d->type, d->name);
    for (i=0;;i++) {
	if (i%20 == 0)
	    printf("flags... fmt speed .bsz.  c in-rl:in-dl:in-fl.ints "
				  " c ou-fl:ou_dl:ou-rl.ints |\n");
	printf("%08x %3x %5d %5d  %d %5d %5d %5d %4d  %d %5d %5d %5d %4d |\n",
	    d->flags, d->play_fmt, d->play_speed, d->play_blocksize,
	    d->dbuf_in.chan,
	    d->dbuf_in.rl,
	    d->dbuf_in.dl,
	    d->dbuf_in.fl,
	    d->dbuf_in.int_count,

	    d->dbuf_out.chan,
	    d->dbuf_out.fl,
	    d->dbuf_out.dl,
	    d->dbuf_out.rl,
	    d->dbuf_out.int_count);
	sleep(1);
    }
}
    
main(int argc, char *argv[])
{
    int fd ;
    int unit = 0;
    char devn[64];

    if (argc>1) unit=atoi(argv[1]);
    sprintf(devn,"/dev/mixer%d", unit);
    fd = open (devn, O_RDWR);
    printf("open returns %d\n", fd);

    w = mmap(NULL, 0x10000, PROT_READ, 0, fd, 0); /* play */ 
    r = mmap(NULL, 0x10000, PROT_READ, 0, fd, 1<<24); /* rec */ 
    d = mmap(NULL, 0x2000, PROT_READ, 0, fd, 2<<24); /* desc */ 

    printf("mmap: w 0x%08lx, r 0x%08lx, d 0x%08lx\n", w, r, d);
    if (d && (int)d != -1 ) {
	print_d((u_long *)d, unit);
    }
    if (w && (int)w != -1) munmap(w, 0x10000);
    if (r && (int)r != -1) munmap(r, 0x10000);
    if (d && (int)d != -1) munmap(d, 0x2000);
    return 0;
}
