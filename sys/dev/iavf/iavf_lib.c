/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2021, Intel Corporation
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *   3. Neither the name of the Intel Corporation nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file iavf_lib.c
 * @brief library code common to both legacy and iflib
 *
 * Contains functions common to the iflib and legacy drivers. Includes
 * hardware initialization and control functions, as well as sysctl handlers
 * for the sysctls which are shared between the legacy and iflib drivers.
 */
#include "iavf_iflib.h"
#include "iavf_vc_common.h"

static void iavf_init_hw(struct iavf_hw *hw, device_t dev);
static u_int iavf_mc_filter_apply(void *arg, struct sockaddr_dl *sdl, u_int cnt);

/**
 * iavf_msec_pause - Pause for at least the specified number of milliseconds
 * @msecs: number of milliseconds to pause for
 *
 * Pause execution of the current thread for a specified number of
 * milliseconds. Used to enforce minimum delay times when waiting for various
 * hardware events.
 */
void
iavf_msec_pause(int msecs)
{
	pause("iavf_msec_pause", MSEC_2_TICKS(msecs));
}

/**
 * iavf_get_default_rss_key - Get the default RSS key for this driver
 * @key: output parameter to store the key in
 *
 * Copies the driver's default RSS key into the provided key variable.
 *
 * @pre assumes that key is not NULL and has at least IAVF_RSS_KEY_SIZE
 * storage space.
 */
void
iavf_get_default_rss_key(u32 *key)
{
	MPASS(key != NULL);

	u32 rss_seed[IAVF_RSS_KEY_SIZE_REG] = {0x41b01687,
	    0x183cfd8c, 0xce880440, 0x580cbc3c,
	    0x35897377, 0x328b25e1, 0x4fa98922,
	    0xb7d90c14, 0xd5bad70d, 0xcd15a2c1,
	    0x0, 0x0, 0x0};

	bcopy(rss_seed, key, IAVF_RSS_KEY_SIZE);
}

/**
 * iavf_allocate_pci_resources_common - Allocate PCI resources
 * @sc: the private device softc pointer
 *
 * @pre sc->dev is set
 *
 * Allocates the common PCI resources used by the driver.
 *
 * @returns zero on success, or an error code on failure.
 */
int
iavf_allocate_pci_resources_common(struct iavf_sc *sc)
{
	struct iavf_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	int rid;

	/* Map PCI BAR0 */
	rid = PCIR_BAR(0);
	sc->pci_mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &rid, RF_ACTIVE);

	if (!(sc->pci_mem)) {
		device_printf(dev, "Unable to allocate bus resource: PCI memory\n");
		return (ENXIO);
	}

	iavf_init_hw(hw, dev);

	/* Save off register access information */
	sc->osdep.mem_bus_space_tag =
		rman_get_bustag(sc->pci_mem);
	sc->osdep.mem_bus_space_handle =
		rman_get_bushandle(sc->pci_mem);
	sc->osdep.mem_bus_space_size = rman_get_size(sc->pci_mem);
	sc->osdep.flush_reg = IAVF_VFGEN_RSTAT;
	sc->osdep.dev = dev;

	sc->hw.hw_addr = (u8 *)&sc->osdep.mem_bus_space_handle;
	sc->hw.back = &sc->osdep;

	return (0);
}

/**
 * iavf_init_hw - Initialize the device HW
 * @hw: device hardware structure
 * @dev: the stack device_t pointer
 *
 * Attach helper function. Gathers information about the (virtual) hardware
 * for use elsewhere in the driver.
 */
static void
iavf_init_hw(struct iavf_hw *hw, device_t dev)
{
	/* Save off the information about this board */
	hw->vendor_id = pci_get_vendor(dev);
	hw->device_id = pci_get_device(dev);
	hw->revision_id = pci_read_config(dev, PCIR_REVID, 1);
	hw->subsystem_vendor_id =
	    pci_read_config(dev, PCIR_SUBVEND_0, 2);
	hw->subsystem_device_id =
	    pci_read_config(dev, PCIR_SUBDEV_0, 2);

	hw->bus.device = pci_get_slot(dev);
	hw->bus.func = pci_get_function(dev);
}

/**
 * iavf_sysctl_current_speed - Sysctl to display the current device speed
 * @oidp: syctl oid pointer
 * @arg1: pointer to the device softc typecasted to void *
 * @arg2: unused sysctl argument
 * @req: sysctl request structure
 *
 * Reads the current speed reported from the physical device into a string for
 * display by the current_speed sysctl.
 *
 * @returns zero or an error code on failure.
 */
int
iavf_sysctl_current_speed(SYSCTL_HANDLER_ARGS)
{
	struct iavf_sc *sc = (struct iavf_sc *)arg1;
	int error = 0;

	UNREFERENCED_PARAMETER(arg2);

	if (iavf_driver_is_detaching(sc))
		return (ESHUTDOWN);

	if (IAVF_CAP_ADV_LINK_SPEED(sc))
		error = sysctl_handle_string(oidp,
		  __DECONST(char *, iavf_ext_speed_to_str(iavf_adv_speed_to_ext_speed(sc->link_speed_adv))),
		  8, req);
	else
		error = sysctl_handle_string(oidp,
		  __DECONST(char *, iavf_vc_speed_to_string(sc->link_speed)),
		  8, req);

	return (error);
}

/**
 * iavf_reset_complete - Wait for a device reset to complete
 * @hw: pointer to the hardware structure
 *
 * Reads the reset registers and waits until they indicate that a device reset
 * is complete.
 *
 * @pre this function may call pause() and must not be called from a context
 * that cannot sleep.
 *
 * @returns zero on success, or EBUSY if it times out waiting for reset.
 */
int
iavf_reset_complete(struct iavf_hw *hw)
{
	u32 reg;

	/* Wait up to ~10 seconds */
	for (int i = 0; i < 100; i++) {
		reg = rd32(hw, IAVF_VFGEN_RSTAT) &
		    IAVF_VFGEN_RSTAT_VFR_STATE_MASK;

                if ((reg == VIRTCHNL_VFR_VFACTIVE) ||
		    (reg == VIRTCHNL_VFR_COMPLETED))
			return (0);
		iavf_msec_pause(100);
	}

	return (EBUSY);
}

/**
 * iavf_setup_vc - Setup virtchnl communication
 * @sc: device private softc
 *
 * iavf_attach() helper function. Initializes the admin queue and attempts to
 * establish contact with the PF by retrying the initial "API version" message
 * several times or until the PF responds.
 *
 * @returns zero on success, or an error code on failure.
 */
int
iavf_setup_vc(struct iavf_sc *sc)
{
	struct iavf_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	int error = 0, ret_error = 0, asq_retries = 0;
	bool send_api_ver_retried = 0;

	/* Need to set these AQ parameters before initializing AQ */
	hw->aq.num_arq_entries = IAVF_AQ_LEN;
	hw->aq.num_asq_entries = IAVF_AQ_LEN;
	hw->aq.arq_buf_size = IAVF_AQ_BUF_SZ;
	hw->aq.asq_buf_size = IAVF_AQ_BUF_SZ;

	for (int i = 0; i < IAVF_AQ_MAX_ERR; i++) {
		/* Initialize admin queue */
		error = iavf_init_adminq(hw);
		if (error) {
			device_printf(dev, "%s: init_adminq failed: %d\n",
			    __func__, error);
			ret_error = 1;
			continue;
		}

		iavf_dbg_init(sc, "Initialized Admin Queue; starting"
		    " send_api_ver attempt %d", i+1);

retry_send:
		/* Send VF's API version */
		error = iavf_send_api_ver(sc);
		if (error) {
			iavf_shutdown_adminq(hw);
			ret_error = 2;
			device_printf(dev, "%s: unable to send api"
			    " version to PF on attempt %d, error %d\n",
			    __func__, i+1, error);
		}

		asq_retries = 0;
		while (!iavf_asq_done(hw)) {
			if (++asq_retries > IAVF_AQ_MAX_ERR) {
				iavf_shutdown_adminq(hw);
				device_printf(dev, "Admin Queue timeout "
				    "(waiting for send_api_ver), %d more tries...\n",
				    IAVF_AQ_MAX_ERR - (i + 1));
				ret_error = 3;
				break;
			}
			iavf_msec_pause(10);
		}
		if (asq_retries > IAVF_AQ_MAX_ERR)
			continue;

		iavf_dbg_init(sc, "Sent API version message to PF");

		/* Verify that the VF accepts the PF's API version */
		error = iavf_verify_api_ver(sc);
		if (error == ETIMEDOUT) {
			if (!send_api_ver_retried) {
				/* Resend message, one more time */
				send_api_ver_retried = true;
				device_printf(dev,
				    "%s: Timeout while verifying API version on first"
				    " try!\n", __func__);
				goto retry_send;
			} else {
				device_printf(dev,
				    "%s: Timeout while verifying API version on second"
				    " try!\n", __func__);
				ret_error = 4;
				break;
			}
		}
		if (error) {
			device_printf(dev,
			    "%s: Unable to verify API version,"
			    " error %d\n", __func__, error);
			ret_error = 5;
		}
		break;
	}

	if (ret_error >= 4)
		iavf_shutdown_adminq(hw);
	return (ret_error);
}

/**
 * iavf_reset - Requests a VF reset from the PF.
 * @sc: device private softc
 *
 * @pre Requires the VF's Admin Queue to be initialized.
 * @returns zero on success, or an error code on failure.
 */
int
iavf_reset(struct iavf_sc *sc)
{
	struct iavf_hw	*hw = &sc->hw;
	device_t	dev = sc->dev;
	int		error = 0;

	/* Ask the PF to reset us if we are initiating */
	if (!iavf_test_state(&sc->state, IAVF_STATE_RESET_PENDING))
		iavf_request_reset(sc);

	iavf_msec_pause(100);
	error = iavf_reset_complete(hw);
	if (error) {
		device_printf(dev, "%s: VF reset failed\n",
		    __func__);
		return (error);
	}
	pci_enable_busmaster(dev);

	error = iavf_shutdown_adminq(hw);
	if (error) {
		device_printf(dev, "%s: shutdown_adminq failed: %d\n",
		    __func__, error);
		return (error);
	}

	error = iavf_init_adminq(hw);
	if (error) {
		device_printf(dev, "%s: init_adminq failed: %d\n",
		    __func__, error);
		return (error);
	}

	/* IFLIB: This is called only in the iflib driver */
	iavf_enable_adminq_irq(hw);
	return (0);
}

/**
 * iavf_enable_admin_irq - Enable the administrative interrupt
 * @hw: pointer to the hardware structure
 *
 * Writes to registers to enable the administrative interrupt cause, in order
 * to handle non-queue related interrupt events.
 */
void
iavf_enable_adminq_irq(struct iavf_hw *hw)
{
	wr32(hw, IAVF_VFINT_DYN_CTL01,
	    IAVF_VFINT_DYN_CTL01_INTENA_MASK |
	    IAVF_VFINT_DYN_CTL01_CLEARPBA_MASK |
	    IAVF_VFINT_DYN_CTL01_ITR_INDX_MASK);
	wr32(hw, IAVF_VFINT_ICR0_ENA1, IAVF_VFINT_ICR0_ENA1_ADMINQ_MASK);
	/* flush */
	rd32(hw, IAVF_VFGEN_RSTAT);
}

/**
 * iavf_disable_admin_irq - Disable the administrative interrupt cause
 * @hw: pointer to the hardware structure
 *
 * Writes to registers to disable the administrative interrupt cause.
 */
void
iavf_disable_adminq_irq(struct iavf_hw *hw)
{
	wr32(hw, IAVF_VFINT_DYN_CTL01, 0);
	wr32(hw, IAVF_VFINT_ICR0_ENA1, 0);
	iavf_flush(hw);
}

/**
 * iavf_vf_config - Configure this VF over the virtchnl
 * @sc: device private softc
 *
 * iavf_attach() helper function. Asks the PF for this VF's configuration, and
 * saves the information if it receives it.
 *
 * @returns zero on success, or an error code on failure.
 */
int
iavf_vf_config(struct iavf_sc *sc)
{
	struct iavf_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	int bufsz, error = 0, ret_error = 0;
	int asq_retries, retried = 0;

retry_config:
	error = iavf_send_vf_config_msg(sc);
	if (error) {
		device_printf(dev,
		    "%s: Unable to send VF config request, attempt %d,"
		    " error %d\n", __func__, retried + 1, error);
		ret_error = 2;
	}

	asq_retries = 0;
	while (!iavf_asq_done(hw)) {
		if (++asq_retries > IAVF_AQ_MAX_ERR) {
			device_printf(dev, "%s: Admin Queue timeout "
			    "(waiting for send_vf_config_msg), attempt %d\n",
			    __func__, retried + 1);
			ret_error = 3;
			goto fail;
		}
		iavf_msec_pause(10);
	}

	iavf_dbg_init(sc, "Sent VF config message to PF, attempt %d\n",
	    retried + 1);

	if (!sc->vf_res) {
		bufsz = sizeof(struct virtchnl_vf_resource) +
		    (IAVF_MAX_VF_VSI * sizeof(struct virtchnl_vsi_resource));
		sc->vf_res = (struct virtchnl_vf_resource *)malloc(bufsz, M_IAVF, M_NOWAIT);
		if (!sc->vf_res) {
			device_printf(dev,
			    "%s: Unable to allocate memory for VF configuration"
			    " message from PF on attempt %d\n", __func__, retried + 1);
			ret_error = 1;
			goto fail;
		}
	}

	/* Check for VF config response */
	error = iavf_get_vf_config(sc);
	if (error == ETIMEDOUT) {
		/* The 1st time we timeout, send the configuration message again */
		if (!retried) {
			retried++;
			goto retry_config;
		}
		device_printf(dev,
		    "%s: iavf_get_vf_config() timed out waiting for a response\n",
		    __func__);
	}
	if (error) {
		device_printf(dev,
		    "%s: Unable to get VF configuration from PF after %d tries!\n",
		    __func__, retried + 1);
		ret_error = 4;
	}
	goto done;

fail:
	free(sc->vf_res, M_IAVF);
done:
	return (ret_error);
}

/**
 * iavf_print_device_info - Print some device parameters at attach
 * @sc: device private softc
 *
 * Log a message about this virtual device's capabilities at attach time.
 */
void
iavf_print_device_info(struct iavf_sc *sc)
{
	device_t dev = sc->dev;

	device_printf(dev,
	    "VSIs %d, QPs %d, MSI-X %d, RSS sizes: key %d lut %d\n",
	    sc->vf_res->num_vsis,
	    sc->vf_res->num_queue_pairs,
	    sc->vf_res->max_vectors,
	    sc->vf_res->rss_key_size,
	    sc->vf_res->rss_lut_size);
	iavf_dbg_info(sc, "Capabilities=%b\n",
	    sc->vf_res->vf_cap_flags, IAVF_PRINTF_VF_OFFLOAD_FLAGS);
}

/**
 * iavf_get_vsi_res_from_vf_res - Get VSI parameters and info for this VF
 * @sc: device private softc
 *
 * Get the VSI parameters and information from the general VF resource info
 * received by the physical device.
 *
 * @returns zero on success, or an error code on failure.
 */
int
iavf_get_vsi_res_from_vf_res(struct iavf_sc *sc)
{
	struct iavf_vsi *vsi = &sc->vsi;
	device_t dev = sc->dev;

	sc->vsi_res = NULL;

	for (int i = 0; i < sc->vf_res->num_vsis; i++) {
		/* XXX: We only use the first VSI we find */
		if (sc->vf_res->vsi_res[i].vsi_type == IAVF_VSI_SRIOV)
			sc->vsi_res = &sc->vf_res->vsi_res[i];
	}
	if (!sc->vsi_res) {
		device_printf(dev, "%s: no LAN VSI found\n", __func__);
		return (EIO);
	}

	vsi->id = sc->vsi_res->vsi_id;
	return (0);
}

/**
 * iavf_set_mac_addresses - Set the MAC address for this interface
 * @sc: device private softc
 *
 * Set the permanent MAC address field in the HW structure. If a MAC address
 * has not yet been set for this device by the physical function, generate one
 * randomly.
 */
void
iavf_set_mac_addresses(struct iavf_sc *sc)
{
	struct iavf_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	u8 addr[ETHER_ADDR_LEN];

	/* If no mac address was assigned just make a random one */
	if (ETHER_IS_ZERO(hw->mac.addr)) {
		arc4rand(&addr, sizeof(addr), 0);
		addr[0] &= 0xFE;
		addr[0] |= 0x02;
		memcpy(hw->mac.addr, addr, sizeof(addr));
		device_printf(dev, "Generated random MAC address\n");
	}
	memcpy(hw->mac.perm_addr, hw->mac.addr, ETHER_ADDR_LEN);
}

/**
 * iavf_init_filters - Initialize filter structures
 * @sc: device private softc
 *
 * Initialize the MAC and VLAN filter list heads.
 *
 * @remark this is intended to be called only once during the device attach
 * process.
 *
 * @pre Because it uses M_WAITOK, this function should only be called in
 * a context that is safe to sleep.
 */
void
iavf_init_filters(struct iavf_sc *sc)
{
	sc->mac_filters = (struct mac_list *)malloc(sizeof(struct iavf_mac_filter),
	    M_IAVF, M_WAITOK | M_ZERO);
	SLIST_INIT(sc->mac_filters);
	sc->vlan_filters = (struct vlan_list *)malloc(sizeof(struct iavf_vlan_filter),
	    M_IAVF, M_WAITOK | M_ZERO);
	SLIST_INIT(sc->vlan_filters);
}

/**
 * iavf_free_filters - Release filter lists
 * @sc: device private softc
 *
 * Free the MAC and VLAN filter lists.
 *
 * @remark this is intended to be called only once during the device detach
 * process.
 */
void
iavf_free_filters(struct iavf_sc *sc)
{
	struct iavf_mac_filter *f;
	struct iavf_vlan_filter *v;

	while (!SLIST_EMPTY(sc->mac_filters)) {
		f = SLIST_FIRST(sc->mac_filters);
		SLIST_REMOVE_HEAD(sc->mac_filters, next);
		free(f, M_IAVF);
	}
	free(sc->mac_filters, M_IAVF);
	while (!SLIST_EMPTY(sc->vlan_filters)) {
		v = SLIST_FIRST(sc->vlan_filters);
		SLIST_REMOVE_HEAD(sc->vlan_filters, next);
		free(v, M_IAVF);
	}
	free(sc->vlan_filters, M_IAVF);
}

/**
 * iavf_add_device_sysctls_common - Initialize common device sysctls
 * @sc: device private softc
 *
 * Setup sysctls common to both the iflib and legacy drivers.
 */
void
iavf_add_device_sysctls_common(struct iavf_sc *sc)
{
	device_t dev = sc->dev;
	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(dev);
	struct sysctl_oid_list *ctx_list =
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev));

	SYSCTL_ADD_PROC(ctx, ctx_list,
	    OID_AUTO, "current_speed", CTLTYPE_STRING | CTLFLAG_RD,
	    sc, 0, iavf_sysctl_current_speed, "A", "Current Port Speed");

	SYSCTL_ADD_PROC(ctx, ctx_list,
	    OID_AUTO, "tx_itr", CTLTYPE_INT | CTLFLAG_RW,
	    sc, 0, iavf_sysctl_tx_itr, "I",
	    "Immediately set TX ITR value for all queues");

	SYSCTL_ADD_PROC(ctx, ctx_list,
	    OID_AUTO, "rx_itr", CTLTYPE_INT | CTLFLAG_RW,
	    sc, 0, iavf_sysctl_rx_itr, "I",
	    "Immediately set RX ITR value for all queues");

	SYSCTL_ADD_UQUAD(ctx, ctx_list,
	    OID_AUTO, "admin_irq", CTLFLAG_RD,
	    &sc->admin_irq, "Admin Queue IRQ Handled");
}

/**
 * iavf_add_debug_sysctls_common - Initialize common debug sysctls
 * @sc: device private softc
 * @debug_list: pionter to debug sysctl node
 *
 * Setup sysctls used for debugging the device driver into the debug sysctl
 * node.
 */
void
iavf_add_debug_sysctls_common(struct iavf_sc *sc, struct sysctl_oid_list *debug_list)
{
	device_t dev = sc->dev;
	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(dev);

	SYSCTL_ADD_UINT(ctx, debug_list,
	    OID_AUTO, "shared_debug_mask", CTLFLAG_RW,
	    &sc->hw.debug_mask, 0, "Shared code debug message level");

	SYSCTL_ADD_UINT(ctx, debug_list,
	    OID_AUTO, "core_debug_mask", CTLFLAG_RW,
	    (unsigned int *)&sc->dbg_mask, 0, "Non-shared code debug message level");

	SYSCTL_ADD_PROC(ctx, debug_list,
	    OID_AUTO, "filter_list", CTLTYPE_STRING | CTLFLAG_RD,
	    sc, 0, iavf_sysctl_sw_filter_list, "A", "SW Filter List");
}

/**
 * iavf_sysctl_tx_itr - Sysctl to set the Tx ITR value
 * @oidp: sysctl oid pointer
 * @arg1: pointer to the device softc
 * @arg2: unused sysctl argument
 * @req: sysctl req pointer
 *
 * On read, returns the Tx ITR value for all of the VF queues. On write,
 * update the Tx ITR registers with the new Tx ITR value.
 *
 * @returns zero on success, or an error code on failure.
 */
int
iavf_sysctl_tx_itr(SYSCTL_HANDLER_ARGS)
{
	struct iavf_sc *sc = (struct iavf_sc *)arg1;
	device_t dev = sc->dev;
	int requested_tx_itr;
	int error = 0;

	UNREFERENCED_PARAMETER(arg2);

	if (iavf_driver_is_detaching(sc))
		return (ESHUTDOWN);

	requested_tx_itr = sc->tx_itr;
	error = sysctl_handle_int(oidp, &requested_tx_itr, 0, req);
	if ((error) || (req->newptr == NULL))
		return (error);
	if (requested_tx_itr < 0 || requested_tx_itr > IAVF_MAX_ITR) {
		device_printf(dev,
		    "Invalid TX itr value; value must be between 0 and %d\n",
		        IAVF_MAX_ITR);
		return (EINVAL);
	}

	sc->tx_itr = requested_tx_itr;
	iavf_configure_tx_itr(sc);

	return (error);
}

/**
 * iavf_sysctl_rx_itr - Sysctl to set the Rx ITR value
 * @oidp: sysctl oid pointer
 * @arg1: pointer to the device softc
 * @arg2: unused sysctl argument
 * @req: sysctl req pointer
 *
 * On read, returns the Rx ITR value for all of the VF queues. On write,
 * update the ITR registers with the new Rx ITR value.
 *
 * @returns zero on success, or an error code on failure.
 */
int
iavf_sysctl_rx_itr(SYSCTL_HANDLER_ARGS)
{
	struct iavf_sc *sc = (struct iavf_sc *)arg1;
	device_t dev = sc->dev;
	int requested_rx_itr;
	int error = 0;

	UNREFERENCED_PARAMETER(arg2);

	if (iavf_driver_is_detaching(sc))
		return (ESHUTDOWN);

	requested_rx_itr = sc->rx_itr;
	error = sysctl_handle_int(oidp, &requested_rx_itr, 0, req);
	if ((error) || (req->newptr == NULL))
		return (error);
	if (requested_rx_itr < 0 || requested_rx_itr > IAVF_MAX_ITR) {
		device_printf(dev,
		    "Invalid RX itr value; value must be between 0 and %d\n",
		        IAVF_MAX_ITR);
		return (EINVAL);
	}

	sc->rx_itr = requested_rx_itr;
	iavf_configure_rx_itr(sc);

	return (error);
}

/**
 * iavf_configure_tx_itr - Configure the Tx ITR
 * @sc: device private softc
 *
 * Updates the ITR registers with a new Tx ITR setting.
 */
void
iavf_configure_tx_itr(struct iavf_sc *sc)
{
	struct iavf_hw		*hw = &sc->hw;
	struct iavf_vsi		*vsi = &sc->vsi;
	struct iavf_tx_queue	*que = vsi->tx_queues;

	vsi->tx_itr_setting = sc->tx_itr;

	for (int i = 0; i < IAVF_NTXQS(vsi); i++, que++) {
		struct tx_ring	*txr = &que->txr;

		wr32(hw, IAVF_VFINT_ITRN1(IAVF_TX_ITR, i),
		    vsi->tx_itr_setting);
		txr->itr = vsi->tx_itr_setting;
		txr->latency = IAVF_AVE_LATENCY;
	}
}

/**
 * iavf_configure_rx_itr - Configure the Rx ITR
 * @sc: device private softc
 *
 * Updates the ITR registers with a new Rx ITR setting.
 */
void
iavf_configure_rx_itr(struct iavf_sc *sc)
{
	struct iavf_hw		*hw = &sc->hw;
	struct iavf_vsi		*vsi = &sc->vsi;
	struct iavf_rx_queue	*que = vsi->rx_queues;

	vsi->rx_itr_setting = sc->rx_itr;

	for (int i = 0; i < IAVF_NRXQS(vsi); i++, que++) {
		struct rx_ring	*rxr = &que->rxr;

		wr32(hw, IAVF_VFINT_ITRN1(IAVF_RX_ITR, i),
		    vsi->rx_itr_setting);
		rxr->itr = vsi->rx_itr_setting;
		rxr->latency = IAVF_AVE_LATENCY;
	}
}

/**
 * iavf_create_debug_sysctl_tree - Create a debug sysctl node
 * @sc: device private softc
 *
 * Create a sysctl node meant to hold sysctls used to print debug information.
 * Mark it as CTLFLAG_SKIP so that these sysctls do not show up in the
 * "sysctl -a" output.
 *
 * @returns a pointer to the created sysctl node.
 */
struct sysctl_oid_list *
iavf_create_debug_sysctl_tree(struct iavf_sc *sc)
{
	device_t dev = sc->dev;
	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(dev);
	struct sysctl_oid_list *ctx_list =
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev));
	struct sysctl_oid *debug_node;

	debug_node = SYSCTL_ADD_NODE(ctx, ctx_list,
	    OID_AUTO, "debug", CTLFLAG_RD | CTLFLAG_SKIP, NULL, "Debug Sysctls");

	return (SYSCTL_CHILDREN(debug_node));
}

/**
 * iavf_add_vsi_sysctls - Add sysctls for a given VSI
 * @dev: device pointer
 * @vsi: pointer to the VSI
 * @ctx: sysctl context to add to
 * @sysctl_name: name of the sysctl node (containing the VSI number)
 *
 * Adds a new sysctl node for holding specific sysctls for the given VSI.
 */
void
iavf_add_vsi_sysctls(device_t dev, struct iavf_vsi *vsi,
    struct sysctl_ctx_list *ctx, const char *sysctl_name)
{
	struct sysctl_oid *tree;
	struct sysctl_oid_list *child;
	struct sysctl_oid_list *vsi_list;

	tree = device_get_sysctl_tree(dev);
	child = SYSCTL_CHILDREN(tree);
	vsi->vsi_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, sysctl_name,
				   CTLFLAG_RD, NULL, "VSI Number");
	vsi_list = SYSCTL_CHILDREN(vsi->vsi_node);

	iavf_add_sysctls_eth_stats(ctx, vsi_list, &vsi->eth_stats);
}

/**
 * iavf_sysctl_sw_filter_list - Dump software filters
 * @oidp: sysctl oid pointer
 * @arg1: pointer to the device softc
 * @arg2: unused sysctl argument
 * @req: sysctl req pointer
 *
 * On read, generates a string which lists the MAC and VLAN filters added to
 * this virtual device. Useful for debugging to see whether or not the
 * expected filters have been configured by software.
 *
 * @returns zero on success, or an error code on failure.
 */
int
iavf_sysctl_sw_filter_list(SYSCTL_HANDLER_ARGS)
{
	struct iavf_sc *sc = (struct iavf_sc *)arg1;
	struct iavf_mac_filter *f;
	struct iavf_vlan_filter *v;
	device_t dev = sc->dev;
	int ftl_len, ftl_counter = 0, error = 0;
	struct sbuf *buf;

	UNREFERENCED_2PARAMETER(arg2, oidp);

	if (iavf_driver_is_detaching(sc))
		return (ESHUTDOWN);

	buf = sbuf_new_for_sysctl(NULL, NULL, 128, req);
	if (!buf) {
		device_printf(dev, "Could not allocate sbuf for output.\n");
		return (ENOMEM);
	}

	sbuf_printf(buf, "\n");

	/* Print MAC filters */
	sbuf_printf(buf, "MAC Filters:\n");
	ftl_len = 0;
	SLIST_FOREACH(f, sc->mac_filters, next)
		ftl_len++;
	if (ftl_len < 1)
		sbuf_printf(buf, "(none)\n");
	else {
		SLIST_FOREACH(f, sc->mac_filters, next) {
			sbuf_printf(buf,
			    MAC_FORMAT ", flags %#06x\n",
			    MAC_FORMAT_ARGS(f->macaddr), f->flags);
		}
	}

	/* Print VLAN filters */
	sbuf_printf(buf, "VLAN Filters:\n");
	ftl_len = 0;
	SLIST_FOREACH(v, sc->vlan_filters, next)
		ftl_len++;
	if (ftl_len < 1)
		sbuf_printf(buf, "(none)");
	else {
		SLIST_FOREACH(v, sc->vlan_filters, next) {
			sbuf_printf(buf,
			    "%d, flags %#06x",
			    v->vlan, v->flags);
			/* don't print '\n' for last entry */
			if (++ftl_counter != ftl_len)
				sbuf_printf(buf, "\n");
		}
	}

	error = sbuf_finish(buf);
	if (error)
		device_printf(dev, "Error finishing sbuf: %d\n", error);

	sbuf_delete(buf);
	return (error);
}

/**
 * iavf_media_status_common - Get media status for this device
 * @sc: device softc pointer
 * @ifmr: ifmedia request structure
 *
 * Report the media status for this device into the given ifmr structure.
 */
void
iavf_media_status_common(struct iavf_sc *sc, struct ifmediareq *ifmr)
{
	enum iavf_ext_link_speed ext_speed;

	iavf_update_link_status(sc);

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	if (!sc->link_up)
		return;

	ifmr->ifm_status |= IFM_ACTIVE;
	/* Hardware is always full-duplex */
	ifmr->ifm_active |= IFM_FDX;

	/* Based on the link speed reported by the PF over the AdminQ, choose a
	 * PHY type to report. This isn't 100% correct since we don't really
	 * know the underlying PHY type of the PF, but at least we can report
	 * a valid link speed...
	 */
	if (IAVF_CAP_ADV_LINK_SPEED(sc))
		ext_speed = iavf_adv_speed_to_ext_speed(sc->link_speed_adv);
	else
		ext_speed = iavf_vc_speed_to_ext_speed(sc->link_speed);

	ifmr->ifm_active |= iavf_ext_speed_to_ifmedia(ext_speed);
}

/**
 * iavf_media_change_common - Change the media type for this device
 * @ifp: ifnet structure
 *
 * @returns ENODEV because changing the media and speed is not supported.
 */
int
iavf_media_change_common(if_t ifp)
{
	if_printf(ifp, "Changing speed is not supported\n");

	return (ENODEV);
}

/**
 * iavf_set_initial_baudrate - Set the initial device baudrate
 * @ifp: ifnet structure
 *
 * Set the baudrate for this ifnet structure to the expected initial value of
 * 40Gbps. This maybe updated to a lower baudrate after the physical function
 * reports speed to us over the virtchnl interface.
 */
void
iavf_set_initial_baudrate(if_t ifp)
{
	if_setbaudrate(ifp, IF_Gbps(40));
}

/**
 * iavf_add_sysctls_eth_stats - Add ethernet statistics sysctls
 * @ctx: the sysctl ctx to add to
 * @child: the node to add the sysctls to
 * @eth_stats: ethernet stats structure
 *
 * Creates sysctls that report the values of the provided ethernet stats
 * structure.
 */
void
iavf_add_sysctls_eth_stats(struct sysctl_ctx_list *ctx,
	struct sysctl_oid_list *child,
	struct iavf_eth_stats *eth_stats)
{
	struct iavf_sysctl_info ctls[] =
	{
		{&eth_stats->rx_bytes, "good_octets_rcvd", "Good Octets Received"},
		{&eth_stats->rx_unicast, "ucast_pkts_rcvd",
			"Unicast Packets Received"},
		{&eth_stats->rx_multicast, "mcast_pkts_rcvd",
			"Multicast Packets Received"},
		{&eth_stats->rx_broadcast, "bcast_pkts_rcvd",
			"Broadcast Packets Received"},
		{&eth_stats->rx_discards, "rx_discards", "Discarded RX packets"},
		{&eth_stats->rx_unknown_protocol, "rx_unknown_proto",
			"RX unknown protocol packets"},
		{&eth_stats->tx_bytes, "good_octets_txd", "Good Octets Transmitted"},
		{&eth_stats->tx_unicast, "ucast_pkts_txd", "Unicast Packets Transmitted"},
		{&eth_stats->tx_multicast, "mcast_pkts_txd",
			"Multicast Packets Transmitted"},
		{&eth_stats->tx_broadcast, "bcast_pkts_txd",
			"Broadcast Packets Transmitted"},
		{&eth_stats->tx_errors, "tx_errors", "TX packet errors"},
		// end
		{0,0,0}
	};

	struct iavf_sysctl_info *entry = ctls;

	while (entry->stat != 0)
	{
		SYSCTL_ADD_UQUAD(ctx, child, OID_AUTO, entry->name,
				CTLFLAG_RD, entry->stat,
				entry->description);
		entry++;
	}
}

/**
 * iavf_max_vc_speed_to_value - Convert link speed to IF speed value
 * @link_speeds: bitmap of supported link speeds
 *
 * @returns the link speed value for the highest speed reported in the
 * link_speeds bitmap.
 */
u64
iavf_max_vc_speed_to_value(u8 link_speeds)
{
	if (link_speeds & VIRTCHNL_LINK_SPEED_40GB)
		return IF_Gbps(40);
	if (link_speeds & VIRTCHNL_LINK_SPEED_25GB)
		return IF_Gbps(25);
	if (link_speeds & VIRTCHNL_LINK_SPEED_20GB)
		return IF_Gbps(20);
	if (link_speeds & VIRTCHNL_LINK_SPEED_10GB)
		return IF_Gbps(10);
	if (link_speeds & VIRTCHNL_LINK_SPEED_1GB)
		return IF_Gbps(1);
	if (link_speeds & VIRTCHNL_LINK_SPEED_100MB)
		return IF_Mbps(100);
	else
		/* Minimum supported link speed */
		return IF_Mbps(100);
}

/**
 * iavf_config_rss_reg - Configure RSS using registers
 * @sc: device private softc
 *
 * Configures RSS for this function using the device registers. Called if the
 * PF does not support configuring RSS over the virtchnl interface.
 */
void
iavf_config_rss_reg(struct iavf_sc *sc)
{
	struct iavf_hw	*hw = &sc->hw;
	struct iavf_vsi	*vsi = &sc->vsi;
	u32		lut = 0;
	u64		set_hena = 0, hena;
	int		i, j, que_id;
	u32		rss_seed[IAVF_RSS_KEY_SIZE_REG];
#ifdef RSS
	u32		rss_hash_config;
#endif

	/* Don't set up RSS if using a single queue */
	if (IAVF_NRXQS(vsi) == 1) {
		wr32(hw, IAVF_VFQF_HENA(0), 0);
		wr32(hw, IAVF_VFQF_HENA(1), 0);
		iavf_flush(hw);
		return;
	}

#ifdef RSS
	/* Fetch the configured RSS key */
	rss_getkey((uint8_t *) &rss_seed);
#else
	iavf_get_default_rss_key(rss_seed);
#endif

	/* Fill out hash function seed */
	for (i = 0; i < IAVF_RSS_KEY_SIZE_REG; i++)
                wr32(hw, IAVF_VFQF_HKEY(i), rss_seed[i]);

	/* Enable PCTYPES for RSS: */
#ifdef RSS
	rss_hash_config = rss_gethashconfig();
	if (rss_hash_config & RSS_HASHTYPE_RSS_IPV4)
                set_hena |= ((u64)1 << IAVF_FILTER_PCTYPE_NONF_IPV4_OTHER);
	if (rss_hash_config & RSS_HASHTYPE_RSS_TCP_IPV4)
                set_hena |= ((u64)1 << IAVF_FILTER_PCTYPE_NONF_IPV4_TCP);
	if (rss_hash_config & RSS_HASHTYPE_RSS_UDP_IPV4)
                set_hena |= ((u64)1 << IAVF_FILTER_PCTYPE_NONF_IPV4_UDP);
	if (rss_hash_config & RSS_HASHTYPE_RSS_IPV6)
                set_hena |= ((u64)1 << IAVF_FILTER_PCTYPE_NONF_IPV6_OTHER);
	if (rss_hash_config & RSS_HASHTYPE_RSS_IPV6_EX)
		set_hena |= ((u64)1 << IAVF_FILTER_PCTYPE_FRAG_IPV6);
	if (rss_hash_config & RSS_HASHTYPE_RSS_TCP_IPV6)
                set_hena |= ((u64)1 << IAVF_FILTER_PCTYPE_NONF_IPV6_TCP);
        if (rss_hash_config & RSS_HASHTYPE_RSS_UDP_IPV6)
                set_hena |= ((u64)1 << IAVF_FILTER_PCTYPE_NONF_IPV6_UDP);
#else
	set_hena = IAVF_DEFAULT_RSS_HENA_XL710;
#endif
	hena = (u64)rd32(hw, IAVF_VFQF_HENA(0)) |
	    ((u64)rd32(hw, IAVF_VFQF_HENA(1)) << 32);
	hena |= set_hena;
	wr32(hw, IAVF_VFQF_HENA(0), (u32)hena);
	wr32(hw, IAVF_VFQF_HENA(1), (u32)(hena >> 32));

	/* Populate the LUT with max no. of queues in round robin fashion */
	for (i = 0, j = 0; i < IAVF_RSS_VSI_LUT_SIZE; i++, j++) {
                if (j == IAVF_NRXQS(vsi))
                        j = 0;
#ifdef RSS
		/*
		 * Fetch the RSS bucket id for the given indirection entry.
		 * Cap it at the number of configured buckets (which is
		 * num_rx_queues.)
		 */
		que_id = rss_get_indirection_to_bucket(i);
		que_id = que_id % IAVF_NRXQS(vsi);
#else
		que_id = j;
#endif
                /* lut = 4-byte sliding window of 4 lut entries */
                lut = (lut << 8) | (que_id & IAVF_RSS_VF_LUT_ENTRY_MASK);
                /* On i = 3, we have 4 entries in lut; write to the register */
                if ((i & 3) == 3) {
                        wr32(hw, IAVF_VFQF_HLUT(i >> 2), lut);
			iavf_dbg_rss(sc, "%s: HLUT(%2d): %#010x", __func__,
			    i, lut);
		}
        }
	iavf_flush(hw);
}

/**
 * iavf_config_rss_pf - Configure RSS using PF virtchnl messages
 * @sc: device private softc
 *
 * Configure RSS by sending virtchnl messages to the PF.
 */
void
iavf_config_rss_pf(struct iavf_sc *sc)
{
	iavf_send_vc_msg(sc, IAVF_FLAG_AQ_CONFIG_RSS_KEY);

	iavf_send_vc_msg(sc, IAVF_FLAG_AQ_SET_RSS_HENA);

	iavf_send_vc_msg(sc, IAVF_FLAG_AQ_CONFIG_RSS_LUT);
}

/**
 * iavf_config_rss - setup RSS
 * @sc: device private softc
 *
 * Configures RSS using the method determined by capability flags in the VF
 * resources structure sent from the PF over the virtchnl interface.
 *
 * @remark RSS keys and table are cleared on VF reset.
 */
void
iavf_config_rss(struct iavf_sc *sc)
{
	if (sc->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_RSS_REG) {
		iavf_dbg_info(sc, "Setting up RSS using VF registers...\n");
		iavf_config_rss_reg(sc);
	} else if (sc->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_RSS_PF) {
		iavf_dbg_info(sc, "Setting up RSS using messages to PF...\n");
		iavf_config_rss_pf(sc);
	} else
		device_printf(sc->dev, "VF does not support RSS capability sent by PF.\n");
}

/**
 * iavf_config_promisc - setup promiscuous mode
 * @sc: device private softc
 * @flags: promiscuous flags to configure
 *
 * Request that promiscuous modes be enabled from the PF
 *
 * @returns zero on success, or an error code on failure.
 */
int
iavf_config_promisc(struct iavf_sc *sc, int flags)
{
	if_t ifp = sc->vsi.ifp;

	sc->promisc_flags = 0;

	if (flags & IFF_ALLMULTI ||
		if_llmaddr_count(ifp) == MAX_MULTICAST_ADDR)
		sc->promisc_flags |= FLAG_VF_MULTICAST_PROMISC;
	if (flags & IFF_PROMISC)
		sc->promisc_flags |= FLAG_VF_UNICAST_PROMISC;

	iavf_send_vc_msg(sc, IAVF_FLAG_AQ_CONFIGURE_PROMISC);

	return (0);
}

/**
 * iavf_mc_filter_apply - Program a MAC filter for this VF
 * @arg: pointer to the device softc
 * @sdl: MAC multicast address
 * @cnt: unused parameter
 *
 * Program a MAC address multicast filter for this device. Intended
 * to be used with the map-like function if_foreach_llmaddr().
 *
 * @returns 1 on success, or 0 on failure
 */
static u_int
iavf_mc_filter_apply(void *arg, struct sockaddr_dl *sdl, u_int cnt __unused)
{
	struct iavf_sc *sc = (struct iavf_sc *)arg;
	int error;

	error = iavf_add_mac_filter(sc, (u8*)LLADDR(sdl), IAVF_FILTER_MC);

	return (!error);
}

/**
 * iavf_init_multi - Initialize multicast address filters
 * @sc: device private softc
 *
 * Called during initialization to reset multicast address filters to a known
 * fresh state by deleting all currently active filters.
 */
void
iavf_init_multi(struct iavf_sc *sc)
{
	struct iavf_mac_filter *f;
	int mcnt = 0;

	/* First clear any multicast filters */
	SLIST_FOREACH(f, sc->mac_filters, next) {
		if ((f->flags & IAVF_FILTER_USED)
		    && (f->flags & IAVF_FILTER_MC)) {
			f->flags |= IAVF_FILTER_DEL;
			mcnt++;
		}
	}
	if (mcnt > 0)
		iavf_send_vc_msg(sc, IAVF_FLAG_AQ_DEL_MAC_FILTER);
}

/**
 * iavf_multi_set - Set multicast filters
 * @sc: device private softc
 *
 * Set multicast MAC filters for this device. If there are too many filters,
 * this will request the device to go into multicast promiscuous mode instead.
 */
void
iavf_multi_set(struct iavf_sc *sc)
{
	if_t ifp = sc->vsi.ifp;
	int mcnt = 0;

	IOCTL_DEBUGOUT("iavf_multi_set: begin");

	mcnt = if_llmaddr_count(ifp);
	if (__predict_false(mcnt == MAX_MULTICAST_ADDR)) {
		/* Delete MC filters and enable mulitcast promisc instead */
		iavf_init_multi(sc);
		sc->promisc_flags |= FLAG_VF_MULTICAST_PROMISC;
		iavf_send_vc_msg(sc, IAVF_FLAG_AQ_CONFIGURE_PROMISC);
		return;
	}

	/* If there aren't too many filters, delete existing MC filters */
	iavf_init_multi(sc);

	/* And (re-)install filters for all mcast addresses */
	mcnt = if_foreach_llmaddr(ifp, iavf_mc_filter_apply, sc);

	if (mcnt > 0)
		iavf_send_vc_msg(sc, IAVF_FLAG_AQ_ADD_MAC_FILTER);
}

/**
 * iavf_add_mac_filter - Add a MAC filter to the sc MAC list
 * @sc: device private softc
 * @macaddr: MAC address to add
 * @flags: filter flags
 *
 * Add a new MAC filter to the softc MAC filter list. These will later be sent
 * to the physical function (and ultimately hardware) via the virtchnl
 * interface.
 *
 * @returns zero on success, EEXIST if the filter already exists, and ENOMEM
 * if we ran out of memory allocating the filter structure.
 */
int
iavf_add_mac_filter(struct iavf_sc *sc, u8 *macaddr, u16 flags)
{
	struct iavf_mac_filter	*f;

	/* Does one already exist? */
	f = iavf_find_mac_filter(sc, macaddr);
	if (f != NULL) {
		iavf_dbg_filter(sc, "exists: " MAC_FORMAT "\n",
		    MAC_FORMAT_ARGS(macaddr));
		return (EEXIST);
	}

	/* If not, get a new empty filter */
	f = iavf_get_mac_filter(sc);
	if (f == NULL) {
		device_printf(sc->dev, "%s: no filters available!!\n",
		    __func__);
		return (ENOMEM);
	}

	iavf_dbg_filter(sc, "marked: " MAC_FORMAT "\n",
	    MAC_FORMAT_ARGS(macaddr));

	bcopy(macaddr, f->macaddr, ETHER_ADDR_LEN);
	f->flags |= (IAVF_FILTER_ADD | IAVF_FILTER_USED);
	f->flags |= flags;
	return (0);
}

/**
 * iavf_find_mac_filter - Find a MAC filter with the given address
 * @sc: device private softc
 * @macaddr: the MAC address to find
 *
 * Finds the filter structure in the MAC filter list with the corresponding
 * MAC address.
 *
 * @returns a pointer to the filter structure, or NULL if no such filter
 * exists in the list yet.
 */
struct iavf_mac_filter *
iavf_find_mac_filter(struct iavf_sc *sc, u8 *macaddr)
{
	struct iavf_mac_filter	*f;
	bool match = FALSE;

	SLIST_FOREACH(f, sc->mac_filters, next) {
		if (cmp_etheraddr(f->macaddr, macaddr)) {
			match = TRUE;
			break;
		}
	}

	if (!match)
		f = NULL;
	return (f);
}

/**
 * iavf_get_mac_filter - Get a new MAC address filter
 * @sc: device private softc
 *
 * Allocates a new filter structure and inserts it into the MAC filter list.
 *
 * @post the caller must fill in the structure details after calling this
 * function, but does not need to insert it into the linked list.
 *
 * @returns a pointer to the new filter structure, or NULL of we failed to
 * allocate it.
 */
struct iavf_mac_filter *
iavf_get_mac_filter(struct iavf_sc *sc)
{
	struct iavf_mac_filter *f;

	f = (struct iavf_mac_filter *)malloc(sizeof(struct iavf_mac_filter),
	    M_IAVF, M_NOWAIT | M_ZERO);
	if (f)
		SLIST_INSERT_HEAD(sc->mac_filters, f, next);

	return (f);
}

/**
 * iavf_baudrate_from_link_speed - Convert link speed to baudrate
 * @sc: device private softc
 *
 * @post The link_speed_adv field is in Mbps, so it is multipled by
 * 1,000,000 before it's returned.
 *
 * @returns the adapter link speed in bits/sec
 */
u64
iavf_baudrate_from_link_speed(struct iavf_sc *sc)
{
	if (sc->vf_res->vf_cap_flags & VIRTCHNL_VF_CAP_ADV_LINK_SPEED)
		return (sc->link_speed_adv * IAVF_ADV_LINK_SPEED_SCALE);
	else
		return iavf_max_vc_speed_to_value(sc->link_speed);
}

/**
 * iavf_add_vlan_filter - Add a VLAN filter to the softc VLAN list
 * @sc: device private softc
 * @vtag: the VLAN id to filter
 *
 * Allocate a new VLAN filter structure and insert it into the VLAN list.
 */
void
iavf_add_vlan_filter(struct iavf_sc *sc, u16 vtag)
{
	struct iavf_vlan_filter	*v;

	v = (struct iavf_vlan_filter *)malloc(sizeof(struct iavf_vlan_filter),
	    M_IAVF, M_WAITOK | M_ZERO);
	SLIST_INSERT_HEAD(sc->vlan_filters, v, next);
	v->vlan = vtag;
	v->flags = IAVF_FILTER_ADD;
}

/**
 * iavf_mark_del_vlan_filter - Mark a given VLAN id for deletion
 * @sc: device private softc
 * @vtag: the VLAN id to delete
 *
 * Marks all VLAN filters matching the given vtag for deletion.
 *
 * @returns the number of filters marked for deletion.
 *
 * @remark the filters are not removed immediately, but will be removed from
 * the list by another function that synchronizes over the virtchnl interface.
 */
int
iavf_mark_del_vlan_filter(struct iavf_sc *sc, u16 vtag)
{
	struct iavf_vlan_filter	*v;
	int i = 0;

	SLIST_FOREACH(v, sc->vlan_filters, next) {
		if (v->vlan == vtag) {
			v->flags = IAVF_FILTER_DEL;
			++i;
		}
	}

	return (i);
}

/**
 * iavf_update_msix_devinfo - Fix MSIX values for pci_msix_count()
 * @dev: pointer to kernel device
 *
 * Fix cached MSI-X control register information. This is a workaround
 * for an issue where VFs spawned in non-passthrough mode on FreeBSD
 * will have their PCI information cached before the PF driver
 * finishes updating their PCI information.
 *
 * @pre Must be called before pci_msix_count()
 */
void
iavf_update_msix_devinfo(device_t dev)
{
	struct pci_devinfo *dinfo;
	u32 msix_ctrl;

	dinfo = (struct pci_devinfo *)device_get_ivars(dev);
	/* We can hardcode this offset since we know the device */
	msix_ctrl = pci_read_config(dev, 0x70 + PCIR_MSIX_CTRL, 2);
	dinfo->cfg.msix.msix_ctrl = msix_ctrl;
	dinfo->cfg.msix.msix_msgnum = (msix_ctrl & PCIM_MSIXCTRL_TABLE_SIZE) + 1;
}

/**
 * iavf_disable_queues_with_retries - Send PF multiple DISABLE_QUEUES messages
 * @sc: device softc
 *
 * Send a virtual channel message to the PF to DISABLE_QUEUES, but resend it up
 * to IAVF_MAX_DIS_Q_RETRY times if the response says that it wasn't
 * successful. This is intended to workaround a bug that can appear on the PF.
 */
void
iavf_disable_queues_with_retries(struct iavf_sc *sc)
{
	bool in_detach = iavf_driver_is_detaching(sc);
	int max_attempts = IAVF_MAX_DIS_Q_RETRY;
	int msg_count = 0;

	/* While the driver is detaching, it doesn't care if the queue
	 * disable finishes successfully or not. Just send one message
	 * to just notify the PF driver.
	 */
	if (in_detach)
		max_attempts = 1;

	while ((msg_count < max_attempts) &&
	    atomic_load_acq_32(&sc->queues_enabled)) {
		msg_count++;
		iavf_send_vc_msg_sleep(sc, IAVF_FLAG_AQ_DISABLE_QUEUES);
	}

	/* Possibly print messages about retry attempts and issues */
	if (msg_count > 1)
		iavf_dbg_vc(sc, "DISABLE_QUEUES messages sent: %d\n",
		    msg_count);

	if (!in_detach && msg_count >= max_attempts)
		device_printf(sc->dev, "%s: DISABLE_QUEUES may have failed\n",
		    __func__);
}
