/*-
 * Copyright (c) 2011-2012 Semihalf.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef IF_DTSEC_H_
#define IF_DTSEC_H_

/**
 * @group dTSEC common API.
 * @{
 */
#define DTSEC_MODE_REGULAR		0
#define DTSEC_MODE_INDEPENDENT		1

#define DTSEC_LOCK(sc)			mtx_lock(&(sc)->sc_base.sc_lock)
#define DTSEC_UNLOCK(sc)		mtx_unlock(&(sc)->sc_base.sc_lock)
#define DTSEC_LOCK_ASSERT(sc)		mtx_assert(&(sc)->sc_base.sc_lock, MA_OWNED)
#define DTSEC_MII_LOCK(sc)		mtx_lock(&(sc)->sc_base.sc_mii_lock)
#define DTSEC_MII_UNLOCK(sc)		mtx_unlock(&(sc)->sc_base.sc_mii_lock)

enum eth_dev_type {
	ETH_DTSEC = 0x1,
	ETH_10GSEC = 0x2
};

struct dtsec_softc {
	struct dpaa_eth_softc sc_base;
	enum eth_dev_type		sc_eth_dev_type;
};
/** @} */


/**
 * @group dTSEC bus interface.
 * @{
 */
int		dtsec_attach(device_t dev);
int		dtsec_detach(device_t dev);
int		dtsec_suspend(device_t dev);
int		dtsec_resume(device_t dev);
int		dtsec_shutdown(device_t dev);
int		dtsec_miibus_readreg(device_t dev, int phy, int reg);
int		dtsec_miibus_writereg(device_t dev, int phy, int reg,
		    int value);
void		dtsec_miibus_statchg(device_t dev);
/** @} */

#endif /* IF_DTSEC_H_ */
