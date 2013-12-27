/*-
 * Copyright (c) 2011
 *	Ben Gray <ben.r.gray@gmail.com>.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/**
 * Driver for the High Speed USB EHCI module on the TI OMAP3530 SoC.
 *
 * WARNING: I've only tried this driver on a limited number of USB peripherals,
 * it is still very raw and bound to have numerous bugs in it.
 *
 * This driver is based on the FreeBSD IXP4xx EHCI driver with a lot of the
 * setup sequence coming from the Linux community and their EHCI driver for
 * OMAP.  Without these as a base I don't think I would have been able to get
 * this driver working.
 *
 * The driver only contains the EHCI parts, the module also supports OHCI and
 * USB on-the-go (OTG), currently neither are supported.
 *
 * CAUTION: This driver was written to run on the beaglebaord dev board, so I
 * have made some assumptions about the type of PHY used and some of the other
 * settings.  Bare that in mind if you intend to use this driver on another
 * platform.
 *
 * NOTE: This module uses a few different clocks, one being a 60Mhz clock for
 * the TTL part of the module.  This clock is derived from DPPL5 which must be
 * configured prior to loading this driver - it is not configured by the
 * bootloader.  It took me a long time to figure this out, and caused much
 * frustration.  This PLL is now setup in the timer/clocks part of the BSP,
 * check out the omap_prcm_setup_dpll5() function in omap_prcm.c for more info.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_bus.h"

#include <sys/stdint.h>
#include <sys/stddef.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/linker_set.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/sysctl.h>
#include <sys/sx.h>
#include <sys/unistd.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/priv.h>
#include <sys/gpio.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>

#include <dev/usb/usb_core.h>
#include <dev/usb/usb_busdma.h>
#include <dev/usb/usb_process.h>
#include <dev/usb/usb_util.h>

#include <dev/usb/usb_controller.h>
#include <dev/usb/usb_bus.h>
#include <dev/usb/controller/ehci.h>
#include <dev/usb/controller/ehcireg.h>

#include <arm/ti/tivar.h>
#include <arm/ti/ti_prcm.h>
#include <arm/ti/ti_scm.h>

#include <arm/ti/usb/omap_usb.h>

#include "gpio_if.h"

struct omap_ehci_softc {
	ehci_softc_t        base;	/* storage for EHCI code */

	device_t            sc_dev;
	device_t            sc_gpio_dev;

	/* TLL register set */
	struct resource*    tll_mem_res;

	/* UHH register set */
	struct resource*    uhh_mem_res;

	/* The revision of the HS USB HOST read from UHH_REVISION */
	uint32_t            ehci_rev;

	/* The following details are provided by conf hints */
	int                 port_mode[3];
	int                 phy_reset[3];
	int                 reset_gpio_pin[3];
};

static device_attach_t omap_ehci_attach;
static device_detach_t omap_ehci_detach;
static device_shutdown_t omap_ehci_shutdown;
static device_suspend_t omap_ehci_suspend;
static device_resume_t omap_ehci_resume;

/**
 *	omap_tll_read_4 - read a 32-bit value from the USBTLL registers
 *	omap_tll_write_4 - write a 32-bit value from the USBTLL registers
 *	omap_tll_readb - read an 8-bit value from the USBTLL registers
 *	omap_tll_writeb - write an 8-bit value from the USBTLL registers
 *	@sc: omap ehci device context
 *	@off: byte offset within the register set to read from
 *	@val: the value to write into the register
 *	
 *
 *	LOCKING:
 *	None
 *
 *	RETURNS:
 *	nothing in case of write function, if read function returns the value read.
 */
static inline uint32_t
omap_tll_read_4(struct omap_ehci_softc *sc, bus_size_t off)
{
	return bus_read_4(sc->tll_mem_res, off);
}

static inline void
omap_tll_write_4(struct omap_ehci_softc *sc, bus_size_t off, uint32_t val)
{
	bus_write_4(sc->tll_mem_res, off, val);
}

static inline uint8_t
omap_tll_readb(struct omap_ehci_softc *sc, bus_size_t off)
{
	return bus_read_1(sc->tll_mem_res, off);
}

static inline void
omap_tll_writeb(struct omap_ehci_softc *sc, bus_size_t off, uint8_t val)
{
	bus_write_1(sc->tll_mem_res, off, val);
}

/**
 *	omap_ehci_read_4 - read a 32-bit value from the EHCI registers
 *	omap_ehci_write_4 - write a 32-bit value from the EHCI registers
 *	@sc: omap ehci device context
 *	@off: byte offset within the register set to read from
 *	@val: the value to write into the register
 *	
 *
 *	LOCKING:
 *	None
 *
 *	RETURNS:
 *	nothing in case of write function, if read function returns the value read.
 */
static inline uint32_t
omap_ehci_read_4(struct omap_ehci_softc *sc, bus_size_t off)
{
	return (bus_read_4(sc->base.sc_io_res, off));
}
static inline void
omap_ehci_write_4(struct omap_ehci_softc *sc, bus_size_t off, uint32_t val)
{
	bus_write_4(sc->base.sc_io_res, off, val);
}

/**
 *	omap_uhh_read_4 - read a 32-bit value from the UHH registers
 *	omap_uhh_write_4 - write a 32-bit value from the UHH registers
 *	@sc: omap ehci device context
 *	@off: byte offset within the register set to read from
 *	@val: the value to write into the register
 *	
 *
 *	LOCKING:
 *	None
 *
 *	RETURNS:
 *	nothing in case of write function, if read function returns the value read.
 */
static inline uint32_t
omap_uhh_read_4(struct omap_ehci_softc *sc, bus_size_t off)
{
	return bus_read_4(sc->uhh_mem_res, off);
}
static inline void
omap_uhh_write_4(struct omap_ehci_softc *sc, bus_size_t off, uint32_t val)
{
	bus_write_4(sc->uhh_mem_res, off, val);
}

/**
 *	omap_ehci_utmi_init - initialises the UTMI part of the controller
 *	@isc: omap ehci device context
 *
 *	
 *
 *	LOCKING:
 *	none
 *
 *	RETURNS:
 *	nothing
 */
static void
omap_ehci_utmi_init(struct omap_ehci_softc *isc, unsigned int en_mask)
{
	unsigned int i;
	uint32_t reg;
	
	/* There are 3 TLL channels, one per USB controller so set them all up the
	 * same, SDR mode, bit stuffing and no autoidle.
	 */
	for (i=0; i<3; i++) {
		reg = omap_tll_read_4(isc, OMAP_USBTLL_TLL_CHANNEL_CONF(i));
		
		reg &= ~(TLL_CHANNEL_CONF_UTMIAUTOIDLE
				 | TLL_CHANNEL_CONF_ULPINOBITSTUFF
				 | TLL_CHANNEL_CONF_ULPIDDRMODE);
		
		omap_tll_write_4(isc, OMAP_USBTLL_TLL_CHANNEL_CONF(i), reg);
	}
	
	/* Program the common TLL register */
	reg = omap_tll_read_4(isc, OMAP_USBTLL_TLL_SHARED_CONF);

	reg &= ~( TLL_SHARED_CONF_USB_90D_DDR_EN
			| TLL_SHARED_CONF_USB_DIVRATIO_MASK);
	reg |=  ( TLL_SHARED_CONF_FCLK_IS_ON
			| TLL_SHARED_CONF_USB_DIVRATIO_2
			| TLL_SHARED_CONF_USB_180D_SDR_EN);
	
	omap_tll_write_4(isc, OMAP_USBTLL_TLL_SHARED_CONF, reg);
	
	/* Enable channels now */
	for (i = 0; i < 3; i++) {
		reg = omap_tll_read_4(isc, OMAP_USBTLL_TLL_CHANNEL_CONF(i));
		
		/* Enable only the reg that is needed */
		if ((en_mask & (1 << i)) == 0)
			continue;
		
		reg |= TLL_CHANNEL_CONF_CHANEN;
		omap_tll_write_4(isc, OMAP_USBTLL_TLL_CHANNEL_CONF(i), reg);
	}
}

/**
 *	omap_ehci_soft_phy_reset - resets the phy using the reset command
 *	@isc: omap ehci device context
 *	@port: port to send the reset over
 *	
 *
 *	LOCKING:
 *	none
 *
 *	RETURNS:
 *	nothing
 */
static void 
omap_ehci_soft_phy_reset(struct omap_ehci_softc *isc, unsigned int port)
{
	unsigned long timeout = (hz < 10) ? 1 : ((100 * hz) / 1000);
	uint32_t reg;

	reg = ULPI_FUNC_CTRL_RESET
		/* FUNCTION_CTRL_SET register */
		| (ULPI_SET(ULPI_FUNC_CTRL) << OMAP_USBHOST_INSNREG05_ULPI_REGADD_SHIFT)
		/* Write */
		| (2 << OMAP_USBHOST_INSNREG05_ULPI_OPSEL_SHIFT)
		/* PORTn */
		| ((port + 1) << OMAP_USBHOST_INSNREG05_ULPI_PORTSEL_SHIFT)
		/* start ULPI access*/
		| (1 << OMAP_USBHOST_INSNREG05_ULPI_CONTROL_SHIFT);

	omap_ehci_write_4(isc, OMAP_USBHOST_INSNREG05_ULPI, reg);

	/* Wait for ULPI access completion */
	while ((omap_ehci_read_4(isc, OMAP_USBHOST_INSNREG05_ULPI)
	       & (1 << OMAP_USBHOST_INSNREG05_ULPI_CONTROL_SHIFT))) {

		/* Sleep for a tick */
		pause("USBPHY_RESET", 1);
		
		if (timeout-- == 0) {
			device_printf(isc->sc_dev, "PHY reset operation timed out\n");
			break;
		}
	}
}
 

/**
 *	omap_ehci_init - initialises the USB host EHCI controller
 *	@isc: omap ehci device context
 *
 *	This initialisation routine is quite heavily based on the work done by the
 *	OMAP Linux team (for which I thank them very much).  The init sequence is
 *	almost identical, diverging only for the FreeBSD specifics.
 *
 *	LOCKING:
 *	none
 *
 *	RETURNS:
 *	0 on success, a negative error code on failure.
 */
static int
omap_ehci_init(struct omap_ehci_softc *isc)
{
	unsigned long timeout;
	int ret = 0;
	uint8_t tll_ch_mask = 0;
	uint32_t reg = 0;
	int reset_performed = 0;
	int i;
	
	device_printf(isc->sc_dev, "Starting TI EHCI USB Controller\n");
	
	
	/* Enable Clocks for high speed USBHOST */
	ti_prcm_clk_enable(USBHSHOST_CLK);
	
	/* Hold the PHY in reset while configuring */
	for (int i = 0; i < 3; i++) {
		if (isc->phy_reset[i]) {
			/* Configure the GPIO to drive low (hold in reset) */
			if ((isc->reset_gpio_pin[i] != -1) && (isc->sc_gpio_dev != NULL)) {
				GPIO_PIN_SETFLAGS(isc->sc_gpio_dev, isc->reset_gpio_pin[i],
				    GPIO_PIN_OUTPUT);
				GPIO_PIN_SET(isc->sc_gpio_dev, isc->reset_gpio_pin[i],
				    GPIO_PIN_LOW);
				reset_performed = 1;
			}
		}
	}

	/* Hold the PHY in RESET for enough time till DIR is high */
	if (reset_performed)
		DELAY(10);

	/* Read the UHH revision */
	isc->ehci_rev = omap_uhh_read_4(isc, OMAP_USBHOST_UHH_REVISION);
	device_printf(isc->sc_dev, "UHH revision 0x%08x\n", isc->ehci_rev);
	
	/* Initilise the low level interface module(s) */
	if (isc->ehci_rev == OMAP_EHCI_REV1) {

		/* Enable the USB TLL */
		ti_prcm_clk_enable(USBTLL_CLK);

		/* Perform TLL soft reset, and wait until reset is complete */
		omap_tll_write_4(isc, OMAP_USBTLL_SYSCONFIG, TLL_SYSCONFIG_SOFTRESET);
	
		/* Set the timeout to 100ms*/
		timeout = (hz < 10) ? 1 : ((100 * hz) / 1000);

		/* Wait for TLL reset to complete */
		while ((omap_tll_read_4(isc, OMAP_USBTLL_SYSSTATUS) & 
		        TLL_SYSSTATUS_RESETDONE) == 0x00) {

			/* Sleep for a tick */
			pause("USBRESET", 1);
		
			if (timeout-- == 0) {
				device_printf(isc->sc_dev, "TLL reset operation timed out\n");
				ret = EINVAL;
				goto err_sys_status;
			}
		}
	
		device_printf(isc->sc_dev, "TLL RESET DONE\n");
		
		/* CLOCKACTIVITY = 1 : OCP-derived internal clocks ON during idle
		 * SIDLEMODE = 2     : Smart-idle mode. Sidleack asserted after Idlereq
		 *                     assertion when no more activity on the USB.
		 * ENAWAKEUP = 1     : Wakeup generation enabled
		 */
		omap_tll_write_4(isc, OMAP_USBTLL_SYSCONFIG, TLL_SYSCONFIG_ENAWAKEUP |
		                                            TLL_SYSCONFIG_AUTOIDLE |
		                                            TLL_SYSCONFIG_SIDLE_SMART_IDLE |
		                                            TLL_SYSCONFIG_CACTIVITY);

	} else if (isc->ehci_rev == OMAP_EHCI_REV2) {
	
		/* For OMAP44xx devices you have to enable the per-port clocks:
		 *  PHY_MODE  - External ULPI clock
		 *  TTL_MODE  - Internal UTMI clock
		 *  HSIC_MODE - Internal 480Mhz and 60Mhz clocks
		 */
		if (isc->ehci_rev == OMAP_EHCI_REV2) {
			if (isc->port_mode[0] == EHCI_HCD_OMAP_MODE_PHY) {
				ti_prcm_clk_set_source(USBP1_PHY_CLK, EXT_CLK);
				ti_prcm_clk_enable(USBP1_PHY_CLK);
			} else if (isc->port_mode[0] == EHCI_HCD_OMAP_MODE_TLL)
				ti_prcm_clk_enable(USBP1_UTMI_CLK);
			else if (isc->port_mode[0] == EHCI_HCD_OMAP_MODE_HSIC)
				ti_prcm_clk_enable(USBP1_HSIC_CLK);

			if (isc->port_mode[1] == EHCI_HCD_OMAP_MODE_PHY) {
				ti_prcm_clk_set_source(USBP2_PHY_CLK, EXT_CLK);
				ti_prcm_clk_enable(USBP2_PHY_CLK);
			} else if (isc->port_mode[1] == EHCI_HCD_OMAP_MODE_TLL)
				ti_prcm_clk_enable(USBP2_UTMI_CLK);
			else if (isc->port_mode[1] == EHCI_HCD_OMAP_MODE_HSIC)
				ti_prcm_clk_enable(USBP2_HSIC_CLK);
		}
	}

	/* Put UHH in SmartIdle/SmartStandby mode */
	reg = omap_uhh_read_4(isc, OMAP_USBHOST_UHH_SYSCONFIG);
	if (isc->ehci_rev == OMAP_EHCI_REV1) {
		reg &= ~(UHH_SYSCONFIG_SIDLEMODE_MASK |
		         UHH_SYSCONFIG_MIDLEMODE_MASK);
		reg |= (UHH_SYSCONFIG_ENAWAKEUP |
		        UHH_SYSCONFIG_AUTOIDLE |
		        UHH_SYSCONFIG_CLOCKACTIVITY |
		        UHH_SYSCONFIG_SIDLEMODE_SMARTIDLE |
		        UHH_SYSCONFIG_MIDLEMODE_SMARTSTANDBY);
	} else if (isc->ehci_rev == OMAP_EHCI_REV2) {
		reg &= ~UHH_SYSCONFIG_IDLEMODE_MASK;
		reg |=  UHH_SYSCONFIG_IDLEMODE_NOIDLE;
		reg &= ~UHH_SYSCONFIG_STANDBYMODE_MASK;
		reg |=  UHH_SYSCONFIG_STANDBYMODE_NOSTDBY;
	}
	omap_uhh_write_4(isc, OMAP_USBHOST_UHH_SYSCONFIG, reg);
	device_printf(isc->sc_dev, "OMAP_UHH_SYSCONFIG: 0x%08x\n", reg);

	reg = omap_uhh_read_4(isc, OMAP_USBHOST_UHH_HOSTCONFIG);
	
	/* Setup ULPI bypass and burst configurations */
	reg |= (UHH_HOSTCONFIG_ENA_INCR4 |
			UHH_HOSTCONFIG_ENA_INCR8 |
			UHH_HOSTCONFIG_ENA_INCR16);
	reg &= ~UHH_HOSTCONFIG_ENA_INCR_ALIGN;
	
	if (isc->ehci_rev == OMAP_EHCI_REV1) {
		if (isc->port_mode[0] == EHCI_HCD_OMAP_MODE_UNKNOWN)
			reg &= ~UHH_HOSTCONFIG_P1_CONNECT_STATUS;
		if (isc->port_mode[1] == EHCI_HCD_OMAP_MODE_UNKNOWN)
			reg &= ~UHH_HOSTCONFIG_P2_CONNECT_STATUS;
		if (isc->port_mode[2] == EHCI_HCD_OMAP_MODE_UNKNOWN)
			reg &= ~UHH_HOSTCONFIG_P3_CONNECT_STATUS;
	
		/* Bypass the TLL module for PHY mode operation */
		if ((isc->port_mode[0] == EHCI_HCD_OMAP_MODE_PHY) ||
		    (isc->port_mode[1] == EHCI_HCD_OMAP_MODE_PHY) ||
		    (isc->port_mode[2] == EHCI_HCD_OMAP_MODE_PHY))
			reg &= ~UHH_HOSTCONFIG_P1_ULPI_BYPASS;
		else
			reg |= UHH_HOSTCONFIG_P1_ULPI_BYPASS;
			
	} else if (isc->ehci_rev == OMAP_EHCI_REV2) {
		reg |=  UHH_HOSTCONFIG_APP_START_CLK;
		
		/* Clear port mode fields for PHY mode*/
		reg &= ~UHH_HOSTCONFIG_P1_MODE_MASK;
		reg &= ~UHH_HOSTCONFIG_P2_MODE_MASK;

		if (isc->port_mode[0] == EHCI_HCD_OMAP_MODE_TLL)
			reg |= UHH_HOSTCONFIG_P1_MODE_UTMI_PHY;
		else if (isc->port_mode[0] == EHCI_HCD_OMAP_MODE_HSIC)
			reg |= UHH_HOSTCONFIG_P1_MODE_HSIC;

		if (isc->port_mode[1] == EHCI_HCD_OMAP_MODE_TLL)
			reg |= UHH_HOSTCONFIG_P2_MODE_UTMI_PHY;
		else if (isc->port_mode[1] == EHCI_HCD_OMAP_MODE_HSIC)
			reg |= UHH_HOSTCONFIG_P2_MODE_HSIC;
	}

	omap_uhh_write_4(isc, OMAP_USBHOST_UHH_HOSTCONFIG, reg);
	device_printf(isc->sc_dev, "UHH setup done, uhh_hostconfig=0x%08x\n", reg);
	

	/* I found the code and comments in the Linux EHCI driver - thanks guys :)
	 *
	 * "An undocumented "feature" in the OMAP3 EHCI controller, causes suspended
	 * ports to be taken out of suspend when the USBCMD.Run/Stop bit is cleared
	 * (for example when we do ehci_bus_suspend). This breaks suspend-resume if
	 * the root-hub is allowed to suspend. Writing 1 to this undocumented
	 * register bit disables this feature and restores normal behavior."
	 */
#if 0
	omap_ehci_write_4(isc, OMAP_USBHOST_INSNREG04,
	                 OMAP_USBHOST_INSNREG04_DISABLE_UNSUSPEND);
#endif

	/* If any of the ports are configured in TLL mode, enable them */
	if ((isc->port_mode[0] == EHCI_HCD_OMAP_MODE_TLL) ||
		(isc->port_mode[1] == EHCI_HCD_OMAP_MODE_TLL) ||
		(isc->port_mode[2] == EHCI_HCD_OMAP_MODE_TLL)) {
		
		if (isc->port_mode[0] == EHCI_HCD_OMAP_MODE_TLL)
			tll_ch_mask |= 0x1;
		if (isc->port_mode[1] == EHCI_HCD_OMAP_MODE_TLL)
			tll_ch_mask |= 0x2;
		if (isc->port_mode[2] == EHCI_HCD_OMAP_MODE_TLL)
			tll_ch_mask |= 0x4;
		
		/* Enable UTMI mode for required TLL channels */
		omap_ehci_utmi_init(isc, tll_ch_mask);
	}


	/* Release the PHY reset signal now we have configured everything */
	if (reset_performed) {

		/* Delay for 10ms */
		DELAY(10000);
		
		for (i = 0; i < 3; i++) {
			/* Release reset */
	
			if (isc->phy_reset[i] && (isc->reset_gpio_pin[i] != -1) 
			    && (isc->sc_gpio_dev != NULL)) {
				GPIO_PIN_SET(isc->sc_gpio_dev, 
					isc->reset_gpio_pin[i], GPIO_PIN_HIGH);
			}
		}
	}

	/* Set the interrupt threshold control, it controls the maximum rate at
	 * which the host controller issues interrupts.  We set it to 1 microframe
	 * at startup - the default is 8 mircoframes (equates to 1ms).
	 */
	reg = omap_ehci_read_4(isc, OMAP_USBHOST_USBCMD);
	reg &= 0xff00ffff;
	reg |= (1 << 16);
	omap_ehci_write_4(isc, OMAP_USBHOST_USBCMD, reg);

	/* Soft reset the PHY using PHY reset command over ULPI */
	if (isc->port_mode[0] == EHCI_HCD_OMAP_MODE_PHY)
		omap_ehci_soft_phy_reset(isc, 0);
	if (isc->port_mode[1] == EHCI_HCD_OMAP_MODE_PHY)
		omap_ehci_soft_phy_reset(isc, 1);	

	return(0);

err_sys_status:
	
	/* Disable the TLL clocks */
	ti_prcm_clk_disable(USBTLL_CLK);
	
	/* Disable Clocks for USBHOST */
	ti_prcm_clk_disable(USBHSHOST_CLK);

	return(ret);
}


/**
 *	omap_ehci_fini - shutdown the EHCI controller
 *	@isc: omap ehci device context
 *
 *	
 *
 *	LOCKING:
 *	none
 *
 *	RETURNS:
 *	0 on success, a negative error code on failure.
 */
static void
omap_ehci_fini(struct omap_ehci_softc *isc)
{
	unsigned long timeout;
	
	device_printf(isc->sc_dev, "Stopping TI EHCI USB Controller\n");
	
	/* Set the timeout */
	if (hz < 10)
		timeout = 1;
	else
		timeout = (100 * hz) / 1000;

	/* Reset the UHH, OHCI and EHCI modules */
	omap_uhh_write_4(isc, OMAP_USBHOST_UHH_SYSCONFIG, 0x0002);
	while ((omap_uhh_read_4(isc, OMAP_USBHOST_UHH_SYSSTATUS) & 0x07) == 0x00) {
		/* Sleep for a tick */
		pause("USBRESET", 1);
		
		if (timeout-- == 0) {
			device_printf(isc->sc_dev, "operation timed out\n");
			break;
		}
	}
	

	/* Set the timeout */
	if (hz < 10)
		timeout = 1;
	else
		timeout = (100 * hz) / 1000;

	/* Reset the TLL module */
	omap_tll_write_4(isc, OMAP_USBTLL_SYSCONFIG, 0x0002);
	while ((omap_tll_read_4(isc, OMAP_USBTLL_SYSSTATUS) & (0x01)) == 0x00) {
		/* Sleep for a tick */
		pause("USBRESET", 1);
		
		if (timeout-- == 0) {
			device_printf(isc->sc_dev, "operation timed out\n");
			break;
		}
	}


	/* Disable functional and interface clocks for the TLL and HOST modules */
	ti_prcm_clk_disable(USBTLL_CLK);
	ti_prcm_clk_disable(USBHSHOST_CLK);

	device_printf(isc->sc_dev, "Clock to USB host has been disabled\n");
	
}



/**
 *	omap_ehci_suspend - suspends the bus
 *	@dev: omap ehci device
 *	
 *	Effectively boilerplate EHCI suspend code.
 *
 *	TODO: There is a lot more we could do here - i.e. force the controller into
 *	idle mode and disable all the clocks for start.
 *
 *	LOCKING:
 *	none
 *
 *	RETURNS:
 *	0 on success or a positive error code
 */
static int
omap_ehci_suspend(device_t dev)
{
	int err;
	
	err = bus_generic_suspend(dev);
	if (err)
		return (err);
	return (0);
}


/**
 *	omap_ehci_resume - resumes a suspended bus
 *	@dev: omap ehci device
 *	
 *	Effectively boilerplate EHCI resume code.
 *
 *	LOCKING:
 *	none
 *
 *	RETURNS:
 *	0 on success or a positive error code on failure
 */
static int
omap_ehci_resume(device_t dev)
{
	
	bus_generic_resume(dev);
	
	return (0);
}


/**
 *	omap_ehci_shutdown - starts the given command
 *	@dev: 
 *	
 *	Effectively boilerplate EHCI shutdown code.
 *
 *	LOCKING:
 *	none.
 *
 *	RETURNS:
 *	0 on success or a positive error code on failure
 */
static int
omap_ehci_shutdown(device_t dev)
{
	int err;
	
	err = bus_generic_shutdown(dev);
	if (err)
		return (err);
	
	return (0);
}


/**
 *	omap_ehci_probe - starts the given command
 *	@dev: 
 *	
 *	Effectively boilerplate EHCI resume code.
 *
 *	LOCKING:
 *	Caller should be holding the OMAP3_MMC lock.
 *
 *	RETURNS:
 *	EH_HANDLED or EH_NOT_HANDLED
 */
static int
omap_ehci_probe(device_t dev)
{
	if (!ofw_bus_is_compatible(dev, "ti,usb-ehci"))
		return (ENXIO);

	device_set_desc(dev, OMAP_EHCI_HC_DEVSTR);
	
	return (BUS_PROBE_DEFAULT);
}

/**
 *	omap_ehci_attach - driver entry point, sets up the ECHI controller/driver
 *	@dev: the new device handle
 *	
 *	Sets up bus spaces, interrupt handles, etc for the EHCI controller.  It also
 *	parses the resource hints and calls omap_ehci_init() to initialise the
 *	H/W.
 *
 *	LOCKING:
 *	none
 *
 *	RETURNS:
 *	0 on success or a positive error code on failure.
 */
static int
omap_ehci_attach(device_t dev)
{
	struct omap_ehci_softc *isc = device_get_softc(dev);
	phandle_t node;
	/* 3 ports with 3 cells per port */
	pcell_t phyconf[3 * 3];
	pcell_t *phyconf_ptr;
	ehci_softc_t *sc = &isc->base;
	int err;
	int rid;
	int len, tuple_size;
	int i;

	/* initialise some bus fields */
	sc->sc_bus.parent = dev;
	sc->sc_bus.devices = sc->sc_devices;
	sc->sc_bus.devices_max = EHCI_MAX_DEVICES;
	
	/* save the device */
	isc->sc_dev = dev;
	
	/* get all DMA memory */
	if (usb_bus_mem_alloc_all(&sc->sc_bus, USB_GET_DMA_TAG(dev),
	                          &ehci_iterate_hw_softc)) {
		return (ENOMEM);
	}
	
	/* When the EHCI driver is added to the tree it is expected that 3
	 * memory resources and 1 interrupt resource is assigned. The memory
	 * resources should be:
	 *   0 => EHCI register range
	 *   1 => UHH register range
	 *   2 => TLL register range
	 *
	 * The interrupt resource is just the single interupt for the controller.
	 */

	/* Allocate resource for the EHCI register set */
	rid = 0;
	sc->sc_io_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (!sc->sc_io_res) {
		device_printf(dev, "Error: Could not map EHCI memory\n");
		goto error;
	}
	/* Request an interrupt resource */
	rid = 0;
	sc->sc_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid, RF_ACTIVE);
	if (sc->sc_irq_res == NULL) {
		device_printf(dev, "Error: could not allocate irq\n");
		goto error;
	}

	/* Allocate resource for the UHH register set */
	rid = 1;
	isc->uhh_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (!isc->uhh_mem_res) {
		device_printf(dev, "Error: Could not map UHH memory\n");
		goto error;
	}
	/* Allocate resource for the TLL register set */
	rid = 2;
	isc->tll_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (!isc->tll_mem_res) {
		device_printf(dev, "Error: Could not map TLL memory\n");
		goto error;
	}
	
	/* Add this device as a child of the USBus device */
	sc->sc_bus.bdev = device_add_child(dev, "usbus", -1);
	if (!sc->sc_bus.bdev) {
		device_printf(dev, "Error: could not add USB device\n");
		goto error;
	}

	device_set_ivars(sc->sc_bus.bdev, &sc->sc_bus);
	device_set_desc(sc->sc_bus.bdev, OMAP_EHCI_HC_DEVSTR);
	
	/* Set the vendor name */
	sprintf(sc->sc_vendor, "Texas Instruments");
	
	/* Get the GPIO device, we may need this if the driver needs to toggle
	 * some pins for external PHY resets.
	 */
	isc->sc_gpio_dev = devclass_get_device(devclass_find("gpio"), 0);
	if (isc->sc_gpio_dev == NULL) {
		device_printf(dev, "Error: failed to get the GPIO device\n");
		goto error;
	}
	
	/* Set the defaults for the hints */
	for (i = 0; i < 3; i++) {
		isc->phy_reset[i] = 0;
		isc->port_mode[i] = EHCI_HCD_OMAP_MODE_UNKNOWN;
		isc->reset_gpio_pin[i] = -1;
	}

	tuple_size = sizeof(pcell_t) * 3;
	node = ofw_bus_get_node(dev);
	len = OF_getprop(node, "phy-config", phyconf, sizeof(phyconf));
	if (len > 0) {
		if (len % tuple_size)
			goto error;
		if ((len / tuple_size) != 3)
			goto error;

		phyconf_ptr = phyconf;
		for (i = 0; i < 3; i++) {
			isc->port_mode[i] = fdt32_to_cpu(*phyconf_ptr);
			isc->phy_reset[i] = fdt32_to_cpu(*(phyconf_ptr + 1));
			isc->reset_gpio_pin[i] = fdt32_to_cpu(*(phyconf_ptr + 2));

			phyconf_ptr += 3;
		}
	}
	
	/* Initialise the ECHI registers */
	err = omap_ehci_init(isc);
	if (err) {
		device_printf(dev, "Error: could not setup OMAP EHCI, %d\n", err);
		goto error;
	}
		

	/* Set the tag and size of the register set in the EHCI context */
	sc->sc_io_hdl = rman_get_bushandle(sc->sc_io_res);
	sc->sc_io_tag = rman_get_bustag(sc->sc_io_res);
	sc->sc_io_size = rman_get_size(sc->sc_io_res);


	/* Setup the interrupt */
	err = bus_setup_intr(dev, sc->sc_irq_res, INTR_TYPE_BIO | INTR_MPSAFE,
						 NULL, (driver_intr_t *)ehci_interrupt, sc, &sc->sc_intr_hdl);
	if (err) {
		device_printf(dev, "Error: could not setup irq, %d\n", err);
		sc->sc_intr_hdl = NULL;
		goto error;
	}
	
	
	/* Finally we are ready to kick off the ECHI host controller */
	err = ehci_init(sc);
	if (err == 0) {
		err = device_probe_and_attach(sc->sc_bus.bdev);
	}
	if (err) {
		device_printf(dev, "Error: USB init failed err=%d\n", err);
		goto error;
	}
	
	return (0);
	
error:
	omap_ehci_detach(dev);
	return (ENXIO);
}

/**
 *	omap_ehci_detach - detach the device and cleanup the driver
 *	@dev: device handle
 *	
 *	Clean-up routine where everything initialised in omap_ehci_attach is
 *	freed and cleaned up.  This function calls omap_ehci_fini() to shutdown
 *	the on-chip module.
 *
 *	LOCKING:
 *	none
 *
 *	RETURNS:
 *	Always returns 0 (success).
 */
static int
omap_ehci_detach(device_t dev)
{
	struct omap_ehci_softc *isc = device_get_softc(dev);
	ehci_softc_t *sc = &isc->base;
	device_t bdev;
	int err;
	
	if (sc->sc_bus.bdev) {
		bdev = sc->sc_bus.bdev;
		device_detach(bdev);
		device_delete_child(dev, bdev);
	}

	/* during module unload there are lots of children leftover */
	device_delete_children(dev);
	
	/*
	 * disable interrupts that might have been switched on in ehci_init
	 */
	if (sc->sc_io_res) {
		EWRITE4(sc, EHCI_USBINTR, 0);
	}
	
	if (sc->sc_irq_res && sc->sc_intr_hdl) {
		/*
		 * only call ehci_detach() after ehci_init()
		 */
		ehci_detach(sc);
		
		err = bus_teardown_intr(dev, sc->sc_irq_res, sc->sc_intr_hdl);
		if (err)
			device_printf(dev, "Error: could not tear down irq, %d\n", err);
		sc->sc_intr_hdl = NULL;
	}
	
	/* Free the resources stored in the base EHCI handler */
	if (sc->sc_irq_res) {
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sc_irq_res);
		sc->sc_irq_res = NULL;
	}
	if (sc->sc_io_res) {
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->sc_io_res);
		sc->sc_io_res = NULL;
	}

	/* Release the other register set memory maps */
	if (isc->tll_mem_res) {
		bus_release_resource(dev, SYS_RES_MEMORY, 0, isc->tll_mem_res);
		isc->tll_mem_res = NULL;
	}
	if (isc->uhh_mem_res) {
		bus_release_resource(dev, SYS_RES_MEMORY, 0, isc->uhh_mem_res);
		isc->uhh_mem_res = NULL;
	}

	usb_bus_mem_free_all(&sc->sc_bus, &ehci_iterate_hw_softc);
	
	omap_ehci_fini(isc);
	
	return (0);
}

static device_method_t ehci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, omap_ehci_probe),
	DEVMETHOD(device_attach, omap_ehci_attach),
	DEVMETHOD(device_detach, omap_ehci_detach),
	DEVMETHOD(device_suspend, omap_ehci_suspend),
	DEVMETHOD(device_resume, omap_ehci_resume),
	DEVMETHOD(device_shutdown, omap_ehci_shutdown),
	
	/* Bus interface */
	DEVMETHOD(bus_print_child, bus_generic_print_child),
	
	{0, 0}
};

static driver_t ehci_driver = {
	"ehci",
	ehci_methods,
	sizeof(struct omap_ehci_softc),
};

static devclass_t ehci_devclass;

DRIVER_MODULE(ehci, simplebus, ehci_driver, ehci_devclass, 0, 0);
