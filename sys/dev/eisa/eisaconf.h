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
 * $Id: eisaconf.h,v 1.1 1995/03/13 09:10:17 root Exp root $
 */

/*
 * Ported to run under FreeBSD by Julian Elischer (julian@tfs.com) Sept 1992
 */

#define EISA_SLOTS 10   /* PCI clashes with higher ones.. fix later */  
struct eisa_dev {
  char productID[4];
  unsigned short productType;
  unsigned char productRevision;
  unsigned int *imask;
  struct isa_device isa_dev;
};

