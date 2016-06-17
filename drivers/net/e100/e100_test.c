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
#include "e100_config.h"

extern u16 e100_eeprom_read(struct e100_private *, u16);
extern int e100_wait_exec_cmplx(struct e100_private *, u32,u8, u8);
extern void e100_phy_reset(struct e100_private *bdp);
extern void e100_phy_autoneg(struct e100_private *bdp);
extern void e100_phy_set_loopback(struct e100_private *bdp);
extern void e100_force_speed_duplex(struct e100_private *bdp);

static u8 e100_diag_selftest(struct net_device *);
static u8 e100_diag_eeprom(struct net_device *);
static u8 e100_diag_loopback(struct net_device *);

static u8 e100_diag_one_loopback (struct net_device *, u8);
static u8 e100_diag_rcv_loopback_pkt(struct e100_private *);
static void e100_diag_config_loopback(struct e100_private *, u8, u8, u8 *,u8 *);
static u8 e100_diag_loopback_alloc(struct e100_private *);
static void e100_diag_loopback_cu_ru_exec(struct e100_private *);
static u8 e100_diag_check_pkt(u8 *);
static void e100_diag_loopback_free(struct e100_private *);
static int e100_cable_diag(struct e100_private *bdp);

#define LB_PACKET_SIZE 1500

/**
 * e100_run_diag - main test execution handler - checks mask of requests and calls the diag routines  
 * @dev: atapter's net device data struct
 * @test_info: array with test request mask also used to store test results
 *
 * RETURNS: updated flags field of struct ethtool_test
 */
u32
e100_run_diag(struct net_device *dev, u64 *test_info, u32 flags)
{
	struct e100_private* bdp = dev->priv;
	u8 test_result = 0;

	if (!e100_get_link_state(bdp)) {
		test_result = ETH_TEST_FL_FAILED;
		test_info[test_link] = true;
	}
	if (!e100_diag_eeprom(dev)) {
		test_result = ETH_TEST_FL_FAILED;
		test_info[test_eeprom] = true;
	}
	if (flags & ETH_TEST_FL_OFFLINE) {
		u8 fail_mask;
		if (netif_running(dev)) {
			spin_lock_bh(&dev->xmit_lock);
			e100_close(dev);
			spin_unlock_bh(&dev->xmit_lock);
		}
		if (e100_diag_selftest(dev)) {
			test_result = ETH_TEST_FL_FAILED;
			test_info[test_self_test] = true;
		}

		fail_mask = e100_diag_loopback(dev);
		if (fail_mask) {
			test_result = ETH_TEST_FL_FAILED;
			if (fail_mask & PHY_LOOPBACK)
				test_info[test_loopback_phy] = true;
			if (fail_mask & MAC_LOOPBACK)
				test_info[test_loopback_mac] = true;
		}

		test_info[cable_diag] = e100_cable_diag(bdp);
		/* Need hw init regardless of netif_running */
		e100_hw_init(bdp);
		if (netif_running(dev)) {
			e100_open(dev);
		}
	}
	else {
		test_info[test_self_test] = false;
		test_info[test_loopback_phy] = false;
		test_info[test_loopback_mac] = false;
		test_info[cable_diag] = false;
	}

	return flags | test_result;
}

/**
 * e100_diag_selftest - run hardware selftest 
 * @dev: atapter's net device data struct
 */
static u8
e100_diag_selftest(struct net_device *dev)
{
	struct e100_private *bdp = dev->priv;
	u32 st_timeout, st_result;
	u8 retval = 0;

	if (!e100_selftest(bdp, &st_timeout, &st_result)) {
		if (!st_timeout) {
			if (st_result & CB_SELFTEST_REGISTER_BIT)
				retval |= REGISTER_TEST_FAIL;
			if (st_result & CB_SELFTEST_DIAG_BIT)
				retval |= SELF_TEST_FAIL;
			if (st_result & CB_SELFTEST_ROM_BIT)
				retval |= ROM_TEST_FAIL;
		} else {
            		retval = TEST_TIMEOUT;
		}
	}

	return retval;
}

/**
 * e100_diag_eeprom - validate eeprom checksum correctness
 * @dev: atapter's net device data struct
 *
 */
static u8
e100_diag_eeprom (struct net_device *dev)
{
	struct e100_private *bdp = dev->priv;
	u16 i, eeprom_sum, eeprom_actual_csm;

	for (i = 0, eeprom_sum = 0; i < (bdp->eeprom_size - 1); i++) {
		eeprom_sum += e100_eeprom_read(bdp, i);
	}

	eeprom_actual_csm = e100_eeprom_read(bdp, bdp->eeprom_size - 1);

	if (eeprom_actual_csm == (u16)(EEPROM_SUM - eeprom_sum)) {
		return true;
	}

	return false;
}

/**
 * e100_diag_loopback - performs loopback test  
 * @dev: atapter's net device data struct
 */
static u8
e100_diag_loopback (struct net_device *dev)
{
	u8 rc = 0;

	printk(KERN_DEBUG "%s: PHY loopback test starts\n", dev->name);
	e100_hw_init(dev->priv);
	if (!e100_diag_one_loopback(dev, PHY_LOOPBACK)) {
		rc |= PHY_LOOPBACK;
	}
	printk(KERN_DEBUG "%s: PHY loopback test ends\n", dev->name);

	printk(KERN_DEBUG "%s: MAC loopback test starts\n", dev->name);
	e100_hw_init(dev->priv);
	if (!e100_diag_one_loopback(dev, MAC_LOOPBACK)) {
		rc |= MAC_LOOPBACK;
	}
	printk(KERN_DEBUG "%s: MAC loopback test ends\n", dev->name);

	return rc;
}

/**
 * e100_diag_loopback - performs loopback test  
 * @dev: atapter's net device data struct
 * @mode: lopback test type
 */
static u8
e100_diag_one_loopback (struct net_device *dev, u8 mode)
{
        struct e100_private *bdp = dev->priv;
        u8 res = false;
   	u8 saved_dynamic_tbd = false;
   	u8 saved_extended_tcb = false;

	if (!e100_diag_loopback_alloc(bdp))
		return false;

	/* change the config block to standard tcb and the correct loopback */
        e100_diag_config_loopback(bdp, true, mode,
				  &saved_extended_tcb, &saved_dynamic_tbd);

	e100_diag_loopback_cu_ru_exec(bdp);

        if (e100_diag_rcv_loopback_pkt(bdp)) {
		res = true;
	}

        e100_diag_loopback_free(bdp);

        /* change the config block to previous tcb mode and the no loopback */
        e100_diag_config_loopback(bdp, false, mode,
				  &saved_extended_tcb, &saved_dynamic_tbd);
	return res;
}

/**
 * e100_diag_config_loopback - setup/clear loopback before/after lpbk test
 * @bdp: atapter's private data struct
 * @set_loopback: true if the function is called to set lb
 * @loopback_mode: the loopback mode(MAC or PHY)
 * @tcb_extended: true if need to set extended tcb mode after clean loopback
 * @dynamic_tbd: true if needed to set dynamic tbd mode after clean loopback
 *
 */
void
e100_diag_config_loopback(struct e100_private* bdp,
			  u8 set_loopback,
			  u8 loopback_mode,
			  u8* tcb_extended,
			  u8* dynamic_tbd)
{
	/* if set_loopback == true - we want to clear tcb_extended/dynamic_tbd.
	 * the previous values are saved in the params tcb_extended/dynamic_tbd
	 * if set_loopback == false - we want to restore previous value.
	 */
	if (set_loopback || (*tcb_extended))
		  *tcb_extended = e100_config_tcb_ext_enable(bdp,*tcb_extended);

	if (set_loopback || (*dynamic_tbd))
		 *dynamic_tbd = e100_config_dynamic_tbd(bdp,*dynamic_tbd);

	if (set_loopback) {
		/* ICH PHY loopback is broken */
		if (bdp->flags & IS_ICH && loopback_mode == PHY_LOOPBACK)
			loopback_mode = MAC_LOOPBACK;
		/* Configure loopback on MAC */
		e100_config_loopback_mode(bdp,loopback_mode);
	} else {
		e100_config_loopback_mode(bdp,NO_LOOPBACK);
	}

	e100_config(bdp);

	if (loopback_mode == PHY_LOOPBACK) {
		if (set_loopback)
                        /* Set PHY loopback mode */
                        e100_phy_set_loopback(bdp);
		else
			/* Reset PHY loopback mode */
			e100_phy_reset(bdp);	
		/* Wait for PHY state change */
		set_current_state(TASK_UNINTERRUPTIBLE);
                schedule_timeout(HZ);
	} else { /* For MAC loopback wait 500 msec to take effect */
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout(HZ / 2);
	}
}
  
/**
 * e100_diag_loopback_alloc - alloc & initate tcb and rfd for the loopback
 * @bdp: atapter's private data struct
 *
 */
static u8
e100_diag_loopback_alloc(struct e100_private *bdp)
{
	dma_addr_t dma_handle;
	tcb_t *tcb;
	rfd_t *rfd;
	tbd_t *tbd;

	/* tcb, tbd and transmit buffer are allocated */
	tcb = pci_alloc_consistent(bdp->pdev,
				   (sizeof (tcb_t) + sizeof (tbd_t) +
				    LB_PACKET_SIZE),
				   &dma_handle);
        if (tcb == NULL)
		return false;

	memset(tcb, 0x00, sizeof (tcb_t) + sizeof (tbd_t) + LB_PACKET_SIZE);
	tcb->tcb_phys = dma_handle;
	tcb->tcb_hdr.cb_status = 0;
	tcb->tcb_hdr.cb_cmd =
		cpu_to_le16(CB_EL_BIT | CB_TRANSMIT | CB_TX_SF_BIT);
	/* Next command is null */
	tcb->tcb_hdr.cb_lnk_ptr = cpu_to_le32(0xffffffff);
	tcb->tcb_cnt = 0;
	tcb->tcb_thrshld = bdp->tx_thld;
	tcb->tcb_tbd_num = 1;
	/* Set up tcb tbd pointer */
	tcb->tcb_tbd_ptr = cpu_to_le32(tcb->tcb_phys + sizeof (tcb_t));
	tbd = (tbd_t *) ((u8 *) tcb + sizeof (tcb_t));
	/* Set up tbd transmit buffer */
	tbd->tbd_buf_addr =
		cpu_to_le32(le32_to_cpu(tcb->tcb_tbd_ptr) + sizeof (tbd_t));
	tbd->tbd_buf_cnt = __constant_cpu_to_le16(1024);
	/* The value of first 512 bytes is FF */
	memset((void *) ((u8 *) tbd + sizeof (tbd_t)), 0xFF, 512);
	/* The value of second 512 bytes is BA */
	memset((void *) ((u8 *) tbd + sizeof (tbd_t) + 512), 0xBA, 512);
	wmb();
	rfd = pci_alloc_consistent(bdp->pdev, sizeof (rfd_t), &dma_handle);

	if (rfd == NULL) {
		pci_free_consistent(bdp->pdev,
				    sizeof (tcb_t) + sizeof (tbd_t) +
				    LB_PACKET_SIZE, tcb, tcb->tcb_phys);
		return false;
	}

	memset(rfd, 0x00, sizeof (rfd_t));

	/* init all fields in rfd */
	rfd->rfd_header.cb_cmd = cpu_to_le16(RFD_EL_BIT);
	rfd->rfd_sz = cpu_to_le16(ETH_FRAME_LEN + CHKSUM_SIZE);
	/* dma_handle is physical address of rfd */
	bdp->loopback.dma_handle = dma_handle;
	bdp->loopback.tcb = tcb;
	bdp->loopback.rfd = rfd;
	wmb();
	return true;
}

/**
 * e100_diag_loopback_cu_ru_exec - activates cu and ru to send & receive the pkt
 * @bdp: atapter's private data struct
 *
 */
static void
e100_diag_loopback_cu_ru_exec(struct e100_private *bdp)
{
	/*load CU & RU base */ 
	if(!e100_wait_exec_cmplx(bdp, bdp->loopback.dma_handle, SCB_RUC_START, 0))
		printk(KERN_ERR "e100: SCB_RUC_START failed!\n");

	bdp->next_cu_cmd = START_WAIT;
	e100_start_cu(bdp, bdp->loopback.tcb);
	bdp->last_tcb = NULL;
	rmb();
}
/**
 * e100_diag_check_pkt - checks if a given packet is a loopback packet
 * @bdp: atapter's private data struct
 *
 * Returns true if OK false otherwise.
 */
static u8
e100_diag_check_pkt(u8 *datap)
{
	int i;
	for (i = 0; i<512; i++) {
		if( !((*datap)==0xFF && (*(datap + 512) == 0xBA)) ) {
			printk (KERN_ERR "e100: check loopback packet failed at: %x\n", i);
			return false;
			}
	}
	printk (KERN_DEBUG "e100: Check received loopback packet OK\n");
	return true;
}

/**
 * e100_diag_rcv_loopback_pkt - waits for receive and checks lpbk packet
 * @bdp: atapter's private data struct
 *
 * Returns true if OK false otherwise.
 */
static u8
e100_diag_rcv_loopback_pkt(struct e100_private* bdp) 
{    
	rfd_t *rfdp;
	u16 rfd_status;
	unsigned long expires = jiffies + HZ * 2;

        rfdp =bdp->loopback.rfd;

        rfd_status = le16_to_cpu(rfdp->rfd_header.cb_status);

        while (!(rfd_status & RFD_STATUS_COMPLETE)) { 
		if (time_before(jiffies, expires)) {
			yield();
			rmb();
			rfd_status = le16_to_cpu(rfdp->rfd_header.cb_status);
		} else {
			break;
		}
        }

        if (rfd_status & RFD_STATUS_COMPLETE) {
		printk(KERN_DEBUG "e100: Loopback packet received\n");
                return e100_diag_check_pkt(((u8 *)rfdp+bdp->rfd_size));
	}
	else {
		printk(KERN_ERR "e100: Loopback packet not received\n");
		return false;
	}
}

/**
 * e100_diag_loopback_free - free data allocated for loopback pkt send/receive
 * @bdp: atapter's private data struct
 *
 */
static void
e100_diag_loopback_free (struct e100_private *bdp)
{
        pci_free_consistent(bdp->pdev,
			    sizeof(tcb_t) + sizeof(tbd_t) + LB_PACKET_SIZE,
			    bdp->loopback.tcb, bdp->loopback.tcb->tcb_phys);

        pci_free_consistent(bdp->pdev, sizeof(rfd_t), bdp->loopback.rfd,
			    bdp->loopback.dma_handle);
}

static int
e100_cable_diag(struct e100_private *bdp)
{	
	int saved_open_circut = 0xffff;
	int saved_short_circut = 0xffff;
	int saved_distance = 0xffff;
	int saved_same = 0;
	int cable_status = E100_CABLE_UNKNOWN;
	int i;
	
	/* If we have link, */	
	if (e100_get_link_state(bdp))
		return E100_CABLE_OK;
	
	if (bdp->rev_id < D102_REV_ID)
		return E100_CABLE_UNKNOWN;

	/* Disable MDI/MDI-X auto switching */
        e100_mdi_write(bdp, MII_NCONFIG, bdp->phy_addr,
		MDI_MDIX_RESET_ALL_MASK);
	/* Set to 100 Full as required by cable test */
	e100_mdi_write(bdp, MII_BMCR, bdp->phy_addr,
		BMCR_SPEED100 | BMCR_FULLDPLX);

	/* Test up to 100 times */
	for (i = 0; i < 100; i++) {
		u16 ctrl_reg;
		int distance, open_circut, short_circut, near_end;

		/* Enable and execute cable test */
		e100_mdi_write(bdp, HWI_CONTROL_REG, bdp->phy_addr,
			(HWI_TEST_ENABLE | HWI_TEST_EXECUTE));
		/* Wait for cable test finished */
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout(HZ/100 + 1);
		/* Read results */
		e100_mdi_read(bdp, HWI_CONTROL_REG, bdp->phy_addr, &ctrl_reg);
		distance = ctrl_reg & HWI_TEST_DISTANCE;
		open_circut = ctrl_reg & HWI_TEST_HIGHZ_PROBLEM;
		short_circut = ctrl_reg & HWI_TEST_LOWZ_PROBLEM;

		if ((distance == saved_distance) &&
	    	    (open_circut == saved_open_circut) &&
	    	    (short_circut == saved_short_circut)) 
			saved_same++;
		else {
			saved_same = 0;
			saved_distance = distance;
			saved_open_circut = open_circut;
			saved_short_circut = short_circut;
		}
		/* If results are the same 3 times */
		if (saved_same == 3) {
			near_end = ((distance * HWI_REGISTER_GRANULARITY) <
			       HWI_NEAR_END_BOUNDARY);
			if (open_circut)
				cable_status = (near_end) ? 
					E100_CABLE_OPEN_NEAR : E100_CABLE_OPEN_FAR;
			if (short_circut)
				cable_status = (near_end) ?
					E100_CABLE_SHORT_NEAR : E100_CABLE_SHORT_FAR;
			break;
		}
	}
	/* Reset cable test */
        e100_mdi_write(bdp, HWI_CONTROL_REG, bdp->phy_addr,					       HWI_RESET_ALL_MASK);
	return cable_status;
}

