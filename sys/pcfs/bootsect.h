/*
 *  Written by Paul Popelka (paulp@uts.amdahl.com)
 *
 *  You can do anything you want with this software,
 *    just don't say you wrote it,
 *    and don't remove this notice.
 *
 *  This software is provided "as is".
 *
 *  The author supplies this software to be publicly
 *  redistributed on the understanding that the author
 *  is not responsible for the correct functioning of
 *  this software in any circumstances and is not liable
 *  for any damages caused by this software.
 *
 *  October 1992
 *
 *	$Id: bootsect.h,v 1.3 1993/11/07 17:51:10 wollman Exp $
 */

#ifndef _PCFS_BOOTSECT_H_
#define _PCFS_BOOTSECT_H_ 1

/*
 *  Format of a boot sector.  This is the first sector
 *  on a DOS floppy disk or the fist sector of a partition
 *  on a hard disk.  But, it is not the first sector of
 *  a partitioned hard disk.
 */
struct bootsector33 {
	char bsJump[3];		/* jump instruction E9xxxx or EBxx90	*/
	char bsOemName[8];	/* OEM name and version			*/
	char bsBPB[19];		/* BIOS parameter block			*/
	char bsDriveNumber;	/* drive number (0x80)			*/
	char bsBootCode[474];	/* pad so structure is 512 bytes long	*/
	u_short bsBootSectSig;
#define	BOOTSIG	0xaa55
};

struct bootsector50 {
	char bsJump[3];		/* jump instruction E9xxxx or EBxx90	*/
	char bsOemName[8];	/* OEM name and version			*/
	char bsBPB[25];		/* BIOS parameter block			*/
	char bsDriveNumber;	/* drive number (0x80)			*/
	char bsReserved1;	/* reserved				*/
	char bsBootSignature;	/* extended boot signature (0x29)	*/
#define	EXBOOTSIG	0x29
	char bsVolumeID[4];	/* volume ID number			*/
	char bsVolumeLabel[11];	/* volume label				*/
	char bsFileSysType[8];	/* file system type (FAT12 or FAT16)	*/
	char bsBootCode[448];	/* pad so structure is 512 bytes long	*/
	u_short bsBootSectSig;
#define	BOOTSIG	0xaa55
};

union bootsector {
	struct bootsector33 bs33;
	struct bootsector50 bs50;
};

/*
 *  Shorthand for fields in the bpb.
 */
#define	bsBytesPerSec	bsBPB.bpbBytesPerSec
#define	bsSectPerClust	bsBPB.bpbSectPerClust
#define	bsResSectors	bsBPB.bpbResSectors
#define	bsFATS		bsBPB.bpbFATS
#define	bsRootDirEnts	bsBPB.bpbRootDirEnts
#define	bsSectors	bsBPB.bpbSectors
#define	bsMedia		bsBPB.bpbMedia
#define	bsFATsecs	bsBPB.bpbFATsecs
#define	bsSectPerTrack	bsBPB.bpbSectPerTrack
#define	bsHeads		bsBPB.bpbHeads
#define	bsHiddenSecs	bsBPB.bpbHiddenSecs
#define	bsHugeSectors	bsBPB.bpbHugeSectors
#endif /* _PCFS_BOOTSECT_H_ */
