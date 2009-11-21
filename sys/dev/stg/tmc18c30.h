/*
 * $FreeBSD: src/sys/dev/stg/tmc18c30.h,v 1.2.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $
 */

extern devclass_t stg_devclass;

int	stg_alloc_resource	(device_t);
void	stg_release_resource	(device_t);
int	stg_probe		(device_t);
int	stg_attach		(device_t);
int	stg_detach		(device_t);
void	stg_intr		(void *);
