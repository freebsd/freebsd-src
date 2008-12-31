/*-
 * Copyright (c) 2006 Kip Macy
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/sun4v/mdesc/mdesc_vdevfindnode.c,v 1.1.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/endian.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/taskqueue.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <sys/types.h>
#include <sys/param.h>
#include <machine/mdesc_bus.h>
#include <machine/cddl/mdesc.h>


int
md_vdev_find_node(device_t dev, mde_cookie_t *valp)
{
	uint64_t              cfg_handle;
	mde_cookie_t          rootnode, node, *listp = NULL;
	int                   i, listsz, num_nodes, num_devices, error;
	md_t                 *mdp;

	cfg_handle = mdesc_bus_get_handle(dev);

	error = EINVAL;

	if ((mdp = md_get()) == NULL) 
		return (ENXIO);

	num_nodes = md_node_count(mdp);
	listsz = num_nodes * sizeof(mde_cookie_t);
	listp = (mde_cookie_t *)malloc(listsz, M_DEVBUF, M_WAITOK);
	rootnode = md_root_node(mdp);
	node = error = 0;

	num_devices = md_scan_dag(mdp, rootnode, 
				  md_find_name(mdp, "virtual-device"),
				  md_find_name(mdp, "fwd"), listp);
	
	if (num_devices == 0) {
		error = ENOENT;
		goto done;
	}
		
	for (i = 0; i < num_devices; i++) {
		uint64_t thandle;

		node = listp[i];
		md_get_prop_val(mdp, node, "cfg-handle", &thandle);
		if (thandle == cfg_handle) {
			*valp = node;
			break;
		}
	}

done:
	md_put(mdp);

	return (error);
}

