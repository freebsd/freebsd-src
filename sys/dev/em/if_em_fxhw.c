/*************************************************************************
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
**************************************************************************/
/*
* Workfile: fxhw.c 
* Date: 9/25/01 2:40p 
* Revision: 48 
*/

#include <dev/em/if_em_fxhw.h>
#include <dev/em/if_em_phy.h>

static void em_shift_out_bits(struct adapter *Adapter,

                              u16 Data, u16 Count);
static void em_raise_clock(struct adapter *Adapter, u32 * EecdRegValue);
static void em_lower_clock(struct adapter *Adapter, u32 * EecdRegValue);
static u16 em_shift_in_bits(struct adapter *Adapter);
static void em_eeprom_cleanup(struct adapter *Adapter);
static u16 em_wait_eeprom_command_done(struct adapter *Adapter);
static void em_stand_by(struct adapter *Adapter);

void em_adapter_stop(struct adapter *Adapter)
{

        u32 IcrContents;

        u16 PciCommandWord;

        DEBUGFUNC("em_adapter_stop")

            if (Adapter->AdapterStopped) {
                DEBUGOUT
                    ("Exiting because the adapter is already stopped!!!\n");
                return;
        }

        Adapter->AdapterStopped = 1;

        if (Adapter->MacType == MAC_WISEMAN_2_0) {
                if (Adapter->PciCommandWord & CMD_MEM_WRT_INVALIDATE) {
                        DEBUGOUT
                            ("Disabling MWI on rev 2.0 Wiseman silicon\n");

                        PciCommandWord =
                            Adapter->
                            PciCommandWord & ~CMD_MEM_WRT_INVALIDATE;

                        WritePciConfigWord(PCI_COMMAND_REGISTER,
                                           &PciCommandWord);
                }
        }

        DEBUGOUT("Masking off all interrupts\n");
        E1000_WRITE_REG(Imc, 0xffffffff);

        E1000_WRITE_REG(Rctl, 0);
        E1000_WRITE_REG(Tctl, 0);

        Adapter->TbiCompatibilityOn = 0;

        DelayInMilliseconds(10);

        DEBUGOUT("Issuing a global reset to MAC\n");
        E1000_WRITE_REG(Ctrl, E1000_CTRL_RST);

        DelayInMilliseconds(10);

        DEBUGOUT("Masking off all interrupts\n");
        E1000_WRITE_REG(Imc, 0xffffffff);

        IcrContents = E1000_READ_REG(Icr);

        if (Adapter->MacType == MAC_WISEMAN_2_0) {
                if (Adapter->PciCommandWord & CMD_MEM_WRT_INVALIDATE) {
                        WritePciConfigWord(PCI_COMMAND_REGISTER,
                                           &Adapter->PciCommandWord);
                }
        }

}

u8 em_initialize_hardware(struct adapter *Adapter)
 {
        u32 i;
        u16 PciCommandWord;
        u8 Status;
        u32 RegisterValue;

        DEBUGFUNC("em_initialize_hardware");

        if (Adapter->MacType != MAC_LIVENGOOD) {

                Adapter->TbiCompatibilityEnable = 0;
        }

        if (Adapter->MacType >= MAC_LIVENGOOD) {
                RegisterValue = E1000_READ_REG(Status);
                if (RegisterValue & E1000_STATUS_TBIMODE) {
                        Adapter->MediaType = MEDIA_TYPE_FIBER;

                        Adapter->TbiCompatibilityEnable = 0;
                } else {
                        Adapter->MediaType = MEDIA_TYPE_COPPER;
                }
        } else {

                Adapter->MediaType = MEDIA_TYPE_FIBER;
        }

        DEBUGOUT("Initializing the IEEE VLAN\n");
        E1000_WRITE_REG(Vet, 0);

        em_clear_vfta(Adapter);

        if (Adapter->MacType == MAC_WISEMAN_2_0) {

                if (Adapter->PciCommandWord & CMD_MEM_WRT_INVALIDATE) {
                        DEBUGOUT
                            ("Disabling MWI on rev 2.0 Wiseman silicon\n");

                        PciCommandWord =
                            Adapter->
                            PciCommandWord & ~CMD_MEM_WRT_INVALIDATE;

                        WritePciConfigWord(PCI_COMMAND_REGISTER,
                                           &PciCommandWord);
                }

                E1000_WRITE_REG(Rctl, E1000_RCTL_RST);

                DelayInMilliseconds(5);
        }

        em_init_rx_addresses(Adapter);

        if (Adapter->MacType == MAC_WISEMAN_2_0) {
                E1000_WRITE_REG(Rctl, 0);

                DelayInMilliseconds(1);

                if (Adapter->PciCommandWord & CMD_MEM_WRT_INVALIDATE) {
                        WritePciConfigWord(PCI_COMMAND_REGISTER,
                                           &Adapter->PciCommandWord);
                }

        }

        DEBUGOUT("Zeroing the MTA\n");
        for (i = 0; i < E1000_MC_TBL_SIZE; i++) {
                E1000_WRITE_REG(Mta[i], 0);
        }

        Status = em_setup_flow_control_and_link(Adapter);

        em_clear_hw_stats_counters(Adapter);

        return (Status);
}

void em_init_rx_addresses(struct adapter *Adapter)
 {
        u32 i;
        u32 HwLowAddress;
        u32 HwHighAddress;

        DEBUGFUNC("em_init_rx_addresses")

            DEBUGOUT("Programming IA into RAR[0]\n");
        HwLowAddress = (Adapter->CurrentNetAddress[0] |
                        (Adapter->CurrentNetAddress[1] << 8) |
                        (Adapter->CurrentNetAddress[2] << 16) |
                        (Adapter->CurrentNetAddress[3] << 24));

        HwHighAddress = (Adapter->CurrentNetAddress[4] |
                         (Adapter->CurrentNetAddress[5] << 8) |
                         E1000_RAH_AV);

        E1000_WRITE_REG(Rar[0].Low, HwLowAddress);
        E1000_WRITE_REG(Rar[0].High, HwHighAddress);

        DEBUGOUT("Clearing RAR[1-15]\n");
        for (i = 1; i < E1000_RAR_ENTRIES; i++) {
                E1000_WRITE_REG(Rar[i].Low, 0);
                E1000_WRITE_REG(Rar[i].High, 0);
        }

        return;
}

void em_multicast_address_list_update(struct adapter *Adapter,
                                      u8 * MulticastAddressList,
                                      u32 MulticastAddressCount,
                                      u32 Padding)
{

        u32 HashValue;
        u32 i;
        u32 RarUsedCount = 1;

        DEBUGFUNC("em_multicast_address_list_update");

        Adapter->NumberOfMcAddresses = MulticastAddressCount;

        DEBUGOUT(" Clearing RAR[1-15]\n");
        for (i = RarUsedCount; i < E1000_RAR_ENTRIES; i++) {
                E1000_WRITE_REG(Rar[i].Low, 0);
                E1000_WRITE_REG(Rar[i].High, 0);
        }

        DEBUGOUT(" Clearing MTA\n");
        for (i = 0; i < E1000_NUM_MTA_REGISTERS; i++) {
                E1000_WRITE_REG(Mta[i], 0);
        }

        for (i = 0; i < MulticastAddressCount; i++) {
                DEBUGOUT(" Adding the multicast addresses:\n");
                DEBUGOUT7(" MC Addr #%d =%.2X %.2X %.2X %.2X %.2X %.2X\n",
                          i,
                          MulticastAddressList[i *
                                               (ETH_LENGTH_OF_ADDRESS +
                                                Padding)],
                          MulticastAddressList[i *
                                               (ETH_LENGTH_OF_ADDRESS +
                                                Padding) + 1],
                          MulticastAddressList[i *
                                               (ETH_LENGTH_OF_ADDRESS +
                                                Padding) + 2],
                          MulticastAddressList[i *
                                               (ETH_LENGTH_OF_ADDRESS +
                                                Padding) + 3],
                          MulticastAddressList[i *
                                               (ETH_LENGTH_OF_ADDRESS +
                                                Padding) + 4],
                          MulticastAddressList[i *
                                               (ETH_LENGTH_OF_ADDRESS +
                                                Padding) + 5]);

                HashValue = em_hash_multicast_address(Adapter,
                                                      MulticastAddressList
                                                      +
                                                      (i *
                                                       (ETH_LENGTH_OF_ADDRESS
                                                        + Padding)));

                DEBUGOUT1(" Hash value = 0x%03X\n", HashValue);

                if (RarUsedCount < E1000_RAR_ENTRIES) {
                        em_rar_set(Adapter,
                                   MulticastAddressList +
                                   (i * (ETH_LENGTH_OF_ADDRESS + Padding)),
                                   RarUsedCount);
                        RarUsedCount++;
                } else {
                        em_mta_set(Adapter, HashValue);
                }

        }

        DEBUGOUT("MC Update Complete\n");
}

u32 em_hash_multicast_address(struct adapter *Adapter,
                              u8 * MulticastAddress)
 {
        u32 HashValue = 0;

        switch (Adapter->MulticastFilterType) {

        case 0:

                HashValue = ((MulticastAddress[4] >> 4) |
                             (((u16) MulticastAddress[5]) << 4));

                break;

        case 1:
                HashValue = ((MulticastAddress[4] >> 3) |
                             (((u16) MulticastAddress[5]) << 5));

                break;

        case 2:
                HashValue = ((MulticastAddress[4] >> 2) |
                             (((u16) MulticastAddress[5]) << 6));

                break;

        case 3:
                HashValue = ((MulticastAddress[4]) |
                             (((u16) MulticastAddress[5]) << 8));

                break;
        }

        HashValue &= 0xFFF;
        return (HashValue);

}

void em_mta_set(struct adapter *Adapter, u32 HashValue)
{
        u32 HashBit, HashReg;
        u32 MtaRegisterValue;
        u32 Temp;

        HashReg = (HashValue >> 5) & 0x7F;
        HashBit = HashValue & 0x1F;

        MtaRegisterValue = E1000_READ_REG(Mta[(HashReg)]);

        MtaRegisterValue |= (1 << HashBit);

        if ((Adapter->MacType == MAC_CORDOVA) && ((HashReg & 0x1) == 1)) {
                Temp = E1000_READ_REG(Mta[HashReg - 1]);
                E1000_WRITE_REG(Mta[HashReg], HashValue);
                E1000_WRITE_REG(Mta[HashReg - 1], Temp);
        } else {
                E1000_WRITE_REG(Mta[HashReg], MtaRegisterValue);
        }

}

void em_rar_set(struct adapter *Adapter,
                u8 * MulticastAddress, u32 RarIndex)
{
        u32 RarLow, RarHigh;

        RarLow = ((u32) MulticastAddress[0] |
                  ((u32) MulticastAddress[1] << 8) |
                  ((u32) MulticastAddress[2] << 16) |
                  ((u32) MulticastAddress[3] << 24));

        RarHigh = ((u32) MulticastAddress[4] |
                   ((u32) MulticastAddress[5] << 8) | E1000_RAH_AV);

        E1000_WRITE_REG(Rar[RarIndex].Low, RarLow);
        E1000_WRITE_REG(Rar[RarIndex].High, RarHigh);
}

void em_write_vfta(struct adapter *Adapter, u32 Offset, u32 Value)
 {
        u32 Temp;

        if ((Adapter->MacType == MAC_CORDOVA) && ((Offset & 0x1) == 1)) {
                Temp = E1000_READ_REG(Vfta[Offset - 1]);
                E1000_WRITE_REG(Vfta[Offset], Value);
                E1000_WRITE_REG(Vfta[Offset - 1], Temp);
        } else {
                E1000_WRITE_REG(Vfta[Offset], Value);
        }
}

void em_clear_vfta(struct adapter *Adapter)
 {
        u32 Offset;

        for (Offset = 0; Offset < E1000_VLAN_FILTER_TBL_SIZE; Offset++)
                E1000_WRITE_REG(Vfta[Offset], 0);

}

u8 em_setup_flow_control_and_link(struct adapter *Adapter)
 {
        u32 TempEepromWord;
        u32 DeviceControlReg;
        u32 ExtDevControlReg;
        u8 Status = 1;

        DEBUGFUNC("em_setup_flow_control_and_link")

            TempEepromWord =
            em_read_eeprom_word(Adapter, EEPROM_INIT_CONTROL1_REG);

        DeviceControlReg =
            (((TempEepromWord & EEPROM_WORD0A_SWDPIO) << SWDPIO_SHIFT) |
             ((TempEepromWord & EEPROM_WORD0A_ILOS) << ILOS_SHIFT));

        if (Adapter->DmaFairness)
                DeviceControlReg |= E1000_CTRL_PRIOR;

        TempEepromWord =
            em_read_eeprom_word(Adapter, EEPROM_INIT_CONTROL2_REG);

        if (Adapter->FlowControl > FLOW_CONTROL_FULL) {
                if ((TempEepromWord & EEPROM_WORD0F_PAUSE_MASK) == 0)
                        Adapter->FlowControl = FLOW_CONTROL_NONE;
                else if ((TempEepromWord & EEPROM_WORD0F_PAUSE_MASK) ==
                         EEPROM_WORD0F_ASM_DIR) Adapter->FlowControl =
                            FLOW_CONTROL_TRANSMIT_PAUSE;
                else
                        Adapter->FlowControl = FLOW_CONTROL_FULL;
        }

        Adapter->OriginalFlowControl = Adapter->FlowControl;

        if (Adapter->MacType == MAC_WISEMAN_2_0)
                Adapter->FlowControl &= (~FLOW_CONTROL_TRANSMIT_PAUSE);

        if ((Adapter->MacType < MAC_LIVENGOOD)
            && (Adapter->ReportTxEarly == 1))
                Adapter->FlowControl &= (~FLOW_CONTROL_RECEIVE_PAUSE);

        DEBUGOUT1("After fix-ups FlowControl is now = %x\n",
                  Adapter->FlowControl);

        if (Adapter->MacType >= MAC_LIVENGOOD) {
                ExtDevControlReg =
                    ((TempEepromWord & EEPROM_WORD0F_SWPDIO_EXT) <<
                     SWDPIO__EXT_SHIFT);
                E1000_WRITE_REG(Exct, ExtDevControlReg);
        }

        if (Adapter->MacType >= MAC_LIVENGOOD) {
                if (Adapter->MediaType == MEDIA_TYPE_FIBER) {
                        Status =
                            em_setup_pcs_link(Adapter, DeviceControlReg);

                } else {

                        Status = em_phy_setup(Adapter, DeviceControlReg);
                }
        } else {
                Status = em_setup_pcs_link(Adapter, DeviceControlReg);
        }

        DEBUGOUT
            ("Initializing the Flow Control address, type and timer regs\n");

        E1000_WRITE_REG(Fcal, FLOW_CONTROL_ADDRESS_LOW);
        E1000_WRITE_REG(Fcah, FLOW_CONTROL_ADDRESS_HIGH);
        E1000_WRITE_REG(Fct, FLOW_CONTROL_TYPE);
        E1000_WRITE_REG(Fcttv, Adapter->FlowControlPauseTime);

        if (!(Adapter->FlowControl & FLOW_CONTROL_TRANSMIT_PAUSE)) {
                E1000_WRITE_REG(Fcrtl, 0);
                E1000_WRITE_REG(Fcrth, 0);
        } else {

                if (Adapter->FlowControlSendXon) {
                        E1000_WRITE_REG(Fcrtl,
                                        (Adapter->
                                         FlowControlLowWatermark |
                                         E1000_FCRTL_XONE));
                        E1000_WRITE_REG(Fcrth,
                                        Adapter->FlowControlHighWatermark);
                } else {
                        E1000_WRITE_REG(Fcrtl,
                                        Adapter->FlowControlLowWatermark);
                        E1000_WRITE_REG(Fcrth,
                                        Adapter->FlowControlHighWatermark);
                }
        }

        return (Status);
}

u8 em_setup_pcs_link(struct adapter * Adapter, u32 DeviceControlReg)
 {
        u32 i;
        u32 StatusContents;
        u32 TctlReg;
        u32 TransmitConfigWord;
        u32 Shift32;

        DEBUGFUNC("em_setup_pcs_link")

            TctlReg = E1000_READ_REG(Tctl);
        Shift32 = E1000_FDX_COLLISION_DISTANCE;
        Shift32 <<= E1000_COLD_SHIFT;
        TctlReg |= Shift32;
        E1000_WRITE_REG(Tctl, TctlReg);

        switch (Adapter->FlowControl) {
        case FLOW_CONTROL_NONE:

                TransmitConfigWord = (E1000_TXCW_ANE | E1000_TXCW_FD);

                break;

        case FLOW_CONTROL_RECEIVE_PAUSE:

                TransmitConfigWord =
                    (E1000_TXCW_ANE | E1000_TXCW_FD |
                     E1000_TXCW_PAUSE_MASK);

                break;

        case FLOW_CONTROL_TRANSMIT_PAUSE:

                TransmitConfigWord =
                    (E1000_TXCW_ANE | E1000_TXCW_FD | E1000_TXCW_ASM_DIR);

                break;

        case FLOW_CONTROL_FULL:

                TransmitConfigWord =
                    (E1000_TXCW_ANE | E1000_TXCW_FD |
                     E1000_TXCW_PAUSE_MASK);

                break;

        default:

                DEBUGOUT("Flow control param set incorrectly\n");
                ASSERT(0);
                break;
        }

        DEBUGOUT("Auto-negotiation enabled\n");

        E1000_WRITE_REG(Txcw, TransmitConfigWord);
        E1000_WRITE_REG(Ctrl, DeviceControlReg);

        Adapter->TxcwRegValue = TransmitConfigWord;
        DelayInMilliseconds(1);

        if (!(E1000_READ_REG(Ctrl) & E1000_CTRL_SWDPIN1)) {

                DEBUGOUT("Looking for Link\n");
                for (i = 0; i < (LINK_UP_TIMEOUT / 10); i++) {
                        DelayInMilliseconds(10);

                        StatusContents = E1000_READ_REG(Status);
                        if (StatusContents & E1000_STATUS_LU)
                                break;
                }

                if (i == (LINK_UP_TIMEOUT / 10)) {

                        DEBUGOUT
                            ("Never got a valid link from auto-neg!!!\n");

                        Adapter->AutoNegFailed = 1;
                        em_check_for_link(Adapter);
                        Adapter->AutoNegFailed = 0;
                } else {
                        Adapter->AutoNegFailed = 0;
                        DEBUGOUT("Valid Link Found\n");
                }
        } else {
                DEBUGOUT("No Signal Detected\n");
        }

        return (1);
}

void em_config_flow_control_after_link_up(struct adapter *Adapter)
 {
        u16 MiiStatusReg, MiiNWayAdvertiseReg, MiiNWayBasePgAbleReg;
        u16 Speed, Duplex;

        DEBUGFUNC("em_config_flow_control_after_link_up")

            if (
                ((Adapter->MediaType == MEDIA_TYPE_FIBER)
                 && (Adapter->AutoNegFailed))
                || ((Adapter->MediaType == MEDIA_TYPE_COPPER)
                    && (!Adapter->AutoNeg))) {
                em_force_mac_flow_control_setting(Adapter);
        }

        if ((Adapter->MediaType == MEDIA_TYPE_COPPER) && Adapter->AutoNeg) {

                MiiStatusReg = em_read_phy_register(Adapter,
                                                    PHY_MII_STATUS_REG,
                                                    Adapter->PhyAddress);

                MiiStatusReg = em_read_phy_register(Adapter,
                                                    PHY_MII_STATUS_REG,
                                                    Adapter->PhyAddress);

                if (MiiStatusReg & MII_SR_AUTONEG_COMPLETE) {

                        MiiNWayAdvertiseReg = em_read_phy_register(Adapter,
                                                                   PHY_AUTONEG_ADVERTISEMENT,
                                                                   Adapter->
                                                                   PhyAddress);

                        MiiNWayBasePgAbleReg =
                            em_read_phy_register(Adapter,
                                                 PHY_AUTONEG_LP_BPA,
                                                 Adapter->PhyAddress);

                        if ((MiiNWayAdvertiseReg & NWAY_AR_PAUSE) &&
                            (MiiNWayBasePgAbleReg & NWAY_LPAR_PAUSE)) {

                                if (Adapter->OriginalFlowControl ==
                                    FLOW_CONTROL_FULL) {
                                        Adapter->FlowControl =
                                            FLOW_CONTROL_FULL;
                                        DEBUGOUT
                                            ("Flow Control = FULL.\r\n");
                                } else {
                                        Adapter->FlowControl =
                                            FLOW_CONTROL_RECEIVE_PAUSE;
                                        DEBUGOUT
                                            ("Flow Control = RX PAUSE frames only.\r\n");
                                }
                        }

                        else if (!(MiiNWayAdvertiseReg & NWAY_AR_PAUSE) &&
                                 (MiiNWayAdvertiseReg & NWAY_AR_ASM_DIR) &&
                                 (MiiNWayBasePgAbleReg & NWAY_LPAR_PAUSE)
                                 && (MiiNWayBasePgAbleReg &
                                     NWAY_LPAR_ASM_DIR)) {
                                Adapter->FlowControl =
                                    FLOW_CONTROL_TRANSMIT_PAUSE;
                                DEBUGOUT
                                    ("Flow Control = TX PAUSE frames only.\r\n");
                        }

                        else if ((MiiNWayAdvertiseReg & NWAY_AR_PAUSE) &&
                                 (MiiNWayAdvertiseReg & NWAY_AR_ASM_DIR) &&
                                 !(MiiNWayBasePgAbleReg & NWAY_LPAR_PAUSE)
                                 && (MiiNWayBasePgAbleReg &
                                     NWAY_LPAR_ASM_DIR)) {
                                Adapter->FlowControl =
                                    FLOW_CONTROL_RECEIVE_PAUSE;
                                DEBUGOUT
                                    ("Flow Control = RX PAUSE frames only.\r\n");
                        }

                        else if (Adapter->OriginalFlowControl ==
                                 FLOW_CONTROL_NONE
                                 || Adapter->OriginalFlowControl ==
                                 FLOW_CONTROL_TRANSMIT_PAUSE) {
                                Adapter->FlowControl = FLOW_CONTROL_NONE;
                                DEBUGOUT("Flow Control = NONE.\r\n");
                        } else {
                                Adapter->FlowControl =
                                    FLOW_CONTROL_RECEIVE_PAUSE;
                                DEBUGOUT
                                    ("Flow Control = RX PAUSE frames only.\r\n");
                        }

                        em_get_speed_and_duplex(Adapter, &Speed, &Duplex);

                        if (Duplex == HALF_DUPLEX)
                                Adapter->FlowControl = FLOW_CONTROL_NONE;

                        em_force_mac_flow_control_setting(Adapter);
                } else {
                        DEBUGOUT
                            ("Copper PHY and Auto Neg has not completed.\r\n");
                }
        }
}

void em_force_mac_flow_control_setting(struct adapter *Adapter)
 {
        u32 CtrlRegValue;

        DEBUGFUNC("em_force_mac_flow_control_setting")

            CtrlRegValue = E1000_READ_REG(Ctrl);

        switch (Adapter->FlowControl) {
        case FLOW_CONTROL_NONE:

                CtrlRegValue &= (~(E1000_CTRL_TFCE | E1000_CTRL_RFCE));
                break;

        case FLOW_CONTROL_RECEIVE_PAUSE:

                CtrlRegValue &= (~E1000_CTRL_TFCE);
                CtrlRegValue |= E1000_CTRL_RFCE;
                break;

        case FLOW_CONTROL_TRANSMIT_PAUSE:

                CtrlRegValue &= (~E1000_CTRL_RFCE);
                CtrlRegValue |= E1000_CTRL_TFCE;
                break;

        case FLOW_CONTROL_FULL:

                CtrlRegValue |= (E1000_CTRL_TFCE | E1000_CTRL_RFCE);
                break;

        default:

                DEBUGOUT("Flow control param set incorrectly\n");
                ASSERT(0);

                break;
        }

        if (Adapter->MacType == MAC_WISEMAN_2_0)
                CtrlRegValue &= (~E1000_CTRL_TFCE);

        E1000_WRITE_REG(Ctrl, CtrlRegValue);
}

void em_check_for_link(struct adapter *Adapter)
 {
        u32 RxcwRegValue;
        u32 CtrlRegValue;
        u32 StatusRegValue;
        u32 RctlRegValue;
        u16 PhyData;
        u16 LinkPartnerCapability;

        DEBUGFUNC("em_check_for_link")

            CtrlRegValue = E1000_READ_REG(Ctrl);

        StatusRegValue = E1000_READ_REG(Status);

        RxcwRegValue = E1000_READ_REG(Rxcw);

        if (Adapter->MediaType == MEDIA_TYPE_COPPER &&
            Adapter->GetLinkStatus) {

                PhyData = em_read_phy_register(Adapter,
                                               PHY_MII_STATUS_REG,
                                               Adapter->PhyAddress);

                PhyData = em_read_phy_register(Adapter,
                                               PHY_MII_STATUS_REG,
                                               Adapter->PhyAddress);

                if (PhyData & MII_SR_LINK_STATUS) {
                        Adapter->GetLinkStatus = 0;
                } else {
                        DEBUGOUT("**** CFL - No link detected. ****\r\n");
                        return;
                }

                if (!Adapter->AutoNeg) {
                        return;
                }

                switch (Adapter->PhyId) {
                case PAXSON_PHY_88E1000:
                case PAXSON_PHY_88E1000S:
                case PAXSON_PHY_INTEGRATED:

                        if (Adapter->MacType > MAC_WAINWRIGHT) {
                                DEBUGOUT
                                    ("CFL - Auto-Neg complete.  Configuring Collision Distance.");
                                em_configure_collision_distance(Adapter);
                        } else {

                                PhyData = em_read_phy_register(Adapter,
                                                               PXN_PHY_SPEC_STAT_REG,
                                                               Adapter->
                                                               PhyAddress);

                                DEBUGOUT1
                                    ("CFL - Auto-Neg complete.  PhyData = %x\r\n",
                                     PhyData);
                                em_configure_mac_to_phy_settings(Adapter,
                                                                 PhyData);
                        }

                        em_config_flow_control_after_link_up(Adapter);
                        break;

                default:
                        DEBUGOUT("CFL - Invalid PHY detected.\r\n");

                }

                if (Adapter->TbiCompatibilityEnable) {
                        LinkPartnerCapability =
                            em_read_phy_register(Adapter,
                                                 PHY_AUTONEG_LP_BPA,
                                                 Adapter->PhyAddress);
                        if (LinkPartnerCapability &
                            (NWAY_LPAR_10T_HD_CAPS | NWAY_LPAR_10T_FD_CAPS
                             | NWAY_LPAR_100TX_HD_CAPS |
                             NWAY_LPAR_100TX_FD_CAPS |
                             NWAY_LPAR_100T4_CAPS)) {

                                if (Adapter->TbiCompatibilityOn) {

                                        RctlRegValue =
                                            E1000_READ_REG(Rctl);
                                        RctlRegValue &= ~E1000_RCTL_SBP;
                                        E1000_WRITE_REG(Rctl,
                                                        RctlRegValue);
                                        Adapter->TbiCompatibilityOn = 0;
                                }
                        } else {

                                if (!Adapter->TbiCompatibilityOn) {
                                        Adapter->TbiCompatibilityOn = 1;
                                        RctlRegValue =
                                            E1000_READ_REG(Rctl);
                                        RctlRegValue |= E1000_RCTL_SBP;
                                        E1000_WRITE_REG(Rctl,
                                                        RctlRegValue);
                                }
                        }
                }
        }

        else
            if ((Adapter->MediaType == MEDIA_TYPE_FIBER) &&
                (!(StatusRegValue & E1000_STATUS_LU)) &&
                (!(CtrlRegValue & E1000_CTRL_SWDPIN1)) &&
                (!(RxcwRegValue & E1000_RXCW_C))) {
                if (Adapter->AutoNegFailed == 0) {
                        Adapter->AutoNegFailed = 1;
                        return;
                }

                DEBUGOUT
                    ("NOT RXing /C/, disable AutoNeg and force link.\r\n");

                E1000_WRITE_REG(Txcw,
                                (Adapter->TxcwRegValue & ~E1000_TXCW_ANE));

                CtrlRegValue = E1000_READ_REG(Ctrl);
                CtrlRegValue |= (E1000_CTRL_SLU | E1000_CTRL_FD);
                E1000_WRITE_REG(Ctrl, CtrlRegValue);

                em_config_flow_control_after_link_up(Adapter);

        }
                else if ((Adapter->MediaType == MEDIA_TYPE_FIBER) &&
                         (CtrlRegValue & E1000_CTRL_SLU) &&
                         (RxcwRegValue & E1000_RXCW_C)) {

                DEBUGOUT
                    ("RXing /C/, enable AutoNeg and stop forcing link.\r\n");

                E1000_WRITE_REG(Txcw, Adapter->TxcwRegValue);

                E1000_WRITE_REG(Ctrl, (CtrlRegValue & ~E1000_CTRL_SLU));
        }

        return;
}

void em_clear_hw_stats_counters(struct adapter *Adapter)
 {
        volatile u32 RegisterContents;

        DEBUGFUNC("em_clear_hw_stats_counters")

            if (Adapter->AdapterStopped) {
                DEBUGOUT("Exiting because the adapter is stopped!!!\n");
                return;
        }

        RegisterContents = E1000_READ_REG(Crcerrs);
        RegisterContents = E1000_READ_REG(Symerrs);
        RegisterContents = E1000_READ_REG(Mpc);
        RegisterContents = E1000_READ_REG(Scc);
        RegisterContents = E1000_READ_REG(Ecol);
        RegisterContents = E1000_READ_REG(Mcc);
        RegisterContents = E1000_READ_REG(Latecol);
        RegisterContents = E1000_READ_REG(Colc);
        RegisterContents = E1000_READ_REG(Dc);
        RegisterContents = E1000_READ_REG(Sec);
        RegisterContents = E1000_READ_REG(Rlec);
        RegisterContents = E1000_READ_REG(Xonrxc);
        RegisterContents = E1000_READ_REG(Xontxc);
        RegisterContents = E1000_READ_REG(Xoffrxc);
        RegisterContents = E1000_READ_REG(Xofftxc);
        RegisterContents = E1000_READ_REG(Fcruc);
        RegisterContents = E1000_READ_REG(Prc64);
        RegisterContents = E1000_READ_REG(Prc127);
        RegisterContents = E1000_READ_REG(Prc255);
        RegisterContents = E1000_READ_REG(Prc511);
        RegisterContents = E1000_READ_REG(Prc1023);
        RegisterContents = E1000_READ_REG(Prc1522);
        RegisterContents = E1000_READ_REG(Gprc);
        RegisterContents = E1000_READ_REG(Bprc);
        RegisterContents = E1000_READ_REG(Mprc);
        RegisterContents = E1000_READ_REG(Gptc);
        RegisterContents = E1000_READ_REG(Gorl);
        RegisterContents = E1000_READ_REG(Gorh);
        RegisterContents = E1000_READ_REG(Gotl);
        RegisterContents = E1000_READ_REG(Goth);
        RegisterContents = E1000_READ_REG(Rnbc);
        RegisterContents = E1000_READ_REG(Ruc);
        RegisterContents = E1000_READ_REG(Rfc);
        RegisterContents = E1000_READ_REG(Roc);
        RegisterContents = E1000_READ_REG(Rjc);
        RegisterContents = E1000_READ_REG(Torl);
        RegisterContents = E1000_READ_REG(Torh);
        RegisterContents = E1000_READ_REG(Totl);
        RegisterContents = E1000_READ_REG(Toth);
        RegisterContents = E1000_READ_REG(Tpr);
        RegisterContents = E1000_READ_REG(Tpt);
        RegisterContents = E1000_READ_REG(Ptc64);
        RegisterContents = E1000_READ_REG(Ptc127);
        RegisterContents = E1000_READ_REG(Ptc255);
        RegisterContents = E1000_READ_REG(Ptc511);
        RegisterContents = E1000_READ_REG(Ptc1023);
        RegisterContents = E1000_READ_REG(Ptc1522);
        RegisterContents = E1000_READ_REG(Mptc);
        RegisterContents = E1000_READ_REG(Bptc);

        if (Adapter->MacType < MAC_LIVENGOOD)
                return;

        RegisterContents = E1000_READ_REG(Algnerrc);
        RegisterContents = E1000_READ_REG(Rxerrc);
        RegisterContents = E1000_READ_REG(Tuc);
        RegisterContents = E1000_READ_REG(Tncrs);
        RegisterContents = E1000_READ_REG(Cexterr);
        RegisterContents = E1000_READ_REG(Rutec);

        RegisterContents = E1000_READ_REG(Tsctc);
        RegisterContents = E1000_READ_REG(Tsctfc);

}

void em_get_speed_and_duplex(struct adapter *Adapter,
                             u16 * Speed, u16 * Duplex)
 {
        u32 DeviceStatusReg;

        DEBUGFUNC("em_get_speed_and_duplex")

            if (Adapter->AdapterStopped) {
                *Speed = 0;
                *Duplex = 0;
                return;
        }

        if (Adapter->MacType >= MAC_LIVENGOOD) {
                DEBUGOUT("Livengood MAC\n");
                DeviceStatusReg = E1000_READ_REG(Status);
                if (DeviceStatusReg & E1000_STATUS_SPEED_1000) {
                        *Speed = SPEED_1000;
                        DEBUGOUT("   1000 Mbs\n");
                } else if (DeviceStatusReg & E1000_STATUS_SPEED_100) {
                        *Speed = SPEED_100;
                        DEBUGOUT("   100 Mbs\n");
                } else {
                        *Speed = SPEED_10;
                        DEBUGOUT("   10 Mbs\n");
                }

                if (DeviceStatusReg & E1000_STATUS_FD) {
                        *Duplex = FULL_DUPLEX;
                        DEBUGOUT("   Full Duplex\r\n");
                } else {
                        *Duplex = HALF_DUPLEX;
                        DEBUGOUT("   Half Duplex\r\n");
                }
        } else {
                DEBUGOUT("Wiseman MAC - 1000 Mbs, Full Duplex\r\n");
                *Speed = SPEED_1000;
                *Duplex = FULL_DUPLEX;
        }

        return;
}

void em_setup_eeprom(struct adapter *Adapter)
{
        u32 val;

        val = E1000_READ_REG(Eecd);

        val &= ~(E1000_EESK | E1000_EEDI);
        E1000_WRITE_REG(Eecd, val);

        val |= E1000_EECS;
        E1000_WRITE_REG(Eecd, val);
}

void em_standby_eeprom(struct adapter *Adapter)
{
        u32 val;

        val = E1000_READ_REG(Eecd);

        val &= ~(E1000_EECS | E1000_EESK);
        E1000_WRITE_REG(Eecd, val);
        DelayInMicroseconds(50);

        val |= E1000_EESK;
        E1000_WRITE_REG(Eecd, val);
        DelayInMicroseconds(50);

        val |= E1000_EECS;
        E1000_WRITE_REG(Eecd, val);
        DelayInMicroseconds(50);

        val &= ~E1000_EESK;
        E1000_WRITE_REG(Eecd, val);
        DelayInMicroseconds(50);
}

void em_clock_eeprom(struct adapter *Adapter)
{
        u32 val;

        val = E1000_READ_REG(Eecd);

        val |= E1000_EESK;
        E1000_WRITE_REG(Eecd, val);
        DelayInMicroseconds(50);

        val &= ~E1000_EESK;
        E1000_WRITE_REG(Eecd, val);
        DelayInMicroseconds(50);
}

void em_cleanup_eeprom(struct adapter *Adapter)
{
        u32 val;

        val = E1000_READ_REG(Eecd);

        val &= ~(E1000_EECS | E1000_EEDI);

        E1000_WRITE_REG(Eecd, val);

        em_clock_eeprom(Adapter);
}

u16 em_read_eeprom_word(struct adapter *Adapter, u16 Reg)
 {
        u16 Data;

        ASSERT(Reg < EEPROM_WORD_SIZE);

        E1000_WRITE_REG(Eecd, E1000_EECS);

        em_shift_out_bits(Adapter, EEPROM_READ_OPCODE, 3);
        em_shift_out_bits(Adapter, Reg, 6);

        Data = em_shift_in_bits(Adapter);

        em_eeprom_cleanup(Adapter);
        return (Data);
}

static void em_shift_out_bits(struct adapter *Adapter, u16 Data, u16 Count)
 {
        u32 EecdRegValue;
        u32 Mask;

        Mask = 0x01 << (Count - 1);

        EecdRegValue = E1000_READ_REG(Eecd);

        EecdRegValue &= ~(E1000_EEDO | E1000_EEDI);

        do {

                EecdRegValue &= ~E1000_EEDI;

                if (Data & Mask)
                        EecdRegValue |= E1000_EEDI;

                E1000_WRITE_REG(Eecd, EecdRegValue);

                DelayInMicroseconds(50);

                em_raise_clock(Adapter, &EecdRegValue);
                em_lower_clock(Adapter, &EecdRegValue);

                Mask = Mask >> 1;

        } while (Mask);

        EecdRegValue &= ~E1000_EEDI;

        E1000_WRITE_REG(Eecd, EecdRegValue);
}

static void em_raise_clock(struct adapter *Adapter, u32 * EecdRegValue)
 {

        *EecdRegValue = *EecdRegValue | E1000_EESK;

        E1000_WRITE_REG(Eecd, *EecdRegValue);

        DelayInMicroseconds(50);
}

static void em_lower_clock(struct adapter *Adapter, u32 * EecdRegValue)
 {

        *EecdRegValue = *EecdRegValue & ~E1000_EESK;

        E1000_WRITE_REG(Eecd, *EecdRegValue);

        DelayInMicroseconds(50);
}

static u16 em_shift_in_bits(struct adapter *Adapter)
 {
        u32 EecdRegValue;
        u32 i;
        u16 Data;

        EecdRegValue = E1000_READ_REG(Eecd);

        EecdRegValue &= ~(E1000_EEDO | E1000_EEDI);
        Data = 0;

        for (i = 0; i < 16; i++) {
                Data = Data << 1;
                em_raise_clock(Adapter, &EecdRegValue);

                EecdRegValue = E1000_READ_REG(Eecd);

                EecdRegValue &= ~(E1000_EEDI);
                if (EecdRegValue & E1000_EEDO)
                        Data |= 1;

                em_lower_clock(Adapter, &EecdRegValue);
        }

        return (Data);
}

static void em_eeprom_cleanup(struct adapter *Adapter)
 {
        u32 EecdRegValue;

        EecdRegValue = E1000_READ_REG(Eecd);

        EecdRegValue &= ~(E1000_EECS | E1000_EEDI);

        E1000_WRITE_REG(Eecd, EecdRegValue);

        em_raise_clock(Adapter, &EecdRegValue);
        em_lower_clock(Adapter, &EecdRegValue);
}

u8 em_validate_eeprom_checksum(struct adapter *Adapter)
 {
        u16 Checksum = 0;
        u16 Iteration;

        for (Iteration = 0; Iteration < (EEPROM_CHECKSUM_REG + 1);
             Iteration++)
                Checksum += em_read_eeprom_word(Adapter, Iteration);

        if (Checksum == (u16) EEPROM_SUM)
                return (1);
        else
                return (0);
}

void em_update_eeprom_checksum(struct adapter *Adapter)
 {
        u16 Checksum = 0;
        u16 Iteration;

        for (Iteration = 0; Iteration < EEPROM_CHECKSUM_REG; Iteration++)
                Checksum += em_read_eeprom_word(Adapter, Iteration);

        Checksum = (u16) EEPROM_SUM - Checksum;

        em_write_eeprom_word(Adapter, EEPROM_CHECKSUM_REG, Checksum);
}

u8 em_write_eeprom_word(struct adapter *Adapter, u16 Reg, u16 Data)
 {

        em_setup_eeprom(Adapter);

        em_shift_out_bits(Adapter, EEPROM_EWEN_OPCODE, 5);
        em_shift_out_bits(Adapter, 0, 4);

        em_standby_eeprom(Adapter);

        em_shift_out_bits(Adapter, EEPROM_WRITE_OPCODE, 3);
        em_shift_out_bits(Adapter, Reg, 6);

        em_shift_out_bits(Adapter, Data, 16);

        em_wait_eeprom_command_done(Adapter);

        em_standby_eeprom(Adapter);

        em_shift_out_bits(Adapter, EEPROM_EWDS_OPCODE, 5);
        em_shift_out_bits(Adapter, 0, 4);

        em_cleanup_eeprom(Adapter);

        return (1);
}

static u16 em_wait_eeprom_command_done(struct adapter *Adapter)
 {
        u32 EecdRegValue;
        u32 i;

        em_stand_by(Adapter);

        for (i = 0; i < 200; i++) {
                EecdRegValue = E1000_READ_REG(Eecd);

                if (EecdRegValue & E1000_EEDO)
                        return (1);

                DelayInMicroseconds(50);
        }
        ASSERT(0);
        return (0);
}

static void em_stand_by(struct adapter *Adapter)
 {
        u32 EecdRegValue;

        EecdRegValue = E1000_READ_REG(Eecd);

        EecdRegValue &= ~(E1000_EECS | E1000_EESK);

        E1000_WRITE_REG(Eecd, EecdRegValue);

        DelayInMicroseconds(5);

        EecdRegValue |= E1000_EECS;

        E1000_WRITE_REG(Eecd, EecdRegValue);
}

u8 em_read_part_number(struct adapter *Adapter, u32 * PartNumber)
{
        u16 EepromWordValue;

        DEBUGFUNC("em_read_part_number")

            if (Adapter->AdapterStopped) {
                *PartNumber = 0;
                return (0);
        }

        EepromWordValue = em_read_eeprom_word(Adapter,
                                              (u16) (EEPROM_PBA_BYTE_1));

        DEBUGOUT("Read first part number word\n");

        *PartNumber = (u32) EepromWordValue;
        *PartNumber = *PartNumber << 16;

        EepromWordValue = em_read_eeprom_word(Adapter,
                                              (u16) (EEPROM_PBA_BYTE_1 +
                                                     1));

        DEBUGOUT("Read second part number word\n");

        *PartNumber |= EepromWordValue;

        return (1);

}

void em_id_led_on(struct adapter *Adapter)
{
        u32 CtrlRegValue;

        if (Adapter->AdapterStopped) {
                return;
        }

        CtrlRegValue = E1000_READ_REG(Ctrl);

        CtrlRegValue |= E1000_CTRL_SWDPIO0;

        if (em_is_low_profile(Adapter)) {
                CtrlRegValue &= ~E1000_CTRL_SWDPIN0;
        } else {
                CtrlRegValue |= E1000_CTRL_SWDPIN0;
        }

        E1000_WRITE_REG(Ctrl, CtrlRegValue);

}

void em_id_led_off(struct adapter *Adapter)
{
        u32 CtrlRegValue;

        if (Adapter->AdapterStopped) {
                return;
        }

        CtrlRegValue = E1000_READ_REG(Ctrl);

        CtrlRegValue |= E1000_CTRL_SWDPIO0;

        if (em_is_low_profile(Adapter)) {
                CtrlRegValue |= E1000_CTRL_SWDPIN0;
        } else {
                CtrlRegValue &= ~E1000_CTRL_SWDPIN0;
        }

        E1000_WRITE_REG(Ctrl, CtrlRegValue);
}

void em_set_id_led_for_pc_ix(struct adapter *Adapter)
{
        u32 PciStatus;

        PciStatus = E1000_READ_REG(Status);

        if (PciStatus & E1000_STATUS_PCIX_MODE) {
                em_id_led_on(Adapter);
        } else {
                em_id_led_off(Adapter);
        }
}

u8 em_is_low_profile(struct adapter *Adapter)
{
        u16 LedLogicWord;
        u8 ReturnValue = 0;

        if (Adapter->MacType >= MAC_CORDOVA) {

                LedLogicWord =
                    em_read_eeprom_word(Adapter, E1000_EEPROM_LED_LOGIC);

                if (LedLogicWord & E1000_EEPROM_SWDPIN0)
                        ReturnValue = 1;
                else
                        ReturnValue = 0;
        }

        return ReturnValue;
}

void em_adjust_tbi_accepted_stats(struct adapter *Adapter,
                                  u32 FrameLength, u8 * MacAddress)
{
        u32 CarryBit;

        FrameLength--;

        Adapter->Crcerrs--;

        Adapter->Gprc++;

        CarryBit = 0x80000000 & Adapter->Gorcl;
        Adapter->Gorcl += FrameLength;

        if (CarryBit && ((Adapter->Gorcl & 0x80000000) == 0)) {
                Adapter->Gorch++;
        }

        if ((MacAddress[0] == (u8) 0xff) && (MacAddress[1] == (u8) 0xff)) {

                Adapter->Bprc++;
        } else if (*MacAddress & 0x01) {

                Adapter->Mprc++;
        }
        if (FrameLength == Adapter->MaxFrameSize) {

                Adapter->Roc += E1000_READ_REG(Roc);
                if (Adapter->Roc > 0)
                        Adapter->Roc--;
        }

        if (FrameLength == 64) {
                Adapter->Prc64++;
                Adapter->Prc127--;
        } else if (FrameLength == 127) {
                Adapter->Prc127++;
                Adapter->Prc255--;
        } else if (FrameLength == 255) {
                Adapter->Prc255++;
                Adapter->Prc511--;
        } else if (FrameLength == 511) {
                Adapter->Prc511++;
                Adapter->Prc1023--;
        } else if (FrameLength == 1023) {
                Adapter->Prc1023++;
                Adapter->Prc1522--;
        } else if (FrameLength == 1522) {
                Adapter->Prc1522++;
        }
}

void em_get_bus_type_speed_width(struct adapter *Adapter)
{
        u32 DeviceStatusReg;

        if (Adapter->MacType < MAC_LIVENGOOD) {
                Adapter->BusType = E1000_BUS_TYPE_UNKNOWN;
                Adapter->BusSpeed = E1000_BUS_SPEED_UNKNOWN;
                Adapter->BusWidth = E1000_BUS_WIDTH_UNKNOWN;
                return;
        }

        DeviceStatusReg = E1000_READ_REG(Status);

        Adapter->BusType = (DeviceStatusReg & E1000_STATUS_PCIX_MODE) ?
            E1000_BUS_TYPE_PCIX : E1000_BUS_TYPE_PCI;

        if (Adapter->BusType == E1000_BUS_TYPE_PCI) {
                Adapter->BusSpeed =
                    (DeviceStatusReg & E1000_STATUS_PCI66) ?
                    E1000_BUS_SPEED_PCI_66MHZ : E1000_BUS_SPEED_PCI_33MHZ;
        } else {
                switch (DeviceStatusReg & E1000_STATUS_PCIX_SPEED) {
                case E1000_STATUS_PCIX_SPEED_66:
                        Adapter->BusSpeed = E1000_BUS_SPEED_PCIX_50_66MHZ;
                        break;
                case E1000_STATUS_PCIX_SPEED_100:
                        Adapter->BusSpeed = E1000_BUS_SPEED_PCIX_66_100MHZ;
                        break;
                case E1000_STATUS_PCIX_SPEED_133:
                        Adapter->BusSpeed =
                            E1000_BUS_SPEED_PCIX_100_133MHZ;
                        break;
                default:
                        Adapter->BusSpeed = E1000_BUS_SPEED_PCIX_RESERVED;
                        break;
                }
        }

        Adapter->BusWidth = (DeviceStatusReg & E1000_STATUS_BUS64) ?
            E1000_BUS_WIDTH_64_BIT : E1000_BUS_WIDTH_32_BIT;

        return;
}
