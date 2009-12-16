/*-
 * Copyright (c) 1997, Stefan Esser <se@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_bus.h"	/* XXX trim includes */
#include "opt_compat.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/linker.h>
#include <sys/fcntl.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/types.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>

#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <sys/pciio.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include "pcib_if.h"
#include "pci_if.h"

/*
 * This is the user interface to PCI configuration space.
 */

static d_open_t 	pci_open;
static d_close_t	pci_close;
static int	pci_conf_match(struct pci_match_conf *matches, int num_matches,
			       struct pci_conf *match_buf);
static d_ioctl_t	pci_ioctl;

struct cdevsw pcicdev = {
	.d_version =	D_VERSION,
	.d_flags =	D_NEEDGIANT,
	.d_open =	pci_open,
	.d_close =	pci_close,
	.d_ioctl =	pci_ioctl,
	.d_name =	"pci",
};
  
static int
pci_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	int error;

	if (oflags & FWRITE) {
		error = securelevel_gt(td->td_ucred, 0);
		if (error)
			return (error);
	}

	return (0);
}

static int
pci_close(struct cdev *dev, int flag, int devtype, struct thread *td)
{
	return 0;
}

/*
 * Match a single pci_conf structure against an array of pci_match_conf
 * structures.  The first argument, 'matches', is an array of num_matches
 * pci_match_conf structures.  match_buf is a pointer to the pci_conf
 * structure that will be compared to every entry in the matches array.
 * This function returns 1 on failure, 0 on success.
 */
static int
pci_conf_match(struct pci_match_conf *matches, int num_matches, 
	       struct pci_conf *match_buf)
{
	int i;

	if ((matches == NULL) || (match_buf == NULL) || (num_matches <= 0))
		return(1);

	for (i = 0; i < num_matches; i++) {
		/*
		 * I'm not sure why someone would do this...but...
		 */
		if (matches[i].flags == PCI_GETCONF_NO_MATCH)
			continue;

		/*
		 * Look at each of the match flags.  If it's set, do the
		 * comparison.  If the comparison fails, we don't have a
		 * match, go on to the next item if there is one.
		 */
		if (((matches[i].flags & PCI_GETCONF_MATCH_DOMAIN) != 0)
		 && (match_buf->pc_sel.pc_domain !=
		 matches[i].pc_sel.pc_domain))
			continue;

		if (((matches[i].flags & PCI_GETCONF_MATCH_BUS) != 0)
		 && (match_buf->pc_sel.pc_bus != matches[i].pc_sel.pc_bus))
			continue;

		if (((matches[i].flags & PCI_GETCONF_MATCH_DEV) != 0)
		 && (match_buf->pc_sel.pc_dev != matches[i].pc_sel.pc_dev))
			continue;

		if (((matches[i].flags & PCI_GETCONF_MATCH_FUNC) != 0)
		 && (match_buf->pc_sel.pc_func != matches[i].pc_sel.pc_func))
			continue;

		if (((matches[i].flags & PCI_GETCONF_MATCH_VENDOR) != 0) 
		 && (match_buf->pc_vendor != matches[i].pc_vendor))
			continue;

		if (((matches[i].flags & PCI_GETCONF_MATCH_DEVICE) != 0)
		 && (match_buf->pc_device != matches[i].pc_device))
			continue;

		if (((matches[i].flags & PCI_GETCONF_MATCH_CLASS) != 0)
		 && (match_buf->pc_class != matches[i].pc_class))
			continue;

		if (((matches[i].flags & PCI_GETCONF_MATCH_UNIT) != 0)
		 && (match_buf->pd_unit != matches[i].pd_unit))
			continue;

		if (((matches[i].flags & PCI_GETCONF_MATCH_NAME) != 0)
		 && (strncmp(matches[i].pd_name, match_buf->pd_name,
			     sizeof(match_buf->pd_name)) != 0))
			continue;

		return(0);
	}

	return(1);
}

#if defined(COMPAT_FREEBSD4) || defined(COMPAT_FREEBSD5) || \
    defined(COMPAT_FREEBSD6)
#define PRE7_COMPAT

typedef enum {
	PCI_GETCONF_NO_MATCH_OLD	= 0x00,
	PCI_GETCONF_MATCH_BUS_OLD	= 0x01,
	PCI_GETCONF_MATCH_DEV_OLD	= 0x02,
	PCI_GETCONF_MATCH_FUNC_OLD	= 0x04,
	PCI_GETCONF_MATCH_NAME_OLD	= 0x08,
	PCI_GETCONF_MATCH_UNIT_OLD	= 0x10,
	PCI_GETCONF_MATCH_VENDOR_OLD	= 0x20,
	PCI_GETCONF_MATCH_DEVICE_OLD	= 0x40,
	PCI_GETCONF_MATCH_CLASS_OLD	= 0x80
} pci_getconf_flags_old;

struct pcisel_old {
	u_int8_t	pc_bus;		/* bus number */
	u_int8_t	pc_dev;		/* device on this bus */
	u_int8_t	pc_func;	/* function on this device */
};

struct pci_conf_old {
	struct pcisel_old pc_sel;	/* bus+slot+function */
	u_int8_t	pc_hdr;		/* PCI header type */
	u_int16_t	pc_subvendor;	/* card vendor ID */
	u_int16_t	pc_subdevice;	/* card device ID, assigned by
					   card vendor */
	u_int16_t	pc_vendor;	/* chip vendor ID */
	u_int16_t	pc_device;	/* chip device ID, assigned by
					   chip vendor */
	u_int8_t	pc_class;	/* chip PCI class */
	u_int8_t	pc_subclass;	/* chip PCI subclass */
	u_int8_t	pc_progif;	/* chip PCI programming interface */
	u_int8_t	pc_revid;	/* chip revision ID */
	char		pd_name[PCI_MAXNAMELEN + 1];  /* device name */
	u_long		pd_unit;	/* device unit number */
};

struct pci_match_conf_old {
	struct pcisel_old	pc_sel;		/* bus+slot+function */
	char			pd_name[PCI_MAXNAMELEN + 1];  /* device name */
	u_long			pd_unit;	/* Unit number */
	u_int16_t		pc_vendor;	/* PCI Vendor ID */
	u_int16_t		pc_device;	/* PCI Device ID */
	u_int8_t		pc_class;	/* PCI class */
	pci_getconf_flags_old	flags;		/* Matching expression */
};

struct pci_io_old {
	struct pcisel_old pi_sel;	/* device to operate on */
	int		pi_reg;		/* configuration register to examine */
	int		pi_width;	/* width (in bytes) of read or write */
	u_int32_t	pi_data;	/* data to write or result of read */
};

#define	PCIOCGETCONF_OLD	_IOWR('p', 1, struct pci_conf_io)
#define	PCIOCREAD_OLD		_IOWR('p', 2, struct pci_io_old)
#define	PCIOCWRITE_OLD		_IOWR('p', 3, struct pci_io_old)

static int	pci_conf_match_old(struct pci_match_conf_old *matches,
		    int num_matches, struct pci_conf *match_buf);

static int
pci_conf_match_old(struct pci_match_conf_old *matches, int num_matches,
    struct pci_conf *match_buf)
{
	int i;

	if ((matches == NULL) || (match_buf == NULL) || (num_matches <= 0))
		return(1);

	for (i = 0; i < num_matches; i++) {
		if (match_buf->pc_sel.pc_domain != 0)
			continue;

		/*
		 * I'm not sure why someone would do this...but...
		 */
		if (matches[i].flags == PCI_GETCONF_NO_MATCH_OLD)
			continue;

		/*
		 * Look at each of the match flags.  If it's set, do the
		 * comparison.  If the comparison fails, we don't have a
		 * match, go on to the next item if there is one.
		 */
		if (((matches[i].flags & PCI_GETCONF_MATCH_BUS_OLD) != 0)
		 && (match_buf->pc_sel.pc_bus != matches[i].pc_sel.pc_bus))
			continue;

		if (((matches[i].flags & PCI_GETCONF_MATCH_DEV_OLD) != 0)
		 && (match_buf->pc_sel.pc_dev != matches[i].pc_sel.pc_dev))
			continue;

		if (((matches[i].flags & PCI_GETCONF_MATCH_FUNC_OLD) != 0)
		 && (match_buf->pc_sel.pc_func != matches[i].pc_sel.pc_func))
			continue;

		if (((matches[i].flags & PCI_GETCONF_MATCH_VENDOR_OLD) != 0)
		 && (match_buf->pc_vendor != matches[i].pc_vendor))
			continue;

		if (((matches[i].flags & PCI_GETCONF_MATCH_DEVICE_OLD) != 0)
		 && (match_buf->pc_device != matches[i].pc_device))
			continue;

		if (((matches[i].flags & PCI_GETCONF_MATCH_CLASS_OLD) != 0)
		 && (match_buf->pc_class != matches[i].pc_class))
			continue;

		if (((matches[i].flags & PCI_GETCONF_MATCH_UNIT_OLD) != 0)
		 && (match_buf->pd_unit != matches[i].pd_unit))
			continue;

		if (((matches[i].flags & PCI_GETCONF_MATCH_NAME_OLD) != 0)
		 && (strncmp(matches[i].pd_name, match_buf->pd_name,
			     sizeof(match_buf->pd_name)) != 0))
			continue;

		return(0);
	}

	return(1);
}

#endif

static int
pci_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int flag, struct thread *td)
{
	device_t pcidev, brdev;
	void *confdata;
	const char *name;
	struct devlist *devlist_head;
	struct pci_conf_io *cio;
	struct pci_devinfo *dinfo;
	struct pci_io *io;
	struct pci_bar_io *bio;
	struct pci_match_conf *pattern_buf;
	struct resource_list_entry *rle;
	uint32_t value;
	size_t confsz, iolen, pbufsz;
	int error, ionum, i, num_patterns;
#ifdef PRE7_COMPAT
	struct pci_conf_old conf_old;
	struct pci_io iodata;
	struct pci_io_old *io_old;
	struct pci_match_conf_old *pattern_buf_old;

	io_old = NULL;
	pattern_buf_old = NULL;

	if (!(flag & FWRITE) && cmd != PCIOCGETBAR &&
	    cmd != PCIOCGETCONF && cmd != PCIOCGETCONF_OLD)
		return EPERM;
#else
	if (!(flag & FWRITE) && cmd != PCIOCGETBAR && cmd != PCIOCGETCONF)
		return EPERM;
#endif

	switch(cmd) {
#ifdef PRE7_COMPAT
	case PCIOCGETCONF_OLD:
		/* FALLTHROUGH */
#endif
	case PCIOCGETCONF:
		cio = (struct pci_conf_io *)data;

		pattern_buf = NULL;
		num_patterns = 0;
		dinfo = NULL;

		cio->num_matches = 0;

		/*
		 * If the user specified an offset into the device list,
		 * but the list has changed since they last called this
		 * ioctl, tell them that the list has changed.  They will
		 * have to get the list from the beginning.
		 */
		if ((cio->offset != 0)
		 && (cio->generation != pci_generation)){
			cio->status = PCI_GETCONF_LIST_CHANGED;
			error = 0;
			break;
		}

		/*
		 * Check to see whether the user has asked for an offset
		 * past the end of our list.
		 */
		if (cio->offset >= pci_numdevs) {
			cio->status = PCI_GETCONF_LAST_DEVICE;
			error = 0;
			break;
		}

		/* get the head of the device queue */
		devlist_head = &pci_devq;

		/*
		 * Determine how much room we have for pci_conf structures.
		 * Round the user's buffer size down to the nearest
		 * multiple of sizeof(struct pci_conf) in case the user
		 * didn't specify a multiple of that size.
		 */
#ifdef PRE7_COMPAT
		if (cmd == PCIOCGETCONF_OLD)
			confsz = sizeof(struct pci_conf_old);
		else
#endif
			confsz = sizeof(struct pci_conf);
		iolen = min(cio->match_buf_len - (cio->match_buf_len % confsz),
		    pci_numdevs * confsz);

		/*
		 * Since we know that iolen is a multiple of the size of
		 * the pciconf union, it's okay to do this.
		 */
		ionum = iolen / confsz;

		/*
		 * If this test is true, the user wants the pci_conf
		 * structures returned to match the supplied entries.
		 */
		if ((cio->num_patterns > 0) && (cio->num_patterns < pci_numdevs)
		 && (cio->pat_buf_len > 0)) {
			/*
			 * pat_buf_len needs to be:
			 * num_patterns * sizeof(struct pci_match_conf)
			 * While it is certainly possible the user just
			 * allocated a large buffer, but set the number of
			 * matches correctly, it is far more likely that
			 * their kernel doesn't match the userland utility
			 * they're using.  It's also possible that the user
			 * forgot to initialize some variables.  Yes, this
			 * may be overly picky, but I hazard to guess that
			 * it's far more likely to just catch folks that
			 * updated their kernel but not their userland.
			 */
#ifdef PRE7_COMPAT
			if (cmd == PCIOCGETCONF_OLD)
				pbufsz = sizeof(struct pci_match_conf_old);
			else
#endif
				pbufsz = sizeof(struct pci_match_conf);
			if (cio->num_patterns * pbufsz != cio->pat_buf_len) {
				/* The user made a mistake, return an error. */
				cio->status = PCI_GETCONF_ERROR;
				error = EINVAL;
				break;
			}

			/*
			 * Allocate a buffer to hold the patterns.
			 */
#ifdef PRE7_COMPAT
			if (cmd == PCIOCGETCONF_OLD) {
				pattern_buf_old = malloc(cio->pat_buf_len,
				    M_TEMP, M_WAITOK);
				error = copyin(cio->patterns,
				    pattern_buf_old, cio->pat_buf_len);
			} else
#endif
			{
				pattern_buf = malloc(cio->pat_buf_len, M_TEMP,
				    M_WAITOK);
				error = copyin(cio->patterns, pattern_buf,
				    cio->pat_buf_len);
			}
			if (error != 0) {
				error = EINVAL;
				goto getconfexit;
			}
			num_patterns = cio->num_patterns;
		} else if ((cio->num_patterns > 0)
			|| (cio->pat_buf_len > 0)) {
			/*
			 * The user made a mistake, spit out an error.
			 */
			cio->status = PCI_GETCONF_ERROR;
			error = EINVAL;
			break;
		}

		/*
		 * Go through the list of devices and copy out the devices
		 * that match the user's criteria.
		 */
		for (cio->num_matches = 0, error = 0, i = 0,
		     dinfo = STAILQ_FIRST(devlist_head);
		     (dinfo != NULL) && (cio->num_matches < ionum)
		     && (error == 0) && (i < pci_numdevs) && (dinfo != NULL);
		     dinfo = STAILQ_NEXT(dinfo, pci_links), i++) {

			if (i < cio->offset)
				continue;

			/* Populate pd_name and pd_unit */
			name = NULL;
			if (dinfo->cfg.dev)
				name = device_get_name(dinfo->cfg.dev);
			if (name) {
				strncpy(dinfo->conf.pd_name, name,
					sizeof(dinfo->conf.pd_name));
				dinfo->conf.pd_name[PCI_MAXNAMELEN] = 0;
				dinfo->conf.pd_unit =
					device_get_unit(dinfo->cfg.dev);
			} else {
				dinfo->conf.pd_name[0] = '\0';
				dinfo->conf.pd_unit = 0;
			}

#ifdef PRE7_COMPAT
			if ((cmd == PCIOCGETCONF_OLD &&
			    (pattern_buf_old == NULL ||
			    pci_conf_match_old(pattern_buf_old, num_patterns,
			    &dinfo->conf) == 0)) ||
			    (cmd == PCIOCGETCONF &&
			    (pattern_buf == NULL ||
			    pci_conf_match(pattern_buf, num_patterns,
			    &dinfo->conf) == 0))) {
#else
			if (pattern_buf == NULL ||
			    pci_conf_match(pattern_buf, num_patterns,
			    &dinfo->conf) == 0) {
#endif
				/*
				 * If we've filled up the user's buffer,
				 * break out at this point.  Since we've
				 * got a match here, we'll pick right back
				 * up at the matching entry.  We can also
				 * tell the user that there are more matches
				 * left.
				 */
				if (cio->num_matches >= ionum)
					break;

#ifdef PRE7_COMPAT
				if (cmd == PCIOCGETCONF_OLD) {
					conf_old.pc_sel.pc_bus =
					    dinfo->conf.pc_sel.pc_bus;
					conf_old.pc_sel.pc_dev =
					    dinfo->conf.pc_sel.pc_dev;
					conf_old.pc_sel.pc_func =
					    dinfo->conf.pc_sel.pc_func;
					conf_old.pc_hdr = dinfo->conf.pc_hdr;
					conf_old.pc_subvendor =
					    dinfo->conf.pc_subvendor;
					conf_old.pc_subdevice =
					    dinfo->conf.pc_subdevice;
					conf_old.pc_vendor =
					    dinfo->conf.pc_vendor;
					conf_old.pc_device =
					    dinfo->conf.pc_device;
					conf_old.pc_class =
					    dinfo->conf.pc_class;
					conf_old.pc_subclass =
					    dinfo->conf.pc_subclass;
					conf_old.pc_progif =
					    dinfo->conf.pc_progif;
					conf_old.pc_revid =
					    dinfo->conf.pc_revid;
					strncpy(conf_old.pd_name,
					    dinfo->conf.pd_name,
					    sizeof(conf_old.pd_name));
					conf_old.pd_name[PCI_MAXNAMELEN] = 0;
					conf_old.pd_unit =
					    dinfo->conf.pd_unit;
					confdata = &conf_old;
				} else
#endif
					confdata = &dinfo->conf;
				/* Only if we can copy it out do we count it. */
				if (!(error = copyout(confdata,
				    (caddr_t)cio->matches +
				    confsz * cio->num_matches, confsz)))
					cio->num_matches++;
			}
		}

		/*
		 * Set the pointer into the list, so if the user is getting
		 * n records at a time, where n < pci_numdevs,
		 */
		cio->offset = i;

		/*
		 * Set the generation, the user will need this if they make
		 * another ioctl call with offset != 0.
		 */
		cio->generation = pci_generation;

		/*
		 * If this is the last device, inform the user so he won't
		 * bother asking for more devices.  If dinfo isn't NULL, we
		 * know that there are more matches in the list because of
		 * the way the traversal is done.
		 */
		if (dinfo == NULL)
			cio->status = PCI_GETCONF_LAST_DEVICE;
		else
			cio->status = PCI_GETCONF_MORE_DEVS;

getconfexit:
		if (pattern_buf != NULL)
			free(pattern_buf, M_TEMP);
#ifdef PRE7_COMPAT
		if (pattern_buf_old != NULL)
			free(pattern_buf_old, M_TEMP);
#endif

		break;

#ifdef PRE7_COMPAT
	case PCIOCREAD_OLD:
	case PCIOCWRITE_OLD:
		io_old = (struct pci_io_old *)data;
		iodata.pi_sel.pc_domain = 0;
		iodata.pi_sel.pc_bus = io_old->pi_sel.pc_bus;
		iodata.pi_sel.pc_dev = io_old->pi_sel.pc_dev;
		iodata.pi_sel.pc_func = io_old->pi_sel.pc_func;
		iodata.pi_reg = io_old->pi_reg;
		iodata.pi_width = io_old->pi_width;
		iodata.pi_data = io_old->pi_data;
		data = (caddr_t)&iodata;
		/* FALLTHROUGH */
#endif
	case PCIOCREAD:
	case PCIOCWRITE:
		io = (struct pci_io *)data;
		switch(io->pi_width) {
		case 4:
		case 2:
		case 1:
			/* Make sure register is not negative and aligned. */
			if (io->pi_reg < 0 ||
			    io->pi_reg & (io->pi_width - 1)) {
				error = EINVAL;
				break;
			}
			/*
			 * Assume that the user-level bus number is
			 * in fact the physical PCI bus number.
			 * Look up the grandparent, i.e. the bridge device,
			 * so that we can issue configuration space cycles.
			 */
			pcidev = pci_find_dbsf(io->pi_sel.pc_domain,
			    io->pi_sel.pc_bus, io->pi_sel.pc_dev,
			    io->pi_sel.pc_func);
			if (pcidev) {
				brdev = device_get_parent(
				    device_get_parent(pcidev));

#ifdef PRE7_COMPAT
				if (cmd == PCIOCWRITE || cmd == PCIOCWRITE_OLD)
#else
				if (cmd == PCIOCWRITE)
#endif
					PCIB_WRITE_CONFIG(brdev,
							  io->pi_sel.pc_bus,
							  io->pi_sel.pc_dev,
							  io->pi_sel.pc_func,
							  io->pi_reg,
							  io->pi_data,
							  io->pi_width);
#ifdef PRE7_COMPAT
				else if (cmd == PCIOCREAD_OLD)
					io_old->pi_data =
						PCIB_READ_CONFIG(brdev,
							  io->pi_sel.pc_bus,
							  io->pi_sel.pc_dev,
							  io->pi_sel.pc_func,
							  io->pi_reg,
							  io->pi_width);
#endif
				else
					io->pi_data =
						PCIB_READ_CONFIG(brdev,
							  io->pi_sel.pc_bus,
							  io->pi_sel.pc_dev,
							  io->pi_sel.pc_func,
							  io->pi_reg,
							  io->pi_width);
				error = 0;
			} else {
#ifdef COMPAT_FREEBSD4
				if (cmd == PCIOCREAD_OLD) {
					io_old->pi_data = -1;
					error = 0;
				} else
#endif
					error = ENODEV;
			}
			break;
		default:
			error = EINVAL;
			break;
		}
		break;

	case PCIOCGETBAR:
		bio = (struct pci_bar_io *)data;

		/*
		 * Assume that the user-level bus number is
		 * in fact the physical PCI bus number.
		 */
		pcidev = pci_find_dbsf(bio->pbi_sel.pc_domain,
		    bio->pbi_sel.pc_bus, bio->pbi_sel.pc_dev,
		    bio->pbi_sel.pc_func);
		if (pcidev == NULL) {
			error = ENODEV;
			break;
		}
		dinfo = device_get_ivars(pcidev);
		
		/*
		 * Look for a resource list entry matching the requested BAR.
		 *
		 * XXX: This will not find BARs that are not initialized, but
		 * maybe that is ok?
		 */
		rle = resource_list_find(&dinfo->resources, SYS_RES_MEMORY,
		    bio->pbi_reg);
		if (rle == NULL)
			rle = resource_list_find(&dinfo->resources,
			    SYS_RES_IOPORT, bio->pbi_reg);
		if (rle == NULL || rle->res == NULL) {
			error = EINVAL;
			break;
		}

		/*
		 * Ok, we have a resource for this BAR.  Read the lower
		 * 32 bits to get any flags.
		 */
		value = pci_read_config(pcidev, bio->pbi_reg, 4);
		if (PCI_BAR_MEM(value)) {
			if (rle->type != SYS_RES_MEMORY) {
				error = EINVAL;
				break;
			}
			value &= ~PCIM_BAR_MEM_BASE;
		} else {
			if (rle->type != SYS_RES_IOPORT) {
				error = EINVAL;
				break;
			}
			value &= ~PCIM_BAR_IO_BASE;
		}
		bio->pbi_base = rman_get_start(rle->res) | value;
		bio->pbi_length = rman_get_size(rle->res);

		/*
		 * Check the command register to determine if this BAR
		 * is enabled.
		 */
		value = pci_read_config(pcidev, PCIR_COMMAND, 2);
		if (rle->type == SYS_RES_MEMORY)
			bio->pbi_enabled = (value & PCIM_CMD_MEMEN) != 0;
		else
			bio->pbi_enabled = (value & PCIM_CMD_PORTEN) != 0;
		error = 0;
		break;
	default:
		error = ENOTTY;
		break;
	}

	return (error);
}
