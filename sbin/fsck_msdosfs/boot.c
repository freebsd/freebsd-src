/*
 * Copyright (C) 1995, 1997 Wolfgang Solfrank
 * Copyright (c) 1995 Martin Husemann
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Martin Husemann
 *	and Wolfgang Solfrank.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: boot.c,v 1.5 1997/10/17 11:19:23 ws Exp $");
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>

#include "ext.h"
#include "fsutil.h"

int
readboot(dosfs, boot)
	int dosfs;
	struct bootblock *boot;
{
	u_char block[DOSBOOTBLOCKSIZE];
	u_char fsinfo[2 * DOSBOOTBLOCKSIZE];
	u_char backup[DOSBOOTBLOCKSIZE];
	int ret = FSOK;
	
	if (read(dosfs, block, sizeof block) < sizeof block) {
		perror("could not read boot block");
		return FSFATAL;
	}

	if (block[510] != 0x55 || block[511] != 0xaa) {
		pfatal("Invalid signature in boot block: %02x%02x", block[511], block[510]);
		return FSFATAL;
	}

	memset(boot, 0, sizeof *boot);
	boot->ValidFat = -1;

	/* decode bios parameter block */
	boot->BytesPerSec = block[11] + (block[12] << 8);
	boot->SecPerClust = block[13];
	boot->ResSectors = block[14] + (block[15] << 8);
	boot->FATs = block[16];
	boot->RootDirEnts = block[17] + (block[18] << 8);
	boot->Sectors = block[19] + (block[20] << 8);
	boot->Media = block[21];
	boot->FATsmall = block[22] + (block[23] << 8);
	boot->SecPerTrack = block[24] + (block[25] << 8);
	boot->Heads = block[26] + (block[27] << 8);
	boot->HiddenSecs = block[28] + (block[29] << 8) + (block[30] << 16) + (block[31] << 24);
	boot->HugeSectors = block[32] + (block[33] << 8) + (block[34] << 16) + (block[35] << 24);

	boot->FATsecs = boot->FATsmall;

	if (!boot->RootDirEnts)
		boot->flags |= FAT32;
	if (boot->flags & FAT32) {
		boot->FATsecs = block[36] + (block[37] << 8)
				+ (block[38] << 16) + (block[39] << 24);
		if (block[40] & 0x80)
			boot->ValidFat = block[40] & 0x0f;

		/* check version number: */
		if (block[42] || block[43]) {
			/* Correct?				XXX */
			pfatal("Unknown filesystem version: %x.%x",
			       block[43], block[42]);
			return FSFATAL;
		}
		boot->RootCl = block[44] + (block[45] << 8)
			       + (block[46] << 16) + (block[47] << 24);
		boot->FSInfo = block[48] + (block[49] << 8);
		boot->Backup = block[50] + (block[51] << 8);

		if (lseek(dosfs, boot->FSInfo * boot->BytesPerSec, SEEK_SET)
		    != boot->FSInfo * boot->BytesPerSec
		    || read(dosfs, fsinfo, sizeof fsinfo)
		    != sizeof fsinfo) {
			perror("could not read fsinfo block");
			return FSFATAL;
		}
		if (memcmp(fsinfo, "RRaA", 4)
		    || memcmp(fsinfo + 0x1e4, "rrAa", 4)
		    || fsinfo[0x1fc]
		    || fsinfo[0x1fd]
		    || fsinfo[0x1fe] != 0x55
		    || fsinfo[0x1ff] != 0xaa
		    || fsinfo[0x3fc]
		    || fsinfo[0x3fd]
		    || fsinfo[0x3fe] != 0x55
		    || fsinfo[0x3ff] != 0xaa) {
			pwarn("Invalid signature in fsinfo block");
			if (ask(0, "fix")) {
				memcpy(fsinfo, "RRaA", 4);
				memcpy(fsinfo + 0x1e4, "rrAa", 4);
				fsinfo[0x1fc] = fsinfo[0x1fd] = 0;
				fsinfo[0x1fe] = 0x55;
				fsinfo[0x1ff] = 0xaa;
				fsinfo[0x3fc] = fsinfo[0x3fd] = 0;
				fsinfo[0x3fe] = 0x55;
				fsinfo[0x3ff] = 0xaa;
				if (lseek(dosfs, boot->FSInfo * boot->BytesPerSec, SEEK_SET)
				    != boot->FSInfo * boot->BytesPerSec
				    || write(dosfs, fsinfo, sizeof fsinfo)
				    != sizeof fsinfo) {
					perror("Unable to write FSInfo");
					return FSFATAL;
				}
				ret = FSBOOTMOD;
			} else
				boot->FSInfo = 0;
		}
		if (boot->FSInfo) {
			boot->FSFree = fsinfo[0x1e8] + (fsinfo[0x1e9] << 8)
				       + (fsinfo[0x1ea] << 16)
				       + (fsinfo[0x1eb] << 24);
			boot->FSNext = fsinfo[0x1ec] + (fsinfo[0x1ed] << 8)
				       + (fsinfo[0x1ee] << 16)
				       + (fsinfo[0x1ef] << 24);
		}

		if (lseek(dosfs, boot->Backup * boot->BytesPerSec, SEEK_SET)
		    != boot->Backup * boot->BytesPerSec
		    || read(dosfs, backup, sizeof backup) != sizeof  backup) {
			perror("could not read backup bootblock");
			return FSFATAL;
		}
		if (memcmp(block, backup, DOSBOOTBLOCKSIZE)) {
			/* Correct?					XXX */
			pfatal("backup doesn't compare to primary bootblock");
			return FSFATAL;
		}
		/* Check backup FSInfo?					XXX */
	}

	boot->ClusterOffset = (boot->RootDirEnts * 32 + boot->BytesPerSec - 1)
	    / boot->BytesPerSec
	    + boot->ResSectors
	    + boot->FATs * boot->FATsecs
	    - CLUST_FIRST * boot->SecPerClust;

	if (boot->BytesPerSec % DOSBOOTBLOCKSIZE != 0) {
		pfatal("Invalid sector size: %u", boot->BytesPerSec);
		return FSFATAL;
	}
	if (boot->SecPerClust == 0) {
		pfatal("Invalid cluster size: %u", boot->SecPerClust);
		return FSFATAL;
	}
	if (boot->Sectors) {
		boot->HugeSectors = 0;
		boot->NumSectors = boot->Sectors;
	} else
		boot->NumSectors = boot->HugeSectors;
	boot->NumClusters = (boot->NumSectors - boot->ClusterOffset) / boot->SecPerClust;

	if (boot->flags&FAT32)
		boot->ClustMask = CLUST32_MASK;
	else if (boot->NumClusters < (CLUST_RSRVD&CLUST12_MASK))
		boot->ClustMask = CLUST12_MASK;
	else if (boot->NumClusters < (CLUST_RSRVD&CLUST16_MASK))
		boot->ClustMask = CLUST16_MASK;
	else {
		pfatal("Filesystem too big (%u clusters) for non-FAT32 partition",
		       boot->NumClusters);
		return FSFATAL;
	}

	switch (boot->ClustMask) {
	case CLUST32_MASK:
		boot->NumFatEntries = (boot->FATsecs * boot->BytesPerSec) / 4;
		break;
	case CLUST16_MASK:
		boot->NumFatEntries = (boot->FATsecs * boot->BytesPerSec) / 2;
		break;
	default:
		boot->NumFatEntries = (boot->FATsecs * boot->BytesPerSec * 2) / 3;
		break;
	}

	if (boot->NumFatEntries < boot->NumClusters) {
		pfatal("FAT size too small, %u entries won't fit into %u sectors\n",
		       boot->NumClusters, boot->FATsecs);
		return FSFATAL;
	}
	boot->ClusterSize = boot->BytesPerSec * boot->SecPerClust;

	boot->NumFiles = 1;
	boot->NumFree = 0;

	return ret;
}

int
writefsinfo(dosfs, boot)
	int dosfs;
	struct bootblock *boot;
{
	u_char fsinfo[2 * DOSBOOTBLOCKSIZE];

	if (lseek(dosfs, boot->FSInfo * boot->BytesPerSec, SEEK_SET)
	    != boot->FSInfo * boot->BytesPerSec
	    || read(dosfs, fsinfo, sizeof fsinfo) != sizeof fsinfo) {
		perror("could not read fsinfo block");
		return FSFATAL;
	}
	fsinfo[0x1e8] = (u_char)boot->FSFree;
	fsinfo[0x1e9] = (u_char)(boot->FSFree >> 8);
	fsinfo[0x1ea] = (u_char)(boot->FSFree >> 16);
	fsinfo[0x1eb] = (u_char)(boot->FSFree >> 24);
	fsinfo[0x1ec] = (u_char)boot->FSNext;
	fsinfo[0x1ed] = (u_char)(boot->FSNext >> 8);
	fsinfo[0x1ee] = (u_char)(boot->FSNext >> 16);
	fsinfo[0x1ef] = (u_char)(boot->FSNext >> 24);
	if (lseek(dosfs, boot->FSInfo * boot->BytesPerSec, SEEK_SET)
	    != boot->FSInfo * boot->BytesPerSec
	    || write(dosfs, fsinfo, sizeof fsinfo)
	    != sizeof fsinfo) {
		perror("Unable to write FSInfo");
		return FSFATAL;
	}
	/*
	 * Technically, we should return FSBOOTMOD here.
	 *
	 * However, since Win95 OSR2 (the first M$ OS that has
	 * support for FAT32) doesn't maintain the FSINFO block
	 * correctly, it has to be fixed pretty often.
	 *
	 * Therefor, we handle the FSINFO block only informally,
	 * fixing it if neccessary, but otherwise ignoring the
	 * fact that it was incorrect.
	 */
	return 0;
}
