/*
 * Copyright (c) 2001-2002 by David Brownell
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* this file is part of ehci-hcd.c */

/*-------------------------------------------------------------------------*/

/*
 * EHCI Root Hub ... the nonsharable stuff
 *
 * Registers don't need cpu_to_le32, that happens transparently
 */

/*-------------------------------------------------------------------------*/

static int check_reset_complete (
	struct ehci_hcd	*ehci,
	int		index,
	int		port_status
) {
	if (!(port_status & PORT_CONNECT)) {
		ehci->reset_done [index] = 0;
		return port_status;
	}

	/* if reset finished and it's still not enabled -- handoff */
	if (!(port_status & PORT_PE)) {
		ehci_dbg (ehci, "port %d full speed --> companion\n",
			index + 1);

		// what happens if HCS_N_CC(params) == 0 ?
		port_status |= PORT_OWNER;
		writel (port_status, &ehci->regs->port_status [index]);

	} else
		ehci_dbg (ehci, "port %d high speed\n", index + 1);

	return port_status;
}

/*-------------------------------------------------------------------------*/


/* build "status change" packet (one or two bytes) from HC registers */

static int
ehci_hub_status_data (struct usb_hcd *hcd, char *buf)
{
	struct ehci_hcd	*ehci = hcd_to_ehci (hcd);
	u32		temp, status = 0;
	int		ports, i, retval = 1;
	unsigned long	flags;

	/* init status to no-changes */
	buf [0] = 0;
	ports = HCS_N_PORTS (ehci->hcs_params);
	if (ports > 7) {
		buf [1] = 0;
		retval++;
	}
	
	/* no hub change reports (bit 0) for now (power, ...) */

	/* port N changes (bit N)? */
	spin_lock_irqsave (&ehci->lock, flags);
	for (i = 0; i < ports; i++) {
		temp = readl (&ehci->regs->port_status [i]);
		if (temp & PORT_OWNER) {
			/* don't report this in GetPortStatus */
			if (temp & PORT_CSC) {
				temp &= ~PORT_CSC;
				writel (temp, &ehci->regs->port_status [i]);
			}
			continue;
		}
		if (!(temp & PORT_CONNECT))
			ehci->reset_done [i] = 0;
		if ((temp & (PORT_CSC | PORT_PEC | PORT_OCC)) != 0) {
			if (i < 7)
			    buf [0] |= 1 << (i + 1);
			else
			    buf [1] |= 1 << (i - 7);
			status = STS_PCD;
		}
	}
	spin_unlock_irqrestore (&ehci->lock, flags);
	return status ? retval : 0;
}

/*-------------------------------------------------------------------------*/

static void
ehci_hub_descriptor (
	struct ehci_hcd			*ehci,
	struct usb_hub_descriptor	*desc
) {
	int		ports = HCS_N_PORTS (ehci->hcs_params);
	u16		temp;

	desc->bDescriptorType = 0x29;
	desc->bPwrOn2PwrGood = 10;	/* ehci 1.0, 2.3.9 says 20ms max */
	desc->bHubContrCurrent = 0;

	desc->bNbrPorts = ports;
	temp = 1 + (ports / 8);
	desc->bDescLength = 7 + 2 * temp;

	/* two bitmaps:  ports removable, and usb 1.0 legacy PortPwrCtrlMask */
	memset (&desc->bitmap [0], 0, temp);
	memset (&desc->bitmap [temp], 0xff, temp);

	temp = 0x0008;			/* per-port overcurrent reporting */
	if (HCS_PPC (ehci->hcs_params))
		temp |= 0x0001;		/* per-port power control */
	if (HCS_INDICATOR (ehci->hcs_params))
		temp |= 0x0080;		/* per-port indicators (LEDs) */
	desc->wHubCharacteristics = cpu_to_le16 (temp);
}

/*-------------------------------------------------------------------------*/

static int ehci_hub_control (
	struct usb_hcd	*hcd,
	u16		typeReq,
	u16		wValue,
	u16		wIndex,
	char		*buf,
	u16		wLength
) {
	struct ehci_hcd	*ehci = hcd_to_ehci (hcd);
	int		ports = HCS_N_PORTS (ehci->hcs_params);
	u32		temp, status;
	unsigned long	flags;
	int		retval = 0;

	/*
	 * FIXME:  support SetPortFeatures USB_PORT_FEAT_INDICATOR.
	 * HCS_INDICATOR may say we can change LEDs to off/amber/green.
	 * (track current state ourselves) ... blink for diagnostics,
	 * power, "this is the one", etc.  EHCI spec supports this.
	 */

	spin_lock_irqsave (&ehci->lock, flags);
	switch (typeReq) {
	case ClearHubFeature:
		switch (wValue) {
		case C_HUB_LOCAL_POWER:
		case C_HUB_OVER_CURRENT:
			/* no hub-wide feature/status flags */
			break;
		default:
			goto error;
		}
		break;
	case ClearPortFeature:
		if (!wIndex || wIndex > ports)
			goto error;
		wIndex--;
		temp = readl (&ehci->regs->port_status [wIndex]);
		if (temp & PORT_OWNER)
			break;

		switch (wValue) {
		case USB_PORT_FEAT_ENABLE:
			writel (temp & ~PORT_PE,
				&ehci->regs->port_status [wIndex]);
			break;
		case USB_PORT_FEAT_C_ENABLE:
			writel (temp | PORT_PEC,
				&ehci->regs->port_status [wIndex]);
			break;
		case USB_PORT_FEAT_SUSPEND:
		case USB_PORT_FEAT_C_SUSPEND:
			/* ? */
			break;
		case USB_PORT_FEAT_POWER:
			if (HCS_PPC (ehci->hcs_params))
				writel (temp & ~PORT_POWER,
					&ehci->regs->port_status [wIndex]);
			break;
		case USB_PORT_FEAT_C_CONNECTION:
			writel (temp | PORT_CSC,
				&ehci->regs->port_status [wIndex]);
			break;
		case USB_PORT_FEAT_C_OVER_CURRENT:
			writel (temp | PORT_OCC,
				&ehci->regs->port_status [wIndex]);
			break;
		case USB_PORT_FEAT_C_RESET:
			/* GetPortStatus clears reset */
			break;
		default:
			goto error;
		}
		readl (&ehci->regs->command);	/* unblock posted write */
		break;
	case GetHubDescriptor:
		ehci_hub_descriptor (ehci, (struct usb_hub_descriptor *)
			buf);
		break;
	case GetHubStatus:
		/* no hub-wide feature/status flags */
		memset (buf, 0, 4);
		//cpu_to_le32s ((u32 *) buf);
		break;
	case GetPortStatus:
		if (!wIndex || wIndex > ports)
			goto error;
		wIndex--;
		status = 0;
		temp = readl (&ehci->regs->port_status [wIndex]);

		// wPortChange bits
		if (temp & PORT_CSC)
			status |= 1 << USB_PORT_FEAT_C_CONNECTION;
		if (temp & PORT_PEC)
			status |= 1 << USB_PORT_FEAT_C_ENABLE;
		// USB_PORT_FEAT_C_SUSPEND
		if (temp & PORT_OCC)
			status |= 1 << USB_PORT_FEAT_C_OVER_CURRENT;

		/* whoever resets must GetPortStatus to complete it!! */
		if ((temp & PORT_RESET)
				&& time_after (jiffies,
					ehci->reset_done [wIndex])) {
			status |= 1 << USB_PORT_FEAT_C_RESET;

			/* force reset to complete */
			writel (temp & ~PORT_RESET,
					&ehci->regs->port_status [wIndex]);
			do {
				temp = readl (
					&ehci->regs->port_status [wIndex]);
				udelay (10);
			} while (temp & PORT_RESET);

			/* see what we found out */
			temp = check_reset_complete (ehci, wIndex, temp);
		}

		// don't show wPortStatus if it's owned by a companion hc
		if (!(temp & PORT_OWNER)) {
			if (temp & PORT_CONNECT) {
				status |= 1 << USB_PORT_FEAT_CONNECTION;
				status |= 1 << USB_PORT_FEAT_HIGHSPEED;
			}
			if (temp & PORT_PE)
				status |= 1 << USB_PORT_FEAT_ENABLE;
			if (temp & PORT_SUSPEND)
				status |= 1 << USB_PORT_FEAT_SUSPEND;
			if (temp & PORT_OC)
				status |= 1 << USB_PORT_FEAT_OVER_CURRENT;
			if (temp & PORT_RESET)
				status |= 1 << USB_PORT_FEAT_RESET;
			if (temp & PORT_POWER)
				status |= 1 << USB_PORT_FEAT_POWER;
		}

#ifndef	EHCI_VERBOSE_DEBUG
	if (status & ~0xffff)	/* only if wPortChange is interesting */
#endif
		dbg_port (ehci, "GetStatus", wIndex + 1, temp);
		// we "know" this alignment is good, caller used kmalloc()...
		*((u32 *) buf) = cpu_to_le32 (status);
		break;
	case SetHubFeature:
		switch (wValue) {
		case C_HUB_LOCAL_POWER:
		case C_HUB_OVER_CURRENT:
			/* no hub-wide feature/status flags */
			break;
		default:
			goto error;
		}
		break;
	case SetPortFeature:
		if (!wIndex || wIndex > ports)
			goto error;
		wIndex--;
		temp = readl (&ehci->regs->port_status [wIndex]);
		if (temp & PORT_OWNER)
			break;

		switch (wValue) {
		case USB_PORT_FEAT_SUSPEND:
			writel (temp | PORT_SUSPEND,
				&ehci->regs->port_status [wIndex]);
			break;
		case USB_PORT_FEAT_POWER:
			if (HCS_PPC (ehci->hcs_params))
				writel (temp | PORT_POWER,
					&ehci->regs->port_status [wIndex]);
			break;
		case USB_PORT_FEAT_RESET:
			/* line status bits may report this as low speed */
			if ((temp & (PORT_PE|PORT_CONNECT)) == PORT_CONNECT
					&& PORT_USB11 (temp)) {
				ehci_dbg (ehci,
					"port %d low speed --> companion\n",
					wIndex + 1);
				temp |= PORT_OWNER;
			} else {
				ehci_vdbg (ehci, "port %d reset\n", wIndex + 1);
				temp |= PORT_RESET;
				temp &= ~PORT_PE;

				/*
				 * caller must wait, then call GetPortStatus
				 * usb 2.0 spec says 50 ms resets on root
				 */
				ehci->reset_done [wIndex] = jiffies
				    	+ ((50 /* msec */ * HZ) / 1000);
			}
			writel (temp, &ehci->regs->port_status [wIndex]);
			break;
		default:
			goto error;
		}
		readl (&ehci->regs->command);	/* unblock posted writes */
		break;

	default:
error:
		/* "stall" on error */
		retval = -EPIPE;
	}
	spin_unlock_irqrestore (&ehci->lock, flags);
	return retval;
}
