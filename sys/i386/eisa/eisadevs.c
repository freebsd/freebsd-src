/*
 * Written by Billie Alsup (balsup@tfs.com)
 * for TRW Financial Systems for use under the MACH(2.5)and OSF/1 operating
 * systems.
 *
 * TRW Financial Systems, in accordance with their agreement with Carnegie
 * Mellon University, makes this software available to CMU to distribute
 * or use in any manner that they see fit as long as this message is kept with
 * the software. For this reason TFS also grants any other persons or
 * organisations permission to use or modify this software.
 *
 * TFS supplies this software to be publicly redistributed
 * on the understanding that TFS is not responsible for the correct
 * functioning of this software in any circumstances.
 *
 * $Id: eisadevs.c,v 1.1 1995/03/13 09:10:17 root Exp root $
 */

/*
 * Ported to run under FreeBSD by Julian Elischer (julian@tfs.com) Sept 1992
 */
/* This needs to be automatically generated.. */

#include <sys/param.h>
#include <sys/systm.h>          /* isn't it a joy */
#include <sys/kernel.h>         /* to have three of these */
#include <sys/conf.h>

#include "i386/isa/isa_device.h"
#include "eisaconf.h"
#include "bt.h"
#if NBT > 0
extern struct isa_driver btdriver;
int btintr();
#endif

struct eisa_dev eisa_dev[] = {
#if NBT > 0
  { "BUS",0x420,0,&bio_imask,{-1,&btdriver,0,0,-1,0,0,btintr,0,0,0,0,0}},
  { "BUS",0x470,0,&bio_imask,{-1,&btdriver,0,0,-1,0,0,btintr,0,0,0,0,0}},
#endif  /* NBT > 0 */
/* add your devices here */

  {0,0,0}
};

