/*
 *
 * THIS SOFTWARE IS PROVIDED BY THE WRITERS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE WRITERS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * written by julian elischer (julian@tfs.com)
 *
 *	@(#)readMBR.c	8.5 (tfs) 1/21/94
 * $Id: readMBR.c,v 1.4 1994/11/14 13:22:41 bde Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/disklabel.h>

#define	b_cylinder	b_resid

/*
 * Attempt to read a machine-type dependent Device partitioning table
 * In this case a PC BIOS MBR.
 * Destroys the original disklabel if it finds an MBR, so you'd better
 * know what you're doing. It assumes that the label you've given it
 * Is the one that controls the device, so that it can fiddle with it
 * to make sure it's reading absolute sectors.
 * On exit:
 * Leaves the disklabel set up with the various partitions
 * in the last 4 entries,
 * the A partition pointing to the BSD part
 * the C partition set as the BSD partition, (read the disklabel from there) and
 * the D partition set as the whole disk for beating up
 * will also give you a copy of the machine dependent table if you ask..
 * returns 0 for success,
 * On failure, restores the disklabel and returns a messages pointer.
 */
char *
readMBRtolabel(dev, strat, lp, dp, cyl)
	dev_t dev;
	void (*strat)();
	register struct disklabel *lp;
	struct dos_partition *dp;
	int *cyl;
{
	register struct buf *bp;
	struct disklabel *dlp;
	struct disklabel labelsave;
	char *msg = NULL;
	int i;
	int pseudopart = 4;     /* we fill in pseudoparts from e through h*/
	int seenBSD = 0;

	/*
	 * Save a copy of the disklabel in case we return with an error
	 */
	bcopy(lp,&labelsave,sizeof(labelsave));

	/*
	 * Set the disklabel to some useable partitions in case it's rubbish
	 */
        if (lp->d_secperunit == 0)
                lp->d_secperunit = 0x1fffffff;
	lp->d_npartitions = 4;
	for (i=0; i<MAXPARTITIONS; i++) {
		lp->d_partitions[i].p_offset = 0;
		lp->d_partitions[i].p_size = 0;
	}
	lp->d_partitions[RAWPART].p_size = DOSBBSECTOR + 1; /* start low */
	strcpy(lp->d_packname,"MBR based label");  /* Watch the length ! */

	/*
	 * Get a buffer and get ready to read the MBR
	 */
	bp = geteblk((int)lp->d_secsize);
	/* read master boot record */
	bp->b_dev = makedev(major(dev), dkminor(dkunit(dev), RAWPART));
	bp->b_blkno = DOSBBSECTOR;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	bp->b_cylinder = DOSBBSECTOR / lp->d_secpercyl;
	(*strat)(bp);

	/* if successful, wander through dos partition table */
	if ( biowait(bp)) {
		msg = "dos partition I/O error";
		goto bad;
	} else {
		/*
		 * If there seems to be  BIOS bootblock and partition table
		 * in that block, then try interpret it, otherwise
		 * give up and use whatever we have synthesised so far
		 */
		if ((*(bp->b_un.b_addr + 510) != (char) 0x55)
		  ||(*(bp->b_un.b_addr + 511) != (char) 0xaa)) {
                        printf("disk doesn't have an MBR\n");
                        if(dp)
                                bzero(bp->b_un.b_addr + DOSPARTOFF,
                                    NDOSPART * sizeof(*dp));
                        goto hrumpf;
		}

		if(dp) { /* if they asked for a copy, give it to them */
			bcopy(bp->b_un.b_addr + DOSPARTOFF, dp,
				NDOSPART * sizeof(*dp));
		}
		dp = (struct dos_partition *)(bp->b_un.b_addr + DOSPARTOFF);
		/*
		 * We have a DOS MBR..
		 * We set up the last 4 partitions in the
		 * disklabel to reflect the DOS partitions
		 * In case we never find a disklabel, in which
		 * case this information will be all we have
		 * but it might be all we need to access a DOS
		 * partition.
		 */
		for (i = 0; i < NDOSPART; i++, dp++,pseudopart++) {

		        if (!dp->dp_size)
				continue;
		        /*
		         * Set this DOS part into the disklabel
		         */
		        lp->d_partitions[pseudopart].p_size =
				dp->dp_size;
		        lp->d_partitions[pseudopart].p_offset =
				dp->dp_start;

		        /*
		         * make sure the D part can hold it all
		         */
		        if((dp->dp_start + dp->dp_size)
		           > lp->d_partitions[3].p_size) {
				lp->d_partitions[3].p_size
				        = (dp->dp_start + dp->dp_size);
		        }

		        /*
		         * If we haven't seen a *BSD partition then
		         * check if this is a valid part..
		         * if it is it may be the best we are going to
		         * to see, so take note of it to deduce a
		         * geometry in case we never find a disklabel.
		         */
			switch(dp->dp_typ) {
			case DOSPTYP_386BSD:
				/*
				 * at a pinch we could throw
				 * a FFS on here
				 */
				lp->d_partitions[pseudopart].p_fstype
						= FS_BSDFFS;
				/*
				 * Only get a disklabel from the
				 * first one we see..
				 */
				if (seenBSD == 0) {
					/*
					 * If it IS our part, then we
					 * need sector address for
					 * SCSI/IDE, cylinder for
					 * ESDI/ST506/RLL
					 */
					seenBSD = 1;
					*cyl = DPCYL(dp->dp_scyl,
						dp->dp_ssect);

					/*
					 * Note which part we are in (?)
					 */
					lp->d_subtype &= ~3;
					lp->d_subtype |= i & 3;
					lp->d_subtype
						|= DSTYPE_INDOSPART;

					/*
					 * update disklabel with
					 * details for reading the REAL
					 * disklabel it it exists
					 */
					lp->d_partitions[OURPART].p_size =
						dp->dp_size;
					lp->d_partitions[OURPART].p_offset =
						dp->dp_start;
				}
				break;
			case 0xB7: /* BSDI (?)*//* doubtful */
				lp->d_partitions[pseudopart].p_fstype
						= FS_BSDFFS;
				break;
			case 1:
			case 4:
			case 6:
			case 0xF2:
				lp->d_partitions[pseudopart].p_fstype
						= FS_MSDOS;
				break;
			}

			/*
			 * Try deduce the geometry, working
			 * on the principle that  this
			 * partition PROBABLY ends on a
			 * cylinder boundary.
			 * This is really a kludge, but we are
			 * forced into it by the PC's design.
			 * If we've seen a 386bsd part,
			 * believe it and check no further.
			 */
			if (seenBSD) continue;
			lp->d_ntracks = dp->dp_ehd + 1;
			lp->d_nsectors = DPSECT(dp->dp_esect);
			lp->d_secpercyl = lp->d_ntracks *
				lp->d_nsectors;
		}
		lp->d_npartitions = 8;
        }
hrumpf:
	bp->b_flags = B_INVAL | B_AGE;
	brelse(bp);
	return 0;
bad:
	bcopy(&labelsave,lp,sizeof(labelsave));
	bp->b_flags = B_INVAL | B_AGE;
	brelse(bp);
	return msg;
}


