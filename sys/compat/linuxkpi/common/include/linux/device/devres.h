/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021-2026 The FreeBSD Foundation
 *
 * Portions of this software were developed by Björn Zeeb
 * under sponsorship from the FreeBSD Foundation.
 *
 */
#ifndef	_LINUXKPI_LINUX_DEVICE_DEVRES_H_
#define	_LINUXKPI_LINUX_DEVICE_DEVRES_H_

#include <linux/types.h>

/* Public and LinuxKPI internal devres functions. */
void *lkpi_devres_alloc(void(*release)(struct device *, void *), size_t, gfp_t);
void lkpi_devres_add(struct device *, void *);
void lkpi_devres_free(void *);
void *lkpi_devres_find(struct device *, void(*release)(struct device *, void *),
    int (*match)(struct device *, void *, void *), void *);
int lkpi_devres_destroy(struct device *, void(*release)(struct device *, void *),
    int (*match)(struct device *, void *, void *), void *);
#define	devres_alloc(_r, _s, _g)	lkpi_devres_alloc(_r, _s, _g)
#define	devres_add(_d, _p)		lkpi_devres_add(_d, _p)
#define	devres_free(_p)			lkpi_devres_free(_p)
#define	devres_find(_d, _rfn, _mfn, _mp) \
					lkpi_devres_find(_d, _rfn, _mfn, _mp)
#define	devres_destroy(_d, _rfn, _mfn, _mp) \
					lkpi_devres_destroy(_d, _rfn, _mfn, _mp)

void lkpi_devm_kfree(struct device *, const void *);
#define	devm_kfree(_d, _p)		lkpi_devm_kfree(_d, _p)

void lkpi_devm_kmalloc_release(struct device *, void *);

static inline void *
devm_kmalloc(struct device *dev, size_t size, gfp_t gfp)
{
	void *p;

	p = lkpi_devres_alloc(lkpi_devm_kmalloc_release, size, gfp);
	if (p != NULL)
		lkpi_devres_add(dev, p);

	return (p);
}

static inline void *
devm_kmemdup(struct device *dev, const void *src, size_t len, gfp_t gfp)
{
	void *dst;

	if (len == 0)
		return (NULL);

	dst = devm_kmalloc(dev, len, gfp);
	if (dst != NULL)
		memcpy(dst, src, len);

	return (dst);
}

static inline void *
devm_kmemdup_array(struct device *dev, const void *src, size_t n, size_t len,
    gfp_t gfp)
{
	return (devm_kmemdup(dev, src, size_mul(n, len), gfp));
}

#endif	/* _LINUXKPI_LINUX_DEVICE_DEVRES_H_ */
