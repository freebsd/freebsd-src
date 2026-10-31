/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Userland control interface for the DPAA2 driver.
 *
 * Lets a userland tool (dpaa2ctl) create/destroy/list DPAA2 network
 * interfaces at run-time -- the FreeBSD equivalent of NXP's
 * "restool ls-addni / ls-listni". Each control device (/dev/dpaa2rcN)
 * corresponds to one DPAA2 resource container (dpaa2_rcN).
 */
#ifndef _DEV_DPAA2_DPAA2_IOCTL_H_
#define _DEV_DPAA2_DPAA2_IOCTL_H_

#include <sys/ioccom.h>

#define DPAA2_NI_NAME_LEN	16
#define DPAA2_NI_LIST_MAX	32

/* Description of a single DPAA2 network interface. */
struct dpaa2_ni_desc {
	uint32_t	ni_id;		/* DPNI object id */
	uint32_t	dpmac_id;	/* connected DPMAC id, or ~0u if none */
	char		name[DPAA2_NI_NAME_LEN]; /* e.g. "dpni1" */
};

/* DPAA2IOC_ADDNI argument. */
struct dpaa2_addni_args {
	uint32_t	dpmac_id;	/* in:  DPMAC to wire the new DPNI to */
	uint32_t	ni_id;		/* out: id of the created DPNI */
};

/* DPAA2IOC_LISTNI argument. */
struct dpaa2_listni_args {
	uint32_t		count;	/* out: number of valid entries */
	struct dpaa2_ni_desc	ni[DPAA2_NI_LIST_MAX];
};

#define DPAA2IOC_ADDNI	_IOWR('D', 1, struct dpaa2_addni_args)
#define DPAA2IOC_DELNI	_IOW('D', 2, uint32_t)			/* DPNI id */
#define DPAA2IOC_LISTNI	_IOR('D', 3, struct dpaa2_listni_args)

#endif /* _DEV_DPAA2_DPAA2_IOCTL_H_ */
