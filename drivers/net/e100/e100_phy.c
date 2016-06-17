/*******************************************************************************

  
  Copyright(c) 1999 - 2004 Intel Corporation. All rights reserved.
  
  This program is free software; you can redistribute it and/or modify it 
  under the terms of the GNU General Public License as published by the Free 
  Software Foundation; either version 2 of the License, or (at your option) 
  any later version.
  
  This program is distributed in the hope that it will be useful, but WITHOUT 
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
  more details.
  
  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 59 
  Temple Place - Suite 330, Boston, MA  02111-1307, USA.
  
  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
  
  Contact Information:
  Linux NICS <linux.nics@intel.com>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
*******************************************************************************/

#include "e100_phy.h"

void e100_handle_zlock(struct e100_private *bdp);

/* 
 * Procedure:	e100_mdi_write
 *
 * Description: This routine will write a value to the specified MII register
 *		of an external MDI compliant device (e.g. PHY 100).  The
 *		command will execute in polled mode.
 *
 * Arguments:
 *	bdp - Ptr to this card's e100_bdconfig structure
 *	reg_addr - The MII register that we are writing to
 *	phy_addr - The MDI address of the Phy component.
 *	data - The value that we are writing to the MII register.
 *
 * Returns:
 *	NOTHING
 */
int
e100_mdi_write(struct e100_private *bdp, u32 reg_addr, u32 phy_addr, u16 data)
{
	int e100_retry;
	u32 temp_val;
	unsigned int mdi_cntrl;

	spin_lock_bh(&bdp->mdi_access_lock);
	temp_val = (((u32) data) | (reg_addr << 16) |
		    (phy_addr << 21) | (MDI_WRITE << 26));
	writel(temp_val, &bdp->scb->scb_mdi_cntrl);
	readw(&bdp->scb->scb_status);

	/* wait 20usec before checking status */
	udelay(20);

	/* poll for the mdi write to complete */
	e100_retry = E100_CMD_WAIT;
	while ((!((mdi_cntrl = readl(&bdp->scb->scb_mdi_cntrl)) & MDI_PHY_READY)) && (e100_retry)) {

		udelay(20);
		e100_retry--;
	}
	spin_unlock_bh(&bdp->mdi_access_lock);
	if (mdi_cntrl & MDI_PHY_READY) 
		return 0;
	else {
		printk(KERN_ERR "e100: MDI write timeout\n");
		return 1;
	}
}

/* 
 * Procedure:	e100_mdi_read
 *
 * Description: This routine will read a value from the specified MII register
 *		of an external MDI compliant device (e.g. PHY 100), and return
 *		it to the calling routine.  The command will execute in polled
 *		mode.
 *
 * Arguments:
 *	bdp - Ptr to this card's e100_bdconfig structure
 *	reg_addr - The MII register that we are reading from
 *	phy_addr - The MDI address of the Phy component.
 *
 * Results:
 *	data - The value that we read from the MII register.
 *
 * Returns:
 *	NOTHING
 */
int
e100_mdi_read(struct e100_private *bdp, u32 reg_addr, u32 phy_addr, u16 *data)
{
	int e100_retry;
	u32 temp_val;
	unsigned int mdi_cntrl;

	spin_lock_bh(&bdp->mdi_access_lock);
	/* Issue the read command to the MDI control register. */
	temp_val = ((reg_addr << 16) | (phy_addr << 21) | (MDI_READ << 26));
	writel(temp_val, &bdp->scb->scb_mdi_cntrl);
	readw(&bdp->scb->scb_status);

	/* wait 20usec before checking status */
	udelay(20);

	/* poll for the mdi read to complete */
	e100_retry = E100_CMD_WAIT;
	while ((!((mdi_cntrl = readl(&bdp->scb->scb_mdi_cntrl)) & MDI_PHY_READY)) && (e100_retry)) {

		udelay(20);
		e100_retry--;
	}

	spin_unlock_bh(&bdp->mdi_access_lock);
	if (mdi_cntrl & MDI_PHY_READY) {
		/* return the lower word */
		*data = (u16) mdi_cntrl;
		return 0;
	}
	else {
		printk(KERN_ERR "e100: MDI read timeout\n");
		return 1;
	}
}

static unsigned char
e100_phy_valid(struct e100_private *bdp, unsigned int phy_address)
{
	u16 ctrl_reg, stat_reg;

	/* Read the MDI control register */
	e100_mdi_read(bdp, MII_BMCR, phy_address, &ctrl_reg);

	/* Read the status register twice, bacause of sticky bits */
	e100_mdi_read(bdp, MII_BMSR, phy_address, &stat_reg);
	e100_mdi_read(bdp, MII_BMSR, phy_address, &stat_reg);

	if ((ctrl_reg == 0xffff) || ((stat_reg == 0) && (ctrl_reg == 0)))
		return false;

	return true;
}

static void
e100_phy_address_detect(struct e100_private *bdp)
{
	unsigned int addr;
	unsigned char valid_phy_found = false;

	if (IS_NC3133(bdp)) {
		bdp->phy_addr = 0;
		return;
	}

	if (e100_phy_valid(bdp, PHY_DEFAULT_ADDRESS)) {
		bdp->phy_addr = PHY_DEFAULT_ADDRESS;
		valid_phy_found = true;

	} else {
		for (addr = MIN_PHY_ADDR; addr <= MAX_PHY_ADDR; addr++) {
			if (e100_phy_valid(bdp, addr)) {
				bdp->phy_addr = addr;
				valid_phy_found = true;
				break;
			}
		}
	}

	if (!valid_phy_found) {
		bdp->phy_addr = PHY_ADDRESS_503;
	}
}

static void
e100_phy_id_detect(struct e100_private *bdp)
{
	u16 low_id_reg, high_id_reg;

	if (bdp->phy_addr == PHY_ADDRESS_503) {
		bdp->PhyId = PHY_503;
		return;
	}
	if (!(bdp->flags & IS_ICH)) {
		if (bdp->rev_id >= D102_REV_ID) {
			bdp->PhyId = PHY_82562ET;
			return;
		}
	}

	/* Read phy id from the MII register */
	e100_mdi_read(bdp, MII_PHYSID1, bdp->phy_addr, &low_id_reg);
	e100_mdi_read(bdp, MII_PHYSID2, bdp->phy_addr, &high_id_reg);

	bdp->PhyId = ((unsigned int) low_id_reg |
		      ((unsigned int) high_id_reg << 16));
}

static void
e100_phy_isolate(struct e100_private *bdp)
{
	unsigned int phy_address;
	u16 ctrl_reg;

	/* Go over all phy addresses. Deisolate the selected one, and isolate
	 * all the rest */
	for (phy_address = 0; phy_address <= MAX_PHY_ADDR; phy_address++) {
		if (phy_address != bdp->phy_addr) {
			e100_mdi_write(bdp, MII_BMCR, phy_address,
				       BMCR_ISOLATE);

		} else {
			e100_mdi_read(bdp, MII_BMCR, bdp->phy_addr, &ctrl_reg);
			ctrl_reg &= ~BMCR_ISOLATE;
			e100_mdi_write(bdp, MII_BMCR, bdp->phy_addr, ctrl_reg);
		}

		udelay(100);
	}
}

static unsigned char
e100_phy_specific_setup(struct e100_private *bdp)
{
	u16 misc_reg;

	if (bdp->phy_addr == PHY_ADDRESS_503) {
		switch (bdp->params.e100_speed_duplex) {
		case E100_AUTONEG:
			/* The adapter can't autoneg. so set to 10/HALF */
			printk(KERN_INFO
			       "e100: 503 serial component detected which "
			       "cannot autonegotiate\n");
			printk(KERN_INFO
			       "e100: speed/duplex forced to "
			       "10Mbps / Half duplex\n");
			bdp->params.e100_speed_duplex = E100_SPEED_10_HALF;
			break;

		case E100_SPEED_100_HALF:
		case E100_SPEED_100_FULL:
			printk(KERN_ERR
			       "e100: 503 serial component detected "
			       "which does not support 100Mbps\n");
			printk(KERN_ERR
			       "e100: Change the forced speed/duplex "
			       "to a supported setting\n");
			return false;
		}

		return true;
	}

	if (IS_NC3133(bdp)) {
		u16 int_reg;

		/* enable 100BASE fiber interface */
		e100_mdi_write(bdp, MDI_NC3133_CONFIG_REG, bdp->phy_addr,
			       MDI_NC3133_100FX_ENABLE);

		if ((bdp->params.e100_speed_duplex != E100_AUTONEG) &&
		    (bdp->params.e100_speed_duplex != E100_SPEED_100_FULL)) {
			/* just inform user about 100 full */
			printk(KERN_ERR "e100: NC3133 NIC can only run "
			       "at 100Mbps full duplex\n");
		}

		bdp->params.e100_speed_duplex = E100_SPEED_100_FULL;

		/* enable interrupts */
		e100_mdi_read(bdp, MDI_NC3133_INT_ENABLE_REG,
			      bdp->phy_addr, &int_reg);
		int_reg |= MDI_NC3133_INT_ENABLE;
		e100_mdi_write(bdp, MDI_NC3133_INT_ENABLE_REG,
			       bdp->phy_addr, int_reg);
	}

	/* Handle the National TX */
	if ((bdp->PhyId & PHY_MODEL_REV_ID_MASK) == PHY_NSC_TX) {
		e100_mdi_read(bdp, NSC_CONG_CONTROL_REG,
			      bdp->phy_addr, &misc_reg);

		misc_reg |= NSC_TX_CONG_TXREADY;

		/* disable the congestion control bit in the National Phy */
		misc_reg &= ~NSC_TX_CONG_ENABLE;

		e100_mdi_write(bdp, NSC_CONG_CONTROL_REG,
			       bdp->phy_addr, misc_reg);
	}

	return true;
}

/* 
 * Procedure:	e100_phy_fix_squelch
 *
 * Description:
 *	Help find link on certain rare scenarios.
 *	NOTE: This routine must be called once per watchdog,
 *	      and *after* setting the current link state.
 *
 * Arguments:
 *	bdp - Ptr to this card's e100_bdconfig structure
 *
 * Returns:
 *	NOTHING
 */
static void
e100_phy_fix_squelch(struct e100_private *bdp)
{
	if ((bdp->PhyId != PHY_82555_TX) || (bdp->flags & DF_SPEED_FORCED))
		return;

	if (netif_carrier_ok(bdp->device)) {
		switch (bdp->PhyState) {
		case 0:
			break;
		case 1:
			e100_mdi_write(bdp, PHY_82555_SPECIAL_CONTROL,
				       bdp->phy_addr, 0x0000);
			break;
		case 2:
			e100_mdi_write(bdp, PHY_82555_MDI_EQUALIZER_CSR,
				       bdp->phy_addr, 0x3000);
			break;
		}
		bdp->PhyState = 0;
		bdp->PhyDelay = 0;

	} else if (!bdp->PhyDelay--) {
		switch (bdp->PhyState) {
		case 0:
			e100_mdi_write(bdp, PHY_82555_SPECIAL_CONTROL,
				       bdp->phy_addr, EXTENDED_SQUELCH_BIT);
			bdp->PhyState = 1;
			break;
		case 1:
			e100_mdi_write(bdp, PHY_82555_SPECIAL_CONTROL,
				       bdp->phy_addr, 0x0000);
			e100_mdi_write(bdp, PHY_82555_MDI_EQUALIZER_CSR,
				       bdp->phy_addr, 0x2010);
			bdp->PhyState = 2;
			break;
		case 2:
			e100_mdi_write(bdp, PHY_82555_MDI_EQUALIZER_CSR,
				       bdp->phy_addr, 0x3000);
			bdp->PhyState = 0;
			break;
		}

		e100_mdi_write(bdp, MII_BMCR, bdp->phy_addr,
			       BMCR_ANENABLE | BMCR_ANRESTART);
		bdp->PhyDelay = 3;
	}
}

/* 
 * Procedure:	e100_fix_polarity
 *
 * Description:
 *	Fix for 82555 auto-polarity toggle problem. With a short cable 
 *	connecting an 82555 with an 840A link partner, if the medium is noisy,
 *	the 82555 sometime thinks that the polarity might be wrong and so 
 *	toggles polarity. This happens repeatedly and results in a high bit 
 *	error rate.
 *	NOTE: This happens only at 10 Mbps
 *
 * Arguments:
 *	bdp - Ptr to this card's e100_bdconfig structure
 *
 * Returns:
 *	NOTHING
 */
static void
e100_fix_polarity(struct e100_private *bdp)
{
	u16 status;
	u16 errors;
	u16 misc_reg;
	int speed;

	if ((bdp->PhyId != PHY_82555_TX) && (bdp->PhyId != PHY_82562ET) &&
	    (bdp->PhyId != PHY_82562EM))
		return;

	/* If the user wants auto-polarity disabled, do only that and nothing *
	 * else. * e100_autopolarity == 0 means disable --- we do just the
	 * disabling * e100_autopolarity == 1 means enable  --- we do nothing at
	 * all * e100_autopolarity >= 2 means we do the workaround code. */
	/* Change for 82558 enhancement */
	switch (E100_AUTOPOLARITY) {
	case 0:
		e100_mdi_read(bdp, PHY_82555_SPECIAL_CONTROL,
			      bdp->phy_addr, &misc_reg);
		e100_mdi_write(bdp, PHY_82555_SPECIAL_CONTROL, bdp->phy_addr,
			       (u16) (misc_reg | DISABLE_AUTO_POLARITY));
		break;

	case 1:
		e100_mdi_read(bdp, PHY_82555_SPECIAL_CONTROL,
			      bdp->phy_addr, &misc_reg);
		e100_mdi_write(bdp, PHY_82555_SPECIAL_CONTROL, bdp->phy_addr,
			       (u16) (misc_reg & ~DISABLE_AUTO_POLARITY));
		break;

	case 2:
		/* we do this only if link is up */
		if (!netif_carrier_ok(bdp->device)) {
			break;
		}

		e100_mdi_read(bdp, PHY_82555_CSR, bdp->phy_addr, &status);
		speed = (status & PHY_82555_SPEED_BIT) ? 100 : 10;

		/* we need to do this only if speed is 10 */
		if (speed != 10) {
			break;
		}

		/* see if we have any end of frame errors */
		e100_mdi_read(bdp, PHY_82555_EOF_COUNTER,
			      bdp->phy_addr, &errors);

		/* if non-zero, wait for 100 ms before reading again */
		if (errors) {
			udelay(200);
			e100_mdi_read(bdp, PHY_82555_EOF_COUNTER,
				      bdp->phy_addr, &errors);

			/* if non-zero again, we disable polarity */
			if (errors) {
				e100_mdi_read(bdp, PHY_82555_SPECIAL_CONTROL,
					      bdp->phy_addr, &misc_reg);
				e100_mdi_write(bdp, PHY_82555_SPECIAL_CONTROL,
					       bdp->phy_addr,
					       (u16) (misc_reg |
						      DISABLE_AUTO_POLARITY));
			}
		}

		if (!errors) {
			/* it is safe to read the polarity now */
			e100_mdi_read(bdp, PHY_82555_CSR,
				      bdp->phy_addr, &status);

			/* if polarity is normal, disable polarity */
			if (!(status & PHY_82555_POLARITY_BIT)) {
				e100_mdi_read(bdp, PHY_82555_SPECIAL_CONTROL,
					      bdp->phy_addr, &misc_reg);
				e100_mdi_write(bdp, PHY_82555_SPECIAL_CONTROL,
					       bdp->phy_addr,
					       (u16) (misc_reg |
						      DISABLE_AUTO_POLARITY));
			}
		}
		break;

	default:
		break;
	}
}

/* 
 * Procedure:	e100_find_speed_duplex
 *
 * Description: This routine will figure out what line speed and duplex mode
 *		the PHY is currently using.
 *
 * Arguments:
 *	bdp - Ptr to this card's e100_bdconfig structure
 *
 * Returns:
 *	NOTHING
 */
static void
e100_find_speed_duplex(struct e100_private *bdp)
{
	unsigned int PhyId;
	u16 stat_reg, misc_reg;
	u16 ad_reg, lp_ad_reg;

	PhyId = bdp->PhyId & PHY_MODEL_REV_ID_MASK;

	/* First we should check to see if we have link */
	/* If we don't have a link no reason to print a speed and duplex */
	if (!e100_update_link_state(bdp)) {
		bdp->cur_line_speed = 0;
		bdp->cur_dplx_mode = 0;
		return;
	}

	/* On the 82559 and later controllers, speed/duplex is part of the *
	 * SCB. So, we save an mdi_read and get these from the SCB. * */
	if (bdp->rev_id >= D101MA_REV_ID) {
		/* Read speed */
		if (readb(&bdp->scb->scb_ext.d101m_scb.scb_gen_stat) & BIT_1)
			bdp->cur_line_speed = 100;
		else
			bdp->cur_line_speed = 10;

		/* Read duplex */
		if (readb(&bdp->scb->scb_ext.d101m_scb.scb_gen_stat) & BIT_2)
			bdp->cur_dplx_mode = FULL_DUPLEX;
		else
			bdp->cur_dplx_mode = HALF_DUPLEX;

		return;
	}

	/* If this is a Phy 100, then read bits 1 and 0 of extended register 0,
	 * to get the current speed and duplex settings. */
	if ((PhyId == PHY_100_A) || (PhyId == PHY_100_C) ||
	    (PhyId == PHY_82555_TX)) {

		/* Read Phy 100 extended register 0 */
		e100_mdi_read(bdp, EXTENDED_REG_0, bdp->phy_addr, &misc_reg);

		/* Get current speed setting */
		if (misc_reg & PHY_100_ER0_SPEED_INDIC)
			bdp->cur_line_speed = 100;
		else
			bdp->cur_line_speed = 10;

		/* Get current duplex setting -- FDX enabled if bit is set */
		if (misc_reg & PHY_100_ER0_FDX_INDIC)
			bdp->cur_dplx_mode = FULL_DUPLEX;
		else
			bdp->cur_dplx_mode = HALF_DUPLEX;

		return;
	}

	/* See if link partner is capable of Auto-Negotiation (bit 0, reg 6) */
	e100_mdi_read(bdp, MII_EXPANSION, bdp->phy_addr, &misc_reg);

	/* See if Auto-Negotiation was complete (bit 5, reg 1) */
	e100_mdi_read(bdp, MII_BMSR, bdp->phy_addr, &stat_reg);

	/* If a True NWAY connection was made, then we can detect speed/dplx
	 * by ANDing our adapter's advertised abilities with our link partner's
	 * advertised ablilities, and then assuming that the highest common
	 * denominator was chosed by NWAY. */
	if ((misc_reg & EXPANSION_NWAY) && (stat_reg & BMSR_ANEGCOMPLETE)) {

		/* Read our advertisement register */
		e100_mdi_read(bdp, MII_ADVERTISE, bdp->phy_addr, &ad_reg);

		/* Read our link partner's advertisement register */
		e100_mdi_read(bdp, MII_LPA, bdp->phy_addr, &lp_ad_reg);

		/* AND the two advertisement registers together, and get rid
		 * of any extraneous bits. */
		ad_reg &= (lp_ad_reg & NWAY_LP_ABILITY);

		/* Get speed setting */
		if (ad_reg &
		    (ADVERTISE_100HALF | ADVERTISE_100FULL |
		     ADVERTISE_100BASE4))

			bdp->cur_line_speed = 100;
		else
			bdp->cur_line_speed = 10;

		/* Get duplex setting -- use priority resolution algorithm */
		if (ad_reg & ADVERTISE_100BASE4) {
			bdp->cur_dplx_mode = HALF_DUPLEX;
		} else if (ad_reg & ADVERTISE_100FULL) {
			bdp->cur_dplx_mode = FULL_DUPLEX;
		} else if (ad_reg & ADVERTISE_100HALF) {
			bdp->cur_dplx_mode = HALF_DUPLEX;
		} else if (ad_reg & ADVERTISE_10FULL) {
			bdp->cur_dplx_mode = FULL_DUPLEX;
		} else {
			bdp->cur_dplx_mode = HALF_DUPLEX;
		}

		return;
	}

	/* If we are connected to a dumb (non-NWAY) repeater or hub, and the
	 * line speed was determined automatically by parallel detection, then
	 * we have no way of knowing exactly what speed the PHY is set to
	 * unless that PHY has a propietary register which indicates speed in
	 * this situation. The NSC TX PHY does have such a register. Also,
	 * since NWAY didn't establish the connection, the duplex setting
	 * should HALF duplex. */
	bdp->cur_dplx_mode = HALF_DUPLEX;

	if (PhyId == PHY_NSC_TX) {
		/* Read register 25 to get the SPEED_10 bit */
		e100_mdi_read(bdp, NSC_SPEED_IND_REG, bdp->phy_addr, &misc_reg);

		/* If bit 6 was set then we're at 10Mbps */
		if (misc_reg & NSC_TX_SPD_INDC_SPEED)
			bdp->cur_line_speed = 10;
		else
			bdp->cur_line_speed = 100;

	} else {
		/* If we don't know the line speed, default to 10Mbps */
		bdp->cur_line_speed = 10;
	}
}

/* 
 * Procedure: e100_force_speed_duplex
 *
 * Description: This routine forces line speed and duplex mode of the
 * adapter based on the values the user has set in e100.c.
 *
 * Arguments:  bdp - Pointer to the e100_private structure for the board
 *
 * Returns: void
 *
 */
void
e100_force_speed_duplex(struct e100_private *bdp)
{
	u16 control;
	unsigned long expires;

	bdp->flags |= DF_SPEED_FORCED;

	e100_mdi_read(bdp, MII_BMCR, bdp->phy_addr, &control);
	control &= ~BMCR_ANENABLE;
	control &= ~BMCR_LOOPBACK;

	switch (bdp->params.e100_speed_duplex) {
	case E100_SPEED_10_HALF:
		control &= ~BMCR_SPEED100;
		control &= ~BMCR_FULLDPLX;
		bdp->cur_line_speed = 10;
		bdp->cur_dplx_mode = HALF_DUPLEX;
		break;

	case E100_SPEED_10_FULL:
		control &= ~BMCR_SPEED100;
		control |= BMCR_FULLDPLX;
		bdp->cur_line_speed = 10;
		bdp->cur_dplx_mode = FULL_DUPLEX;
		break;

	case E100_SPEED_100_HALF:
		control |= BMCR_SPEED100;
		control &= ~BMCR_FULLDPLX;
		bdp->cur_line_speed = 100;
		bdp->cur_dplx_mode = HALF_DUPLEX;
		break;

	case E100_SPEED_100_FULL:
		control |= BMCR_SPEED100;
		control |= BMCR_FULLDPLX;
		bdp->cur_line_speed = 100;
		bdp->cur_dplx_mode = FULL_DUPLEX;
		break;
	}

	e100_mdi_write(bdp, MII_BMCR, bdp->phy_addr, control);

	/* loop must run at least once */
	expires = jiffies + 2 * HZ;
	do {
		if (e100_update_link_state(bdp) || 
		    time_after(jiffies, expires)) {
			break;
		} else {
			yield();
		}

	} while (true);
}

void
e100_force_speed_duplex_to_phy(struct e100_private *bdp)
{
	u16 control;

	e100_mdi_read(bdp, MII_BMCR, bdp->phy_addr, &control);
	control &= ~BMCR_ANENABLE;
	control &= ~BMCR_LOOPBACK;

	switch (bdp->params.e100_speed_duplex) {
	case E100_SPEED_10_HALF:
		control &= ~BMCR_SPEED100;
		control &= ~BMCR_FULLDPLX;
		break;

	case E100_SPEED_10_FULL:
		control &= ~BMCR_SPEED100;
		control |= BMCR_FULLDPLX;
		break;

	case E100_SPEED_100_HALF:
		control |= BMCR_SPEED100;
		control &= ~BMCR_FULLDPLX;
		break;

	case E100_SPEED_100_FULL:
		control |= BMCR_SPEED100;
		control |= BMCR_FULLDPLX;
		break;
	}

	/* Send speed/duplex command to PHY layer. */
	e100_mdi_write(bdp, MII_BMCR, bdp->phy_addr, control);
}

/* 
 * Procedure: e100_set_fc
 *
 * Description: Checks the link's capability for flow control.
 * 
 * Arguments:  bdp - Pointer to the e100_private structure for the board
 *		    
 * Returns: void
 *
 */
static void
e100_set_fc(struct e100_private *bdp)
{
	u16 ad_reg;
	u16 lp_ad_reg;
	u16 exp_reg;

	/* no flow control for 82557, forced links or half duplex */
	if (!netif_carrier_ok(bdp->device) || (bdp->flags & DF_SPEED_FORCED) ||
	    (bdp->cur_dplx_mode == HALF_DUPLEX) ||
	    !(bdp->flags & IS_BACHELOR)) {

		bdp->flags &= ~DF_LINK_FC_CAP;
		return;
	}

	/* See if link partner is capable of Auto-Negotiation (bit 0, reg 6) */
	e100_mdi_read(bdp, MII_EXPANSION, bdp->phy_addr, &exp_reg);

	if (exp_reg & EXPANSION_NWAY) {
		/* Read our advertisement register */
		e100_mdi_read(bdp, MII_ADVERTISE, bdp->phy_addr, &ad_reg);

		/* Read our link partner's advertisement register */
		e100_mdi_read(bdp, MII_LPA, bdp->phy_addr, &lp_ad_reg);

		ad_reg &= lp_ad_reg;	/* AND the 2 ad registers */

		if (ad_reg & NWAY_AD_FC_SUPPORTED)
			bdp->flags |= DF_LINK_FC_CAP;
		else
			/* If link partner is capable of autoneg, but  */
			/* not capable of flow control, Received PAUSE */
			/* frames are still honored, i.e.,             */
		        /* transmitted frames would be paused */
			/* by incoming PAUSE frames           */
			bdp->flags |= DF_LINK_FC_TX_ONLY;

	} else {
		bdp->flags &= ~DF_LINK_FC_CAP;
	}
}

/* 
 * Procedure: e100_phy_check
 * 
 * Arguments:  bdp - Pointer to the e100_private structure for the board
 *
 * Returns: true if link state was changed
 *	   false otherwise
 *
 */
unsigned char
e100_phy_check(struct e100_private *bdp)
{
	unsigned char old_link;
	unsigned char changed = false;

	old_link = netif_carrier_ok(bdp->device) ? 1 : 0;
	e100_find_speed_duplex(bdp);

	if (!old_link && netif_carrier_ok(bdp->device)) {
		e100_set_fc(bdp);
		changed = true;
	}

	if (old_link && !netif_carrier_ok(bdp->device)) {
		/* reset the zero lock state */
		bdp->zlock_state = ZLOCK_INITIAL;

		// set auto lock for phy auto-negotiation on link up
		if ((bdp->PhyId & PHY_MODEL_REV_ID_MASK) == PHY_82555_TX)
			e100_mdi_write(bdp, PHY_82555_MDI_EQUALIZER_CSR,
				       bdp->phy_addr, 0);
		changed = true;
	}

	e100_phy_fix_squelch(bdp);
	e100_handle_zlock(bdp);

	return changed;
}

/* 
 * Procedure:	e100_auto_neg
 *
 * Description: This routine will start autonegotiation and wait
 *		     for it to complete
 *
 * Arguments:
 *	bdp		- pointer to this card's e100_bdconfig structure
 *	force_restart	- defines if autoneg should be restarted even if it
 *			has been completed before
 * Returns:
 *	NOTHING
 */
static void
e100_auto_neg(struct e100_private *bdp, unsigned char force_restart)
{
	u16 stat_reg;
	unsigned long expires;

	bdp->flags &= ~DF_SPEED_FORCED;

	e100_mdi_read(bdp, MII_BMSR, bdp->phy_addr, &stat_reg);
	e100_mdi_read(bdp, MII_BMSR, bdp->phy_addr, &stat_reg);

	/* if we are capable of performing autoneg then we restart if needed */
	if ((stat_reg != 0xFFFF) && (stat_reg & BMSR_ANEGCAPABLE)) {

		if ((!force_restart) &&
		    (stat_reg & BMSR_ANEGCOMPLETE)) {
			goto exit;
		}

		e100_mdi_write(bdp, MII_BMCR, bdp->phy_addr,
			       BMCR_ANENABLE | BMCR_ANRESTART);

		/* wait for autoneg to complete (up to 3 seconds) */
		expires = jiffies + HZ * 3;
		do {
			/* now re-read the value. Sticky so read twice */
			e100_mdi_read(bdp, MII_BMSR, bdp->phy_addr, &stat_reg);
			e100_mdi_read(bdp, MII_BMSR, bdp->phy_addr, &stat_reg);

			if ((stat_reg & BMSR_ANEGCOMPLETE) ||
			    time_after(jiffies, expires) ) {
				goto exit;
			} else {
				yield();
			}
		} while (true);
	}

exit:
	e100_find_speed_duplex(bdp);
}

void
e100_phy_set_speed_duplex(struct e100_private *bdp, unsigned char force_restart)
{
	if (bdp->params.e100_speed_duplex == E100_AUTONEG) {
        	if (bdp->rev_id >= D102_REV_ID) 
			/* Enable MDI/MDI-X auto switching */
                	e100_mdi_write(bdp, MII_NCONFIG, bdp->phy_addr,
		                       MDI_MDIX_AUTO_SWITCH_ENABLE);
		e100_auto_neg(bdp, force_restart);

	} else {
        	if (bdp->rev_id >= D102_REV_ID) 
			/* Disable MDI/MDI-X auto switching */
                	e100_mdi_write(bdp, MII_NCONFIG, bdp->phy_addr,
		                       MDI_MDIX_RESET_ALL_MASK);
		e100_force_speed_duplex(bdp);
	}

	e100_set_fc(bdp);
}

void
e100_phy_autoneg(struct e100_private *bdp)
{
	u16 ctrl_reg;

	ctrl_reg = BMCR_ANENABLE | BMCR_ANRESTART | BMCR_RESET;

	e100_mdi_write(bdp, MII_BMCR, bdp->phy_addr, ctrl_reg);

	udelay(100);
}

void
e100_phy_set_loopback(struct e100_private *bdp)
{
	u16 ctrl_reg;
	ctrl_reg = BMCR_LOOPBACK;
	e100_mdi_write(bdp, MII_BMCR, bdp->phy_addr, ctrl_reg);
		udelay(100);
}
	
void
e100_phy_reset(struct e100_private *bdp)
{
	u16 ctrl_reg;
	ctrl_reg = BMCR_RESET;
	e100_mdi_write(bdp, MII_BMCR, bdp->phy_addr, ctrl_reg);
	/* ieee 802.3 : The reset process shall be completed       */
	/* within 0.5 seconds from the settting of PHY reset bit.  */
	set_current_state(TASK_UNINTERRUPTIBLE);
	schedule_timeout(HZ / 2);
}

unsigned char
e100_phy_init(struct e100_private *bdp)
{
	e100_phy_reset(bdp);
	e100_phy_address_detect(bdp);
	e100_phy_isolate(bdp);
	e100_phy_id_detect(bdp);

	if (!e100_phy_specific_setup(bdp))
		return false;

	bdp->PhyState = 0;
	bdp->PhyDelay = 0;
	bdp->zlock_state = ZLOCK_INITIAL;

	e100_phy_set_speed_duplex(bdp, false);
	e100_fix_polarity(bdp);

	return true;
}

/* 
 * Procedure: e100_get_link_state
 * 
 * Description: This routine checks the link status of the adapter
 *
 * Arguments:  bdp - Pointer to the e100_private structure for the board
 *		    
 *
 * Returns: true - If a link is found
 *		false - If there is no link
 *
 */
unsigned char
e100_get_link_state(struct e100_private *bdp)
{
	unsigned char link = false;
	u16 status;

	/* Check link status */
	/* If the controller is a 82559 or later one, link status is available
	 * from the CSR. This avoids the mdi_read. */
	if (bdp->rev_id >= D101MA_REV_ID) {
		if (readb(&bdp->scb->scb_ext.d101m_scb.scb_gen_stat) & BIT_0) {
			link = true;
		} else {
			link = false;
		}

	} else {
		/* Read the status register twice because of sticky bits */
		e100_mdi_read(bdp, MII_BMSR, bdp->phy_addr, &status);
		e100_mdi_read(bdp, MII_BMSR, bdp->phy_addr, &status);

		if (status & BMSR_LSTATUS) {
			link = true;
		} else {
			link = false;
		}
	}

	return link;
}

/* 
 * Procedure: e100_update_link_state
 * 
 * Description: This routine updates the link status of the adapter,
 * 		also considering netif_running
 *
 * Arguments:  bdp - Pointer to the e100_private structure for the board
 *		    
 *
 * Returns: true - If a link is found
 *		false - If there is no link
 *
 */
unsigned char
e100_update_link_state(struct e100_private *bdp)
{
	unsigned char link;

	/* Logical AND PHY link & netif_running */
	link = e100_get_link_state(bdp) && netif_running(bdp->device);

	if (link) {
		if (!netif_carrier_ok(bdp->device))
			netif_carrier_on(bdp->device);
	} else {
		if (netif_carrier_ok(bdp->device))
			netif_carrier_off(bdp->device);
	}

	return link;
}

/**************************************************************************\
 **
 ** PROC NAME:     e100_handle_zlock
 **    This function manages a state machine that controls
 **    the driver's zero locking algorithm.
 **    This function is called by e100_watchdog() every ~2 second.
 ** States:
 **    The current link handling state is stored in 
 **    bdp->zlock_state, and is one of:
 **    ZLOCK_INITIAL, ZLOCK_READING, ZLOCK_SLEEPING
 **    Detailed description of the states and the transitions
 **    between states is found below.
 **    Note that any time the link is down / there is a reset
 **    state will be changed outside this function to ZLOCK_INITIAL
 ** Algorithm:
 **    1. If link is up & 100 Mbps continue else stay in #1:
 **    2. Set 'auto lock'
 **    3. Read & Store 100 times 'Zero' locked in 1 sec interval
 **    4. If max zero read >= 0xB continue else goto 1
 **    5. Set most popular 'Zero' read in #3
 **    6. Sleep 5 minutes
 **    7. Read number of errors, if it is > 300 goto 2 else goto 6
 ** Data Structures (in DRIVER_DATA):
 **    zlock_state           - current state of the algorithm
 **    zlock_read_cnt        - counts number of reads (up to 100)
 **    zlock_read_data[i]    - counts number of times 'Zero' read was i, 0 <= i <= 15
 **    zlock_sleep_cnt       - keeps track of "sleep" time (up to 300 secs = 5 minutes)
 **                                
 ** Parameters:    DRIVER_DATA    *bdp
 **
 **                bdp  - Pointer to HSM's adapter data space
 **
 ** Return Value:  NONE
 **
 ** See Also:      e100_watchdog()
 **
 \**************************************************************************/
void
e100_handle_zlock(struct e100_private *bdp)
{
	u16 pos;
	u16 eq_reg;
	u16 err_cnt;
	u8 mpz;			/* Most Popular Zero */

	switch (bdp->zlock_state) {
	case ZLOCK_INITIAL:

		if (((u8) bdp->rev_id <= D102_REV_ID) ||
		    !(bdp->cur_line_speed == 100) ||
		    !netif_carrier_ok(bdp->device)) {
			break;
		}

		/* initialize hw and sw and start reading */
		e100_mdi_write(bdp, PHY_82555_MDI_EQUALIZER_CSR,
			       bdp->phy_addr, 0);
		/* reset read counters: */
		bdp->zlock_read_cnt = 0;
		for (pos = 0; pos < 16; pos++)
			bdp->zlock_read_data[pos] = 0;
		/* start reading in the next call back: */
		bdp->zlock_state = ZLOCK_READING;

		/* FALL THROUGH !! */

	case ZLOCK_READING:
		/* state: reading (100 times) zero locked in 1 sec interval
		 * prev states: ZLOCK_INITIAL
		 * next states: ZLOCK_INITIAL, ZLOCK_SLEEPING */

		e100_mdi_read(bdp, PHY_82555_MDI_EQUALIZER_CSR,
			      bdp->phy_addr, &eq_reg);
		pos = (eq_reg & ZLOCK_ZERO_MASK) >> 4;
		bdp->zlock_read_data[pos]++;
		bdp->zlock_read_cnt++;

		if (bdp->zlock_read_cnt == ZLOCK_MAX_READS) {
			/* check if we read a 'Zero' value of 0xB or greater */
			if ((bdp->zlock_read_data[0xB]) ||
			    (bdp->zlock_read_data[0xC]) ||
			    (bdp->zlock_read_data[0xD]) ||
			    (bdp->zlock_read_data[0xE]) ||
			    (bdp->zlock_read_data[0xF])) {

				/* we've read 'Zero' value of 0xB or greater,
				 * find most popular 'Zero' value and lock it */
				mpz = 0;
				/* this loop finds the most popular 'Zero': */
				for (pos = 1; pos < 16; pos++) {
					if (bdp->zlock_read_data[pos] >
					    bdp->zlock_read_data[mpz])

						mpz = pos;
				}
				/* now lock the most popular 'Zero': */
				eq_reg = (ZLOCK_SET_ZERO | mpz);
				e100_mdi_write(bdp,
					       PHY_82555_MDI_EQUALIZER_CSR,
					       bdp->phy_addr, eq_reg);

				/* sleep for 5 minutes: */
				bdp->zlock_sleep_cnt = jiffies;
				bdp->zlock_state = ZLOCK_SLEEPING;
				/* we will be reading the # of errors after 5
				 * minutes, so we need to reset the error
				 * counters - these registers are self clearing
				 * on read, so read them */
				e100_mdi_read(bdp, PHY_82555_SYMBOL_ERR,
					      bdp->phy_addr, &err_cnt);

			} else {
				/* we did not read a 'Zero' value of 0xB or
				 * above. go back to the start */
				bdp->zlock_state = ZLOCK_INITIAL;
			}

		}
		break;

	case ZLOCK_SLEEPING:
		/* state: sleeping for 5 minutes
		 * prev states: ZLOCK_READING
		 * next states: ZLOCK_READING, ZLOCK_SLEEPING */

		/* if 5 minutes have passed: */
		if ((jiffies - bdp->zlock_sleep_cnt) >= ZLOCK_MAX_SLEEP) {
			/* read and sum up the number of errors:  */
			e100_mdi_read(bdp, PHY_82555_SYMBOL_ERR,
				      bdp->phy_addr, &err_cnt);
			/* if we've more than 300 errors (this number was
			 * calculated according to the spec max allowed errors
			 * (80 errors per 1 million frames) for 5 minutes in
			 * 100 Mbps (or the user specified max BER number) */
			if (err_cnt > bdp->params.ber) {
				/* start again in the next callback: */
				bdp->zlock_state = ZLOCK_INITIAL;
			} else {
				/* we don't have more errors than allowed,
				 * sleep for 5 minutes */
				bdp->zlock_sleep_cnt = jiffies;
			}
		}
		break;

	default:
		break;
	}
}
