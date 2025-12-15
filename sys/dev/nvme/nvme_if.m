# Copyright (c) 2025 Netlix, Inc
#
# SPDX-License-Identifier: BSD-2-Clause
#

# Interface from the NVME controller to its children to notify it of certain
# interesting events.

INTERFACE nvme;

HEADER {
	#include "dev/nvme/nvme_private.h"
};

#
# A new namespace is now available
#
METHOD int ns_added {
	device_t	dev;		/* nvme device */
	struct nvme_namespace *ns;	/* information about the namespace */
};

#
# A namespace has been removed
#
METHOD int ns_removed {
	device_t	dev;		/* nvme device */
	struct nvme_namespace *ns;	/* information about the namespace */
};

#
# A namespace has been changed somehow
#
METHOD int ns_changed {
	device_t	dev;		/* nvme device */
	uint32_t	nsid;		/* nsid that just changed */
};

#
# The controller has failed
#
METHOD int controller_failed {
	device_t	dev;		/* nvme device */
};

#
# Async completion
#
METHOD int handle_aen {
	device_t	dev;		/* nvme device */
	const struct nvme_completion *cpl; /* Completion for this async event */
	uint32_t	pg_nr;		/* Page number reported by async event */
	void		*page;		/* Contents of the page */
	uint32_t	page_len;	/* Length of the page */
};
