/* "superscsi" pseudo device.
 * "superscsi" supports general SCSI utilities that can iterate
 * over all SCSI targets, including those without device entry
 * points.
 *
 * "superscsi" supports the SCIOCADDR ioctl to change the BUS, ID, LUN
 * of the target so that you can get to all devices.  The only thing
 * you can do to "superscsi" is open it, set the target, perform ioctl
 * calls, and close it.
 *
 * Keep "superscsi" protected: you can drive a truck through the
 * security hole if you don't.
 *
 *Begin copyright
 *
 * Copyright (C) 1993, 1994, 1995, HD Associates, Inc.
 * PO Box 276
 * Pepperell, MA 01463
 * 508 433 5266
 * dufault@hda.com
 *
 * This code is contributed to the University of California at Berkeley:
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *End copyright
 * $Id: ssc.c,v 1.3 1995/05/03 18:09:18 dufault Exp $
 */

#include <sys/types.h>
#include <sys/conf.h>
#include <scsi/scsiconf.h>
#include <sys/scsiio.h>

#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/buf.h>
#include <sys/systm.h>

static dev_t sscdev = NODEV;

int sscopen(dev_t dev, int flag, int type, struct proc *p)
{
	if (sscdev != NODEV)
		return suopen(sscdev, flag, type, p);
	return 0;
}

int sscclose(dev_t dev, int fflag, int type, struct proc *p)
{

	if (sscdev != NODEV)
		return suclose(sscdev, fflag, type, p);
	return 0;
}

int sscioctl(dev_t dev, int cmd, caddr_t data, int fflag, struct proc *p)
{
	if (cmd == SCIOCADDR)
	{
		struct scsi_addr *sca = (struct scsi_addr *) data;
		dev_t newdev = SCSI_MKFIXED(sca->scbus,sca->target,sca->lun);
		int ret;

		if (sscdev != NODEV)
		{
			suclose(sscdev, fflag, S_IFCHR, p);
			sscdev = NODEV;
		}

		if ( (ret = suopen(newdev, fflag, S_IFCHR, p)) )
			return ret;

		sscdev = newdev;

		return 0;
	}

	if (sscdev != NODEV)
		return suioctl(sscdev, cmd, data, fflag, p);

	return ENXIO;
}

/* I've elected not to support any of these other entries.  There
 * really is no good reason other than I'm not sure how you would use
 * them.
 */
void sscstrategy(struct buf *bp) { }
int sscread(dev_t dev, struct uio *uio, int ioflag) { return ENXIO; }
int sscwrite(dev_t dev, struct uio *uio, int ioflag) { return ENXIO; }
int sscselect(dev_t dev, int which, struct proc *p) { return ENXIO; }
