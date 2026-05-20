/*
 * Copyright (c) 2026 Justin Hibbits
 * Copyright (c) 2011-2012 Semihalf.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef IF_MEMAC_H_
#define IF_MEMAC_H_

/**
 * @group dTSEC common API.
 * @{
 */
#define MEMAC_MODE_REGULAR		0

#define MEMAC_LOCK(sc)			mtx_lock(&(sc)->sc_base.sc_lock)
#define MEMAC_UNLOCK(sc)		mtx_unlock(&(sc)->sc_base.sc_lock)
#define MEMAC_LOCK_ASSERT(sc)		mtx_assert(&(sc)->sc_base.sc_lock, MA_OWNED)
#define MEMAC_MII_LOCK(sc)		mtx_lock(&(sc)->sc_base.sc_mii_lock)
#define MEMAC_MII_UNLOCK(sc)		mtx_unlock(&(sc)->sc_base.sc_mii_lock)

enum eth_dev_type {
	ETH_MEMAC = 0x1,
	ETH_10GSEC = 0x2
};

struct memac_softc {
	struct dpaa_eth_softc	sc_base;
	bool			sc_fixed_link;
};
/** @} */


/**
 * @group dTSEC bus interface.
 * @{
 */
int		memac_attach(device_t dev);
int		memac_detach(device_t dev);
int		memac_suspend(device_t dev);
int		memac_resume(device_t dev);
int		memac_shutdown(device_t dev);
int		memac_miibus_readreg(device_t dev, int phy, int reg);
int		memac_miibus_writereg(device_t dev, int phy, int reg,
		    int value);
void		memac_miibus_statchg(device_t dev);
/** @} */

#endif /* IF_MEMAC_H_ */
