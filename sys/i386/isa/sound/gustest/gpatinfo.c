/*
 *	gpatinfo.c: This program demonstrates the patch management
 *		    interface of the GUS driver.
 *
 *	NOTE! The patch manager interface is highly device dependent,
 *	      currently incompletely implemented prototype and
 *	      will change before final implementation.
 * 
 */

#include <stdio.h>
#include <machine/ultrasound.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include "gmidi.h"

#define GUS_DEV 	gus_dev

#define patch_access(cmd, rec) \
	rec.command = cmd;\
	rec.device = gus_dev;\
	if (ioctl(seqfd, SNDCTL_PMGR_IFACE, &rec)==-1)\
	{\
		perror("/dev/sequencer(SNDCTL_PMGR_IFACE/" #cmd ")");\
		exit(-1);\
	}

SEQ_DEFINEBUF (2048);

int             seqfd;

int             gus_dev = -1;

/*
 * The function seqbuf_dump() must always be provided
 */

void
seqbuf_dump ()
{
  if (_seqbufptr)
    if (write (seqfd, _seqbuf, _seqbufptr) == -1)
      {
	perror ("write /dev/sequencer");
	exit (-1);
      }
  _seqbufptr = 0;
}

int
main (int argc, char *argv[])
{
  int             i, j, n;
  struct synth_info info;
  struct patch_info *patch;
  struct patmgr_info mgr, mgr2, mgr3;

  if ((seqfd = open ("/dev/sequencer", O_WRONLY, 0)) == -1)
    {
      perror ("/dev/sequencer");
      exit (-1);
    }

  if (ioctl (seqfd, SNDCTL_SEQ_NRSYNTHS, &n) == -1)
    {
      perror ("/dev/sequencer");
      exit (-1);
    }

/*
 * First locate the GUS device
 */

  for (i = 0; i < n; i++)
    {
      info.device = i;

      if (ioctl (seqfd, SNDCTL_SYNTH_INFO, &info) == -1)
	{
	  perror ("/dev/sequencer");
	  exit (-1);
	}

      if (info.synth_type == SYNTH_TYPE_SAMPLE
	  && info.synth_subtype == SAMPLE_TYPE_GUS)
	gus_dev = i;
    }

  if (gus_dev == -1)
    {
      fprintf (stderr, "Error: Gravis Ultrasound not detected\n");
      exit (-1);
    }

  printf("Gravis UltraSound device = %d\n", gus_dev);

 /*
  * Get type of the Patch Manager interface of the GUS device
  */

  patch_access(PM_GET_DEVTYPE, mgr);
  printf("Patch manager type: %d\n", mgr.parm1);

  if (mgr.parm1 != PMTYPE_WAVE)
  {
  	fprintf(stderr, "Hups, this program seems to be obsolete\n");
  	exit(-1);
  }

  /*
   * The GUS driver supports up to 256 different midi program numbers but
   * this limit can be changed before compiling the driver. The following
   * call returns the value compiled to the driver.
   */

  patch_access(PM_GET_PGMMAP, mgr);
  printf("Device supports %d midi programs.\n", mgr.parm1);

  /*
   * Each program can be undefined or it may have one or more patches.
   * A patch consists of header and the waveform data. If there is more
   * than one patch in a program, the right one is selected by checking the
   * note number when the program is played.
   *
   * The following call reads an array indexed by program number. Each
   * element defines the number of patches defined for the corresponding
   * program.
   */
  printf("Loaded programs:\n");

  for (i=0;i<mgr.parm1;i++)
  if (mgr.data.data8[i]) 
  {
     printf("%03d: %2d patches\n", i, mgr.data.data8[i]);

 /*
  * Next get the magic keys of the patches associated with this program.
  * This key can be used to access the patc data.
  */
     mgr2.parm1=i;
     patch_access(PM_GET_PGM_PATCHES, mgr2);
     for (j = 0;j<mgr2.parm1;j++)
     {
     	printf("\tPatch %d: %3d ", j, mgr2.data.data32[j]);

 /*
  * The last step is to read the patch header (without wave data).
  * The header is returned in the mgr3.data. The field parm1 returns
  * address of the wave data in tge GUS DRAM. Parm2 returns
  * size of the struct patch_info in the kernel.
  *
  * There is also the PM_SET_PATCH call which allows modification of the
  * header data. The only limitation is that the sample len cannot be
  * increased.
  */
     	mgr3.parm1 = mgr2.data.data32[j];
     	patch_access(PM_GET_PATCH, mgr3);
     	patch = (struct patch_info *)&mgr3.data; /* Pointer to the patch hdr */

     	printf("DRAM ptr = %7d, sample len =%6d bytes.\n", 
     		mgr3.parm1, patch->len);

     }
  }

  i = gus_dev;

  if (ioctl(seqfd, SNDCTL_SYNTH_MEMAVL, &i)==-1) exit(-1);
  printf("%d bytes of DRAM available for wave data\n", i);


  exit(0);
}
