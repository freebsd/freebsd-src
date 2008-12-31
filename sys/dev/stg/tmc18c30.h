/*
 * $FreeBSD: src/sys/dev/stg/tmc18c30.h,v 1.1.30.1 2008/11/25 02:59:29 kensmith Exp $
 */

extern devclass_t stg_devclass;

int	stg_alloc_resource	(device_t);
void	stg_release_resource	(device_t);
int	stg_probe		(device_t);
int	stg_attach		(device_t);
void	stg_detach		(device_t);
void	stg_intr		(void *);
