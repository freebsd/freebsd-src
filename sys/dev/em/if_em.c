/**************************************************************************

Copyright (c) 2001 Intel Corporation
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

char em_driver_version[] = "1.1.10";


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
static void EnableInterrupts __P((struct adapter *));
static void DisableInterrupts __P((struct adapter *));
static void em_free_transmit_structures __P((struct adapter *));
static void em_free_receive_structures __P((struct adapter *));
static void em_update_stats_counters __P((struct adapter *));
static void em_clean_transmit_interrupts __P((struct adapter *));
static int em_allocate_receive_structures __P((struct adapter *));
static int em_allocate_transmit_structures __P((struct adapter *));
static void em_process_receive_interrupts __P((struct adapter *));
static void em_receive_checksum __P((struct adapter *, 
                                     struct em_rx_desc * RxDescriptor,
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
   struct adapter * Adapter;
   int             s;
   int             tsize, rsize;

   INIT_DEBUGOUT("em_attach: begin");
   s = splimp();

   /* Allocate, clear, and link in our Adapter structure */
   if (!(Adapter = device_get_softc(dev))) {
      printf("em: Adapter structure allocation failed\n");
      splx(s);
      return(ENOMEM);
   }
   bzero(Adapter, sizeof(struct adapter ));
   Adapter->dev = dev;
   Adapter->osdep.dev = dev;
   Adapter->unit = device_get_unit(dev);

   if (em_adapter_list != NULL)
      em_adapter_list->prev = Adapter;
   Adapter->next = em_adapter_list;
   em_adapter_list = Adapter;

   callout_handle_init(&Adapter->timer_handle);

   /* Determine hardware revision */
   em_identify_hardware(Adapter);

   /* Parameters (to be read from user) */
   Adapter->NumTxDescriptors = MAX_TXD;
   Adapter->NumRxDescriptors = MAX_RXD;
   Adapter->TxIntDelay = TIDV;
   Adapter->RxIntDelay = RIDV;
   Adapter->shared.autoneg = DO_AUTO_NEG;
   Adapter->shared.wait_autoneg_complete = WAIT_FOR_AUTO_NEG_DEFAULT;
   Adapter->shared.autoneg_advertised = AUTONEG_ADV_DEFAULT;
   Adapter->shared.tbi_compatibility_en = TRUE;
   Adapter->RxBufferLen = EM_RXBUFFER_2048;
   Adapter->RxChecksum = EM_ENABLE_RXCSUM_OFFLOAD;
   
   Adapter->shared.fc_high_water = FC_DEFAULT_HI_THRESH;
   Adapter->shared.fc_low_water  = FC_DEFAULT_LO_THRESH;
   Adapter->shared.fc_pause_time = FC_DEFAULT_TX_TIMER;
   Adapter->shared.fc_send_xon   = TRUE;
   Adapter->shared.fc = em_fc_full;


   /* Set the max frame size assuming standard ethernet sized frames */   
   Adapter->shared.max_frame_size = ETHERMTU + ETHER_HDR_LEN + ETHER_CRC_LEN;

   /* This controls when hardware reports transmit completion status. */
   if ((EM_REPORT_TX_EARLY == 0) || (EM_REPORT_TX_EARLY == 1)) {
      Adapter->shared.report_tx_early = EM_REPORT_TX_EARLY;
   } else {
      if(Adapter->shared.mac_type < em_82543) {
         Adapter->shared.report_tx_early = 0;
      } else {
         Adapter->shared.report_tx_early = 1;
      }
   }

   if (em_allocate_pci_resources(Adapter)) {
      printf("em%d: Allocation of PCI resources failed\n", Adapter->unit);
      em_free_pci_resources(Adapter);
      splx(s);
      return(ENXIO);
   }

   tsize = EM_ROUNDUP(Adapter->NumTxDescriptors *
                      sizeof(struct em_tx_desc), 4096);

   /* Allocate Transmit Descriptor ring */
   if (!(Adapter->TxDescBase = (struct em_tx_desc *)
         contigmalloc(tsize, M_DEVBUF, M_NOWAIT, 0, ~0, PAGE_SIZE, 0))) {
      printf("em%d: Unable to allocate TxDescriptor memory\n", Adapter->unit);
      em_free_pci_resources(Adapter);
      splx(s);
      return(ENOMEM);
   }

   rsize = EM_ROUNDUP(Adapter->NumRxDescriptors *
                      sizeof(struct em_rx_desc), 4096);

   /* Allocate Receive Descriptor ring */
   if (!(Adapter->RxDescBase = (struct em_rx_desc *)
        contigmalloc(rsize, M_DEVBUF, M_NOWAIT, 0, ~0, PAGE_SIZE, 0))) {
      printf("em%d: Unable to allocate RxDescriptor memory\n", Adapter->unit);
      em_free_pci_resources(Adapter);
      contigfree(Adapter->TxDescBase, tsize, M_DEVBUF);
      splx(s);
      return(ENOMEM);
   }

   /* Initialize the hardware */
   if (em_hardware_init(Adapter)) {
      printf("em%d: Unable to initialize the hardware\n",Adapter->unit);
      em_free_pci_resources(Adapter);
      contigfree(Adapter->TxDescBase, tsize, M_DEVBUF);
      contigfree(Adapter->RxDescBase, rsize, M_DEVBUF);
      splx(s);
      return(EIO);
   }

   /* Setup OS specific network interface */
   em_setup_interface(dev, Adapter);

   /* Initialize statistics */
   em_clear_hw_cntrs(&Adapter->shared);
   em_update_stats_counters(Adapter);
   Adapter->shared.get_link_status = 1;
   em_check_for_link(&Adapter->shared);

   /* Print the link status */
   if (Adapter->LinkIsActive == 1) {
      em_get_speed_and_duplex(&Adapter->shared, &Adapter->LineSpeed, &Adapter->FullDuplex);
      printf("em%d:  Speed:%d Mbps  Duplex:%s\n",
             Adapter->unit,
             Adapter->LineSpeed,
             Adapter->FullDuplex == FULL_DUPLEX ? "Full" : "Half");
   }
   else
      printf("em%d:  Speed:N/A  Duplex:N/A\n", Adapter->unit);


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
   struct adapter * Adapter = device_get_softc(dev);
   struct ifnet   *ifp = &Adapter->interface_data.ac_if;
   int             s;
   int             size;

   INIT_DEBUGOUT("em_detach: begin");
   s = splimp();

   em_stop(Adapter);
   em_phy_hw_reset(&Adapter->shared);
   ether_ifdetach(&Adapter->interface_data.ac_if, ETHER_BPF_SUPPORTED);
   em_free_pci_resources(Adapter);

   size = EM_ROUNDUP(Adapter->NumTxDescriptors *
                     sizeof(struct em_tx_desc), 4096);

   /* Free Transmit Descriptor ring */
   if (Adapter->TxDescBase) {
      contigfree(Adapter->TxDescBase, size, M_DEVBUF);
      Adapter->TxDescBase = NULL;
   }

   size = EM_ROUNDUP(Adapter->NumRxDescriptors *
                     sizeof(struct em_rx_desc), 4096);

   /* Free Receive Descriptor ring */
   if (Adapter->RxDescBase) {
      contigfree(Adapter->RxDescBase, size, M_DEVBUF);
      Adapter->RxDescBase = NULL;
   }

   /* Remove from the adapter list */
   if(em_adapter_list == Adapter)
      em_adapter_list = Adapter->next;
   if(Adapter->next != NULL)
      Adapter->next->prev = Adapter->prev;
   if(Adapter->prev != NULL)
      Adapter->prev->next = Adapter->next;

   ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
   ifp->if_timer = 0;

   splx(s);
   return(0);
}

static int
em_shutdown(device_t dev)
{
   struct adapter * Adapter = device_get_softc(dev);
   
   /* Issue a global reset */
   em_adapter_stop(&Adapter->shared);
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
   struct em_tx_buffer   *tx_buffer;
   struct mbuf    *m_head;
   struct mbuf    *mp;
   vm_offset_t     VirtualAddress;
   u_int32_t       txd_upper; 
   u_int32_t       txd_lower;
   struct em_tx_desc * CurrentTxDescriptor = NULL;
   struct adapter * Adapter = ifp->if_softc;

   TXRX_DEBUGOUT("em_start: begin");   

   if (!Adapter->LinkIsActive)
      return;

   s = splimp();      
   while (ifp->if_snd.ifq_head != NULL) {

      IF_DEQUEUE(&ifp->if_snd, m_head);
      
      if(m_head == NULL) break;

      if (Adapter->NumTxDescriptorsAvail <= TX_CLEANUP_THRESHOLD)
         em_clean_transmit_interrupts(Adapter);

      if (Adapter->NumTxDescriptorsAvail <= TX_CLEANUP_THRESHOLD) {
         ifp->if_flags |= IFF_OACTIVE;
         IF_PREPEND(&ifp->if_snd, m_head);
#ifdef DBG_STATS
         Adapter->NoTxDescAvail++;
#endif
         break;
      }

      tx_buffer =  STAILQ_FIRST(&Adapter->FreeSwTxPacketList);
      if (!tx_buffer) {
#ifdef DBG_STATS
         Adapter->NoTxBufferAvail1++;
#endif
         /* 
          * OK so we should not get here but I've seen it so lets try to 
          * clean up and then try to get a SwPacket again and only break 
          * if we still don't get one 
          */
         em_clean_transmit_interrupts(Adapter);
         tx_buffer = STAILQ_FIRST(&Adapter->FreeSwTxPacketList);
         if (!tx_buffer) {
            ifp->if_flags |= IFF_OACTIVE;
            IF_PREPEND(&ifp->if_snd, m_head);
#ifdef DBG_STATS
            Adapter->NoTxBufferAvail2++;
#endif
            break;
         }
      }
      STAILQ_REMOVE_HEAD(&Adapter->FreeSwTxPacketList, em_tx_entry);
      tx_buffer->NumTxDescriptorsUsed = 0;
      tx_buffer->Packet = m_head;

      if (ifp->if_hwassist > 0) {
         em_transmit_checksum_setup(Adapter,  m_head, tx_buffer, &txd_upper, &txd_lower);
      } else {
         txd_upper = 0;
         txd_lower = 0;
      }

      for (mp = m_head; mp != NULL; mp = mp->m_next) {
         if (mp->m_len == 0)
            continue;
         CurrentTxDescriptor = Adapter->NextAvailTxDescriptor;
         VirtualAddress = mtod(mp, vm_offset_t);
         CurrentTxDescriptor->buffer_addr = vtophys(VirtualAddress);

         CurrentTxDescriptor->lower.data = (txd_lower | mp->m_len);
         CurrentTxDescriptor->upper.data = (txd_upper);

         if (CurrentTxDescriptor == Adapter->LastTxDescriptor)
            Adapter->NextAvailTxDescriptor =
            Adapter->FirstTxDescriptor;
         else
            Adapter->NextAvailTxDescriptor++;

         Adapter->NumTxDescriptorsAvail--;
         tx_buffer->NumTxDescriptorsUsed++;
      }
      /* Put this tx_buffer at the end in the "in use" list */
      STAILQ_INSERT_TAIL(&Adapter->UsedSwTxPacketList, tx_buffer, em_tx_entry);

      /* 
       * Last Descriptor of Packet needs End Of Packet (EOP), Report Status
       * (RS) and append Ethernet CRC (IFCS) bits set.
       */
      CurrentTxDescriptor->lower.data |= (Adapter->TxdCmd | E1000_TXD_CMD_EOP);

      /* Send a copy of the frame to the BPF listener */
      if (ifp->if_bpf)
         bpf_mtap(ifp, m_head);
      /* 
       * Advance the Transmit Descriptor Tail (Tdt), this tells the E1000
       * that this frame is available to transmit.
       */
      E1000_WRITE_REG(&Adapter->shared, TDT, (((u_int32_t) Adapter->NextAvailTxDescriptor -
                             (u_int32_t) Adapter->FirstTxDescriptor) >> 4));
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
   int             s,
                   error = 0;
   struct ifreq   *ifr = (struct ifreq *) data;
   struct adapter * Adapter = ifp->if_softc;

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
         Adapter->shared.max_frame_size = ifp->if_mtu + ETHER_HDR_LEN + ETHER_CRC_LEN;
         em_init(Adapter);
      }
      break;
   case SIOCSIFFLAGS:
      IOCTL_DEBUGOUT("ioctl rcv'd: SIOCSIFFLAGS (Set Interface Flags)");
      if (ifp->if_flags & IFF_UP) {
         if (ifp->if_flags & IFF_RUNNING &&
             ifp->if_flags & IFF_PROMISC) {
            em_set_promisc(Adapter);
         } else if (ifp->if_flags & IFF_RUNNING &&
                    !(ifp->if_flags & IFF_PROMISC)) {
            em_disable_promisc(Adapter);
         } else
            em_init(Adapter);
      } else {
         if (ifp->if_flags & IFF_RUNNING) {
            em_stop(Adapter);
         }
      }
      break;
   case SIOCADDMULTI:
   case SIOCDELMULTI:
      IOCTL_DEBUGOUT("ioctl rcv'd: SIOC(ADD|DEL)MULTI");
      if (ifp->if_flags & IFF_RUNNING) {
         DisableInterrupts(Adapter);
         em_set_multi(Adapter);
         if(Adapter->shared.mac_type == em_82542_rev2_0)
            em_initialize_receive_unit(Adapter);
         EnableInterrupts(Adapter);
      }
      break;
   case SIOCSIFMEDIA:
   case SIOCGIFMEDIA:
      IOCTL_DEBUGOUT("ioctl rcv'd: SIOCxIFMEDIA (Get/Set Interface Media)");
      error = ifmedia_ioctl(ifp, ifr, &Adapter->media, command);
      break;
   default:
      IOCTL_DEBUGOUT1("ioctl received: UNKNOWN (0x%d)\n", (int)command);
      error = EINVAL;
   }

   splx(s);
   return(error);
}

static void
em_set_promisc(struct adapter * Adapter)
{

   u_int32_t       reg_rctl;
   struct ifnet   *ifp = &Adapter->interface_data.ac_if;

   reg_rctl = E1000_READ_REG(&Adapter->shared, RCTL);

   if(ifp->if_flags & IFF_PROMISC) {
      reg_rctl |= (E1000_RCTL_UPE | E1000_RCTL_MPE);
      E1000_WRITE_REG(&Adapter->shared, RCTL, reg_rctl);
   }
   else if (ifp->if_flags & IFF_ALLMULTI) {
      reg_rctl |= E1000_RCTL_MPE;
      reg_rctl &= ~E1000_RCTL_UPE;
      E1000_WRITE_REG(&Adapter->shared, RCTL, reg_rctl);
   }

   return;
}

static void
em_disable_promisc(struct adapter * Adapter)
{
   u_int32_t       reg_rctl;

   reg_rctl = E1000_READ_REG(&Adapter->shared, RCTL);

   reg_rctl &=  (~E1000_RCTL_UPE);
   reg_rctl &=  (~E1000_RCTL_MPE);
   E1000_WRITE_REG(&Adapter->shared, RCTL, reg_rctl);

   return;
}


/*********************************************************************
 *  Multicast Update
 *
 *  This routine is called whenever multicast address list is updated.
 *
 **********************************************************************/

static void
em_set_multi(struct adapter * Adapter)
{
   u_int32_t reg_rctl = 0;
   u_int8_t  mta[MAX_NUM_MULTICAST_ADDRESSES * ETH_LENGTH_OF_ADDRESS];
   u_int16_t PciCommandWord;
   struct ifmultiaddr  *ifma;
   int mcnt = 0;
   struct ifnet   *ifp = &Adapter->interface_data.ac_if;

   IOCTL_DEBUGOUT("em_set_multi: begin");
   
    if(Adapter->shared.mac_type == em_82542_rev2_0) {
       reg_rctl = E1000_READ_REG(&Adapter->shared, RCTL);
       if(Adapter->shared.pci_cmd_word & CMD_MEM_WRT_INVALIDATE) {
          PciCommandWord = Adapter->shared.pci_cmd_word & ~CMD_MEM_WRT_INVALIDATE;
          pci_write_config(Adapter->dev, PCIR_COMMAND, PciCommandWord, 2);
       }
       reg_rctl |= E1000_RCTL_RST;
       E1000_WRITE_REG(&Adapter->shared, RCTL, reg_rctl);
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
       reg_rctl = E1000_READ_REG(&Adapter->shared, RCTL);
       reg_rctl |= E1000_RCTL_MPE;
       E1000_WRITE_REG(&Adapter->shared, RCTL, reg_rctl);
    }
    else
       em_mc_addr_list_update(&Adapter->shared, mta, mcnt, 0);

    if(Adapter->shared.mac_type == em_82542_rev2_0) {
       reg_rctl = E1000_READ_REG(&Adapter->shared, RCTL);
       reg_rctl &= ~E1000_RCTL_RST;
       E1000_WRITE_REG(&Adapter->shared, RCTL, reg_rctl);
       msec_delay(5);
       if(Adapter->shared.pci_cmd_word & CMD_MEM_WRT_INVALIDATE) {
          pci_write_config(Adapter->dev, PCIR_COMMAND, Adapter->shared.pci_cmd_word, 2);
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
   struct adapter * Adapter;
   Adapter = ifp->if_softc;
 
   /* If we are in this routine because of pause frames, then
    * don't reset the hardware.
    */
   if(E1000_READ_REG(&Adapter->shared, STATUS) & E1000_STATUS_TXOFF) {
      ifp->if_timer = EM_TX_TIMEOUT;
      return;
   }

   printf("em%d: watchdog timeout -- resetting\n", Adapter->unit);

   ifp->if_flags &= ~IFF_RUNNING;

   em_stop(Adapter);
   em_init(Adapter);

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
   struct adapter * Adapter = arg;
   ifp = &Adapter->interface_data.ac_if;

   s = splimp();

   em_check_for_link(&Adapter->shared);
   em_print_link_status(Adapter);
   em_update_stats_counters(Adapter);   
   if(em_display_debug_stats && ifp->if_flags & IFF_RUNNING) {
      em_print_hw_stats(Adapter);
   }
   Adapter->timer_handle = timeout(em_local_timer, Adapter, 2*hz);

   splx(s);
   return;
}

static void
em_print_link_status(struct adapter * Adapter)
{
   if(E1000_READ_REG(&Adapter->shared, STATUS) & E1000_STATUS_LU) {
      if(Adapter->LinkIsActive == 0) {
         em_get_speed_and_duplex(&Adapter->shared, &Adapter->LineSpeed, &Adapter->FullDuplex);
         printf("em%d: Link is up %d Mbps %s\n",
                Adapter->unit,
                Adapter->LineSpeed,
                ((Adapter->FullDuplex == FULL_DUPLEX) ?
                 "Full Duplex" : "Half Duplex"));
         Adapter->LinkIsActive = 1;
      }
   } else {
      if(Adapter->LinkIsActive == 1) {
         Adapter->LineSpeed = 0;
         Adapter->FullDuplex = 0;
         printf("em%d: Link is Down\n", Adapter->unit);
         Adapter->LinkIsActive = 0;
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
   struct adapter * Adapter = arg;

   INIT_DEBUGOUT("em_init: begin");

   s = splimp();

   em_stop(Adapter);

   /* Initialize the hardware */
   if (em_hardware_init(Adapter)) {
      printf("em%d: Unable to initialize the hardware\n", Adapter->unit);
      splx(s);
      return;
   }
   Adapter->shared.adapter_stopped = FALSE;
   
   /* Prepare transmit descriptors and buffers */
   if (em_setup_transmit_structures(Adapter)) {
      printf("em%d: Could not setup transmit structures\n", Adapter->unit);
      em_stop(Adapter); 
      splx(s);
      return;
   }
   em_initialize_transmit_unit(Adapter);

   /* Setup Multicast table */
   em_set_multi(Adapter);

   /* Prepare receive descriptors and buffers */
   if (em_setup_receive_structures(Adapter)) {
      printf("em%d: Could not setup receive structures\n", Adapter->unit);
      em_stop(Adapter);
      splx(s);
      return;
   }
   em_initialize_receive_unit(Adapter);

   ifp = &Adapter->interface_data.ac_if;
   ifp->if_flags |= IFF_RUNNING;
   ifp->if_flags &= ~IFF_OACTIVE;

   if(Adapter->shared.mac_type >= em_82543)
      ifp->if_hwassist = EM_CHECKSUM_FEATURES;

   Adapter->timer_handle = timeout(em_local_timer, Adapter, 2*hz);
   em_clear_hw_cntrs(&Adapter->shared);
   EnableInterrupts(Adapter);

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
   struct adapter * Adapter = arg;
   ifp = &Adapter->interface_data.ac_if;

   INIT_DEBUGOUT("em_stop: begin\n");
   DisableInterrupts(Adapter);
   em_adapter_stop(&Adapter->shared);
   untimeout(em_local_timer, Adapter, Adapter->timer_handle);
   em_free_transmit_structures(Adapter);
   em_free_receive_structures(Adapter);


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
   u_int32_t            ProcessCount = EM_MAX_INTR;
   u_int32_t            IcrContents;
   struct ifnet   *ifp;
   struct adapter *Adapter = arg;

   ifp = &Adapter->interface_data.ac_if;

   DisableInterrupts(Adapter);
   while(ProcessCount > 0 && (IcrContents = E1000_READ_REG(&Adapter->shared, ICR)) != 0) {

      /* Link status change */
      if(IcrContents & (E1000_ICR_RXSEQ | E1000_ICR_LSC)) {
         untimeout(em_local_timer, Adapter, Adapter->timer_handle);
         Adapter->shared.get_link_status = 1;
         em_check_for_link(&Adapter->shared);
         em_print_link_status(Adapter);
         Adapter->timer_handle = timeout(em_local_timer, Adapter, 2*hz);      
      }

      if (ifp->if_flags & IFF_RUNNING) {
         em_process_receive_interrupts(Adapter);
         em_clean_transmit_interrupts(Adapter);
      }
      ProcessCount--;
   }

   EnableInterrupts(Adapter);

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
   struct adapter * Adapter = ifp->if_softc;

   INIT_DEBUGOUT("em_media_status: begin");

   em_check_for_link(&Adapter->shared);
   if(E1000_READ_REG(&Adapter->shared, STATUS) & E1000_STATUS_LU) {
      if(Adapter->LinkIsActive == 0) {
         em_get_speed_and_duplex(&Adapter->shared, &Adapter->LineSpeed, &Adapter->FullDuplex);
         Adapter->LinkIsActive = 1;
      }
   }
   else {
      if(Adapter->LinkIsActive == 1) {
         Adapter->LineSpeed = 0;
         Adapter->FullDuplex = 0;
         Adapter->LinkIsActive = 0;
      }
   }

   ifmr->ifm_status = IFM_AVALID;
   ifmr->ifm_active = IFM_ETHER;

   if (!Adapter->LinkIsActive)
      return;

   ifmr->ifm_status |= IFM_ACTIVE;

   if (Adapter->shared.media_type == em_media_type_fiber) {
      ifmr->ifm_active |= IFM_1000_SX | IFM_FDX;
   } else {
      switch (Adapter->LineSpeed) {
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
      if (Adapter->FullDuplex == FULL_DUPLEX)
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
   struct adapter * Adapter = ifp->if_softc;
   struct ifmedia  *ifm = &Adapter->media;
   
   INIT_DEBUGOUT("em_media_change: begin");

   if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
      return(EINVAL);

   switch(IFM_SUBTYPE(ifm->ifm_media)) {
   case IFM_AUTO:
      if (Adapter->shared.autoneg)
         return 0;
      else {
         Adapter->shared.autoneg = DO_AUTO_NEG;
         Adapter->shared.autoneg_advertised = AUTONEG_ADV_DEFAULT;
      }
      break;
   case IFM_1000_SX:
   case IFM_1000_TX:
      Adapter->shared.autoneg = DO_AUTO_NEG;
      Adapter->shared.autoneg_advertised = ADVERTISE_1000_FULL;
      break;
   case IFM_100_TX:
      Adapter->shared.autoneg = FALSE;
      Adapter->shared.autoneg_advertised = 0;
      if ((ifm->ifm_media & IFM_GMASK) == IFM_FDX)
         Adapter->shared.forced_speed_duplex = em_100_full;
      else
         Adapter->shared.forced_speed_duplex = em_100_half;
      break;
   case IFM_10_T:
     Adapter->shared.autoneg = FALSE;
     Adapter->shared.autoneg_advertised = 0;
     if ((ifm->ifm_media & IFM_GMASK) == IFM_FDX)
        Adapter->shared.forced_speed_duplex = em_10_full;
     else
        Adapter->shared.forced_speed_duplex = em_10_half;
     break;
   default:
      printf("em%d: Unsupported media type\n", Adapter->unit);
   }

   em_init(Adapter);

   return(0);
}
/* Section end: Other registered entry points */


/*********************************************************************
 *
 *  Determine hardware revision.
 *
 **********************************************************************/
static void
em_identify_hardware(struct adapter * Adapter)
{
   device_t dev = Adapter->dev;

   /* Make sure our PCI config space has the necessary stuff set */
   Adapter->shared.pci_cmd_word = pci_read_config(dev, PCIR_COMMAND, 2);
   if (!((Adapter->shared.pci_cmd_word & PCIM_CMD_BUSMASTEREN) &&
         (Adapter->shared.pci_cmd_word & PCIM_CMD_MEMEN))) {
      printf("em%d: Memory Access and/or Bus Master bits were not set!\n", 
             Adapter->unit);
      Adapter->shared.pci_cmd_word |= (PCIM_CMD_BUSMASTEREN | PCIM_CMD_MEMEN);
      pci_write_config(dev, PCIR_COMMAND, Adapter->shared.pci_cmd_word, 2);
   }

   /* Save off the information about this board */
   Adapter->VendorId = pci_get_vendor(dev);
   Adapter->DeviceId = pci_get_device(dev);
   Adapter->RevId = pci_read_config(dev, PCIR_REVID, 1);
   Adapter->SubVendorId = pci_read_config(dev, PCIR_SUBVEND_0, 2);
   Adapter->SubSystemId = pci_read_config(dev, PCIR_SUBDEV_0, 2);

   INIT_DEBUGOUT2("device id = 0x%x, Revid = 0x%x", Adapter->DeviceId, Adapter->RevId);
   
   /* Set MacType, etc. based on this PCI info */
   switch (Adapter->DeviceId) {
   case PCI_DEVICE_ID_82542:
      Adapter->shared.mac_type = (Adapter->RevId == 3) ?
         em_82542_rev2_1 : em_82542_rev2_0;
      break;
   case PCI_DEVICE_ID_82543GC_FIBER:
   case PCI_DEVICE_ID_82543GC_COPPER:
      Adapter->shared.mac_type = em_82543;
      break;
   case PCI_DEVICE_ID_82544EI_FIBER:
   case PCI_DEVICE_ID_82544EI_COPPER:
   case PCI_DEVICE_ID_82544GC_COPPER:
   case PCI_DEVICE_ID_82544GC_STRG:
      Adapter->shared.mac_type = em_82544;
      break;
   default:
      INIT_DEBUGOUT1("Unknown device id 0x%x", Adapter->DeviceId);
   }
   return;
}

static int
em_allocate_pci_resources(struct adapter * Adapter)
{
   int             resource_id = EM_MMBA;
   device_t        dev = Adapter->dev;

   Adapter->res_memory = bus_alloc_resource(dev, SYS_RES_MEMORY,
                                            &resource_id, 0, ~0, 1,
                                            RF_ACTIVE);
   if (!(Adapter->res_memory)) {
      printf("em%d: Unable to allocate bus resource: memory\n", Adapter->unit);
      return(ENXIO);
   }
   Adapter->osdep.bus_space_tag = rman_get_bustag(Adapter->res_memory);
   Adapter->osdep.bus_space_handle = rman_get_bushandle(Adapter->res_memory);
   Adapter->shared.hw_addr = (uint8_t *)Adapter->osdep.bus_space_handle;

   resource_id = 0x0;
   Adapter->res_interrupt = bus_alloc_resource(dev, SYS_RES_IRQ,
                                               &resource_id, 0, ~0, 1,
                                               RF_SHAREABLE | RF_ACTIVE);
   if (!(Adapter->res_interrupt)) {
      printf("em%d: Unable to allocate bus resource: interrupt\n", Adapter->unit);
      return(ENXIO);
   }
   if (bus_setup_intr(dev, Adapter->res_interrupt, INTR_TYPE_NET,
                  (void (*)(void *)) em_intr, Adapter,
                  &Adapter->int_handler_tag)) {
      printf("em%d: Error registering interrupt handler!\n", Adapter->unit);
      return(ENXIO);
   }

   Adapter->shared.back = &Adapter->osdep;

   return(0);
}

static void
em_free_pci_resources(struct adapter * Adapter)
{
   device_t dev = Adapter->dev;

   if(Adapter->res_interrupt != NULL) {
      bus_teardown_intr(dev, Adapter->res_interrupt, Adapter->int_handler_tag);
      bus_release_resource(dev, SYS_RES_IRQ, 0, Adapter->res_interrupt);
   }
   if (Adapter->res_memory != NULL) {
      bus_release_resource(dev, SYS_RES_MEMORY, EM_MMBA, Adapter->res_memory);
   }
   return;
}

/*********************************************************************
 *
 *  Initialize the hardware to a configuration as specified by the
 *  Adapter structure. The controller is reset, the EEPROM is
 *  verified, the MAC address is set, then the shared initialization
 *  routines are called.
 *
 **********************************************************************/
static int
em_hardware_init(struct adapter * Adapter)
{
   /* Issue a global reset */
   Adapter->shared.adapter_stopped = FALSE;
   em_adapter_stop(&Adapter->shared);
   Adapter->shared.adapter_stopped = FALSE;

   /* Make sure we have a good EEPROM before we read from it */
   if (!em_validate_eeprom_checksum(&Adapter->shared)) {
      printf("em%d: The EEPROM Checksum Is Not Valid\n", Adapter->unit);
      return EIO;
   }
   /* Copy the permanent MAC address and part number out of the EEPROM */
   em_read_mac_address(Adapter, Adapter->interface_data.ac_enaddr);
   memcpy(Adapter->shared.mac_addr, Adapter->interface_data.ac_enaddr,
         ETH_LENGTH_OF_ADDRESS);
   em_read_part_num(&Adapter->shared, &(Adapter->PartNumber));

   if (!em_init_hw(&Adapter->shared)) {
      printf("em%d: Hardware Initialization Failed", Adapter->unit);
      return EIO;
   }

   em_check_for_link(&Adapter->shared);
   if (E1000_READ_REG(&Adapter->shared, STATUS) & E1000_STATUS_LU)
      Adapter->LinkIsActive = 1;
   else
      Adapter->LinkIsActive = 0;
 
   if (Adapter->LinkIsActive) {
      em_get_speed_and_duplex(&Adapter->shared, &Adapter->LineSpeed, &Adapter->FullDuplex);
   } else {
      Adapter->LineSpeed = 0;
      Adapter->FullDuplex = 0;
   }

   return 0;
}

static void
em_read_mac_address(struct adapter * Adapter, u_int8_t * NodeAddress)
{
   u_int16_t       EepromWordValue;
   int             i;

   for (i = 0; i < NODE_ADDRESS_SIZE; i += 2) {
      EepromWordValue =
         em_read_eeprom(&Adapter->shared, EEPROM_NODE_ADDRESS_BYTE_0 + (i / 2));
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
em_setup_interface(device_t dev, struct adapter * Adapter)
{
   struct ifnet   *ifp;
   INIT_DEBUGOUT("em_setup_interface: begin");

   ifp = &Adapter->interface_data.ac_if;
   ifp->if_unit = Adapter->unit;
   ifp->if_name = "em";
   ifp->if_mtu = ETHERMTU;
   ifp->if_output = ether_output;
   ifp->if_baudrate = 1000000000;
   ifp->if_init =  em_init;
   ifp->if_softc = Adapter;
   ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
   ifp->if_ioctl = em_ioctl;
   ifp->if_start = em_start;
   ifp->if_watchdog = em_watchdog;
   ifp->if_snd.ifq_maxlen = Adapter->NumTxDescriptors - 1;
   ether_ifattach(ifp, ETHER_BPF_SUPPORTED);

   /* 
    * Specify the media types supported by this adapter and register
    * callbacks to update media and link information
    */
   ifmedia_init(&Adapter->media, IFM_IMASK, em_media_change,
                em_media_status);
   if (Adapter->shared.media_type == em_media_type_fiber) {
      ifmedia_add(&Adapter->media, IFM_ETHER | IFM_1000_SX | IFM_FDX, 0,
                  NULL);
      ifmedia_add(&Adapter->media, IFM_ETHER | IFM_1000_SX , 0, NULL);
   } else {
      ifmedia_add(&Adapter->media, IFM_ETHER | IFM_10_T, 0, NULL);
      ifmedia_add(&Adapter->media, IFM_ETHER | IFM_10_T | IFM_FDX, 0,
                  NULL);
      ifmedia_add(&Adapter->media, IFM_ETHER | IFM_100_TX, 0, NULL);
      ifmedia_add(&Adapter->media, IFM_ETHER | IFM_100_TX | IFM_FDX, 0,
                  NULL);
      ifmedia_add(&Adapter->media, IFM_ETHER | IFM_1000_TX | IFM_FDX, 0,
                  NULL);
      ifmedia_add(&Adapter->media, IFM_ETHER | IFM_1000_TX, 0, NULL);
   }
   ifmedia_add(&Adapter->media, IFM_ETHER | IFM_AUTO, 0, NULL);
   ifmedia_set(&Adapter->media, IFM_ETHER | IFM_AUTO);

   INIT_DEBUGOUT("em_setup_interface: end");
   return;
}


/*********************************************************************
 *
 *  Allocate memory for tx_buffer structures. The tx_buffer stores all 
 *  the information needed to transmit a packet on the wire. 
 *
 **********************************************************************/
static int
em_allocate_transmit_structures(struct adapter * Adapter)
{
   if (!(Adapter->tx_buffer_area =
         (struct em_tx_buffer *) malloc(sizeof(struct em_tx_buffer) *
                                        Adapter->NumTxDescriptors, M_DEVBUF,
                                        M_NOWAIT))) {
      printf("em%d: Unable to allocate tx_buffer memory\n", Adapter->unit);
      return ENOMEM;
   }

   bzero(Adapter->tx_buffer_area,
         sizeof(struct em_tx_buffer) * Adapter->NumTxDescriptors);

   return 0;
}

/*********************************************************************
 *
 *  Allocate and initialize transmit structures. 
 *
 **********************************************************************/
static int
em_setup_transmit_structures(struct adapter * Adapter)
{
   struct em_tx_buffer   *tx_buffer;
   int             i;

   if (em_allocate_transmit_structures(Adapter))
      return ENOMEM;

   Adapter->FirstTxDescriptor = Adapter->TxDescBase;
   Adapter->LastTxDescriptor =
      Adapter->FirstTxDescriptor + (Adapter->NumTxDescriptors - 1);

   
   STAILQ_INIT(&Adapter->FreeSwTxPacketList);
   STAILQ_INIT(&Adapter->UsedSwTxPacketList);

   tx_buffer = Adapter->tx_buffer_area;

   /* Setup the linked list of the tx_buffer's */
   for (i = 0; i < Adapter->NumTxDescriptors; i++, tx_buffer++) {
      bzero((void *) tx_buffer, sizeof(struct em_tx_buffer));
      STAILQ_INSERT_TAIL(&Adapter->FreeSwTxPacketList, tx_buffer, em_tx_entry);
   }

   bzero((void *) Adapter->FirstTxDescriptor,
        (sizeof(struct em_tx_desc)) * Adapter->NumTxDescriptors);

   /* Setup TX descriptor pointers */
   Adapter->NextAvailTxDescriptor = Adapter->FirstTxDescriptor;
   Adapter->OldestUsedTxDescriptor = Adapter->FirstTxDescriptor;

   /* Set number of descriptors available */
   Adapter->NumTxDescriptorsAvail = Adapter->NumTxDescriptors;

   /* Set checksum context */
   Adapter->ActiveChecksumContext = OFFLOAD_NONE;

   return 0;
}

/*********************************************************************
 *
 *  Enable transmit unit.
 *
 **********************************************************************/
static void
em_initialize_transmit_unit(struct adapter * Adapter)
{
   u_int32_t       reg_tctl;
   u_int32_t       reg_tipg = 0;

   /* Setup the Base and Length of the Tx Descriptor Ring */
   E1000_WRITE_REG(&Adapter->shared, TDBAL, vtophys((vm_offset_t) Adapter->TxDescBase));
   E1000_WRITE_REG(&Adapter->shared, TDBAH, 0);
   E1000_WRITE_REG(&Adapter->shared, TDLEN, Adapter->NumTxDescriptors *
               sizeof(struct em_tx_desc));

   /* Setup the HW Tx Head and Tail descriptor pointers */
   E1000_WRITE_REG(&Adapter->shared, TDH, 0);
   E1000_WRITE_REG(&Adapter->shared, TDT, 0);


   HW_DEBUGOUT2("Base = %x, Length = %x\n", E1000_READ_REG(&Adapter->shared, TDBAL),
                E1000_READ_REG(&Adapter->shared, TDLEN));

   
   /* Set the default values for the Tx Inter Packet Gap timer */
   switch (Adapter->shared.mac_type) {
   case em_82543:
   case em_82544:
      if (Adapter->shared.media_type == em_media_type_fiber)
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
      printf("em%d: Invalid mac type detected\n", Adapter->unit);
   }
   E1000_WRITE_REG(&Adapter->shared, TIPG, reg_tipg);
   E1000_WRITE_REG(&Adapter->shared, TIDV, Adapter->TxIntDelay);

   /* Program the Transmit Control Register */
   reg_tctl = E1000_TCTL_PSP | E1000_TCTL_EN |
      (E1000_COLLISION_THRESHOLD << E1000_CT_SHIFT);
   if (Adapter->FullDuplex == 1) {
      reg_tctl |= E1000_FDX_COLLISION_DISTANCE << E1000_COLD_SHIFT;
   } else {
      reg_tctl |= E1000_HDX_COLLISION_DISTANCE << E1000_COLD_SHIFT;
   }
   E1000_WRITE_REG(&Adapter->shared, TCTL, reg_tctl);

   /* Setup Transmit Descriptor Settings for this adapter */   
   Adapter->TxdCmd = E1000_TXD_CMD_IFCS;

   if(Adapter->TxIntDelay > 0)
      Adapter->TxdCmd |= E1000_TXD_CMD_IDE;
   
   if(Adapter->shared.report_tx_early == 1)
      Adapter->TxdCmd |= E1000_TXD_CMD_RS;
   else
      Adapter->TxdCmd |= E1000_TXD_CMD_RPS;

   return;
}

/*********************************************************************
 *
 *  Free all transmit related data structures.
 *
 **********************************************************************/
static void
em_free_transmit_structures(struct adapter * Adapter)
{
   struct em_tx_buffer   *tx_buffer;
   int             i;

   INIT_DEBUGOUT("free_transmit_structures: begin");

   if (Adapter->tx_buffer_area != NULL) {
      tx_buffer = Adapter->tx_buffer_area;
      for (i = 0; i < Adapter->NumTxDescriptors; i++, tx_buffer++) {
         if (tx_buffer->Packet != NULL)
            m_freem(tx_buffer->Packet);
         tx_buffer->Packet = NULL;
      }
   }
   if (Adapter->tx_buffer_area != NULL) {
      free(Adapter->tx_buffer_area, M_DEVBUF);
      Adapter->tx_buffer_area = NULL;
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
em_transmit_checksum_setup(struct adapter * Adapter,
                struct mbuf *mp,
                struct em_tx_buffer *tx_buffer,
                u_int32_t *txd_upper,
                u_int32_t *txd_lower) 
{
   struct em_context_desc *TXD;
   struct em_tx_desc * CurrentTxDescriptor;
     
   if (mp->m_pkthdr.csum_flags) {

      if(mp->m_pkthdr.csum_flags & CSUM_TCP) {
         TXCSUM_DEBUGOUT("Checksum TCP");
         *txd_upper = E1000_TXD_POPTS_TXSM << 8;
         *txd_lower = E1000_TXD_CMD_DEXT | E1000_TXD_DTYP_D;
         if(Adapter->ActiveChecksumContext == OFFLOAD_TCP_IP)
            return;
         else 
            Adapter->ActiveChecksumContext = OFFLOAD_TCP_IP;

      } else if(mp->m_pkthdr.csum_flags & CSUM_UDP) {
         TXCSUM_DEBUGOUT("Checksum UDP");
         *txd_upper = E1000_TXD_POPTS_TXSM << 8;
         *txd_lower = E1000_TXD_CMD_DEXT | E1000_TXD_DTYP_D;
         if(Adapter->ActiveChecksumContext == OFFLOAD_UDP_IP)
            return;
         else 
            Adapter->ActiveChecksumContext = OFFLOAD_UDP_IP;
      } else {
         TXCSUM_DEBUGOUT("Invalid protocol for checksum calculation\n");
         *txd_upper = 0;
         *txd_lower = 0;
         return;
      }
   }
   else {
      TXCSUM_DEBUGOUT("No checksum detected\n");
      *txd_upper = 0;
      *txd_lower = 0;
      return;
   }

   /* If we reach this point, the checksum offload context
    * needs to be reset.
    */
   CurrentTxDescriptor = Adapter->NextAvailTxDescriptor;
   TXD = (struct em_context_desc *)CurrentTxDescriptor;

   TXD->lower_setup.ip_fields.ipcss = ETHER_HDR_LEN;
   TXD->lower_setup.ip_fields.ipcso = ETHER_HDR_LEN + offsetof(struct ip, ip_sum);
   TXD->lower_setup.ip_fields.ipcse = ETHER_HDR_LEN + sizeof(struct ip) - 1;

   TXD->upper_setup.tcp_fields.tucss = ETHER_HDR_LEN + sizeof(struct ip);
   TXD->upper_setup.tcp_fields.tucse = 0;

   if(Adapter->ActiveChecksumContext == OFFLOAD_TCP_IP) {
      TXD->upper_setup.tcp_fields.tucso = ETHER_HDR_LEN + sizeof(struct ip) + 
         offsetof(struct tcphdr, th_sum);
   } else if (Adapter->ActiveChecksumContext == OFFLOAD_UDP_IP) {
      TXD->upper_setup.tcp_fields.tucso = ETHER_HDR_LEN + sizeof(struct ip) + 
         offsetof(struct udphdr, uh_sum);
   }

   TXD->tcp_seg_setup.data = 0;
   TXD->cmd_and_length = E1000_TXD_CMD_DEXT;

   if (CurrentTxDescriptor == Adapter->LastTxDescriptor)
      Adapter->NextAvailTxDescriptor = Adapter->FirstTxDescriptor;
   else
      Adapter->NextAvailTxDescriptor++;

   Adapter->NumTxDescriptorsAvail--;
    
   tx_buffer->NumTxDescriptorsUsed++;
   return;
}


/*********************************************************************
 *
 *  Get a buffer from system mbuf buffer pool.
 *
 **********************************************************************/
static int
em_get_buf(struct em_rx_buffer *rx_buffer, struct adapter *Adapter,
           struct mbuf *mp)
{
   struct mbuf    *nmp;
   struct ifnet   *ifp;

   ifp = &Adapter->interface_data.ac_if;

   if (mp == NULL) {
      MGETHDR(nmp, M_DONTWAIT, MT_DATA);
      if (nmp == NULL) {
         printf("em%d: Mbuf allocation failed\n", Adapter->unit);
         Adapter->StdMbufFailed++;
         return (ENOBUFS);
      }
      MCLGET(nmp, M_DONTWAIT);
      if ((nmp->m_flags & M_EXT) == 0) {
         m_freem(nmp);
         printf("em%d: Mbuf cluster allocation failed\n", Adapter->unit);
         Adapter->StdClusterFailed++;
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

   rx_buffer->Packet = nmp;
   rx_buffer->LowPhysicalAddress = vtophys(mtod(nmp, vm_offset_t));
   rx_buffer->HighPhysicalAddress = 0;

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
em_allocate_receive_structures(struct adapter * Adapter)
{
   int             i;
   struct em_rx_buffer   *rx_buffer;

   if (!(Adapter->rx_buffer_area =
        (struct em_rx_buffer *) malloc(sizeof(struct em_rx_buffer) *
                                       Adapter->NumRxDescriptors, M_DEVBUF,
                                       M_NOWAIT))) {
      printf("em%d: Unable to allocate rx_buffer memory\n", Adapter->unit);
      return (ENOMEM);
   }

   bzero(Adapter->rx_buffer_area,
         sizeof(struct em_rx_buffer) * Adapter->NumRxDescriptors);

   for (i = 0, rx_buffer = Adapter->rx_buffer_area;
       i < Adapter->NumRxDescriptors; i++, rx_buffer++) {

      if (em_get_buf(rx_buffer, Adapter, NULL) == ENOBUFS) {
         rx_buffer->Packet = NULL;
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
em_setup_receive_structures(struct adapter * Adapter)
{
   struct em_rx_buffer   *rx_buffer;
   struct em_rx_desc * RxDescriptorPtr;
   int             i;

   if(em_allocate_receive_structures(Adapter))
      return ENOMEM;

   STAILQ_INIT(&Adapter->RxSwPacketList);

   Adapter->FirstRxDescriptor =
      (struct em_rx_desc *) Adapter->RxDescBase;
   Adapter->LastRxDescriptor =
      Adapter->FirstRxDescriptor + (Adapter->NumRxDescriptors - 1);

   rx_buffer = (struct em_rx_buffer *) Adapter->rx_buffer_area;

   bzero((void *) Adapter->FirstRxDescriptor,
        (sizeof(struct em_rx_desc)) * Adapter->NumRxDescriptors);

   /* Build a linked list of rx_buffer's */
   for (i = 0, RxDescriptorPtr = Adapter->FirstRxDescriptor;
       i < Adapter->NumRxDescriptors;
       i++, rx_buffer++, RxDescriptorPtr++) {
      if (rx_buffer->Packet == NULL)
         printf("em%d: Receive buffer memory not allocated", Adapter->unit);
      else {
         RxDescriptorPtr->buffer_addr = rx_buffer->LowPhysicalAddress;
         STAILQ_INSERT_TAIL(&Adapter->RxSwPacketList, rx_buffer, em_rx_entry);
      }
   }

   /* Setup our descriptor pointers */
   Adapter->NextRxDescriptorToCheck = Adapter->FirstRxDescriptor;

   return(0);
}

/*********************************************************************
 *
 *  Enable receive unit.
 *  
 **********************************************************************/
static void
em_initialize_receive_unit(struct adapter * Adapter)
{
   u_int32_t       reg_rctl;
   u_int32_t       reg_rxcsum;

   /* Make sure receives are disabled while setting up the descriptor ring */
   E1000_WRITE_REG(&Adapter->shared, RCTL, 0);

   /* Set the Receive Delay Timer Register */
   E1000_WRITE_REG(&Adapter->shared, RDTR, Adapter->RxIntDelay | E1000_RDT_FPDB);

   /* Setup the Base and Length of the Rx Descriptor Ring */
   E1000_WRITE_REG(&Adapter->shared, RDBAL, vtophys((vm_offset_t) Adapter->RxDescBase));
   E1000_WRITE_REG(&Adapter->shared, RDBAH, 0);
   E1000_WRITE_REG(&Adapter->shared, RDLEN, Adapter->NumRxDescriptors *
               sizeof(struct em_rx_desc));

   /* Setup the HW Rx Head and Tail Descriptor Pointers */
   E1000_WRITE_REG(&Adapter->shared, RDH, 0);
   E1000_WRITE_REG(&Adapter->shared, RDT,
               (((u_int32_t) Adapter->LastRxDescriptor -
                 (u_int32_t) Adapter->FirstRxDescriptor) >> 4));
   
   /* Setup the Receive Control Register */
   reg_rctl = E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_LBM_NO |
      E1000_RCTL_RDMTS_HALF |
      (Adapter->shared.mc_filter_type << E1000_RCTL_MO_SHIFT);

   if (Adapter->shared.tbi_compatibility_on == TRUE)
      reg_rctl |= E1000_RCTL_SBP;


   switch (Adapter->RxBufferLen) {
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
   if((Adapter->shared.mac_type >= em_82543) && (Adapter->RxChecksum == 1)) {
      reg_rxcsum = E1000_READ_REG(&Adapter->shared, RXCSUM);
      reg_rxcsum |= (E1000_RXCSUM_IPOFL | E1000_RXCSUM_TUOFL);
      E1000_WRITE_REG(&Adapter->shared, RXCSUM, reg_rxcsum);
   }

   /* Enable Receives */
   E1000_WRITE_REG(&Adapter->shared, RCTL, reg_rctl);

   return;
}

/*********************************************************************
 *
 *  Free receive related data structures.
 *
 **********************************************************************/
static void
em_free_receive_structures(struct adapter * Adapter)
{
   struct em_rx_buffer   *rx_buffer;
   int             i;

   INIT_DEBUGOUT("free_receive_structures: begin");

   if (Adapter->rx_buffer_area != NULL) {
      rx_buffer = Adapter->rx_buffer_area;
      for (i = 0; i < Adapter->NumRxDescriptors; i++, rx_buffer++) {
         if (rx_buffer->Packet != NULL)
            m_freem(rx_buffer->Packet);
         rx_buffer->Packet = NULL;
      }
   }
   if (Adapter->rx_buffer_area != NULL) {
      free(Adapter->rx_buffer_area, M_DEVBUF);
      Adapter->rx_buffer_area = NULL;
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
em_process_receive_interrupts(struct adapter * Adapter)
{
   struct mbuf         *mp;
   struct ifnet        *ifp;
   struct ether_header *eh;
   u_int16_t           Length;
   u_int8_t            LastByte;
   u_int8_t            AcceptFrame = 0;
   u_int8_t            EndOfPacket = 0;
   u_int32_t           PacketLength = 0;

   /* Pointer to the receive descriptor being examined. */
   struct em_rx_desc * CurrentDescriptor;
   struct em_rx_desc * LastDescriptorProcessed;
   struct em_rx_buffer   *rx_buffer;

   TXRX_DEBUGOUT("em_process_receive_interrupts: begin");

   ifp = &Adapter->interface_data.ac_if;
   CurrentDescriptor = Adapter->NextRxDescriptorToCheck;

   if (!((CurrentDescriptor->status) & E1000_RXD_STAT_DD)) {
#ifdef DBG_STATS
      Adapter->NoPacketsAvail++;
#endif
      return;
   }
   
   while (CurrentDescriptor->status & E1000_RXD_STAT_DD) {

      /* Get a pointer to the actual receive buffer */
      rx_buffer = STAILQ_FIRST(&Adapter->RxSwPacketList);

      if(rx_buffer == NULL) {
         printf("em%d: Found null rx_buffer\n", Adapter->unit);
         return;
      }

      mp = rx_buffer->Packet;      
      AcceptFrame = 1;

      if (CurrentDescriptor->status & E1000_RXD_STAT_EOP) {
         EndOfPacket = 1;
         Length = CurrentDescriptor->length - ETHER_CRC_LEN;
      }
      else {
         EndOfPacket = 0;
         Length = CurrentDescriptor->length;
      }

      if(CurrentDescriptor->errors & E1000_RXD_ERR_FRAME_ERR_MASK) {
         
         /* Compute packet length for tbi_accept macro */
         PacketLength = CurrentDescriptor->length;
         if (Adapter->fmp != NULL) {
            PacketLength += Adapter->fmp->m_pkthdr.len; 
         }

         LastByte = *(mtod(rx_buffer->Packet,caddr_t) + CurrentDescriptor->length - 1);

         if (TBI_ACCEPT(&Adapter->shared, 0, CurrentDescriptor->errors, 
                        PacketLength, LastByte)) {  
            PacketLength = em_tbi_adjust_stats(&Adapter->shared, &Adapter->stats, 
                                               PacketLength, Adapter->shared.mac_addr);
            Length--;
         } else {  
            AcceptFrame = 0;
         }
      }

      if (AcceptFrame) {

         if (em_get_buf(rx_buffer, Adapter, NULL) == ENOBUFS) {
            Adapter->DroppedPackets++;
            em_get_buf(rx_buffer, Adapter, mp);
            if(Adapter->fmp != NULL) m_freem(Adapter->fmp);
            Adapter->fmp = NULL;
            Adapter->lmp = NULL;
            break;
         }

         /* Assign correct length to the current fragment */
         mp->m_len = Length;

         if(Adapter->fmp == NULL) {
            mp->m_pkthdr.len = Length;
            Adapter->fmp = mp;       /* Store the first mbuf */
            Adapter->lmp = mp;
         }
         else {
            /* Chain mbuf's together */
            mp->m_flags &= ~M_PKTHDR;
            Adapter->lmp->m_next = mp;
            Adapter->lmp = Adapter->lmp->m_next;
            Adapter->fmp->m_pkthdr.len += Length;
         }

         if (EndOfPacket) {
            Adapter->fmp->m_pkthdr.rcvif = ifp;

            eh = mtod(Adapter->fmp, struct ether_header *);

            /* Remove ethernet header from mbuf */
            m_adj(Adapter->fmp, sizeof(struct ether_header));
            em_receive_checksum(Adapter, CurrentDescriptor, Adapter->fmp);
            ether_input(ifp, eh, Adapter->fmp);
      
            Adapter->fmp = NULL;
            Adapter->lmp = NULL;
         }
      } else {
         Adapter->DroppedPackets++;
         em_get_buf(rx_buffer, Adapter, mp);
         if(Adapter->fmp != NULL) m_freem(Adapter->fmp);
         Adapter->fmp = NULL;
         Adapter->lmp = NULL;
      }
      
      /* Zero out the receive descriptors status  */
      CurrentDescriptor->status = 0;
      
      if (rx_buffer->Packet != NULL) {
         CurrentDescriptor->buffer_addr = rx_buffer->LowPhysicalAddress;
      }

      /* Advance our pointers to the next descriptor (checking for wrap). */
      if (CurrentDescriptor == Adapter->LastRxDescriptor)
         Adapter->NextRxDescriptorToCheck = Adapter->FirstRxDescriptor;
      else
         ((Adapter)->NextRxDescriptorToCheck)++;

      LastDescriptorProcessed = CurrentDescriptor;
      CurrentDescriptor = Adapter->NextRxDescriptorToCheck;
      /* 
       * Put the buffer that we just indicated back at the end of our list
       */
      STAILQ_REMOVE_HEAD(&Adapter->RxSwPacketList, em_rx_entry);
      STAILQ_INSERT_TAIL(&Adapter->RxSwPacketList, rx_buffer, em_rx_entry);

      /* Advance the E1000's Receive Queue #0  "Tail Pointer". */
      E1000_WRITE_REG(&Adapter->shared, RDT, (((u_int32_t) LastDescriptorProcessed -
                        (u_int32_t) Adapter->FirstRxDescriptor) >> 4));
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
em_receive_checksum(struct adapter * Adapter,
           struct em_rx_desc * RxDescriptor,
           struct mbuf *mp)
{
   /* 82543 or newer only */
   if((Adapter->shared.mac_type < em_82543) ||
      /* Ignore Checksum bit is set */
      (RxDescriptor->status & E1000_RXD_STAT_IXSM)) {
      RXCSUM_DEBUGOUT("Ignoring checksum");
      mp->m_pkthdr.csum_flags = 0;
      return;
   }

   if (RxDescriptor->status & E1000_RXD_STAT_IPCS) {
      /* Did it pass? */
      if (!(RxDescriptor->errors & E1000_RXD_ERR_IPE)) {
         /* IP Checksum Good */
         RXCSUM_DEBUGOUT("Good IP checksum");
         mp->m_pkthdr.csum_flags = CSUM_IP_CHECKED;
         mp->m_pkthdr.csum_flags |= CSUM_IP_VALID;

      }
      else {
         RXCSUM_DEBUGOUT("Bad IP checksum");
         mp->m_pkthdr.csum_flags = 0;
      }
   }
   else {
      RXCSUM_DEBUGOUT("IP Checksum not verified");
   }

   if (RxDescriptor->status & E1000_RXD_STAT_TCPCS) {
      /* Did it pass? */        
      if (!(RxDescriptor->errors & E1000_RXD_ERR_TCPE)) {
         RXCSUM_DEBUGOUT("Good TCP/UDP checksum");
         mp->m_pkthdr.csum_flags |= (CSUM_DATA_VALID | CSUM_PSEUDO_HDR);
         mp->m_pkthdr.csum_data = htons(0xffff);
      }
      else {
         RXCSUM_DEBUGOUT("Bad TCP/UDP checksum");
      }
   }
   else {
      RXCSUM_DEBUGOUT("TCP/UDP checksum not verified");
   }


   return;
}


static void
EnableInterrupts(struct adapter * Adapter)
{
   E1000_WRITE_REG(&Adapter->shared, IMS, (IMS_ENABLE_MASK));
   return;
}

static void
DisableInterrupts(struct adapter * Adapter)
{
   E1000_WRITE_REG(&Adapter->shared, IMC, (0xffffffff & ~E1000_IMC_RXSEQ));
   return;
}

void em_write_pci_cfg(struct em_shared_adapter *Adapter,
                      uint32_t reg,
                      uint16_t * value)
{
   pci_write_config(((struct em_osdep *)Adapter->back)->dev, reg, *value, 2);
}


/**********************************************************************
 *
 *  Update the board statistics counters. 
 *
 **********************************************************************/
static void
em_update_stats_counters(struct adapter * Adapter)
{
   struct ifnet   *ifp;

   Adapter->stats.crcerrs += E1000_READ_REG(&Adapter->shared, CRCERRS);
   Adapter->stats.symerrs += E1000_READ_REG(&Adapter->shared, SYMERRS);
   Adapter->stats.mpc += E1000_READ_REG(&Adapter->shared, MPC);
   Adapter->stats.scc += E1000_READ_REG(&Adapter->shared, SCC);
   Adapter->stats.ecol += E1000_READ_REG(&Adapter->shared, ECOL);
   Adapter->stats.mcc += E1000_READ_REG(&Adapter->shared, MCC);
   Adapter->stats.latecol += E1000_READ_REG(&Adapter->shared, LATECOL);
   Adapter->stats.colc += E1000_READ_REG(&Adapter->shared, COLC);
   Adapter->stats.dc += E1000_READ_REG(&Adapter->shared, DC);
   Adapter->stats.sec += E1000_READ_REG(&Adapter->shared, SEC);
   Adapter->stats.rlec += E1000_READ_REG(&Adapter->shared, RLEC);
   Adapter->stats.xonrxc += E1000_READ_REG(&Adapter->shared, XONRXC);
   Adapter->stats.xontxc += E1000_READ_REG(&Adapter->shared, XONTXC);
   Adapter->stats.xoffrxc += E1000_READ_REG(&Adapter->shared, XOFFRXC);
   Adapter->stats.xofftxc += E1000_READ_REG(&Adapter->shared, XOFFTXC);
   Adapter->stats.fcruc += E1000_READ_REG(&Adapter->shared, FCRUC);
   Adapter->stats.prc64 += E1000_READ_REG(&Adapter->shared, PRC64);
   Adapter->stats.prc127 += E1000_READ_REG(&Adapter->shared, PRC127);
   Adapter->stats.prc255 += E1000_READ_REG(&Adapter->shared, PRC255);
   Adapter->stats.prc511 += E1000_READ_REG(&Adapter->shared, PRC511);
   Adapter->stats.prc1023 += E1000_READ_REG(&Adapter->shared, PRC1023);
   Adapter->stats.prc1522 += E1000_READ_REG(&Adapter->shared, PRC1522);
   Adapter->stats.gprc += E1000_READ_REG(&Adapter->shared, GPRC);
   Adapter->stats.bprc += E1000_READ_REG(&Adapter->shared, BPRC);
   Adapter->stats.mprc += E1000_READ_REG(&Adapter->shared, MPRC);
   Adapter->stats.gptc += E1000_READ_REG(&Adapter->shared, GPTC);

   /* For the 64-bit byte counters the low dword must be read first. */
   /* Both registers clear on the read of the high dword */

   Adapter->stats.gorcl += E1000_READ_REG(&Adapter->shared, GORCL); 
   Adapter->stats.gorch += E1000_READ_REG(&Adapter->shared, GORCH);
   Adapter->stats.gotcl += E1000_READ_REG(&Adapter->shared, GOTCL);
   Adapter->stats.gotch += E1000_READ_REG(&Adapter->shared, GOTCH);

   Adapter->stats.rnbc += E1000_READ_REG(&Adapter->shared, RNBC);
   Adapter->stats.ruc += E1000_READ_REG(&Adapter->shared, RUC);
   Adapter->stats.rfc += E1000_READ_REG(&Adapter->shared, RFC);
   Adapter->stats.roc += E1000_READ_REG(&Adapter->shared, ROC);
   Adapter->stats.rjc += E1000_READ_REG(&Adapter->shared, RJC);

   Adapter->stats.torl += E1000_READ_REG(&Adapter->shared, TORL);
   Adapter->stats.torh += E1000_READ_REG(&Adapter->shared, TORH);
   Adapter->stats.totl += E1000_READ_REG(&Adapter->shared, TOTL);
   Adapter->stats.toth += E1000_READ_REG(&Adapter->shared, TOTH);

   Adapter->stats.tpr += E1000_READ_REG(&Adapter->shared, TPR);
   Adapter->stats.tpt += E1000_READ_REG(&Adapter->shared, TPT);
   Adapter->stats.ptc64 += E1000_READ_REG(&Adapter->shared, PTC64);
   Adapter->stats.ptc127 += E1000_READ_REG(&Adapter->shared, PTC127);
   Adapter->stats.ptc255 += E1000_READ_REG(&Adapter->shared, PTC255);
   Adapter->stats.ptc511 += E1000_READ_REG(&Adapter->shared, PTC511);
   Adapter->stats.ptc1023 += E1000_READ_REG(&Adapter->shared, PTC1023);
   Adapter->stats.ptc1522 += E1000_READ_REG(&Adapter->shared, PTC1522);
   Adapter->stats.mptc += E1000_READ_REG(&Adapter->shared, MPTC);
   Adapter->stats.bptc += E1000_READ_REG(&Adapter->shared, BPTC);

   if (Adapter->shared.mac_type >= em_82543) {
      Adapter->stats.algnerrc += E1000_READ_REG(&Adapter->shared, ALGNERRC);
      Adapter->stats.rxerrc += E1000_READ_REG(&Adapter->shared, RXERRC);
      Adapter->stats.tncrs += E1000_READ_REG(&Adapter->shared, TNCRS);
      Adapter->stats.cexterr += E1000_READ_REG(&Adapter->shared, CEXTERR);
      Adapter->stats.tsctc += E1000_READ_REG(&Adapter->shared, TSCTC);
      Adapter->stats.tsctfc += E1000_READ_REG(&Adapter->shared, TSCTFC);
   }
   ifp = &Adapter->interface_data.ac_if;

   /* Fill out the OS statistics structure */
   ifp->if_ipackets = Adapter->stats.gprc;
   ifp->if_opackets = Adapter->stats.gptc;
   ifp->if_ibytes = Adapter->stats.gorcl;
   ifp->if_obytes = Adapter->stats.gotcl;
   ifp->if_imcasts = Adapter->stats.mprc;
   ifp->if_collisions = Adapter->stats.colc;

   /* Rx Errors */
   ifp->if_ierrors =
      Adapter->DroppedPackets +
      Adapter->stats.rxerrc +
      Adapter->stats.crcerrs +
      Adapter->stats.algnerrc +
      Adapter->stats.rlec + Adapter->stats.rnbc + 
      Adapter->stats.mpc + Adapter->stats.cexterr;

   /* Tx Errors */
   ifp->if_oerrors = Adapter->stats.ecol + Adapter->stats.latecol;

}


/**********************************************************************
 *
 *  This routine is called only when em_display_debug_stats is enabled.
 *  This routine provides a way to take a look at important statistics
 *  maintained by the driver and hardware.
 *
 **********************************************************************/
static void
em_print_hw_stats(struct adapter * Adapter)
{
   int unit = Adapter->unit;

#ifdef DBG_STATS
   printf("em%d: Tx Descriptors not Avail = %ld\n", unit, Adapter->NoTxDescAvail);
   printf("em%d: Packets not Avail = %ld\n", unit, Adapter->NoPacketsAvail);
   printf("em%d: CleanTxInterrupts = %ld\n", unit, Adapter->CleanTxInterrupts);
   printf("em%d: Tx Buffer not avail1 = %ld\n", unit, Adapter->NoTxBufferAvail1);
   printf("em%d: Tx Buffer not avail2 = %ld\n", unit, Adapter->NoTxBufferAvail2);
#endif
   printf("em%d: No Jumbo Buffer Avail = %ld\n",unit, Adapter->NoJumboBufAvail);
   printf("em%d: Jumbo Mbuf Failed = %ld\n",unit, Adapter->JumboMbufFailed);
   printf("em%d: Jumbo Cluster Failed = %ld\n",unit, Adapter->JumboClusterFailed);
   printf("em%d: Std Mbuf Failed = %ld\n",unit, Adapter->StdMbufFailed);
   printf("em%d: Std Cluster Failed = %ld\n",unit, Adapter->StdClusterFailed);

   printf("em%d: Symbol errors = %lld\n", unit, Adapter->stats.symerrs);
   printf("em%d: Sequence errors = %lld\n", unit, Adapter->stats.sec);
   printf("em%d: Defer count = %lld\n", unit, Adapter->stats.dc);

   printf("em%d: Missed Packets = %lld\n", unit, Adapter->stats.mpc);
   printf("em%d: Receive No Buffers = %lld\n", unit, Adapter->stats.rnbc);
   printf("em%d: Receive length errors = %lld\n", unit, Adapter->stats.rlec);
   printf("em%d: Receive errors = %lld\n", unit, Adapter->stats.rxerrc);
   printf("em%d: Crc errors = %lld\n", unit, Adapter->stats.crcerrs);
   printf("em%d: Alignment errors = %lld\n", unit, Adapter->stats.algnerrc);
   printf("em%d: Carrier extension errors = %lld\n", unit, Adapter->stats.cexterr);
   printf("em%d: Driver dropped packets = %ld\n", unit, Adapter->DroppedPackets);

   printf("em%d: XON Rcvd = %lld\n", unit, Adapter->stats.xonrxc);
   printf("em%d: XON Xmtd = %lld\n", unit, Adapter->stats.xontxc);
   printf("em%d: XOFF Rcvd = %lld\n", unit, Adapter->stats.xoffrxc);
   printf("em%d: XOFF Xmtd = %lld\n", unit, Adapter->stats.xofftxc);

   printf("em%d: Good Packets Rcvd = %lld\n", unit, Adapter->stats.gprc);
   printf("em%d: Good Packets Xmtd = %lld\n", unit, Adapter->stats.gptc);
}


/**********************************************************************
 *
 *  Examine each tx_buffer in the used queue. If the hardware is done
 *  processing the packet then free associated resources. The
 *  tx_buffer is put back on the free queue. 
 *
 **********************************************************************/
static void
em_clean_transmit_interrupts(struct adapter * Adapter)
{
   struct em_tx_buffer *tx_buffer;
   struct em_tx_desc *TransmitDescriptor;
   int             s;
   struct ifnet   *ifp;

   s = splimp();
#ifdef DBG_STATS
   Adapter->CleanTxInterrupts++;
#endif

   for (tx_buffer = STAILQ_FIRST(&Adapter->UsedSwTxPacketList);
        tx_buffer; 
        tx_buffer = STAILQ_FIRST(&Adapter->UsedSwTxPacketList)) {

      /* 
       * Get hold of the next descriptor that the em will report status
       * back to (this will be the last descriptor of a given tx_buffer). We
       * only want to free the tx_buffer (and it resources) if the driver is
       * done with ALL of the descriptors.  If the driver is done with the
       * last one then it is done with all of them.
       */

      TransmitDescriptor = Adapter->OldestUsedTxDescriptor +
         (tx_buffer->NumTxDescriptorsUsed - 1);

      /* Check for wrap case */
      if (TransmitDescriptor > Adapter->LastTxDescriptor)
         TransmitDescriptor -= Adapter->NumTxDescriptors;


      /* 
       * If the descriptor done bit is set free tx_buffer and associated
       * resources
       */
      if (TransmitDescriptor->upper.fields.status &
         E1000_TXD_STAT_DD) {

         STAILQ_REMOVE_HEAD(&Adapter->UsedSwTxPacketList, em_tx_entry);

         if ((TransmitDescriptor == Adapter->LastTxDescriptor))
            Adapter->OldestUsedTxDescriptor =
               Adapter->FirstTxDescriptor;
         else
            Adapter->OldestUsedTxDescriptor = (TransmitDescriptor + 1);

         /* Make available the descriptors that were previously used */
         Adapter->NumTxDescriptorsAvail +=
            tx_buffer->NumTxDescriptorsUsed;

         tx_buffer->NumTxDescriptorsUsed = 0;

         if (tx_buffer->Packet) {
            m_freem(tx_buffer->Packet);
            tx_buffer->Packet = NULL;
         }
         /* Return this "Software packet" back to the "free" list */
         STAILQ_INSERT_TAIL(&Adapter->FreeSwTxPacketList, tx_buffer, em_tx_entry);
      } else {
         /* 
          * Found a tx_buffer that the em is not done with then there is
          * no reason to check the rest of the queue.
          */
         break;
      }
   }                     /* end for each tx_buffer */

   ifp = &Adapter->interface_data.ac_if;

   /* Tell the stack that it is OK to send packets */
   if (Adapter->NumTxDescriptorsAvail > TX_CLEANUP_THRESHOLD) {
      ifp->if_timer = 0;
      ifp->if_flags &= ~IFF_OACTIVE;
   }
   splx(s);
   return;
}

