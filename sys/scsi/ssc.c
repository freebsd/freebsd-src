/* "superscsi" pseudo device.  This requires options SCSISUPER.
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
		dev_t newdev =
		 SCSI_MKSUPER(SCSI_MKDEV(sca->scbus,sca->lun,sca->target));
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
int sscstrategy(struct buf *bp) { return ENXIO; }
int sscdump(dev_t dev) { return ENXIO; }
int sscpsize(dev_t dev) { return ENXIO; }
int sscread(dev_t dev, struct uio *uio, int ioflag) { return ENXIO; }
int sscwrite(dev_t dev, struct uio *uio, int ioflag) { return ENXIO; }
int sscselect(dev_t dev, int which, struct proc *p) { return ENXIO; }
