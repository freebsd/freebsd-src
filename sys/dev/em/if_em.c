/**************************************************************************

Copyright (c) 2001-2002 Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms of the Software, with or
without modification, are permitted provided that the following conditions
are met:

 1. Redistributions of source code of the Software may retain the above
    copyright notice, this list of conditions and the following disclaimer.

 2. Redistributions in binary form of the Software may reproduce the above
    copyright notice, this list of conditions and the following disclaimer
    in the documentation and/or other materials provided with the
    distribution.

 3. Neither the name of the Intel Corporation nor the names of its
    contributors shall be used to endorse or promote products derived from
    this Software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR ITS CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.

***************************************************************************/

/*$FreeBSD$*/

#include <dev/em/if_em.h>

/*********************************************************************
 *  Set this to one to display debug statistics                                                   
 *********************************************************************/
int             em_display_debug_stats = 0;

/*********************************************************************
 *  Linked list of board private structures for all NICs found
 *********************************************************************/

struct adapter *em_adapter_list = NULL;


/*********************************************************************
 *  Driver version
 *********************************************************************/

char em_driver_version[] = "1.2.7";


/*********************************************************************
 *  PCI Device ID Table
 *
 *  Used by probe to select devices to load on
 *  Last field stores an index into em_strings
 *  Last entry must be all 0s
 *
 *  { Vendor ID, Device ID, SubVendor ID, SubDevice ID, String Index }
 *********************************************************************/
static em_vendor_info_t em_vendor_info_array[] =
{
   /* Intel(R) PRO/1000 Network Connection */
    { 0x8086, 0x1000, PCI_ANY_ID, PCI_ANY_ID, 0 },
    { 0x8086, 0x1001, PCI_ANY_ID, PCI_ANY_ID, 0 },
    { 0x8086, 0x1004, PCI_ANY_ID, PCI_ANY_ID, 0 },
    { 0x8086, 0x1008, PCI_ANY_ID, PCI_ANY_ID, 0 },
    { 0x8086, 0x1009, PCI_ANY_ID, PCI_ANY_ID, 0 },
    { 0x8086, 0x100C, PCI_ANY_ID, PCI_ANY_ID, 0 },
    { 0x8086, 0x100D, PCI_ANY_ID, PCI_ANY_ID, 0 },
    { 0x8086, 0x100E, PCI_ANY_ID, PCI_ANY_ID, 0 },
    /* required last entry */
    { 0, 0, 0, 0, 0}
};


/*********************************************************************
 *  Table of branding strings for all supported NICs.
 *********************************************************************/

static char *em_strings[] = {
    "Intel(R) PRO/1000 Network Connection"
};

/*********************************************************************
 *  Function prototypes            
 *********************************************************************/
static int em_probe     __P((device_t));
static int em_attach        __P((device_t));
static int em_detach        __P((device_t));
static int em_shutdown        __P((device_t));
static void em_intr __P((void *));
static void em_start __P((struct ifnet *));
static int em_ioctl __P((struct ifnet *, IOCTL_CMD_TYPE, caddr_t));
static void em_watchdog __P((struct ifnet *));
static void em_init __P((void *));
static void em_stop __P((void *));
static void em_media_status __P((struct ifnet *, struct ifmediareq *));
static int em_media_change __P((struct ifnet *));
static void em_identify_hardware __P((struct adapter *));
static int em_allocate_pci_resources __P((struct adapter *));
static void em_free_pci_resources __P((struct adapter *));
static void em_local_timer __P((void *));
static int em_hardware_init __P((struct adapter *));
static void em_read_mac_address __P((struct adapter *, u_int8_t *));
static void em_setup_interface __P((device_t, struct adapter *));
static int em_setup_transmit_structures __P((struct adapter *));
static void em_initialize_transmit_unit __P((struct adapter *));
static int em_setup_receive_structures __P((struct adapter *));
static void em_initialize_receive_unit __P((struct adapter *));
static void em_enable_intr __P((struct adapter *));
static void em_disable_intr __P((struct adapter *));
static void em_free_transmit_structures __P((struct adapter *));
static void em_free_receive_structures __P((struct adapter *));
static void em_update_stats_counters __P((struct adapter *));
static void em_clean_transmit_interrupts __P((struct adapter *));
static int em_allocate_receive_structures __P((struct adapter *));
static int em_allocate_transmit_structures __P((struct adapter *));
static void em_process_receive_interrupts __P((struct adapter *));
static void em_receive_checksum __P((struct adapter *, 
                                     struct em_rx_desc * rx_desc,
                                     struct mbuf *));
static void em_transmit_checksum_setup __P((struct adapter *,
                                            struct mbuf *,
                                            struct em_tx_buffer *,
                                            u_int32_t *,
                                            u_int32_t *));
static void em_set_promisc __P((struct adapter *));
static void em_disable_promisc __P((struct adapter *));
static void em_set_multi __P((struct adapter *));
static void em_print_hw_stats __P((struct adapter *));
static void em_print_link_status __P((struct adapter *));
static int em_get_buf __P((struct em_rx_buffer *, struct adapter *,
                           struct mbuf *));
/*********************************************************************
 *  FreeBSD Device Interface Entry Points                    
 *********************************************************************/

static device_method_t em_methods[] = {
   /* Device interface */
   DEVMETHOD(device_probe, em_probe),
   DEVMETHOD(device_attach, em_attach),
   DEVMETHOD(device_detach, em_detach),
   DEVMETHOD(device_shutdown, em_shutdown),
   {0, 0}
};

static driver_t em_driver = {
   "em", em_methods, sizeof(struct adapter ),
};

static devclass_t em_devclass;
DRIVER_MODULE(if_em, pci, em_driver, em_devclass, 0, 0);

/*********************************************************************
 *  Device identification routine
 *
 *  em_probe determines if the driver should be loaded on
 *  adapter based on PCI vendor/device id of the adapter.
 *
 *  return 0 on success, positive on failure
 *********************************************************************/

static int
em_probe(device_t dev)
{
   em_vendor_info_t *ent;

   u_int16_t       pci_vendor_id = 0;
   u_int16_t       pci_device_id = 0;
   u_int16_t       pci_subvendor_id = 0;
   u_int16_t       pci_subdevice_id = 0;
   char            adapter_name[60];

   INIT_DEBUGOUT("em_probe: begin");

   pci_vendor_id = pci_get_vendor(dev);
   if (pci_vendor_id != EM_VENDOR_ID)
      return (ENXIO);

   pci_device_id = pci_get_device(dev);
   pci_subvendor_id = pci_get_subvendor(dev);
   pci_subdevice_id = pci_get_subdevice(dev);

   ent = em_vendor_info_array;
   while(ent->vendor_id != 0) {
      if ((pci_vendor_id == ent->vendor_id) &&
          (pci_device_id == ent->device_id) &&
          
          ((pci_subvendor_id == ent->subvendor_id) ||
           (ent->subvendor_id == PCI_ANY_ID)) &&

          ((pci_subdevice_id == ent->subdevice_id) ||
           (ent->subdevice_id == PCI_ANY_ID))) {
         INIT_DEBUGOUT1("em_probe: Found PRO/1000  (pci_device_id=0x%x)",
                        pci_device_id);
         sprintf(adapter_name, "%s, Version - %s", em_strings[ent->index], 
                 em_driver_version);
         device_set_desc_copy(dev, adapter_name);
         return(0);
      }
      ent++;
   }

   return (ENXIO);
}

/*********************************************************************
 *  Device initialization routine
 *
 *  The attach entry point is called when the driver is being loaded.
 *  This routine identifies the type of hardware, allocates all resources 
 *  and initializes the hardware.     
 *  
 *  return 0 on success, positive on failure
 *********************************************************************/

static int
em_attach(device_t dev)
{
   struct adapter * adapter;
   int             s;
   int             tsize, rsize;

   INIT_DEBUGOUT("em_attach: begin");
   s = splimp();

   /* Allocate, clear, and link in our adapter structure */
   if (!(adapter = device_get_softc(dev))) {
      printf("em: adapter structure allocation failed\n");
      splx(s);
      return(ENOMEM);
   }
   bzero(adapter, sizeof(struct adapter ));
   adapter->dev = dev;
   adapter->osdep.dev = dev;
   adapter->unit = device_get_unit(dev);

   if (em_adapter_list != NULL)
      em_adapter_list->prev = adapter;
   adapter->next = em_adapter_list;
   em_adapter_list = adapter;

   callout_handle_init(&adapter->timer_handle);

   /* Determine hardware revision */
   em_identify_hardware(adapter);

   /* Parameters (to be read from user) */
   adapter->num_tx_desc = MAX_TXD;
   adapter->num_rx_desc = MAX_RXD;
   adapter->tx_int_delay = TIDV;
   adapter->rx_int_delay = RIDV;
   adapter->shared.autoneg = DO_AUTO_NEG;
   adapter->shared.wait_autoneg_complete = WAIT_FOR_AUTO_NEG_DEFAULT;
   adapter->shared.autoneg_advertised = AUTONEG_ADV_DEFAULT;
   adapter->shared.tbi_compatibility_en = TRUE;
   adapter->rx_buffer_len = EM_RXBUFFER_2048;
   adapter->rx_checksum = EM_ENABLE_RXCSUM_OFFLOAD;
   
   adapter->shared.fc_high_water = FC_DEFAULT_HI_THRESH;
   adapter->shared.fc_low_water  = FC_DEFAULT_LO_THRESH;
   adapter->shared.fc_pause_time = FC_DEFAULT_TX_TIMER;
   adapter->shared.fc_send_xon   = TRUE;
   adapter->shared.fc = em_fc_full;

   /* Set the max frame size assuming standard ethernet sized frames */   
   adapter->shared.max_frame_size = ETHERMTU + ETHER_HDR_LEN + ETHER_CRC_LEN;
   adapter->shared.min_frame_size = MINIMUM_ETHERNET_PACKET_SIZE + ETHER_CRC_LEN;

   /* This controls when hardware reports transmit completion status. */
   if ((EM_REPORT_TX_EARLY == 0) || (EM_REPORT_TX_EARLY == 1)) {
      adapter->shared.report_tx_early = EM_REPORT_TX_EARLY;
   } else {
      if(adapter->shared.mac_type < em_82543) {
         adapter->shared.report_tx_early = 0;
      } else {
         adapter->shared.report_tx_early = 1;
      }
   }

   if (em_allocate_pci_resources(adapter)) {
      printf("em%d: Allocation of PCI resources failed\n", adapter->unit);
      em_free_pci_resources(adapter);
      splx(s);
      return(ENXIO);
   }

   tsize = EM_ROUNDUP(adapter->num_tx_desc *
                      sizeof(struct em_tx_desc), 4096);

   /* Allocate Transmit Descriptor ring */
   if (!(adapter->tx_desc_base = (struct em_tx_desc *)
         contigmalloc(tsize, M_DEVBUF, M_NOWAIT, 0, ~0, PAGE_SIZE, 0))) {
      printf("em%d: Unable to allocate TxDescriptor memory\n", adapter->unit);
      em_free_pci_resources(adapter);
      splx(s);
      return(ENOMEM);
   }

   rsize = EM_ROUNDUP(adapter->num_rx_desc *
                      sizeof(struct em_rx_desc), 4096);

   /* Allocate Receive Descriptor ring */
   if (!(adapter->rx_desc_base = (struct em_rx_desc *)
        contigmalloc(rsize, M_DEVBUF, M_NOWAIT, 0, ~0, PAGE_SIZE, 0))) {
      printf("em%d: Unable to allocate rx_desc memory\n", adapter->unit);
      em_free_pci_resources(adapter);
      contigfree(adapter->tx_desc_base, tsize, M_DEVBUF);
      splx(s);
      return(ENOMEM);
   }

   /* Initialize the hardware */
   if (em_hardware_init(adapter)) {
      printf("em%d: Unable to initialize the hardware\n",adapter->unit);
      em_free_pci_resources(adapter);
      contigfree(adapter->tx_desc_base, tsize, M_DEVBUF);
      contigfree(adapter->rx_desc_base, rsize, M_DEVBUF);
      splx(s);
      return(EIO);
   }

   /* Setup OS specific network interface */
   em_setup_interface(dev, adapter);

   /* Initialize statistics */
   em_clear_hw_cntrs(&adapter->shared);
   em_update_stats_counters(adapter);
   adapter->shared.get_link_status = 1;
   em_check_for_link(&adapter->shared);

   /* Print the link status */
   if (adapter->link_active == 1) {
      em_get_speed_and_duplex(&adapter->shared, &adapter->link_speed, &adapter->link_duplex);
      printf("em%d:  Speed:%d Mbps  Duplex:%s\n",
             adapter->unit,
             adapter->link_speed,
             adapter->link_duplex == FULL_DUPLEX ? "Full" : "Half");
   }
   else
      printf("em%d:  Speed:N/A  Duplex:N/A\n", adapter->unit);


   INIT_DEBUGOUT("em_attach: end");
   splx(s);
   return(0);
}

/*********************************************************************
 *  Device removal routine
 *
 *  The detach entry point is called when the driver is being removed.
 *  This routine stops the adapter and deallocates all the resources
 *  that were allocated for driver operation.
 *  
 *  return 0 on success, positive on failure
 *********************************************************************/

static int
em_detach(device_t dev)
{
   struct adapter * adapter = device_get_softc(dev);
   struct ifnet   *ifp = &adapter->interface_data.ac_if;
   int             s;
   int             size;

   INIT_DEBUGOUT("em_detach: begin");
   s = splimp();

   em_stop(adapter);
   em_phy_hw_reset(&adapter->shared);
   ether_ifdetach(&adapter->interface_data.ac_if, ETHER_BPF_SUPPORTED);
   em_free_pci_resources(adapter);

   size = EM_ROUNDUP(adapter->num_tx_desc *
                     sizeof(struct em_tx_desc), 4096);

   /* Free Transmit Descriptor ring */
   if (adapter->tx_desc_base) {
      contigfree(adapter->tx_desc_base, size, M_DEVBUF);
      adapter->tx_desc_base = NULL;
   }

   size = EM_ROUNDUP(adapter->num_rx_desc *
                     sizeof(struct em_rx_desc), 4096);

   /* Free Receive Descriptor ring */
   if (adapter->rx_desc_base) {
      contigfree(adapter->rx_desc_base, size, M_DEVBUF);
      adapter->rx_desc_base = NULL;
   }

   /* Remove from the adapter list */
   if(em_adapter_list == adapter)
      em_adapter_list = adapter->next;
   if(adapter->next != NULL)
      adapter->next->prev = adapter->prev;
   if(adapter->prev != NULL)
      adapter->prev->next = adapter->next;

   ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
   ifp->if_timer = 0;

   splx(s);
   return(0);
}

static int
em_shutdown(device_t dev)
{
   struct adapter * adapter = device_get_softc(dev);
   
   /* Issue a global reset */
   em_adapter_stop(&adapter->shared);
   return(0);
}


/*********************************************************************
 *  Transmit entry point
 *
 *  em_start is called by the stack to initiate a transmit.
 *  The driver will remain in this routine as long as there are
 *  packets to transmit and transmit resources are available.
 *  In case resources are not available stack is notified and
 *  the packet is requeued.
 **********************************************************************/

static void
em_start(struct ifnet *ifp)
{
   int             s;
   struct mbuf    *m_head, *mp;
   vm_offset_t     virtual_addr;
   u_int32_t       txd_upper; 
   u_int32_t       txd_lower;
   struct em_tx_buffer   *tx_buffer;
   struct em_tx_desc *current_tx_desc = NULL;
   struct adapter * adapter = ifp->if_softc;

   if (!adapter->link_active)
      return;

   s = splimp();      
   while (ifp->if_snd.ifq_head != NULL) {

      IF_DEQUEUE(&ifp->if_snd, m_head);
      
      if(m_head == NULL) break;

      if (adapter->num_tx_desc_avail <= TX_CLEANUP_THRESHOLD)
         em_clean_transmit_interrupts(adapter);

      if (adapter->num_tx_desc_avail <= TX_CLEANUP_THRESHOLD) {
         ifp->if_flags |= IFF_OACTIVE;
         IF_PREPEND(&ifp->if_snd, m_head);
         adapter->no_tx_desc_avail++;
         break;
      }

      tx_buffer =  STAILQ_FIRST(&adapter->free_tx_buffer_list);
      if (!tx_buffer) {
         adapter->no_tx_buffer_avail1++;
         
         /* 
          * OK so we should not get here but I've seen it so lets try to 
          * clean up and then try to get a SwPacket again and only break 
          * if we still don't get one 
          */
         em_clean_transmit_interrupts(adapter);
         tx_buffer = STAILQ_FIRST(&adapter->free_tx_buffer_list);
         if (!tx_buffer) {
            ifp->if_flags |= IFF_OACTIVE;
            IF_PREPEND(&ifp->if_snd, m_head);
            adapter->no_tx_buffer_avail2++;
            break;
         }
      }
      STAILQ_REMOVE_HEAD(&adapter->free_tx_buffer_list, em_tx_entry);

      tx_buffer->num_tx_desc_used = 0;
      tx_buffer->m_head = m_head;

      if (ifp->if_hwassist > 0) {
         em_transmit_checksum_setup(adapter,  m_head, tx_buffer, &txd_upper, &txd_lower);
      } else {
         txd_upper = 0;
         txd_lower = 0;
      }

      for (mp = m_head; mp != NULL; mp = mp->m_next) {
         if (mp->m_len == 0)
            continue;
         current_tx_desc = adapter->next_avail_tx_desc;
         virtual_addr = mtod(mp, vm_offset_t);
         current_tx_desc->buffer_addr = vtophys(virtual_addr);
 
         current_tx_desc->lower.data = (txd_lower | mp->m_len);
         current_tx_desc->upper.data = (txd_upper);

         if (current_tx_desc == adapter->last_tx_desc)
            adapter->next_avail_tx_desc =
            adapter->first_tx_desc;
         else
            adapter->next_avail_tx_desc++;

         adapter->num_tx_desc_avail--;
         tx_buffer->num_tx_desc_used++;
      }

      /* Put this tx_buffer at the end in the "in use" list */
      STAILQ_INSERT_TAIL(&adapter->used_tx_buffer_list, tx_buffer, em_tx_entry);

      /* 
       * Last Descriptor of Packet needs End Of Packet (EOP), Report Status
       * (RS) and append Ethernet CRC (IFCS) bits set.
       */
      current_tx_desc->lower.data |= (adapter->txd_cmd | E1000_TXD_CMD_EOP);

      /* Send a copy of the frame to the BPF listener */
      if (ifp->if_bpf)
         bpf_mtap(ifp, m_head);
      /* 
       * Advance the Transmit Descriptor Tail (Tdt), this tells the E1000
       * that this frame is available to transmit.
       */
      E1000_WRITE_REG(&adapter->shared, TDT, (((u_int32_t) adapter->next_avail_tx_desc -
                             (u_int32_t) adapter->first_tx_desc) >> 4));
   } /* end of while loop */

   splx(s);

   /* Set timeout in case chip has problems transmitting */
   ifp->if_timer = EM_TX_TIMEOUT;
   
   return;
}

/*********************************************************************
 *  Ioctl entry point
 *
 *  em_ioctl is called when the user wants to configure the
 *  interface.
 *
 *  return 0 on success, positive on failure
 **********************************************************************/

static int
em_ioctl(struct ifnet *ifp, IOCTL_CMD_TYPE command, caddr_t data)
{
   int             s, mask, error = 0;
   struct ifreq   *ifr = (struct ifreq *) data;
   struct adapter * adapter = ifp->if_softc;

   s = splimp();
   switch (command) {
   case SIOCSIFADDR:
   case SIOCGIFADDR:
      IOCTL_DEBUGOUT("ioctl rcv'd: SIOCxIFADDR (Get/Set Interface Addr)");
      ether_ioctl(ifp, command, data);
      break;
   case SIOCSIFMTU:
      IOCTL_DEBUGOUT("ioctl rcv'd: SIOCSIFMTU (Set Interface MTU)");
      if (ifr->ifr_mtu > MAX_JUMBO_FRAME_SIZE - ETHER_HDR_LEN) {
         error = EINVAL;
      } else {
         ifp->if_mtu = ifr->ifr_mtu;
         adapter->shared.max_frame_size = ifp->if_mtu + ETHER_HDR_LEN + ETHER_CRC_LEN;
         em_init(adapter);
      }
      break;
   case SIOCSIFFLAGS:
      IOCTL_DEBUGOUT("ioctl rcv'd: SIOCSIFFLAGS (Set Interface Flags)");
      if (ifp->if_flags & IFF_UP) {
         if (ifp->if_flags & IFF_RUNNING &&
             ifp->if_flags & IFF_PROMISC) {
            em_set_promisc(adapter);
         } else if (ifp->if_flags & IFF_RUNNING &&
                    !(ifp->if_flags & IFF_PROMISC)) {
            em_disable_promisc(adapter);
         } else
            em_init(adapter);
      } else {
         if (ifp->if_flags & IFF_RUNNING) {
            em_stop(adapter);
         }
      }
      break;
   case SIOCADDMULTI:
   case SIOCDELMULTI:
      IOCTL_DEBUGOUT("ioctl rcv'd: SIOC(ADD|DEL)MULTI");
      if (ifp->if_flags & IFF_RUNNING) {
         em_disable_intr(adapter);
         em_set_multi(adapter);
         if(adapter->shared.mac_type == em_82542_rev2_0)
            em_initialize_receive_unit(adapter);
         em_enable_intr(adapter);
      }
      break;
   case SIOCSIFMEDIA:
   case SIOCGIFMEDIA:
      IOCTL_DEBUGOUT("ioctl rcv'd: SIOCxIFMEDIA (Get/Set Interface Media)");
      error = ifmedia_ioctl(ifp, ifr, &adapter->media, command);
      break;
   case SIOCSIFCAP:
      IOCTL_DEBUGOUT("ioctl rcv'd: SIOCSIFCAP (Set Capabilities)");
      mask = ifr->ifr_reqcap ^ ifp->if_capenable;
      if (mask & IFCAP_HWCSUM) {
         if (IFCAP_HWCSUM & ifp->if_capenable)
            ifp->if_capenable &= ~IFCAP_HWCSUM;
         else
            ifp->if_capenable |= IFCAP_HWCSUM;
         if (ifp->if_flags & IFF_RUNNING)
            em_init(adapter);
      }
      break;
   default:
      IOCTL_DEBUGOUT1("ioctl received: UNKNOWN (0x%d)\n", (int)command);
      error = EINVAL;
   }

   splx(s);
   return(error);
}

static void
em_set_promisc(struct adapter * adapter)
{

   u_int32_t       reg_rctl;
   struct ifnet   *ifp = &adapter->interface_data.ac_if;

   reg_rctl = E1000_READ_REG(&adapter->shared, RCTL);

   if(ifp->if_flags & IFF_PROMISC) {
      reg_rctl |= (E1000_RCTL_UPE | E1000_RCTL_MPE);
      E1000_WRITE_REG(&adapter->shared, RCTL, reg_rctl);
   }
   else if (ifp->if_flags & IFF_ALLMULTI) {
      reg_rctl |= E1000_RCTL_MPE;
      reg_rctl &= ~E1000_RCTL_UPE;
      E1000_WRITE_REG(&adapter->shared, RCTL, reg_rctl);
   }

   return;
}

static void
em_disable_promisc(struct adapter * adapter)
{
   u_int32_t       reg_rctl;

   reg_rctl = E1000_READ_REG(&adapter->shared, RCTL);

   reg_rctl &=  (~E1000_RCTL_UPE);
   reg_rctl &=  (~E1000_RCTL_MPE);
   E1000_WRITE_REG(&adapter->shared, RCTL, reg_rctl);

   return;
}


/*********************************************************************
 *  Multicast Update
 *
 *  This routine is called whenever multicast address list is updated.
 *
 **********************************************************************/

static void
em_set_multi(struct adapter * adapter)
{
   u_int32_t reg_rctl = 0;
   u_int8_t  mta[MAX_NUM_MULTICAST_ADDRESSES * ETH_LENGTH_OF_ADDRESS];
   u_int16_t pci_cmd_word;
   struct ifmultiaddr  *ifma;
   int mcnt = 0;
   struct ifnet   *ifp = &adapter->interface_data.ac_if;

   IOCTL_DEBUGOUT("em_set_multi: begin");
   
    if(adapter->shared.mac_type == em_82542_rev2_0) {
       reg_rctl = E1000_READ_REG(&adapter->shared, RCTL);
       if(adapter->shared.pci_cmd_word & CMD_MEM_WRT_INVALIDATE) {
          pci_cmd_word = adapter->shared.pci_cmd_word & ~CMD_MEM_WRT_INVALIDATE;
          pci_write_config(adapter->dev, PCIR_COMMAND, pci_cmd_word, 2);
       }
       reg_rctl |= E1000_RCTL_RST;
       E1000_WRITE_REG(&adapter->shared, RCTL, reg_rctl);
       msec_delay(5);
    }

#if __FreeBSD_version < 500000 
    LIST_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
#else
    TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
#endif
       if (ifma->ifma_addr->sa_family != AF_LINK)
          continue;
       
       bcopy(LLADDR((struct sockaddr_dl *)ifma->ifma_addr),
             &mta[mcnt*ETH_LENGTH_OF_ADDRESS], ETH_LENGTH_OF_ADDRESS);
       mcnt++;
    }

    if (mcnt > MAX_NUM_MULTICAST_ADDRESSES) {
       reg_rctl = E1000_READ_REG(&adapter->shared, RCTL);
       reg_rctl |= E1000_RCTL_MPE;
       E1000_WRITE_REG(&adapter->shared, RCTL, reg_rctl);
    }
    else
       em_mc_addr_list_update(&adapter->shared, mta, mcnt, 0);

    if(adapter->shared.mac_type == em_82542_rev2_0) {
       reg_rctl = E1000_READ_REG(&adapter->shared, RCTL);
       reg_rctl &= ~E1000_RCTL_RST;
       E1000_WRITE_REG(&adapter->shared, RCTL, reg_rctl);
       msec_delay(5);
       if(adapter->shared.pci_cmd_word & CMD_MEM_WRT_INVALIDATE) {
          pci_write_config(adapter->dev, PCIR_COMMAND, adapter->shared.pci_cmd_word, 2);
       }
    }

    return;
}

/*********************************************************************
 *  Watchdog entry point
 *
 *  This routine is called whenever hardware quits transmitting.
 *
 **********************************************************************/

static void
em_watchdog(struct ifnet *ifp)
{
   struct adapter * adapter;
   adapter = ifp->if_softc;
 
   /* If we are in this routine because of pause frames, then
    * don't reset the hardware.
    */
   if(E1000_READ_REG(&adapter->shared, STATUS) & E1000_STATUS_TXOFF) {
      ifp->if_timer = EM_TX_TIMEOUT;
      return;
   }

   printf("em%d: watchdog timeout -- resetting\n", adapter->unit);

   ifp->if_flags &= ~IFF_RUNNING;

   em_stop(adapter);
   em_init(adapter);

   ifp->if_oerrors++;
   return;
}

/*********************************************************************
 *  Timer routine
 *
 *  This routine checks for link status and updates statistics.
 *
 **********************************************************************/

static void
em_local_timer(void *arg)
{
   int s;
   struct ifnet   *ifp;
   struct adapter * adapter = arg;
   ifp = &adapter->interface_data.ac_if;

   s = splimp();

   em_check_for_link(&adapter->shared);
   em_print_link_status(adapter);
   em_update_stats_counters(adapter);   
   if(em_display_debug_stats && ifp->if_flags & IFF_RUNNING) {
      em_print_hw_stats(adapter);
   }
   adapter->timer_handle = timeout(em_local_timer, adapter, 2*hz);

   splx(s);
   return;
}

static void
em_print_link_status(struct adapter * adapter)
{
   if(E1000_READ_REG(&adapter->shared, STATUS) & E1000_STATUS_LU) {
      if(adapter->link_active == 0) {
         em_get_speed_and_duplex(&adapter->shared, &adapter->link_speed, &adapter->link_duplex);
         printf("em%d: Link is up %d Mbps %s\n",
                adapter->unit,
                adapter->link_speed,
                ((adapter->link_duplex == FULL_DUPLEX) ?
                 "Full Duplex" : "Half Duplex"));
         adapter->link_active = 1;
      }
   } else {
      if(adapter->link_active == 1) {
         adapter->link_speed = 0;
         adapter->link_duplex = 0;
         printf("em%d: Link is Down\n", adapter->unit);
         adapter->link_active = 0;
      }
   }

   return;
}

/*********************************************************************
 *  Init entry point
 *
 *  This routine is used in two ways. It is used by the stack as
 *  init entry point in network interface structure. It is also used
 *  by the driver as a hw/sw initialization routine to get to a 
 *  consistent state.
 *
 *  return 0 on success, positive on failure
 **********************************************************************/

static void
em_init(void *arg)
{
   int             s;
   struct ifnet   *ifp;
   struct adapter * adapter = arg;

   INIT_DEBUGOUT("em_init: begin");

   s = splimp();

   em_stop(adapter);

   /* Initialize the hardware */
   if (em_hardware_init(adapter)) {
      printf("em%d: Unable to initialize the hardware\n", adapter->unit);
      splx(s);
      return;
   }
   adapter->shared.adapter_stopped = FALSE;
   
   /* Prepare transmit descriptors and buffers */
   if (em_setup_transmit_structures(adapter)) {
      printf("em%d: Could not setup transmit structures\n", adapter->unit);
      em_stop(adapter); 
      splx(s);
      return;
   }
   em_initialize_transmit_unit(adapter);

   /* Setup Multicast table */
   em_set_multi(adapter);

   /* Prepare receive descriptors and buffers */
   if (em_setup_receive_structures(adapter)) {
      printf("em%d: Could not setup receive structures\n", adapter->unit);
      em_stop(adapter);
      splx(s);
      return;
   }
   em_initialize_receive_unit(adapter);

   ifp = &adapter->interface_data.ac_if;
   ifp->if_flags |= IFF_RUNNING;
   ifp->if_flags &= ~IFF_OACTIVE;

   if(adapter->shared.mac_type >= em_82543) {
      if (ifp->if_capenable & IFCAP_TXCSUM)
         ifp->if_hwassist = EM_CHECKSUM_FEATURES;
   }

   adapter->timer_handle = timeout(em_local_timer, adapter, 2*hz);
   em_clear_hw_cntrs(&adapter->shared);
   em_enable_intr(adapter);

   splx(s);
   return;
}


/*********************************************************************
 *
 *  This routine disables all traffic on the adapter by issuing a
 *  global reset on the MAC and deallocates TX/RX buffers. 
 *
 **********************************************************************/

static void
em_stop(void *arg)
{
   struct ifnet   *ifp;
   struct adapter * adapter = arg;
   ifp = &adapter->interface_data.ac_if;

   INIT_DEBUGOUT("em_stop: begin\n");
   em_disable_intr(adapter);
   em_adapter_stop(&adapter->shared);
   untimeout(em_local_timer, adapter, adapter->timer_handle);
   em_free_transmit_structures(adapter);
   em_free_receive_structures(adapter);


   /* Tell the stack that the interface is no longer active */
   ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

   return;
}

/*********************************************************************
 *
 *  Interrupt Service routine
 *
 **********************************************************************/

static void
em_intr(void *arg)
{
   u_int32_t       loop_cnt = EM_MAX_INTR;
   u_int32_t       reg_icr;
   struct ifnet    *ifp;
   struct adapter  *adapter = arg;

   ifp = &adapter->interface_data.ac_if;

   em_disable_intr(adapter);
   while(loop_cnt > 0 && (reg_icr = E1000_READ_REG(&adapter->shared, ICR)) != 0) {

      /* Link status change */
      if(reg_icr & (E1000_ICR_RXSEQ | E1000_ICR_LSC)) {
         untimeout(em_local_timer, adapter, adapter->timer_handle);
         adapter->shared.get_link_status = 1;
         em_check_for_link(&adapter->shared);
         em_print_link_status(adapter);
         adapter->timer_handle = timeout(em_local_timer, adapter, 2*hz);      
      }

      if (ifp->if_flags & IFF_RUNNING) {
         em_process_receive_interrupts(adapter);
         em_clean_transmit_interrupts(adapter);
      }
      loop_cnt--;
   }

   em_enable_intr(adapter);

   if(ifp->if_flags & IFF_RUNNING && ifp->if_snd.ifq_head != NULL)
      em_start(ifp);
   
   return;
}


/*********************************************************************
 *
 *  Media Ioctl callback
 *
 *  This routine is called whenever the user queries the status of
 *  the interface using ifconfig.
 *
 **********************************************************************/
static void
em_media_status(struct ifnet *ifp, struct ifmediareq *ifmr)
{
   struct adapter * adapter = ifp->if_softc;

   INIT_DEBUGOUT("em_media_status: begin");

   em_check_for_link(&adapter->shared);
   if(E1000_READ_REG(&adapter->shared, STATUS) & E1000_STATUS_LU) {
      if(adapter->link_active == 0) {
         em_get_speed_and_duplex(&adapter->shared, &adapter->link_speed, &adapter->link_duplex);
         adapter->link_active = 1;
      }
   }
   else {
      if(adapter->link_active == 1) {
         adapter->link_speed = 0;
         adapter->link_duplex = 0;
         adapter->link_active = 0;
      }
   }

   ifmr->ifm_status = IFM_AVALID;
   ifmr->ifm_active = IFM_ETHER;

   if (!adapter->link_active)
      return;

   ifmr->ifm_status |= IFM_ACTIVE;

   if (adapter->shared.media_type == em_media_type_fiber) {
      ifmr->ifm_active |= IFM_1000_SX | IFM_FDX;
   } else {
      switch (adapter->link_speed) {
      case 10:
         ifmr->ifm_active |= IFM_10_T;
         break;
      case 100:
         ifmr->ifm_active |= IFM_100_TX;
         break;
      case 1000:
         ifmr->ifm_active |= IFM_1000_TX;
         break;
      }
      if (adapter->link_duplex == FULL_DUPLEX)
         ifmr->ifm_active |= IFM_FDX;
      else
         ifmr->ifm_active |= IFM_HDX;
   }
   return;
}

/*********************************************************************
 *
 *  Media Ioctl callback
 *
 *  This routine is called when the user changes speed/duplex using
 *  media/mediopt option with ifconfig.
 *
 **********************************************************************/
static int
em_media_change(struct ifnet *ifp)
{
   struct adapter * adapter = ifp->if_softc;
   struct ifmedia  *ifm = &adapter->media;
   
   INIT_DEBUGOUT("em_media_change: begin");

   if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
      return(EINVAL);

   switch(IFM_SUBTYPE(ifm->ifm_media)) {
   case IFM_AUTO:
      if (adapter->shared.autoneg)
         return 0;
      else {
         adapter->shared.autoneg = DO_AUTO_NEG;
         adapter->shared.autoneg_advertised = AUTONEG_ADV_DEFAULT;
      }
      break;
   case IFM_1000_SX:
   case IFM_1000_TX:
      adapter->shared.autoneg = DO_AUTO_NEG;
      adapter->shared.autoneg_advertised = ADVERTISE_1000_FULL;
      break;
   case IFM_100_TX:
      adapter->shared.autoneg = FALSE;
      adapter->shared.autoneg_advertised = 0;
      if ((ifm->ifm_media & IFM_GMASK) == IFM_FDX)
         adapter->shared.forced_speed_duplex = em_100_full;
      else
         adapter->shared.forced_speed_duplex = em_100_half;
      break;
   case IFM_10_T:
     adapter->shared.autoneg = FALSE;
     adapter->shared.autoneg_advertised = 0;
     if ((ifm->ifm_media & IFM_GMASK) == IFM_FDX)
        adapter->shared.forced_speed_duplex = em_10_full;
     else
        adapter->shared.forced_speed_duplex = em_10_half;
     break;
   default:
      printf("em%d: Unsupported media type\n", adapter->unit);
   }

   em_init(adapter);

   return(0);
}
/* Section end: Other registered entry points */


/*********************************************************************
 *
 *  Determine hardware revision.
 *
 **********************************************************************/
static void
em_identify_hardware(struct adapter * adapter)
{
   device_t dev = adapter->dev;

   /* Make sure our PCI config space has the necessary stuff set */
   adapter->shared.pci_cmd_word = pci_read_config(dev, PCIR_COMMAND, 2);
   if (!((adapter->shared.pci_cmd_word & PCIM_CMD_BUSMASTEREN) &&
         (adapter->shared.pci_cmd_word & PCIM_CMD_MEMEN))) {
      printf("em%d: Memory Access and/or Bus Master bits were not set!\n", 
             adapter->unit);
      adapter->shared.pci_cmd_word |= (PCIM_CMD_BUSMASTEREN | PCIM_CMD_MEMEN);
      pci_write_config(dev, PCIR_COMMAND, adapter->shared.pci_cmd_word, 2);
   }

   /* Save off the information about this board */
   adapter->shared.vendor_id = pci_get_vendor(dev);
   adapter->shared.device_id = pci_get_device(dev);
   adapter->shared.revision_id = pci_read_config(dev, PCIR_REVID, 1);
   adapter->shared.subsystem_vendor_id = pci_read_config(dev, PCIR_SUBVEND_0, 2);
   adapter->shared.subsystem_id = pci_read_config(dev, PCIR_SUBDEV_0, 2);

   
   /* Set MacType, etc. based on this PCI info */
   switch (adapter->shared.device_id) {
   case E1000_DEV_ID_82542:
      adapter->shared.mac_type = (adapter->shared.revision_id == 3) ?
         em_82542_rev2_1 : em_82542_rev2_0;
      break;
   case E1000_DEV_ID_82543GC_FIBER:
   case E1000_DEV_ID_82543GC_COPPER:
      adapter->shared.mac_type = em_82543;
      break;
   case E1000_DEV_ID_82544EI_FIBER:
   case E1000_DEV_ID_82544EI_COPPER:
   case E1000_DEV_ID_82544GC_COPPER:
   case E1000_DEV_ID_82544GC_LOM:
      adapter->shared.mac_type = em_82544;
      break;
   case E1000_DEV_ID_82540EM:
      adapter->shared.mac_type = em_82540;
   default:
      INIT_DEBUGOUT1("Unknown device id 0x%x", adapter->shared.device_id);
   }
   return;
}

static int
em_allocate_pci_resources(struct adapter * adapter)
{
   int             resource_id = EM_MMBA;
   device_t        dev = adapter->dev;

   adapter->res_memory = bus_alloc_resource(dev, SYS_RES_MEMORY,
                                            &resource_id, 0, ~0, 1,
                                            RF_ACTIVE);
   if (!(adapter->res_memory)) {
      printf("em%d: Unable to allocate bus resource: memory\n", adapter->unit);
      return(ENXIO);
   }
   adapter->osdep.bus_space_tag = rman_get_bustag(adapter->res_memory);
   adapter->osdep.bus_space_handle = rman_get_bushandle(adapter->res_memory);
   adapter->shared.hw_addr = (uint8_t *)adapter->osdep.bus_space_handle;

   resource_id = 0x0;
   adapter->res_interrupt = bus_alloc_resource(dev, SYS_RES_IRQ,
                                               &resource_id, 0, ~0, 1,
                                               RF_SHAREABLE | RF_ACTIVE);
   if (!(adapter->res_interrupt)) {
      printf("em%d: Unable to allocate bus resource: interrupt\n", adapter->unit);
      return(ENXIO);
   }
   if (bus_setup_intr(dev, adapter->res_interrupt, INTR_TYPE_NET,
                  (void (*)(void *)) em_intr, adapter,
                  &adapter->int_handler_tag)) {
      printf("em%d: Error registering interrupt handler!\n", adapter->unit);
      return(ENXIO);
   }

   adapter->shared.back = &adapter->osdep;

   return(0);
}

static void
em_free_pci_resources(struct adapter * adapter)
{
   device_t dev = adapter->dev;

   if(adapter->res_interrupt != NULL) {
      bus_teardown_intr(dev, adapter->res_interrupt, adapter->int_handler_tag);
      bus_release_resource(dev, SYS_RES_IRQ, 0, adapter->res_interrupt);
   }
   if (adapter->res_memory != NULL) {
      bus_release_resource(dev, SYS_RES_MEMORY, EM_MMBA, adapter->res_memory);
   }
   return;
}

/*********************************************************************
 *
 *  Initialize the hardware to a configuration as specified by the
 *  adapter structure. The controller is reset, the EEPROM is
 *  verified, the MAC address is set, then the shared initialization
 *  routines are called.
 *
 **********************************************************************/
static int
em_hardware_init(struct adapter * adapter)
{
   /* Issue a global reset */
   adapter->shared.adapter_stopped = FALSE;
   em_adapter_stop(&adapter->shared);
   adapter->shared.adapter_stopped = FALSE;

   /* Make sure we have a good EEPROM before we read from it */
   if (!em_validate_eeprom_checksum(&adapter->shared)) {
      printf("em%d: The EEPROM Checksum Is Not Valid\n", adapter->unit);
      return EIO;
   }
   /* Copy the permanent MAC address and part number out of the EEPROM */
   em_read_mac_address(adapter, adapter->interface_data.ac_enaddr);
   memcpy(adapter->shared.mac_addr, adapter->interface_data.ac_enaddr,
         ETH_LENGTH_OF_ADDRESS);
   em_read_part_num(&adapter->shared, &(adapter->part_num));

   if (!em_init_hw(&adapter->shared)) {
      printf("em%d: Hardware Initialization Failed", adapter->unit);
      return EIO;
   }

   em_check_for_link(&adapter->shared);
   if (E1000_READ_REG(&adapter->shared, STATUS) & E1000_STATUS_LU)
      adapter->link_active = 1;
   else
      adapter->link_active = 0;
 
   if (adapter->link_active) {
      em_get_speed_and_duplex(&adapter->shared, &adapter->link_speed, &adapter->link_duplex);
   } else {
      adapter->link_speed = 0;
      adapter->link_duplex = 0;
   }

   return 0;
}

static void
em_read_mac_address(struct adapter * adapter, u_int8_t * NodeAddress)
{
   u_int16_t       EepromWordValue;
   int             i;

   for (i = 0; i < NODE_ADDRESS_SIZE; i += 2) {
      EepromWordValue =
         em_read_eeprom(&adapter->shared, EEPROM_NODE_ADDRESS_BYTE_0 + (i / 2));
      NodeAddress[i] = (uint8_t) (EepromWordValue & 0x00FF);
      NodeAddress[i + 1] = (uint8_t) (EepromWordValue >> 8);
   }

   return;
}

/*********************************************************************
 *
 *  Setup networking device structure and register an interface.
 *
 **********************************************************************/
static void
em_setup_interface(device_t dev, struct adapter * adapter)
{
   struct ifnet   *ifp;
   INIT_DEBUGOUT("em_setup_interface: begin");

   ifp = &adapter->interface_data.ac_if;
   ifp->if_unit = adapter->unit;
   ifp->if_name = "em";
   ifp->if_mtu = ETHERMTU;
   ifp->if_output = ether_output;
   ifp->if_baudrate = 1000000000;
   ifp->if_init =  em_init;
   ifp->if_softc = adapter;
   ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
   ifp->if_ioctl = em_ioctl;
   ifp->if_start = em_start;
   ifp->if_watchdog = em_watchdog;
   ifp->if_snd.ifq_maxlen = adapter->num_tx_desc - 1;
   ether_ifattach(ifp, ETHER_BPF_SUPPORTED);

   if (adapter->shared.mac_type >= em_82543) {
      ifp->if_capabilities = IFCAP_HWCSUM;
      ifp->if_capenable = ifp->if_capabilities;
   }

   /* 
    * Specify the media types supported by this adapter and register
    * callbacks to update media and link information
    */
   ifmedia_init(&adapter->media, IFM_IMASK, em_media_change,
                em_media_status);
   if (adapter->shared.media_type == em_media_type_fiber) {
      ifmedia_add(&adapter->media, IFM_ETHER | IFM_1000_SX | IFM_FDX, 0,
                  NULL);
      ifmedia_add(&adapter->media, IFM_ETHER | IFM_1000_SX , 0, NULL);
   } else {
      ifmedia_add(&adapter->media, IFM_ETHER | IFM_10_T, 0, NULL);
      ifmedia_add(&adapter->media, IFM_ETHER | IFM_10_T | IFM_FDX, 0,
                  NULL);
      ifmedia_add(&adapter->media, IFM_ETHER | IFM_100_TX, 0, NULL);
      ifmedia_add(&adapter->media, IFM_ETHER | IFM_100_TX | IFM_FDX, 0,
                  NULL);
      ifmedia_add(&adapter->media, IFM_ETHER | IFM_1000_TX | IFM_FDX, 0,
                  NULL);
      ifmedia_add(&adapter->media, IFM_ETHER | IFM_1000_TX, 0, NULL);
   }
   ifmedia_add(&adapter->media, IFM_ETHER | IFM_AUTO, 0, NULL);
   ifmedia_set(&adapter->media, IFM_ETHER | IFM_AUTO);

   return;
}


/*********************************************************************
 *
 *  Allocate memory for tx_buffer structures. The tx_buffer stores all 
 *  the information needed to transmit a packet on the wire. 
 *
 **********************************************************************/
static int
em_allocate_transmit_structures(struct adapter * adapter)
{
   if (!(adapter->tx_buffer_area =
         (struct em_tx_buffer *) malloc(sizeof(struct em_tx_buffer) *
                                        adapter->num_tx_desc, M_DEVBUF,
                                        M_NOWAIT))) {
      printf("em%d: Unable to allocate tx_buffer memory\n", adapter->unit);
      return ENOMEM;
   }

   bzero(adapter->tx_buffer_area,
         sizeof(struct em_tx_buffer) * adapter->num_tx_desc);

   return 0;
}

/*********************************************************************
 *
 *  Allocate and initialize transmit structures. 
 *
 **********************************************************************/
static int
em_setup_transmit_structures(struct adapter * adapter)
{
   struct em_tx_buffer   *tx_buffer;
   int             i;

   if (em_allocate_transmit_structures(adapter))
      return ENOMEM;

   adapter->first_tx_desc = adapter->tx_desc_base;
   adapter->last_tx_desc =
      adapter->first_tx_desc + (adapter->num_tx_desc - 1);

   
   STAILQ_INIT(&adapter->free_tx_buffer_list);
   STAILQ_INIT(&adapter->used_tx_buffer_list);

   tx_buffer = adapter->tx_buffer_area;

   /* Setup the linked list of the tx_buffer's */
   for (i = 0; i < adapter->num_tx_desc; i++, tx_buffer++) {
      bzero((void *) tx_buffer, sizeof(struct em_tx_buffer));
      STAILQ_INSERT_TAIL(&adapter->free_tx_buffer_list, tx_buffer, em_tx_entry);
   }

   bzero((void *) adapter->first_tx_desc,
        (sizeof(struct em_tx_desc)) * adapter->num_tx_desc);

   /* Setup TX descriptor pointers */
   adapter->next_avail_tx_desc = adapter->first_tx_desc;
   adapter->oldest_used_tx_desc = adapter->first_tx_desc;

   /* Set number of descriptors available */
   adapter->num_tx_desc_avail = adapter->num_tx_desc;

   /* Set checksum context */
   adapter->active_checksum_context = OFFLOAD_NONE;

   return 0;
}

/*********************************************************************
 *
 *  Enable transmit unit.
 *
 **********************************************************************/
static void
em_initialize_transmit_unit(struct adapter * adapter)
{
   u_int32_t       reg_tctl;
   u_int32_t       reg_tipg = 0;

   /* Setup the Base and Length of the Tx Descriptor Ring */
   E1000_WRITE_REG(&adapter->shared, TDBAL, vtophys((vm_offset_t) adapter->tx_desc_base));
   E1000_WRITE_REG(&adapter->shared, TDBAH, 0);
   E1000_WRITE_REG(&adapter->shared, TDLEN, adapter->num_tx_desc *
               sizeof(struct em_tx_desc));

   /* Setup the HW Tx Head and Tail descriptor pointers */
   E1000_WRITE_REG(&adapter->shared, TDH, 0);
   E1000_WRITE_REG(&adapter->shared, TDT, 0);


   HW_DEBUGOUT2("Base = %x, Length = %x\n", E1000_READ_REG(&adapter->shared, TDBAL),
                E1000_READ_REG(&adapter->shared, TDLEN));

   
   /* Set the default values for the Tx Inter Packet Gap timer */
   switch (adapter->shared.mac_type) {
   case em_82543:
   case em_82544:
   case em_82540:
      if (adapter->shared.media_type == em_media_type_fiber)
         reg_tipg = DEFAULT_82543_TIPG_IPGT_FIBER;
      else
         reg_tipg = DEFAULT_82543_TIPG_IPGT_COPPER;
      reg_tipg |= DEFAULT_82543_TIPG_IPGR1 << E1000_TIPG_IPGR1_SHIFT;
      reg_tipg |= DEFAULT_82543_TIPG_IPGR2 << E1000_TIPG_IPGR2_SHIFT;
      break;
   case em_82542_rev2_0:
   case em_82542_rev2_1:
      reg_tipg = DEFAULT_82542_TIPG_IPGT;
      reg_tipg |= DEFAULT_82542_TIPG_IPGR1 << E1000_TIPG_IPGR1_SHIFT;
      reg_tipg |= DEFAULT_82542_TIPG_IPGR2 << E1000_TIPG_IPGR2_SHIFT;
      break;
   default:
      printf("em%d: Invalid mac type detected\n", adapter->unit);
   }
   E1000_WRITE_REG(&adapter->shared, TIPG, reg_tipg);
   E1000_WRITE_REG(&adapter->shared, TIDV, adapter->tx_int_delay);

   /* Program the Transmit Control Register */
   reg_tctl = E1000_TCTL_PSP | E1000_TCTL_EN |
      (E1000_COLLISION_THRESHOLD << E1000_CT_SHIFT);
   if (adapter->link_duplex == 1) {
      reg_tctl |= E1000_FDX_COLLISION_DISTANCE << E1000_COLD_SHIFT;
   } else {
      reg_tctl |= E1000_HDX_COLLISION_DISTANCE << E1000_COLD_SHIFT;
   }
   E1000_WRITE_REG(&adapter->shared, TCTL, reg_tctl);

   /* Setup Transmit Descriptor Settings for this adapter */   
   adapter->txd_cmd = E1000_TXD_CMD_IFCS;

   if(adapter->tx_int_delay > 0)
      adapter->txd_cmd |= E1000_TXD_CMD_IDE;
   
   if(adapter->shared.report_tx_early == 1)
      adapter->txd_cmd |= E1000_TXD_CMD_RS;
   else
      adapter->txd_cmd |= E1000_TXD_CMD_RPS;

   return;
}

/*********************************************************************
 *
 *  Free all transmit related data structures.
 *
 **********************************************************************/
static void
em_free_transmit_structures(struct adapter * adapter)
{
   struct em_tx_buffer   *tx_buffer;
   int             i;

   INIT_DEBUGOUT("free_transmit_structures: begin");

   if (adapter->tx_buffer_area != NULL) {
      tx_buffer = adapter->tx_buffer_area;
      for (i = 0; i < adapter->num_tx_desc; i++, tx_buffer++) {
         if (tx_buffer->m_head != NULL)
            m_freem(tx_buffer->m_head);
         tx_buffer->m_head = NULL;
      }
   }
   if (adapter->tx_buffer_area != NULL) {
      free(adapter->tx_buffer_area, M_DEVBUF);
      adapter->tx_buffer_area = NULL;
   }
   return;
}

/*********************************************************************
 *
 *  The offload context needs to be set when we transfer the first
 *  packet of a particular protocol (TCP/UDP). We change the
 *  context only if the protocol type changes.
 *
 **********************************************************************/
static void
em_transmit_checksum_setup(struct adapter * adapter,
                struct mbuf *mp,
                struct em_tx_buffer *tx_buffer,
                u_int32_t *txd_upper,
                u_int32_t *txd_lower) 
{
   struct em_context_desc *TXD;
   struct em_tx_desc * current_tx_desc;
     
   if (mp->m_pkthdr.csum_flags) {

      if(mp->m_pkthdr.csum_flags & CSUM_TCP) {
         *txd_upper = E1000_TXD_POPTS_TXSM << 8;
         *txd_lower = E1000_TXD_CMD_DEXT | E1000_TXD_DTYP_D;
         if(adapter->active_checksum_context == OFFLOAD_TCP_IP)
            return;
         else 
            adapter->active_checksum_context = OFFLOAD_TCP_IP;

      } else if(mp->m_pkthdr.csum_flags & CSUM_UDP) {
         *txd_upper = E1000_TXD_POPTS_TXSM << 8;
         *txd_lower = E1000_TXD_CMD_DEXT | E1000_TXD_DTYP_D;
         if(adapter->active_checksum_context == OFFLOAD_UDP_IP)
            return;
         else 
            adapter->active_checksum_context = OFFLOAD_UDP_IP;
      } else {
         *txd_upper = 0;
         *txd_lower = 0;
         return;
      }
   }
   else {
      *txd_upper = 0;
      *txd_lower = 0;
      return;
   }

   /* If we reach this point, the checksum offload context
    * needs to be reset.
    */
   current_tx_desc = adapter->next_avail_tx_desc;
   TXD = (struct em_context_desc *)current_tx_desc;

   TXD->lower_setup.ip_fields.ipcss = ETHER_HDR_LEN;
   TXD->lower_setup.ip_fields.ipcso = ETHER_HDR_LEN + offsetof(struct ip, ip_sum);
   TXD->lower_setup.ip_fields.ipcse = ETHER_HDR_LEN + sizeof(struct ip) - 1;

   TXD->upper_setup.tcp_fields.tucss = ETHER_HDR_LEN + sizeof(struct ip);
   TXD->upper_setup.tcp_fields.tucse = 0;

   if(adapter->active_checksum_context == OFFLOAD_TCP_IP) {
      TXD->upper_setup.tcp_fields.tucso = ETHER_HDR_LEN + sizeof(struct ip) + 
         offsetof(struct tcphdr, th_sum);
   } else if (adapter->active_checksum_context == OFFLOAD_UDP_IP) {
      TXD->upper_setup.tcp_fields.tucso = ETHER_HDR_LEN + sizeof(struct ip) + 
         offsetof(struct udphdr, uh_sum);
   }

   TXD->tcp_seg_setup.data = 0;
   TXD->cmd_and_length = E1000_TXD_CMD_DEXT;

   if (current_tx_desc == adapter->last_tx_desc)
      adapter->next_avail_tx_desc = adapter->first_tx_desc;
   else
      adapter->next_avail_tx_desc++;

   adapter->num_tx_desc_avail--;
    
   tx_buffer->num_tx_desc_used++;
   return;
}


/*********************************************************************
 *
 *  Get a buffer from system mbuf buffer pool.
 *
 **********************************************************************/
static int
em_get_buf(struct em_rx_buffer *rx_buffer, struct adapter *adapter,
           struct mbuf *mp)
{
   struct mbuf    *nmp;
   struct ifnet   *ifp;

   ifp = &adapter->interface_data.ac_if;

   if (mp == NULL) {
      MGETHDR(nmp, M_DONTWAIT, MT_DATA);
      if (nmp == NULL) {
         adapter->mbuf_alloc_failed++;
         return (ENOBUFS);
      }
      MCLGET(nmp, M_DONTWAIT);
      if ((nmp->m_flags & M_EXT) == 0) {
         m_freem(nmp);
         adapter->mbuf_cluster_failed++;
         return (ENOBUFS);
      }
      nmp->m_len = nmp->m_pkthdr.len = MCLBYTES;
   } else {
      nmp = mp;
      nmp->m_len = nmp->m_pkthdr.len = MCLBYTES;
      nmp->m_data = nmp->m_ext.ext_buf;
      nmp->m_next = NULL;
   }

   if (ifp->if_mtu <= ETHERMTU) {
      m_adj(nmp, ETHER_ALIGN);
   }

   rx_buffer->m_head = nmp;
   rx_buffer->buffer_addr = vtophys(mtod(nmp, vm_offset_t));

   return (0);
}

/*********************************************************************
 *
 *  Allocate memory for rx_buffer structures. Since we use one 
 *  rx_buffer per received packet, the maximum number of rx_buffer's 
 *  that we'll need is equal to the number of receive descriptors 
 *  that we've allocated.
 *
 **********************************************************************/
static int
em_allocate_receive_structures(struct adapter * adapter)
{
   int             i;
   struct em_rx_buffer   *rx_buffer;

   if (!(adapter->rx_buffer_area =
        (struct em_rx_buffer *) malloc(sizeof(struct em_rx_buffer) *
                                       adapter->num_rx_desc, M_DEVBUF,
                                       M_NOWAIT))) {
      printf("em%d: Unable to allocate rx_buffer memory\n", adapter->unit);
      return (ENOMEM);
   }

   bzero(adapter->rx_buffer_area,
         sizeof(struct em_rx_buffer) * adapter->num_rx_desc);

   for (i = 0, rx_buffer = adapter->rx_buffer_area;
       i < adapter->num_rx_desc; i++, rx_buffer++) {

      if (em_get_buf(rx_buffer, adapter, NULL) == ENOBUFS) {
         rx_buffer->m_head = NULL;
         return (ENOBUFS);
      }
   }

   return (0);
}

/*********************************************************************
 *
 *  Allocate and initialize receive structures.
 *  
 **********************************************************************/
static int
em_setup_receive_structures(struct adapter * adapter)
{
   struct em_rx_buffer   *rx_buffer;
   struct em_rx_desc     *rx_desc;
   int             i;

   if(em_allocate_receive_structures(adapter))
      return ENOMEM;

   STAILQ_INIT(&adapter->rx_buffer_list);

   adapter->first_rx_desc =
      (struct em_rx_desc *) adapter->rx_desc_base;
   adapter->last_rx_desc =
      adapter->first_rx_desc + (adapter->num_rx_desc - 1);

   rx_buffer = (struct em_rx_buffer *) adapter->rx_buffer_area;

   bzero((void *) adapter->first_rx_desc,
        (sizeof(struct em_rx_desc)) * adapter->num_rx_desc);

   /* Build a linked list of rx_buffer's */
   for (i = 0, rx_desc = adapter->first_rx_desc;
       i < adapter->num_rx_desc;
       i++, rx_buffer++, rx_desc++) {
      if (rx_buffer->m_head == NULL)
         printf("em%d: Receive buffer memory not allocated", adapter->unit);
      else {
         rx_desc->buffer_addr = rx_buffer->buffer_addr;
         STAILQ_INSERT_TAIL(&adapter->rx_buffer_list, rx_buffer, em_rx_entry);
      }
   }

   /* Setup our descriptor pointers */
   adapter->next_rx_desc_to_check = adapter->first_rx_desc;

   return(0);
}

/*********************************************************************
 *
 *  Enable receive unit.
 *  
 **********************************************************************/
static void
em_initialize_receive_unit(struct adapter * adapter)
{
   u_int32_t       reg_rctl;
   u_int32_t       reg_rxcsum;
   struct ifnet    *ifp;
   
   ifp = &adapter->interface_data.ac_if;

   /* Make sure receives are disabled while setting up the descriptor ring */
   E1000_WRITE_REG(&adapter->shared, RCTL, 0);

   /* Set the Receive Delay Timer Register */
   E1000_WRITE_REG(&adapter->shared, RDTR, adapter->rx_int_delay | E1000_RDT_FPDB);

   /* Setup the Base and Length of the Rx Descriptor Ring */
   E1000_WRITE_REG(&adapter->shared, RDBAL, vtophys((vm_offset_t) adapter->rx_desc_base));
   E1000_WRITE_REG(&adapter->shared, RDBAH, 0);
   E1000_WRITE_REG(&adapter->shared, RDLEN, adapter->num_rx_desc *
               sizeof(struct em_rx_desc));

   /* Setup the HW Rx Head and Tail Descriptor Pointers */
   E1000_WRITE_REG(&adapter->shared, RDH, 0);
   E1000_WRITE_REG(&adapter->shared, RDT,
               (((u_int32_t) adapter->last_rx_desc -
                 (u_int32_t) adapter->first_rx_desc) >> 4));
   
   /* Setup the Receive Control Register */
   reg_rctl = E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_LBM_NO |
      E1000_RCTL_RDMTS_HALF |
      (adapter->shared.mc_filter_type << E1000_RCTL_MO_SHIFT);

   if (adapter->shared.tbi_compatibility_on == TRUE)
      reg_rctl |= E1000_RCTL_SBP;


   switch (adapter->rx_buffer_len) {
   case EM_RXBUFFER_2048:
      reg_rctl |= E1000_RCTL_SZ_2048 | E1000_RCTL_LPE;
      break;
   case EM_RXBUFFER_4096:
      reg_rctl |= E1000_RCTL_SZ_4096 | E1000_RCTL_BSEX | E1000_RCTL_LPE;
      break;            
   case EM_RXBUFFER_8192:
      reg_rctl |= E1000_RCTL_SZ_8192 | E1000_RCTL_BSEX | E1000_RCTL_LPE;
      break;
   case EM_RXBUFFER_16384:
      reg_rctl |= E1000_RCTL_SZ_16384 | E1000_RCTL_BSEX | E1000_RCTL_LPE;
      break;
   default:
      reg_rctl |= E1000_RCTL_SZ_2048;
   }

   /* Enable 82543 Receive Checksum Offload for TCP and UDP */
   if((adapter->shared.mac_type >= em_82543) && (ifp->if_capenable & IFCAP_RXCSUM)) {
      reg_rxcsum = E1000_READ_REG(&adapter->shared, RXCSUM);
      reg_rxcsum |= (E1000_RXCSUM_IPOFL | E1000_RXCSUM_TUOFL);
      E1000_WRITE_REG(&adapter->shared, RXCSUM, reg_rxcsum);
   }

   /* Enable Receives */
   E1000_WRITE_REG(&adapter->shared, RCTL, reg_rctl);

   return;
}

/*********************************************************************
 *
 *  Free receive related data structures.
 *
 **********************************************************************/
static void
em_free_receive_structures(struct adapter * adapter)
{
   struct em_rx_buffer   *rx_buffer;
   int             i;

   INIT_DEBUGOUT("free_receive_structures: begin");

   if (adapter->rx_buffer_area != NULL) {
      rx_buffer = adapter->rx_buffer_area;
      for (i = 0; i < adapter->num_rx_desc; i++, rx_buffer++) {
         if (rx_buffer->m_head != NULL)
            m_freem(rx_buffer->m_head);
         rx_buffer->m_head = NULL;
      }
   }
   if (adapter->rx_buffer_area != NULL) {
      free(adapter->rx_buffer_area, M_DEVBUF);
      adapter->rx_buffer_area = NULL;
   }
   return;
}

/*********************************************************************
 *
 *  This routine executes in interrupt context. It replenishes
 *  the mbufs in the descriptor and sends data which has been
 *  dma'ed into host memory to upper layer.
 *
 *********************************************************************/
static void
em_process_receive_interrupts(struct adapter * adapter)
{
   struct mbuf         *mp;
   struct ifnet        *ifp;
   struct ether_header *eh;
   u_int16_t           len;
   u_int8_t            last_byte;
   u_int8_t            accept_frame = 0;
   u_int8_t            eop = 0;
   u_int32_t           pkt_len = 0;

   /* Pointer to the receive descriptor being examined. */
   struct em_rx_desc   *current_desc;
   struct em_rx_desc   *last_desc_processed;
   struct em_rx_buffer *rx_buffer;

   ifp = &adapter->interface_data.ac_if;
   current_desc = adapter->next_rx_desc_to_check;

   if (!((current_desc->status) & E1000_RXD_STAT_DD)) {
#ifdef DBG_STATS
      adapter->no_pkts_avail++;
#endif
      return;
   }
   
   while (current_desc->status & E1000_RXD_STAT_DD) {

      /* Get a pointer to the actual receive buffer */
      rx_buffer = STAILQ_FIRST(&adapter->rx_buffer_list);

      if(rx_buffer == NULL) {
         printf("em%d: Found null rx_buffer\n", adapter->unit);
         return;
      }

      mp = rx_buffer->m_head;      
      accept_frame = 1;

      if (current_desc->status & E1000_RXD_STAT_EOP) {
         eop = 1;
         len = current_desc->length - ETHER_CRC_LEN;
      }
      else {
         eop = 0;
         len = current_desc->length;
      }

      if(current_desc->errors & E1000_RXD_ERR_FRAME_ERR_MASK) {
         
         /* Compute packet length for tbi_accept macro */
         pkt_len = current_desc->length;
         if (adapter->fmp != NULL) {
            pkt_len += adapter->fmp->m_pkthdr.len; 
         }

         last_byte = *(mtod(rx_buffer->m_head,caddr_t) + current_desc->length - 1);

         if (TBI_ACCEPT(&adapter->shared, current_desc->status, 
                        current_desc->errors, 
                        pkt_len, last_byte)) {  
            pkt_len = em_tbi_adjust_stats(&adapter->shared, &adapter->stats, 
                                               pkt_len, adapter->shared.mac_addr);
            len--;
         } else {  
            accept_frame = 0;
         }
      }

      if (accept_frame) {

         if (em_get_buf(rx_buffer, adapter, NULL) == ENOBUFS) {
            adapter->dropped_pkts++;
            em_get_buf(rx_buffer, adapter, mp);
            if(adapter->fmp != NULL) m_freem(adapter->fmp);
            adapter->fmp = NULL;
            adapter->lmp = NULL;
            break;
         }

         /* Assign correct length to the current fragment */
         mp->m_len = len;

         if(adapter->fmp == NULL) {
            mp->m_pkthdr.len = len;
            adapter->fmp = mp;       /* Store the first mbuf */
            adapter->lmp = mp;
         }
         else {
            /* Chain mbuf's together */
            mp->m_flags &= ~M_PKTHDR;
            adapter->lmp->m_next = mp;
            adapter->lmp = adapter->lmp->m_next;
            adapter->fmp->m_pkthdr.len += len;
         }

         if (eop) {
            adapter->fmp->m_pkthdr.rcvif = ifp;

            eh = mtod(adapter->fmp, struct ether_header *);

            /* Remove ethernet header from mbuf */
            m_adj(adapter->fmp, sizeof(struct ether_header));
            em_receive_checksum(adapter, current_desc, adapter->fmp);
            ether_input(ifp, eh, adapter->fmp);
      
            adapter->fmp = NULL;
            adapter->lmp = NULL;
         }
      } else {
         adapter->dropped_pkts++;
         em_get_buf(rx_buffer, adapter, mp);
         if(adapter->fmp != NULL) m_freem(adapter->fmp);
         adapter->fmp = NULL;
         adapter->lmp = NULL;
      }
      
      /* Zero out the receive descriptors status  */
      current_desc->status = 0;
      
      if (rx_buffer->m_head != NULL) {
         current_desc->buffer_addr = rx_buffer->buffer_addr;
      }

      /* Advance our pointers to the next descriptor (checking for wrap). */
      if (current_desc == adapter->last_rx_desc)
         adapter->next_rx_desc_to_check = adapter->first_rx_desc;
      else
         ((adapter)->next_rx_desc_to_check)++;

      last_desc_processed = current_desc;
      current_desc = adapter->next_rx_desc_to_check;
      /* 
       * Put the buffer that we just indicated back at the end of our list
       */
      STAILQ_REMOVE_HEAD(&adapter->rx_buffer_list, em_rx_entry);
      STAILQ_INSERT_TAIL(&adapter->rx_buffer_list, rx_buffer, em_rx_entry);

      /* Advance the E1000's Receive Queue #0  "Tail Pointer". */
      E1000_WRITE_REG(&adapter->shared, RDT, (((u_int32_t) last_desc_processed -
                        (u_int32_t) adapter->first_rx_desc) >> 4));
   }
   return;
}

/*********************************************************************
 *
 *  Verify that the hardware indicated that the checksum is valid. 
 *  Inform the stack about the status of checksum so that stack
 *  doesn't spend time verifying the checksum.
 *
 *********************************************************************/
static void
em_receive_checksum(struct adapter *adapter,
                    struct em_rx_desc *rx_desc,
                    struct mbuf *mp)
{
   /* 82543 or newer only */
   if((adapter->shared.mac_type < em_82543) ||
      /* Ignore Checksum bit is set */
      (rx_desc->status & E1000_RXD_STAT_IXSM)) {
      mp->m_pkthdr.csum_flags = 0;
      return;
   }

   if (rx_desc->status & E1000_RXD_STAT_IPCS) {
      /* Did it pass? */
      if (!(rx_desc->errors & E1000_RXD_ERR_IPE)) {
         /* IP Checksum Good */
         mp->m_pkthdr.csum_flags = CSUM_IP_CHECKED;
         mp->m_pkthdr.csum_flags |= CSUM_IP_VALID;

      }
      else {
         mp->m_pkthdr.csum_flags = 0;
      }
   }

   if (rx_desc->status & E1000_RXD_STAT_TCPCS) {
      /* Did it pass? */        
      if (!(rx_desc->errors & E1000_RXD_ERR_TCPE)) {
         mp->m_pkthdr.csum_flags |= (CSUM_DATA_VALID | CSUM_PSEUDO_HDR);
         mp->m_pkthdr.csum_data = htons(0xffff);
      }
   }

   return;
}


static void
em_enable_intr(struct adapter * adapter)
{
   E1000_WRITE_REG(&adapter->shared, IMS, (IMS_ENABLE_MASK));
   return;
}

static void
em_disable_intr(struct adapter * adapter)
{
   E1000_WRITE_REG(&adapter->shared, IMC, (0xffffffff & ~E1000_IMC_RXSEQ));
   return;
}

void em_write_pci_cfg(struct em_shared_adapter *adapter,
                      uint32_t reg,
                      uint16_t * value)
{
   pci_write_config(((struct em_osdep *)adapter->back)->dev, reg, *value, 2);
}


/**********************************************************************
 *
 *  Update the board statistics counters. 
 *
 **********************************************************************/
static void
em_update_stats_counters(struct adapter * adapter)
{
   struct ifnet   *ifp;

   adapter->stats.crcerrs += E1000_READ_REG(&adapter->shared, CRCERRS);
   adapter->stats.symerrs += E1000_READ_REG(&adapter->shared, SYMERRS);
   adapter->stats.mpc += E1000_READ_REG(&adapter->shared, MPC);
   adapter->stats.scc += E1000_READ_REG(&adapter->shared, SCC);
   adapter->stats.ecol += E1000_READ_REG(&adapter->shared, ECOL);
   adapter->stats.mcc += E1000_READ_REG(&adapter->shared, MCC);
   adapter->stats.latecol += E1000_READ_REG(&adapter->shared, LATECOL);
   adapter->stats.colc += E1000_READ_REG(&adapter->shared, COLC);
   adapter->stats.dc += E1000_READ_REG(&adapter->shared, DC);
   adapter->stats.sec += E1000_READ_REG(&adapter->shared, SEC);
   adapter->stats.rlec += E1000_READ_REG(&adapter->shared, RLEC);
   adapter->stats.xonrxc += E1000_READ_REG(&adapter->shared, XONRXC);
   adapter->stats.xontxc += E1000_READ_REG(&adapter->shared, XONTXC);
   adapter->stats.xoffrxc += E1000_READ_REG(&adapter->shared, XOFFRXC);
   adapter->stats.xofftxc += E1000_READ_REG(&adapter->shared, XOFFTXC);
   adapter->stats.fcruc += E1000_READ_REG(&adapter->shared, FCRUC);
   adapter->stats.prc64 += E1000_READ_REG(&adapter->shared, PRC64);
   adapter->stats.prc127 += E1000_READ_REG(&adapter->shared, PRC127);
   adapter->stats.prc255 += E1000_READ_REG(&adapter->shared, PRC255);
   adapter->stats.prc511 += E1000_READ_REG(&adapter->shared, PRC511);
   adapter->stats.prc1023 += E1000_READ_REG(&adapter->shared, PRC1023);
   adapter->stats.prc1522 += E1000_READ_REG(&adapter->shared, PRC1522);
   adapter->stats.gprc += E1000_READ_REG(&adapter->shared, GPRC);
   adapter->stats.bprc += E1000_READ_REG(&adapter->shared, BPRC);
   adapter->stats.mprc += E1000_READ_REG(&adapter->shared, MPRC);
   adapter->stats.gptc += E1000_READ_REG(&adapter->shared, GPTC);

   /* For the 64-bit byte counters the low dword must be read first. */
   /* Both registers clear on the read of the high dword */

   adapter->stats.gorcl += E1000_READ_REG(&adapter->shared, GORCL); 
   adapter->stats.gorch += E1000_READ_REG(&adapter->shared, GORCH);
   adapter->stats.gotcl += E1000_READ_REG(&adapter->shared, GOTCL);
   adapter->stats.gotch += E1000_READ_REG(&adapter->shared, GOTCH);

   adapter->stats.rnbc += E1000_READ_REG(&adapter->shared, RNBC);
   adapter->stats.ruc += E1000_READ_REG(&adapter->shared, RUC);
   adapter->stats.rfc += E1000_READ_REG(&adapter->shared, RFC);
   adapter->stats.roc += E1000_READ_REG(&adapter->shared, ROC);
   adapter->stats.rjc += E1000_READ_REG(&adapter->shared, RJC);

   adapter->stats.torl += E1000_READ_REG(&adapter->shared, TORL);
   adapter->stats.torh += E1000_READ_REG(&adapter->shared, TORH);
   adapter->stats.totl += E1000_READ_REG(&adapter->shared, TOTL);
   adapter->stats.toth += E1000_READ_REG(&adapter->shared, TOTH);

   adapter->stats.tpr += E1000_READ_REG(&adapter->shared, TPR);
   adapter->stats.tpt += E1000_READ_REG(&adapter->shared, TPT);
   adapter->stats.ptc64 += E1000_READ_REG(&adapter->shared, PTC64);
   adapter->stats.ptc127 += E1000_READ_REG(&adapter->shared, PTC127);
   adapter->stats.ptc255 += E1000_READ_REG(&adapter->shared, PTC255);
   adapter->stats.ptc511 += E1000_READ_REG(&adapter->shared, PTC511);
   adapter->stats.ptc1023 += E1000_READ_REG(&adapter->shared, PTC1023);
   adapter->stats.ptc1522 += E1000_READ_REG(&adapter->shared, PTC1522);
   adapter->stats.mptc += E1000_READ_REG(&adapter->shared, MPTC);
   adapter->stats.bptc += E1000_READ_REG(&adapter->shared, BPTC);

   if (adapter->shared.mac_type >= em_82543) {
      adapter->stats.algnerrc += E1000_READ_REG(&adapter->shared, ALGNERRC);
      adapter->stats.rxerrc += E1000_READ_REG(&adapter->shared, RXERRC);
      adapter->stats.tncrs += E1000_READ_REG(&adapter->shared, TNCRS);
      adapter->stats.cexterr += E1000_READ_REG(&adapter->shared, CEXTERR);
      adapter->stats.tsctc += E1000_READ_REG(&adapter->shared, TSCTC);
      adapter->stats.tsctfc += E1000_READ_REG(&adapter->shared, TSCTFC);
   }
   ifp = &adapter->interface_data.ac_if;

   /* Fill out the OS statistics structure */
   ifp->if_ipackets = adapter->stats.gprc;
   ifp->if_opackets = adapter->stats.gptc;
   ifp->if_ibytes = adapter->stats.gorcl;
   ifp->if_obytes = adapter->stats.gotcl;
   ifp->if_imcasts = adapter->stats.mprc;
   ifp->if_collisions = adapter->stats.colc;

   /* Rx Errors */
   ifp->if_ierrors =
      adapter->dropped_pkts +
      adapter->stats.rxerrc +
      adapter->stats.crcerrs +
      adapter->stats.algnerrc +
      adapter->stats.rlec + adapter->stats.rnbc + 
      adapter->stats.mpc + adapter->stats.cexterr;

   /* Tx Errors */
   ifp->if_oerrors = adapter->stats.ecol + adapter->stats.latecol;

}


/**********************************************************************
 *
 *  This routine is called only when em_display_debug_stats is enabled.
 *  This routine provides a way to take a look at important statistics
 *  maintained by the driver and hardware.
 *
 **********************************************************************/
static void
em_print_hw_stats(struct adapter * adapter)
{
   int unit = adapter->unit;

#ifdef DBG_STATS
   printf("em%d: Packets not Avail = %ld\n", unit, adapter->no_pkts_avail);
   printf("em%d: clean_tx_interrupts = %ld\n", unit, adapter->clean_tx_interrupts); 
#endif
   
   printf("em%d: Tx Descriptors not Avail = %ld\n", unit, adapter->no_tx_desc_avail);
   printf("em%d: Tx Buffer not avail1 = %ld\n", unit, adapter->no_tx_buffer_avail1);
   printf("em%d: Tx Buffer not avail2 = %ld\n", unit, adapter->no_tx_buffer_avail2);
   printf("em%d: Std Mbuf Failed = %ld\n",unit, adapter->mbuf_alloc_failed);
   printf("em%d: Std Cluster Failed = %ld\n",unit, adapter->mbuf_cluster_failed);
   printf("em%d: Xmit Pullup = %ld\n",unit, adapter->xmit_pullup);

   printf("em%d: Symbol errors = %lld\n", unit, adapter->stats.symerrs);
   printf("em%d: Sequence errors = %lld\n", unit, adapter->stats.sec);
   printf("em%d: Defer count = %lld\n", unit, adapter->stats.dc);

   printf("em%d: Missed Packets = %lld\n", unit, adapter->stats.mpc);
   printf("em%d: Receive No Buffers = %lld\n", unit, adapter->stats.rnbc);
   printf("em%d: Receive length errors = %lld\n", unit, adapter->stats.rlec);
   printf("em%d: Receive errors = %lld\n", unit, adapter->stats.rxerrc);
   printf("em%d: Crc errors = %lld\n", unit, adapter->stats.crcerrs);
   printf("em%d: Alignment errors = %lld\n", unit, adapter->stats.algnerrc);
   printf("em%d: Carrier extension errors = %lld\n", unit, adapter->stats.cexterr);
   printf("em%d: Driver dropped packets = %ld\n", unit, adapter->dropped_pkts);

   printf("em%d: XON Rcvd = %lld\n", unit, adapter->stats.xonrxc);
   printf("em%d: XON Xmtd = %lld\n", unit, adapter->stats.xontxc);
   printf("em%d: XOFF Rcvd = %lld\n", unit, adapter->stats.xoffrxc);
   printf("em%d: XOFF Xmtd = %lld\n", unit, adapter->stats.xofftxc);

   printf("em%d: Good Packets Rcvd = %lld\n", unit, adapter->stats.gprc);
   printf("em%d: Good Packets Xmtd = %lld\n", unit, adapter->stats.gptc);
}


/**********************************************************************
 *
 *  Examine each tx_buffer in the used queue. If the hardware is done
 *  processing the packet then free associated resources. The
 *  tx_buffer is put back on the free queue. 
 *
 **********************************************************************/
static void
em_clean_transmit_interrupts(struct adapter * adapter)
{
   struct em_tx_buffer *tx_buffer;
   struct em_tx_desc   *tx_desc;
   int             s;
   struct ifnet   *ifp;

   s = splimp();
#ifdef DBG_STATS
   adapter->clean_tx_interrupts++;
#endif

   for (tx_buffer = STAILQ_FIRST(&adapter->used_tx_buffer_list);
        tx_buffer; 
        tx_buffer = STAILQ_FIRST(&adapter->used_tx_buffer_list)) {

      /* 
       * Get hold of the next descriptor that the em will report status
       * back to (this will be the last descriptor of a given tx_buffer). We
       * only want to free the tx_buffer (and it resources) if the driver is
       * done with ALL of the descriptors.  If the driver is done with the
       * last one then it is done with all of them.
       */

      tx_desc = adapter->oldest_used_tx_desc +
         (tx_buffer->num_tx_desc_used - 1);

      /* Check for wrap case */
      if (tx_desc > adapter->last_tx_desc)
         tx_desc -= adapter->num_tx_desc;


      /* 
       * If the descriptor done bit is set free tx_buffer and associated
       * resources
       */
      if (tx_desc->upper.fields.status &
         E1000_TXD_STAT_DD) {

         STAILQ_REMOVE_HEAD(&adapter->used_tx_buffer_list, em_tx_entry);

         if ((tx_desc == adapter->last_tx_desc))
            adapter->oldest_used_tx_desc =
               adapter->first_tx_desc;
         else
            adapter->oldest_used_tx_desc = (tx_desc + 1);

         /* Make available the descriptors that were previously used */
         adapter->num_tx_desc_avail +=
            tx_buffer->num_tx_desc_used;

         tx_buffer->num_tx_desc_used = 0;

         if (tx_buffer->m_head) {
            m_freem(tx_buffer->m_head);
            tx_buffer->m_head = NULL;
         }
         /* Return this "Software packet" back to the "free" list */
         STAILQ_INSERT_TAIL(&adapter->free_tx_buffer_list, tx_buffer, em_tx_entry);
      } else {
         /* 
          * Found a tx_buffer that the em is not done with then there is
          * no reason to check the rest of the queue.
          */
         break;
      }
   }                     /* end for each tx_buffer */

   ifp = &adapter->interface_data.ac_if;

   /* Tell the stack that it is OK to send packets */
   if (adapter->num_tx_desc_avail > TX_CLEANUP_THRESHOLD) {
      ifp->if_timer = 0;
      ifp->if_flags &= ~IFF_OACTIVE;
   }
   splx(s);
   return;
}

