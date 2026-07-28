/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2010-2016, Intel Corporation
 * Copyright (c) 2026 Kevin Bowling <kbowling@FreeBSD.org>
 */

#ifndef _IF_IGB_IOV_H_
#define _IF_IGB_IOV_H_

#define	IGB_IOV_MAX_FRAME_SIZE	0x2600

#ifdef PCI_IOV

#include <sys/nv.h>
#include <sys/iov_schema.h>
#include <dev/pci/pci_iov.h>

int	igb_iov_attach(struct e1000_softc *);
void	igb_iov_detach(struct e1000_softc *);
bool	igb_iov_supported(const struct e1000_softc *);
bool	igb_iov_enabled(const struct e1000_softc *);
int	igb_iov_validate(struct e1000_softc *, u16);
int	igb_if_iov_init(if_ctx_t, u16, const nvlist_t *);
void	igb_if_iov_uninit(if_ctx_t);
int	igb_if_iov_vf_add(if_ctx_t, u16, const nvlist_t *);
void	igb_iov_initialize(struct e1000_softc *);
void	igb_iov_handle_mbx(struct e1000_softc *);
void	igb_iov_handle_mdd(struct e1000_softc *);
void	igb_iov_mdd_event(struct e1000_softc *);
void	igb_iov_ping_all_vfs(struct e1000_softc *);
void	igb_iov_reset_prepare(struct e1000_softc *);
u32	igb_iov_intr_mask(const struct e1000_softc *);
void	igb_iov_rebuild_mta(struct e1000_softc *);
void	igb_iov_rebuild_vlan(struct e1000_softc *);
void	igb_iov_update_pf_vmolr(struct e1000_softc *);

#else

#define	igb_iov_attach(_sc)			((void)(_sc), 0)
#define	igb_iov_detach(_sc)			((void)(_sc))
#define	igb_iov_supported(_sc)			(false)
#define	igb_iov_enabled(_sc)			(false)
#define	igb_iov_initialize(_sc)
#define	igb_iov_handle_mbx(_sc)
#define	igb_iov_handle_mdd(_sc)
#define	igb_iov_mdd_event(_sc)
#define	igb_iov_ping_all_vfs(_sc)
#define	igb_iov_reset_prepare(_sc)
#define	igb_iov_intr_mask(_sc)			(0)
#define	igb_iov_rebuild_mta(_sc)
#define	igb_iov_rebuild_vlan(_sc)
#define	igb_iov_update_pf_vmolr(_sc)

#endif

#endif /* _IF_IGB_IOV_H_ */
