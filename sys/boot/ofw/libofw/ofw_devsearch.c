/*
 * Copyright (C) 2000 Benno Rice
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
 * THIS SOFTWARE IS PROVIDED BY Benno Rice ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <stand.h>

#include "libofw.h"

static phandle_t	curnode;

/*
 * Initialise a device tree search.  We do this by setting curpackage to point
 * to the root node.
 */
void
ofw_devsearch_init(void)
{
	curnode = OF_peer(0);
}

static phandle_t
nextnode(phandle_t current)
{
	phandle_t node;

	node = OF_child(current);

	if (node == -1)
		return(-1);

	if (node == 0) {
		node = OF_peer(current);

		if (node == -1)
			return(-1);

		if (node == 0) {
			phandle_t	newnode;

			newnode = current;
			node = 0;

			while (node == 0) {
				node = OF_parent(newnode);

				if (node == -1 || node == 0)
					return ((int)node);

				newnode = node;
				node = OF_peer(newnode);
			}
		}
	}

	return(node);
}

/*
 * Search for devices in the device tree with a certain device_type.
 * Return their paths.
 */
int
ofw_devsearch(const char *type, char *path)
{
	phandle_t	new;
	char		str[32];
	int		i;

	for (;;) {
		new = nextnode(curnode);
		if (new == 0 || new == -1) {
			return((int)new);
		}

		curnode = new;

		if ((i = OF_getprop(curnode, "device_type", str, 31)) != -1) {

			if (strncmp(str, type, 32) == 0) {
				if ((i = OF_package_to_path(curnode, path, 254)) == -1)
					return(-1);

				path[i] = '\0';
				return(1);
			}
		}
	}
}

/*
 * Get the device_type of a node.
 * Return DEVT_DISK, DEVT_NET or DEVT_NONE.
 */
int
ofw_devicetype(char *path)
{
	phandle_t	node;
	char		type[16];

	node = OF_finddevice(path);
	if (node == -1)
		return DEVT_NONE;

	OF_getprop(node, "device_type", type, 16);

	if (strncmp(type, "block", 16) == 0)
		return DEVT_DISK;
	else if (strncmp(type, "network", 16) == 0)
		return DEVT_NET;
	else
		return DEVT_NONE;
}
