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

/**********************************************************************
*                                                                     *
* INTEL CORPORATION                                                   *
*                                                                     *
* This software is supplied under the terms of the license included   *
* above.  All use of this driver must be in accordance with the terms *
* of that license.                                                    *
*                                                                     *
* Module Name:  e100_main.c                                           *
*                                                                     *
* Abstract:     Functions for the driver entry points like load,      *
*               unload, open and close. All board specific calls made *
*               by the network interface section of the driver.       *
*                                                                     *
* Environment:  This file is intended to be specific to the Linux     *
*               operating system.                                     *
*                                                                     *
**********************************************************************/

/* Change Log
 * 
 * 2.3.38       12/14/03
 * o Added netpoll support.
 * o Added ICH6 device ID support
 * o Moved to 2.6 APIs: pci_name() and free_netdev().
 * o Removed some __devinit from some functions that shouldn't be marked
 *   as such (Anton Blanchard [anton@samba.org]).
 * 
 * 2.3.33       10/21/03
 * o Bug fix (Bugzilla 97908): Loading e100 was causing crash on Itanium2
 *   with HP chipset
 * o Bug fix (Bugzilla 101583): e100 can't pass traffic with ipv6
 * o Bug fix (Bugzilla 101360): PRO/10+ can't pass traffic
 * 
 * 2.3.27       08/08/03
 */
 
#include <linux/config.h>
#include <net/checksum.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include "e100.h"
#include "e100_ucode.h"
#include "e100_config.h"
#include "e100_phy.h"

extern void e100_force_speed_duplex_to_phy(struct e100_private *bdp);

static char e100_gstrings_stats[][ETH_GSTRING_LEN] = {
	"rx_packets", "tx_packets", "rx_bytes", "tx_bytes", "rx_errors",
	"tx_errors", "rx_dropped", "tx_dropped", "multicast", "collisions",
	"rx_length_errors", "rx_over_errors", "rx_crc_errors",
	"rx_frame_errors", "rx_fifo_errors", "rx_missed_errors",
	"tx_aborted_errors", "tx_carrier_errors", "tx_fifo_errors",
	"tx_heartbeat_errors", "tx_window_errors",
	/* device-specific stats */
	"tx_late_collision_errors", "tx_deferred", 
	"tx_single_collisions", "tx_multi_collisions", 
	"rx_collision_detected_errors", "tx_flow_control_pause",
	"rx_flow_control_pause", "rx_flow_control_unsupported",
	"tx_tco_packets", "rx_tco_packets",
};
#define E100_NET_STATS_LEN	21
#define E100_STATS_LEN	sizeof(e100_gstrings_stats) / ETH_GSTRING_LEN

static int e100_do_ethtool_ioctl(struct net_device *, struct ifreq *);
static void e100_get_speed_duplex_caps(struct e100_private *);
static int e100_ethtool_get_settings(struct net_device *, struct ifreq *);
static int e100_ethtool_set_settings(struct net_device *, struct ifreq *);

static int e100_ethtool_get_drvinfo(struct net_device *, struct ifreq *);
static int e100_ethtool_eeprom(struct net_device *, struct ifreq *);

#define E100_EEPROM_MAGIC 0x1234
static int e100_ethtool_glink(struct net_device *, struct ifreq *);
static int e100_ethtool_gregs(struct net_device *, struct ifreq *);
static int e100_ethtool_nway_rst(struct net_device *, struct ifreq *);
static int e100_ethtool_wol(struct net_device *, struct ifreq *);
#ifdef CONFIG_PM
static unsigned char e100_setup_filter(struct e100_private *bdp);
static void e100_do_wol(struct pci_dev *pcid, struct e100_private *bdp);
#endif
static u16 e100_get_ip_lbytes(struct net_device *dev);
extern void e100_config_wol(struct e100_private *bdp);
extern u32 e100_run_diag(struct net_device *dev, u64 *test_info, u32 flags);
static int e100_ethtool_test(struct net_device *, struct ifreq *);
static int e100_ethtool_gstrings(struct net_device *, struct ifreq *);
static char test_strings[][ETH_GSTRING_LEN] = {
	"Link test     (on/offline)",
	"Eeprom test   (on/offline)",
	"Self test        (offline)",
	"Mac loopback     (offline)",
	"Phy loopback     (offline)",
	"Cable diagnostic (offline)"
};

static int e100_ethtool_led_blink(struct net_device *, struct ifreq *);

static int e100_mii_ioctl(struct net_device *, struct ifreq *, int);

static unsigned char e100_delayed_exec_non_cu_cmd(struct e100_private *,
						  nxmit_cb_entry_t *);
static void e100_free_nontx_list(struct e100_private *);
static void e100_non_tx_background(unsigned long);
static inline void e100_tx_skb_free(struct e100_private *bdp, tcb_t *tcb);
/* Global Data structures and variables */
char e100_copyright[] __devinitdata = "Copyright (c) 2004 Intel Corporation";
char e100_driver_version[]="2.3.38-k1";
const char *e100_full_driver_name = "Intel(R) PRO/100 Network Driver";
char e100_short_driver_name[] = "e100";
static int e100nics = 0;
static void e100_vlan_rx_register(struct net_device *netdev, struct vlan_group
		*grp);
static void e100_vlan_rx_add_vid(struct net_device *netdev, u16 vid);
static void e100_vlan_rx_kill_vid(struct net_device *netdev, u16 vid);

#ifdef CONFIG_NET_POLL_CONTROLLER
/* for netdump / net console */
static void e100_netpoll (struct net_device *dev);
#endif

#ifdef CONFIG_PM
static int e100_notify_reboot(struct notifier_block *, unsigned long event, void *ptr);
static int e100_suspend(struct pci_dev *pcid, u32 state);
static int e100_resume(struct pci_dev *pcid);
static unsigned char e100_asf_enabled(struct e100_private *bdp);
struct notifier_block e100_notifier_reboot = {
        .notifier_call  = e100_notify_reboot,
        .next           = NULL,
        .priority       = 0
};
#endif

/*********************************************************************/
/*! This is a GCC extension to ANSI C.
 *  See the item "Labeled Elements in Initializers" in the section
 *  "Extensions to the C Language Family" of the GCC documentation.
 *********************************************************************/
#define E100_PARAM_INIT { [0 ... E100_MAX_NIC] = -1 }

/* All parameters are treated the same, as an integer array of values.
 * This macro just reduces the need to repeat the same declaration code
 * over and over (plus this helps to avoid typo bugs).
 */
#define E100_PARAM(X, S)                                        \
        static const int X[E100_MAX_NIC + 1] = E100_PARAM_INIT; \
        MODULE_PARM(X, "1-" __MODULE_STRING(E100_MAX_NIC) "i"); \
        MODULE_PARM_DESC(X, S);

/* ====================================================================== */
static u8 e100_D101M_checksum(struct e100_private *, struct sk_buff *);
static u8 e100_D102_check_checksum(rfd_t *);
static int e100_ioctl(struct net_device *, struct ifreq *, int);
static int e100_change_mtu(struct net_device *, int);
static int e100_xmit_frame(struct sk_buff *, struct net_device *);
static unsigned char e100_init(struct e100_private *);
static int e100_set_mac(struct net_device *, void *);
struct net_device_stats *e100_get_stats(struct net_device *);

static irqreturn_t e100intr(int, void *, struct pt_regs *);
static void e100_print_brd_conf(struct e100_private *);
static void e100_set_multi(struct net_device *);

static u8 e100_pci_setup(struct pci_dev *, struct e100_private *);
static u8 e100_sw_init(struct e100_private *);
static void e100_tco_workaround(struct e100_private *);
static unsigned char e100_alloc_space(struct e100_private *);
static void e100_dealloc_space(struct e100_private *);
static int e100_alloc_tcb_pool(struct e100_private *);
static void e100_setup_tcb_pool(tcb_t *, unsigned int, struct e100_private *);
static void e100_free_tcb_pool(struct e100_private *);
static int e100_alloc_rfd_pool(struct e100_private *);
static void e100_free_rfd_pool(struct e100_private *);

static void e100_rd_eaddr(struct e100_private *);
static void e100_rd_pwa_no(struct e100_private *);
extern u16 e100_eeprom_read(struct e100_private *, u16);
extern void e100_eeprom_write_block(struct e100_private *, u16, u16 *, u16);
extern u16 e100_eeprom_size(struct e100_private *);
u16 e100_eeprom_calculate_chksum(struct e100_private *adapter);

static unsigned char e100_clr_cntrs(struct e100_private *);
static unsigned char e100_load_microcode(struct e100_private *);
static unsigned char e100_setup_iaaddr(struct e100_private *, u8 *);
static unsigned char e100_update_stats(struct e100_private *bdp);

static void e100_start_ru(struct e100_private *);
static void e100_dump_stats_cntrs(struct e100_private *);

static void e100_check_options(int board, struct e100_private *bdp);
static void e100_set_int_option(int *, int, int, int, int, char *);
static void e100_set_bool_option(struct e100_private *bdp, int, u32, int,
				 char *);
unsigned char e100_wait_exec_cmplx(struct e100_private *, u32, u8, u8);
void e100_exec_cmplx(struct e100_private *, u32, u8);

/**
 * e100_get_rx_struct - retrieve cell to hold skb buff from the pool
 * @bdp: atapter's private data struct
 *
 * Returns the new cell to hold sk_buff or %NULL.
 */
static inline struct rx_list_elem *
e100_get_rx_struct(struct e100_private *bdp)
{
	struct rx_list_elem *rx_struct = NULL;

	if (!list_empty(&(bdp->rx_struct_pool))) {
		rx_struct = list_entry(bdp->rx_struct_pool.next,
				       struct rx_list_elem, list_elem);
		list_del(&(rx_struct->list_elem));
	}

	return rx_struct;
}

/**
 * e100_alloc_skb - allocate an skb for the adapter
 * @bdp: atapter's private data struct
 *
 * Allocates skb with enough room for rfd, and data, and reserve non-data space.
 * Returns the new cell with sk_buff or %NULL.
 */
static inline struct rx_list_elem *
e100_alloc_skb(struct e100_private *bdp)
{
	struct sk_buff *new_skb;
	u32 skb_size = sizeof (rfd_t);
	struct rx_list_elem *rx_struct;

	new_skb = (struct sk_buff *) dev_alloc_skb(skb_size);
	if (new_skb) {
		/* The IP data should be 
		   DWORD aligned. since the ethernet header is 14 bytes long, 
		   we need to reserve 2 extra bytes so that the TCP/IP headers
		   will be DWORD aligned. */
		skb_reserve(new_skb, 2);
		if ((rx_struct = e100_get_rx_struct(bdp)) == NULL)
			goto err;
		rx_struct->skb = new_skb;
		rx_struct->dma_addr = pci_map_single(bdp->pdev, new_skb->data,
						     sizeof (rfd_t),
						     PCI_DMA_FROMDEVICE);
		if (!rx_struct->dma_addr)
			goto err;
		skb_reserve(new_skb, bdp->rfd_size);
		return rx_struct;
	} else {
		return NULL;
	}

err:
	dev_kfree_skb_irq(new_skb);
	return NULL;
}

/**
 * e100_add_skb_to_end - add an skb to the end of our rfd list
 * @bdp: atapter's private data struct
 * @rx_struct: rx_list_elem with the new skb
 *
 * Adds a newly allocated skb to the end of our rfd list.
 */
inline void
e100_add_skb_to_end(struct e100_private *bdp, struct rx_list_elem *rx_struct)
{
	rfd_t *rfdn;		/* The new rfd */
	rfd_t *rfd;		/* The old rfd */
	struct rx_list_elem *rx_struct_last;

	(rx_struct->skb)->dev = bdp->device;
	rfdn = RFD_POINTER(rx_struct->skb, bdp);
	rfdn->rfd_header.cb_status = 0;
	rfdn->rfd_header.cb_cmd = __constant_cpu_to_le16(RFD_EL_BIT);
	rfdn->rfd_act_cnt = 0;
	rfdn->rfd_sz = __constant_cpu_to_le16(RFD_DATA_SIZE);

	pci_dma_sync_single(bdp->pdev, rx_struct->dma_addr, bdp->rfd_size,
			    PCI_DMA_TODEVICE);

	if (!list_empty(&(bdp->active_rx_list))) {
		rx_struct_last = list_entry(bdp->active_rx_list.prev,
					    struct rx_list_elem, list_elem);
		rfd = RFD_POINTER(rx_struct_last->skb, bdp);
		pci_dma_sync_single(bdp->pdev, rx_struct_last->dma_addr,
				    4, PCI_DMA_FROMDEVICE);
		put_unaligned(cpu_to_le32(rx_struct->dma_addr),
			      ((u32 *) (&(rfd->rfd_header.cb_lnk_ptr))));

		pci_dma_sync_single(bdp->pdev, rx_struct_last->dma_addr,
				    8, PCI_DMA_TODEVICE);
		rfd->rfd_header.cb_cmd &=
			__constant_cpu_to_le16((u16) ~RFD_EL_BIT);

		pci_dma_sync_single(bdp->pdev, rx_struct_last->dma_addr,
				    4, PCI_DMA_TODEVICE);
	}

	list_add_tail(&(rx_struct->list_elem), &(bdp->active_rx_list));
}

static inline void
e100_alloc_skbs(struct e100_private *bdp)
{
	for (; bdp->skb_req > 0; bdp->skb_req--) {
		struct rx_list_elem *rx_struct;

		if ((rx_struct = e100_alloc_skb(bdp)) == NULL)
			return;

		e100_add_skb_to_end(bdp, rx_struct);
	}
}

void e100_tx_srv(struct e100_private *);
u32 e100_rx_srv(struct e100_private *);

void e100_watchdog(struct net_device *);
void e100_refresh_txthld(struct e100_private *);
void e100_manage_adaptive_ifs(struct e100_private *);
void e100_clear_pools(struct e100_private *);
static void e100_clear_structs(struct net_device *);
static inline tcb_t *e100_prepare_xmit_buff(struct e100_private *,
					    struct sk_buff *);
static void e100_set_multi_exec(struct net_device *dev);

MODULE_AUTHOR("Intel Corporation, <linux.nics@intel.com>");
MODULE_DESCRIPTION("Intel(R) PRO/100 Network Driver");
MODULE_LICENSE("GPL");

E100_PARAM(TxDescriptors, "Number of transmit descriptors");
E100_PARAM(RxDescriptors, "Number of receive descriptors");
E100_PARAM(XsumRX, "Disable or enable Receive Checksum offload");
E100_PARAM(e100_speed_duplex, "Speed and Duplex settings");
E100_PARAM(ucode, "Disable or enable microcode loading");
E100_PARAM(ber, "Value for the BER correction algorithm");
E100_PARAM(flow_control, "Disable or enable Ethernet PAUSE frames processing");
E100_PARAM(IntDelay, "Value for CPU saver's interrupt delay");
E100_PARAM(BundleSmallFr, "Disable or enable interrupt bundling of small frames");
E100_PARAM(BundleMax, "Maximum number for CPU saver's packet bundling");
E100_PARAM(IFS, "Disable or enable the adaptive IFS algorithm");

/**
 * e100_exec_cmd - issue a comand
 * @bdp: atapter's private data struct
 * @scb_cmd_low: the command that is to be issued
 *
 * This general routine will issue a command to the e100.
 */
static inline void
e100_exec_cmd(struct e100_private *bdp, u8 cmd_low)
{
	writeb(cmd_low, &(bdp->scb->scb_cmd_low));
	readw(&(bdp->scb->scb_status));	/* flushes last write, read-safe */
}

/**
 * e100_wait_scb - wait for SCB to clear
 * @bdp: atapter's private data struct
 *
 * This routine checks to see if the e100 has accepted a command.
 * It does so by checking the command field in the SCB, which will
 * be zeroed by the e100 upon accepting a command.  The loop waits
 * for up to 1 millisecond for command acceptance.
 *
 * Returns:
 *      true if the SCB cleared within 1 millisecond.
 *      false if it didn't clear within 1 millisecond
 */
unsigned char
e100_wait_scb(struct e100_private *bdp)
{
	int i;

	/* loop on the scb for a few times */
	for (i = 0; i < 100; i++) {
		if (!readb(&bdp->scb->scb_cmd_low))
			return true;
		cpu_relax();
	}

	/* it didn't work. do it the slow way using udelay()s */
	for (i = 0; i < E100_MAX_SCB_WAIT; i++) {
		if (!readb(&bdp->scb->scb_cmd_low))
			return true;
		cpu_relax();
		udelay(1);
	}

	return false;
}

/**
 * e100_wait_exec_simple - issue a command
 * @bdp: atapter's private data struct
 * @scb_cmd_low: the command that is to be issued
 *
 * This general routine will issue a command to the e100 after waiting for
 * the previous command to finish.
 *
 * Returns:
 *      true if the command was issued to the chip successfully
 *      false if the command was not issued to the chip
 */
inline unsigned char
e100_wait_exec_simple(struct e100_private *bdp, u8 scb_cmd_low)
{
	if (!e100_wait_scb(bdp)) {
		printk(KERN_DEBUG "e100: %s: e100_wait_exec_simple: failed\n",
		       bdp->device->name);
#ifdef E100_CU_DEBUG		
		printk(KERN_ERR "e100: %s: Last command (%x/%x) "
			"timeout\n", bdp->device->name, 
			bdp->last_cmd, bdp->last_sub_cmd);
		printk(KERN_ERR "e100: %s: Current simple command (%x) "
			"can't be executed\n", 
			bdp->device->name, scb_cmd_low);
#endif		
		return false;
	}
	e100_exec_cmd(bdp, scb_cmd_low);
#ifdef E100_CU_DEBUG	
	bdp->last_cmd = scb_cmd_low;
	bdp->last_sub_cmd = 0;
#endif	
	return true;
}

void
e100_exec_cmplx(struct e100_private *bdp, u32 phys_addr, u8 cmd)
{
	writel(phys_addr, &(bdp->scb->scb_gen_ptr));
	readw(&(bdp->scb->scb_status));	/* flushes last write, read-safe */
	e100_exec_cmd(bdp, cmd);
}

unsigned char
e100_wait_exec_cmplx(struct e100_private *bdp, u32 phys_addr, u8 cmd, u8 sub_cmd)
{
	if (!e100_wait_scb(bdp)) {
#ifdef E100_CU_DEBUG		
		printk(KERN_ERR "e100: %s: Last command (%x/%x) "
			"timeout\n", bdp->device->name, 
			bdp->last_cmd, bdp->last_sub_cmd);
		printk(KERN_ERR "e100: %s: Current complex command "
			"(%x/%x) can't be executed\n", 
			bdp->device->name, cmd, sub_cmd);
#endif		
		return false;
	}
	e100_exec_cmplx(bdp, phys_addr, cmd);
#ifdef E100_CU_DEBUG	
	bdp->last_cmd = cmd;
	bdp->last_sub_cmd = sub_cmd;
#endif	
	return true;
}

inline u8
e100_wait_cus_idle(struct e100_private *bdp)
{
	int i;

	/* loop on the scb for a few times */
	for (i = 0; i < 100; i++) {
		if (((readw(&(bdp->scb->scb_status)) & SCB_CUS_MASK) !=
		     SCB_CUS_ACTIVE)) {
			return true;
		}
		cpu_relax();
	}

	for (i = 0; i < E100_MAX_CU_IDLE_WAIT; i++) {
		if (((readw(&(bdp->scb->scb_status)) & SCB_CUS_MASK) !=
		     SCB_CUS_ACTIVE)) {
			return true;
		}
		cpu_relax();
		udelay(1);
	}

	return false;
}

/**
 * e100_disable_clear_intr - disable and clear/ack interrupts
 * @bdp: atapter's private data struct
 *
 * This routine disables interrupts at the hardware, by setting
 * the M (mask) bit in the adapter's CSR SCB command word.
 * It also clear/ack interrupts.
 */
static inline void
e100_disable_clear_intr(struct e100_private *bdp)
{
	u16 intr_status;
	/* Disable interrupts on our PCI board by setting the mask bit */
	writeb(SCB_INT_MASK, &bdp->scb->scb_cmd_hi);
	intr_status = readw(&bdp->scb->scb_status);
	/* ack and clear intrs */
	writew(intr_status, &bdp->scb->scb_status);
	readw(&bdp->scb->scb_status);
}

/**
 * e100_set_intr_mask - set interrupts
 * @bdp: atapter's private data struct
 *
 * This routine sets interrupts at the hardware, by resetting
 * the M (mask) bit in the adapter's CSR SCB command word
 */
static inline void
e100_set_intr_mask(struct e100_private *bdp)
{
	writeb(bdp->intr_mask, &bdp->scb->scb_cmd_hi);
	readw(&(bdp->scb->scb_status)); /* flushes last write, read-safe */
}

static inline void
e100_trigger_SWI(struct e100_private *bdp)
{
	/* Trigger interrupt on our PCI board by asserting SWI bit */
	writeb(SCB_SOFT_INT, &bdp->scb->scb_cmd_hi);
	readw(&(bdp->scb->scb_status));	/* flushes last write, read-safe */
}

static int __devinit
e100_found1(struct pci_dev *pcid, const struct pci_device_id *ent)
{
	static int first_time = true;
	struct net_device *dev = NULL;
	struct e100_private *bdp = NULL;
	int rc = 0;
	u16 cal_checksum, read_checksum;

	dev = alloc_etherdev(sizeof (struct e100_private));
	if (dev == NULL) {
		printk(KERN_ERR "e100: Not able to alloc etherdev struct\n");
		rc = -ENODEV;
		goto out;
	}

	SET_MODULE_OWNER(dev);

	if (first_time) {
		first_time = false;
        	printk(KERN_NOTICE "%s - version %s\n",
	               e100_full_driver_name, e100_driver_version);
		printk(KERN_NOTICE "%s\n", e100_copyright);
		printk(KERN_NOTICE "\n");
	}

	bdp = dev->priv;
	bdp->pdev = pcid;
	bdp->device = dev;

	pci_set_drvdata(pcid, dev);

	bdp->flags = 0;
	bdp->ifs_state = 0;
	bdp->ifs_value = 0;
	bdp->scb = 0;

	init_timer(&bdp->nontx_timer_id);
	bdp->nontx_timer_id.data = (unsigned long) bdp;
	bdp->nontx_timer_id.function = (void *) &e100_non_tx_background;
	INIT_LIST_HEAD(&(bdp->non_tx_cmd_list));
	bdp->non_tx_command_state = E100_NON_TX_IDLE;

	init_timer(&bdp->watchdog_timer);
	bdp->watchdog_timer.data = (unsigned long) dev;
	bdp->watchdog_timer.function = (void *) &e100_watchdog;

	if ((rc = e100_pci_setup(pcid, bdp)) != 0) {
		goto err_dev;
	}

	if ((rc = e100_alloc_space(bdp)) != 0) {
		goto err_pci;
	}

	if (((bdp->pdev->device > 0x1030)
	       && (bdp->pdev->device < 0x103F))
	    || ((bdp->pdev->device >= 0x1050)
	       && (bdp->pdev->device <= 0x1057))
	    || ((bdp->pdev->device >= 0x1064)
	       && (bdp->pdev->device <= 0x106B))
	    || (bdp->pdev->device == 0x2449)
	    || (bdp->pdev->device == 0x2459)
	    || (bdp->pdev->device == 0x245D)) {
		bdp->rev_id = D101MA_REV_ID;	/* workaround for ICH3 */
		bdp->flags |= IS_ICH;
	}

	if (bdp->rev_id == 0xff)
		bdp->rev_id = 1;

	if ((u8) bdp->rev_id >= D101A4_REV_ID)
		bdp->flags |= IS_BACHELOR;

	if ((u8) bdp->rev_id >= D102_REV_ID) {
		bdp->flags |= USE_IPCB;
		bdp->rfd_size = 32;
	} else {
		bdp->rfd_size = 16;
	}

	dev->vlan_rx_register = e100_vlan_rx_register;
	dev->vlan_rx_add_vid = e100_vlan_rx_add_vid;
	dev->vlan_rx_kill_vid = e100_vlan_rx_kill_vid;
	dev->irq = pcid->irq;
	dev->open = &e100_open;
	dev->hard_start_xmit = &e100_xmit_frame;
	dev->stop = &e100_close;
	dev->change_mtu = &e100_change_mtu;
	dev->get_stats = &e100_get_stats;
	dev->set_multicast_list = &e100_set_multi;
	dev->set_mac_address = &e100_set_mac;
	dev->do_ioctl = &e100_ioctl;

#ifdef CONFIG_NET_POLL_CONTROLLER
	dev->poll_controller = e100_netpoll;
#endif

	if (bdp->flags & USE_IPCB)
	dev->features = NETIF_F_SG | NETIF_F_IP_CSUM |
			NETIF_F_HW_VLAN_TX | NETIF_F_HW_VLAN_RX;
		
	if ((rc = register_netdev(dev)) != 0) {
		goto err_dealloc;
	}

	e100_check_options(e100nics, bdp);

	if (!e100_init(bdp)) {
		printk(KERN_ERR "e100: Failed to initialize, instance #%d\n",
		       e100nics);
		rc = -ENODEV;
		goto err_unregister_netdev;
	}

	/* Check if checksum is valid */
	cal_checksum = e100_eeprom_calculate_chksum(bdp);
	read_checksum = e100_eeprom_read(bdp, (bdp->eeprom_size - 1));
	if (cal_checksum != read_checksum) {
                printk(KERN_ERR "e100: Corrupted EEPROM on instance #%d\n",
		       e100nics);
                rc = -ENODEV;
                goto err_unregister_netdev;
	}
	
	e100nics++;

	e100_get_speed_duplex_caps(bdp);

	printk(KERN_NOTICE
	       "e100: %s: %s\n", 
	       bdp->device->name, "Intel(R) PRO/100 Network Connection");
	e100_print_brd_conf(bdp);

	bdp->wolsupported = 0;
	bdp->wolopts = 0;
	if (bdp->rev_id >= D101A4_REV_ID)
		bdp->wolsupported = WAKE_PHY | WAKE_MAGIC;
	if (bdp->rev_id >= D101MA_REV_ID)
		bdp->wolsupported |= WAKE_UCAST | WAKE_ARP;
	
	/* Check if WoL is enabled on EEPROM */
	if (e100_eeprom_read(bdp, EEPROM_ID_WORD) & BIT_5) {
		/* Magic Packet WoL is enabled on device by default */
		/* if EEPROM WoL bit is TRUE                        */
		bdp->wolopts = WAKE_MAGIC;
	}

	printk(KERN_NOTICE "\n");

	goto out;

err_unregister_netdev:
	unregister_netdev(dev);
err_dealloc:
	e100_dealloc_space(bdp);
err_pci:
	iounmap(bdp->scb);
	pci_release_regions(pcid);
	pci_disable_device(pcid);
err_dev:
	pci_set_drvdata(pcid, NULL);
	free_netdev(dev);
out:
	return rc;
}

/**
 * e100_clear_structs - free resources
 * @dev: adapter's net_device struct
 *
 * Free all device specific structs, unmap i/o address, etc.
 */
static void __devexit
e100_clear_structs(struct net_device *dev)
{
	struct e100_private *bdp = dev->priv;

	iounmap(bdp->scb);
	pci_release_regions(bdp->pdev);
	pci_disable_device(bdp->pdev);

	e100_dealloc_space(bdp);
	pci_set_drvdata(bdp->pdev, NULL);
	free_netdev(dev);
}

static void __devexit
e100_remove1(struct pci_dev *pcid)
{
	struct net_device *dev;
	struct e100_private *bdp;

	if (!(dev = (struct net_device *) pci_get_drvdata(pcid)))
		return;

	bdp = dev->priv;

	unregister_netdev(dev);

	e100_sw_reset(bdp, PORT_SELECTIVE_RESET);

	if (bdp->non_tx_command_state != E100_NON_TX_IDLE) {
		del_timer_sync(&bdp->nontx_timer_id);
		e100_free_nontx_list(bdp);
		bdp->non_tx_command_state = E100_NON_TX_IDLE;
	}

	e100_clear_structs(dev);

	--e100nics;
}

static struct pci_device_id e100_id_table[] = {
	{0x8086, 0x1229, PCI_ANY_ID, PCI_ANY_ID, 0, 0, },
	{0x8086, 0x2449, PCI_ANY_ID, PCI_ANY_ID, 0, 0, },
	{0x8086, 0x1059, PCI_ANY_ID, PCI_ANY_ID, 0, 0, },
	{0x8086, 0x1209, PCI_ANY_ID, PCI_ANY_ID, 0, 0, },
  	{0x8086, 0x1029, PCI_ANY_ID, PCI_ANY_ID, 0, 0, },
	{0x8086, 0x1030, PCI_ANY_ID, PCI_ANY_ID, 0, 0, },	
	{0x8086, 0x1031, PCI_ANY_ID, PCI_ANY_ID, 0, 0, }, 
	{0x8086, 0x1032, PCI_ANY_ID, PCI_ANY_ID, 0, 0, },
	{0x8086, 0x1033, PCI_ANY_ID, PCI_ANY_ID, 0, 0, }, 
	{0x8086, 0x1034, PCI_ANY_ID, PCI_ANY_ID, 0, 0, }, 
	{0x8086, 0x1038, PCI_ANY_ID, PCI_ANY_ID, 0, 0, },
	{0x8086, 0x1039, PCI_ANY_ID, PCI_ANY_ID, 0, 0, },
	{0x8086, 0x103A, PCI_ANY_ID, PCI_ANY_ID, 0, 0, },
	{0x8086, 0x103B, PCI_ANY_ID, PCI_ANY_ID, 0, 0, },
	{0x8086, 0x103C, PCI_ANY_ID, PCI_ANY_ID, 0, 0, },
	{0x8086, 0x103D, PCI_ANY_ID, PCI_ANY_ID, 0, 0, },
	{0x8086, 0x103E, PCI_ANY_ID, PCI_ANY_ID, 0, 0, },
	{0x8086, 0x1050, PCI_ANY_ID, PCI_ANY_ID, 0, 0, },
	{0x8086, 0x1051, PCI_ANY_ID, PCI_ANY_ID, 0, 0, },
	{0x8086, 0x1052, PCI_ANY_ID, PCI_ANY_ID, 0, 0, },
	{0x8086, 0x1053, PCI_ANY_ID, PCI_ANY_ID, 0, 0, },
	{0x8086, 0x1054, PCI_ANY_ID, PCI_ANY_ID, 0, 0, },
	{0x8086, 0x1055, PCI_ANY_ID, PCI_ANY_ID, 0, 0, },
	{0x8086, 0x1064, PCI_ANY_ID, PCI_ANY_ID, 0, 0, },
	{0x8086, 0x1065, PCI_ANY_ID, PCI_ANY_ID, 0, 0, },
	{0x8086, 0x1066, PCI_ANY_ID, PCI_ANY_ID, 0, 0, },
	{0x8086, 0x1067, PCI_ANY_ID, PCI_ANY_ID, 0, 0, },
	{0x8086, 0x1068, PCI_ANY_ID, PCI_ANY_ID, 0, 0, },
	{0x8086, 0x1069, PCI_ANY_ID, PCI_ANY_ID, 0, 0, },
	{0x8086, 0x106A, PCI_ANY_ID, PCI_ANY_ID, 0, 0, },
	{0x8086, 0x106B, PCI_ANY_ID, PCI_ANY_ID, 0, 0, },
	{0x8086, 0x2459, PCI_ANY_ID, PCI_ANY_ID, 0, 0, },
	{0x8086, 0x245D, PCI_ANY_ID, PCI_ANY_ID, 0, 0, },
	{0,} /* This has to be the last entry*/
};
MODULE_DEVICE_TABLE(pci, e100_id_table);

static struct pci_driver e100_driver = {
	.name         = "e100",
	.id_table     = e100_id_table,
	.probe        = e100_found1,
	.remove       = __devexit_p(e100_remove1),
#ifdef CONFIG_PM
	.suspend      = e100_suspend,
	.resume       = e100_resume,
#endif
};

static int __init
e100_init_module(void)
{
	int ret;
        ret = pci_module_init(&e100_driver);

	if(ret >= 0) {
#ifdef CONFIG_PM
		register_reboot_notifier(&e100_notifier_reboot);
#endif 
	}

	return ret;
}

static void __exit
e100_cleanup_module(void)
{
#ifdef CONFIG_PM	
	unregister_reboot_notifier(&e100_notifier_reboot);
#endif 

	pci_unregister_driver(&e100_driver);
}

module_init(e100_init_module);
module_exit(e100_cleanup_module);

/**
 * e100_check_options - check command line options
 * @board: board number
 * @bdp: atapter's private data struct
 *
 * This routine does range checking on command-line options
 */
void __devinit
e100_check_options(int board, struct e100_private *bdp)
{
	if (board >= E100_MAX_NIC) {
		printk(KERN_NOTICE 
		       "e100: No configuration available for board #%d\n",
		       board);
		printk(KERN_NOTICE "e100: Using defaults for all values\n");
		board = E100_MAX_NIC;
	}

	e100_set_int_option(&(bdp->params.TxDescriptors), TxDescriptors[board],
			    E100_MIN_TCB, E100_MAX_TCB, E100_DEFAULT_TCB,
			    "TxDescriptor count");

	e100_set_int_option(&(bdp->params.RxDescriptors), RxDescriptors[board],
			    E100_MIN_RFD, E100_MAX_RFD, E100_DEFAULT_RFD,
			    "RxDescriptor count");

	e100_set_int_option(&(bdp->params.e100_speed_duplex),
			    e100_speed_duplex[board], 0, 4,
			    E100_DEFAULT_SPEED_DUPLEX, "speed/duplex mode");

	e100_set_int_option(&(bdp->params.ber), ber[board], 0, ZLOCK_MAX_ERRORS,
			    E100_DEFAULT_BER, "Bit Error Rate count");

	e100_set_bool_option(bdp, XsumRX[board], PRM_XSUMRX, E100_DEFAULT_XSUM,
			     "XsumRX value");

	/* Default ucode value depended on controller revision */
	if (bdp->rev_id >= D101MA_REV_ID) {
		e100_set_bool_option(bdp, ucode[board], PRM_UCODE,
				     E100_DEFAULT_UCODE, "ucode value");
	} else {
		e100_set_bool_option(bdp, ucode[board], PRM_UCODE, false,
				     "ucode value");
	}

	e100_set_bool_option(bdp, flow_control[board], PRM_FC, E100_DEFAULT_FC,
			     "flow control value");

	e100_set_bool_option(bdp, IFS[board], PRM_IFS, E100_DEFAULT_IFS,
			     "IFS value");

	e100_set_bool_option(bdp, BundleSmallFr[board], PRM_BUNDLE_SMALL,
			     E100_DEFAULT_BUNDLE_SMALL_FR,
			     "CPU saver bundle small frames value");

	e100_set_int_option(&(bdp->params.IntDelay), IntDelay[board], 0x0,
			    0xFFFF, E100_DEFAULT_CPUSAVER_INTERRUPT_DELAY,
			    "CPU saver interrupt delay value");

	e100_set_int_option(&(bdp->params.BundleMax), BundleMax[board], 0x1,
			    0xFFFF, E100_DEFAULT_CPUSAVER_BUNDLE_MAX,
			    "CPU saver bundle max value");

}

/**
 * e100_set_int_option - check and set an integer option
 * @option: a pointer to the relevant option field
 * @val: the value specified
 * @min: the minimum valid value
 * @max: the maximum valid value
 * @default_val: the default value
 * @name: the name of the option
 *
 * This routine does range checking on a command-line option.
 * If the option's value is '-1' use the specified default.
 * Otherwise, if the value is invalid, change it to the default.
 */
void __devinit
e100_set_int_option(int *option, int val, int min, int max, int default_val,
		    char *name)
{
	if (val == -1) {	/* no value specified. use default */
		*option = default_val;

	} else if ((val < min) || (val > max)) {
		printk(KERN_NOTICE
		       "e100: Invalid %s specified (%i). "
		       "Valid range is %i-%i\n",
		       name, val, min, max);
		printk(KERN_NOTICE "e100: Using default %s of %i\n", name,
		       default_val);
		*option = default_val;
	} else {
		printk(KERN_INFO "e100: Using specified %s of %i\n", name, val);
		*option = val;
	}
}

/**
 * e100_set_bool_option - check and set a boolean option
 * @bdp: atapter's private data struct
 * @val: the value specified
 * @mask: the mask for the relevant option
 * @default_val: the default value
 * @name: the name of the option
 *
 * This routine checks a boolean command-line option.
 * If the option's value is '-1' use the specified default.
 * Otherwise, if the value is invalid (not 0 or 1), 
 * change it to the default.
 */
void __devinit
e100_set_bool_option(struct e100_private *bdp, int val, u32 mask,
		     int default_val, char *name)
{
	if (val == -1) {
		if (default_val)
			bdp->params.b_params |= mask;

	} else if ((val != true) && (val != false)) {
		printk(KERN_NOTICE
		       "e100: Invalid %s specified (%i). "
		       "Valid values are %i/%i\n",
		       name, val, false, true);
		printk(KERN_NOTICE "e100: Using default %s of %i\n", name,
		       default_val);

		if (default_val)
			bdp->params.b_params |= mask;
	} else {
		printk(KERN_INFO "e100: Using specified %s of %i\n", name, val);
		if (val)
			bdp->params.b_params |= mask;
	}
}

int
e100_open(struct net_device *dev)
{
	struct e100_private *bdp;
	int rc = 0;

	bdp = dev->priv;

	/* setup the tcb pool */
	if (!e100_alloc_tcb_pool(bdp)) {
		rc = -ENOMEM;
		goto err_exit;
	}
	bdp->last_tcb = NULL;

	bdp->tcb_pool.head = 0;
	bdp->tcb_pool.tail = 1;	

	e100_setup_tcb_pool((tcb_t *) bdp->tcb_pool.data,
			    bdp->params.TxDescriptors, bdp);

	if (!e100_alloc_rfd_pool(bdp)) {
		rc = -ENOMEM;
		goto err_exit;
	}

	if (!e100_wait_exec_cmplx(bdp, 0, SCB_CUC_LOAD_BASE, 0)) {
		rc = -EAGAIN;
		goto err_exit;
	}

	if (!e100_wait_exec_cmplx(bdp, 0, SCB_RUC_LOAD_BASE, 0)) {
		rc = -EAGAIN;
		goto err_exit;
	}

	mod_timer(&(bdp->watchdog_timer), jiffies + (2 * HZ));

	if (dev->flags & IFF_UP)
		/* Otherwise process may sleep forever */
		netif_wake_queue(dev);
	else
		netif_start_queue(dev);

	e100_start_ru(bdp);
	if ((rc = request_irq(dev->irq, &e100intr, SA_SHIRQ,
			      dev->name, dev)) != 0) {
		del_timer_sync(&bdp->watchdog_timer);
		goto err_exit;
	}
	bdp->intr_mask = 0;
	e100_set_intr_mask(bdp);

	e100_force_config(bdp);

	goto exit;

err_exit:
	e100_clear_pools(bdp);
exit:
	return rc;
}

int
e100_close(struct net_device *dev)
{
	struct e100_private *bdp = dev->priv;

	e100_disable_clear_intr(bdp);
	free_irq(dev->irq, dev);
	bdp->intr_mask = SCB_INT_MASK;
	e100_isolate_driver(bdp);

	netif_carrier_off(bdp->device);
	bdp->cur_line_speed = 0;
	bdp->cur_dplx_mode = 0;
	e100_clear_pools(bdp);

	return 0;
}

static int
e100_change_mtu(struct net_device *dev, int new_mtu)
{
	if ((new_mtu < 68) || (new_mtu > (ETH_DATA_LEN + VLAN_SIZE)))
		return -EINVAL;

	dev->mtu = new_mtu;
	return 0;
}

static int
e100_xmit_frame(struct sk_buff *skb, struct net_device *dev)
{
	int rc = 0;
	int notify_stop = false;
	struct e100_private *bdp = dev->priv;

	if (!spin_trylock(&bdp->bd_non_tx_lock)) {
		notify_stop = true;
		rc = 1;
		goto exit2;
	}

	/* tcb list may be empty temporarily during releasing resources */
	if (!TCBS_AVAIL(bdp->tcb_pool) || (bdp->tcb_phys == 0) ||
	    (bdp->non_tx_command_state != E100_NON_TX_IDLE)) {
		notify_stop = true;
		rc = 1;
		goto exit1;
	}

	bdp->drv_stats.net_stats.tx_bytes += skb->len;

	e100_prepare_xmit_buff(bdp, skb);

	dev->trans_start = jiffies;

exit1:
	spin_unlock(&bdp->bd_non_tx_lock);
exit2:
	if (notify_stop) {
		netif_stop_queue(dev);
	}

	return rc;
}

/**
 * e100_get_stats - get driver statistics
 * @dev: adapter's net_device struct
 *
 * This routine is called when the OS wants the adapter's stats returned.
 * It returns the address of the net_device_stats stucture for the device.
 * If the statistics are currently being updated, then they might be incorrect
 * for a short while. However, since this cannot actually cause damage, no
 * locking is used.
 */
struct net_device_stats *
e100_get_stats(struct net_device *dev)
{
	struct e100_private *bdp = dev->priv;

	bdp->drv_stats.net_stats.tx_errors =
		bdp->drv_stats.net_stats.tx_carrier_errors +
		bdp->drv_stats.net_stats.tx_aborted_errors;

	bdp->drv_stats.net_stats.rx_errors =
		bdp->drv_stats.net_stats.rx_crc_errors +
		bdp->drv_stats.net_stats.rx_frame_errors +
		bdp->drv_stats.net_stats.rx_length_errors +
		bdp->drv_stats.rcv_cdt_frames;

	return &(bdp->drv_stats.net_stats);
}

/**
 * e100_set_mac - set the MAC address
 * @dev: adapter's net_device struct
 * @addr: the new address
 *
 * This routine sets the ethernet address of the board
 * Returns:
 * 0  - if successful
 * -1 - otherwise
 */
static int
e100_set_mac(struct net_device *dev, void *addr)
{
	struct e100_private *bdp;
	int rc = -1;
	struct sockaddr *p_sockaddr = (struct sockaddr *) addr;

	if (!is_valid_ether_addr(p_sockaddr->sa_data))
		return -EADDRNOTAVAIL;
	bdp = dev->priv;

	if (e100_setup_iaaddr(bdp, (u8 *) (p_sockaddr->sa_data))) {
		memcpy(&(dev->dev_addr[0]), p_sockaddr->sa_data, ETH_ALEN);
		rc = 0;
	}

	return rc;
}

static void
e100_set_multi_exec(struct net_device *dev)
{
	struct e100_private *bdp = dev->priv;
	mltcst_cb_t *mcast_buff;
	cb_header_t *cb_hdr;
	struct dev_mc_list *mc_list;
	unsigned int i;
	nxmit_cb_entry_t *cmd = e100_alloc_non_tx_cmd(bdp);

	if (cmd != NULL) {
		mcast_buff = &((cmd->non_tx_cmd)->ntcb.multicast);
		cb_hdr = &((cmd->non_tx_cmd)->ntcb.multicast.mc_cbhdr);
	} else {
		return;
	}

	/* initialize the multi cast command */
	cb_hdr->cb_cmd = __constant_cpu_to_le16(CB_MULTICAST);

	/* now fill in the rest of the multicast command */
	*(u16 *) (&(mcast_buff->mc_count)) = cpu_to_le16(dev->mc_count * 6);
	for (i = 0, mc_list = dev->mc_list;
	     (i < dev->mc_count) && (i < MAX_MULTICAST_ADDRS);
	     i++, mc_list = mc_list->next) {
		/* copy into the command */
		memcpy(&(mcast_buff->mc_addr[i * ETH_ALEN]),
		       (u8 *) &(mc_list->dmi_addr), ETH_ALEN);
	}

	if (!e100_exec_non_cu_cmd(bdp, cmd)) {
		printk(KERN_WARNING "e100: %s: Multicast setup failed\n", 
		       dev->name);
	}
}

/**
 * e100_set_multi - set multicast status
 * @dev: adapter's net_device struct
 *
 * This routine is called to add or remove multicast addresses, and/or to
 * change the adapter's promiscuous state.
 */
static void
e100_set_multi(struct net_device *dev)
{
	struct e100_private *bdp = dev->priv;
	unsigned char promisc_enbl;
	unsigned char mulcast_enbl;

	promisc_enbl = ((dev->flags & IFF_PROMISC) == IFF_PROMISC);
	mulcast_enbl = ((dev->flags & IFF_ALLMULTI) ||
			(dev->mc_count > MAX_MULTICAST_ADDRS));

	e100_config_promisc(bdp, promisc_enbl);
	e100_config_mulcast_enbl(bdp, mulcast_enbl);

	/* reconfigure the chip if something has changed in its config space */
	e100_config(bdp);

	if (promisc_enbl || mulcast_enbl) {
		return;	/* no need for Multicast Cmd */
	}

	/* get the multicast CB */
	e100_set_multi_exec(dev);
}

static int
e100_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{

	switch (cmd) {

	case SIOCETHTOOL:
		return e100_do_ethtool_ioctl(dev, ifr);
		break;

	case SIOCGMIIPHY:	/* Get address of MII PHY in use. */
	case SIOCGMIIREG:	/* Read MII PHY register. */
	case SIOCSMIIREG:	/* Write to MII PHY register. */
		return e100_mii_ioctl(dev, ifr, cmd);
		break;

	default:
		return -EOPNOTSUPP;
	}
	return 0;

}

/**
 * e100init - initialize the adapter
 * @bdp: atapter's private data struct
 *
 * This routine is called when this driver is loaded. This is the initialization
 * routine which allocates memory, configures the adapter and determines the
 * system resources.
 *
 * Returns:
 *      true: if successful
 *      false: otherwise
 */
static unsigned char __devinit
e100_init(struct e100_private *bdp)
{
	u32 st_timeout = 0;
	u32 st_result = 0;
	e100_sw_init(bdp);

	if (!e100_selftest(bdp, &st_timeout, &st_result)) {
        	if (st_timeout) {
			printk(KERN_ERR "e100: selftest timeout\n");
		} else {
			printk(KERN_ERR "e100: selftest failed. Results: %x\n",
					st_result);
		}
		return false;
	}
	else
		printk(KERN_DEBUG "e100: selftest OK.\n");

	/* read the MAC address from the eprom */
	e100_rd_eaddr(bdp);
	if (!is_valid_ether_addr(bdp->device->dev_addr)) {
		printk(KERN_ERR "e100: Invalid Ethernet address\n");
		return false;
	}
	/* read NIC's part number */
	e100_rd_pwa_no(bdp);

	if (!e100_hw_init(bdp))
		return false;
	/* Interrupts are enabled after device reset */
	e100_disable_clear_intr(bdp);

	return true;
}

/**
 * e100_sw_init - initialize software structs
 * @bdp: atapter's private data struct
 * 
 * This routine initializes all software structures. Sets up the
 * circular structures for the RFD's & TCB's. Allocates the per board
 * structure for storing adapter information. The CSR is also memory 
 * mapped in this routine.
 *
 * Returns :
 *      true: if S/W was successfully initialized
 *      false: otherwise
 */
static unsigned char __devinit
e100_sw_init(struct e100_private *bdp)
{
	bdp->next_cu_cmd = START_WAIT;	// init the next cu state

	/* 
	 * Set the value for # of good xmits per underrun. the value assigned
	 * here is an intelligent  suggested default. Nothing magical about it.
	 */
	bdp->tx_per_underrun = DEFAULT_TX_PER_UNDERRUN;

	/* get the default transmit threshold value */
	bdp->tx_thld = TX_THRSHLD;

	/* get the EPROM size */
	bdp->eeprom_size = e100_eeprom_size(bdp);

	/* Initialize our spinlocks */
	spin_lock_init(&(bdp->bd_lock));
	spin_lock_init(&(bdp->bd_non_tx_lock));
	spin_lock_init(&(bdp->config_lock));
	spin_lock_init(&(bdp->mdi_access_lock));
	/* Initialize configuration data */
	e100_config_init(bdp);

	return 1;
}

static void
e100_tco_workaround(struct e100_private *bdp)
{
	int i;

	/* Do software reset */
	e100_sw_reset(bdp, PORT_SOFTWARE_RESET);

	/* Do a dummy LOAD CU BASE command. */
	/* This gets us out of pre-driver to post-driver. */
	e100_exec_cmplx(bdp, 0, SCB_CUC_LOAD_BASE);

	/* Wait 20 msec for reset to take effect */
	set_current_state(TASK_UNINTERRUPTIBLE);
	schedule_timeout(HZ / 50 + 1);

	/* disable interrupts since they are enabled */
	/* after device reset                        */
	e100_disable_clear_intr(bdp);

	/* Wait for command to be cleared up to 1 sec */
	for (i=0; i<100; i++) {
		if (!readb(&bdp->scb->scb_cmd_low))
			break;
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout(HZ / 100 + 1);
	}

	/* Wait for TCO request bit in PMDR register to be clear */
	for (i=0; i<50; i++) {
		if (!(readb(&bdp->scb->scb_ext.d101m_scb.scb_pmdr) & BIT_1))
			break;
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout(HZ / 100 + 1);
	}
}

/**
 * e100_hw_init - initialized tthe hardware
 * @bdp: atapter's private data struct
 *
 * This routine performs a reset on the adapter, and configures the adapter.
 * This includes configuring the 82557 LAN controller, validating and setting
 * the node address, detecting and configuring the Phy chip on the adapter,
 * and initializing all of the on chip counters.
 *
 * Returns:
 *      true - If the adapter was initialized
 *      false - If the adapter failed initialization
 */
unsigned char
e100_hw_init(struct e100_private *bdp)
{
	if (!e100_phy_init(bdp))
		goto err;

	e100_sw_reset(bdp, PORT_SELECTIVE_RESET);

	/* Only 82559 or above needs TCO workaround */
	if (bdp->rev_id >= D101MA_REV_ID)
		e100_tco_workaround(bdp);

	/* Load the CU BASE (set to 0, because we use linear mode) */
	if (!e100_wait_exec_cmplx(bdp, 0, SCB_CUC_LOAD_BASE, 0))
		goto err;

	if (!e100_wait_exec_cmplx(bdp, 0, SCB_RUC_LOAD_BASE, 0))
		goto err;

	/* Load interrupt microcode  */
	if (e100_load_microcode(bdp)) {
		bdp->flags |= DF_UCODE_LOADED;
	}

	if ((u8) bdp->rev_id < D101A4_REV_ID)
		e100_config_init_82557(bdp);
		
	if (!e100_config(bdp))
		goto err;

	if (!e100_setup_iaaddr(bdp, bdp->device->dev_addr))
		goto err;

	/* Clear the internal counters */
	if (!e100_clr_cntrs(bdp))
		goto err;

	/* Change for 82558 enhancement */
	/* If 82558/9 and if the user has enabled flow control, set up the
	 * Flow Control Reg. in the CSR */
	if ((bdp->flags & IS_BACHELOR)
	    && (bdp->params.b_params & PRM_FC)) {
		writeb(DFLT_FC_THLD, &bdp->scb->scb_ext.d101_scb.scb_fc_thld);
		writeb(DFLT_FC_CMD,
		       &bdp->scb->scb_ext.d101_scb.scb_fc_xon_xoff);
	}

	return true;
err:
	printk(KERN_ERR "e100: hw init failed\n");
	return false;
}

/**
 * e100_setup_tcb_pool - setup TCB circular list
 * @head: Pointer to head of the allocated TCBs
 * @qlen: Number of elements in the queue
 * @bdp: atapter's private data struct
 * 
 * This routine arranges the contigiously allocated TCB's in a circular list.
 * Also does the one time initialization of the TCBs.
 */
static void
e100_setup_tcb_pool(tcb_t *head, unsigned int qlen, struct e100_private *bdp)
{
	int ele_no;
	tcb_t *pcurr_tcb;	/* point to current tcb */
	u32 next_phys;		/* the next phys addr */
	u16 txcommand = CB_S_BIT | CB_TX_SF_BIT;

	bdp->tx_count = 0;
	if (bdp->flags & USE_IPCB) {
		txcommand |= CB_IPCB_TRANSMIT | CB_CID_DEFAULT;
	} else if (bdp->flags & IS_BACHELOR) {
		txcommand |= CB_TRANSMIT | CB_CID_DEFAULT;
	} else {
		txcommand |= CB_TRANSMIT;
	}

	for (ele_no = 0, next_phys = bdp->tcb_phys, pcurr_tcb = head;
	     ele_no < qlen; ele_no++, pcurr_tcb++) {

		/* set the phys addr for this TCB, next_phys has not incr. yet */
		pcurr_tcb->tcb_phys = next_phys;
		next_phys += sizeof (tcb_t);

		/* set the link to next tcb */
		if (ele_no == (qlen - 1))
			pcurr_tcb->tcb_hdr.cb_lnk_ptr =
				cpu_to_le32(bdp->tcb_phys);
		else
			pcurr_tcb->tcb_hdr.cb_lnk_ptr = cpu_to_le32(next_phys);

		pcurr_tcb->tcb_hdr.cb_status = 0;
		pcurr_tcb->tcb_hdr.cb_cmd = cpu_to_le16(txcommand);
		pcurr_tcb->tcb_cnt = 0;	
		pcurr_tcb->tcb_thrshld = bdp->tx_thld;	
		if (ele_no < 2) {
			pcurr_tcb->tcb_hdr.cb_status =
				cpu_to_le16(CB_STATUS_COMPLETE);
		}
		pcurr_tcb->tcb_tbd_num = 1;

		if (bdp->flags & IS_BACHELOR) {
			pcurr_tcb->tcb_tbd_ptr =
				__constant_cpu_to_le32(0xFFFFFFFF);
		} else {
			pcurr_tcb->tcb_tbd_ptr =
				cpu_to_le32(pcurr_tcb->tcb_phys + 0x10);
		}

		if (bdp->flags & IS_BACHELOR) {
			pcurr_tcb->tcb_tbd_expand_ptr =
				cpu_to_le32(pcurr_tcb->tcb_phys + 0x20);
		} else {
			pcurr_tcb->tcb_tbd_expand_ptr =
				cpu_to_le32(pcurr_tcb->tcb_phys + 0x10);
		}
		pcurr_tcb->tcb_tbd_dflt_ptr = pcurr_tcb->tcb_tbd_ptr;

		if (bdp->flags & USE_IPCB) {
			pcurr_tcb->tbd_ptr = &(pcurr_tcb->tcbu.tbd_array[1]);
			pcurr_tcb->tcbu.ipcb.ip_activation_high =
				IPCB_IP_ACTIVATION_DEFAULT;
			pcurr_tcb->tcbu.ipcb.vlan = 0;
		} else {
			pcurr_tcb->tbd_ptr = &(pcurr_tcb->tcbu.tbd_array[0]);
		}

		pcurr_tcb->tcb_skb = NULL;
	}

	wmb();
}

/***************************************************************************/
/***************************************************************************/
/*       Memory Management Routines                                        */
/***************************************************************************/

/**
 * e100_alloc_space - allocate private driver data
 * @bdp: atapter's private data struct
 *
 * This routine allocates memory for the driver. Memory allocated is for the
 * selftest and statistics structures.
 *
 * Returns:
 *      0: if the operation was successful
 *      %-ENOMEM: if memory allocation failed
 */
unsigned char __devinit
e100_alloc_space(struct e100_private *bdp)
{
	unsigned long off;

	/* allocate all the dma-able structures in one call:
	 * selftest results, adapter stats, and non-tx cb commands */
	if (!(bdp->dma_able =
	      pci_alloc_consistent(bdp->pdev, sizeof (bd_dma_able_t),
				   &(bdp->dma_able_phys)))) {
		goto err;
	}

	/* now assign the various pointers into the struct we've just allocated */
	off = offsetof(bd_dma_able_t, selftest);

	bdp->selftest = (self_test_t *) (bdp->dma_able + off);
	bdp->selftest_phys = bdp->dma_able_phys + off;

	off = offsetof(bd_dma_able_t, stats_counters);

	bdp->stats_counters = (max_counters_t *) (bdp->dma_able + off);
	bdp->stat_cnt_phys = bdp->dma_able_phys + off;

	return 0;

err:
	printk(KERN_ERR
	       "e100: Failed to allocate memory\n");
	return -ENOMEM;
}

/**
 * e100_alloc_tcb_pool - allocate TCB circular list
 * @bdp: atapter's private data struct
 *
 * This routine allocates memory for the circular list of transmit descriptors.
 *
 * Returns:
 *       0: if allocation has failed.
 *       1: Otherwise. 
 */
int
e100_alloc_tcb_pool(struct e100_private *bdp)
{
	int stcb = sizeof (tcb_t) * bdp->params.TxDescriptors;

	/* allocate space for the TCBs */
	if (!(bdp->tcb_pool.data =
	      pci_alloc_consistent(bdp->pdev, stcb, &bdp->tcb_phys)))
		return 0;

	memset(bdp->tcb_pool.data, 0x00, stcb);

	return 1;
}

void
e100_free_tcb_pool(struct e100_private *bdp)
{
	tcb_t *tcb;
	int i;
	/* Return tx skbs */ 
	for (i = 0; i < bdp->params.TxDescriptors; i++) {
	  	tcb = bdp->tcb_pool.data;
		tcb += bdp->tcb_pool.head;
  		e100_tx_skb_free(bdp, tcb);
		if (NEXT_TCB_TOUSE(bdp->tcb_pool.head) == bdp->tcb_pool.tail)
		  	break;
		bdp->tcb_pool.head = NEXT_TCB_TOUSE(bdp->tcb_pool.head);
	}
	pci_free_consistent(bdp->pdev,
			    sizeof (tcb_t) * bdp->params.TxDescriptors,
			    bdp->tcb_pool.data, bdp->tcb_phys);
	bdp->tcb_pool.head = 0;
	bdp->tcb_pool.tail = 1;	
	bdp->tcb_phys = 0;
}

static void
e100_dealloc_space(struct e100_private *bdp)
{
	if (bdp->dma_able) {
		pci_free_consistent(bdp->pdev, sizeof (bd_dma_able_t),
				    bdp->dma_able, bdp->dma_able_phys);
	}

	bdp->selftest_phys = 0;
	bdp->stat_cnt_phys = 0;
	bdp->dma_able_phys = 0;
	bdp->dma_able = 0;
}

static void
e100_free_rfd_pool(struct e100_private *bdp)
{
	struct rx_list_elem *rx_struct;

	while (!list_empty(&(bdp->active_rx_list))) {

		rx_struct = list_entry(bdp->active_rx_list.next,
				       struct rx_list_elem, list_elem);
		list_del(&(rx_struct->list_elem));
		pci_unmap_single(bdp->pdev, rx_struct->dma_addr,
				 sizeof (rfd_t), PCI_DMA_TODEVICE);
		dev_kfree_skb(rx_struct->skb);
		kfree(rx_struct);
	}

	while (!list_empty(&(bdp->rx_struct_pool))) {
		rx_struct = list_entry(bdp->rx_struct_pool.next,
				       struct rx_list_elem, list_elem);
		list_del(&(rx_struct->list_elem));
		kfree(rx_struct);
	}
}

/**
 * e100_alloc_rfd_pool - allocate RFDs
 * @bdp: atapter's private data struct
 *
 * Allocates initial pool of skb which holds both rfd and data,
 * and return a pointer to the head of the list
 */
static int
e100_alloc_rfd_pool(struct e100_private *bdp)
{
	struct rx_list_elem *rx_struct;
	int i;

	INIT_LIST_HEAD(&(bdp->active_rx_list));
	INIT_LIST_HEAD(&(bdp->rx_struct_pool));
	bdp->skb_req = bdp->params.RxDescriptors;
	for (i = 0; i < bdp->skb_req; i++) {
		rx_struct = kmalloc(sizeof (struct rx_list_elem), GFP_ATOMIC);
		list_add(&(rx_struct->list_elem), &(bdp->rx_struct_pool));
	}
	e100_alloc_skbs(bdp);
	return !list_empty(&(bdp->active_rx_list));

}

void
e100_clear_pools(struct e100_private *bdp)
{
	bdp->last_tcb = NULL;
	e100_free_rfd_pool(bdp);
	e100_free_tcb_pool(bdp);
}

/*****************************************************************************/
/*****************************************************************************/
/*      Run Time Functions                                                   */
/*****************************************************************************/

/**
 * e100_watchdog
 * @dev: adapter's net_device struct
 *
 * This routine runs every 2 seconds and updates our statitics and link state,
 * and refreshs txthld value.
 */
void
e100_watchdog(struct net_device *dev)
{
	struct e100_private *bdp = dev->priv;

#ifdef E100_CU_DEBUG
	if (e100_cu_unknown_state(bdp)) {
		printk(KERN_ERR "e100: %s: CU unknown state in e100_watchdog\n",
			dev->name);
	}
#endif	
	if (!netif_running(dev)) {
		return;
	}

	/* check if link state has changed */
	if (e100_phy_check(bdp)) {
		if (netif_carrier_ok(dev)) {
			printk(KERN_ERR
			       "e100: %s NIC Link is Up %d Mbps %s duplex\n",
			       bdp->device->name, bdp->cur_line_speed,
			       (bdp->cur_dplx_mode == HALF_DUPLEX) ?
			       "Half" : "Full");

			e100_config_fc(bdp);
			e100_config(bdp);  

		} else {
			printk(KERN_ERR "e100: %s NIC Link is Down\n",
			       bdp->device->name);
		}
	}

	// toggle the tx queue according to link status
	// this also resolves a race condition between tx & non-cu cmd flows
	if (netif_carrier_ok(dev)) {
		if (netif_running(dev))
			netif_wake_queue(dev);
	} else {
		if (netif_running(dev))
			netif_stop_queue(dev);
		/* When changing to non-autoneg, device may lose  */
		/* link with some switches. e100 will try to      */
		/* revover link by sending command to PHY layer   */
		if (bdp->params.e100_speed_duplex != E100_AUTONEG)
			e100_force_speed_duplex_to_phy(bdp);
	}

	rmb();

	if (e100_update_stats(bdp)) {

		/* Check if a change in the IFS parameter is needed,
		   and configure the device accordingly */
		if (bdp->params.b_params & PRM_IFS)
			e100_manage_adaptive_ifs(bdp);

		/* Now adjust our dynamic tx threshold value */
		e100_refresh_txthld(bdp);

		/* Now if we are on a 557 and we havn't received any frames then we
		 * should issue a multicast command to reset the RU */
		if (bdp->rev_id < D101A4_REV_ID) {
			if (!(bdp->stats_counters->basic_stats.rcv_gd_frames)) {
				e100_set_multi(dev);
			}
		}
	}
	/* Issue command to dump statistics from device.        */
	/* Check for command completion on next watchdog timer. */
	e100_dump_stats_cntrs(bdp);

	wmb();

	/* relaunch watchdog timer in 2 sec */
	mod_timer(&(bdp->watchdog_timer), jiffies + (2 * HZ));

	if (list_empty(&bdp->active_rx_list))
		e100_trigger_SWI(bdp);
}

/**
 * e100_manage_adaptive_ifs
 * @bdp: atapter's private data struct
 *
 * This routine manages the adaptive Inter-Frame Spacing algorithm
 * using a state machine.
 */
void
e100_manage_adaptive_ifs(struct e100_private *bdp)
{
	static u16 state_table[9][4] = {	// rows are states
		{2, 0, 0, 0},	// state0   // column0: next state if increasing
		{2, 0, 5, 30},	// state1   // column1: next state if decreasing
		{5, 1, 5, 30},	// state2   // column2: IFS value for 100 mbit
		{5, 3, 0, 0},	// state3   // column3: IFS value for 10 mbit
		{5, 3, 10, 60},	// state4
		{8, 4, 10, 60},	// state5
		{8, 6, 0, 0},	// state6
		{8, 6, 20, 60},	// state7
		{8, 7, 20, 60}	// state8
	};

	u32 transmits =
		le32_to_cpu(bdp->stats_counters->basic_stats.xmt_gd_frames);
	u32 collisions =
		le32_to_cpu(bdp->stats_counters->basic_stats.xmt_ttl_coll);
	u32 state = bdp->ifs_state;
	u32 old_value = bdp->ifs_value;
	int next_col;
	u32 min_transmits;

	if (bdp->cur_dplx_mode == FULL_DUPLEX) {
		bdp->ifs_state = 0;
		bdp->ifs_value = 0;

	} else {		/* Half Duplex */
		/* Set speed specific parameters */
		if (bdp->cur_line_speed == 100) {
			next_col = 2;
			min_transmits = MIN_NUMBER_OF_TRANSMITS_100;

		} else {	/* 10 Mbps */
			next_col = 3;
			min_transmits = MIN_NUMBER_OF_TRANSMITS_10;
		}

		if ((transmits / 32 < collisions)
		    && (transmits > min_transmits)) {
			state = state_table[state][0];	/* increment */

		} else if (transmits < min_transmits) {
			state = state_table[state][1];	/* decrement */
		}

		bdp->ifs_value = state_table[state][next_col];
		bdp->ifs_state = state;
	}

	/* If the IFS value has changed, configure the device */
	if (bdp->ifs_value != old_value) {
		e100_config_ifs(bdp);
		e100_config(bdp);
	}
}

/**
 * e100intr - interrupt handler
 * @irq: the IRQ number
 * @dev_inst: the net_device struct
 * @regs: registers (unused)
 *
 * This routine is the ISR for the e100 board. It services
 * the RX & TX queues & starts the RU if it has stopped due
 * to no resources.
 */
irqreturn_t
e100intr(int irq, void *dev_inst, struct pt_regs *regs)
{
	struct net_device *dev;
	struct e100_private *bdp;
	u16 intr_status;

	dev = dev_inst;
	bdp = dev->priv;

	intr_status = readw(&bdp->scb->scb_status);
	/* If not my interrupt, just return */
	if (!(intr_status & SCB_STATUS_ACK_MASK) || (intr_status == 0xffff)) {
		return IRQ_NONE;
	}

	/* disable and ack intr */
	e100_disable_clear_intr(bdp);

	/* the device is closed, don't continue or else bad things may happen. */
	if (!netif_running(dev)) {
		e100_set_intr_mask(bdp);
		return IRQ_NONE;
	}

	/* SWI intr (triggered by watchdog) is signal to allocate new skb buffers */
	if (intr_status & SCB_STATUS_ACK_SWI) {
		e100_alloc_skbs(bdp);
	}

	/* do recv work if any */
	if (intr_status &
	    (SCB_STATUS_ACK_FR | SCB_STATUS_ACK_RNR | SCB_STATUS_ACK_SWI)) 
		bdp->drv_stats.rx_intr_pkts += e100_rx_srv(bdp);

	/* clean up after tx'ed packets */
	if (intr_status & (SCB_STATUS_ACK_CNA | SCB_STATUS_ACK_CX))
		e100_tx_srv(bdp);

	e100_set_intr_mask(bdp);
	return IRQ_HANDLED;
}

/**
 * e100_tx_skb_free - free TX skbs resources
 * @bdp: atapter's private data struct
 * @tcb: associated tcb of the freed skb
 *
 * This routine frees resources of TX skbs.
 */
static inline void
e100_tx_skb_free(struct e100_private *bdp, tcb_t *tcb)
{
	if (tcb->tcb_skb) {
		int i;
		tbd_t *tbd_arr = tcb->tbd_ptr;
		int frags = skb_shinfo(tcb->tcb_skb)->nr_frags;

		for (i = 0; i <= frags; i++, tbd_arr++) {
			pci_unmap_single(bdp->pdev,
					 le32_to_cpu(tbd_arr->tbd_buf_addr),
					 le16_to_cpu(tbd_arr->tbd_buf_cnt),
					 PCI_DMA_TODEVICE);
		}
		dev_kfree_skb_irq(tcb->tcb_skb);
		tcb->tcb_skb = NULL;
	}
}

/**
 * e100_tx_srv - service TX queues
 * @bdp: atapter's private data struct
 *
 * This routine services the TX queues. It reclaims the TCB's & TBD's & other
 * resources used during the transmit of this buffer. It is called from the ISR.
 * We don't need a tx_lock since we always access buffers which were already
 * prepared.
 */
void
e100_tx_srv(struct e100_private *bdp)
{
	tcb_t *tcb;
	int i;

	/* go over at most TxDescriptors buffers */
	for (i = 0; i < bdp->params.TxDescriptors; i++) {
		tcb = bdp->tcb_pool.data;
		tcb += bdp->tcb_pool.head;

		rmb();

		/* if the buffer at 'head' is not complete, break */
		if (!(tcb->tcb_hdr.cb_status &
		      __constant_cpu_to_le16(CB_STATUS_COMPLETE)))
			break;

		/* service next buffer, clear the out of resource condition */
		e100_tx_skb_free(bdp, tcb);

		if (netif_running(bdp->device))
			netif_wake_queue(bdp->device);

		/* if we've caught up with 'tail', break */
		if (NEXT_TCB_TOUSE(bdp->tcb_pool.head) == bdp->tcb_pool.tail) {
			break;
		}

		bdp->tcb_pool.head = NEXT_TCB_TOUSE(bdp->tcb_pool.head);
	}
}

/**
 * e100_rx_srv - service RX queue
 * @bdp: atapter's private data struct
 * @max_number_of_rfds: max number of RFDs to process
 * @rx_congestion: flag pointer, to inform the calling function of congestion.
 *
 * This routine processes the RX interrupt & services the RX queues.
 * For each successful RFD, it allocates a new msg block, links that
 * into the RFD list, and sends the old msg upstream.
 * The new RFD is then put at the end of the free list of RFD's.
 * It returns the number of serviced RFDs.
 */
u32
e100_rx_srv(struct e100_private *bdp)
{
	rfd_t *rfd;		/* new rfd, received rfd */
	int i;
	u16 rfd_status;
	struct sk_buff *skb;
	struct net_device *dev;
	unsigned int data_sz;
	struct rx_list_elem *rx_struct;
	u32 rfd_cnt = 0;

	dev = bdp->device;

	/* current design of rx is as following:
	 * 1. socket buffer (skb) used to pass network packet to upper layer
	 * 2. all HW host memory structures (like RFDs, RBDs and data buffers)
	 *    are placed in a skb's data room
	 * 3. when rx process is complete, we change skb internal pointers to exclude
	 *    from data area all unrelated things (RFD, RDB) and to leave
	 *    just rx'ed packet netto
	 * 4. for each skb passed to upper layer, new one is allocated instead.
	 * 5. if no skb left, in 2 sec another atempt to allocate skbs will be made
	 *    (watchdog trigger SWI intr and isr should allocate new skbs)
	 */
	for (i = 0; i < bdp->params.RxDescriptors; i++) {
		if (list_empty(&(bdp->active_rx_list))) {
			break;
		}

		rx_struct = list_entry(bdp->active_rx_list.next,
				       struct rx_list_elem, list_elem);
		skb = rx_struct->skb;

		rfd = RFD_POINTER(skb, bdp);	/* locate RFD within skb */

		// sync only the RFD header
		pci_dma_sync_single(bdp->pdev, rx_struct->dma_addr,
				    bdp->rfd_size, PCI_DMA_FROMDEVICE);
		rfd_status = le16_to_cpu(rfd->rfd_header.cb_status);	/* get RFD's status */
		if (!(rfd_status & RFD_STATUS_COMPLETE))	/* does not contains data yet - exit */
			break;

		/* to allow manipulation with current skb we need to unlink it */
		list_del(&(rx_struct->list_elem));

		/* do not free & unmap badly received packet.
		 * move it to the end of skb list for reuse */
		if (!(rfd_status & RFD_STATUS_OK)) {
			e100_add_skb_to_end(bdp, rx_struct);
			continue;
		}

		data_sz = min_t(u16, (le16_to_cpu(rfd->rfd_act_cnt) & 0x3fff),
				(sizeof (rfd_t) - bdp->rfd_size));

		/* now sync all the data */
		pci_dma_sync_single(bdp->pdev, rx_struct->dma_addr,
				    (data_sz + bdp->rfd_size),
				    PCI_DMA_FROMDEVICE);

		pci_unmap_single(bdp->pdev, rx_struct->dma_addr,
				 sizeof (rfd_t), PCI_DMA_FROMDEVICE);

		list_add(&(rx_struct->list_elem), &(bdp->rx_struct_pool));

		/* end of dma access to rfd */
		bdp->skb_req++;	/* incr number of requested skbs */
		e100_alloc_skbs(bdp);	/* and get them */

		/* set packet size, excluding checksum (2 last bytes) if it is present */
		if ((bdp->flags & DF_CSUM_OFFLOAD)
		    && (bdp->rev_id < D102_REV_ID))
			skb_put(skb, (int) data_sz - 2);
		else
			skb_put(skb, (int) data_sz);

		/* set the protocol */
		skb->protocol = eth_type_trans(skb, dev);

		/* set the checksum info */
		if (bdp->flags & DF_CSUM_OFFLOAD) {
			if (bdp->rev_id >= D102_REV_ID) {
				skb->ip_summed = e100_D102_check_checksum(rfd);
			} else {
				skb->ip_summed = e100_D101M_checksum(bdp, skb);
			}
		} else {
			skb->ip_summed = CHECKSUM_NONE;
		}

		bdp->drv_stats.net_stats.rx_bytes += skb->len;

		if(bdp->vlgrp && (rfd_status & CB_STATUS_VLAN)) {
			vlan_hwaccel_rx(skb, bdp->vlgrp, be16_to_cpu(rfd->vlanid));
		} else {
			netif_rx(skb);
		}
		dev->last_rx = jiffies;
		
		rfd_cnt++;
	}			/* end of rfd loop */

	/* restart the RU if it has stopped */
	if ((readw(&bdp->scb->scb_status) & SCB_RUS_MASK) != SCB_RUS_READY) {
		e100_start_ru(bdp);
	}

	return rfd_cnt;
}

void
e100_refresh_txthld(struct e100_private *bdp)
{
	basic_cntr_t *pstat = &(bdp->stats_counters->basic_stats);

	/* as long as tx_per_underrun is not 0, we can go about dynamically *
	 * adjusting the xmit threshold. we stop doing that & resort to defaults
	 * * once the adjustments become meaningless. the value is adjusted by *
	 * dumping the error counters & checking the # of xmit underrun errors *
	 * we've had. */
	if (bdp->tx_per_underrun) {
		/* We are going to last values dumped from the dump statistics
		 * command */
		if (le32_to_cpu(pstat->xmt_gd_frames)) {
			if (le32_to_cpu(pstat->xmt_uruns)) {
				/* 
				 * if we have had more than one underrun per "DEFAULT #
				 * OF XMITS ALLOWED PER UNDERRUN" good xmits, raise the
				 * THRESHOLD.
				 */
				if ((le32_to_cpu(pstat->xmt_gd_frames) /
				     le32_to_cpu(pstat->xmt_uruns)) <
				    bdp->tx_per_underrun) {
					bdp->tx_thld += 3;
				}
			}

			/* 
			 * if we've had less than one underrun per the DEFAULT number of
			 * of good xmits allowed, lower the THOLD but not less than 0 
			 */
			if (le32_to_cpu(pstat->xmt_gd_frames) >
			    bdp->tx_per_underrun) {
				bdp->tx_thld--;

				if (bdp->tx_thld < 6)
					bdp->tx_thld = 6;

			}
		}

		/* end good xmits */
		/* 
		 * * if our adjustments are becoming unresonable, stop adjusting &
		 * resort * to defaults & pray. A THOLD value > 190 means that the
		 * adapter will * wait for 190*8=1520 bytes in TX FIFO before it
		 * starts xmit. Since * MTU is 1514, it doesn't make any sense for
		 * further increase. */
		if (bdp->tx_thld >= 190) {
			bdp->tx_per_underrun = 0;
			bdp->tx_thld = 189;
		}
	}			/* end underrun check */
}

/**
 * e100_prepare_xmit_buff - prepare a buffer for transmission
 * @bdp: atapter's private data struct
 * @skb: skb to send
 *
 * This routine prepare a buffer for transmission. It checks
 * the message length for the appropiate size. It picks up a
 * free tcb from the TCB pool and sets up the corresponding
 * TBD's. If the number of fragments are more than the number
 * of TBD/TCB it copies all the fragments in a coalesce buffer.
 * It returns a pointer to the prepared TCB.
 */
static inline tcb_t *
e100_prepare_xmit_buff(struct e100_private *bdp, struct sk_buff *skb)
{
	tcb_t *tcb, *prev_tcb;

	tcb = bdp->tcb_pool.data;
	tcb += TCB_TO_USE(bdp->tcb_pool);

	if (bdp->flags & USE_IPCB) {
		tcb->tcbu.ipcb.ip_activation_high = IPCB_IP_ACTIVATION_DEFAULT;
		tcb->tcbu.ipcb.ip_schedule &= ~IPCB_TCP_PACKET;
		tcb->tcbu.ipcb.ip_schedule &= ~IPCB_TCPUDP_CHECKSUM_ENABLE;
	}

	if(bdp->vlgrp && vlan_tx_tag_present(skb)) {
		(tcb->tcbu).ipcb.ip_activation_high |= IPCB_INSERTVLAN_ENABLE;
		(tcb->tcbu).ipcb.vlan = cpu_to_be16(vlan_tx_tag_get(skb));
	}
	
	tcb->tcb_hdr.cb_status = 0;
	tcb->tcb_thrshld = bdp->tx_thld;
	tcb->tcb_hdr.cb_cmd |= __constant_cpu_to_le16(CB_S_BIT);

	/* Set I (Interrupt) bit on every (TX_FRAME_CNT)th packet */
	if (!(++bdp->tx_count % TX_FRAME_CNT))
		tcb->tcb_hdr.cb_cmd |= __constant_cpu_to_le16(CB_I_BIT);
	else
		/* Clear I bit on other packets */
		tcb->tcb_hdr.cb_cmd &= ~__constant_cpu_to_le16(CB_I_BIT);

	tcb->tcb_skb = skb;

	if (skb->ip_summed == CHECKSUM_HW) {
		const struct iphdr *ip = skb->nh.iph;

		if ((ip->protocol == IPPROTO_TCP) ||
		    (ip->protocol == IPPROTO_UDP)) {

			tcb->tcbu.ipcb.ip_activation_high |=
				IPCB_HARDWAREPARSING_ENABLE;
			tcb->tcbu.ipcb.ip_schedule |=
				IPCB_TCPUDP_CHECKSUM_ENABLE;

			if (ip->protocol == IPPROTO_TCP)
				tcb->tcbu.ipcb.ip_schedule |= IPCB_TCP_PACKET;
		}
	}

	if (!skb_shinfo(skb)->nr_frags) {
		(tcb->tbd_ptr)->tbd_buf_addr =
			cpu_to_le32(pci_map_single(bdp->pdev, skb->data,
						   skb->len, PCI_DMA_TODEVICE));
		(tcb->tbd_ptr)->tbd_buf_cnt = cpu_to_le16(skb->len);
		tcb->tcb_tbd_num = 1;
		tcb->tcb_tbd_ptr = tcb->tcb_tbd_dflt_ptr;
	} else {
		int i;
		void *addr;
		tbd_t *tbd_arr_ptr = &(tcb->tbd_ptr[1]);
		skb_frag_t *frag = &skb_shinfo(skb)->frags[0];

		(tcb->tbd_ptr)->tbd_buf_addr =
			cpu_to_le32(pci_map_single(bdp->pdev, skb->data,
						   skb_headlen(skb),
						   PCI_DMA_TODEVICE));
		(tcb->tbd_ptr)->tbd_buf_cnt =
			cpu_to_le16(skb_headlen(skb));

		for (i = 0; i < skb_shinfo(skb)->nr_frags;
		     i++, tbd_arr_ptr++, frag++) {

			addr = ((void *) page_address(frag->page) +
				frag->page_offset);

			tbd_arr_ptr->tbd_buf_addr =
				cpu_to_le32(pci_map_single(bdp->pdev,
							   addr, frag->size,
							   PCI_DMA_TODEVICE));
			tbd_arr_ptr->tbd_buf_cnt = cpu_to_le16(frag->size);
		}
		tcb->tcb_tbd_num = skb_shinfo(skb)->nr_frags + 1;
		tcb->tcb_tbd_ptr = tcb->tcb_tbd_expand_ptr;
	}

	/* clear the S-BIT on the previous tcb */
	prev_tcb = bdp->tcb_pool.data;
	prev_tcb += PREV_TCB_USED(bdp->tcb_pool);
	prev_tcb->tcb_hdr.cb_cmd &= __constant_cpu_to_le16((u16) ~CB_S_BIT);

	bdp->tcb_pool.tail = NEXT_TCB_TOUSE(bdp->tcb_pool.tail);

	wmb();

	e100_start_cu(bdp, tcb);

	return tcb;
}

/* Changed for 82558 enhancement */
/**
 * e100_start_cu - start the adapter's CU
 * @bdp: atapter's private data struct
 * @tcb: TCB to be transmitted
 *
 * This routine issues a CU Start or CU Resume command to the 82558/9.
 * This routine was added because the prepare_ext_xmit_buff takes advantage
 * of the 82558/9's Dynamic TBD chaining feature and has to start the CU as
 * soon as the first TBD is ready. 
 *
 * e100_start_cu must be called while holding the tx_lock ! 
 */
u8
e100_start_cu(struct e100_private *bdp, tcb_t *tcb)
{
	unsigned long lock_flag;
	u8 ret = true;

	spin_lock_irqsave(&(bdp->bd_lock), lock_flag);
	switch (bdp->next_cu_cmd) {
	case RESUME_NO_WAIT:
		/*last cu command was a CU_RESMUE if this is a 558 or newer we don't need to
		 * wait for command word to clear, we reach here only if we are bachlor
		 */
		e100_exec_cmd(bdp, SCB_CUC_RESUME);
		break;

	case RESUME_WAIT:
		if ((bdp->flags & IS_ICH) &&
		    (bdp->cur_line_speed == 10) &&
		    (bdp->cur_dplx_mode == HALF_DUPLEX)) {
			e100_wait_exec_simple(bdp, SCB_CUC_NOOP);
			udelay(1);
		}
		if ((e100_wait_exec_simple(bdp, SCB_CUC_RESUME)) &&
		    (bdp->flags & IS_BACHELOR) && (!(bdp->flags & IS_ICH))) {
			bdp->next_cu_cmd = RESUME_NO_WAIT;
		}
		break;

	case START_WAIT:
		// The last command was a non_tx CU command
		if (!e100_wait_cus_idle(bdp))
			printk(KERN_DEBUG
			       "e100: %s: cu_start: timeout waiting for cu\n",
			       bdp->device->name);
		if (!e100_wait_exec_cmplx(bdp, (u32) (tcb->tcb_phys),
					  SCB_CUC_START, CB_TRANSMIT)) {
			printk(KERN_DEBUG
			       "e100: %s: cu_start: timeout waiting for scb\n",
			       bdp->device->name);
			e100_exec_cmplx(bdp, (u32) (tcb->tcb_phys),
					SCB_CUC_START);
			ret = false;
		}

		bdp->next_cu_cmd = RESUME_WAIT;

		break;
	}

	/* save the last tcb */
	bdp->last_tcb = tcb;

	spin_unlock_irqrestore(&(bdp->bd_lock), lock_flag);
	return ret;
}

/* ====================================================================== */
/* hw                                                                     */
/* ====================================================================== */

/**
 * e100_selftest - perform H/W self test
 * @bdp: atapter's private data struct
 * @st_timeout: address to return timeout value, if fails
 * @st_result: address to return selftest result, if fails
 *
 * This routine will issue PORT Self-test command to test the e100.
 * The self-test will fail if the adapter's master-enable bit is not
 * set in the PCI Command Register, or if the adapter is not seated
 * in a PCI master-enabled slot. we also disable interrupts when the
 * command is completed.
 *
 * Returns:
 *      true: if adapter passes self_test
 *      false: otherwise
 */
unsigned char
e100_selftest(struct e100_private *bdp, u32 *st_timeout, u32 *st_result)
{
	u32 selftest_cmd;

	/* initialize the nic state before running test */
	e100_sw_reset(bdp, PORT_SOFTWARE_RESET);
	/* Setup the address of the self_test area */
	selftest_cmd = bdp->selftest_phys;

	/* Setup SELF TEST Command Code in D3 - D0 */
	selftest_cmd |= PORT_SELFTEST;

	/* Initialize the self-test signature and results DWORDS */
	bdp->selftest->st_sign = 0;
	bdp->selftest->st_result = 0xffffffff;

	/* Do the port command */
	writel(selftest_cmd, &bdp->scb->scb_port);
	readw(&(bdp->scb->scb_status));	/* flushes last write, read-safe */

	/* Wait at least 10 milliseconds for the self-test to complete */
	set_current_state(TASK_UNINTERRUPTIBLE);
	schedule_timeout(HZ / 100 + 1);

	/* disable interrupts since they are enabled */
	/* after device reset during selftest        */
	e100_disable_clear_intr(bdp);

	/* if The First Self Test DWORD Still Zero, We've timed out. If the
	 * second DWORD is not zero then we have an error. */
	if ((bdp->selftest->st_sign == 0) || (bdp->selftest->st_result != 0)) {

		if (st_timeout)
			*st_timeout = !(le32_to_cpu(bdp->selftest->st_sign));

		if (st_result)
			*st_result = le32_to_cpu(bdp->selftest->st_result);

		return false;
	}

	return true;
}

/**
 * e100_setup_iaaddr - issue IA setup sommand
 * @bdp: atapter's private data struct
 * @eaddr: new ethernet address
 *
 * This routine will issue the IA setup command. This command
 * will notify the 82557 (e100) of what its individual (node)
 * address is. This command will be executed in polled mode.
 *
 * Returns:
 *      true: if the IA setup command was successfully issued and completed
 *      false: otherwise
 */
unsigned char
e100_setup_iaaddr(struct e100_private *bdp, u8 *eaddr)
{
	unsigned int i;
	cb_header_t *ntcb_hdr;
	unsigned char res;
	nxmit_cb_entry_t *cmd;

	if ((cmd = e100_alloc_non_tx_cmd(bdp)) == NULL) {
		res = false;
		goto exit;
	}

	ntcb_hdr = (cb_header_t *) cmd->non_tx_cmd;
	ntcb_hdr->cb_cmd = __constant_cpu_to_le16(CB_IA_ADDRESS);

	for (i = 0; i < ETH_ALEN; i++) {
		(cmd->non_tx_cmd)->ntcb.setup.ia_addr[i] = eaddr[i];
	}

	res = e100_exec_non_cu_cmd(bdp, cmd);
	if (!res)
		printk(KERN_WARNING "e100: %s: IA setup failed\n", 
		       bdp->device->name);

exit:
	return res;
}

/**
 * e100_start_ru - start the RU if needed
 * @bdp: atapter's private data struct
 *
 * This routine checks the status of the 82557's receive unit(RU),
 * and starts the RU if it was not already active.  However,
 * before restarting the RU, the driver gives the RU the buffers
 * it freed up during the servicing of the ISR. If there are
 * no free buffers to give to the RU, (i.e. we have reached a
 * no resource condition) the RU will not be started till the
 * next ISR.
 */
void
e100_start_ru(struct e100_private *bdp)
{
	struct rx_list_elem *rx_struct = NULL;
	int buffer_found = 0;
	struct list_head *entry_ptr;

	list_for_each(entry_ptr, &(bdp->active_rx_list)) {
		rx_struct =
			list_entry(entry_ptr, struct rx_list_elem, list_elem);
		pci_dma_sync_single(bdp->pdev, rx_struct->dma_addr,
				    bdp->rfd_size, PCI_DMA_FROMDEVICE);
		if (!((SKB_RFD_STATUS(rx_struct->skb, bdp) &
		       __constant_cpu_to_le16(RFD_STATUS_COMPLETE)))) {
			buffer_found = 1;
			break;
		}
	}

	/* No available buffers */
	if (!buffer_found) {
		return;
	}

	spin_lock(&bdp->bd_lock);

	if (!e100_wait_exec_cmplx(bdp, rx_struct->dma_addr, SCB_RUC_START, 0)) {
		printk(KERN_DEBUG
		       "e100: %s: start_ru: wait_scb failed\n", 
		       bdp->device->name);
		e100_exec_cmplx(bdp, rx_struct->dma_addr, SCB_RUC_START);
	}
	if (bdp->next_cu_cmd == RESUME_NO_WAIT) {
		bdp->next_cu_cmd = RESUME_WAIT;
	}
	spin_unlock(&bdp->bd_lock);
}

/**
 * e100_cmd_complete_location
 * @bdp: atapter's private data struct
 *
 * This routine returns a pointer to the location of the command-complete
 * DWord in the dump statistical counters area, according to the statistical
 * counters mode (557 - basic, 558 - extended, or 559 - TCO mode).
 * See e100_config_init() for the setting of the statistical counters mode.
 */
static u32 *
e100_cmd_complete_location(struct e100_private *bdp)
{
	u32 *cmd_complete;
	max_counters_t *stats = bdp->stats_counters;

	switch (bdp->stat_mode) {
	case E100_EXTENDED_STATS:
		cmd_complete =
			(u32 *) &(((err_cntr_558_t *) (stats))->cmd_complete);
		break;

	case E100_TCO_STATS:
		cmd_complete =
			(u32 *) &(((err_cntr_559_t *) (stats))->cmd_complete);
		break;

	case E100_BASIC_STATS:
	default:		
		cmd_complete =
			(u32 *) &(((err_cntr_557_t *) (stats))->cmd_complete);
		break;
	}

	return cmd_complete;
}

/**
 * e100_clr_cntrs - clear statistics counters
 * @bdp: atapter's private data struct
 *
 * This routine will clear the adapter error statistic counters.
 *
 * Returns:
 *      true: if successfully cleared stat counters
 *      false: otherwise
 */
static unsigned char
e100_clr_cntrs(struct e100_private *bdp)
{
	volatile u32 *pcmd_complete;

	/* clear the dump counter complete word */
	pcmd_complete = e100_cmd_complete_location(bdp);
	*pcmd_complete = 0;
	wmb();

	if (!e100_wait_exec_cmplx(bdp, bdp->stat_cnt_phys, SCB_CUC_DUMP_ADDR, 0))
		return false;

	/* wait 10 microseconds for the command to complete */
	udelay(10);

	if (!e100_wait_exec_simple(bdp, SCB_CUC_DUMP_RST_STAT))
		return false;

	if (bdp->next_cu_cmd == RESUME_NO_WAIT) {
		bdp->next_cu_cmd = RESUME_WAIT;
	}

	return true;
}

static unsigned char
e100_update_stats(struct e100_private *bdp)
{
	u32 *pcmd_complete;
	basic_cntr_t *pstat = &(bdp->stats_counters->basic_stats);

	// check if last dump command completed
	pcmd_complete = e100_cmd_complete_location(bdp);
	if (*pcmd_complete != le32_to_cpu(DUMP_RST_STAT_COMPLETED) &&
	    *pcmd_complete != le32_to_cpu(DUMP_STAT_COMPLETED)) {
		*pcmd_complete = 0;
		return false;
	}

	/* increment the statistics */
	bdp->drv_stats.net_stats.rx_packets +=
		le32_to_cpu(pstat->rcv_gd_frames);
	bdp->drv_stats.net_stats.tx_packets +=
		le32_to_cpu(pstat->xmt_gd_frames);
	bdp->drv_stats.net_stats.rx_dropped += le32_to_cpu(pstat->rcv_rsrc_err);
	bdp->drv_stats.net_stats.collisions += le32_to_cpu(pstat->xmt_ttl_coll);
	bdp->drv_stats.net_stats.rx_length_errors +=
		le32_to_cpu(pstat->rcv_shrt_frames);
	bdp->drv_stats.net_stats.rx_over_errors +=
		le32_to_cpu(pstat->rcv_rsrc_err);
	bdp->drv_stats.net_stats.rx_crc_errors +=
		le32_to_cpu(pstat->rcv_crc_errs);
	bdp->drv_stats.net_stats.rx_frame_errors +=
		le32_to_cpu(pstat->rcv_algn_errs);
	bdp->drv_stats.net_stats.rx_fifo_errors +=
		le32_to_cpu(pstat->rcv_oruns);
	bdp->drv_stats.net_stats.tx_aborted_errors +=
		le32_to_cpu(pstat->xmt_max_coll);
	bdp->drv_stats.net_stats.tx_carrier_errors +=
		le32_to_cpu(pstat->xmt_lost_crs);
	bdp->drv_stats.net_stats.tx_fifo_errors +=
		le32_to_cpu(pstat->xmt_uruns);

	bdp->drv_stats.tx_late_col += le32_to_cpu(pstat->xmt_late_coll);
	bdp->drv_stats.tx_ok_defrd += le32_to_cpu(pstat->xmt_deferred);
	bdp->drv_stats.tx_one_retry += le32_to_cpu(pstat->xmt_sngl_coll);
	bdp->drv_stats.tx_mt_one_retry += le32_to_cpu(pstat->xmt_mlt_coll);
	bdp->drv_stats.rcv_cdt_frames += le32_to_cpu(pstat->rcv_err_coll);

	if (bdp->stat_mode != E100_BASIC_STATS) {
		ext_cntr_t *pex_stat = &bdp->stats_counters->extended_stats;

		bdp->drv_stats.xmt_fc_pkts +=
			le32_to_cpu(pex_stat->xmt_fc_frames);
		bdp->drv_stats.rcv_fc_pkts +=
			le32_to_cpu(pex_stat->rcv_fc_frames);
		bdp->drv_stats.rcv_fc_unsupported +=
			le32_to_cpu(pex_stat->rcv_fc_unsupported);
	}

	if (bdp->stat_mode == E100_TCO_STATS) {
		tco_cntr_t *ptco_stat = &bdp->stats_counters->tco_stats;

		bdp->drv_stats.xmt_tco_pkts +=
			le16_to_cpu(ptco_stat->xmt_tco_frames);
		bdp->drv_stats.rcv_tco_pkts +=
			le16_to_cpu(ptco_stat->rcv_tco_frames);
	}

	*pcmd_complete = 0;
	return true;
}

/**
 * e100_dump_stat_cntrs
 * @bdp: atapter's private data struct
 *
 * This routine will dump the board statistical counters without waiting
 * for stat_dump to complete. Any access to this stats should verify the completion
 * of the command
 */
void
e100_dump_stats_cntrs(struct e100_private *bdp)
{
	unsigned long lock_flag_bd;

	spin_lock_irqsave(&(bdp->bd_lock), lock_flag_bd);

	/* dump h/w stats counters */
	if (e100_wait_exec_simple(bdp, SCB_CUC_DUMP_RST_STAT)) {
		if (bdp->next_cu_cmd == RESUME_NO_WAIT) {
			bdp->next_cu_cmd = RESUME_WAIT;
		}
	}

	spin_unlock_irqrestore(&(bdp->bd_lock), lock_flag_bd);
}

/**
 * e100_exec_non_cu_cmd
 * @bdp: atapter's private data struct
 * @command: the non-cu command to execute
 *
 * This routine will submit a command block to be executed,
 */
unsigned char
e100_exec_non_cu_cmd(struct e100_private *bdp, nxmit_cb_entry_t *command)
{
	cb_header_t *ntcb_hdr;
	unsigned long lock_flag;
	unsigned long expiration_time;
	unsigned char rc = true;
	u8 sub_cmd;

	ntcb_hdr = (cb_header_t *) command->non_tx_cmd;	/* get hdr of non tcb cmd */
	sub_cmd = cpu_to_le16(ntcb_hdr->cb_cmd);

	/* Set the Command Block to be the last command block */
	ntcb_hdr->cb_cmd |= __constant_cpu_to_le16(CB_EL_BIT);
	ntcb_hdr->cb_status = 0;
	ntcb_hdr->cb_lnk_ptr = 0;

	wmb();
	if (in_interrupt())
		return e100_delayed_exec_non_cu_cmd(bdp, command);

	if (netif_running(bdp->device) && netif_carrier_ok(bdp->device))
		return e100_delayed_exec_non_cu_cmd(bdp, command);

	spin_lock_bh(&(bdp->bd_non_tx_lock));

	if (bdp->non_tx_command_state != E100_NON_TX_IDLE) {
		goto delayed_exec;
	}

	if (bdp->last_tcb) {
		rmb();
		if ((bdp->last_tcb->tcb_hdr.cb_status &
		     __constant_cpu_to_le16(CB_STATUS_COMPLETE)) == 0)
			goto delayed_exec;
	}

	if ((readw(&bdp->scb->scb_status) & SCB_CUS_MASK) == SCB_CUS_ACTIVE) {
		goto delayed_exec;
	}

	spin_lock_irqsave(&bdp->bd_lock, lock_flag);

	if (!e100_wait_exec_cmplx(bdp, command->dma_addr, SCB_CUC_START, sub_cmd)) {
		spin_unlock_irqrestore(&(bdp->bd_lock), lock_flag);
		rc = false;
		goto exit;
	}

	bdp->next_cu_cmd = START_WAIT;
	spin_unlock_irqrestore(&(bdp->bd_lock), lock_flag);

	/* now wait for completion of non-cu CB up to 20 msec */
	expiration_time = jiffies + HZ / 50 + 1;
	rmb();
	while (!(ntcb_hdr->cb_status &
		     __constant_cpu_to_le16(CB_STATUS_COMPLETE))) {

		if (time_before(jiffies, expiration_time)) {
			spin_unlock_bh(&(bdp->bd_non_tx_lock));
			yield();
			spin_lock_bh(&(bdp->bd_non_tx_lock));
		} else {
#ifdef E100_CU_DEBUG			
			printk(KERN_ERR "e100: %s: non-TX command (%x) "
				"timeout\n", bdp->device->name, sub_cmd);
#endif			
			rc = false;
			goto exit;
		}
		rmb();
	}

exit:
	e100_free_non_tx_cmd(bdp, command);

	if (netif_running(bdp->device))
		netif_wake_queue(bdp->device);

	spin_unlock_bh(&(bdp->bd_non_tx_lock));
	return rc;

delayed_exec:
	spin_unlock_bh(&(bdp->bd_non_tx_lock));
	return e100_delayed_exec_non_cu_cmd(bdp, command);
}

/**
 * e100_sw_reset
 * @bdp: atapter's private data struct
 * @reset_cmd: s/w reset or selective reset
 *
 * This routine will issue a software reset to the adapter. It 
 * will also disable interrupts, as the are enabled after reset.
 */
void
e100_sw_reset(struct e100_private *bdp, u32 reset_cmd)
{
	/* Do  a selective reset first to avoid a potential PCI hang */
	writel(PORT_SELECTIVE_RESET, &bdp->scb->scb_port);
	readw(&(bdp->scb->scb_status));	/* flushes last write, read-safe */

	/* wait for the reset to take effect */
	udelay(20);
	if (reset_cmd == PORT_SOFTWARE_RESET) {
		writel(PORT_SOFTWARE_RESET, &bdp->scb->scb_port);

		/* wait 20 micro seconds for the reset to take effect */
		udelay(20);
	}

	/* Mask off our interrupt line -- it is unmasked after reset */
	e100_disable_clear_intr(bdp);
#ifdef E100_CU_DEBUG	
	bdp->last_cmd = 0;
	bdp->last_sub_cmd = 0;
#endif	
}

/**
 * e100_load_microcode - Download microsocde to controller.
 * @bdp: atapter's private data struct
 *
 * This routine downloads microcode on to the controller. This
 * microcode is available for the 82558/9, 82550. Currently the
 * microcode handles interrupt bundling and TCO workaround.
 *
 * Returns:
 *      true: if successfull
 *      false: otherwise
 */
static unsigned char
e100_load_microcode(struct e100_private *bdp)
{
	static struct {
		u8 rev_id;
		u32 ucode[UCODE_MAX_DWORDS + 1];
		int timer_dword;
		int bundle_dword;
		int min_size_dword;
	} ucode_opts[] = {
		{ D101A4_REV_ID,
		  D101_A_RCVBUNDLE_UCODE,
		  D101_CPUSAVER_TIMER_DWORD,
		  D101_CPUSAVER_BUNDLE_DWORD,
		  D101_CPUSAVER_MIN_SIZE_DWORD },
		{ D101B0_REV_ID,
		  D101_B0_RCVBUNDLE_UCODE,
		  D101_CPUSAVER_TIMER_DWORD,
		  D101_CPUSAVER_BUNDLE_DWORD,
		  D101_CPUSAVER_MIN_SIZE_DWORD },
		{ D101MA_REV_ID,
		  D101M_B_RCVBUNDLE_UCODE,
		  D101M_CPUSAVER_TIMER_DWORD,
		  D101M_CPUSAVER_BUNDLE_DWORD,
		  D101M_CPUSAVER_MIN_SIZE_DWORD },
		{ D101S_REV_ID,
		  D101S_RCVBUNDLE_UCODE,
		  D101S_CPUSAVER_TIMER_DWORD,
		  D101S_CPUSAVER_BUNDLE_DWORD,
		  D101S_CPUSAVER_MIN_SIZE_DWORD },
		{ D102_REV_ID,
		  D102_B_RCVBUNDLE_UCODE,
		  D102_B_CPUSAVER_TIMER_DWORD,
		  D102_B_CPUSAVER_BUNDLE_DWORD,
		  D102_B_CPUSAVER_MIN_SIZE_DWORD },
		{ D102C_REV_ID,
		  D102_C_RCVBUNDLE_UCODE,
		  D102_C_CPUSAVER_TIMER_DWORD,
		  D102_C_CPUSAVER_BUNDLE_DWORD,
		  D102_C_CPUSAVER_MIN_SIZE_DWORD },
		{ D102E_REV_ID,
		  D102_E_RCVBUNDLE_UCODE,
		  D102_E_CPUSAVER_TIMER_DWORD,
		  D102_E_CPUSAVER_BUNDLE_DWORD,
		  D102_E_CPUSAVER_MIN_SIZE_DWORD },
		{ 0, {0}, 0, 0, 0}
	}, *opts;

	opts = ucode_opts;

	/* User turned ucode loading off */
	if (!(bdp->params.b_params & PRM_UCODE))
		return false;

	/* These controllers do not need ucode */
	if (bdp->flags & IS_ICH)
		return false;

	/* Search for ucode match against h/w rev_id */
	while (opts->rev_id) {
		if (bdp->rev_id == opts->rev_id) {
			int i;
			u32 *ucode_dword;
			load_ucode_cb_t *ucode_cmd_ptr;
			nxmit_cb_entry_t *cmd = e100_alloc_non_tx_cmd(bdp);

			if (cmd != NULL) {
				ucode_cmd_ptr =
					(load_ucode_cb_t *) cmd->non_tx_cmd;
				ucode_dword = ucode_cmd_ptr->ucode_dword;
			} else {
				return false;
			}

			memcpy(ucode_dword, opts->ucode, sizeof (opts->ucode));

			/* Insert user-tunable settings */
			ucode_dword[opts->timer_dword] &= 0xFFFF0000;
			ucode_dword[opts->timer_dword] |=
				(u16) bdp->params.IntDelay;
			ucode_dword[opts->bundle_dword] &= 0xFFFF0000;
			ucode_dword[opts->bundle_dword] |=
				(u16) bdp->params.BundleMax;
			ucode_dword[opts->min_size_dword] &= 0xFFFF0000;
			ucode_dword[opts->min_size_dword] |=
				(bdp->params.b_params & PRM_BUNDLE_SMALL) ?
				0xFFFF : 0xFF80;

			for (i = 0; i < UCODE_MAX_DWORDS; i++)
				cpu_to_le32s(&(ucode_dword[i]));

			ucode_cmd_ptr->load_ucode_cbhdr.cb_cmd =
				__constant_cpu_to_le16(CB_LOAD_MICROCODE);

			return e100_exec_non_cu_cmd(bdp, cmd);
		}
		opts++;
	}

	return false;
}

/***************************************************************************/
/***************************************************************************/
/*       EEPROM  Functions                                                 */
/***************************************************************************/

/* Read PWA (printed wired assembly) number */
void __devinit
e100_rd_pwa_no(struct e100_private *bdp)
{
	bdp->pwa_no = e100_eeprom_read(bdp, EEPROM_PWA_NO);
	bdp->pwa_no <<= 16;
	bdp->pwa_no |= e100_eeprom_read(bdp, EEPROM_PWA_NO + 1);
}

/* Read the permanent ethernet address from the eprom. */
void __devinit
e100_rd_eaddr(struct e100_private *bdp)
{
	int i;
	u16 eeprom_word;

	for (i = 0; i < 6; i += 2) {
		eeprom_word =
			e100_eeprom_read(bdp,
					 EEPROM_NODE_ADDRESS_BYTE_0 + (i / 2));

		bdp->device->dev_addr[i] =
			bdp->perm_node_address[i] = (u8) eeprom_word;
		bdp->device->dev_addr[i + 1] =
			bdp->perm_node_address[i + 1] = (u8) (eeprom_word >> 8);
	}
}

/* Check the D102 RFD flags to see if the checksum passed */
static unsigned char
e100_D102_check_checksum(rfd_t *rfd)
{
	if (((le16_to_cpu(rfd->rfd_header.cb_status)) & RFD_PARSE_BIT)
	    && (((rfd->rcvparserstatus & CHECKSUM_PROTOCOL_MASK) ==
		 RFD_TCP_PACKET)
		|| ((rfd->rcvparserstatus & CHECKSUM_PROTOCOL_MASK) ==
		    RFD_UDP_PACKET))
	    && (rfd->checksumstatus & TCPUDP_CHECKSUM_BIT_VALID)
	    && (rfd->checksumstatus & TCPUDP_CHECKSUM_VALID)) {
		return CHECKSUM_UNNECESSARY;
	}
	return CHECKSUM_NONE;
}

/**
 * e100_D101M_checksum
 * @bdp: atapter's private data struct
 * @skb: skb received
 *
 * Sets the skb->csum value from D101 csum found at the end of the Rx frame. The
 * D101M sums all words in frame excluding the ethernet II header (14 bytes) so
 * in case the packet is ethernet II and the protocol is IP, all is need is to
 * assign this value to skb->csum.
 */
static unsigned char
e100_D101M_checksum(struct e100_private *bdp, struct sk_buff *skb)
{
	unsigned short proto = (skb->protocol);

	if (proto == __constant_htons(ETH_P_IP)) {

		skb->csum = get_unaligned((u16 *) (skb->tail));
		return CHECKSUM_HW;
	}
	return CHECKSUM_NONE;
}

/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/*       Auxilary Functions                                                */
/***************************************************************************/

/* Print the board's configuration */
void __devinit
e100_print_brd_conf(struct e100_private *bdp)
{
	/* Print the string if checksum Offloading was enabled */
	if (bdp->flags & DF_CSUM_OFFLOAD)
		printk(KERN_NOTICE "  Hardware receive checksums enabled\n");
	else {
		if (bdp->rev_id >= D101MA_REV_ID) 
			printk(KERN_NOTICE "  Hardware receive checksums disabled\n");
	}

	if ((bdp->flags & DF_UCODE_LOADED))
		printk(KERN_NOTICE "  cpu cycle saver enabled\n");
}

/**
 * e100_pci_setup - setup the adapter's PCI information
 * @pcid: adapter's pci_dev struct
 * @bdp: atapter's private data struct
 *
 * This routine sets up all PCI information for the adapter. It enables the bus
 * master bit (some BIOS don't do this), requests memory ans I/O regions, and
 * calls ioremap() on the adapter's memory region.
 *
 * Returns:
 *      true: if successfull
 *      false: otherwise
 */
static unsigned char __devinit
e100_pci_setup(struct pci_dev *pcid, struct e100_private *bdp)
{
	struct net_device *dev = bdp->device;
	int rc = 0;

	if ((rc = pci_enable_device(pcid)) != 0) {
		goto err;
	}

	/* dev and ven ID have already been checked so it is our device */
	pci_read_config_byte(pcid, PCI_REVISION_ID, (u8 *) &(bdp->rev_id));

	/* address #0 is a memory region */
	dev->mem_start = pci_resource_start(pcid, 0);
	dev->mem_end = dev->mem_start + sizeof (scb_t);

	/* address #1 is a IO region */
	dev->base_addr = pci_resource_start(pcid, 1);

	if ((rc = pci_request_regions(pcid, e100_short_driver_name)) != 0) {
		goto err_disable;
	}

	pci_enable_wake(pcid, 0, 0);

	/* if Bus Mastering is off, turn it on! */
	pci_set_master(pcid);

	/* address #0 is a memory mapping */
	bdp->scb = (scb_t *) ioremap_nocache(dev->mem_start, sizeof (scb_t));

	if (!bdp->scb) {
		printk(KERN_ERR "e100: %s: Failed to map PCI address 0x%lX\n",
		       dev->name, pci_resource_start(pcid, 0));
		rc = -ENOMEM;
		goto err_region;
	}

	return 0;

err_region:
	pci_release_regions(pcid);
err_disable:
	pci_disable_device(pcid);
err:
	return rc;
}

void
e100_isolate_driver(struct e100_private *bdp)
{

	/* Check if interface is up                              */
	/* NOTE: Can't use netif_running(bdp->device) because    */
	/* dev_close clears __LINK_STATE_START before calling    */
	/* e100_close (aka dev->stop)                            */
	if (bdp->device->flags & IFF_UP) {
		e100_disable_clear_intr(bdp);
		del_timer_sync(&bdp->watchdog_timer);
		netif_carrier_off(bdp->device);
		netif_stop_queue(bdp->device); 
		bdp->last_tcb = NULL;
	} 
	e100_sw_reset(bdp, PORT_SELECTIVE_RESET);
}

static void
e100_tcb_add_C_bit(struct e100_private *bdp)
{
	tcb_t *tcb = (tcb_t *) bdp->tcb_pool.data;
	int i;

	for (i = 0; i < bdp->params.TxDescriptors; i++, tcb++) {
		tcb->tcb_hdr.cb_status |= cpu_to_le16(CB_STATUS_COMPLETE);
	}
}

/* 
 * Procedure:   e100_configure_device
 *
 * Description: This routine will configure device
 *
 * Arguments:
 *      bdp - Ptr to this card's e100_bdconfig structure
 *
 * Returns:
 *        true upon success
 *        false upon failure
 */
unsigned char
e100_configure_device(struct e100_private *bdp)
{
	/*load CU & RU base */
	if (!e100_wait_exec_cmplx(bdp, 0, SCB_CUC_LOAD_BASE, 0))
		return false;

	if (e100_load_microcode(bdp))
		bdp->flags |= DF_UCODE_LOADED;

	if (!e100_wait_exec_cmplx(bdp, 0, SCB_RUC_LOAD_BASE, 0))
		return false;

	/* Issue the load dump counters address command */
	if (!e100_wait_exec_cmplx(bdp, bdp->stat_cnt_phys, SCB_CUC_DUMP_ADDR, 0))
		return false;

	if (!e100_setup_iaaddr(bdp, bdp->device->dev_addr)) {
		printk(KERN_ERR "e100: e100_configure_device: "
			"setup iaaddr failed\n");
		return false;
	}

	e100_set_multi_exec(bdp->device);

	/* Change for 82558 enhancement                                */
	/* If 82558/9 and if the user has enabled flow control, set up */
	/* flow Control Reg. in the CSR                                */
	if ((bdp->flags & IS_BACHELOR)
	    && (bdp->params.b_params & PRM_FC)) {
		writeb(DFLT_FC_THLD,
			&bdp->scb->scb_ext.d101_scb.scb_fc_thld);
		writeb(DFLT_FC_CMD,
			&bdp->scb->scb_ext.d101_scb.scb_fc_xon_xoff);
	}

	e100_force_config(bdp);

	return true;
}

void
e100_deisolate_driver(struct e100_private *bdp, u8 full_reset)
{
	u32 cmd = full_reset ? PORT_SOFTWARE_RESET : PORT_SELECTIVE_RESET;
	e100_sw_reset(bdp, cmd);
	if (cmd == PORT_SOFTWARE_RESET) {
		if (!e100_configure_device(bdp))
			printk(KERN_ERR "e100: e100_deisolate_driver:" 
		       		" device configuration failed\n");
	} 

	if (netif_running(bdp->device)) {

		bdp->next_cu_cmd = START_WAIT;
		bdp->last_tcb = NULL;

		e100_start_ru(bdp);

		/* relaunch watchdog timer in 2 sec */
		mod_timer(&(bdp->watchdog_timer), jiffies + (2 * HZ));

		// we must clear tcbs since we may have lost Tx intrrupt
		// or have unsent frames on the tcb chain
		e100_tcb_add_C_bit(bdp);
		e100_tx_srv(bdp);
		netif_wake_queue(bdp->device);
		e100_set_intr_mask(bdp);
	}
}

static int
e100_do_ethtool_ioctl(struct net_device *dev, struct ifreq *ifr)
{
	struct ethtool_cmd ecmd;
	int rc = -EOPNOTSUPP;

	if (copy_from_user(&ecmd, ifr->ifr_data, sizeof (ecmd.cmd)))
		return -EFAULT;

	switch (ecmd.cmd) {
	case ETHTOOL_GSET:
		rc = e100_ethtool_get_settings(dev, ifr);
		break;
	case ETHTOOL_SSET:
		rc = e100_ethtool_set_settings(dev, ifr);
		break;
	case ETHTOOL_GDRVINFO:
		rc = e100_ethtool_get_drvinfo(dev, ifr);
		break;
	case ETHTOOL_GREGS:
		rc = e100_ethtool_gregs(dev, ifr);
		break;
	case ETHTOOL_NWAY_RST:
		rc = e100_ethtool_nway_rst(dev, ifr);
		break;
	case ETHTOOL_GLINK:
		rc = e100_ethtool_glink(dev, ifr);
		break;
	case ETHTOOL_GEEPROM:
	case ETHTOOL_SEEPROM:
		rc = e100_ethtool_eeprom(dev, ifr);
		break;
	case ETHTOOL_GSTATS: {
		struct {
			struct ethtool_stats cmd;
			uint64_t data[E100_STATS_LEN];
		} stats = { {ETHTOOL_GSTATS, E100_STATS_LEN} };
		struct e100_private *bdp = dev->priv;
		void *addr = ifr->ifr_data;
		int i;

		for(i = 0; i < E100_NET_STATS_LEN; i++)
			stats.data[i] =
				((unsigned long *)&bdp->drv_stats.net_stats)[i];
		stats.data[i++] = bdp->drv_stats.tx_late_col;
		stats.data[i++] = bdp->drv_stats.tx_ok_defrd;
		stats.data[i++] = bdp->drv_stats.tx_one_retry;
		stats.data[i++] = bdp->drv_stats.tx_mt_one_retry;
		stats.data[i++] = bdp->drv_stats.rcv_cdt_frames;
		stats.data[i++] = bdp->drv_stats.xmt_fc_pkts;
		stats.data[i++] = bdp->drv_stats.rcv_fc_pkts;
		stats.data[i++] = bdp->drv_stats.rcv_fc_unsupported;
		stats.data[i++] = bdp->drv_stats.xmt_tco_pkts;
		stats.data[i++] = bdp->drv_stats.rcv_tco_pkts;
		if(copy_to_user(addr, &stats, sizeof(stats)))
			return -EFAULT;
		return 0;
	}
	case ETHTOOL_GWOL:
	case ETHTOOL_SWOL:
		rc = e100_ethtool_wol(dev, ifr);
		break;
	case ETHTOOL_TEST:
		rc = e100_ethtool_test(dev, ifr);
		break;
	case ETHTOOL_GSTRINGS:
		rc = e100_ethtool_gstrings(dev,ifr);
		break;
	case ETHTOOL_PHYS_ID:
		rc = e100_ethtool_led_blink(dev,ifr);
		break;
#ifdef	ETHTOOL_GRINGPARAM
	case ETHTOOL_GRINGPARAM: {
		struct ethtool_ringparam ering;
		struct e100_private *bdp = dev->priv;
		memset((void *) &ering, 0, sizeof(ering));
		ering.rx_max_pending = E100_MAX_RFD;
		ering.tx_max_pending = E100_MAX_TCB;
		ering.rx_pending = bdp->params.RxDescriptors;
		ering.tx_pending = bdp->params.TxDescriptors;
		rc = copy_to_user(ifr->ifr_data, &ering, sizeof(ering))
			? -EFAULT : 0;
		return rc;
	}
#endif
#ifdef	ETHTOOL_SRINGPARAM
	case ETHTOOL_SRINGPARAM: {
		struct ethtool_ringparam ering;
		struct e100_private *bdp = dev->priv;
		if (copy_from_user(&ering, ifr->ifr_data, sizeof(ering)))
			return -EFAULT;
		if (ering.rx_pending > E100_MAX_RFD 
		    || ering.rx_pending < E100_MIN_RFD)
			return -EINVAL;
		if (ering.tx_pending > E100_MAX_TCB 
		    || ering.tx_pending < E100_MIN_TCB)
			return -EINVAL;
		if (netif_running(dev)) {
			spin_lock_bh(&dev->xmit_lock);
			e100_close(dev);
			spin_unlock_bh(&dev->xmit_lock);
			/* Use new values to open interface */
			bdp->params.RxDescriptors = ering.rx_pending;
			bdp->params.TxDescriptors = ering.tx_pending;
			e100_hw_init(bdp);
			e100_open(dev);
		}
		else {
			bdp->params.RxDescriptors = ering.rx_pending;
			bdp->params.TxDescriptors = ering.tx_pending;
		}
		return 0;
	}
#endif
#ifdef	ETHTOOL_GPAUSEPARAM
	case ETHTOOL_GPAUSEPARAM: {
		struct ethtool_pauseparam epause;
		struct e100_private *bdp = dev->priv;
		memset((void *) &epause, 0, sizeof(epause));
		if ((bdp->flags & IS_BACHELOR)
		    && (bdp->params.b_params & PRM_FC)) {
			epause.autoneg = 1;
			if (bdp->flags && DF_LINK_FC_CAP) {
				epause.rx_pause = 1;
				epause.tx_pause = 1;
			}
			if (bdp->flags && DF_LINK_FC_TX_ONLY)
				epause.tx_pause = 1;
		}
		rc = copy_to_user(ifr->ifr_data, &epause, sizeof(epause))
			? -EFAULT : 0;
		return rc;
	}
#endif
#ifdef	ETHTOOL_SPAUSEPARAM
	case ETHTOOL_SPAUSEPARAM: {
		struct ethtool_pauseparam epause;
		struct e100_private *bdp = dev->priv;
		if (!(bdp->flags & IS_BACHELOR))
			return -EINVAL;
		if (copy_from_user(&epause, ifr->ifr_data, sizeof(epause)))
			return -EFAULT;
		if (epause.autoneg == 1)
			bdp->params.b_params |= PRM_FC;
		else
			bdp->params.b_params &= ~PRM_FC;
		if (netif_running(dev)) {
			spin_lock_bh(&dev->xmit_lock);
			e100_close(dev);
			spin_unlock_bh(&dev->xmit_lock);
			e100_hw_init(bdp);
			e100_open(dev);
		}
		return 0;
	}
#endif
#ifdef	ETHTOOL_GRXCSUM
	case ETHTOOL_GRXCSUM:
	case ETHTOOL_GTXCSUM:
	case ETHTOOL_GSG:
	{	struct ethtool_value eval;
		struct e100_private *bdp = dev->priv;
		memset((void *) &eval, 0, sizeof(eval));
		if ((ecmd.cmd == ETHTOOL_GRXCSUM) 
		    && (bdp->params.b_params & PRM_XSUMRX))
			eval.data = 1;
		else
			eval.data = 0;
		rc = copy_to_user(ifr->ifr_data, &eval, sizeof(eval))
			? -EFAULT : 0;
		return rc;
	}
#endif
#ifdef	ETHTOOL_SRXCSUM
	case ETHTOOL_SRXCSUM:
	case ETHTOOL_STXCSUM:
	case ETHTOOL_SSG:
	{	struct ethtool_value eval;
		struct e100_private *bdp = dev->priv;
		if (copy_from_user(&eval, ifr->ifr_data, sizeof(eval)))
			return -EFAULT;
		if (ecmd.cmd == ETHTOOL_SRXCSUM) {
			if (eval.data == 1) { 
				if (bdp->rev_id >= D101MA_REV_ID)
					bdp->params.b_params |= PRM_XSUMRX;
				else
					return -EINVAL;
			} else {
				if (bdp->rev_id >= D101MA_REV_ID)
					bdp->params.b_params &= ~PRM_XSUMRX;
				else
					return 0;
			}
		} else {
			if (eval.data == 1)
				return -EINVAL;
			else
				return 0;
		}
		if (netif_running(dev)) {
			spin_lock_bh(&dev->xmit_lock);
			e100_close(dev);
			spin_unlock_bh(&dev->xmit_lock);
			e100_hw_init(bdp);
			e100_open(dev);
		}
		return 0;
	}
#endif
	default:
		break;
	}			//switch
	return rc;
}

static int
e100_ethtool_get_settings(struct net_device *dev, struct ifreq *ifr)
{
	struct e100_private *bdp;
	struct ethtool_cmd ecmd;
	u16 advert = 0;

	memset((void *) &ecmd, 0, sizeof (ecmd));

	bdp = dev->priv;

	ecmd.supported = bdp->speed_duplex_caps;

	ecmd.port =
		(bdp->speed_duplex_caps & SUPPORTED_TP) ? PORT_TP : PORT_FIBRE;
	ecmd.transceiver = XCVR_INTERNAL;
	ecmd.phy_address = bdp->phy_addr;

	if (netif_carrier_ok(bdp->device)) {
		ecmd.speed = bdp->cur_line_speed;
		ecmd.duplex =
			(bdp->cur_dplx_mode == HALF_DUPLEX) ? DUPLEX_HALF : DUPLEX_FULL;
	}
	else {
		ecmd.speed = -1;
		ecmd.duplex = -1;
	}

	ecmd.advertising = ADVERTISED_TP;

	if (bdp->params.e100_speed_duplex == E100_AUTONEG) {
		ecmd.autoneg = AUTONEG_ENABLE;
		ecmd.advertising |= ADVERTISED_Autoneg;
	} else {
		ecmd.autoneg = AUTONEG_DISABLE;
	}

	if (bdp->speed_duplex_caps & SUPPORTED_MII) {
		e100_mdi_read(bdp, MII_ADVERTISE, bdp->phy_addr, &advert);

		if (advert & ADVERTISE_10HALF)
			ecmd.advertising |= ADVERTISED_10baseT_Half;
		if (advert & ADVERTISE_10FULL)
			ecmd.advertising |= ADVERTISED_10baseT_Full;
		if (advert & ADVERTISE_100HALF)
			ecmd.advertising |= ADVERTISED_100baseT_Half;
		if (advert & ADVERTISE_100FULL)
			ecmd.advertising |= ADVERTISED_100baseT_Full;
	} else {
		ecmd.autoneg = AUTONEG_DISABLE;
		ecmd.advertising &= ~ADVERTISED_Autoneg;
	}

	if (copy_to_user(ifr->ifr_data, &ecmd, sizeof (ecmd)))
		return -EFAULT;

	return 0;
}

static int
e100_ethtool_set_settings(struct net_device *dev, struct ifreq *ifr)
{
	struct e100_private *bdp;
	int e100_new_speed_duplex;
	int ethtool_new_speed_duplex;
	struct ethtool_cmd ecmd;

	bdp = dev->priv;
	if (copy_from_user(&ecmd, ifr->ifr_data, sizeof (ecmd))) {
		return -EFAULT;
	}

	if ((ecmd.autoneg == AUTONEG_ENABLE)
	    && (bdp->speed_duplex_caps & SUPPORTED_Autoneg)) {
		bdp->params.e100_speed_duplex = E100_AUTONEG;
		if (netif_running(dev)) {
			spin_lock_bh(&dev->xmit_lock);
			e100_close(dev);
			spin_unlock_bh(&dev->xmit_lock);
			e100_hw_init(bdp);
			e100_open(dev);
		}
	} else {
		if (ecmd.speed == SPEED_10) {
			if (ecmd.duplex == DUPLEX_HALF) {
				e100_new_speed_duplex =
					E100_SPEED_10_HALF;
				ethtool_new_speed_duplex =
					SUPPORTED_10baseT_Half;
			} else { 
				e100_new_speed_duplex =
					E100_SPEED_10_FULL;
				ethtool_new_speed_duplex =
					SUPPORTED_10baseT_Full;
			} 
		} else { 
			if (ecmd.duplex == DUPLEX_HALF) {
				e100_new_speed_duplex =
					E100_SPEED_100_HALF;
				ethtool_new_speed_duplex =
					SUPPORTED_100baseT_Half;
			} else { 
				e100_new_speed_duplex =
					E100_SPEED_100_FULL;
				ethtool_new_speed_duplex =
					SUPPORTED_100baseT_Full;
			} 
		}

		if (bdp->speed_duplex_caps & ethtool_new_speed_duplex) {
			bdp->params.e100_speed_duplex =
				e100_new_speed_duplex;
			if (netif_running(dev)) {
				spin_lock_bh(&dev->xmit_lock);
				e100_close(dev);
				spin_unlock_bh(&dev->xmit_lock);
				e100_hw_init(bdp);
				e100_open(dev);
			}
		} else {
			return -EOPNOTSUPP;
		} 
	}

	return 0;
}

static int
e100_ethtool_glink(struct net_device *dev, struct ifreq *ifr)
{
	struct e100_private *bdp;
	struct ethtool_value info;

	memset((void *) &info, 0, sizeof (info));

	bdp = dev->priv;
	info.cmd = ETHTOOL_GLINK;

	/* Consider both PHY link and netif_running */
	info.data = e100_update_link_state(bdp);

	if (copy_to_user(ifr->ifr_data, &info, sizeof (info)))
		return -EFAULT;

	return 0;
}

static int
e100_ethtool_test(struct net_device *dev, struct ifreq *ifr)
{
	struct ethtool_test *info;
	int rc = -EFAULT;

	info = kmalloc(sizeof(*info) + max_test_res * sizeof(u64),
		       GFP_ATOMIC);

	if (!info)
		return -ENOMEM;

	memset((void *) info, 0, sizeof(*info) +
				 max_test_res * sizeof(u64));

	if (copy_from_user(info, ifr->ifr_data, sizeof(*info)))
		goto exit;

	info->flags = e100_run_diag(dev, info->data, info->flags);

	if (!copy_to_user(ifr->ifr_data, info,
			 sizeof(*info) + max_test_res * sizeof(u64)))
		rc = 0;
exit:
	kfree(info);
	return rc;
}

static int
e100_ethtool_gregs(struct net_device *dev, struct ifreq *ifr)
{
	struct e100_private *bdp;
	u32 regs_buff[E100_REGS_LEN];
	struct ethtool_regs regs = {ETHTOOL_GREGS};
	void *addr = ifr->ifr_data;
	u16 mdi_reg;

	bdp = dev->priv;

	if(copy_from_user(&regs, addr, sizeof(regs)))
		return -EFAULT;

	regs.version = (1 << 24) | bdp->rev_id;
	regs_buff[0] = readb(&(bdp->scb->scb_cmd_hi)) << 24 |
		readb(&(bdp->scb->scb_cmd_low)) << 16 |
		readw(&(bdp->scb->scb_status));
	e100_mdi_read(bdp, MII_NCONFIG, bdp->phy_addr, &mdi_reg);
	regs_buff[1] = mdi_reg;

	if(copy_to_user(addr, &regs, sizeof(regs)))
		return -EFAULT;

	addr += offsetof(struct ethtool_regs, data);
	if(copy_to_user(addr, regs_buff, regs.len))
		return -EFAULT;

	return 0;
}

static int
e100_ethtool_nway_rst(struct net_device *dev, struct ifreq *ifr)
{
	struct e100_private *bdp;

	bdp = dev->priv;

	if ((bdp->speed_duplex_caps & SUPPORTED_Autoneg) &&
	    (bdp->params.e100_speed_duplex == E100_AUTONEG)) {
		if (netif_running(dev)) {
			spin_lock_bh(&dev->xmit_lock);
			e100_close(dev);
			spin_unlock_bh(&dev->xmit_lock);
			e100_hw_init(bdp);
			e100_open(dev);
		}
	} else {
		return -EFAULT;
	}
	return 0;
}

static int
e100_ethtool_get_drvinfo(struct net_device *dev, struct ifreq *ifr)
{
	struct e100_private *bdp;
	struct ethtool_drvinfo info;

	memset((void *) &info, 0, sizeof (info));

	bdp = dev->priv;

	strncpy(info.driver, e100_short_driver_name, sizeof (info.driver) - 1);
	strncpy(info.version, e100_driver_version, sizeof (info.version) - 1);
	strncpy(info.fw_version, "N/A",
		sizeof (info.fw_version) - 1);
	strncpy(info.bus_info, pci_name(bdp->pdev),
		sizeof (info.bus_info) - 1);
	info.n_stats = E100_STATS_LEN;
	info.regdump_len  = E100_REGS_LEN * sizeof(u32);
	info.eedump_len = (bdp->eeprom_size << 1);	
	info.testinfo_len = max_test_res;
	if (copy_to_user(ifr->ifr_data, &info, sizeof (info)))
		return -EFAULT;

	return 0;
}

static int
e100_ethtool_eeprom(struct net_device *dev, struct ifreq *ifr)
{
	struct e100_private *bdp;
	struct ethtool_eeprom ecmd;
	u16 eeprom_data[256];
	u16 *usr_eeprom_ptr;
	u16 first_word, last_word;
	int i, max_len;
	void *ptr;
	u8 *eeprom_data_bytes = (u8 *)eeprom_data;

	bdp = dev->priv;

	if (copy_from_user(&ecmd, ifr->ifr_data, sizeof (ecmd)))
		return -EFAULT;

	usr_eeprom_ptr =
		(u16 *) (ifr->ifr_data + offsetof(struct ethtool_eeprom, data));

        max_len = bdp->eeprom_size * 2;
        
        if (ecmd.offset > ecmd.offset + ecmd.len)
        	return -EINVAL;
        	
	if ((ecmd.offset + ecmd.len) > max_len)
		ecmd.len = (max_len - ecmd.offset);

	first_word = ecmd.offset >> 1;
	last_word = (ecmd.offset + ecmd.len - 1) >> 1;
		
	if (first_word >= bdp->eeprom_size)
		return -EFAULT;

	if (ecmd.cmd == ETHTOOL_GEEPROM) {
        	for(i = 0; i <= (last_word - first_word); i++)
			eeprom_data[i] = e100_eeprom_read(bdp, first_word + i);

		ecmd.magic = E100_EEPROM_MAGIC;

		if (copy_to_user(ifr->ifr_data, &ecmd, sizeof (ecmd)))
			return -EFAULT;

		if(ecmd.offset & 1)
			eeprom_data_bytes++;
		if (copy_to_user(usr_eeprom_ptr, eeprom_data_bytes, ecmd.len))
			return -EFAULT;
	} else {
		if (ecmd.magic != E100_EEPROM_MAGIC)
			return -EFAULT;

		ptr = (void *)eeprom_data;
        	if(ecmd.offset & 1) {
                	/* need modification of first changed EEPROM word */
                	/* only the second byte of the word is being modified */
			eeprom_data[0] = e100_eeprom_read(bdp, first_word);
                	ptr++;
        	}
        	if((ecmd.offset + ecmd.len) & 1) {
	                /* need modification of last changed EEPROM word */
	                /* only the first byte of the word is being modified */
			eeprom_data[last_word - first_word] = 
				e100_eeprom_read(bdp, last_word);
		}
        	if(copy_from_user(ptr, usr_eeprom_ptr, ecmd.len))
	                return -EFAULT;

		e100_eeprom_write_block(bdp, first_word, eeprom_data,
					last_word - first_word + 1);

		if (copy_to_user(ifr->ifr_data, &ecmd, sizeof (ecmd)))
			return -EFAULT;
	}
	return 0;
}

#define E100_BLINK_INTERVAL	(HZ/4)
/**
 * e100_led_control
 * @bdp: atapter's private data struct
 * @led_mdi_op: led operation
 *
 * Software control over adapter's led. The possible operations are:
 * TURN LED OFF, TURN LED ON and RETURN LED CONTROL TO HARDWARE.
 */
static void
e100_led_control(struct e100_private *bdp, u16 led_mdi_op)
{
	e100_mdi_write(bdp, PHY_82555_LED_SWITCH_CONTROL,
		       bdp->phy_addr, led_mdi_op);

}
/**
 * e100_led_blink_callback
 * @data: pointer to atapter's private data struct
 *
 * Blink timer callback function. Toggles ON/OFF led status bit and calls
 * led hardware access function. 
 */
static void
e100_led_blink_callback(unsigned long data)
{
	struct e100_private *bdp = (struct e100_private *) data;

	if(bdp->flags & LED_IS_ON) {
		bdp->flags &= ~LED_IS_ON;
		e100_led_control(bdp, PHY_82555_LED_OFF);
	} else {
		bdp->flags |= LED_IS_ON;
		if (bdp->rev_id >= D101MA_REV_ID)
			e100_led_control(bdp, PHY_82555_LED_ON_559);
		else
			e100_led_control(bdp, PHY_82555_LED_ON_PRE_559);
	}

	mod_timer(&bdp->blink_timer, jiffies + E100_BLINK_INTERVAL);
}
/**
 * e100_ethtool_led_blink
 * @dev: pointer to atapter's net_device struct
 * @ifr: pointer to ioctl request structure
 *
 * Blink led ioctl handler. Initialtes blink timer and sleeps until
 * blink period expires. Than it kills timer and returns. The led control
 * is returned back to hardware when blink timer is killed.
 */
static int
e100_ethtool_led_blink(struct net_device *dev, struct ifreq *ifr)
{
	struct e100_private *bdp;
	struct ethtool_value ecmd;

	bdp = dev->priv;

	if (copy_from_user(&ecmd, ifr->ifr_data, sizeof (ecmd)))
		return -EFAULT;

	if(!bdp->blink_timer.function) {
		init_timer(&bdp->blink_timer);
		bdp->blink_timer.function = e100_led_blink_callback;
		bdp->blink_timer.data = (unsigned long) bdp;
	}

	mod_timer(&bdp->blink_timer, jiffies);

	set_current_state(TASK_INTERRUPTIBLE);

	if ((!ecmd.data) || (ecmd.data > (u32)(MAX_SCHEDULE_TIMEOUT / HZ)))
		ecmd.data = (u32)(MAX_SCHEDULE_TIMEOUT / HZ);

	schedule_timeout(ecmd.data * HZ);

	del_timer_sync(&bdp->blink_timer);

	e100_led_control(bdp, PHY_82555_LED_NORMAL_CONTROL);

	return 0;
}

static inline int __devinit
e100_10BaseT_adapter(struct e100_private *bdp)
{
	return ((bdp->pdev->device == 0x1229) &&
		(bdp->pdev->subsystem_vendor == 0x8086) &&
		(bdp->pdev->subsystem_device == 0x0003));
}

static void __devinit
e100_get_speed_duplex_caps(struct e100_private *bdp)
{
	u16 status;

	e100_mdi_read(bdp, MII_BMSR, bdp->phy_addr, &status);

	bdp->speed_duplex_caps = 0;

	bdp->speed_duplex_caps |=
		(status & BMSR_ANEGCAPABLE) ? SUPPORTED_Autoneg : 0;

	bdp->speed_duplex_caps |=
		(status & BMSR_10HALF) ? SUPPORTED_10baseT_Half : 0;

	bdp->speed_duplex_caps |=
		(status & BMSR_10FULL) ? SUPPORTED_10baseT_Full : 0;

	bdp->speed_duplex_caps |=
		(status & BMSR_100HALF) ? SUPPORTED_100baseT_Half : 0;

	bdp->speed_duplex_caps |=
		(status & BMSR_100FULL) ? SUPPORTED_100baseT_Full : 0;

	if (IS_NC3133(bdp))
		bdp->speed_duplex_caps =
			(SUPPORTED_FIBRE | SUPPORTED_100baseT_Full);
	else
		bdp->speed_duplex_caps |= SUPPORTED_TP;

	if ((status == 0xFFFF) && e100_10BaseT_adapter(bdp)) {
		bdp->speed_duplex_caps =
			(SUPPORTED_10baseT_Half | SUPPORTED_TP);
	} else {
		bdp->speed_duplex_caps |= SUPPORTED_MII;
	}

}

#ifdef CONFIG_PM
static unsigned char
e100_setup_filter(struct e100_private *bdp)
{
	cb_header_t *ntcb_hdr;
	unsigned char res = false;
	nxmit_cb_entry_t *cmd;

	if ((cmd = e100_alloc_non_tx_cmd(bdp)) == NULL) {
		goto exit;
	}

	ntcb_hdr = (cb_header_t *) cmd->non_tx_cmd;
	ntcb_hdr->cb_cmd = __constant_cpu_to_le16(CB_LOAD_FILTER);

	/* Set EL and FIX bit */
	(cmd->non_tx_cmd)->ntcb.filter.filter_data[0] =
		__constant_cpu_to_le32(CB_FILTER_EL | CB_FILTER_FIX);

	if (bdp->wolopts & WAKE_UCAST) {
		(cmd->non_tx_cmd)->ntcb.filter.filter_data[0] |=
			__constant_cpu_to_le32(CB_FILTER_IA_MATCH);
	}

	if (bdp->wolopts & WAKE_ARP) {
		/* Setup ARP bit and lower IP parts */
		/* bdp->ip_lbytes contains 2 lower bytes of IP address in network byte order */
		(cmd->non_tx_cmd)->ntcb.filter.filter_data[0] |=
			cpu_to_le32(CB_FILTER_ARP | bdp->ip_lbytes);
	}

	res = e100_exec_non_cu_cmd(bdp, cmd);
	if (!res)
		printk(KERN_WARNING "e100: %s: Filter setup failed\n",
		       bdp->device->name);

exit:
	return res;

}

static void
e100_do_wol(struct pci_dev *pcid, struct e100_private *bdp)
{
	e100_config_wol(bdp);

	if (e100_config(bdp)) {
		if (bdp->wolopts & (WAKE_UCAST | WAKE_ARP))
			if (!e100_setup_filter(bdp))
				printk(KERN_ERR
				       "e100: WOL options failed\n");
	} else {
		printk(KERN_ERR "e100: config WOL failed\n");
	}
}
#endif

static u16
e100_get_ip_lbytes(struct net_device *dev)
{
	struct in_ifaddr *ifa;
	struct in_device *in_dev;
	u32 res = 0;

	in_dev = (struct in_device *) dev->ip_ptr;
	/* Check if any in_device bound to interface */
	if (in_dev) {
		/* Check if any IP address is bound to interface */
		if ((ifa = in_dev->ifa_list) != NULL) {
			res = __constant_ntohl(ifa->ifa_address);
			res = __constant_htons(res & 0x0000ffff);
		}
	}
	return res;
}

static int
e100_ethtool_wol(struct net_device *dev, struct ifreq *ifr)
{
	struct e100_private *bdp;
	struct ethtool_wolinfo wolinfo;
	int res = 0;

	bdp = dev->priv;

	if (copy_from_user(&wolinfo, ifr->ifr_data, sizeof (wolinfo))) {
		return -EFAULT;
	}

	switch (wolinfo.cmd) {
	case ETHTOOL_GWOL:
		wolinfo.supported = bdp->wolsupported;
		wolinfo.wolopts = bdp->wolopts;
		if (copy_to_user(ifr->ifr_data, &wolinfo, sizeof (wolinfo)))
			res = -EFAULT;
		break;
	case ETHTOOL_SWOL:
		/* If ALL requests are supported or request is DISABLE wol */
		if (((wolinfo.wolopts & bdp->wolsupported) == wolinfo.wolopts)
		    || (wolinfo.wolopts == 0)) {
			bdp->wolopts = wolinfo.wolopts;
		} else {
			res = -EOPNOTSUPP;
		}
		if (wolinfo.wolopts & WAKE_ARP)
			bdp->ip_lbytes = e100_get_ip_lbytes(dev);
		break;
	default:
		break;
	}
	return res;
}

static int e100_ethtool_gstrings(struct net_device *dev, struct ifreq *ifr)
{
	struct ethtool_gstrings info;
	char *strings = NULL;
	char *usr_strings;
	int i;

	memset((void *) &info, 0, sizeof(info));

	usr_strings = (u8 *) (ifr->ifr_data + 
			      offsetof(struct ethtool_gstrings, data));

	if (copy_from_user(&info, ifr->ifr_data, sizeof (info)))
		return -EFAULT;

	switch (info.string_set) {
	case ETH_SS_TEST: {
		int ret = 0;
		if (info.len > max_test_res)
			info.len = max_test_res;
		strings = kmalloc(info.len * ETH_GSTRING_LEN, GFP_ATOMIC);
		if (!strings)
			return -ENOMEM;
		memset(strings, 0, info.len * ETH_GSTRING_LEN);

		for (i = 0; i < info.len; i++) {
			sprintf(strings + i * ETH_GSTRING_LEN, "%s",
				test_strings[i]);
		}
		if (copy_to_user(ifr->ifr_data, &info, sizeof (info)))
			ret = -EFAULT;
		if (copy_to_user(usr_strings, strings, info.len * ETH_GSTRING_LEN))
			ret = -EFAULT;
		kfree(strings);
		return ret;
	}
	case ETH_SS_STATS: {
		char *strings = NULL;
		void *addr = ifr->ifr_data;
		info.len = E100_STATS_LEN;
		strings = *e100_gstrings_stats;
		if(copy_to_user(ifr->ifr_data, &info, sizeof(info)))
			return -EFAULT;
		addr += offsetof(struct ethtool_gstrings, data);
		if(copy_to_user(addr, strings,
		   info.len * ETH_GSTRING_LEN))
			return -EFAULT;
		return 0;
	}
	default:
		return -EOPNOTSUPP;
	}
}

static int
e100_mii_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	struct e100_private *bdp;
	struct mii_ioctl_data *data_ptr =
		(struct mii_ioctl_data *) &(ifr->ifr_data);

	bdp = dev->priv;

	switch (cmd) {
	case SIOCGMIIPHY:
		data_ptr->phy_id = bdp->phy_addr & 0x1f;
		break;

	case SIOCGMIIREG:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
		e100_mdi_read(bdp, data_ptr->reg_num & 0x1f, bdp->phy_addr,
			      &(data_ptr->val_out));
		break;

	case SIOCSMIIREG:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
		/* If reg = 0 && change speed/duplex */
		if (data_ptr->reg_num == 0 && 
			(data_ptr->val_in == (BMCR_ANENABLE | BMCR_ANRESTART) /* restart cmd */
			|| data_ptr->val_in == (BMCR_RESET) /* reset cmd */ 
			|| data_ptr->val_in & (BMCR_SPEED100 | BMCR_FULLDPLX) 
			|| data_ptr->val_in == 0)) {
				if (data_ptr->val_in == (BMCR_ANENABLE | BMCR_ANRESTART)
					|| data_ptr->val_in == (BMCR_RESET))
					bdp->params.e100_speed_duplex = E100_AUTONEG;
				else if (data_ptr->val_in == (BMCR_SPEED100 | BMCR_FULLDPLX))
					bdp->params.e100_speed_duplex = E100_SPEED_100_FULL;
				else if (data_ptr->val_in == (BMCR_SPEED100))
					bdp->params.e100_speed_duplex = E100_SPEED_100_HALF;
				else if (data_ptr->val_in == (BMCR_FULLDPLX))
					bdp->params.e100_speed_duplex = E100_SPEED_10_FULL;
				else
					bdp->params.e100_speed_duplex = E100_SPEED_10_HALF;
				if (netif_running(dev)) {
					spin_lock_bh(&dev->xmit_lock);
					e100_close(dev);
					spin_unlock_bh(&dev->xmit_lock);
					e100_hw_init(bdp);
					e100_open(dev);
				}
		}
		else 
			/* Only allows changing speed/duplex */
			return -EINVAL;
		
		break;

	default:
		return -EOPNOTSUPP;
	}
	return 0;
}

nxmit_cb_entry_t *
e100_alloc_non_tx_cmd(struct e100_private *bdp)
{
	nxmit_cb_entry_t *non_tx_cmd_elem;

	if (!(non_tx_cmd_elem = (nxmit_cb_entry_t *)
	      kmalloc(sizeof (nxmit_cb_entry_t), GFP_ATOMIC))) {
		return NULL;
	}
	non_tx_cmd_elem->non_tx_cmd =
		pci_alloc_consistent(bdp->pdev, sizeof (nxmit_cb_t),
				     &(non_tx_cmd_elem->dma_addr));
	if (non_tx_cmd_elem->non_tx_cmd == NULL) {
		kfree(non_tx_cmd_elem);
		return NULL;
	}
	return non_tx_cmd_elem;
}

void
e100_free_non_tx_cmd(struct e100_private *bdp,
		     nxmit_cb_entry_t *non_tx_cmd_elem)
{
	pci_free_consistent(bdp->pdev, sizeof (nxmit_cb_t),
			    non_tx_cmd_elem->non_tx_cmd,
			    non_tx_cmd_elem->dma_addr);
	kfree(non_tx_cmd_elem);
}

static void
e100_free_nontx_list(struct e100_private *bdp)
{
	nxmit_cb_entry_t *command;
	int i;

	while (!list_empty(&bdp->non_tx_cmd_list)) {
		command = list_entry(bdp->non_tx_cmd_list.next,
				     nxmit_cb_entry_t, list_elem);
		list_del(&(command->list_elem));
		e100_free_non_tx_cmd(bdp, command);
	}

	for (i = 0; i < CB_MAX_NONTX_CMD; i++) {
		bdp->same_cmd_entry[i] = NULL;
	}
}

static unsigned char
e100_delayed_exec_non_cu_cmd(struct e100_private *bdp,
			     nxmit_cb_entry_t *command)
{
	nxmit_cb_entry_t *same_command;
	cb_header_t *ntcb_hdr;
	u16 cmd;

	ntcb_hdr = (cb_header_t *) command->non_tx_cmd;

	cmd = CB_CMD_MASK & le16_to_cpu(ntcb_hdr->cb_cmd);

	spin_lock_bh(&(bdp->bd_non_tx_lock));

	same_command = bdp->same_cmd_entry[cmd];

	if (same_command != NULL) {
		memcpy((void *) (same_command->non_tx_cmd),
		       (void *) (command->non_tx_cmd), sizeof (nxmit_cb_t));
		e100_free_non_tx_cmd(bdp, command);
	} else {
		list_add_tail(&(command->list_elem), &(bdp->non_tx_cmd_list));
		bdp->same_cmd_entry[cmd] = command;
	}

	if (bdp->non_tx_command_state == E100_NON_TX_IDLE) {
		bdp->non_tx_command_state = E100_WAIT_TX_FINISH;
		mod_timer(&(bdp->nontx_timer_id), jiffies + 1);
	}

	spin_unlock_bh(&(bdp->bd_non_tx_lock));
	return true;
}

static void
e100_non_tx_background(unsigned long ptr)
{
	struct e100_private *bdp = (struct e100_private *) ptr;
	nxmit_cb_entry_t *active_command;
	int restart = true;
	cb_header_t *non_tx_cmd;
	u8 sub_cmd;

	spin_lock_bh(&(bdp->bd_non_tx_lock));

	switch (bdp->non_tx_command_state) {
	case E100_WAIT_TX_FINISH:
		if (bdp->last_tcb != NULL) {
			rmb();
			if ((bdp->last_tcb->tcb_hdr.cb_status &
			     __constant_cpu_to_le16(CB_STATUS_COMPLETE)) == 0)
				goto exit;
		}
		if ((readw(&bdp->scb->scb_status) & SCB_CUS_MASK) ==
		    SCB_CUS_ACTIVE) {
			goto exit;
		}
		break;

	case E100_WAIT_NON_TX_FINISH:
		active_command = list_entry(bdp->non_tx_cmd_list.next,
					    nxmit_cb_entry_t, list_elem);
		rmb();

		if (((((cb_header_t *) (active_command->non_tx_cmd))->cb_status
		      & __constant_cpu_to_le16(CB_STATUS_COMPLETE)) == 0)
		    && time_before(jiffies, active_command->expiration_time)) {
			goto exit;
		} else {
			non_tx_cmd = (cb_header_t *) active_command->non_tx_cmd;
			sub_cmd = CB_CMD_MASK & le16_to_cpu(non_tx_cmd->cb_cmd);
#ifdef E100_CU_DEBUG			
			if (!(non_tx_cmd->cb_status 
			    & __constant_cpu_to_le16(CB_STATUS_COMPLETE)))
				printk(KERN_ERR "e100: %s: Queued "
					"command (%x) timeout\n", 
					bdp->device->name, sub_cmd);
#endif			
			list_del(&(active_command->list_elem));
			e100_free_non_tx_cmd(bdp, active_command);
		}
		break;

	default:
		break;
	}			//switch

	if (list_empty(&bdp->non_tx_cmd_list)) {
		bdp->non_tx_command_state = E100_NON_TX_IDLE;
		spin_lock_irq(&(bdp->bd_lock));
		bdp->next_cu_cmd = START_WAIT;
		spin_unlock_irq(&(bdp->bd_lock));
		restart = false;
		goto exit;
	} else {
		u16 cmd_type;

		bdp->non_tx_command_state = E100_WAIT_NON_TX_FINISH;
		active_command = list_entry(bdp->non_tx_cmd_list.next,
					    nxmit_cb_entry_t, list_elem);
		sub_cmd = ((cb_header_t *) active_command->non_tx_cmd)->cb_cmd;
		spin_lock_irq(&(bdp->bd_lock));
		e100_wait_exec_cmplx(bdp, active_command->dma_addr,
				     SCB_CUC_START, sub_cmd);
		spin_unlock_irq(&(bdp->bd_lock));
		active_command->expiration_time = jiffies + HZ;
		cmd_type = CB_CMD_MASK &
			le16_to_cpu(((cb_header_t *)
				     (active_command->non_tx_cmd))->cb_cmd);
		bdp->same_cmd_entry[cmd_type] = NULL;
	}

exit:
	if (restart) {
		mod_timer(&(bdp->nontx_timer_id), jiffies + 1);
	} else {
		if (netif_running(bdp->device))
			netif_wake_queue(bdp->device);
	}
	spin_unlock_bh(&(bdp->bd_non_tx_lock));
}

static void
e100_vlan_rx_register(struct net_device *netdev, struct vlan_group *grp)
{
	struct e100_private *bdp = netdev->priv;

	e100_disable_clear_intr(bdp);
	bdp->vlgrp = grp;

	if(grp) {
		/* enable VLAN tag insert/strip */
		e100_config_vlan_drop(bdp, true);

	} else {
		/* disable VLAN tag insert/strip */
		e100_config_vlan_drop(bdp, false);
	}

	e100_config(bdp);
	e100_set_intr_mask(bdp);
}

static void
e100_vlan_rx_add_vid(struct net_device *netdev, u16 vid)
{
	/* We don't do Vlan filtering */
	return;
}

static void
e100_vlan_rx_kill_vid(struct net_device *netdev, u16 vid)
{
	struct e100_private *bdp = netdev->priv;

	if(bdp->vlgrp)
		bdp->vlgrp->vlan_devices[vid] = NULL;
	/* We don't do Vlan filtering */
	return;
}

#ifdef CONFIG_PM
static int
e100_notify_reboot(struct notifier_block *nb, unsigned long event, void *p)
{
        struct pci_dev *pdev = NULL;
	
        switch(event) {
        case SYS_DOWN:
        case SYS_HALT:
        case SYS_POWER_OFF:
		while ((pdev = pci_find_device(PCI_ANY_ID, PCI_ANY_ID, pdev)) != NULL) {
                        if(pci_dev_driver(pdev) == &e100_driver) {
				/* If net_device struct is allocated? */
                                if (pci_get_drvdata(pdev))
					e100_suspend(pdev, 3);

			}
		}
        }
        return NOTIFY_DONE;
}

static int
e100_suspend(struct pci_dev *pcid, u32 state)
{
	struct net_device *netdev = pci_get_drvdata(pcid);
	struct e100_private *bdp = netdev->priv;

	e100_isolate_driver(bdp);
	pci_save_state(pcid, bdp->pci_state);

	/* Enable or disable WoL */
	e100_do_wol(pcid, bdp);
	
	/* If wol is enabled */
	if (bdp->wolopts || e100_asf_enabled(bdp)) {
		pci_enable_wake(pcid, 3, 1);	/* Enable PME for power state D3 */
		pci_set_power_state(pcid, 3);	/* Set power state to D3.        */
	} else {
		/* Disable bus mastering */
		pci_disable_device(pcid);
		pci_set_power_state(pcid, state);
	}
	return 0;
}

static int
e100_resume(struct pci_dev *pcid)
{
	struct net_device *netdev = pci_get_drvdata(pcid);
	struct e100_private *bdp = netdev->priv;

	pci_set_power_state(pcid, 0);
	pci_enable_wake(pcid, 0, 0);	/* Clear PME status and disable PME */
	pci_restore_state(pcid, bdp->pci_state);

	/* Also do device full reset because device was in D3 state */
	e100_deisolate_driver(bdp, true);

	return 0;
}

/**
 * e100_asf_enabled - checks if ASF is configured on the current adaper
 *                    by reading registers 0xD and 0x90 in the EEPROM 
 * @bdp: atapter's private data struct
 *
 * Returns: true if ASF is enabled
 */
static unsigned char
e100_asf_enabled(struct e100_private *bdp)
{
	u16 asf_reg;
	u16 smbus_addr_reg;
	if ((bdp->pdev->device >= 0x1050) && (bdp->pdev->device <= 0x1055)) {
		asf_reg = e100_eeprom_read(bdp, EEPROM_CONFIG_ASF);
		if ((asf_reg & EEPROM_FLAG_ASF)
		    && !(asf_reg & EEPROM_FLAG_GCL)) {
			smbus_addr_reg = 
				e100_eeprom_read(bdp, EEPROM_SMBUS_ADDR);
			if ((smbus_addr_reg & 0xFF) != 0xFE) 
				return true;
		}
	}
	return false;
}
#endif /* CONFIG_PM */

#ifdef E100_CU_DEBUG
unsigned char
e100_cu_unknown_state(struct e100_private *bdp)
{
	u8 scb_cmd_low;
	u16 scb_status;
	scb_cmd_low = bdp->scb->scb_cmd_low;
	scb_status = le16_to_cpu(bdp->scb->scb_status);
	/* If CU is active and executing unknown cmd */
	if (scb_status & SCB_CUS_ACTIVE && scb_cmd_low & SCB_CUC_UNKNOWN)
		return true;
	else
		return false;
}
#endif

#ifdef CONFIG_NET_POLL_CONTROLLER
/*
 * Polling 'interrupt' - used by things like netconsole to send skbs
 * without having to re-enable interrupts. It's not called while
 * the interrupt routine is executing.
 */

static void e100_netpoll (struct net_device *dev)
{
	struct e100_private *adapter = dev->priv;
	disable_irq(adapter->pdev->irq);
	e100intr (adapter->pdev->irq, dev, NULL);
	enable_irq(adapter->pdev->irq);
}
#endif
