/********************************************************/
/*  map_vme.c                                           */
/*  VME control of TrueTime VME-SG sync gen  card       */
/*  and  TrueTime GPS-VME      receiver card            */
/* Version for 700 series HPUX 9.0                      */
/* Richard E.Schmidt, US Naval Observatory, Washington  */
/*  27 March 94                                         */
/********************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#if defined(REFCLOCK) && defined(CLOCK_GPSVME)
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/types.h> 
#include <sys/stat.h> 
#include <sys/sysmacros.h> 
#include <sys/rtprio.h>		/* for rtprio */
#include <sys/lock.h>		/* for plock */
#include "/etc/conf/machine/vme2.h"
#include "/etc/conf/h/io.h"
#include "gps.h"

/* GLOBALS */
void *gps_base;
unsigned short  *greg[NREGS];
struct vme2_map_addr ma;           /* memory mapped structure */
int fd;                            /* file descriptor for VME */

void unmap_vme (); 

caddr_t
map_vme (
	char *filename
	)
{
	int ret; 
	caddr_t base;
	struct vme2_io_testx tx;
	caddr_t cp;

#define VME_START_ADDR  0x00000   /* Starting address in A16N VME Space */
#define VMESIZE 0xFF      /* 256 bytes of A16N space length */

	/* 
	   To create the HP9000/700 series device file, /dev/vme2:
	   mknod /dev/vme2 c 44 0x0; chmod 600 /dev/vme2

	   Then must create /etc/vme.CFG and run /etc/vme_config and reboot.
	*/
	if ((fd = open (filename, O_RDWR)) < 0) {
		printf("ERROR: VME bus adapter open failed. errno:%d\n",
		       errno);
		if(errno == ENODEV) {
			printf("ENODEV. Is driver in kernel? vme2 in dfile?\n");
		}
		exit(errno);
	}
	tx.card_type = VME_A16;
	tx.vme_addr = VME_START_ADDR;
	tx.width = SHORT_WIDE;

	if(ioctl(fd, VME2_IO_TESTR, &tx)) {
		printf("ioctl to test VME space failed. Errno: %d\n",
		       errno);
		exit(errno);
	}
	if(tx.error)
	    printf("io_testr failed internal error %d\n",tx.error);
	if(tx.access_result < 0) {
		printf("io_testr failed\n");
		exit(2);
	}

	/* If successful mmap the device */
	/* NOW MAP THE CARD  */
	ma.card_type = VME_A16;
	ma.vme_addr = VME_START_ADDR;
	ma.size = VMESIZE;

	if(ioctl(fd, VME2_MAP_ADDR, &ma)) {
		printf("ioctl to map VME space failed. Errno: %d\n",
		       errno);
		exit(errno);
	}
	if(ma.error) {
		printf("ioctl to map VME failed\n");
		exit(ENOMEM);
	}
	base = ma.user_addr;
	return(base);   
}


void
unmap_vme(void)
{
	if(ioctl(fd, VME2_UNMAP_ADDR, &ma)) 
	    printf("ioctl to unmap VME space failed. Errno: %d\n",
		   errno);
	close(fd);
	return;
}


int
init_vme(boid)
{
	/*  set up address offsets */

	gps_base = map_vme (GPS_VME);

/* offsets from base address: */

	greg[0] = (unsigned short *)gps_base + GFRZ1;
	greg[1] = (unsigned short *)gps_base + GUFRZ1;
	greg[2] = (unsigned short *)gps_base + GREG1A;
	greg[3] = (unsigned short *)gps_base + GREG1B;
	greg[4] = (unsigned short *)gps_base + GREG1C;
	greg[5] = (unsigned short *)gps_base + GREG1D;
	greg[6] = (unsigned short *)gps_base + GREG1E;

	return (0); 
}

#else /* not (REFCLOCK && CLOCK_GPSVME) */
int map_vme_bs;
#endif /* not (REFCLOCK && CLOCK_GPSVME) */
