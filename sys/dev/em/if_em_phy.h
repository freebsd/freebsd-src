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

#ifndef _EM_PHY_H_
#define _EM_PHY_H_

/*
* Workfile: phy.h 
* Date: 9/25/01 2:40p 
* Revision: 9 
*/

#define _PHY_

#include <dev/em/if_em_osdep.h>

typedef enum {
        PXN_PSSR_CABLE_LENGTH_50 = 0,
        PXN_PSSR_CABLE_LENGTH_50_80,
        PXN_PSSR_CABLE_LENGTH_80_110,
        PXN_PSSR_CABLE_LENGTH_110_140,
        PXN_PSSR_CABLE_LENGTH_140,
        PXN_PSSR_CABLE_LENGTH_UNDEFINED = 0xFF
} PXN_PSSR_CABLE_LENGTH_ENUM;

typedef enum {
        PXN_PSCR_10BT_EXT_DIST_ENABLE_NORMAL = 0,
        PXN_PSCR_10BT_EXT_DIST_ENABLE_LOWER,
        PXN_PSCR_10BT_EXT_DIST_ENABLE_UNDEFINED = 0xFF
} PXN_PSCR_10BT_EXT_DIST_ENABLE_ENUM;

typedef enum {
        PXN_PSSR_REV_POLARITY_NORMAL = 0,
        PXN_PSSR_REV_POLARITY_REVERSED,
        PXN_PSSR_REV_POLARITY_UNDEFINED = 0xFF
} PXN_PSSR_REV_POLARITY_ENUM;

typedef enum {
        PXN_PSCR_POLARITY_REVERSAL_ENABLED = 0,
        PXN_PSCR_POLARITY_REVERSAL_DISABLED,
        PXN_PSCR_POLARITY_REVERSAL_UNDEFINED = 0xFF
} PXN_PSCR_POLARITY_REVERSAL_ENUM;

typedef enum {
        PXN_EPSCR_DOWN_NO_IDLE_NO_DETECT = 0,
        PXN_EPSCR_DOWN_NO_IDLE_DETECT,
        PXN_EPSCR_DOWN_NO_IDLE_UNDEFINED = 0xFF
} PXN_EPSCR_DOWN_NO_IDLE_ENUM;

typedef enum {
        PXN_PSCR_AUTO_X_MODE_MANUAL_MDI = 0,
        PXN_PSCR_AUTO_X_MODE_MANUAL_MDIX,
        PXN_PSCR_AUTO_X_MODE_AUTO_1,
        PXN_PSCR_AUTO_X_MODE_AUTO_2,
        PXN_PSCR_AUTO_X_MODE_UNDEFINED = 0xFF
} PXN_PSCR_AUTO_X_MODE_ENUM;

typedef enum {
        SR_1000T_RX_STATUS_NOT_OK = 0,
        SR_1000T_RX_STATUS_OK,
        SR_1000T_RX_STATUS_UNDEFINED = 0xFF
} SR_1000T_RX_STATUS_ENUM;

typedef struct {
        PXN_PSSR_CABLE_LENGTH_ENUM CableLength;
        PXN_PSCR_10BT_EXT_DIST_ENABLE_ENUM Extended10BTDistance;
        PXN_PSSR_REV_POLARITY_ENUM CablePolarity;
        PXN_PSCR_POLARITY_REVERSAL_ENUM PolarityCorrection;
        PXN_EPSCR_DOWN_NO_IDLE_ENUM LinkReset;
        PXN_PSCR_AUTO_X_MODE_ENUM MDIXMode;
        SR_1000T_RX_STATUS_ENUM LocalRx;
        SR_1000T_RX_STATUS_ENUM RemoteRx;
} phy_status_info_struct;

u16 em_read_phy_register(struct adapter *Adapter,

                         u32 RegAddress, u32 PhyAddress);
void em_write_phy_register(struct adapter *Adapter,
                           u32 RegAddress, u32 PhyAddress, u16 Data);
void em_phy_hardware_reset(struct adapter *Adapter);
u8 em_phy_reset(struct adapter *Adapter);
u8 em_phy_setup(struct adapter *Adapter, u32 DeviceControlReg);
void em_configure_mac_to_phy_settings(struct adapter *Adapter,

                                      u16 MiiRegisterData);
void em_configure_collision_distance(struct adapter *Adapter);
void em_display_mii_contents(struct adapter *Adapter, u8 PhyAddress);
u32 em_auto_detect_gigabit_phy(struct adapter *Adapter);
void em_pxn_phy_reset_dsp(struct adapter *Adapter);
void PxnIntegratedPhyLoopback(struct adapter *Adapter, u16 Speed);
void PxnPhyEnableReceiver(struct adapter *Adapter);
void PxnPhyDisableReceiver(struct adapter *Adapter);
u8 em_wait_for_auto_neg(struct adapter *Adapter);
u8 em_phy_get_status_info(struct adapter *Adapter,

                          phy_status_info_struct * PhyStatusInfo);

#define E1000_CTRL_PHY_RESET_DIR  E1000_CTRL_SWDPIO0
#define E1000_CTRL_PHY_RESET      E1000_CTRL_SWDPIN0
#define E1000_CTRL_MDIO_DIR       E1000_CTRL_SWDPIO2
#define E1000_CTRL_MDIO           E1000_CTRL_SWDPIN2
#define E1000_CTRL_MDC_DIR        E1000_CTRL_SWDPIO3
#define E1000_CTRL_MDC            E1000_CTRL_SWDPIN3
#define E1000_CTRL_PHY_RESET_DIR4 E1000_EXCTRL_SWDPIO4
#define E1000_CTRL_PHY_RESET4     E1000_EXCTRL_SWDPIN4

#define PHY_MII_CTRL_REG             0x00
#define PHY_MII_STATUS_REG           0x01
#define PHY_PHY_ID_REG1              0x02
#define PHY_PHY_ID_REG2              0x03
#define PHY_AUTONEG_ADVERTISEMENT    0x04
#define PHY_AUTONEG_LP_BPA           0x05
#define PHY_AUTONEG_EXPANSION_REG    0x06
#define PHY_AUTONEG_NEXT_PAGE_TX     0x07
#define PHY_AUTONEG_LP_RX_NEXT_PAGE  0x08
#define PHY_1000T_CTRL_REG           0x09
#define PHY_1000T_STATUS_REG         0x0A
#define PHY_IEEE_EXT_STATUS_REG      0x0F

#define PXN_PHY_SPEC_CTRL_REG        0x10
#define PXN_PHY_SPEC_STAT_REG        0x11
#define PXN_INT_ENABLE_REG           0x12
#define PXN_INT_STATUS_REG           0x13
#define PXN_EXT_PHY_SPEC_CTRL_REG    0x14
#define PXN_RX_ERROR_COUNTER         0x15
#define PXN_LED_CTRL_REG             0x18

#define MAX_PHY_REG_ADDRESS          0x1F

#define MII_CR_SPEED_SELECT_MSB   0x0040
#define MII_CR_COLL_TEST_ENABLE   0x0080
#define MII_CR_FULL_DUPLEX        0x0100
#define MII_CR_RESTART_AUTO_NEG   0x0200
#define MII_CR_ISOLATE            0x0400
#define MII_CR_POWER_DOWN         0x0800
#define MII_CR_AUTO_NEG_EN        0x1000
#define MII_CR_SPEED_SELECT_LSB   0x2000
#define MII_CR_LOOPBACK           0x4000
#define MII_CR_RESET              0x8000

#define MII_SR_EXTENDED_CAPS      0x0001
#define MII_SR_JABBER_DETECT      0x0002
#define MII_SR_LINK_STATUS        0x0004
#define MII_SR_AUTONEG_CAPS       0x0008
#define MII_SR_REMOTE_FAULT       0x0010
#define MII_SR_AUTONEG_COMPLETE   0x0020
#define MII_SR_PREAMBLE_SUPPRESS  0x0040
#define MII_SR_EXTENDED_STATUS    0x0100
#define MII_SR_100T2_HD_CAPS      0x0200
#define MII_SR_100T2_FD_CAPS      0x0400
#define MII_SR_10T_HD_CAPS        0x0800
#define MII_SR_10T_FD_CAPS        0x1000
#define MII_SR_100X_HD_CAPS       0x2000
#define MII_SR_100X_FD_CAPS       0x4000
#define MII_SR_100T4_CAPS         0x8000

#define NWAY_AR_SELECTOR_FIELD    0x0001
#define NWAY_AR_10T_HD_CAPS       0x0020
#define NWAY_AR_10T_FD_CAPS       0x0040
#define NWAY_AR_100TX_HD_CAPS     0x0080
#define NWAY_AR_100TX_FD_CAPS     0x0100
#define NWAY_AR_100T4_CAPS        0x0200
#define NWAY_AR_PAUSE             0x0400
#define NWAY_AR_ASM_DIR           0x0800
#define NWAY_AR_REMOTE_FAULT      0x2000
#define NWAY_AR_NEXT_PAGE         0x8000

#define NWAY_LPAR_SELECTOR_FIELD  0x0000
#define NWAY_LPAR_10T_HD_CAPS     0x0020
#define NWAY_LPAR_10T_FD_CAPS     0x0040
#define NWAY_LPAR_100TX_HD_CAPS   0x0080
#define NWAY_LPAR_100TX_FD_CAPS   0x0100
#define NWAY_LPAR_100T4_CAPS      0x0200
#define NWAY_LPAR_PAUSE           0x0400
#define NWAY_LPAR_ASM_DIR         0x0800
#define NWAY_LPAR_REMOTE_FAULT    0x2000
#define NWAY_LPAR_ACKNOWLEDGE     0x4000
#define NWAY_LPAR_NEXT_PAGE       0x8000

#define NWAY_ER_LP_NWAY_CAPS      0x0001
#define NWAY_ER_PAGE_RXD          0x0002
#define NWAY_ER_NEXT_PAGE_CAPS    0x0004
#define NWAY_ER_LP_NEXT_PAGE_CAPS 0x0008
#define NWAY_ER_PAR_DETECT_FAULT  0x0100

#define NPTX_MSG_CODE_FIELD       0x0001
#define NPTX_TOGGLE               0x0800

#define NPTX_ACKNOWLDGE2          0x1000

#define NPTX_MSG_PAGE             0x2000
#define NPTX_NEXT_PAGE            0x8000

#define LP_RNPR_MSG_CODE_FIELD    0x0001
#define LP_RNPR_TOGGLE            0x0800

#define LP_RNPR_ACKNOWLDGE2       0x1000

#define LP_RNPR_MSG_PAGE          0x2000
#define LP_RNPR_ACKNOWLDGE        0x4000
#define LP_RNPR_NEXT_PAGE         0x8000

#define CR_1000T_ASYM_PAUSE       0x0080
#define CR_1000T_HD_CAPS          0x0100

#define CR_1000T_FD_CAPS          0x0200

#define CR_1000T_REPEATER_DTE     0x0400

#define CR_1000T_MS_VALUE         0x0800

#define CR_1000T_MS_ENABLE        0x1000

#define CR_1000T_TEST_MODE_NORMAL 0x0000
#define CR_1000T_TEST_MODE_1      0x2000
#define CR_1000T_TEST_MODE_2      0x4000
#define CR_1000T_TEST_MODE_3      0x6000
#define CR_1000T_TEST_MODE_4      0x8000

#define SR_1000T_IDLE_ERROR_CNT   0x00FF
#define SR_1000T_ASYM_PAUSE_DIR   0x0100
#define SR_1000T_LP_HD_CAPS       0x0400

#define SR_1000T_LP_FD_CAPS       0x0800

#define SR_1000T_REMOTE_RX_STATUS 0x1000
#define SR_1000T_LOCAL_RX_STATUS  0x2000
#define SR_1000T_MS_CONFIG_RES    0x4000
#define SR_1000T_MS_CONFIG_FAULT  0x8000

#define SR_1000T_REMOTE_RX_STATUS_SHIFT     12
#define SR_1000T_LOCAL_RX_STATUS_SHIFT      13

#define IEEE_ESR_1000T_HD_CAPS    0x1000

#define IEEE_ESR_1000T_FD_CAPS    0x2000

#define IEEE_ESR_1000X_HD_CAPS    0x4000

#define IEEE_ESR_1000X_FD_CAPS    0x8000

#define PHY_TX_POLARITY_MASK        0x0100
#define PHY_TX_NORMAL_POLARITY      0

#define AUTO_POLARITY_DISABLE       0x0010

#define PXN_PSCR_JABBER_DISABLE         0x0001
#define PXN_PSCR_POLARITY_REVERSAL      0x0002
#define PXN_PSCR_SQE_TEST           0x0004
#define PXN_PSCR_INT_FIFO_DISABLE        0x0008

#define PXN_PSCR_CLK125_DISABLE         0x0010
#define PXN_PSCR_MDI_MANUAL_MODE        0x0000

#define PXN_PSCR_MDIX_MANUAL_MODE       0x0020
#define PXN_PSCR_AUTO_X_1000T           0x0040
#define PXN_PSCR_AUTO_X_MODE            0x0060
#define PXN_PSCR_10BT_EXT_DIST_ENABLE   0x0080
#define PXN_PSCR_MII_5BIT_ENABLE        0x0100
#define PXN_PSCR_SCRAMBLER_DISABLE      0x0200
#define PXN_PSCR_FORCE_LINK_GOOD        0x0400
#define PXN_PSCR_ASSERT_CRS_ON_TX       0x0800
#define PXN_PSCR_RX_FIFO_DEPTH_6        0x0000
#define PXN_PSCR_RX_FIFO_DEPTH_8        0x1000
#define PXN_PSCR_RX_FIFO_DEPTH_10       0x2000
#define PXN_PSCR_RX_FIFO_DEPTH_12       0x3000

#define PXN_PSCR_TXFR_FIFO_DEPTH_6      0x0000
#define PXN_PSCR_TXFR_FIFO_DEPTH_8      0x4000
#define PXN_PSCR_TXFR_FIFO_DEPTH_10     0x8000
#define PXN_PSCR_TXFR_FIFO_DEPTH_12     0xC000

#define PXN_PSCR_POLARITY_REVERSAL_SHIFT    1
#define PXN_PSCR_AUTO_X_MODE_SHIFT          5
#define PXN_PSCR_10BT_EXT_DIST_ENABLE_SHIFT 7

#define PXN_PSSR_JABBER             0x0001
#define PXN_PSSR_REV_POLARITY       0x0002
#define PXN_PSSR_MDIX               0x0040
#define PXN_PSSR_CABLE_LENGTH       0x0380
#define PXN_PSSR_LINK               0x0400
#define PXN_PSSR_SPD_DPLX_RESOLVED  0x0800
#define PXN_PSSR_PAGE_RCVD          0x1000
#define PXN_PSSR_DPLX               0x2000
#define PXN_PSSR_SPEED              0xC000
#define PXN_PSSR_10MBS              0x0000
#define PXN_PSSR_100MBS             0x4000
#define PXN_PSSR_1000MBS            0x8000

#define PXN_PSSR_REV_POLARITY_SHIFT         1
#define PXN_PSSR_CABLE_LENGTH_SHIFT         7

#define PXN_IER_JABBER              0x0001
#define PXN_IER_POLARITY_CHANGE     0x0002
#define PXN_IER_MDIX_CHANGE         0x0040
#define PXN_IER_FIFO_OVER_UNDERUN   0x0080
#define PXN_IER_FALSE_CARRIER       0x0100
#define PXN_IER_SYMBOL_ERROR        0x0200
#define PXN_IER_LINK_STAT_CHANGE    0x0400
#define PXN_IER_AUTO_NEG_COMPLETE   0x0800
#define PXN_IER_PAGE_RECEIVED       0x1000
#define PXN_IER_DUPLEX_CHANGED      0x2000
#define PXN_IER_SPEED_CHANGED       0x4000
#define PXN_IER_AUTO_NEG_ERR        0x8000

#define PXN_ISR_JABBER              0x0001
#define PXN_ISR_POLARITY_CHANGE     0x0002
#define PXN_ISR_MDIX_CHANGE         0x0040
#define PXN_ISR_FIFO_OVER_UNDERUN   0x0080
#define PXN_ISR_FALSE_CARRIER       0x0100
#define PXN_ISR_SYMBOL_ERROR        0x0200
#define PXN_ISR_LINK_STAT_CHANGE    0x0400
#define PXN_ISR_AUTO_NEG_COMPLETE   0x0800
#define PXN_ISR_PAGE_RECEIVED       0x1000
#define PXN_ISR_DUPLEX_CHANGED      0x2000
#define PXN_ISR_SPEED_CHANGED       0x4000
#define PXN_ISR_AUTO_NEG_ERR        0x8000

#define PXN_EPSCR_FIBER_LOOPBACK    0x4000
#define PXN_EPSCR_DOWN_NO_IDLE      0x8000

#define PXN_EPSCR_TX_CLK_2_5        0x0060
#define PXN_EPSCR_TX_CLK_25         0x0070
#define PXN_EPSCR_TX_CLK_0          0x0000

#define PXN_EPSCR_DOWN_NO_IDLE_SHIFT    15

#define PXN_LCR_LED_TX          0x0001
#define PXN_LCR_LED_RX          0x0002
#define PXN_LCR_LED_DUPLEX      0x0004
#define PXN_LCR_LINK            0x0008
#define PXN_LCR_BLINK_RATE_42MS     0x0000
#define PXN_LCR_BLINK_RATE_84MS     0x0100
#define PXN_LCR_BLINK_RATE_170MS    0x0200
#define PXN_LCR_BLINK_RATE_340MS    0x0300
#define PXN_LCR_BLINK_RATE_670MS    0x0400

#define PXN_LCR_PULSE_STRETCH_OFF   0x0000
#define PXN_LCR_PULSE_STRETCH_21_42MS   0x1000
#define PXN_LCR_PULSE_STRETCH_42_84MS   0x2000
#define PXN_LCR_PULSE_STRETCH_84_170MS  0x3000
#define PXN_LCR_PULSE_STRETCH_170_340MS 0x4000
#define PXN_LCR_PULSE_STRETCH_340_670MS 0x5000
#define PXN_LCR_PULSE_STRETCH_670_13S   0x6000
#define PXN_LCR_PULSE_STRETCH_13_26S    0x7000

#define PHY_PREAMBLE                    0xFFFFFFFF
#define PHY_SOF                         0x01
#define PHY_OP_READ                     0x02
#define PHY_OP_WRITE                    0x01
#define PHY_TURNAROUND                  0x02

#define PHY_PREAMBLE_SIZE               32

#define MII_CR_SPEED_1000               0x0040
#define MII_CR_SPEED_100                0x2000
#define MII_CR_SPEED_10                 0x0000

#define E1000_PHY_ADDRESS               0x01
#define E1000_10MB_PHY_ADDRESS          0x02

#define PHY_AUTO_NEG_TIME               45

#define PAXSON_PHY_88E1000                0x01410C50
#define PAXSON_PHY_88E1000S               0x01410C40
#define PAXSON_PHY_INTEGRATED             0x01410C30

#define PHY_REVISION_MASK               0xFFFFFFF0
#define AUTONEG_ADVERTISE_SPEED_DEFAULT 0x002F

#define DEVICE_SPEED_MASK               0x00000300

#define REG4_SPEED_MASK                 0x01E0
#define REG9_SPEED_MASK                 0x0300

#define ADVERTISE_10_HALF               0x0001
#define ADVERTISE_10_FULL               0x0002
#define ADVERTISE_100_HALF              0x0004
#define ADVERTISE_100_FULL              0x0008
#define ADVERTISE_1000_HALF             0x0010
#define ADVERTISE_1000_FULL             0x0020

#endif /* _EM_PHY_H_ */

