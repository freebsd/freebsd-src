/* 
 * Copyright 2003 Eric Anholt
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * $FreeBSD$
 */

#ifdef __FreeBSD__

#include <sys/sysctl.h>

static int	   DRM(name_info)DRM_SYSCTL_HANDLER_ARGS;
static int	   DRM(vm_info)DRM_SYSCTL_HANDLER_ARGS;
static int	   DRM(clients_info)DRM_SYSCTL_HANDLER_ARGS;
#if __HAVE_DMA
static int	   DRM(bufs_info)DRM_SYSCTL_HANDLER_ARGS;
#endif

struct DRM(sysctl_list) {
	const char *name;
	int	   (*f) DRM_SYSCTL_HANDLER_ARGS;
} DRM(sysctl_list)[] = {
	{ "name",    DRM(name_info)    },
#ifdef DEBUG_MEMORY
	{ "mem",     DRM(mem_info)     },
#endif
	{ "vm",	     DRM(vm_info)      },
	{ "clients", DRM(clients_info) },
#if __HAVE_DMA
	{ "bufs",    DRM(bufs_info)    },
#endif
};
#define DRM_SYSCTL_ENTRIES (sizeof(DRM(sysctl_list))/sizeof(DRM(sysctl_list)[0]))

struct drm_sysctl_info {
	struct sysctl_ctx_list ctx;
	char		       name[2];
};

int DRM(sysctl_init)(drm_device_t *dev)
{
	struct drm_sysctl_info *info;
	struct sysctl_oid *oid;
	struct sysctl_oid *top, *drioid;
	int		  i;

	info = DRM(alloc)(sizeof *info, DRM_MEM_DRIVER);
	if ( !info )
		return 1;
	bzero(info, sizeof *info);
	dev->sysctl = info;

	/* Add the sysctl node for DRI if it doesn't already exist */
	drioid = SYSCTL_ADD_NODE( &info->ctx, &sysctl__hw_children, OID_AUTO, "dri", CTLFLAG_RW, NULL, "DRI Graphics");
	if (!drioid)
		return 1;

	/* Find the next free slot under hw.dri */
	i = 0;
	SLIST_FOREACH(oid, SYSCTL_CHILDREN(drioid), oid_link) {
		if (i <= oid->oid_arg2)
			i = oid->oid_arg2 + 1;
	}
	if (i>9)
		return 1;
	
	/* Add the hw.dri.x for our device */
	info->name[0] = '0' + i;
	info->name[1] = 0;
	top = SYSCTL_ADD_NODE( &info->ctx, SYSCTL_CHILDREN(drioid), OID_AUTO, info->name, CTLFLAG_RW, NULL, NULL);
	if (!top)
		return 1;
	
	for (i = 0; i < DRM_SYSCTL_ENTRIES; i++) {
		oid = sysctl_add_oid( &info->ctx, 
			SYSCTL_CHILDREN(top), 
			OID_AUTO, 
			DRM(sysctl_list)[i].name, 
			CTLTYPE_INT | CTLFLAG_RD, 
			dev, 
			0, 
			DRM(sysctl_list)[i].f, 
			"A", 
			NULL);
		if (!oid)
			return 1;
	}
	return 0;
}

int DRM(sysctl_cleanup)(drm_device_t *dev)
{
	int error;
	error = sysctl_ctx_free( &dev->sysctl->ctx );

	DRM(free)(dev->sysctl, sizeof *dev->sysctl, DRM_MEM_DRIVER);
	dev->sysctl = NULL;

	return error;
}

#define DRM_SYSCTL_PRINT(fmt, arg...)				\
do {								\
	snprintf(buf, sizeof(buf), fmt, ##arg);			\
	retcode = SYSCTL_OUT(req, buf, strlen(buf));		\
	if (retcode)						\
		goto done;					\
} while (0)

static int DRM(name_info)DRM_SYSCTL_HANDLER_ARGS
{
	drm_device_t *dev = arg1;
	char buf[128];
	int retcode;
	int hasunique = 0;

	DRM_SYSCTL_PRINT("%s 0x%x", dev->name, dev2udev(dev->devnode));
	
	DRM_LOCK();
	if (dev->unique) {
		snprintf(buf, sizeof(buf), " %s", dev->unique);
		hasunique = 1;
	}
	DRM_UNLOCK();
	
	if (hasunique)
		SYSCTL_OUT(req, buf, strlen(buf));

	SYSCTL_OUT(req, "", 1);

done:
	return retcode;
}

static int DRM(vm_info)DRM_SYSCTL_HANDLER_ARGS
{
	drm_device_t *dev = arg1;
	drm_local_map_t *map, *tempmaps;
	drm_map_list_entry_t    *listentry;
	const char   *types[] = { "FB", "REG", "SHM", "AGP", "SG" };
	const char *type, *yesno;
	int i, mapcount;
	char buf[128];
	int retcode;

	/* We can't hold the lock while doing SYSCTL_OUTs, so allocate a
	 * temporary copy of all the map entries and then SYSCTL_OUT that.
	 */
	DRM_LOCK();

	mapcount = 0;
	TAILQ_FOREACH(listentry, dev->maplist, link)
		mapcount++;

	tempmaps = DRM(alloc)(sizeof(drm_local_map_t) * mapcount, DRM_MEM_MAPS);
	if (tempmaps == NULL) {
		DRM_UNLOCK();
		return ENOMEM;
	}

	i = 0;
	TAILQ_FOREACH(listentry, dev->maplist, link)
		tempmaps[i++] = *listentry->map;

	DRM_UNLOCK();

	DRM_SYSCTL_PRINT("\nslot	 offset	      size type flags	 "
			 "address mtrr\n");

	for (i = 0; i < mapcount; i++) {
		map = &tempmaps[i];

		if (map->type < 0 || map->type > 4)
			type = "??";
		else
			type = types[map->type];

		if (map->mtrr <= 0)
			yesno = "no";
		else
			yesno = "yes";

		DRM_SYSCTL_PRINT(
		    "%4d 0x%08lx 0x%08lx %4.4s  0x%02x 0x%08lx %s\n", i,
		    map->offset, map->size, type, map->flags,
		    (unsigned long)map->handle, yesno);
	}
	SYSCTL_OUT(req, "", 1);

done:
	DRM(free)(tempmaps, sizeof(drm_local_map_t) * mapcount, DRM_MEM_MAPS);
	return retcode;
}

#if __HAVE_DMA
static int DRM(bufs_info) DRM_SYSCTL_HANDLER_ARGS
{
	drm_device_t	 *dev = arg1;
	drm_device_dma_t *dma = dev->dma;
	drm_device_dma_t tempdma;
	int *templists;
	int i;
	char buf[128];
	int retcode;

	/* We can't hold the locks around DRM_SYSCTL_PRINT, so make a temporary
	 * copy of the whole structure and the relevant data from buflist.
	 */
	DRM_LOCK();
	if (dma == NULL) {
		DRM_UNLOCK();
		return 0;
	}
	DRM_SPINLOCK(&dev->dma_lock);
	tempdma = *dma;
	templists = DRM(alloc)(sizeof(int) * dma->buf_count, DRM_MEM_BUFS);
	for (i = 0; i < dma->buf_count; i++)
		templists[i] = dma->buflist[i]->list;
	dma = &tempdma;
	DRM_SPINUNLOCK(&dev->dma_lock);
	DRM_UNLOCK();

	DRM_SYSCTL_PRINT("\n o     size count  free	 segs pages    kB\n");
	for (i = 0; i <= DRM_MAX_ORDER; i++) {
		if (dma->bufs[i].buf_count)
			DRM_SYSCTL_PRINT("%2d %8d %5d %5d %5d %5d %5d\n",
				       i,
				       dma->bufs[i].buf_size,
				       dma->bufs[i].buf_count,
				       atomic_read(&dma->bufs[i]
						   .freelist.count),
				       dma->bufs[i].seg_count,
				       dma->bufs[i].seg_count
				       *(1 << dma->bufs[i].page_order),
				       (dma->bufs[i].seg_count
					* (1 << dma->bufs[i].page_order))
				       * PAGE_SIZE / 1024);
	}
	DRM_SYSCTL_PRINT("\n");
	for (i = 0; i < dma->buf_count; i++) {
		if (i && !(i%32)) DRM_SYSCTL_PRINT("\n");
		DRM_SYSCTL_PRINT(" %d", templists[i]);
	}
	DRM_SYSCTL_PRINT("\n");

	SYSCTL_OUT(req, "", 1);
done:
	DRM(free)(templists, sizeof(int) * dma->buf_count, DRM_MEM_BUFS);
	return retcode;
}
#endif

static int DRM(clients_info)DRM_SYSCTL_HANDLER_ARGS
{
	drm_device_t *dev = arg1;
	drm_file_t *priv, *tempprivs;
	char buf[128];
	int retcode;
	int privcount, i;

	DRM_LOCK();

	privcount = 0;
	TAILQ_FOREACH(priv, &dev->files, link)
		privcount++;

	tempprivs = DRM(alloc)(sizeof(drm_file_t) * privcount, DRM_MEM_FILES);
	if (tempprivs == NULL) {
		DRM_UNLOCK();
		return ENOMEM;
	}
	i = 0;
	TAILQ_FOREACH(priv, &dev->files, link)
		tempprivs[i++] = *priv;

	DRM_UNLOCK();

	DRM_SYSCTL_PRINT("\na dev	pid    uid	magic	  ioctls\n");
	for (i = 0; i < privcount; i++) {
		priv = &tempprivs[i];
		DRM_SYSCTL_PRINT("%c %3d %5d %5d %10u %10lu\n",
			       priv->authenticated ? 'y' : 'n',
			       priv->minor,
			       priv->pid,
			       priv->uid,
			       priv->magic,
			       priv->ioctl_count);
	}

	SYSCTL_OUT(req, "", 1);
done:
	DRM(free)(tempprivs, sizeof(drm_file_t) * privcount, DRM_MEM_FILES);
	return retcode;
}

#elif defined(__NetBSD__)
/* stub it out for now, sysctl is only for debugging */
int DRM(sysctl_init)(drm_device_t *dev)
{
	return 0;
}

int DRM(sysctl_cleanup)(drm_device_t *dev)
{
	return 0;
}
#endif
