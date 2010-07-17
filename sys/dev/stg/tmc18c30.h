/*
 * $FreeBSD: src/sys/dev/stg/tmc18c30.h,v 1.2.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $
 */

extern devclass_t stg_devclass;

int	stg_alloc_resource	(device_t);
void	stg_release_resource	(device_t);
int	stg_probe		(device_t);
int	stg_attach		(device_t);
int	stg_detach		(device_t);
void	stg_intr		(void *);
