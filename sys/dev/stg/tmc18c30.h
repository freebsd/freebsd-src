/*
 * $FreeBSD: src/sys/dev/stg/tmc18c30.h,v 1.1.28.1 2008/10/02 02:57:24 kensmith Exp $
 */

extern devclass_t stg_devclass;

int	stg_alloc_resource	(device_t);
void	stg_release_resource	(device_t);
int	stg_probe		(device_t);
int	stg_attach		(device_t);
void	stg_detach		(device_t);
void	stg_intr		(void *);
