/*
 * $FreeBSD$
 */

#ifdef __FreeBSD__

#include <sys/sysctl.h>

static int	   DRM(name_info)DRM_SYSCTL_HANDLER_ARGS;
static int	   DRM(vm_info)DRM_SYSCTL_HANDLER_ARGS;
static int	   DRM(clients_info)DRM_SYSCTL_HANDLER_ARGS;
static int	   DRM(bufs_info)DRM_SYSCTL_HANDLER_ARGS;

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
	{ "bufs",    DRM(bufs_info)    },
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

static int DRM(name_info)DRM_SYSCTL_HANDLER_ARGS
{
	drm_device_t *dev = arg1;
	char buf[128];
	int error;

	if (dev->unique) {
		DRM_SYSCTL_PRINT("%s 0x%x %s",
			       dev->name, dev2udev(dev->devnode), dev->unique);
	} else {
		DRM_SYSCTL_PRINT("%s 0x%x", dev->name, dev2udev(dev->devnode));
	}

	SYSCTL_OUT(req, "", 1);

	return 0;
}

static int DRM(_vm_info)DRM_SYSCTL_HANDLER_ARGS
{
	drm_device_t *dev = arg1;
	drm_local_map_t    *map;
	drm_map_list_entry_t    *listentry;
	const char   *types[] = { "FB", "REG", "SHM", "AGP", "SG" };
	const char   *type;
	int	     i=0;
	char         buf[128];
	int          error;

	DRM_SYSCTL_PRINT("\nslot	 offset	      size type flags	 "
			 "address mtrr\n");

	if (dev->maplist != NULL) {
		TAILQ_FOREACH(listentry, dev->maplist, link) {
			map = listentry->map;
			if (map->type < 0 || map->type > 4)
				type = "??";
			else
				type = types[map->type];
			DRM_SYSCTL_PRINT("%4d 0x%08lx 0x%08lx %4.4s  0x%02x 0x%08lx ",
					 i,
					 map->offset,
					 map->size,
					 type,
					 map->flags,
					 (unsigned long)map->handle);
			if (map->mtrr < 0) {
				DRM_SYSCTL_PRINT("no\n");
			} else {
				DRM_SYSCTL_PRINT("yes\n");
			}
			i++;
		}
	}
	SYSCTL_OUT(req, "", 1);

	return 0;
}

static int DRM(vm_info)DRM_SYSCTL_HANDLER_ARGS
{
	drm_device_t *dev = arg1;
	int	     ret;

	DRM_LOCK;
	ret = DRM(_vm_info)(oidp, arg1, arg2, req);
	DRM_UNLOCK;

	return ret;
}


/* drm_bufs_info is called whenever a process reads
   hw.dri.0.bufs. */

static int DRM(_bufs_info) DRM_SYSCTL_HANDLER_ARGS
{
	drm_device_t	 *dev = arg1;
	drm_device_dma_t *dma = dev->dma;
	int		 i;
	char             buf[128];
	int              error;

	if (!dma)	return 0;
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
		DRM_SYSCTL_PRINT(" %d", dma->buflist[i]->list);
	}
	DRM_SYSCTL_PRINT("\n");

	SYSCTL_OUT(req, "", 1);
	return 0;
}

static int DRM(bufs_info) DRM_SYSCTL_HANDLER_ARGS
{
	drm_device_t *dev = arg1;
	int	     ret;

	DRM_LOCK;
	ret = DRM(_bufs_info)(oidp, arg1, arg2, req);
	DRM_UNLOCK;
	return ret;
}


static int DRM(_clients_info) DRM_SYSCTL_HANDLER_ARGS
{
	drm_device_t *dev = arg1;
	drm_file_t   *priv;
	char         buf[128];
	int          error;

	DRM_SYSCTL_PRINT("\na dev	pid    uid	magic	  ioctls\n");
	TAILQ_FOREACH(priv, &dev->files, link) {
		DRM_SYSCTL_PRINT("%c %3d %5d %5d %10u %10lu\n",
			       priv->authenticated ? 'y' : 'n',
			       priv->minor,
			       priv->pid,
			       priv->uid,
			       priv->magic,
			       priv->ioctl_count);
	}

	SYSCTL_OUT(req, "", 1);
	return 0;
}

static int DRM(clients_info)DRM_SYSCTL_HANDLER_ARGS
{
	drm_device_t *dev = arg1;
	int	     ret;

	DRM_LOCK;
	ret = DRM(_clients_info)(oidp, arg1, arg2, req);
	DRM_UNLOCK;
	return ret;
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
