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
* Workfile: phy.c 
* Date: 9/25/01 2:40p 
* Revision: 37 
*/

#include <dev/em/if_em_fxhw.h>
#include <dev/em/if_em_phy.h>

static void em_mii_shift_out_phy_data(struct adapter *Adapter,
                                      u32 Data, u16 Count);
static void em_raise_mdc_clock(struct adapter *Adapter,

                               u32 * CtrlRegValue);
static void em_lower_mdc_clock(struct adapter *Adapter,

                               u32 * CtrlRegValue);
static u16 em_mii_shift_in_phy_data(struct adapter *Adapter);
static u8 em_phy_setup_auto_neg_advertisement(struct adapter *Adapter);
static void em_phy_force_speed_and_duplex(struct adapter *Adapter);

#define GOOD_MII_IF 0

u16 em_read_phy_register(struct adapter *Adapter,
                         u32 RegAddress, u32 PhyAddress)
 {
        u32 i;
        u32 Data = 0;
        u32 Command = 0;

        ASSERT(RegAddress <= MAX_PHY_REG_ADDRESS);

        if (Adapter->MacType > MAC_LIVENGOOD) {

                Command = ((RegAddress << MDI_REGADD_SHIFT) |
                           (PhyAddress << MDI_PHYADD_SHIFT) |
                           (E1000_MDI_READ));

                E1000_WRITE_REG(Mdic, Command);

                for (i = 0; i < 32; i++) {
                        DelayInMicroseconds(10);

                        Data = E1000_READ_REG(Mdic);

                        if (Data & E1000_MDI_READY)
                                break;
                }
        } else {

                em_mii_shift_out_phy_data(Adapter, PHY_PREAMBLE,
                                          PHY_PREAMBLE_SIZE);

                Command = ((RegAddress) |
                           (PhyAddress << 5) |
                           (PHY_OP_READ << 10) | (PHY_SOF << 12));

                em_mii_shift_out_phy_data(Adapter, Command, 14);

                Data = (u32) em_mii_shift_in_phy_data(Adapter);
        }

        ASSERT(!(Data & E1000_MDI_ERR));

        return ((u16) Data);
}

void em_write_phy_register(struct adapter *Adapter,
                           u32 RegAddress, u32 PhyAddress, u16 Data)
 {
        u32 i;
        u32 Command = 0;
        u32 MdicRegValue;

        ASSERT(RegAddress <= MAX_PHY_REG_ADDRESS);

        if (Adapter->MacType > MAC_LIVENGOOD) {

                Command = (((u32) Data) |
                           (RegAddress << MDI_REGADD_SHIFT) |
                           (PhyAddress << MDI_PHYADD_SHIFT) |
                           (E1000_MDI_WRITE));

                E1000_WRITE_REG(Mdic, Command);

                for (i = 0; i < 10; i++) {
                        DelayInMicroseconds(10);

                        MdicRegValue = E1000_READ_REG(Mdic);

                        if (MdicRegValue & E1000_MDI_READY)
                                break;
                }

        } else {

                em_mii_shift_out_phy_data(Adapter, PHY_PREAMBLE,
                                          PHY_PREAMBLE_SIZE);

                Command = ((PHY_TURNAROUND) |
                           (RegAddress << 2) |
                           (PhyAddress << 7) |
                           (PHY_OP_WRITE << 12) | (PHY_SOF << 14));
                Command <<= 16;
                Command |= ((u32) Data);

                em_mii_shift_out_phy_data(Adapter, Command, 32);
        }

        return;
}

static u16 em_mii_shift_in_phy_data(struct adapter *Adapter)
 {
        u32 CtrlRegValue;
        u16 Data = 0;
        u8 i;

        CtrlRegValue = E1000_READ_REG(Ctrl);

        CtrlRegValue &= ~E1000_CTRL_MDIO_DIR;
        CtrlRegValue &= ~E1000_CTRL_MDIO;

        E1000_WRITE_REG(Ctrl, CtrlRegValue);

        em_raise_mdc_clock(Adapter, &CtrlRegValue);
        em_lower_mdc_clock(Adapter, &CtrlRegValue);

        for (Data = 0, i = 0; i < 16; i++) {
                Data = Data << 1;
                em_raise_mdc_clock(Adapter, &CtrlRegValue);

                CtrlRegValue = E1000_READ_REG(Ctrl);

                if (CtrlRegValue & E1000_CTRL_MDIO)
                        Data |= 1;

                em_lower_mdc_clock(Adapter, &CtrlRegValue);
        }

        em_raise_mdc_clock(Adapter, &CtrlRegValue);
        em_lower_mdc_clock(Adapter, &CtrlRegValue);

        CtrlRegValue &= ~E1000_CTRL_MDIO;

        return (Data);
}

static void em_mii_shift_out_phy_data(struct adapter *Adapter,
                                      u32 Data, u16 Count)
 {
        u32 CtrlRegValue;
        u32 Mask;

        if (Count > 32)
                ASSERT(0);

        Mask = 0x01;
        Mask <<= (Count - 1);

        CtrlRegValue = E1000_READ_REG(Ctrl);

        CtrlRegValue |= (E1000_CTRL_MDIO_DIR | E1000_CTRL_MDC_DIR);

        while (Mask) {

                if (Data & Mask)
                        CtrlRegValue |= E1000_CTRL_MDIO;
                else
                        CtrlRegValue &= ~E1000_CTRL_MDIO;

                E1000_WRITE_REG(Ctrl, CtrlRegValue);

                DelayInMicroseconds(2);

                em_raise_mdc_clock(Adapter, &CtrlRegValue);
                em_lower_mdc_clock(Adapter, &CtrlRegValue);

                Mask = Mask >> 1;
        }

        CtrlRegValue &= ~E1000_CTRL_MDIO;
}

static void em_raise_mdc_clock(struct adapter *Adapter, u32 * CtrlRegValue)
 {

        E1000_WRITE_REG(Ctrl, (*CtrlRegValue | E1000_CTRL_MDC));

        DelayInMicroseconds(2);
}

static void em_lower_mdc_clock(struct adapter *Adapter, u32 * CtrlRegValue)
 {

        E1000_WRITE_REG(Ctrl, (*CtrlRegValue & ~E1000_CTRL_MDC));

        DelayInMicroseconds(2);
}

void em_phy_hardware_reset(struct adapter *Adapter)
 {
        u32 ExtCtrlRegValue, CtrlRegValue;

        DEBUGFUNC("em_phy_hardware_reset")

            DEBUGOUT("Resetting Phy...\n");

        if (Adapter->MacType > MAC_LIVENGOOD) {

                CtrlRegValue = E1000_READ_REG(Ctrl);

                CtrlRegValue |= E1000_CTRL_PHY_RST;

                E1000_WRITE_REG(Ctrl, CtrlRegValue);

                DelayInMilliseconds(20);

                CtrlRegValue &= ~E1000_CTRL_PHY_RST;

                E1000_WRITE_REG(Ctrl, CtrlRegValue);

                DelayInMilliseconds(20);
        } else {

                ExtCtrlRegValue = E1000_READ_REG(Exct);

                ExtCtrlRegValue |= E1000_CTRL_PHY_RESET_DIR4;

                E1000_WRITE_REG(Exct, ExtCtrlRegValue);

                DelayInMilliseconds(20);

                ExtCtrlRegValue = E1000_READ_REG(Exct);

                ExtCtrlRegValue &= ~E1000_CTRL_PHY_RESET4;

                E1000_WRITE_REG(Exct, ExtCtrlRegValue);

                DelayInMilliseconds(20);

                ExtCtrlRegValue = E1000_READ_REG(Exct);

                ExtCtrlRegValue |= E1000_CTRL_PHY_RESET4;

                E1000_WRITE_REG(Exct, ExtCtrlRegValue);

                DelayInMilliseconds(20);
        }

        return;
}

u8 em_phy_reset(struct adapter * Adapter)
 {
        u16 RegData;
        u16 i;

        DEBUGFUNC("em_phy_reset")

            RegData = em_read_phy_register(Adapter,
                                           PHY_MII_CTRL_REG,
                                           Adapter->PhyAddress);

        RegData |= MII_CR_RESET;

        em_write_phy_register(Adapter,
                              PHY_MII_CTRL_REG,
                              Adapter->PhyAddress, RegData);

        i = 0;
        while ((RegData & MII_CR_RESET) && i++ < 500) {
                RegData = em_read_phy_register(Adapter,
                                               PHY_MII_CTRL_REG,
                                               Adapter->PhyAddress);
                DelayInMicroseconds(1);
        }

        if (i >= 500) {
                DEBUGOUT("Timeout waiting for PHY to reset.\n");
                return 0;
        }

        return 1;
}

u8 em_phy_setup(struct adapter * Adapter, u32 DeviceControlReg)
 {
        u16 MiiCtrlReg, MiiStatusReg;
        u16 PhySpecCtrlReg;
        u16 MiiAutoNegAdvertiseReg, Mii1000TCtrlReg;
        u16 i, Data;
        u16 AutoNegHwSetting;
        u16 AutoNegFCSetting;
        u8 RestartAutoNeg = 0;
        u8 ForceAutoNegRestart = 0;

        DEBUGFUNC("em_phy_setup")

            ASSERT(Adapter->MacType >= MAC_LIVENGOOD);

        if (Adapter->MacType > MAC_WAINWRIGHT) {
                DeviceControlReg |= (E1000_CTRL_ASDE | E1000_CTRL_SLU);
                E1000_WRITE_REG(Ctrl, DeviceControlReg);
        } else {
                DeviceControlReg |= (E1000_CTRL_FRCSPD |
                                     E1000_CTRL_FRCDPX | E1000_CTRL_SLU);
                E1000_WRITE_REG(Ctrl, DeviceControlReg);

                if (Adapter->MacType == MAC_LIVENGOOD)
                        em_phy_hardware_reset(Adapter);
        }

        Adapter->PhyAddress = em_auto_detect_gigabit_phy(Adapter);

        if (Adapter->PhyAddress > MAX_PHY_REG_ADDRESS) {

                DEBUGOUT
                    ("em_phy_setup failure, did not detect valid phy.\n");
                return (0);
        }

        DEBUGOUT1("Phy ID = %x \n", Adapter->PhyId);

        MiiCtrlReg = em_read_phy_register(Adapter,
                                          PHY_MII_CTRL_REG,
                                          Adapter->PhyAddress);

        DEBUGOUT1("MII Ctrl Reg contents = %x\n", MiiCtrlReg);

        if (!(MiiCtrlReg & MII_CR_AUTO_NEG_EN))
                ForceAutoNegRestart = 1;

        MiiCtrlReg &= ~(MII_CR_ISOLATE);

        em_write_phy_register(Adapter,
                              PHY_MII_CTRL_REG,
                              Adapter->PhyAddress, MiiCtrlReg);

        Data = em_read_phy_register(Adapter,
                                    PXN_PHY_SPEC_CTRL_REG,
                                    Adapter->PhyAddress);

        Data |= PXN_PSCR_ASSERT_CRS_ON_TX;

        DEBUGOUT1("Paxson PSCR: %x \n", Data);

        em_write_phy_register(Adapter,
                              PXN_PHY_SPEC_CTRL_REG,
                              Adapter->PhyAddress, Data);

        Data = em_read_phy_register(Adapter,
                                    PXN_EXT_PHY_SPEC_CTRL_REG,
                                    Adapter->PhyAddress);

        Data |= PXN_EPSCR_TX_CLK_25;

        em_write_phy_register(Adapter,
                              PXN_EXT_PHY_SPEC_CTRL_REG,
                              Adapter->PhyAddress, Data);

        MiiAutoNegAdvertiseReg = em_read_phy_register(Adapter,
                                                      PHY_AUTONEG_ADVERTISEMENT,
                                                      Adapter->PhyAddress);

        AutoNegHwSetting = (MiiAutoNegAdvertiseReg >> 5) & 0xF;

        Mii1000TCtrlReg = em_read_phy_register(Adapter,
                                               PHY_1000T_CTRL_REG,
                                               Adapter->PhyAddress);

        AutoNegHwSetting |= ((Mii1000TCtrlReg & 0x0300) >> 4);

        AutoNegFCSetting = ((MiiAutoNegAdvertiseReg & 0x0C00) >> 10);

        Adapter->AutoNegAdvertised &= AUTONEG_ADVERTISE_SPEED_DEFAULT;

        if (Adapter->AutoNegAdvertised == 0)
                Adapter->AutoNegAdvertised =
                    AUTONEG_ADVERTISE_SPEED_DEFAULT;

        if (!ForceAutoNegRestart && Adapter->AutoNeg &&
            (Adapter->AutoNegAdvertised == AutoNegHwSetting) &&
            (Adapter->FlowControl == AutoNegFCSetting)) {
                DEBUGOUT("No overrides - Reading MII Status Reg..\n");

                MiiStatusReg = em_read_phy_register(Adapter,
                                                    PHY_MII_STATUS_REG,
                                                    Adapter->PhyAddress);

                MiiStatusReg = em_read_phy_register(Adapter,
                                                    PHY_MII_STATUS_REG,
                                                    Adapter->PhyAddress);

                DEBUGOUT1("MII Status Reg contents = %x\n", MiiStatusReg);

                if (MiiStatusReg & MII_SR_LINK_STATUS) {
                        Data = em_read_phy_register(Adapter,
                                                    PXN_PHY_SPEC_STAT_REG,
                                                    Adapter->PhyAddress);
                        DEBUGOUT1
                            ("Paxson Phy Specific Status Reg contents = %x\n",
                             Data);

                        if (Adapter->MacType > MAC_WAINWRIGHT)
                                em_configure_collision_distance(Adapter);
                        else
                                em_configure_mac_to_phy_settings(Adapter,
                                                                 Data);

                        em_config_flow_control_after_link_up(Adapter);

                        return (1);
                }
        }

        PhySpecCtrlReg = em_read_phy_register(Adapter,
                                              PXN_PHY_SPEC_CTRL_REG,
                                              Adapter->PhyAddress);

        PhySpecCtrlReg &= ~PXN_PSCR_AUTO_X_MODE;

        switch (Adapter->MdiX) {
        case 1:
                PhySpecCtrlReg |= PXN_PSCR_MDI_MANUAL_MODE;
                break;
        case 2:
                PhySpecCtrlReg |= PXN_PSCR_MDIX_MANUAL_MODE;
                break;
        case 3:
                PhySpecCtrlReg |= PXN_PSCR_AUTO_X_1000T;
                break;
        case 0:
        default:
                PhySpecCtrlReg |= PXN_PSCR_AUTO_X_MODE;
                break;
        }

        em_write_phy_register(Adapter,
                              PXN_PHY_SPEC_CTRL_REG,
                              Adapter->PhyAddress, PhySpecCtrlReg);

        PhySpecCtrlReg = em_read_phy_register(Adapter,
                                              PXN_PHY_SPEC_CTRL_REG,
                                              Adapter->PhyAddress);

        PhySpecCtrlReg &= ~PXN_PSCR_POLARITY_REVERSAL;

        if (Adapter->DisablePolarityCorrection == 1)
                PhySpecCtrlReg |= PXN_PSCR_POLARITY_REVERSAL;

        em_write_phy_register(Adapter,
                              PXN_PHY_SPEC_CTRL_REG,
                              Adapter->PhyAddress, PhySpecCtrlReg);

        if (Adapter->AutoNeg) {
                DEBUGOUT
                    ("Livengood - Reconfiguring auto-neg advertisement params\n");
                RestartAutoNeg =
                    em_phy_setup_auto_neg_advertisement(Adapter);
        } else {
                DEBUGOUT("Livengood - Forcing speed and duplex\n");
                em_phy_force_speed_and_duplex(Adapter);
        }

        if (RestartAutoNeg) {
                DEBUGOUT("Restarting Auto-Neg\n");

                MiiCtrlReg = em_read_phy_register(Adapter,
                                                  PHY_MII_CTRL_REG,
                                                  Adapter->PhyAddress);

                MiiCtrlReg |=
                    (MII_CR_AUTO_NEG_EN | MII_CR_RESTART_AUTO_NEG);

                em_write_phy_register(Adapter,
                                      PHY_MII_CTRL_REG,
                                      Adapter->PhyAddress, MiiCtrlReg);

                if (Adapter->WaitAutoNegComplete)
                        em_wait_for_auto_neg(Adapter);

        }

        MiiStatusReg = em_read_phy_register(Adapter,
                                            PHY_MII_STATUS_REG,
                                            Adapter->PhyAddress);

        MiiStatusReg = em_read_phy_register(Adapter,
                                            PHY_MII_STATUS_REG,
                                            Adapter->PhyAddress);

        DEBUGOUT1
            ("Checking for link status - MII Status Reg contents = %x\n",
             MiiStatusReg);

        for (i = 0; i < 10; i++) {
                if (MiiStatusReg & MII_SR_LINK_STATUS) {
                        break;
                }
                DelayInMicroseconds(10);
                DEBUGOUT(". ");

                MiiStatusReg = em_read_phy_register(Adapter,
                                                    PHY_MII_STATUS_REG,
                                                    Adapter->PhyAddress);

                MiiStatusReg = em_read_phy_register(Adapter,
                                                    PHY_MII_STATUS_REG,
                                                    Adapter->PhyAddress);
        }

        if (MiiStatusReg & MII_SR_LINK_STATUS) {

                Data = em_read_phy_register(Adapter,
                                            PXN_PHY_SPEC_STAT_REG,
                                            Adapter->PhyAddress);

                DEBUGOUT1("Paxson Phy Specific Status Reg contents = %x\n",
                          Data);

                if (Adapter->MacType > MAC_WAINWRIGHT)
                        em_configure_collision_distance(Adapter);
                else
                        em_configure_mac_to_phy_settings(Adapter, Data);

                em_config_flow_control_after_link_up(Adapter);

                DEBUGOUT("Valid link established!!!\n");
        } else {
                DEBUGOUT("Unable to establish link!!!\n");
        }

        return (1);
}

u8 em_phy_setup_auto_neg_advertisement(struct adapter * Adapter)
 {
        u16 MiiAutoNegAdvertiseReg, Mii1000TCtrlReg;

        DEBUGFUNC("em_phy_setup_auto_neg_advertisement")

            MiiAutoNegAdvertiseReg = em_read_phy_register(Adapter,
                                                          PHY_AUTONEG_ADVERTISEMENT,
                                                          Adapter->
                                                          PhyAddress);

        Mii1000TCtrlReg = em_read_phy_register(Adapter,
                                               PHY_1000T_CTRL_REG,
                                               Adapter->PhyAddress);

        MiiAutoNegAdvertiseReg &= ~REG4_SPEED_MASK;
        Mii1000TCtrlReg &= ~REG9_SPEED_MASK;

        DEBUGOUT1("AutoNegAdvertised %x\n", Adapter->AutoNegAdvertised);

        if (Adapter->AutoNegAdvertised & ADVERTISE_10_HALF) {
                DEBUGOUT("Advertise 10mb Half duplex\n");
                MiiAutoNegAdvertiseReg |= NWAY_AR_10T_HD_CAPS;
        }

        if (Adapter->AutoNegAdvertised & ADVERTISE_10_FULL) {
                DEBUGOUT("Advertise 10mb Full duplex\n");
                MiiAutoNegAdvertiseReg |= NWAY_AR_10T_FD_CAPS;
        }

        if (Adapter->AutoNegAdvertised & ADVERTISE_100_HALF) {
                DEBUGOUT("Advertise 100mb Half duplex\n");
                MiiAutoNegAdvertiseReg |= NWAY_AR_100TX_HD_CAPS;
        }

        if (Adapter->AutoNegAdvertised & ADVERTISE_100_FULL) {
                DEBUGOUT("Advertise 100mb Full duplex\n");
                MiiAutoNegAdvertiseReg |= NWAY_AR_100TX_FD_CAPS;
        }

        if (Adapter->AutoNegAdvertised & ADVERTISE_1000_HALF) {
                DEBUGOUT
                    ("Advertise 1000mb Half duplex requested, request denied!\n");
        }

        if (Adapter->AutoNegAdvertised & ADVERTISE_1000_FULL) {
                DEBUGOUT("Advertise 1000mb Full duplex\n");
                Mii1000TCtrlReg |= CR_1000T_FD_CAPS;
        }

        switch (Adapter->FlowControl) {
        case FLOW_CONTROL_NONE:

                MiiAutoNegAdvertiseReg &=
                    ~(NWAY_AR_ASM_DIR | NWAY_AR_PAUSE);

                break;

        case FLOW_CONTROL_RECEIVE_PAUSE:

                MiiAutoNegAdvertiseReg |=
                    (NWAY_AR_ASM_DIR | NWAY_AR_PAUSE);

                break;

        case FLOW_CONTROL_TRANSMIT_PAUSE:

                MiiAutoNegAdvertiseReg |= NWAY_AR_ASM_DIR;
                MiiAutoNegAdvertiseReg &= ~NWAY_AR_PAUSE;

                break;

        case FLOW_CONTROL_FULL:

                MiiAutoNegAdvertiseReg |=
                    (NWAY_AR_ASM_DIR | NWAY_AR_PAUSE);

                break;

        default:

                DEBUGOUT("Flow control param set incorrectly\n");
                ASSERT(0);
                break;
        }

        em_write_phy_register(Adapter,
                              PHY_AUTONEG_ADVERTISEMENT,
                              Adapter->PhyAddress, MiiAutoNegAdvertiseReg);

        DEBUGOUT1("Auto-Neg Advertising %x\n", MiiAutoNegAdvertiseReg);

        em_write_phy_register(Adapter,
                              PHY_1000T_CTRL_REG,
                              Adapter->PhyAddress, Mii1000TCtrlReg);
        return (1);
}

static void em_phy_force_speed_and_duplex(struct adapter *Adapter)
 {
        u16 MiiCtrlReg;
        u16 MiiStatusReg;
        u16 PhyData;
        u16 i;
        u32 TctlReg;
        u32 DeviceCtrlReg;
        u32 Shift32;

        DEBUGFUNC("em_phy_force_speed_and_duplex")

            Adapter->FlowControl = FLOW_CONTROL_NONE;

        DEBUGOUT1("Adapter->FlowControl = %d\n", Adapter->FlowControl);

        DeviceCtrlReg = E1000_READ_REG(Ctrl);

        DeviceCtrlReg |= (E1000_CTRL_FRCSPD | E1000_CTRL_FRCDPX);
        DeviceCtrlReg &= ~(DEVICE_SPEED_MASK);

        DeviceCtrlReg &= ~E1000_CTRL_ASDE;

        MiiCtrlReg = em_read_phy_register(Adapter,
                                          PHY_MII_CTRL_REG,
                                          Adapter->PhyAddress);

        MiiCtrlReg &= ~MII_CR_AUTO_NEG_EN;

        if (Adapter->ForcedSpeedDuplex == FULL_100 ||
            Adapter->ForcedSpeedDuplex == FULL_10) {

                DeviceCtrlReg |= E1000_CTRL_FD;
                MiiCtrlReg |= MII_CR_FULL_DUPLEX;

                DEBUGOUT("Full Duplex\n");
        } else {

                DeviceCtrlReg &= ~E1000_CTRL_FD;
                MiiCtrlReg &= ~MII_CR_FULL_DUPLEX;

                DEBUGOUT("Half Duplex\n");
        }

        if (Adapter->ForcedSpeedDuplex == FULL_100 ||
            Adapter->ForcedSpeedDuplex == HALF_100) {

                DeviceCtrlReg |= E1000_CTRL_SPD_100;
                MiiCtrlReg |= MII_CR_SPEED_100;
                MiiCtrlReg &= ~(MII_CR_SPEED_1000 | MII_CR_SPEED_10);

                DEBUGOUT("Forcing 100mb ");
        } else {

                DeviceCtrlReg &=
                    ~(E1000_CTRL_SPD_1000 | E1000_CTRL_SPD_100);
                MiiCtrlReg |= MII_CR_SPEED_10;
                MiiCtrlReg &= ~(MII_CR_SPEED_1000 | MII_CR_SPEED_100);

                DEBUGOUT("Forcing 10mb ");
        }

        TctlReg = E1000_READ_REG(Tctl);
        DEBUGOUT1("TctlReg = %x\n", TctlReg);

        if (!(MiiCtrlReg & MII_CR_FULL_DUPLEX)) {

                TctlReg &= ~E1000_TCTL_COLD;
                Shift32 = E1000_HDX_COLLISION_DISTANCE;
                Shift32 <<= E1000_COLD_SHIFT;
                TctlReg |= Shift32;
        } else {

                TctlReg &= ~E1000_TCTL_COLD;
                Shift32 = E1000_FDX_COLLISION_DISTANCE;
                Shift32 <<= E1000_COLD_SHIFT;
                TctlReg |= Shift32;
        }

        E1000_WRITE_REG(Tctl, TctlReg);

        E1000_WRITE_REG(Ctrl, DeviceCtrlReg);

        PhyData = em_read_phy_register(Adapter,
                                       PXN_PHY_SPEC_CTRL_REG,
                                       Adapter->PhyAddress);

        PhyData &= ~PXN_PSCR_AUTO_X_MODE;

        em_write_phy_register(Adapter,
                              PXN_PHY_SPEC_CTRL_REG,
                              Adapter->PhyAddress, PhyData);

        DEBUGOUT1("Paxson PSCR: %x \n", PhyData);

        MiiCtrlReg |= MII_CR_RESET;

        em_write_phy_register(Adapter,
                              PHY_MII_CTRL_REG,
                              Adapter->PhyAddress, MiiCtrlReg);

        if (Adapter->WaitAutoNegComplete) {

                DEBUGOUT("Waiting for forced speed/duplex link.\n");
                MiiStatusReg = 0;

#define PHY_WAIT_FOR_FORCED_TIME    20

                for (i = 20; i > 0; i--) {

                        MiiStatusReg = em_read_phy_register(Adapter,
                                                            PHY_MII_STATUS_REG,
                                                            Adapter->
                                                            PhyAddress);

                        MiiStatusReg = em_read_phy_register(Adapter,
                                                            PHY_MII_STATUS_REG,
                                                            Adapter->
                                                            PhyAddress);

                        if (MiiStatusReg & MII_SR_LINK_STATUS) {
                                break;
                        }
                        DelayInMilliseconds(100);
                }

                if (i == 0) {

                        em_pxn_phy_reset_dsp(Adapter);
                }

                for (i = 20; i > 0; i--) {
                        if (MiiStatusReg & MII_SR_LINK_STATUS) {
                                break;
                        }

                        DelayInMilliseconds(100);

                        MiiStatusReg = em_read_phy_register(Adapter,
                                                            PHY_MII_STATUS_REG,
                                                            Adapter->
                                                            PhyAddress);

                        MiiStatusReg = em_read_phy_register(Adapter,
                                                            PHY_MII_STATUS_REG,
                                                            Adapter->
                                                            PhyAddress);

                }
        }

        PhyData = em_read_phy_register(Adapter,
                                       PXN_EXT_PHY_SPEC_CTRL_REG,
                                       Adapter->PhyAddress);

        PhyData |= PXN_EPSCR_TX_CLK_25;

        em_write_phy_register(Adapter,
                              PXN_EXT_PHY_SPEC_CTRL_REG,
                              Adapter->PhyAddress, PhyData);

        PhyData = em_read_phy_register(Adapter,
                                       PXN_PHY_SPEC_CTRL_REG,
                                       Adapter->PhyAddress);

        PhyData |= PXN_PSCR_ASSERT_CRS_ON_TX;

        em_write_phy_register(Adapter,
                              PXN_PHY_SPEC_CTRL_REG,
                              Adapter->PhyAddress, PhyData);
        DEBUGOUT1("After force, Paxson Phy Specific Ctrl Reg = %4x\r\n",
                  PhyData);

        return;
}

void em_configure_mac_to_phy_settings(struct adapter *Adapter,
                                      u16 MiiRegisterData)
 {
        u32 DeviceCtrlReg, TctlReg;
        u32 Shift32;

        DEBUGFUNC("em_configure_mac_to_phy_settings")

            TctlReg = E1000_READ_REG(Tctl);
        DEBUGOUT1("TctlReg = %x\n", TctlReg);

        DeviceCtrlReg = E1000_READ_REG(Ctrl);

        DeviceCtrlReg |= (E1000_CTRL_FRCSPD | E1000_CTRL_FRCDPX);
        DeviceCtrlReg &= ~(DEVICE_SPEED_MASK);

        DEBUGOUT1("MII Register Data = %x\r\n", MiiRegisterData);

        DeviceCtrlReg &= ~E1000_CTRL_ILOS;

        if (MiiRegisterData & PXN_PSSR_DPLX) {
                DeviceCtrlReg |= E1000_CTRL_FD;

                TctlReg &= ~E1000_TCTL_COLD;
                Shift32 = E1000_FDX_COLLISION_DISTANCE;
                Shift32 <<= E1000_COLD_SHIFT;
                TctlReg |= Shift32;
        } else {
                DeviceCtrlReg &= ~E1000_CTRL_FD;

                if ((MiiRegisterData & PXN_PSSR_SPEED) == PXN_PSSR_1000MBS) {
                        TctlReg &= ~E1000_TCTL_COLD;
                        Shift32 = E1000_GB_HDX_COLLISION_DISTANCE;
                        Shift32 <<= E1000_COLD_SHIFT;
                        TctlReg |= Shift32;

                        TctlReg |= E1000_TCTL_PBE;

                } else {
                        TctlReg &= ~E1000_TCTL_COLD;
                        Shift32 = E1000_HDX_COLLISION_DISTANCE;
                        Shift32 <<= E1000_COLD_SHIFT;
                        TctlReg |= Shift32;
                }
        }

        if ((MiiRegisterData & PXN_PSSR_SPEED) == PXN_PSSR_1000MBS)
                DeviceCtrlReg |= E1000_CTRL_SPD_1000;
        else if ((MiiRegisterData & PXN_PSSR_SPEED) == PXN_PSSR_100MBS)
                DeviceCtrlReg |= E1000_CTRL_SPD_100;
        else
                DeviceCtrlReg &=
                    ~(E1000_CTRL_SPD_1000 | E1000_CTRL_SPD_100);

        E1000_WRITE_REG(Tctl, TctlReg);

        E1000_WRITE_REG(Ctrl, DeviceCtrlReg);

        return;
}

void em_configure_collision_distance(struct adapter *Adapter)
 {
        u32 TctlReg;
        u16 Speed;
        u16 Duplex;
        u32 Shift32;

        DEBUGFUNC("em_configure_collision_distance")

            em_get_speed_and_duplex(Adapter, &Speed, &Duplex);

        TctlReg = E1000_READ_REG(Tctl);
        DEBUGOUT1("TctlReg = %x\n", TctlReg);

        TctlReg &= ~E1000_TCTL_COLD;

        if (Duplex == FULL_DUPLEX) {

                Shift32 = E1000_FDX_COLLISION_DISTANCE;
                Shift32 <<= E1000_COLD_SHIFT;
                TctlReg |= Shift32;
        } else {

                if (Speed == SPEED_1000) {
                        Shift32 = E1000_GB_HDX_COLLISION_DISTANCE;
                        Shift32 <<= E1000_COLD_SHIFT;
                        TctlReg |= Shift32;

                        TctlReg |= E1000_TCTL_PBE;

                } else {
                        Shift32 = E1000_HDX_COLLISION_DISTANCE;
                        Shift32 <<= E1000_COLD_SHIFT;
                        TctlReg |= Shift32;
                }
        }

        E1000_WRITE_REG(Tctl, TctlReg);

        return;
}

void em_display_mii_contents(struct adapter *Adapter, u8 PhyAddress)
 {
        u16 Data, PhyIDHi, PhyIDLo;
        u32 PhyID;

        DEBUGFUNC("em_display_mii_contents")

            DEBUGOUT1("Adapter Base Address = %x\n",
                      Adapter->HardwareVirtualAddress);

        Data = em_read_phy_register(Adapter, PHY_MII_CTRL_REG, PhyAddress);

        DEBUGOUT1("MII Ctrl Reg contents = %x\n", Data);

        Data = em_read_phy_register(Adapter,
                                    PHY_MII_STATUS_REG, PhyAddress);

        Data = em_read_phy_register(Adapter,
                                    PHY_MII_STATUS_REG, PhyAddress);

        DEBUGOUT1("MII Status Reg contents = %x\n", Data);

        PhyIDHi = em_read_phy_register(Adapter,
                                       PHY_PHY_ID_REG1, PhyAddress);

        DelayInMicroseconds(2);

        PhyIDLo = em_read_phy_register(Adapter,
                                       PHY_PHY_ID_REG2, PhyAddress);

        PhyID = (PhyIDLo | (PhyIDHi << 16)) & PHY_REVISION_MASK;

        DEBUGOUT1("Phy ID = %x \n", PhyID);

        Data = em_read_phy_register(Adapter,
                                    PHY_AUTONEG_ADVERTISEMENT, PhyAddress);

        DEBUGOUT1("Reg 4 contents = %x\n", Data);

        Data = em_read_phy_register(Adapter,
                                    PHY_AUTONEG_LP_BPA, PhyAddress);

        DEBUGOUT1("Reg 5 contents = %x\n", Data);

        Data = em_read_phy_register(Adapter,
                                    PHY_AUTONEG_EXPANSION_REG, PhyAddress);

        DEBUGOUT1("Reg 6 contents = %x\n", Data);

        Data = em_read_phy_register(Adapter,
                                    PHY_AUTONEG_NEXT_PAGE_TX, PhyAddress);

        DEBUGOUT1("Reg 7 contents = %x\n", Data);

        Data = em_read_phy_register(Adapter,
                                    PHY_AUTONEG_LP_RX_NEXT_PAGE,
                                    PhyAddress);

        DEBUGOUT1("Reg 8 contents = %x\n", Data);

        Data = em_read_phy_register(Adapter,
                                    PHY_1000T_CTRL_REG, PhyAddress);

        DEBUGOUT1("Reg 9 contents = %x\n", Data);

        Data = em_read_phy_register(Adapter,
                                    PHY_1000T_STATUS_REG, PhyAddress);

        DEBUGOUT1("Reg A contents = %x\n", Data);

        Data = em_read_phy_register(Adapter,
                                    PHY_IEEE_EXT_STATUS_REG, PhyAddress);

        DEBUGOUT1("Reg F contents = %x\n", Data);

        Data = em_read_phy_register(Adapter,
                                    PXN_PHY_SPEC_CTRL_REG, PhyAddress);

        DEBUGOUT1("Paxson Specific Control Reg (0x10) = %x\n", Data);

        Data = em_read_phy_register(Adapter,
                                    PXN_PHY_SPEC_STAT_REG, PhyAddress);

        DEBUGOUT1("Paxson Specific Status Reg (0x11) = %x\n", Data);

        Data = em_read_phy_register(Adapter,
                                    PXN_INT_ENABLE_REG, PhyAddress);

        DEBUGOUT1("Paxson Interrupt Enable Reg (0x12) = %x\n", Data);

        Data = em_read_phy_register(Adapter,
                                    PXN_INT_STATUS_REG, PhyAddress);

        DEBUGOUT1("Paxson Interrupt Status Reg (0x13) = %x\n", Data);

        Data = em_read_phy_register(Adapter,
                                    PXN_EXT_PHY_SPEC_CTRL_REG, PhyAddress);

        DEBUGOUT1("Paxson Ext. Phy Specific Control (0x14) = %x\n", Data);

        Data = em_read_phy_register(Adapter,
                                    PXN_RX_ERROR_COUNTER, PhyAddress);

        DEBUGOUT1("Paxson Receive Error Counter (0x15) = %x\n", Data);

        Data = em_read_phy_register(Adapter, PXN_LED_CTRL_REG, PhyAddress);

        DEBUGOUT1("Paxson LED control reg (0x18) = %x\n", Data);
}

u32 em_auto_detect_gigabit_phy(struct adapter *Adapter)
{
        u32 PhyAddress = 1;
        u32 PhyIDHi;
        u16 PhyIDLo;
        u8 GotOne = 0;

        DEBUGFUNC("em_auto_detect_gigabit_phy")

            while ((!GotOne) && (PhyAddress <= MAX_PHY_REG_ADDRESS)) {

                PhyIDHi = em_read_phy_register(Adapter,
                                               PHY_PHY_ID_REG1,
                                               PhyAddress);

                DelayInMicroseconds(2);

                PhyIDLo = em_read_phy_register(Adapter,
                                               PHY_PHY_ID_REG2,
                                               PhyAddress);

                Adapter->PhyId =
                    (PhyIDLo | (PhyIDHi << 16)) & PHY_REVISION_MASK;

                if (Adapter->PhyId == PAXSON_PHY_88E1000 ||
                    Adapter->PhyId == PAXSON_PHY_88E1000S ||
                    Adapter->PhyId == PAXSON_PHY_INTEGRATED) {
                        DEBUGOUT2("PhyId 0x%x detected at address 0x%x\n",
                                  Adapter->PhyId, PhyAddress);

                        GotOne = 1;
                } else {
                        PhyAddress++;
                }

        }

        if (PhyAddress > MAX_PHY_REG_ADDRESS) {
                DEBUGOUT("Could not auto-detect Phy!\n");
        }

        return (PhyAddress);
}

void em_pxn_phy_reset_dsp(struct adapter *Adapter)
{
        em_write_phy_register(Adapter, 29, Adapter->PhyAddress, 0x1d);
        em_write_phy_register(Adapter, 30, Adapter->PhyAddress, 0xc1);
        em_write_phy_register(Adapter, 30, Adapter->PhyAddress, 0x00);
}

u8 em_wait_for_auto_neg(struct adapter *Adapter)
{
        u8 AutoNegComplete = 0;
        u16 i;
        u16 MiiStatusReg;

        DEBUGFUNC("em_wait_for_auto_neg");

        DEBUGOUT("Waiting for Auto-Neg to complete.\n");
        MiiStatusReg = 0;

        for (i = PHY_AUTO_NEG_TIME; i > 0; i--) {

                MiiStatusReg = em_read_phy_register(Adapter,
                                                    PHY_MII_STATUS_REG,
                                                    Adapter->PhyAddress);

                MiiStatusReg = em_read_phy_register(Adapter,
                                                    PHY_MII_STATUS_REG,
                                                    Adapter->PhyAddress);

                if (MiiStatusReg & MII_SR_AUTONEG_COMPLETE) {
                        AutoNegComplete = 1;
                        break;
                }

                DelayInMilliseconds(100);
        }

        return (AutoNegComplete);
}

u8 em_phy_get_status_info(struct adapter * Adapter,
                          phy_status_info_struct * PhyStatusInfo)
{
        u16 PhyMIIStatReg;
        u16 PhySpecCtrlReg;
        u16 PhySpecStatReg;
        u16 PhyExtSpecCtrlReg;
        u16 Phy1000BTStatReg;

        PhyStatusInfo->CableLength = PXN_PSSR_CABLE_LENGTH_UNDEFINED;
        PhyStatusInfo->Extended10BTDistance =
            PXN_PSCR_10BT_EXT_DIST_ENABLE_UNDEFINED;
        PhyStatusInfo->CablePolarity = PXN_PSSR_REV_POLARITY_UNDEFINED;
        PhyStatusInfo->PolarityCorrection =
            PXN_PSCR_POLARITY_REVERSAL_UNDEFINED;
        PhyStatusInfo->LinkReset = PXN_EPSCR_DOWN_NO_IDLE_UNDEFINED;
        PhyStatusInfo->MDIXMode = PXN_PSCR_AUTO_X_MODE_UNDEFINED;
        PhyStatusInfo->LocalRx = SR_1000T_RX_STATUS_UNDEFINED;
        PhyStatusInfo->RemoteRx = SR_1000T_RX_STATUS_UNDEFINED;

        if (Adapter == NULL || Adapter->MediaType != MEDIA_TYPE_COPPER)
                return 0;

        PhyMIIStatReg = em_read_phy_register(Adapter,
                                             PHY_MII_STATUS_REG,
                                             Adapter->PhyAddress);
        PhyMIIStatReg = em_read_phy_register(Adapter,
                                             PHY_MII_STATUS_REG,
                                             Adapter->PhyAddress);
        if ((PhyMIIStatReg & MII_SR_LINK_STATUS) != MII_SR_LINK_STATUS)
                return 0;

        PhySpecCtrlReg = em_read_phy_register(Adapter,
                                              PXN_PHY_SPEC_CTRL_REG,
                                              Adapter->PhyAddress);
        PhySpecStatReg = em_read_phy_register(Adapter,
                                              PXN_PHY_SPEC_STAT_REG,
                                              Adapter->PhyAddress);
        PhyExtSpecCtrlReg = em_read_phy_register(Adapter,
                                                 PXN_EXT_PHY_SPEC_CTRL_REG,
                                                 Adapter->PhyAddress);
        Phy1000BTStatReg = em_read_phy_register(Adapter,
                                                PHY_1000T_STATUS_REG,
                                                Adapter->PhyAddress);

        PhyStatusInfo->CableLength = (PXN_PSSR_CABLE_LENGTH_ENUM)
            ((PhySpecStatReg & PXN_PSSR_CABLE_LENGTH) >>
             PXN_PSSR_CABLE_LENGTH_SHIFT);

        PhyStatusInfo->Extended10BTDistance =
            (PXN_PSCR_10BT_EXT_DIST_ENABLE_ENUM) (PhySpecCtrlReg &
                                                  PXN_PSCR_10BT_EXT_DIST_ENABLE)
            >> PXN_PSCR_10BT_EXT_DIST_ENABLE_SHIFT;

        PhyStatusInfo->CablePolarity = (PXN_PSSR_REV_POLARITY_ENUM)
            (PhySpecStatReg & PXN_PSSR_REV_POLARITY) >>
            PXN_PSSR_REV_POLARITY_SHIFT;

        PhyStatusInfo->PolarityCorrection =
            (PXN_PSCR_POLARITY_REVERSAL_ENUM) (PhySpecCtrlReg &
                                               PXN_PSCR_POLARITY_REVERSAL)
            >> PXN_PSCR_POLARITY_REVERSAL_SHIFT;

        PhyStatusInfo->LinkReset = (PXN_EPSCR_DOWN_NO_IDLE_ENUM)
            (PhyExtSpecCtrlReg & PXN_EPSCR_DOWN_NO_IDLE) >>
            PXN_EPSCR_DOWN_NO_IDLE_SHIFT;

        PhyStatusInfo->MDIXMode = (PXN_PSCR_AUTO_X_MODE_ENUM)
            (PhySpecCtrlReg & PXN_PSCR_AUTO_X_MODE) >>
            PXN_PSCR_AUTO_X_MODE_SHIFT;

        PhyStatusInfo->LocalRx = (SR_1000T_RX_STATUS_ENUM)
            (Phy1000BTStatReg & SR_1000T_LOCAL_RX_STATUS) >>
            SR_1000T_LOCAL_RX_STATUS_SHIFT;

        PhyStatusInfo->RemoteRx = (SR_1000T_RX_STATUS_ENUM)
            (Phy1000BTStatReg & SR_1000T_REMOTE_RX_STATUS) >>
            SR_1000T_REMOTE_RX_STATUS_SHIFT;

        return 1;
}
