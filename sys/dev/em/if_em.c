/**************************************************************************
**************************************************************************

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


$FreeBSD$
***************************************************************************
***************************************************************************/

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

char em_driver_version[] = "1.0.7";


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
                                     PE1000_RECEIVE_DESCRIPTOR RxDescriptor,
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
static int em_get_std_buf __P((struct em_rx_buffer *, struct adapter *,
                               struct mbuf *));
/* Jumbo Frame */
static int em_alloc_jumbo_mem __P((struct adapter *));
static void *em_jalloc __P((struct adapter *));
static void em_jfree __P((caddr_t buf, void *args));
static int em_get_jumbo_buf __P((struct em_rx_buffer *, struct adapter *,
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
   Adapter->AutoNeg = DO_AUTO_NEG;
   Adapter->WaitAutoNegComplete = WAIT_FOR_AUTO_NEG_DEFAULT;
   Adapter->AutoNegAdvertised = AUTONEG_ADV_DEFAULT;
   Adapter->TbiCompatibilityEnable = TRUE;
   Adapter->RxBufferLen = EM_RXBUFFER_2048;
   Adapter->RxChecksum = EM_ENABLE_RXCSUM_OFFLOAD;
   Adapter->JumboEnable = EM_JUMBO_ENABLE_DEFAULT;
   
   Adapter->FlowControlHighWatermark = FC_DEFAULT_HI_THRESH;
   Adapter->FlowControlLowWatermark  = FC_DEFAULT_LO_THRESH;
   Adapter->FlowControlPauseTime = FC_DEFAULT_TX_TIMER;
   Adapter->FlowControlSendXon   = TRUE;
   Adapter->FlowControl = FLOW_CONTROL_FULL;


   /* Set the max frame size assuming standard ethernet sized frames */   
   Adapter->MaxFrameSize = ETHERMTU + ETHER_HDR_LEN + ETHER_CRC_LEN;

   /* This controls when hardware reports transmit completion status. */
   if ((EM_REPORT_TX_EARLY == 0) || (EM_REPORT_TX_EARLY == 1)) {
      Adapter->ReportTxEarly = EM_REPORT_TX_EARLY;
   } else {
      if(Adapter->MacType < MAC_LIVENGOOD) {
         Adapter->ReportTxEarly = 0;
      } else {
         Adapter->ReportTxEarly = 1;
      }
   }

   if (em_allocate_pci_resources(Adapter)) {
      printf("em%d: Allocation of PCI resources failed\n", Adapter->unit);
      em_free_pci_resources(Adapter);
      splx(s);
      return(ENXIO);
   }

   tsize = EM_ROUNDUP(Adapter->NumTxDescriptors *
                         sizeof(E1000_TRANSMIT_DESCRIPTOR), 4096);

   /* Allocate Transmit Descriptor ring */
   if (!(Adapter->TxDescBase = (PE1000_TRANSMIT_DESCRIPTOR)
         contigmalloc(tsize, M_DEVBUF, M_NOWAIT, 0, ~0, PAGE_SIZE, 0))) {
      printf("em%d: Unable to allocate TxDescriptor memory\n", Adapter->unit);
      em_free_pci_resources(Adapter);
      splx(s);
      return(ENOMEM);
   }

   rsize = EM_ROUNDUP(Adapter->NumRxDescriptors *
                         sizeof(E1000_RECEIVE_DESCRIPTOR), 4096);

   /* Allocate Receive Descriptor ring */
   if (!(Adapter->RxDescBase = (PE1000_RECEIVE_DESCRIPTOR)
        contigmalloc(rsize, M_DEVBUF, M_NOWAIT, 0, ~0, PAGE_SIZE, 0))) {
      printf("em%d: Unable to allocate RxDescriptor memory\n", Adapter->unit);
      em_free_pci_resources(Adapter);
      contigfree(Adapter->TxDescBase, tsize, M_DEVBUF);
      splx(s);
      return(ENOMEM);
   }

   /* Allocate memory for jumbo frame buffers.
    * We don't support jumbo frames on 82542 based adapters.
    */
   if (Adapter->MacType >= MAC_LIVENGOOD) {
      if (em_alloc_jumbo_mem(Adapter)) {
         printf("em%d: Unable to allocate Jumbo memory\n", Adapter->unit);
         em_free_pci_resources(Adapter);
         contigfree(Adapter->TxDescBase, tsize, M_DEVBUF);
         contigfree(Adapter->RxDescBase, rsize, M_DEVBUF);
         splx(s);
         return(ENOMEM);
      }
   }

   /* Initialize the hardware */
   if (em_hardware_init(Adapter)) {
      printf("em%d: Unable to initialize the hardware\n",Adapter->unit);
      em_free_pci_resources(Adapter);
      contigfree(Adapter->TxDescBase, tsize, M_DEVBUF);
      contigfree(Adapter->RxDescBase, rsize, M_DEVBUF);
      if (Adapter->MacType >= MAC_LIVENGOOD)
         contigfree(Adapter->em_jumbo_buf, EM_JMEM, M_DEVBUF);
      splx(s);
      return(EIO);
   }

   /* Setup OS specific network interface */
   em_setup_interface(dev, Adapter);

   /* Initialize statistics */
   em_clear_hw_stats_counters(Adapter);
   em_update_stats_counters(Adapter);
   Adapter->GetLinkStatus = 1;
   em_check_for_link(Adapter);

   /* Print the link status */
   if (Adapter->LinkIsActive == 1)
      printf("em%d:  Speed:%d Mbps  Duplex:%s\n",
             Adapter->unit,
             Adapter->LineSpeed,
             Adapter->FullDuplex == FULL_DUPLEX ? "Full" : "Half");
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
   em_phy_hardware_reset(Adapter);
   ether_ifdetach(&Adapter->interface_data.ac_if, ETHER_BPF_SUPPORTED);
   em_free_pci_resources(Adapter);

   size = EM_ROUNDUP(Adapter->NumTxDescriptors *
                     sizeof(E1000_TRANSMIT_DESCRIPTOR), 4096);

   /* Free Transmit Descriptor ring */
   if (Adapter->TxDescBase) {
      contigfree(Adapter->TxDescBase, size, M_DEVBUF);
      Adapter->TxDescBase = NULL;
   }

   size = EM_ROUNDUP(Adapter->NumRxDescriptors *
                     sizeof(E1000_RECEIVE_DESCRIPTOR), 4096);

   /* Free Receive Descriptor ring */
   if (Adapter->RxDescBase) {
      contigfree(Adapter->RxDescBase, size, M_DEVBUF);
      Adapter->RxDescBase = NULL;
   }

   /* Free Jumbo Frame buffers */
   if (Adapter->MacType >= MAC_LIVENGOOD) {
      if (Adapter->em_jumbo_buf) {
         contigfree(Adapter->em_jumbo_buf, EM_JMEM, M_DEVBUF);
         Adapter->em_jumbo_buf = NULL;
      }
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
   em_adapter_stop(Adapter);
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
   PE1000_TRANSMIT_DESCRIPTOR CurrentTxDescriptor = NULL;
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
         CurrentTxDescriptor->BufferAddress.Hi32 = 0;
         CurrentTxDescriptor->BufferAddress.Lo32 =
            vtophys(VirtualAddress);

         CurrentTxDescriptor->Lower.DwordData = (txd_lower | mp->m_len);
         CurrentTxDescriptor->Upper.DwordData = (txd_upper);

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
      CurrentTxDescriptor->Lower.DwordData |= (Adapter->TxdCmd | E1000_TXD_CMD_EOP);

      /* Send a copy of the frame to the BPF listener */
      if (ifp->if_bpf)
         bpf_mtap(ifp, m_head);
      /* 
       * Advance the Transmit Descriptor Tail (Tdt), this tells the E1000
       * that this frame is available to transmit.
       */
      E1000_WRITE_REG(Tdt, (((u_int32_t) Adapter->NextAvailTxDescriptor -
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
#ifdef SUPPORTLARGEFRAME
      if (ifr->ifr_mtu > MAX_JUMBO_FRAME_SIZE - ETHER_HDR_LEN) {
         error = EINVAL;
      } else {
         ifp->if_mtu = ifr->ifr_mtu;
         Adapter->MaxFrameSize = ifp->if_mtu + ETHER_HDR_LEN + ETHER_CRC_LEN;
         em_init(Adapter);
      }
#else
     if (ifr->ifr_mtu > EM_JUMBO_MTU) {
         error = EINVAL;
      } else {

         if(ifr->ifr_mtu > ETHERMTU &&
            Adapter->MacType < MAC_LIVENGOOD) {
            printf("Jumbo frames are not supported on 82542 based adapters\n");
            error = EINVAL;
         }
         else {
            ifp->if_mtu = ifr->ifr_mtu;
            if (ifp->if_mtu > ETHERMTU) {
               Adapter->JumboEnable = 1;
               Adapter->RxBufferLen = EM_RXBUFFER_16384;
            }
            else {
               Adapter->JumboEnable = 0;
               Adapter->RxBufferLen = EM_RXBUFFER_2048;
            }
            Adapter->MaxFrameSize = ifp->if_mtu + ETHER_HDR_LEN + ETHER_CRC_LEN;
            em_init(Adapter);
         }
      }
#endif

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
         if(Adapter->MacType == MAC_WISEMAN_2_0)
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

   reg_rctl = E1000_READ_REG(Rctl);

   if(ifp->if_flags & IFF_PROMISC) {
      reg_rctl |= (E1000_RCTL_UPE | E1000_RCTL_MPE);
      E1000_WRITE_REG(Rctl, reg_rctl);
   }
   else if (ifp->if_flags & IFF_ALLMULTI) {
      reg_rctl |= E1000_RCTL_MPE;
      reg_rctl &= ~E1000_RCTL_UPE;
      E1000_WRITE_REG(Rctl, reg_rctl);
   }

   return;
}

static void
em_disable_promisc(struct adapter * Adapter)
{
   u_int32_t       reg_rctl;

   reg_rctl = E1000_READ_REG(Rctl);

   reg_rctl &=  (~E1000_RCTL_UPE);
   reg_rctl &=  (~E1000_RCTL_MPE);
   E1000_WRITE_REG(Rctl, reg_rctl);

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
   struct ifmultiaddr  *ifma_ptr;
   int i = 0;
   int multi_cnt = 0;
   struct ifnet   *ifp = &Adapter->interface_data.ac_if;
   
   IOCTL_DEBUGOUT("em_set_multi: begin");

    if(Adapter->MacType == MAC_WISEMAN_2_0) {
       reg_rctl = E1000_READ_REG(Rctl);
       if(Adapter->PciCommandWord & CMD_MEM_WRT_INVALIDATE) {
          PciCommandWord =Adapter->PciCommandWord & ~CMD_MEM_WRT_INVALIDATE;
          pci_write_config(Adapter->dev, PCIR_COMMAND, PciCommandWord, 2);
       }
       reg_rctl |= E1000_RCTL_RST;
       E1000_WRITE_REG(Rctl, reg_rctl);
       DelayInMilliseconds(5);
    }

    TAILQ_FOREACH(ifma_ptr, &ifp->if_multiaddrs, ifma_link) {
       multi_cnt++;
       bcopy(LLADDR((struct sockaddr_dl *)ifma_ptr->ifma_addr),
             &mta[i*ETH_LENGTH_OF_ADDRESS], ETH_LENGTH_OF_ADDRESS);
       i++;
    }

    if (multi_cnt > MAX_NUM_MULTICAST_ADDRESSES) {
       reg_rctl = E1000_READ_REG(Rctl);
       reg_rctl |= E1000_RCTL_MPE;
       E1000_WRITE_REG(Rctl, reg_rctl);
    }
    else
       em_multicast_address_list_update(Adapter, mta, multi_cnt, 0);

    if(Adapter->MacType == MAC_WISEMAN_2_0) {
       reg_rctl = E1000_READ_REG(Rctl);
       reg_rctl &= ~E1000_RCTL_RST;
       E1000_WRITE_REG(Rctl, reg_rctl);
       DelayInMilliseconds(5);
       if(Adapter->PciCommandWord & CMD_MEM_WRT_INVALIDATE) {
          pci_write_config(Adapter->dev, PCIR_COMMAND, Adapter->PciCommandWord, 2);
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
   if(E1000_READ_REG(Status) & E1000_STATUS_TXOFF) {
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

   em_check_for_link(Adapter);
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
   if(E1000_READ_REG(Status) & E1000_STATUS_LU) {
      if(Adapter->LinkIsActive == 0) {
         em_get_speed_and_duplex(Adapter, &Adapter->LineSpeed, &Adapter->FullDuplex);
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
   Adapter->AdapterStopped = FALSE;
   
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

   if(Adapter->MacType >= MAC_LIVENGOOD)
      ifp->if_hwassist = EM_CHECKSUM_FEATURES;

   Adapter->timer_handle = timeout(em_local_timer, Adapter, 2*hz);
   em_clear_hw_stats_counters(Adapter);
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
   em_adapter_stop(Adapter);
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
   while(ProcessCount > 0 && (IcrContents = E1000_READ_REG(Icr)) != 0) {

      /* Link status change */
      if(IcrContents & (E1000_ICR_RXSEQ | E1000_ICR_LSC)) {
         untimeout(em_local_timer, Adapter, Adapter->timer_handle);
         Adapter->GetLinkStatus = 1;
         em_check_for_link(Adapter);
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

   em_check_for_link(Adapter);
   if(E1000_READ_REG(Status) & E1000_STATUS_LU) {
      if(Adapter->LinkIsActive == 0) {
         em_get_speed_and_duplex(Adapter, &Adapter->LineSpeed, &Adapter->FullDuplex);
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

   if (Adapter->MediaType == MEDIA_TYPE_FIBER) {
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
      if (Adapter->AutoNeg)
         return 0;
      else {
         Adapter->AutoNeg = DO_AUTO_NEG;
         Adapter->AutoNegAdvertised = AUTONEG_ADV_DEFAULT;
      }
      break;
   case IFM_1000_SX:
   case IFM_1000_TX:
      Adapter->AutoNeg = DO_AUTO_NEG;
      Adapter->AutoNegAdvertised = ADVERTISE_1000_FULL;
      break;
   case IFM_100_TX:
      Adapter->AutoNeg = FALSE;
      Adapter->AutoNegAdvertised = 0;
      if ((ifm->ifm_media & IFM_GMASK) == IFM_FDX)
         Adapter->ForcedSpeedDuplex = FULL_100;
      else
         Adapter->ForcedSpeedDuplex = HALF_100;
      break;
   case IFM_10_T:
     Adapter->AutoNeg = FALSE;
     Adapter->AutoNegAdvertised = 0;
     if ((ifm->ifm_media & IFM_GMASK) == IFM_FDX)
        Adapter->ForcedSpeedDuplex = FULL_10;
     else
        Adapter->ForcedSpeedDuplex = HALF_10;
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
   Adapter->PciCommandWord = pci_read_config(dev, PCIR_COMMAND, 2);
   Adapter->PciCommandWord |= (PCIM_CMD_BUSMASTEREN | PCIM_CMD_MEMEN);
   pci_write_config(dev, PCIR_COMMAND, Adapter->PciCommandWord, 2);

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
      Adapter->MacType = (Adapter->RevId == 3) ?
         MAC_WISEMAN_2_1 : MAC_WISEMAN_2_0;
      break;
   case PCI_DEVICE_ID_82543GC_FIBER:
   case PCI_DEVICE_ID_82543GC_COPPER:
      Adapter->MacType = MAC_LIVENGOOD;
      break;
   case PCI_DEVICE_ID_82544EI_FIBER:
   case PCI_DEVICE_ID_82544EI_COPPER:
   case PCI_DEVICE_ID_82544GC_COPPER:
   case PCI_DEVICE_ID_82544GC_STRG:
      Adapter->MacType = MAC_CORDOVA;
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
   Adapter->bus_space_tag = rman_get_bustag(Adapter->res_memory);
   Adapter->bus_space_handle = rman_get_bushandle(Adapter->res_memory);

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
   Adapter->AdapterStopped = FALSE;
   em_adapter_stop(Adapter);
   Adapter->AdapterStopped = FALSE;

   /* Make sure we have a good EEPROM before we read from it */
   if (!em_validate_eeprom_checksum(Adapter)) {
      printf("em%d: The EEPROM Checksum Is Not Valid\n", Adapter->unit);
      return EIO;
   }
   /* Copy the permanent MAC address and part number out of the EEPROM */
   em_read_mac_address(Adapter, Adapter->interface_data.ac_enaddr);
   memcpy(Adapter->CurrentNetAddress, Adapter->interface_data.ac_enaddr,
         ETH_LENGTH_OF_ADDRESS);
   em_read_part_number(Adapter, &(Adapter->PartNumber));

   if (!em_initialize_hardware(Adapter)) {
      printf("em%d: Hardware Initialization Failed", Adapter->unit);
      return EIO;
   }
   em_check_for_link(Adapter);
   if (E1000_READ_REG(Status) & E1000_STATUS_LU)
      Adapter->LinkIsActive = 1;
   else
      Adapter->LinkIsActive = 0;
 
   if (Adapter->LinkIsActive) {
      em_get_speed_and_duplex(Adapter, &Adapter->LineSpeed, &Adapter->FullDuplex);
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
         em_read_eeprom_word(Adapter, EEPROM_NODE_ADDRESS_BYTE_0 + (i / 2));
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
   if (Adapter->MediaType == MEDIA_TYPE_FIBER) {
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
        (sizeof(E1000_TRANSMIT_DESCRIPTOR)) * Adapter->NumTxDescriptors);

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
   E1000_WRITE_REG(Tdbal, vtophys((vm_offset_t) Adapter->TxDescBase));
   E1000_WRITE_REG(Tdbah, 0);
   E1000_WRITE_REG(Tdl, Adapter->NumTxDescriptors *
               sizeof(E1000_TRANSMIT_DESCRIPTOR));

   /* Setup the HW Tx Head and Tail descriptor pointers */
   E1000_WRITE_REG(Tdh, 0);
   E1000_WRITE_REG(Tdt, 0);


   HW_DEBUGOUT2("Base = %x, Length = %x\n", E1000_READ_REG(Tdbal),
             E1000_READ_REG(Tdl));


   /* Zero out the 82542 Tx Queue State registers - we don't use them */
   if (Adapter->MacType < MAC_LIVENGOOD) {
      E1000_WRITE_REG(Tqsal, 0);
      E1000_WRITE_REG(Tqsah, 0);
   }
   
   /* Set the default values for the Tx Inter Packet Gap timer */
   switch (Adapter->MacType) {
   case MAC_LIVENGOOD:
   case MAC_WAINWRIGHT:
   case MAC_CORDOVA:
      if (Adapter->MediaType == MEDIA_TYPE_FIBER)
         reg_tipg = DEFAULT_LVGD_TIPG_IPGT_FIBER;
      else
         reg_tipg = DEFAULT_LVGD_TIPG_IPGT_COPPER;
      reg_tipg |= DEFAULT_LVGD_TIPG_IPGR1 << E1000_TIPG_IPGR1_SHIFT;
      reg_tipg |= DEFAULT_LVGD_TIPG_IPGR2 << E1000_TIPG_IPGR2_SHIFT;
      break;
   case MAC_WISEMAN_2_0:
   case MAC_WISEMAN_2_1:
      reg_tipg = DEFAULT_WSMN_TIPG_IPGT;
      reg_tipg |= DEFAULT_WSMN_TIPG_IPGR1 << E1000_TIPG_IPGR1_SHIFT;
      reg_tipg |= DEFAULT_WSMN_TIPG_IPGR2 << E1000_TIPG_IPGR2_SHIFT;
      break;
   }
   E1000_WRITE_REG(Tipg, reg_tipg);
   E1000_WRITE_REG(Tidv, Adapter->TxIntDelay);

   /* Program the Transmit Control Register */
   reg_tctl = E1000_TCTL_PSP | E1000_TCTL_EN |
      (E1000_COLLISION_THRESHOLD << E1000_CT_SHIFT);
   if (Adapter->FullDuplex == 1) {
      reg_tctl |= E1000_FDX_COLLISION_DISTANCE << E1000_COLD_SHIFT;
   } else {
      reg_tctl |= E1000_HDX_COLLISION_DISTANCE << E1000_COLD_SHIFT;
   }
   E1000_WRITE_REG(Tctl, reg_tctl);

   /* Setup Transmit Descriptor Settings for this adapter */   
   Adapter->TxdCmd = E1000_TXD_CMD_IFCS;

   if(Adapter->TxIntDelay > 0)
      Adapter->TxdCmd |= E1000_TXD_CMD_IDE;
   
   if(Adapter->ReportTxEarly == 1)
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
   PE1000_TCPIP_CONTEXT_TRANSMIT_DESCRIPTOR TXD;
   PE1000_TRANSMIT_DESCRIPTOR CurrentTxDescriptor;
     
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
   TXD = (PE1000_TCPIP_CONTEXT_TRANSMIT_DESCRIPTOR)CurrentTxDescriptor;

   TXD->LowerXsumSetup.IpFields.Ipcss = ETHER_HDR_LEN;
   TXD->LowerXsumSetup.IpFields.Ipcso = ETHER_HDR_LEN + offsetof(struct ip, ip_sum);
   TXD->LowerXsumSetup.IpFields.Ipcse = ETHER_HDR_LEN + sizeof(struct ip) - 1;

   TXD->UpperXsumSetup.TcpFields.Tucss = ETHER_HDR_LEN + sizeof(struct ip);
   TXD->UpperXsumSetup.TcpFields.Tucse = 0;

   if(Adapter->ActiveChecksumContext == OFFLOAD_TCP_IP) {
      TXD->UpperXsumSetup.TcpFields.Tucso = ETHER_HDR_LEN + sizeof(struct ip) + 
         offsetof(struct tcphdr, th_sum);
   } else if (Adapter->ActiveChecksumContext == OFFLOAD_UDP_IP) {
      TXD->UpperXsumSetup.TcpFields.Tucso = ETHER_HDR_LEN + sizeof(struct ip) + 
         offsetof(struct udphdr, uh_sum);
   }

   TXD->TcpSegSetup.DwordData = 0;
   TXD->CmdAndLength = E1000_TXD_CMD_DEXT;

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
 *  Get buffer from driver maintained free list for jumbo frames.
 *
 **********************************************************************/
static int
em_get_jumbo_buf(struct em_rx_buffer *rx_buffer, struct adapter *Adapter, 
                 struct mbuf *mp)
{
   struct mbuf    *nmp;

   if (mp == NULL) {
      caddr_t  *buf = NULL;
      MGETHDR(nmp, M_DONTWAIT, MT_DATA);
      if (nmp == NULL) {
         printf("em%d: Mbuf allocation failed\n", Adapter->unit);
         Adapter->JumboMbufFailed++;
         return (ENOBUFS);
      }

      /* Allocate the jumbo buffer */
      buf = em_jalloc(Adapter);
      if (buf == NULL) {
         m_freem(nmp);
         Adapter->JumboClusterFailed++;
         return(ENOBUFS);
      }

     /* Attach the buffer to the mbuf. */
      nmp->m_data = (void *)buf;
      nmp->m_len = nmp->m_pkthdr.len = EM_JUMBO_FRAMELEN;
      MEXTADD(nmp, buf, EM_JUMBO_FRAMELEN, em_jfree,
        (struct adapter *)Adapter, 0, EXT_NET_DRV);
   } else {
      nmp = mp;
      nmp->m_data = nmp->m_ext.ext_buf;
      nmp->m_ext.ext_size = EM_JUMBO_FRAMELEN;
   }

   m_adj(nmp, ETHER_ALIGN);

   rx_buffer->Packet = nmp;
   rx_buffer->LowPhysicalAddress = vtophys(mtod(nmp, vm_offset_t));
   rx_buffer->HighPhysicalAddress = 0;

   return (0);
}


/*********************************************************************
 *
 *  Get a buffer from system mbuf buffer pool.
 *
 **********************************************************************/
static int
em_get_std_buf(struct em_rx_buffer *rx_buffer, struct adapter *Adapter,
               struct mbuf *mp)
{
   struct mbuf    *nmp;

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
   }

#ifndef SUPPORTLARGEFRAME
   m_adj(nmp, ETHER_ALIGN);
#endif

   rx_buffer->Packet = nmp;
   rx_buffer->LowPhysicalAddress = vtophys(mtod(nmp, vm_offset_t));
   rx_buffer->HighPhysicalAddress = 0;

   return (0);
}

/*********************************************************************
 *
 *  Get buffer from system or driver maintained buffer freelist.
 *
 **********************************************************************/
static int
em_get_buf(struct em_rx_buffer *rx_buffer, struct adapter * Adapter, 
           struct mbuf *mp)
{
   int error = 0;

   if(Adapter->JumboEnable == 1)
      error = em_get_jumbo_buf(rx_buffer, Adapter, mp);
   else
      error = em_get_std_buf(rx_buffer, Adapter, mp);

   return error;
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
   PE1000_RECEIVE_DESCRIPTOR RxDescriptorPtr;
   int             i;

   if(em_allocate_receive_structures(Adapter))
      return ENOMEM;

   STAILQ_INIT(&Adapter->RxSwPacketList);

   Adapter->FirstRxDescriptor =
      (PE1000_RECEIVE_DESCRIPTOR) Adapter->RxDescBase;
   Adapter->LastRxDescriptor =
      Adapter->FirstRxDescriptor + (Adapter->NumRxDescriptors - 1);

   rx_buffer = (struct em_rx_buffer *) Adapter->rx_buffer_area;

   bzero((void *) Adapter->FirstRxDescriptor,
        (sizeof(E1000_RECEIVE_DESCRIPTOR)) * Adapter->NumRxDescriptors);

   /* Build a linked list of rx_buffer's */
   for (i = 0, RxDescriptorPtr = Adapter->FirstRxDescriptor;
       i < Adapter->NumRxDescriptors;
       i++, rx_buffer++, RxDescriptorPtr++) {
      if (rx_buffer->Packet == NULL)
         printf("em%d: Receive buffer memory not allocated", Adapter->unit);
      else {
         RxDescriptorPtr->BufferAddress.Lo32 =
            rx_buffer->LowPhysicalAddress;
         RxDescriptorPtr->BufferAddress.Hi32 =
            rx_buffer->HighPhysicalAddress;
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
   E1000_WRITE_REG(Rctl, 0);

   /* Set the Receive Delay Timer Register */
   E1000_WRITE_REG(Rdtr0, Adapter->RxIntDelay | E1000_RDT0_FPDB);

   /* Setup the Base and Length of the Rx Descriptor Ring */
   E1000_WRITE_REG(Rdbal0, vtophys((vm_offset_t) Adapter->RxDescBase));
   E1000_WRITE_REG(Rdbah0, 0);
   E1000_WRITE_REG(Rdlen0, Adapter->NumRxDescriptors *
               sizeof(E1000_RECEIVE_DESCRIPTOR));

   /* Setup the HW Rx Head and Tail Descriptor Pointers */
   E1000_WRITE_REG(Rdh0, 0);
   E1000_WRITE_REG(Rdt0,
               (((u_int32_t) Adapter->LastRxDescriptor -
                 (u_int32_t) Adapter->FirstRxDescriptor) >> 4));

   /* 
    * Zero out the registers associated with the 82542 second receive
    * descriptor ring - we don't use it
    */
   if (Adapter->MacType < MAC_LIVENGOOD) {
      E1000_WRITE_REG(Rdbal1, 0);
      E1000_WRITE_REG(Rdbah1, 0);
      E1000_WRITE_REG(Rdlen1, 0);
      E1000_WRITE_REG(Rdh1, 0);
      E1000_WRITE_REG(Rdt1, 0);
   }
   
   /* Setup the Receive Control Register */
   reg_rctl = E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_LBM_NO |
      E1000_RCTL_RDMTS0_HALF |
      (Adapter->MulticastFilterType << E1000_RCTL_MO_SHIFT);

   if (Adapter->TbiCompatibilityOn == TRUE)
      reg_rctl |= E1000_RCTL_SBP;


#ifdef SUPPORTLARGEFRAME
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
#else
   switch (Adapter->RxBufferLen) {
   case EM_RXBUFFER_2048:
      reg_rctl |= E1000_RCTL_SZ_2048;
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
#endif

   /* Enable 82543 Receive Checksum Offload for TCP and UDP */
   if((Adapter->MacType >= MAC_LIVENGOOD) && (Adapter->RxChecksum == 1)) {
      reg_rxcsum = E1000_READ_REG(Rxcsum);
      reg_rxcsum |= (E1000_RXCSUM_IPOFL | E1000_RXCSUM_TUOFL);
      E1000_WRITE_REG(Rxcsum, reg_rxcsum);
   }

   /* Enable Receives */
   E1000_WRITE_REG(Rctl, reg_rctl);

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
 *  Allocate memory to be used for jumbo buffers
 *
 **********************************************************************/
static int 
em_alloc_jumbo_mem(struct adapter *Adapter)
{
   caddr_t                 ptr;
   register int            i;
   struct em_jpool_entry   *entry;

   
   Adapter->em_jumbo_buf = contigmalloc(EM_JMEM, M_DEVBUF,
                                        M_NOWAIT, 0, 0xffffffff, PAGE_SIZE, 0);

   if (Adapter->em_jumbo_buf == NULL) {
      printf("em%d: No memory for jumbo buffers!\n", Adapter->unit);
      return(ENOBUFS);
   }

   SLIST_INIT(&Adapter->em_jfree_listhead);
   SLIST_INIT(&Adapter->em_jinuse_listhead);

   /*
    * Now divide it up into 9K pieces and save the addresses
    * in an array. We use the the first few bytes in the buffer to hold 
    * the address of the adapter (softc) structure for this interface. 
    * This is because em_jfree() needs it, but it is called by the mbuf 
    * management code which will not pass it to us explicitly.
    */

   ptr = Adapter->em_jumbo_buf;
   for (i = 0; i < EM_JSLOTS; i++) {
      Adapter->em_jslots[i].em_buf = ptr;
      ptr += EM_JLEN;
      entry = malloc(sizeof(struct em_jpool_entry),
                     M_DEVBUF, M_NOWAIT);
      if (entry == NULL) {
         contigfree(Adapter->em_jumbo_buf, EM_JMEM,
                    M_DEVBUF);
         Adapter->em_jumbo_buf = NULL;
         printf("em%d: No memory for jumbo buffer queue!\n", Adapter->unit);
         return(ENOBUFS);
      }
      entry->slot = i;
      SLIST_INSERT_HEAD(&Adapter->em_jfree_listhead, entry, em_jpool_entries);
   }
   return(0);
}


/*********************************************************************
 *
 *  Get Jumbo buffer from free list.
 *
 **********************************************************************/
static void *em_jalloc(struct adapter *Adapter)
{
   struct em_jpool_entry   *entry;

   entry = SLIST_FIRST(&Adapter->em_jfree_listhead);

   if (entry == NULL) {
      Adapter->NoJumboBufAvail++;
      return(NULL);
   }

   SLIST_REMOVE_HEAD(&Adapter->em_jfree_listhead, em_jpool_entries);
   SLIST_INSERT_HEAD(&Adapter->em_jinuse_listhead, entry, em_jpool_entries);
   return(Adapter->em_jslots[entry->slot].em_buf);
}


/*********************************************************************
 *
 *  Put the jumbo buffer back onto free list.
 *
 *********************************************************************/
static void 
em_jfree(caddr_t buf, void *args)
{
   struct adapter *Adapter;
   int                     i;
   struct em_jpool_entry   *entry;

   /* Extract the adapter (softc) struct pointer. */
   Adapter = (struct adapter *)args;

   if (Adapter == NULL)
      panic("em_jfree: Can't find softc pointer!");

   /* Calculate the slot this buffer belongs to */
   i = ((vm_offset_t)buf
        - (vm_offset_t)Adapter->em_jumbo_buf) / EM_JLEN;

   if ((i < 0) || (i >= EM_JSLOTS))
      panic("em_jfree: Asked to free buffer that we don't manage!");

   entry = SLIST_FIRST(&Adapter->em_jinuse_listhead);
   if (entry == NULL)
      panic("em_jfree: Buffer not in use!");
   entry->slot = i;
   SLIST_REMOVE_HEAD(&Adapter->em_jinuse_listhead,
                     em_jpool_entries);
   SLIST_INSERT_HEAD(&Adapter->em_jfree_listhead,
                     entry, em_jpool_entries);

   return;
}

#ifdef SUPPORTLARGEFRAME
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
   struct mbuf         *mp, *lmp;
   struct mbuf         *fmp = NULL;
   struct ifnet        *ifp;
   struct ether_header *eh;
   u_int16_t           Length;
   u_int8_t            LastByte;
   u_int8_t            AcceptFrame = 0;
   u_int8_t            EndOfPacket = 0;
   u_int16_t           PacketLength = 0;

   /* Pointer to the receive descriptor being examined. */
   PE1000_RECEIVE_DESCRIPTOR CurrentDescriptor;
   PE1000_RECEIVE_DESCRIPTOR LastDescriptorProcessed;
   struct em_rx_buffer   *rx_buffer;

   TXRX_DEBUGOUT("em_process_receive_interrupts: begin");

   ifp = &Adapter->interface_data.ac_if;
   CurrentDescriptor = Adapter->NextRxDescriptorToCheck;

   if (!((CurrentDescriptor->ReceiveStatus) & E1000_RXD_STAT_DD)) {
#ifdef DBG_STATS
      Adapter->NoPacketsAvail++;
#endif
      return;
   }
   
   while (CurrentDescriptor->ReceiveStatus & E1000_RXD_STAT_DD) {

      /* Get a pointer to the actual receive buffer */
      rx_buffer = STAILQ_FIRST(&Adapter->RxSwPacketList);

      if(rx_buffer == NULL) {
         printf("em%d: Found null rx_buffer\n", Adapter->unit);
         return;
      }

      mp = rx_buffer->Packet;      
      AcceptFrame = 1;

      if (CurrentDescriptor->ReceiveStatus & E1000_RXD_STAT_EOP) {
         EndOfPacket = 1;
         Length = CurrentDescriptor->Length - ETHER_CRC_LEN;
      }
      else {
         EndOfPacket = 0;
         Length = CurrentDescriptor->Length;
      }

      if(CurrentDescriptor->Errors & E1000_RXD_ERR_FRAME_ERR_MASK) {

         LastByte = *(mtod(rx_buffer->Packet,caddr_t) + Length - 1);

         if (TBI_ACCEPT(CurrentDescriptor->Errors, LastByte, Length)) {  
            em_adjust_tbi_accepted_stats(Adapter, Length, Adapter->CurrentNetAddress);
            Length--;
         } else {  
            AcceptFrame = 0;
         }
      }

      if (AcceptFrame) {

         /* Keep track of entire packet length */
         PacketLength += Length;

         /* Assign correct length to the current fragment */
         mp->m_len = Length;

         if(fmp == NULL) {
            fmp = mp;       /* Store the first mbuf */
            lmp = fmp;
         }
         else {
            /* Chain mbuf's together */
            mp->m_flags &= ~M_PKTHDR;
            lmp->m_next = mp;
            lmp = lmp->m_next;
            lmp->m_next = NULL;
         }

         if (em_get_buf(rx_buffer, Adapter, NULL) == ENOBUFS) {
            Adapter->DroppedPackets++;
            em_get_buf(rx_buffer, Adapter, mp);
            if(fmp != NULL) m_freem(fmp);
            fmp = NULL;
            lmp = NULL;
            PacketLength = 0;
            break;
         }

         if (EndOfPacket) {
            fmp->m_pkthdr.rcvif = ifp;
            fmp->m_pkthdr.len = PacketLength;

            eh = mtod(fmp, struct ether_header *);

            /* Remove ethernet header from mbuf */
            m_adj(fmp, sizeof(struct ether_header));
            em_receive_checksum(Adapter, CurrentDescriptor, fmp);
            ether_input(ifp, eh, fmp);
      
            fmp = NULL;
            lmp = NULL;
            PacketLength = 0;
         }
      } else {
         Adapter->DroppedPackets++;
         em_get_buf(rx_buffer, Adapter, mp);
         if(fmp != NULL) m_freem(fmp);
         fmp = NULL;
         lmp = NULL;
         PacketLength = 0;
      }
      
      /* Zero out the receive descriptors status  */
      CurrentDescriptor->ReceiveStatus = 0;
      
      if (rx_buffer->Packet != NULL) {
         CurrentDescriptor->BufferAddress.Lo32 =
            rx_buffer->LowPhysicalAddress;
         CurrentDescriptor->BufferAddress.Hi32 =
            rx_buffer->HighPhysicalAddress;
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
      E1000_WRITE_REG(Rdt0, (((u_int32_t) LastDescriptorProcessed -
                        (u_int32_t) Adapter->FirstRxDescriptor) >> 4));
   }
   return;
}

#else
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
   u_int8_t            AcceptFrame;

   /* Pointer to the receive descriptor being examined. */
   PE1000_RECEIVE_DESCRIPTOR CurrentDescriptor;
   PE1000_RECEIVE_DESCRIPTOR LastDescriptorProcessed;
   struct em_rx_buffer   *rx_buffer;

   TXRX_DEBUGOUT("em_process_receive_interrupts: begin");

   ifp = &Adapter->interface_data.ac_if;
   CurrentDescriptor = Adapter->NextRxDescriptorToCheck;

   if (!((CurrentDescriptor->ReceiveStatus) & E1000_RXD_STAT_DD)) {
#ifdef DBG_STATS
      Adapter->NoPacketsAvail++;
#endif
      return;
   }

   while (CurrentDescriptor->ReceiveStatus & E1000_RXD_STAT_DD) {

      /* Get a pointer to the actual receive buffer */
      rx_buffer = STAILQ_FIRST(&Adapter->RxSwPacketList);
      if(rx_buffer == NULL) return;      
      mp = rx_buffer->Packet;

      Length = CurrentDescriptor->Length;
      
      /* Make sure this is also the last descriptor in the packet. */      
      if (CurrentDescriptor->ReceiveStatus & E1000_RXD_STAT_EOP) { 

         AcceptFrame = 1;

         if(CurrentDescriptor->Errors & E1000_RXD_ERR_FRAME_ERR_MASK) {

            LastByte = *(mtod(rx_buffer->Packet,caddr_t) + Length - 1);
            
            if (TBI_ACCEPT(CurrentDescriptor->Errors, LastByte, Length)) {  
               em_adjust_tbi_accepted_stats(Adapter, Length, Adapter->CurrentNetAddress);
               Length--;
            } else {  
               AcceptFrame = 0;
            }
         }

         if (AcceptFrame) {
            if (em_get_buf(rx_buffer, Adapter, NULL) == ENOBUFS) {
               Adapter->DroppedPackets++;
               em_get_buf(rx_buffer, Adapter, mp);
               break;
            }
            
            mp->m_pkthdr.rcvif = ifp;
            mp->m_pkthdr.len = mp->m_len = Length - ETHER_CRC_LEN;
            eh = mtod(mp, struct ether_header *);

            /* Remove ethernet header from mbuf */
            m_adj(mp, sizeof(struct ether_header));
            em_receive_checksum(Adapter, CurrentDescriptor, mp);            
            ether_input(ifp, eh, mp);

         } else { 
            em_get_buf(rx_buffer, Adapter, mp);
            Adapter->DroppedPackets++;
         }
      } else {
         /* 
          * If the received packet has spanned multiple descriptors, ignore
          * and discard all the packets that do not have EOP set and proceed
          * to the next packet.
          */
         printf("em%d: !Receive packet consumed multiple buffers\n", Adapter->unit);
         em_get_buf(rx_buffer, Adapter, mp);
         Adapter->DroppedPackets++;
      }

      /* Zero out the receive descriptors status  */
      CurrentDescriptor->ReceiveStatus = 0;

      if (rx_buffer->Packet != NULL) {
         CurrentDescriptor->BufferAddress.Lo32 =
            rx_buffer->LowPhysicalAddress;
         CurrentDescriptor->BufferAddress.Hi32 =
            rx_buffer->HighPhysicalAddress;
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
      E1000_WRITE_REG(Rdt0, (((u_int32_t) LastDescriptorProcessed -
                              (u_int32_t) Adapter->FirstRxDescriptor) >> 4));
   }
   return;
}
#endif


/*********************************************************************
 *
 *  Verify that the hardware indicated that the checksum is valid. 
 *  Inform the stack about the status of checksum so that stack
 *  doesn't spend time verifying the checksum.
 *
 *********************************************************************/
static void
em_receive_checksum(struct adapter * Adapter,
           PE1000_RECEIVE_DESCRIPTOR RxDescriptor,
           struct mbuf *mp)
{
   /* 82543 or newer only */
   if((Adapter->MacType < MAC_LIVENGOOD) ||
      /* Ignore Checksum bit is set */
      (RxDescriptor->ReceiveStatus & E1000_RXD_STAT_IXSM)) {
      RXCSUM_DEBUGOUT("Ignoring checksum");
      mp->m_pkthdr.csum_flags = 0;
      return;
   }

   if (RxDescriptor->ReceiveStatus & E1000_RXD_STAT_IPCS) {
      /* Did it pass? */
      if (!(RxDescriptor->Errors & E1000_RXD_ERR_IPE)) {
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

   if (RxDescriptor->ReceiveStatus & E1000_RXD_STAT_TCPCS) {
      /* Did it pass? */        
      if (!(RxDescriptor->Errors & E1000_RXD_ERR_TCPE)) {
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
   E1000_WRITE_REG(Ims, (IMS_ENABLE_MASK));
   return;
}

static void
DisableInterrupts(struct adapter * Adapter)
{
   E1000_WRITE_REG(Imc, (0xffffffff & ~E1000_IMC_RXSEQ));
   return;
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

   Adapter->Crcerrs += E1000_READ_REG(Crcerrs);
   Adapter->Crcerrs += E1000_READ_REG(Crcerrs);
   Adapter->Symerrs += E1000_READ_REG(Symerrs);
   Adapter->Mpc += E1000_READ_REG(Mpc);
   Adapter->Scc += E1000_READ_REG(Scc);
   Adapter->Ecol += E1000_READ_REG(Ecol);
   Adapter->Mcc += E1000_READ_REG(Mcc);
   Adapter->Latecol += E1000_READ_REG(Latecol);
   Adapter->Colc += E1000_READ_REG(Colc);
   Adapter->Dc += E1000_READ_REG(Dc);
   Adapter->Sec += E1000_READ_REG(Sec);
   Adapter->Rlec += E1000_READ_REG(Rlec);
   Adapter->Xonrxc += E1000_READ_REG(Xonrxc);
   Adapter->Xontxc += E1000_READ_REG(Xontxc);
   Adapter->Xoffrxc += E1000_READ_REG(Xoffrxc);
   Adapter->Xofftxc += E1000_READ_REG(Xofftxc);
   Adapter->Fcruc += E1000_READ_REG(Fcruc);
   Adapter->Prc64 += E1000_READ_REG(Prc64);
   Adapter->Prc127 += E1000_READ_REG(Prc127);
   Adapter->Prc255 += E1000_READ_REG(Prc255);
   Adapter->Prc511 += E1000_READ_REG(Prc511);
   Adapter->Prc1023 += E1000_READ_REG(Prc1023);
   Adapter->Prc1522 += E1000_READ_REG(Prc1522);
   Adapter->Gprc += E1000_READ_REG(Gprc);
   Adapter->Bprc += E1000_READ_REG(Bprc);
   Adapter->Mprc += E1000_READ_REG(Mprc);
   Adapter->Gptc += E1000_READ_REG(Gptc);

   /* For the 64-bit byte counters the low dword must be read first. */
   /* Both registers clear on the read of the high dword */

   Adapter->Gorcl += E1000_READ_REG(Gorl); 
   Adapter->Gorch += E1000_READ_REG(Gorh);
   Adapter->Gotcl += E1000_READ_REG(Gotl);
   Adapter->Gotch += E1000_READ_REG(Goth);

   Adapter->Rnbc += E1000_READ_REG(Rnbc);
   Adapter->Ruc += E1000_READ_REG(Ruc);
   Adapter->Rfc += E1000_READ_REG(Rfc);
   Adapter->Roc += E1000_READ_REG(Roc);
   Adapter->Rjc += E1000_READ_REG(Rjc);

   Adapter->Torcl += E1000_READ_REG(Torl);
   Adapter->Torch += E1000_READ_REG(Torh);
   Adapter->Totcl += E1000_READ_REG(Totl);
   Adapter->Totch += E1000_READ_REG(Toth);

   Adapter->Tpr += E1000_READ_REG(Tpr);
   Adapter->Tpt += E1000_READ_REG(Tpt);
   Adapter->Ptc64 += E1000_READ_REG(Ptc64);
   Adapter->Ptc127 += E1000_READ_REG(Ptc127);
   Adapter->Ptc255 += E1000_READ_REG(Ptc255);
   Adapter->Ptc511 += E1000_READ_REG(Ptc511);
   Adapter->Ptc1023 += E1000_READ_REG(Ptc1023);
   Adapter->Ptc1522 += E1000_READ_REG(Ptc1522);
   Adapter->Mptc += E1000_READ_REG(Mptc);
   Adapter->Bptc += E1000_READ_REG(Bptc);

   if (Adapter->MacType >= MAC_LIVENGOOD) {
      Adapter->Algnerrc += E1000_READ_REG(Algnerrc);
      Adapter->Rxerrc += E1000_READ_REG(Rxerrc);
      Adapter->Tuc += E1000_READ_REG(Tuc);
      Adapter->Tncrs += E1000_READ_REG(Tncrs);
      Adapter->Cexterr += E1000_READ_REG(Cexterr);
      Adapter->Rutec += E1000_READ_REG(Rutec);
   }
   ifp = &Adapter->interface_data.ac_if;

   /* Fill out the OS statistics structure */
   ifp->if_ipackets = Adapter->Gprc;
   ifp->if_opackets = Adapter->Gptc;
   ifp->if_ibytes = Adapter->Gorcl;
   ifp->if_obytes = Adapter->Gotcl;
   ifp->if_imcasts = Adapter->Mprc;
   ifp->if_collisions = Adapter->Colc;

   /* Rx Errors */
   ifp->if_ierrors =
      Adapter->DroppedPackets +
      Adapter->Rxerrc +
      Adapter->Crcerrs +
      Adapter->Algnerrc +
      Adapter->Rlec + Adapter->Rnbc + Adapter->Mpc + Adapter->Cexterr;

   /* Tx Errors */
   ifp->if_oerrors = Adapter->Ecol + Adapter->Tuc + Adapter->Latecol;

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

   printf("em%d: Symbol errors = %ld\n",unit, Adapter->Symerrs);
   printf("em%d: Sequence errors = %ld\n", unit, Adapter->Sec);
   printf("em%d: Defer count = %ld\n", unit, Adapter->Dc);

   printf("em%d: Missed Packets = %ld\n", unit, Adapter->Mpc);
   printf("em%d: Receive No Buffers = %ld\n", unit, Adapter->Rnbc);
   printf("em%d: Receive length errors = %ld\n", unit, Adapter->Rlec);
   printf("em%d: Receive errors = %ld\n", unit, Adapter->Rxerrc);
   printf("em%d: Crc errors = %ld\n", unit, Adapter->Crcerrs);
   printf("em%d: Alignment errors = %ld\n", unit, Adapter->Algnerrc);
   printf("em%d: Carrier extension errors = %ld\n", unit, Adapter->Cexterr);
   printf("em%d: Driver dropped packets = %ld\n", unit, Adapter->DroppedPackets);

   printf("em%d: XON Rcvd = %ld\n", unit, Adapter->Xonrxc);
   printf("em%d: XON Xmtd = %ld\n", unit, Adapter->Xontxc);
   printf("em%d: XOFF Rcvd = %ld\n", unit, Adapter->Xoffrxc);
   printf("em%d: XOFF Xmtd = %ld\n", unit, Adapter->Xofftxc);

   printf("em%d: Good Packets Rcvd = %ld\n", unit, Adapter->Gprc);
   printf("em%d: Good Packets Xmtd = %ld\n", unit, Adapter->Gptc);
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
   volatile PE1000_TRANSMIT_DESCRIPTOR TransmitDescriptor;
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
      if (TransmitDescriptor->Upper.Fields.TransmitStatus &
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

