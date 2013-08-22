/*-
 * Copyright (c) 2011 NetApp, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/malloc.h>

#include "vdev.h"

struct vdev {
	SLIST_ENTRY(vdev) 	 entry;
	struct vdev_ops 	*ops;
	void			*dev;
};
static SLIST_HEAD(, vdev)	vdev_head;
static int 		  	vdev_count;

struct vdev_region {
	SLIST_ENTRY(vdev_region) 	 entry;
	struct vdev_ops 		*ops;
	void				*dev;
	struct io_region		*io;
};
static SLIST_HEAD(, vdev_region)	 region_head;
static int 		  		 region_count;

static MALLOC_DEFINE(M_VDEV, "vdev", "vdev");

#define VDEV_INIT 	(0)
#define VDEV_RESET	(1)
#define VDEV_HALT	(2)

// static const char* vdev_event_str[] = {"VDEV_INIT", "VDEV_RESET", "VDEV_HALT"};

static int
vdev_system_event(int event)
{
	struct vdev 	*vd;
	int		 rc;

	// TODO: locking
	SLIST_FOREACH(vd, &vdev_head, entry) {
		// printf("%s : %s Device %s\n", __func__, vdev_event_str[event], vd->ops->name);
		switch (event) {
			case VDEV_INIT:
				rc = vd->ops->init(vd->dev);
				break;
			case VDEV_RESET:
				rc = vd->ops->reset(vd->dev);
				break;
			case VDEV_HALT:
				rc = vd->ops->halt(vd->dev);
				break;
			default:
				break;
		}
		if (rc) {
			printf("vdev %s init failed rc=%d\n",
			    vd->ops->name, rc);
			return rc;
		}
	}
	return 0;
}

int
vdev_init(void)
{
	return vdev_system_event(VDEV_INIT);
}

int
vdev_reset(void)
{
	return vdev_system_event(VDEV_RESET);
}

int
vdev_halt(void)
{
	return vdev_system_event(VDEV_HALT);
}

void
vdev_vm_init(void)
{
	SLIST_INIT(&vdev_head);
	vdev_count = 0;

	SLIST_INIT(&region_head);
	region_count = 0;
}
void
vdev_vm_cleanup(void)
{
	struct vdev *vd;
     
	// TODO: locking
	while (!SLIST_EMPTY(&vdev_head)) {
		vd = SLIST_FIRST(&vdev_head);
		SLIST_REMOVE_HEAD(&vdev_head, entry);
		free(vd, M_VDEV);
		vdev_count--;
	}
}

int
vdev_register(struct vdev_ops *ops, void *dev)
{
	struct vdev *vd;
	vd = malloc(sizeof(*vd), M_VDEV, M_WAITOK | M_ZERO); 
	vd->ops = ops;
	vd->dev = dev;
	
	// TODO: locking
	SLIST_INSERT_HEAD(&vdev_head, vd, entry); 
	vdev_count++;
	return 0;
}

void
vdev_unregister(void *dev)
{
	struct vdev 	*vd, *found;

	found = NULL;
	// TODO: locking
	SLIST_FOREACH(vd, &vdev_head, entry) {
		if (vd->dev == dev) {
			found = vd;
		}
	}

	if (found) {
		SLIST_REMOVE(&vdev_head, found, vdev, entry);
		free(found, M_VDEV);
	}
}

#define IN_RANGE(val, start, end)	\
    (((val) >= (start)) && ((val) < (end)))

static struct vdev_region*
vdev_find_region(struct io_region *io, void *dev) 
{
	struct 		vdev_region *region, *found;
	uint64_t	region_base;
	uint64_t	region_end;

	found = NULL;

	// TODO: locking
	// FIXME: we should verify we are in the context the current
	// 	  vcpu here as well.
	SLIST_FOREACH(region, &region_head, entry) {
		region_base = region->io->base;
		region_end = region_base + region->io->len;
		if (IN_RANGE(io->base, region_base, region_end) &&
		    IN_RANGE(io->base+io->len, region_base, region_end+1) &&
		    (dev && dev == region->dev)) {
			found = region;
			break;
		}
	}
	return found;
}

int
vdev_register_region(struct vdev_ops *ops, void *dev, struct io_region *io)
{
	struct vdev_region *region;

	region = vdev_find_region(io, dev);
	if (region) {
		return -EEXIST;
	}

	region = malloc(sizeof(*region), M_VDEV, M_WAITOK | M_ZERO);
	region->io = io;
	region->ops = ops;
	region->dev = dev;

	// TODO: locking
	SLIST_INSERT_HEAD(&region_head, region, entry); 
	region_count++;

	return 0;
}

void
vdev_unregister_region(void *dev, struct io_region *io)
{
	struct vdev_region *region;

	region = vdev_find_region(io, dev);
	
	if (region) {
		SLIST_REMOVE(&region_head, region, vdev_region, entry);
		free(region, M_VDEV);
		region_count--;
	}
}

static int
vdev_memrw(uint64_t gpa, opsize_t size, uint64_t *data, int read)
{
	struct vdev_region 	*region;
	struct io_region	 io;
	region_attr_t		 attr;
	int			 rc;

	io.base = gpa;
	io.len = size;

	region = vdev_find_region(&io, NULL);
	if (!region)
		return -EINVAL;
	
	attr = (read) ? MMIO_READ : MMIO_WRITE;
	if (!(region->io->attr & attr))
		return -EPERM;

	if (read)
		rc = region->ops->memread(region->dev, gpa, size, data);
	else 
		rc = region->ops->memwrite(region->dev, gpa, size, *data);

	return rc;
}

int
vdev_memread(uint64_t gpa, opsize_t size, uint64_t *data)
{
	return vdev_memrw(gpa, size, data, 1);
}

int
vdev_memwrite(uint64_t gpa, opsize_t size, uint64_t data)
{
	return vdev_memrw(gpa, size, &data, 0);
}
