/*******************************************************************************

  Copyright (c) 2001-2002, Intel Corporation 
  All rights reserved.
  
  Redistribution and use in source and binary forms, with or without 
  modification, are permitted provided that the following conditions are met:
  
   1. Redistributions of source code must retain the above copyright notice, 
      this list of conditions and the following disclaimer.
  
   2. Redistributions in binary form must reproduce the above copyright 
      notice, this list of conditions and the following disclaimer in the 
      documentation and/or other materials provided with the distribution.
  
   3. Neither the name of the Intel Corporation nor the names of its 
      contributors may be used to endorse or promote products derived from 
      this software without specific prior written permission.
  
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

/*$FreeBSD$*/
/* if_em_hw.c
 * Shared functions for accessing and configuring the MAC
 */

#include <dev/em/if_em_hw.h>

static int32_t em_setup_fiber_link(struct em_hw *hw);
static int32_t em_setup_copper_link(struct em_hw *hw);
static int32_t em_phy_force_speed_duplex(struct em_hw *hw);
static int32_t em_config_mac_to_phy(struct em_hw *hw);
static int32_t em_force_mac_fc(struct em_hw *hw);
static void em_raise_mdi_clk(struct em_hw *hw, uint32_t *ctrl);
static void em_lower_mdi_clk(struct em_hw *hw, uint32_t *ctrl);
static void em_shift_out_mdi_bits(struct em_hw *hw, uint32_t data, uint16_t count);
static uint16_t em_shift_in_mdi_bits(struct em_hw *hw);
static int32_t em_phy_reset_dsp(struct em_hw *hw);
static void em_raise_ee_clk(struct em_hw *hw, uint32_t *eecd);
static void em_lower_ee_clk(struct em_hw *hw, uint32_t *eecd);
static void em_shift_out_ee_bits(struct em_hw *hw, uint16_t data, uint16_t count);
static uint16_t em_shift_in_ee_bits(struct em_hw *hw);
static void em_setup_eeprom(struct em_hw *hw);
static void em_standby_eeprom(struct em_hw *hw);
static int32_t em_id_led_init(struct em_hw * hw);

/******************************************************************************
 * Reset the transmit and receive units; mask and clear all interrupts.
 *
 * hw - Struct containing variables accessed by shared code
 *****************************************************************************/
void
em_reset_hw(struct em_hw *hw)
{
    uint32_t ctrl;
    uint32_t ctrl_ext;
    uint32_t icr;
    uint32_t manc;
    uint16_t pci_cmd_word;

    DEBUGFUNC("em_reset_hw");
    
    /* For 82542 (rev 2.0), disable MWI before issuing a device reset */
    if(hw->mac_type == em_82542_rev2_0) {
        if(hw->pci_cmd_word & CMD_MEM_WRT_INVALIDATE) {
            DEBUGOUT("Disabling MWI on 82542 rev 2.0\n");
            pci_cmd_word = hw->pci_cmd_word & ~CMD_MEM_WRT_INVALIDATE;
            em_write_pci_cfg(hw, PCI_COMMAND_REGISTER, &pci_cmd_word);
        }
    }

    /* Clear interrupt mask to stop board from generating interrupts */
    DEBUGOUT("Masking off all interrupts\n");
    E1000_WRITE_REG(hw, IMC, 0xffffffff);

    /* Disable the Transmit and Receive units.  Then delay to allow
     * any pending transactions to complete before we hit the MAC with
     * the global reset.
     */
    E1000_WRITE_REG(hw, RCTL, 0);
    E1000_WRITE_REG(hw, TCTL, E1000_TCTL_PSP);

    /* The tbi_compatibility_on Flag must be cleared when Rctl is cleared. */
    hw->tbi_compatibility_on = FALSE;

    /* Delay to allow any outstanding PCI transactions to complete before
     * resetting the device
     */ 
    msec_delay(10);

    /* Issue a global reset to the MAC.  This will reset the chip's
     * transmit, receive, DMA, and link units.  It will not effect
     * the current PCI configuration.  The global reset bit is self-
     * clearing, and should clear within a microsecond.
     */
    DEBUGOUT("Issuing a global reset to MAC\n");
    ctrl = E1000_READ_REG(hw, CTRL);

    if(hw->mac_type > em_82543)
        E1000_WRITE_REG_IO(hw, CTRL, (ctrl | E1000_CTRL_RST));
    else
        E1000_WRITE_REG(hw, CTRL, (ctrl | E1000_CTRL_RST));

    /* Force a reload from the EEPROM if necessary */
    if(hw->mac_type < em_82540) {
        /* Wait for reset to complete */
        usec_delay(10);
        ctrl_ext = E1000_READ_REG(hw, CTRL_EXT);
        ctrl_ext |= E1000_CTRL_EXT_EE_RST;
        E1000_WRITE_REG(hw, CTRL_EXT, ctrl_ext);
        /* Wait for EEPROM reload */
        msec_delay(2);
    } else {
        /* Wait for EEPROM reload (it happens automatically) */
        msec_delay(4);
        /* Dissable HW ARPs on ASF enabled adapters */
        manc = E1000_READ_REG(hw, MANC);
        manc &= ~(E1000_MANC_ARP_EN);
        E1000_WRITE_REG(hw, MANC, manc);
    }
    
    /* Clear interrupt mask to stop board from generating interrupts */
    DEBUGOUT("Masking off all interrupts\n");
    E1000_WRITE_REG(hw, IMC, 0xffffffff);

    /* Clear any pending interrupt events. */
    icr = E1000_READ_REG(hw, ICR);

    /* If MWI was previously enabled, reenable it. */
    if(hw->mac_type == em_82542_rev2_0) {
        if(hw->pci_cmd_word & CMD_MEM_WRT_INVALIDATE)
            em_write_pci_cfg(hw, PCI_COMMAND_REGISTER, &hw->pci_cmd_word);
    }
}

/******************************************************************************
 * Performs basic configuration of the adapter.
 *
 * hw - Struct containing variables accessed by shared code
 * 
 * Assumes that the controller has previously been reset and is in a 
 * post-reset uninitialized state. Initializes the receive address registers,
 * multicast table, and VLAN filter table. Calls routines to setup link
 * configuration and flow control settings. Clears all on-chip counters. Leaves
 * the transmit and receive units disabled and uninitialized.
 *****************************************************************************/
int32_t
em_init_hw(struct em_hw *hw)
{
    uint32_t ctrl, status;
    uint32_t i;
    int32_t ret_val;
    uint16_t pci_cmd_word;
    uint16_t pcix_cmd_word;
    uint16_t pcix_stat_hi_word;
    uint16_t cmd_mmrbc;
    uint16_t stat_mmrbc;

    DEBUGFUNC("em_init_hw");

    /* Initialize Identification LED */
    ret_val = em_id_led_init(hw);
    if(ret_val < 0) {
        DEBUGOUT("Error Initializing Identification LED\n");
        return ret_val;
    }
    
    /* Set the Media Type and exit with error if it is not valid. */
    if(hw->mac_type != em_82543) {
        /* tbi_compatibility is only valid on 82543 */
        hw->tbi_compatibility_en = FALSE;
    }

    if(hw->mac_type >= em_82543) {
        status = E1000_READ_REG(hw, STATUS);
        if(status & E1000_STATUS_TBIMODE) {
            hw->media_type = em_media_type_fiber;
            /* tbi_compatibility not valid on fiber */
            hw->tbi_compatibility_en = FALSE;
        } else {
            hw->media_type = em_media_type_copper;
        }
    } else {
        /* This is an 82542 (fiber only) */
        hw->media_type = em_media_type_fiber;
    }

    /* Disabling VLAN filtering. */
    DEBUGOUT("Initializing the IEEE VLAN\n");
    E1000_WRITE_REG(hw, VET, 0);

    em_clear_vfta(hw);

    /* For 82542 (rev 2.0), disable MWI and put the receiver into reset */
    if(hw->mac_type == em_82542_rev2_0) {
        if(hw->pci_cmd_word & CMD_MEM_WRT_INVALIDATE) {
            DEBUGOUT("Disabling MWI on 82542 rev 2.0\n");
            pci_cmd_word = hw->pci_cmd_word & ~CMD_MEM_WRT_INVALIDATE;
            em_write_pci_cfg(hw, PCI_COMMAND_REGISTER, &pci_cmd_word);
        }
        E1000_WRITE_REG(hw, RCTL, E1000_RCTL_RST);
        msec_delay(5);
    }

    /* Setup the receive address. This involves initializing all of the Receive
     * Address Registers (RARs 0 - 15).
     */
    em_init_rx_addrs(hw);

    /* For 82542 (rev 2.0), take the receiver out of reset and enable MWI */
    if(hw->mac_type == em_82542_rev2_0) {
        E1000_WRITE_REG(hw, RCTL, 0);
        msec_delay(1);
        if(hw->pci_cmd_word & CMD_MEM_WRT_INVALIDATE)
            em_write_pci_cfg(hw, PCI_COMMAND_REGISTER, &hw->pci_cmd_word);
    }

    /* Zero out the Multicast HASH table */
    DEBUGOUT("Zeroing the MTA\n");
    for(i = 0; i < E1000_MC_TBL_SIZE; i++)
        E1000_WRITE_REG_ARRAY(hw, MTA, i, 0);

    /* Set the PCI priority bit correctly in the CTRL register.  This
     * determines if the adapter gives priority to receives, or if it
     * gives equal priority to transmits and receives.
     */
    if(hw->dma_fairness) {
        ctrl = E1000_READ_REG(hw, CTRL);
        E1000_WRITE_REG(hw, CTRL, ctrl | E1000_CTRL_PRIOR);
    }

    /* Workaround for PCI-X problem when BIOS sets MMRBC incorrectly. */
    if(hw->bus_type == em_bus_type_pcix) {
        em_read_pci_cfg(hw, PCIX_COMMAND_REGISTER, &pcix_cmd_word);
        em_read_pci_cfg(hw, PCIX_STATUS_REGISTER_HI, &pcix_stat_hi_word);
        cmd_mmrbc = (pcix_cmd_word & PCIX_COMMAND_MMRBC_MASK) >>
            PCIX_COMMAND_MMRBC_SHIFT;
        stat_mmrbc = (pcix_stat_hi_word & PCIX_STATUS_HI_MMRBC_MASK) >>
            PCIX_STATUS_HI_MMRBC_SHIFT;
        if(cmd_mmrbc > stat_mmrbc) {
            pcix_cmd_word &= ~PCIX_COMMAND_MMRBC_MASK;
            pcix_cmd_word |= stat_mmrbc << PCIX_COMMAND_MMRBC_SHIFT;
            em_write_pci_cfg(hw, PCIX_COMMAND_REGISTER, &pcix_cmd_word);
        }
    }

    /* Call a subroutine to configure the link and setup flow control. */
    ret_val = em_setup_link(hw);

    /* Clear all of the statistics registers (clear on read).  It is
     * important that we do this after we have tried to establish link
     * because the symbol error count will increment wildly if there
     * is no link.
     */
    em_clear_hw_cntrs(hw);

    return ret_val;
}

/******************************************************************************
 * Configures flow control and link settings.
 * 
 * hw - Struct containing variables accessed by shared code
 * 
 * Determines which flow control settings to use. Calls the apropriate media-
 * specific link configuration function. Configures the flow control settings.
 * Assuming the adapter has a valid link partner, a valid link should be
 * established. Assumes the hardware has previously been reset and the 
 * transmitter and receiver are not enabled.
 *****************************************************************************/
int32_t
em_setup_link(struct em_hw *hw)
{
    uint32_t ctrl_ext;
    int32_t ret_val;
    uint16_t eeprom_data;

    DEBUGFUNC("em_setup_link");

    /* Read and store word 0x0F of the EEPROM. This word contains bits
     * that determine the hardware's default PAUSE (flow control) mode,
     * a bit that determines whether the HW defaults to enabling or
     * disabling auto-negotiation, and the direction of the
     * SW defined pins. If there is no SW over-ride of the flow
     * control setting, then the variable hw->fc will
     * be initialized based on a value in the EEPROM.
     */
    if(em_read_eeprom(hw, EEPROM_INIT_CONTROL2_REG, &eeprom_data) < 0) {
        DEBUGOUT("EEPROM Read Error\n");
        return -E1000_ERR_EEPROM;
    }

    if(hw->fc == em_fc_default) {
        if((eeprom_data & EEPROM_WORD0F_PAUSE_MASK) == 0)
            hw->fc = em_fc_none;
        else if((eeprom_data & EEPROM_WORD0F_PAUSE_MASK) == 
                EEPROM_WORD0F_ASM_DIR)
            hw->fc = em_fc_tx_pause;
        else
            hw->fc = em_fc_full;
    }

    /* We want to save off the original Flow Control configuration just
     * in case we get disconnected and then reconnected into a different
     * hub or switch with different Flow Control capabilities.
     */
    if(hw->mac_type == em_82542_rev2_0)
        hw->fc &= (~em_fc_tx_pause);

    if((hw->mac_type < em_82543) && (hw->report_tx_early == 1))
        hw->fc &= (~em_fc_rx_pause);

    hw->original_fc = hw->fc;

    DEBUGOUT1("After fix-ups FlowControl is now = %x\n", hw->fc);

    /* Take the 4 bits from EEPROM word 0x0F that determine the initial
     * polarity value for the SW controlled pins, and setup the
     * Extended Device Control reg with that info.
     * This is needed because one of the SW controlled pins is used for
     * signal detection.  So this should be done before em_setup_pcs_link()
     * or em_phy_setup() is called.
     */
    if(hw->mac_type == em_82543) {
        ctrl_ext = ((eeprom_data & EEPROM_WORD0F_SWPDIO_EXT) << 
                    SWDPIO__EXT_SHIFT);
        E1000_WRITE_REG(hw, CTRL_EXT, ctrl_ext);
    }

    /* Call the necessary subroutine to configure the link. */
    ret_val = (hw->media_type == em_media_type_fiber) ?
              em_setup_fiber_link(hw) :
              em_setup_copper_link(hw);

    /* Initialize the flow control address, type, and PAUSE timer
     * registers to their default values.  This is done even if flow
     * control is disabled, because it does not hurt anything to
     * initialize these registers.
     */
    DEBUGOUT("Initializing the Flow Control address, type and timer regs\n");

    E1000_WRITE_REG(hw, FCAL, FLOW_CONTROL_ADDRESS_LOW);
    E1000_WRITE_REG(hw, FCAH, FLOW_CONTROL_ADDRESS_HIGH);
    E1000_WRITE_REG(hw, FCT, FLOW_CONTROL_TYPE);
    E1000_WRITE_REG(hw, FCTTV, hw->fc_pause_time);

    /* Set the flow control receive threshold registers.  Normally,
     * these registers will be set to a default threshold that may be
     * adjusted later by the driver's runtime code.  However, if the
     * ability to transmit pause frames in not enabled, then these
     * registers will be set to 0. 
     */
    if(!(hw->fc & em_fc_tx_pause)) {
        E1000_WRITE_REG(hw, FCRTL, 0);
        E1000_WRITE_REG(hw, FCRTH, 0);
    } else {
        /* We need to set up the Receive Threshold high and low water marks
         * as well as (optionally) enabling the transmission of XON frames.
         */
        if(hw->fc_send_xon) {
            E1000_WRITE_REG(hw, FCRTL, (hw->fc_low_water | E1000_FCRTL_XONE));
            E1000_WRITE_REG(hw, FCRTH, hw->fc_high_water);
        } else {
            E1000_WRITE_REG(hw, FCRTL, hw->fc_low_water);
            E1000_WRITE_REG(hw, FCRTH, hw->fc_high_water);
        }
    }
    return ret_val;
}

/******************************************************************************
 * Sets up link for a fiber based adapter
 *
 * hw - Struct containing variables accessed by shared code
 * ctrl - Current value of the device control register
 *
 * Manipulates Physical Coding Sublayer functions in order to configure
 * link. Assumes the hardware has been previously reset and the transmitter
 * and receiver are not enabled.
 *****************************************************************************/
static int32_t 
em_setup_fiber_link(struct em_hw *hw)
{
    uint32_t ctrl;
    uint32_t status;
    uint32_t txcw = 0;
    uint32_t i;
    uint32_t signal;
    int32_t ret_val;

    DEBUGFUNC("em_setup_fiber_link");

    /* On adapters with a MAC newer that 82544, SW Defineable pin 1 will be 
     * set when the optics detect a signal. On older adapters, it will be 
     * cleared when there is a signal
     */
    ctrl = E1000_READ_REG(hw, CTRL);
    if(hw->mac_type > em_82544) signal = E1000_CTRL_SWDPIN1;
    else signal = 0;
   
    /* Take the link out of reset */
    ctrl &= ~(E1000_CTRL_LRST);
    
    em_config_collision_dist(hw);

    /* Check for a software override of the flow control settings, and setup
     * the device accordingly.  If auto-negotiation is enabled, then software
     * will have to set the "PAUSE" bits to the correct value in the Tranmsit
     * Config Word Register (TXCW) and re-start auto-negotiation.  However, if
     * auto-negotiation is disabled, then software will have to manually 
     * configure the two flow control enable bits in the CTRL register.
     *
     * The possible values of the "fc" parameter are:
     *      0:  Flow control is completely disabled
     *      1:  Rx flow control is enabled (we can receive pause frames, but 
     *          not send pause frames).
     *      2:  Tx flow control is enabled (we can send pause frames but we do
     *          not support receiving pause frames).
     *      3:  Both Rx and TX flow control (symmetric) are enabled.
     */
    switch (hw->fc) {
    case em_fc_none:
        /* Flow control is completely disabled by a software over-ride. */
        txcw = (E1000_TXCW_ANE | E1000_TXCW_FD);
        break;
    case em_fc_rx_pause:
        /* RX Flow control is enabled and TX Flow control is disabled by a 
         * software over-ride. Since there really isn't a way to advertise 
         * that we are capable of RX Pause ONLY, we will advertise that we
         * support both symmetric and asymmetric RX PAUSE. Later, we will
         *  disable the adapter's ability to send PAUSE frames.
         */
        txcw = (E1000_TXCW_ANE | E1000_TXCW_FD | E1000_TXCW_PAUSE_MASK);
        break;
    case em_fc_tx_pause:
        /* TX Flow control is enabled, and RX Flow control is disabled, by a 
         * software over-ride.
         */
        txcw = (E1000_TXCW_ANE | E1000_TXCW_FD | E1000_TXCW_ASM_DIR);
        break;
    case em_fc_full:
        /* Flow control (both RX and TX) is enabled by a software over-ride. */
        txcw = (E1000_TXCW_ANE | E1000_TXCW_FD | E1000_TXCW_PAUSE_MASK);
        break;
    default:
        DEBUGOUT("Flow control param set incorrectly\n");
        return -E1000_ERR_CONFIG;
        break;
    }

    /* Since auto-negotiation is enabled, take the link out of reset (the link
     * will be in reset, because we previously reset the chip). This will
     * restart auto-negotiation.  If auto-neogtiation is successful then the
     * link-up status bit will be set and the flow control enable bits (RFCE
     * and TFCE) will be set according to their negotiated value.
     */
    DEBUGOUT("Auto-negotiation enabled\n");

    E1000_WRITE_REG(hw, TXCW, txcw);
    E1000_WRITE_REG(hw, CTRL, ctrl);

    hw->txcw = txcw;
    msec_delay(1);

    /* If we have a signal (the cable is plugged in) then poll for a "Link-Up"
     * indication in the Device Status Register.  Time-out if a link isn't 
     * seen in 500 milliseconds seconds (Auto-negotiation should complete in 
     * less than 500 milliseconds even if the other end is doing it in SW).
     */
    if((E1000_READ_REG(hw, CTRL) & E1000_CTRL_SWDPIN1) == signal) {
        DEBUGOUT("Looking for Link\n");
        for(i = 0; i < (LINK_UP_TIMEOUT / 10); i++) {
            msec_delay(10);
            status = E1000_READ_REG(hw, STATUS);
            if(status & E1000_STATUS_LU) break;
        }
        if(i == (LINK_UP_TIMEOUT / 10)) {
            /* AutoNeg failed to achieve a link, so we'll call 
             * em_check_for_link. This routine will force the link up if we
             * detect a signal. This will allow us to communicate with
             * non-autonegotiating link partners.
             */
            DEBUGOUT("Never got a valid link from auto-neg!!!\n");
            hw->autoneg_failed = 1;
            ret_val = em_check_for_link(hw);
            if(ret_val < 0) {
                DEBUGOUT("Error while checking for link\n");
                return ret_val;
            }
            hw->autoneg_failed = 0;
        } else {
            hw->autoneg_failed = 0;
            DEBUGOUT("Valid Link Found\n");
        }
    } else {
        DEBUGOUT("No Signal Detected\n");
    }
    return 0;
}

/******************************************************************************
* Detects which PHY is present and the speed and duplex
*
* hw - Struct containing variables accessed by shared code
* ctrl - current value of the device control register
******************************************************************************/
static int32_t 
em_setup_copper_link(struct em_hw *hw)
{
    uint32_t ctrl;
    int32_t ret_val;
    uint16_t i;
    uint16_t phy_data;

    DEBUGFUNC("em_setup_copper_link");

    ctrl = E1000_READ_REG(hw, CTRL);
    /* With 82543, we need to force speed and duplex on the MAC equal to what
     * the PHY speed and duplex configuration is. In addition, we need to
     * perform a hardware reset on the PHY to take it out of reset.
     */
    if(hw->mac_type > em_82543) {
        ctrl |= E1000_CTRL_SLU;
        ctrl &= ~(E1000_CTRL_FRCSPD | E1000_CTRL_FRCDPX);
        E1000_WRITE_REG(hw, CTRL, ctrl);
    } else {
        ctrl |= (E1000_CTRL_FRCSPD | E1000_CTRL_FRCDPX | E1000_CTRL_SLU);
        E1000_WRITE_REG(hw, CTRL, ctrl);
        em_phy_hw_reset(hw);
    }

    /* Make sure we have a valid PHY */
    ret_val = em_detect_gig_phy(hw);
    if(ret_val < 0) {
        DEBUGOUT("Error, did not detect valid phy.\n");
        return ret_val;
    }
    DEBUGOUT1("Phy ID = %x \n", hw->phy_id);

    /* Enable CRS on TX. This must be set for half-duplex operation. */
    if(em_read_phy_reg(hw, M88E1000_PHY_SPEC_CTRL, &phy_data) < 0) {
        DEBUGOUT("PHY Read Error\n");
        return -E1000_ERR_PHY;
    }
    phy_data |= M88E1000_PSCR_ASSERT_CRS_ON_TX;

    /* Options:
     *   MDI/MDI-X = 0 (default)
     *   0 - Auto for all speeds
     *   1 - MDI mode
     *   2 - MDI-X mode
     *   3 - Auto for 1000Base-T only (MDI-X for 10/100Base-T modes)
     */
    phy_data &= ~M88E1000_PSCR_AUTO_X_MODE;

    switch (hw->mdix) {
    case 1:
        phy_data |= M88E1000_PSCR_MDI_MANUAL_MODE;
        break;
    case 2:
        phy_data |= M88E1000_PSCR_MDIX_MANUAL_MODE;
        break;
    case 3:
        phy_data |= M88E1000_PSCR_AUTO_X_1000T;
        break;
    case 0:
    default:
        phy_data |= M88E1000_PSCR_AUTO_X_MODE;
        break;
    }

    /* Options:
     *   disable_polarity_correction = 0 (default)
     *       Automatic Correction for Reversed Cable Polarity
     *   0 - Disabled
     *   1 - Enabled
     */
    phy_data &= ~M88E1000_PSCR_POLARITY_REVERSAL;
    if(hw->disable_polarity_correction == 1)
        phy_data |= M88E1000_PSCR_POLARITY_REVERSAL;
    if(em_write_phy_reg(hw, M88E1000_PHY_SPEC_CTRL, phy_data) < 0) {
        DEBUGOUT("PHY Write Error\n");
        return -E1000_ERR_PHY;
    }

    /* Force TX_CLK in the Extended PHY Specific Control Register
     * to 25MHz clock.
     */
    if(em_read_phy_reg(hw, M88E1000_EXT_PHY_SPEC_CTRL, &phy_data) < 0) {
        DEBUGOUT("PHY Read Error\n");
        return -E1000_ERR_PHY;
    }
    phy_data |= M88E1000_EPSCR_TX_CLK_25;
    /* Configure Master and Slave downshift values */
    phy_data &= ~(M88E1000_EPSCR_MASTER_DOWNSHIFT_MASK |
                  M88E1000_EPSCR_SLAVE_DOWNSHIFT_MASK);
    phy_data |= (M88E1000_EPSCR_MASTER_DOWNSHIFT_1X |
                 M88E1000_EPSCR_SLAVE_DOWNSHIFT_1X);
    if(em_write_phy_reg(hw, M88E1000_EXT_PHY_SPEC_CTRL, phy_data) < 0) {
        DEBUGOUT("PHY Write Error\n");
        return -E1000_ERR_PHY;
    }

    /* SW Reset the PHY so all changes take effect */
    ret_val = em_phy_reset(hw);
    if(ret_val < 0) {
        DEBUGOUT("Error Resetting the PHY\n");
        return ret_val;
    }
    
    /* Options:
     *   autoneg = 1 (default)
     *      PHY will advertise value(s) parsed from
     *      autoneg_advertised and fc
     *   autoneg = 0
     *      PHY will be set to 10H, 10F, 100H, or 100F
     *      depending on value parsed from forced_speed_duplex.
     */

    /* Is autoneg enabled?  This is enabled by default or by software override.
     * If so, call em_phy_setup_autoneg routine to parse the
     * autoneg_advertised and fc options. If autoneg is NOT enabled, then the
     * user should have provided a speed/duplex override.  If so, then call
     * em_phy_force_speed_duplex to parse and set this up.
     */
    if(hw->autoneg) {
        /* Perform some bounds checking on the hw->autoneg_advertised
         * parameter.  If this variable is zero, then set it to the default.
         */
        hw->autoneg_advertised &= AUTONEG_ADVERTISE_SPEED_DEFAULT;

        /* If autoneg_advertised is zero, we assume it was not defaulted
         * by the calling code so we set to advertise full capability.
         */
        if(hw->autoneg_advertised == 0)
            hw->autoneg_advertised = AUTONEG_ADVERTISE_SPEED_DEFAULT;

        DEBUGOUT("Reconfiguring auto-neg advertisement params\n");
        ret_val = em_phy_setup_autoneg(hw);
        if(ret_val < 0) {
            DEBUGOUT("Error Setting up Auto-Negotiation\n");
            return ret_val;
        }
        DEBUGOUT("Restarting Auto-Neg\n");

        /* Restart auto-negotiation by setting the Auto Neg Enable bit and
         * the Auto Neg Restart bit in the PHY control register.
         */
        if(em_read_phy_reg(hw, PHY_CTRL, &phy_data) < 0) {
            DEBUGOUT("PHY Read Error\n");
            return -E1000_ERR_PHY;
        }
        phy_data |= (MII_CR_AUTO_NEG_EN | MII_CR_RESTART_AUTO_NEG);
        if(em_write_phy_reg(hw, PHY_CTRL, phy_data) < 0) {
            DEBUGOUT("PHY Write Error\n");
            return -E1000_ERR_PHY;
        }

        /* Does the user want to wait for Auto-Neg to complete here, or
         * check at a later time (for example, callback routine).
         */
        if(hw->wait_autoneg_complete) {
            ret_val = em_wait_autoneg(hw);
            if(ret_val < 0) {
                DEBUGOUT("Error while waiting for autoneg to complete\n");
                return ret_val;
            }
        }
    } else {
        DEBUGOUT("Forcing speed and duplex\n");
        ret_val = em_phy_force_speed_duplex(hw);
        if(ret_val < 0) {
            DEBUGOUT("Error Forcing Speed and Duplex\n");
            return ret_val;
        }
    }

    /* Check link status. Wait up to 100 microseconds for link to become
     * valid.
     */
    for(i = 0; i < 10; i++) {
        if(em_read_phy_reg(hw, PHY_STATUS, &phy_data) < 0) {
            DEBUGOUT("PHY Read Error\n");
            return -E1000_ERR_PHY;
        }
        if(em_read_phy_reg(hw, PHY_STATUS, &phy_data) < 0) {
            DEBUGOUT("PHY Read Error\n");
            return -E1000_ERR_PHY;
        }
        if(phy_data & MII_SR_LINK_STATUS) {
            /* We have link, so we need to finish the config process:
             *   1) Set up the MAC to the current PHY speed/duplex
             *      if we are on 82543.  If we
             *      are on newer silicon, we only need to configure
             *      collision distance in the Transmit Control Register.
             *   2) Set up flow control on the MAC to that established with
             *      the link partner.
             */
            if(hw->mac_type >= em_82544) {
                em_config_collision_dist(hw);
            } else {
                ret_val = em_config_mac_to_phy(hw);
                if(ret_val < 0) {
                    DEBUGOUT("Error configuring MAC to PHY settings\n");
                    return ret_val;
                  }
            }
            ret_val = em_config_fc_after_link_up(hw);
            if(ret_val < 0) {
                DEBUGOUT("Error Configuring Flow Control\n");
                return ret_val;
            }
            DEBUGOUT("Valid link established!!!\n");
            return 0;
        }
        usec_delay(10);
    }

    DEBUGOUT("Unable to establish link!!!\n");
    return 0;
}

/******************************************************************************
* Configures PHY autoneg and flow control advertisement settings
*
* hw - Struct containing variables accessed by shared code
******************************************************************************/
int32_t
em_phy_setup_autoneg(struct em_hw *hw)
{
    uint16_t mii_autoneg_adv_reg;
    uint16_t mii_1000t_ctrl_reg;

    DEBUGFUNC("em_phy_setup_autoneg");

    /* Read the MII Auto-Neg Advertisement Register (Address 4). */
    if(em_read_phy_reg(hw, PHY_AUTONEG_ADV, &mii_autoneg_adv_reg) < 0) {
        DEBUGOUT("PHY Read Error\n");
        return -E1000_ERR_PHY;
    }

    /* Read the MII 1000Base-T Control Register (Address 9). */
    if(em_read_phy_reg(hw, PHY_1000T_CTRL, &mii_1000t_ctrl_reg) < 0) {
        DEBUGOUT("PHY Read Error\n");
        return -E1000_ERR_PHY;
    }

    /* Need to parse both autoneg_advertised and fc and set up
     * the appropriate PHY registers.  First we will parse for
     * autoneg_advertised software override.  Since we can advertise
     * a plethora of combinations, we need to check each bit
     * individually.
     */

    /* First we clear all the 10/100 mb speed bits in the Auto-Neg
     * Advertisement Register (Address 4) and the 1000 mb speed bits in
     * the  1000Base-T Control Register (Address 9).
     */
    mii_autoneg_adv_reg &= ~REG4_SPEED_MASK;
    mii_1000t_ctrl_reg &= ~REG9_SPEED_MASK;

    DEBUGOUT1("autoneg_advertised %x\n", hw->autoneg_advertised);

    /* Do we want to advertise 10 Mb Half Duplex? */
    if(hw->autoneg_advertised & ADVERTISE_10_HALF) {
        DEBUGOUT("Advertise 10mb Half duplex\n");
        mii_autoneg_adv_reg |= NWAY_AR_10T_HD_CAPS;
    }

    /* Do we want to advertise 10 Mb Full Duplex? */
    if(hw->autoneg_advertised & ADVERTISE_10_FULL) {
        DEBUGOUT("Advertise 10mb Full duplex\n");
        mii_autoneg_adv_reg |= NWAY_AR_10T_FD_CAPS;
    }

    /* Do we want to advertise 100 Mb Half Duplex? */
    if(hw->autoneg_advertised & ADVERTISE_100_HALF) {
        DEBUGOUT("Advertise 100mb Half duplex\n");
        mii_autoneg_adv_reg |= NWAY_AR_100TX_HD_CAPS;
    }

    /* Do we want to advertise 100 Mb Full Duplex? */
    if(hw->autoneg_advertised & ADVERTISE_100_FULL) {
        DEBUGOUT("Advertise 100mb Full duplex\n");
        mii_autoneg_adv_reg |= NWAY_AR_100TX_FD_CAPS;
    }

    /* We do not allow the Phy to advertise 1000 Mb Half Duplex */
    if(hw->autoneg_advertised & ADVERTISE_1000_HALF) {
        DEBUGOUT("Advertise 1000mb Half duplex requested, request denied!\n");
    }

    /* Do we want to advertise 1000 Mb Full Duplex? */
    if(hw->autoneg_advertised & ADVERTISE_1000_FULL) {
        DEBUGOUT("Advertise 1000mb Full duplex\n");
        mii_1000t_ctrl_reg |= CR_1000T_FD_CAPS;
    }

    /* Check for a software override of the flow control settings, and
     * setup the PHY advertisement registers accordingly.  If
     * auto-negotiation is enabled, then software will have to set the
     * "PAUSE" bits to the correct value in the Auto-Negotiation
     * Advertisement Register (PHY_AUTONEG_ADV) and re-start auto-negotiation.
     *
     * The possible values of the "fc" parameter are:
     *      0:  Flow control is completely disabled
     *      1:  Rx flow control is enabled (we can receive pause frames
     *          but not send pause frames).
     *      2:  Tx flow control is enabled (we can send pause frames
     *          but we do not support receiving pause frames).
     *      3:  Both Rx and TX flow control (symmetric) are enabled.
     *  other:  No software override.  The flow control configuration
     *          in the EEPROM is used.
     */
    switch (hw->fc) {
    case em_fc_none: /* 0 */
        /* Flow control (RX & TX) is completely disabled by a
         * software over-ride.
         */
        mii_autoneg_adv_reg &= ~(NWAY_AR_ASM_DIR | NWAY_AR_PAUSE);
        break;
    case em_fc_rx_pause: /* 1 */
        /* RX Flow control is enabled, and TX Flow control is
         * disabled, by a software over-ride.
         */
        /* Since there really isn't a way to advertise that we are
         * capable of RX Pause ONLY, we will advertise that we
         * support both symmetric and asymmetric RX PAUSE.  Later
         * (in em_config_fc_after_link_up) we will disable the
         *hw's ability to send PAUSE frames.
         */
        mii_autoneg_adv_reg |= (NWAY_AR_ASM_DIR | NWAY_AR_PAUSE);
        break;
    case em_fc_tx_pause: /* 2 */
        /* TX Flow control is enabled, and RX Flow control is
         * disabled, by a software over-ride.
         */
        mii_autoneg_adv_reg |= NWAY_AR_ASM_DIR;
        mii_autoneg_adv_reg &= ~NWAY_AR_PAUSE;
        break;
    case em_fc_full: /* 3 */
        /* Flow control (both RX and TX) is enabled by a software
         * over-ride.
         */
        mii_autoneg_adv_reg |= (NWAY_AR_ASM_DIR | NWAY_AR_PAUSE);
        break;
    default:
        DEBUGOUT("Flow control param set incorrectly\n");
        return -E1000_ERR_CONFIG;
    }

    if(em_write_phy_reg(hw, PHY_AUTONEG_ADV, mii_autoneg_adv_reg) < 0) {
        DEBUGOUT("PHY Write Error\n");
        return -E1000_ERR_PHY;
    }

    DEBUGOUT1("Auto-Neg Advertising %x\n", mii_autoneg_adv_reg);

    if(em_write_phy_reg(hw, PHY_1000T_CTRL, mii_1000t_ctrl_reg) < 0) {
        DEBUGOUT("PHY Write Error\n");
        return -E1000_ERR_PHY;
    }
    return 0;
}

/******************************************************************************
* Force PHY speed and duplex settings to hw->forced_speed_duplex
*
* hw - Struct containing variables accessed by shared code
******************************************************************************/
static int32_t
em_phy_force_speed_duplex(struct em_hw *hw)
{
    uint32_t ctrl;
    int32_t ret_val;
    uint16_t mii_ctrl_reg;
    uint16_t mii_status_reg;
    uint16_t phy_data;
    uint16_t i;

    DEBUGFUNC("em_phy_force_speed_duplex");

    /* Turn off Flow control if we are forcing speed and duplex. */
    hw->fc = em_fc_none;

    DEBUGOUT1("hw->fc = %d\n", hw->fc);

    /* Read the Device Control Register. */
    ctrl = E1000_READ_REG(hw, CTRL);

    /* Set the bits to Force Speed and Duplex in the Device Ctrl Reg. */
    ctrl |= (E1000_CTRL_FRCSPD | E1000_CTRL_FRCDPX);
    ctrl &= ~(DEVICE_SPEED_MASK);

    /* Clear the Auto Speed Detect Enable bit. */
    ctrl &= ~E1000_CTRL_ASDE;

    /* Read the MII Control Register. */
    if(em_read_phy_reg(hw, PHY_CTRL, &mii_ctrl_reg) < 0) {
        DEBUGOUT("PHY Read Error\n");
        return -E1000_ERR_PHY;
    }

    /* We need to disable autoneg in order to force link and duplex. */

    mii_ctrl_reg &= ~MII_CR_AUTO_NEG_EN;

    /* Are we forcing Full or Half Duplex? */
    if(hw->forced_speed_duplex == em_100_full ||
       hw->forced_speed_duplex == em_10_full) {
        /* We want to force full duplex so we SET the full duplex bits in the
         * Device and MII Control Registers.
         */
        ctrl |= E1000_CTRL_FD;
        mii_ctrl_reg |= MII_CR_FULL_DUPLEX;
        DEBUGOUT("Full Duplex\n");
    } else {
        /* We want to force half duplex so we CLEAR the full duplex bits in
         * the Device and MII Control Registers.
         */
        ctrl &= ~E1000_CTRL_FD;
        mii_ctrl_reg &= ~MII_CR_FULL_DUPLEX;
        DEBUGOUT("Half Duplex\n");
    }

    /* Are we forcing 100Mbps??? */
    if(hw->forced_speed_duplex == em_100_full ||
       hw->forced_speed_duplex == em_100_half) {
        /* Set the 100Mb bit and turn off the 1000Mb and 10Mb bits. */
        ctrl |= E1000_CTRL_SPD_100;
        mii_ctrl_reg |= MII_CR_SPEED_100;
        mii_ctrl_reg &= ~(MII_CR_SPEED_1000 | MII_CR_SPEED_10);
        DEBUGOUT("Forcing 100mb ");
    } else {
        /* Set the 10Mb bit and turn off the 1000Mb and 100Mb bits. */
        ctrl &= ~(E1000_CTRL_SPD_1000 | E1000_CTRL_SPD_100);
        mii_ctrl_reg |= MII_CR_SPEED_10;
        mii_ctrl_reg &= ~(MII_CR_SPEED_1000 | MII_CR_SPEED_100);
        DEBUGOUT("Forcing 10mb ");
    }

    em_config_collision_dist(hw);

    /* Write the configured values back to the Device Control Reg. */
    E1000_WRITE_REG(hw, CTRL, ctrl);

    /* Write the MII Control Register with the new PHY configuration. */
    if(em_read_phy_reg(hw, M88E1000_PHY_SPEC_CTRL, &phy_data) < 0) {
        DEBUGOUT("PHY Read Error\n");
        return -E1000_ERR_PHY;
    }

    /* Clear Auto-Crossover to force MDI manually. M88E1000 requires MDI
     * forced whenever speed are duplex are forced.
     */
    phy_data &= ~M88E1000_PSCR_AUTO_X_MODE;
    if(em_write_phy_reg(hw, M88E1000_PHY_SPEC_CTRL, phy_data) < 0) {
        DEBUGOUT("PHY Write Error\n");
        return -E1000_ERR_PHY;
    }
    DEBUGOUT1("M88E1000 PSCR: %x \n", phy_data);
    
    /* Need to reset the PHY or these changes will be ignored */
    mii_ctrl_reg |= MII_CR_RESET;
    if(em_write_phy_reg(hw, PHY_CTRL, mii_ctrl_reg) < 0) {
        DEBUGOUT("PHY Write Error\n");
        return -E1000_ERR_PHY;
    }
    usec_delay(1);

    /* The wait_autoneg_complete flag may be a little misleading here.
     * Since we are forcing speed and duplex, Auto-Neg is not enabled.
     * But we do want to delay for a period while forcing only so we
     * don't generate false No Link messages.  So we will wait here
     * only if the user has set wait_autoneg_complete to 1, which is
     * the default.
     */
    if(hw->wait_autoneg_complete) {
        /* We will wait for autoneg to complete. */
        DEBUGOUT("Waiting for forced speed/duplex link.\n");
        mii_status_reg = 0;

        /* We will wait for autoneg to complete or 4.5 seconds to expire. */
        for(i = PHY_FORCE_TIME; i > 0; i--) {
            /* Read the MII Status Register and wait for Auto-Neg Complete bit
             * to be set.
             */
            if(em_read_phy_reg(hw, PHY_STATUS, &mii_status_reg) < 0) {
                DEBUGOUT("PHY Read Error\n");
                return -E1000_ERR_PHY;
            }
            if(em_read_phy_reg(hw, PHY_STATUS, &mii_status_reg) < 0) {
                DEBUGOUT("PHY Read Error\n");
                return -E1000_ERR_PHY;
            }
            if(mii_status_reg & MII_SR_LINK_STATUS) break;
            msec_delay(100);
        }
        if(i == 0) { /* We didn't get link */
            /* Reset the DSP and wait again for link. */
            
            ret_val = em_phy_reset_dsp(hw);
            if(ret_val < 0) {
                DEBUGOUT("Error Resetting PHY DSP\n");
                return ret_val;
            }
        }
        /* This loop will early-out if the link condition has been met.  */
        for(i = PHY_FORCE_TIME; i > 0; i--) {
            if(mii_status_reg & MII_SR_LINK_STATUS) break;
            msec_delay(100);
            /* Read the MII Status Register and wait for Auto-Neg Complete bit
             * to be set.
             */
            if(em_read_phy_reg(hw, PHY_STATUS, &mii_status_reg) < 0) {
                DEBUGOUT("PHY Read Error\n");
                return -E1000_ERR_PHY;
            }
            if(em_read_phy_reg(hw, PHY_STATUS, &mii_status_reg) < 0) {
                DEBUGOUT("PHY Read Error\n");
                return -E1000_ERR_PHY;
            }
        }
    }
    
    /* Because we reset the PHY above, we need to re-force TX_CLK in the
     * Extended PHY Specific Control Register to 25MHz clock.  This value
     * defaults back to a 2.5MHz clock when the PHY is reset.
     */
    if(em_read_phy_reg(hw, M88E1000_EXT_PHY_SPEC_CTRL, &phy_data) < 0) {
        DEBUGOUT("PHY Read Error\n");
        return -E1000_ERR_PHY;
    }
    phy_data |= M88E1000_EPSCR_TX_CLK_25;
    if(em_write_phy_reg(hw, M88E1000_EXT_PHY_SPEC_CTRL, phy_data) < 0) {
        DEBUGOUT("PHY Write Error\n");
        return -E1000_ERR_PHY;
    }

    /* In addition, because of the s/w reset above, we need to enable CRS on
     * TX.  This must be set for both full and half duplex operation.
     */
    if(em_read_phy_reg(hw, M88E1000_PHY_SPEC_CTRL, &phy_data) < 0) {
        DEBUGOUT("PHY Read Error\n");
        return -E1000_ERR_PHY;
    }
    phy_data |= M88E1000_PSCR_ASSERT_CRS_ON_TX;
    if(em_write_phy_reg(hw, M88E1000_PHY_SPEC_CTRL, phy_data) < 0) {
        DEBUGOUT("PHY Write Error\n");
        return -E1000_ERR_PHY;
    }
    return 0;
}

/******************************************************************************
* Sets the collision distance in the Transmit Control register
*
* hw - Struct containing variables accessed by shared code
*
* Link should have been established previously. Reads the speed and duplex
* information from the Device Status register.
******************************************************************************/
void
em_config_collision_dist(struct em_hw *hw)
{
    uint32_t tctl;

    tctl = E1000_READ_REG(hw, TCTL);

    tctl &= ~E1000_TCTL_COLD;
    tctl |= E1000_COLLISION_DISTANCE << E1000_COLD_SHIFT;

    E1000_WRITE_REG(hw, TCTL, tctl);
}

/******************************************************************************
* Sets MAC speed and duplex settings to reflect the those in the PHY
*
* hw - Struct containing variables accessed by shared code
* mii_reg - data to write to the MII control register
*
* The contents of the PHY register containing the needed information need to
* be passed in.
******************************************************************************/
static int32_t
em_config_mac_to_phy(struct em_hw *hw)
{
    uint32_t ctrl;
    uint16_t phy_data;

    DEBUGFUNC("em_config_mac_to_phy");

    /* Read the Device Control Register and set the bits to Force Speed
     * and Duplex.
     */
    ctrl = E1000_READ_REG(hw, CTRL);
    ctrl |= (E1000_CTRL_FRCSPD | E1000_CTRL_FRCDPX);
    ctrl &= ~(E1000_CTRL_SPD_SEL | E1000_CTRL_ILOS);

    /* Set up duplex in the Device Control and Transmit Control
     * registers depending on negotiated values.
     */
    if(em_read_phy_reg(hw, M88E1000_PHY_SPEC_STATUS, &phy_data) < 0) {
        DEBUGOUT("PHY Read Error\n");
        return -E1000_ERR_PHY;
    }
    if(phy_data & M88E1000_PSSR_DPLX) ctrl |= E1000_CTRL_FD;
    else ctrl &= ~E1000_CTRL_FD;

    em_config_collision_dist(hw);

    /* Set up speed in the Device Control register depending on
     * negotiated values.
     */
    if((phy_data & M88E1000_PSSR_SPEED) == M88E1000_PSSR_1000MBS)
        ctrl |= E1000_CTRL_SPD_1000;
    else if((phy_data & M88E1000_PSSR_SPEED) == M88E1000_PSSR_100MBS)
        ctrl |= E1000_CTRL_SPD_100;
    /* Write the configured values back to the Device Control Reg. */
    E1000_WRITE_REG(hw, CTRL, ctrl);
    return 0;
}

/******************************************************************************
 * Forces the MAC's flow control settings.
 * 
 * hw - Struct containing variables accessed by shared code
 *
 * Sets the TFCE and RFCE bits in the device control register to reflect
 * the adapter settings. TFCE and RFCE need to be explicitly set by
 * software when a Copper PHY is used because autonegotiation is managed
 * by the PHY rather than the MAC. Software must also configure these
 * bits when link is forced on a fiber connection.
 *****************************************************************************/
static int32_t
em_force_mac_fc(struct em_hw *hw)
{
    uint32_t ctrl;

    DEBUGFUNC("em_force_mac_fc");

    /* Get the current configuration of the Device Control Register */
    ctrl = E1000_READ_REG(hw, CTRL);

    /* Because we didn't get link via the internal auto-negotiation
     * mechanism (we either forced link or we got link via PHY
     * auto-neg), we have to manually enable/disable transmit an
     * receive flow control.
     *
     * The "Case" statement below enables/disable flow control
     * according to the "hw->fc" parameter.
     *
     * The possible values of the "fc" parameter are:
     *      0:  Flow control is completely disabled
     *      1:  Rx flow control is enabled (we can receive pause
     *          frames but not send pause frames).
     *      2:  Tx flow control is enabled (we can send pause frames
     *          frames but we do not receive pause frames).
     *      3:  Both Rx and TX flow control (symmetric) is enabled.
     *  other:  No other values should be possible at this point.
     */

    switch (hw->fc) {
    case em_fc_none:
        ctrl &= (~(E1000_CTRL_TFCE | E1000_CTRL_RFCE));
        break;
    case em_fc_rx_pause:
        ctrl &= (~E1000_CTRL_TFCE);
        ctrl |= E1000_CTRL_RFCE;
        break;
    case em_fc_tx_pause:
        ctrl &= (~E1000_CTRL_RFCE);
        ctrl |= E1000_CTRL_TFCE;
        break;
    case em_fc_full:
        ctrl |= (E1000_CTRL_TFCE | E1000_CTRL_RFCE);
        break;
    default:
        DEBUGOUT("Flow control param set incorrectly\n");
        return -E1000_ERR_CONFIG;
    }

    /* Disable TX Flow Control for 82542 (rev 2.0) */
    if(hw->mac_type == em_82542_rev2_0)
        ctrl &= (~E1000_CTRL_TFCE);

    E1000_WRITE_REG(hw, CTRL, ctrl);
    return 0;
}

/******************************************************************************
 * Configures flow control settings after link is established
 * 
 * hw - Struct containing variables accessed by shared code
 *
 * Should be called immediately after a valid link has been established.
 * Forces MAC flow control settings if link was forced. When in MII/GMII mode
 * and autonegotiation is enabled, the MAC flow control settings will be set
 * based on the flow control negotiated by the PHY. In TBI mode, the TFCE
 * and RFCE bits will be automaticaly set to the negotiated flow control mode.
 *****************************************************************************/
int32_t
em_config_fc_after_link_up(struct em_hw *hw)
{
    int32_t ret_val;
    uint16_t mii_status_reg;
    uint16_t mii_nway_adv_reg;
    uint16_t mii_nway_lp_ability_reg;
    uint16_t speed;
    uint16_t duplex;

    DEBUGFUNC("em_config_fc_after_link_up");

    /* Check for the case where we have fiber media and auto-neg failed
     * so we had to force link.  In this case, we need to force the
     * configuration of the MAC to match the "fc" parameter.
     */
    if(((hw->media_type == em_media_type_fiber) && (hw->autoneg_failed)) ||
       ((hw->media_type == em_media_type_copper) && (!hw->autoneg))) {
        ret_val = em_force_mac_fc(hw);
        if(ret_val < 0) {
            DEBUGOUT("Error forcing flow control settings\n");
            return ret_val;
        }
    }

    /* Check for the case where we have copper media and auto-neg is
     * enabled.  In this case, we need to check and see if Auto-Neg
     * has completed, and if so, how the PHY and link partner has
     * flow control configured.
     */
    if((hw->media_type == em_media_type_copper) && hw->autoneg) {
        /* Read the MII Status Register and check to see if AutoNeg
         * has completed.  We read this twice because this reg has
         * some "sticky" (latched) bits.
         */
        if(em_read_phy_reg(hw, PHY_STATUS, &mii_status_reg) < 0) {
            DEBUGOUT("PHY Read Error \n");
            return -E1000_ERR_PHY;
        }
        if(em_read_phy_reg(hw, PHY_STATUS, &mii_status_reg) < 0) {
            DEBUGOUT("PHY Read Error \n");
            return -E1000_ERR_PHY;
        }

        if(mii_status_reg & MII_SR_AUTONEG_COMPLETE) {
            /* The AutoNeg process has completed, so we now need to
             * read both the Auto Negotiation Advertisement Register
             * (Address 4) and the Auto_Negotiation Base Page Ability
             * Register (Address 5) to determine how flow control was
             * negotiated.
             */
            if(em_read_phy_reg(hw, PHY_AUTONEG_ADV, &mii_nway_adv_reg) < 0) {
                DEBUGOUT("PHY Read Error\n");
                return -E1000_ERR_PHY;
            }
            if(em_read_phy_reg(hw, PHY_LP_ABILITY, &mii_nway_lp_ability_reg) < 0) {
                DEBUGOUT("PHY Read Error\n");
                return -E1000_ERR_PHY;
            }

            /* Two bits in the Auto Negotiation Advertisement Register
             * (Address 4) and two bits in the Auto Negotiation Base
             * Page Ability Register (Address 5) determine flow control
             * for both the PHY and the link partner.  The following
             * table, taken out of the IEEE 802.3ab/D6.0 dated March 25,
             * 1999, describes these PAUSE resolution bits and how flow
             * control is determined based upon these settings.
             * NOTE:  DC = Don't Care
             *
             *   LOCAL DEVICE  |   LINK PARTNER
             * PAUSE | ASM_DIR | PAUSE | ASM_DIR | NIC Resolution
             *-------|---------|-------|---------|--------------------
             *   0   |    0    |  DC   |   DC    | em_fc_none
             *   0   |    1    |   0   |   DC    | em_fc_none
             *   0   |    1    |   1   |    0    | em_fc_none
             *   0   |    1    |   1   |    1    | em_fc_tx_pause
             *   1   |    0    |   0   |   DC    | em_fc_none
             *   1   |   DC    |   1   |   DC    | em_fc_full
             *   1   |    1    |   0   |    0    | em_fc_none
             *   1   |    1    |   0   |    1    | em_fc_rx_pause
             *
             */
            /* Are both PAUSE bits set to 1?  If so, this implies
             * Symmetric Flow Control is enabled at both ends.  The
             * ASM_DIR bits are irrelevant per the spec.
             *
             * For Symmetric Flow Control:
             *
             *   LOCAL DEVICE  |   LINK PARTNER
             * PAUSE | ASM_DIR | PAUSE | ASM_DIR | Result
             *-------|---------|-------|---------|--------------------
             *   1   |   DC    |   1   |   DC    | em_fc_full
             *
             */
            if((mii_nway_adv_reg & NWAY_AR_PAUSE) &&
               (mii_nway_lp_ability_reg & NWAY_LPAR_PAUSE)) {
                /* Now we need to check if the user selected RX ONLY
                 * of pause frames.  In this case, we had to advertise
                 * FULL flow control because we could not advertise RX
                 * ONLY. Hence, we must now check to see if we need to
                 * turn OFF  the TRANSMISSION of PAUSE frames.
                 */
                if(hw->original_fc == em_fc_full) {
                    hw->fc = em_fc_full;
                    DEBUGOUT("Flow Control = FULL.\r\n");
                } else {
                    hw->fc = em_fc_rx_pause;
                    DEBUGOUT("Flow Control = RX PAUSE frames only.\r\n");
                }
            }
            /* For receiving PAUSE frames ONLY.
             *
             *   LOCAL DEVICE  |   LINK PARTNER
             * PAUSE | ASM_DIR | PAUSE | ASM_DIR | Result
             *-------|---------|-------|---------|--------------------
             *   0   |    1    |   1   |    1    | em_fc_tx_pause
             *
             */
            else if(!(mii_nway_adv_reg & NWAY_AR_PAUSE) &&
                    (mii_nway_adv_reg & NWAY_AR_ASM_DIR) &&
                    (mii_nway_lp_ability_reg & NWAY_LPAR_PAUSE) &&
                    (mii_nway_lp_ability_reg & NWAY_LPAR_ASM_DIR)) {
                hw->fc = em_fc_tx_pause;
                DEBUGOUT("Flow Control = TX PAUSE frames only.\r\n");
            }
            /* For transmitting PAUSE frames ONLY.
             *
             *   LOCAL DEVICE  |   LINK PARTNER
             * PAUSE | ASM_DIR | PAUSE | ASM_DIR | Result
             *-------|---------|-------|---------|--------------------
             *   1   |    1    |   0   |    1    | em_fc_rx_pause
             *
             */
            else if((mii_nway_adv_reg & NWAY_AR_PAUSE) &&
                    (mii_nway_adv_reg & NWAY_AR_ASM_DIR) &&
                    !(mii_nway_lp_ability_reg & NWAY_LPAR_PAUSE) &&
                    (mii_nway_lp_ability_reg & NWAY_LPAR_ASM_DIR)) {
                hw->fc = em_fc_rx_pause;
                DEBUGOUT("Flow Control = RX PAUSE frames only.\r\n");
            }
            /* Per the IEEE spec, at this point flow control should be
             * disabled.  However, we want to consider that we could
             * be connected to a legacy switch that doesn't advertise
             * desired flow control, but can be forced on the link
             * partner.  So if we advertised no flow control, that is
             * what we will resolve to.  If we advertised some kind of
             * receive capability (Rx Pause Only or Full Flow Control)
             * and the link partner advertised none, we will configure
             * ourselves to enable Rx Flow Control only.  We can do
             * this safely for two reasons:  If the link partner really
             * didn't want flow control enabled, and we enable Rx, no
             * harm done since we won't be receiving any PAUSE frames
             * anyway.  If the intent on the link partner was to have
             * flow control enabled, then by us enabling RX only, we
             * can at least receive pause frames and process them.
             * This is a good idea because in most cases, since we are
             * predominantly a server NIC, more times than not we will
             * be asked to delay transmission of packets than asking
             * our link partner to pause transmission of frames.
             */
            else if(hw->original_fc == em_fc_none ||
                    hw->original_fc == em_fc_tx_pause) {
                hw->fc = em_fc_none;
                DEBUGOUT("Flow Control = NONE.\r\n");
            } else {
                hw->fc = em_fc_rx_pause;
                DEBUGOUT("Flow Control = RX PAUSE frames only.\r\n");
            }

            /* Now we need to do one last check...  If we auto-
             * negotiated to HALF DUPLEX, flow control should not be
             * enabled per IEEE 802.3 spec.
             */
            em_get_speed_and_duplex(hw, &speed, &duplex);

            if(duplex == HALF_DUPLEX)
                hw->fc = em_fc_none;

            /* Now we call a subroutine to actually force the MAC
             * controller to use the correct flow control settings.
             */
            ret_val = em_force_mac_fc(hw);
            if(ret_val < 0) {
                DEBUGOUT("Error forcing flow control settings\n");
                return ret_val;
             }
        } else {
            DEBUGOUT("Copper PHY and Auto Neg has not completed.\r\n");
        }
    }
    return 0;
}

/******************************************************************************
 * Checks to see if the link status of the hardware has changed.
 *
 * hw - Struct containing variables accessed by shared code
 *
 * Called by any function that needs to check the link status of the adapter.
 *****************************************************************************/
int32_t
em_check_for_link(struct em_hw *hw)
{
    uint32_t rxcw;
    uint32_t ctrl;
    uint32_t status;
    uint32_t rctl;
    uint32_t signal;
    int32_t ret_val;
    uint16_t phy_data;
    uint16_t lp_capability;

    DEBUGFUNC("em_check_for_link");
    
    /* On adapters with a MAC newer that 82544, SW Defineable pin 1 will be 
     * set when the optics detect a signal. On older adapters, it will be 
     * cleared when there is a signal
     */
    if(hw->mac_type > em_82544) signal = E1000_CTRL_SWDPIN1;
    else signal = 0;

    ctrl = E1000_READ_REG(hw, CTRL);
    status = E1000_READ_REG(hw, STATUS);
    rxcw = E1000_READ_REG(hw, RXCW);

    /* If we have a copper PHY then we only want to go out to the PHY
     * registers to see if Auto-Neg has completed and/or if our link
     * status has changed.  The get_link_status flag will be set if we
     * receive a Link Status Change interrupt or we have Rx Sequence
     * Errors.
     */
    if((hw->media_type == em_media_type_copper) && hw->get_link_status) {
        /* First we want to see if the MII Status Register reports
         * link.  If so, then we want to get the current speed/duplex
         * of the PHY.
         * Read the register twice since the link bit is sticky.
         */
        if(em_read_phy_reg(hw, PHY_STATUS, &phy_data) < 0) {
            DEBUGOUT("PHY Read Error\n");
            return -E1000_ERR_PHY;
        }
        if(em_read_phy_reg(hw, PHY_STATUS, &phy_data) < 0) {
            DEBUGOUT("PHY Read Error\n");
            return -E1000_ERR_PHY;
        }

        if(phy_data & MII_SR_LINK_STATUS) {
            hw->get_link_status = FALSE;
        } else {
            /* No link detected */
            return 0;
        }

        /* If we are forcing speed/duplex, then we simply return since
         * we have already determined whether we have link or not.
         */
        if(!hw->autoneg) return -E1000_ERR_CONFIG;

        /* We have a M88E1000 PHY and Auto-Neg is enabled.  If we
         * have Si on board that is 82544 or newer, Auto
         * Speed Detection takes care of MAC speed/duplex
         * configuration.  So we only need to configure Collision
         * Distance in the MAC.  Otherwise, we need to force
         * speed/duplex on the MAC to the current PHY speed/duplex
         * settings.
         */
        if(hw->mac_type >= em_82544)
            em_config_collision_dist(hw);
        else {
            ret_val = em_config_mac_to_phy(hw);
            if(ret_val < 0) {
                DEBUGOUT("Error configuring MAC to PHY settings\n");
                return ret_val;
            }
        }

        /* Configure Flow Control now that Auto-Neg has completed. First, we 
         * need to restore the desired flow control settings because we may
         * have had to re-autoneg with a different link partner.
         */
        ret_val = em_config_fc_after_link_up(hw);
        if(ret_val < 0) {
            DEBUGOUT("Error configuring flow control\n");
            return ret_val;
        }

        /* At this point we know that we are on copper and we have
         * auto-negotiated link.  These are conditions for checking the link
         * parter capability register.  We use the link partner capability to
         * determine if TBI Compatibility needs to be turned on or off.  If
         * the link partner advertises any speed in addition to Gigabit, then
         * we assume that they are GMII-based, and TBI compatibility is not
         * needed. If no other speeds are advertised, we assume the link
         * partner is TBI-based, and we turn on TBI Compatibility.
         */
        if(hw->tbi_compatibility_en) {
            if(em_read_phy_reg(hw, PHY_LP_ABILITY, &lp_capability) < 0) {
                DEBUGOUT("PHY Read Error\n");
                return -E1000_ERR_PHY;
            }
            if(lp_capability & (NWAY_LPAR_10T_HD_CAPS |
                                NWAY_LPAR_10T_FD_CAPS |
                                NWAY_LPAR_100TX_HD_CAPS |
                                NWAY_LPAR_100TX_FD_CAPS |
                                NWAY_LPAR_100T4_CAPS)) {
                /* If our link partner advertises anything in addition to 
                 * gigabit, we do not need to enable TBI compatibility.
                 */
                if(hw->tbi_compatibility_on) {
                    /* If we previously were in the mode, turn it off. */
                    rctl = E1000_READ_REG(hw, RCTL);
                    rctl &= ~E1000_RCTL_SBP;
                    E1000_WRITE_REG(hw, RCTL, rctl);
                    hw->tbi_compatibility_on = FALSE;
                }
            } else {
                /* If TBI compatibility is was previously off, turn it on. For
                 * compatibility with a TBI link partner, we will store bad
                 * packets. Some frames have an additional byte on the end and
                 * will look like CRC errors to to the hardware.
                 */
                if(!hw->tbi_compatibility_on) {
                    hw->tbi_compatibility_on = TRUE;
                    rctl = E1000_READ_REG(hw, RCTL);
                    rctl |= E1000_RCTL_SBP;
                    E1000_WRITE_REG(hw, RCTL, rctl);
                }
            }
        }
    }
    /* If we don't have link (auto-negotiation failed or link partner cannot
     * auto-negotiate), the cable is plugged in (we have signal), and our
     * link partner is not trying to auto-negotiate with us (we are receiving
     * idles or data), we need to force link up. We also need to give
     * auto-negotiation time to complete, in case the cable was just plugged
     * in. The autoneg_failed flag does this.
     */
    else if((hw->media_type == em_media_type_fiber) &&
            (!(status & E1000_STATUS_LU)) &&
            ((ctrl & E1000_CTRL_SWDPIN1) == signal) &&
            (!(rxcw & E1000_RXCW_C))) {
        if(hw->autoneg_failed == 0) {
            hw->autoneg_failed = 1;
            return 0;
        }
        DEBUGOUT("NOT RXing /C/, disable AutoNeg and force link.\r\n");

        /* Disable auto-negotiation in the TXCW register */
        E1000_WRITE_REG(hw, TXCW, (hw->txcw & ~E1000_TXCW_ANE));

        /* Force link-up and also force full-duplex. */
        ctrl = E1000_READ_REG(hw, CTRL);
        ctrl |= (E1000_CTRL_SLU | E1000_CTRL_FD);
        E1000_WRITE_REG(hw, CTRL, ctrl);

        /* Configure Flow Control after forcing link up. */
        ret_val = em_config_fc_after_link_up(hw);
        if(ret_val < 0) {
            DEBUGOUT("Error configuring flow control\n");
            return ret_val;
        }
    }
    /* If we are forcing link and we are receiving /C/ ordered sets, re-enable
     * auto-negotiation in the TXCW register and disable forced link in the
     * Device Control register in an attempt to auto-negotiate with our link
     * partner.
     */
    else if((hw->media_type == em_media_type_fiber) &&
              (ctrl & E1000_CTRL_SLU) &&
              (rxcw & E1000_RXCW_C)) {
        DEBUGOUT("RXing /C/, enable AutoNeg and stop forcing link.\r\n");
        E1000_WRITE_REG(hw, TXCW, hw->txcw);
        E1000_WRITE_REG(hw, CTRL, (ctrl & ~E1000_CTRL_SLU));
    }
    return 0;
}

/******************************************************************************
 * Detects the current speed and duplex settings of the hardware.
 *
 * hw - Struct containing variables accessed by shared code
 * speed - Speed of the connection
 * duplex - Duplex setting of the connection
 *****************************************************************************/
void
em_get_speed_and_duplex(struct em_hw *hw,
                           uint16_t *speed,
                           uint16_t *duplex)
{
    uint32_t status;

    DEBUGFUNC("em_get_speed_and_duplex");

    if(hw->mac_type >= em_82543) {
        status = E1000_READ_REG(hw, STATUS);
        if(status & E1000_STATUS_SPEED_1000) {
            *speed = SPEED_1000;
            DEBUGOUT("1000 Mbs, ");
        } else if(status & E1000_STATUS_SPEED_100) {
            *speed = SPEED_100;
            DEBUGOUT("100 Mbs, ");
        } else {
            *speed = SPEED_10;
            DEBUGOUT("10 Mbs, ");
        }

        if(status & E1000_STATUS_FD) {
            *duplex = FULL_DUPLEX;
            DEBUGOUT("Full Duplex\r\n");
        } else {
            *duplex = HALF_DUPLEX;
            DEBUGOUT(" Half Duplex\r\n");
        }
    } else {
        DEBUGOUT("1000 Mbs, Full Duplex\r\n");
        *speed = SPEED_1000;
        *duplex = FULL_DUPLEX;
    }
}

/******************************************************************************
* Blocks until autoneg completes or times out (~4.5 seconds)
*
* hw - Struct containing variables accessed by shared code
******************************************************************************/
int32_t
em_wait_autoneg(struct em_hw *hw)
{
    uint16_t i;
    uint16_t phy_data;

    DEBUGFUNC("em_wait_autoneg");
    DEBUGOUT("Waiting for Auto-Neg to complete.\n");

    /* We will wait for autoneg to complete or 4.5 seconds to expire. */
    for(i = PHY_AUTO_NEG_TIME; i > 0; i--) {
        /* Read the MII Status Register and wait for Auto-Neg
         * Complete bit to be set.
         */
        if(em_read_phy_reg(hw, PHY_STATUS, &phy_data) < 0) {
            DEBUGOUT("PHY Read Error\n");
            return -E1000_ERR_PHY;
        }
        if(em_read_phy_reg(hw, PHY_STATUS, &phy_data) < 0) {
            DEBUGOUT("PHY Read Error\n");
            return -E1000_ERR_PHY;
        }
        if(phy_data & MII_SR_AUTONEG_COMPLETE) {
            return 0;
        }
        msec_delay(100);
    }
    return 0;
}

/******************************************************************************
* Raises the Management Data Clock
*
* hw - Struct containing variables accessed by shared code
* ctrl - Device control register's current value
******************************************************************************/
static void
em_raise_mdi_clk(struct em_hw *hw,
                    uint32_t *ctrl)
{
    /* Raise the clock input to the Management Data Clock (by setting the MDC
     * bit), and then delay 2 microseconds.
     */
    E1000_WRITE_REG(hw, CTRL, (*ctrl | E1000_CTRL_MDC));
    usec_delay(2);
}

/******************************************************************************
* Lowers the Management Data Clock
*
* hw - Struct containing variables accessed by shared code
* ctrl - Device control register's current value
******************************************************************************/
static void
em_lower_mdi_clk(struct em_hw *hw,
                    uint32_t *ctrl)
{
    /* Lower the clock input to the Management Data Clock (by clearing the MDC
     * bit), and then delay 2 microseconds.
     */
    E1000_WRITE_REG(hw, CTRL, (*ctrl & ~E1000_CTRL_MDC));
    usec_delay(2);
}

/******************************************************************************
* Shifts data bits out to the PHY
*
* hw - Struct containing variables accessed by shared code
* data - Data to send out to the PHY
* count - Number of bits to shift out
*
* Bits are shifted out in MSB to LSB order.
******************************************************************************/
static void
em_shift_out_mdi_bits(struct em_hw *hw,
                         uint32_t data,
                         uint16_t count)
{
    uint32_t ctrl;
    uint32_t mask;

    /* We need to shift "count" number of bits out to the PHY. So, the value
     * in the "data" parameter will be shifted out to the PHY one bit at a 
     * time. In order to do this, "data" must be broken down into bits.
     */
    mask = 0x01;
    mask <<= (count - 1);

    ctrl = E1000_READ_REG(hw, CTRL);

    /* Set MDIO_DIR and MDC_DIR direction bits to be used as output pins. */
    ctrl |= (E1000_CTRL_MDIO_DIR | E1000_CTRL_MDC_DIR);

    while(mask) {
        /* A "1" is shifted out to the PHY by setting the MDIO bit to "1" and
         * then raising and lowering the Management Data Clock. A "0" is
         * shifted out to the PHY by setting the MDIO bit to "0" and then
         * raising and lowering the clock.
         */
        if(data & mask) ctrl |= E1000_CTRL_MDIO;
        else ctrl &= ~E1000_CTRL_MDIO;

        E1000_WRITE_REG(hw, CTRL, ctrl);

        usec_delay(2);

        em_raise_mdi_clk(hw, &ctrl);
        em_lower_mdi_clk(hw, &ctrl);

        mask = mask >> 1;
    }

    /* Clear the data bit just before leaving this routine. */
    ctrl &= ~E1000_CTRL_MDIO;
}

/******************************************************************************
* Shifts data bits in from the PHY
*
* hw - Struct containing variables accessed by shared code
*
* Bits are shifted in in MSB to LSB order. 
******************************************************************************/
static uint16_t
em_shift_in_mdi_bits(struct em_hw *hw)
{
    uint32_t ctrl;
    uint16_t data = 0;
    uint8_t i;

    /* In order to read a register from the PHY, we need to shift in a total
     * of 18 bits from the PHY. The first two bit (turnaround) times are used
     * to avoid contention on the MDIO pin when a read operation is performed.
     * These two bits are ignored by us and thrown away. Bits are "shifted in"
     * by raising the input to the Management Data Clock (setting the MDC bit),
     * and then reading the value of the MDIO bit.
     */ 
    ctrl = E1000_READ_REG(hw, CTRL);

    /* Clear MDIO_DIR (SWDPIO1) to indicate this bit is to be used as input. */
    ctrl &= ~E1000_CTRL_MDIO_DIR;
    ctrl &= ~E1000_CTRL_MDIO;

    E1000_WRITE_REG(hw, CTRL, ctrl);

    /* Raise and Lower the clock before reading in the data. This accounts for
     * the turnaround bits. The first clock occurred when we clocked out the
     * last bit of the Register Address.
     */
    em_raise_mdi_clk(hw, &ctrl);
    em_lower_mdi_clk(hw, &ctrl);

    for(data = 0, i = 0; i < 16; i++) {
        data = data << 1;
        em_raise_mdi_clk(hw, &ctrl);
        ctrl = E1000_READ_REG(hw, CTRL);
        /* Check to see if we shifted in a "1". */
        if(ctrl & E1000_CTRL_MDIO) data |= 1;
        em_lower_mdi_clk(hw, &ctrl);
    }

    em_raise_mdi_clk(hw, &ctrl);
    em_lower_mdi_clk(hw, &ctrl);

    /* Clear the MDIO bit just before leaving this routine. */
    ctrl &= ~E1000_CTRL_MDIO;

    return data;
}

/*****************************************************************************
* Reads the value from a PHY register
*
* hw - Struct containing variables accessed by shared code
* reg_addr - address of the PHY register to read
******************************************************************************/
int32_t
em_read_phy_reg(struct em_hw *hw,
                   uint32_t reg_addr,
                   uint16_t *phy_data)
{
    uint32_t i;
    uint32_t mdic = 0;
    const uint32_t phy_addr = 1;

    DEBUGFUNC("em_read_phy_reg");

    if(reg_addr > MAX_PHY_REG_ADDRESS) {
        DEBUGOUT1("PHY Address %d is out of range\n", reg_addr);
        return -E1000_ERR_PARAM;
    }

    if(hw->mac_type > em_82543) {
        /* Set up Op-code, Phy Address, and register address in the MDI
         * Control register.  The MAC will take care of interfacing with the
         * PHY to retrieve the desired data.
         */
        mdic = ((reg_addr << E1000_MDIC_REG_SHIFT) |
                (phy_addr << E1000_MDIC_PHY_SHIFT) | 
                (E1000_MDIC_OP_READ));

        E1000_WRITE_REG(hw, MDIC, mdic);

        /* Poll the ready bit to see if the MDI read completed */
        for(i = 0; i < 64; i++) {
            usec_delay(10);
            mdic = E1000_READ_REG(hw, MDIC);
            if(mdic & E1000_MDIC_READY) break;
        }
        if(!(mdic & E1000_MDIC_READY)) {
            DEBUGOUT("MDI Read did not complete\n");
            return -E1000_ERR_PHY;
        }
        if(mdic & E1000_MDIC_ERROR) {
            DEBUGOUT("MDI Error\n");
            return -E1000_ERR_PHY;
        }
        *phy_data = (uint16_t) mdic;
    } else {
        /* We must first send a preamble through the MDIO pin to signal the
         * beginning of an MII instruction.  This is done by sending 32
         * consecutive "1" bits.
         */
        em_shift_out_mdi_bits(hw, PHY_PREAMBLE, PHY_PREAMBLE_SIZE);

        /* Now combine the next few fields that are required for a read
         * operation.  We use this method instead of calling the
         * em_shift_out_mdi_bits routine five different times. The format of
         * a MII read instruction consists of a shift out of 14 bits and is
         * defined as follows:
         *    <Preamble><SOF><Op Code><Phy Addr><Reg Addr>
         * followed by a shift in of 18 bits.  This first two bits shifted in
         * are TurnAround bits used to avoid contention on the MDIO pin when a
         * READ operation is performed.  These two bits are thrown away
         * followed by a shift in of 16 bits which contains the desired data.
         */
        mdic = ((reg_addr) | (phy_addr << 5) | 
                (PHY_OP_READ << 10) | (PHY_SOF << 12));

        em_shift_out_mdi_bits(hw, mdic, 14);

        /* Now that we've shifted out the read command to the MII, we need to
         * "shift in" the 16-bit value (18 total bits) of the requested PHY
         * register address.
         */
        *phy_data = em_shift_in_mdi_bits(hw);
    }
    return 0;
}

/******************************************************************************
* Writes a value to a PHY register
*
* hw - Struct containing variables accessed by shared code
* reg_addr - address of the PHY register to write
* data - data to write to the PHY
******************************************************************************/
int32_t
em_write_phy_reg(struct em_hw *hw,
                    uint32_t reg_addr,
                    uint16_t phy_data)
{
    uint32_t i;
    uint32_t mdic = 0;
    const uint32_t phy_addr = 1;

    DEBUGFUNC("em_write_phy_reg");

    if(reg_addr > MAX_PHY_REG_ADDRESS) {
        DEBUGOUT1("PHY Address %d is out of range\n", reg_addr);
        return -E1000_ERR_PARAM;
    }

    if(hw->mac_type > em_82543) {
        /* Set up Op-code, Phy Address, register address, and data intended
         * for the PHY register in the MDI Control register.  The MAC will take
         * care of interfacing with the PHY to send the desired data.
         */
        mdic = (((uint32_t) phy_data) |
                (reg_addr << E1000_MDIC_REG_SHIFT) |
                (phy_addr << E1000_MDIC_PHY_SHIFT) | 
                (E1000_MDIC_OP_WRITE));

        E1000_WRITE_REG(hw, MDIC, mdic);

        /* Poll the ready bit to see if the MDI read completed */
        for(i = 0; i < 64; i++) {
            usec_delay(10);
            mdic = E1000_READ_REG(hw, MDIC);
            if(mdic & E1000_MDIC_READY) break;
        }
        if(!(mdic & E1000_MDIC_READY)) {
            DEBUGOUT("MDI Write did not complete\n");
            return -E1000_ERR_PHY;
        }
    } else {
        /* We'll need to use the SW defined pins to shift the write command
         * out to the PHY. We first send a preamble to the PHY to signal the
         * beginning of the MII instruction.  This is done by sending 32 
         * consecutive "1" bits.
         */
        em_shift_out_mdi_bits(hw, PHY_PREAMBLE, PHY_PREAMBLE_SIZE);

        /* Now combine the remaining required fields that will indicate a 
         * write operation. We use this method instead of calling the
         * em_shift_out_mdi_bits routine for each field in the command. The
         * format of a MII write instruction is as follows:
         * <Preamble><SOF><Op Code><Phy Addr><Reg Addr><Turnaround><Data>.
         */
        mdic = ((PHY_TURNAROUND) | (reg_addr << 2) | (phy_addr << 7) |
                (PHY_OP_WRITE << 12) | (PHY_SOF << 14));
        mdic <<= 16;
        mdic |= (uint32_t) phy_data;

        em_shift_out_mdi_bits(hw, mdic, 32);
    }
    return 0;
}

/******************************************************************************
* Returns the PHY to the power-on reset state
*
* hw - Struct containing variables accessed by shared code
******************************************************************************/
void
em_phy_hw_reset(struct em_hw *hw)
{
    uint32_t ctrl;
    uint32_t ctrl_ext;

    DEBUGFUNC("em_phy_hw_reset");

    DEBUGOUT("Resetting Phy...\n");

    if(hw->mac_type > em_82543) {
        /* Read the device control register and assert the E1000_CTRL_PHY_RST
         * bit. Then, take it out of reset.
         */
        ctrl = E1000_READ_REG(hw, CTRL);
        E1000_WRITE_REG(hw, CTRL, ctrl | E1000_CTRL_PHY_RST);
        msec_delay(10);
        E1000_WRITE_REG(hw, CTRL, ctrl);
    } else {
        /* Read the Extended Device Control Register, assert the PHY_RESET_DIR
         * bit to put the PHY into reset. Then, take it out of reset.
         */
        ctrl_ext = E1000_READ_REG(hw, CTRL_EXT);
        ctrl_ext |= E1000_CTRL_EXT_SDP4_DIR;
        ctrl_ext &= ~E1000_CTRL_EXT_SDP4_DATA;
        E1000_WRITE_REG(hw, CTRL_EXT, ctrl_ext);
        msec_delay(10);
        ctrl_ext |= E1000_CTRL_EXT_SDP4_DATA;
        E1000_WRITE_REG(hw, CTRL_EXT, ctrl_ext);
    }
    usec_delay(150);
}

/******************************************************************************
* Resets the PHY
*
* hw - Struct containing variables accessed by shared code
*
* Sets bit 15 of the MII Control regiser
******************************************************************************/
int32_t
em_phy_reset(struct em_hw *hw)
{
    uint16_t phy_data;

    DEBUGFUNC("em_phy_reset");

    if(em_read_phy_reg(hw, PHY_CTRL, &phy_data) < 0) {
        DEBUGOUT("PHY Read Error\n");
        return -E1000_ERR_PHY;
    }
    phy_data |= MII_CR_RESET;
    if(em_write_phy_reg(hw, PHY_CTRL, phy_data) < 0) {
        DEBUGOUT("PHY Write Error\n");
        return -E1000_ERR_PHY;
    }
    usec_delay(1);
    return 0;
}

/******************************************************************************
* Probes the expected PHY address for known PHY IDs
*
* hw - Struct containing variables accessed by shared code
******************************************************************************/
int32_t
em_detect_gig_phy(struct em_hw *hw)
{
    uint16_t phy_id_high, phy_id_low;
    boolean_t match = FALSE;

    DEBUGFUNC("em_detect_gig_phy");

    /* Read the PHY ID Registers to identify which PHY is onboard. */
    if(em_read_phy_reg(hw, PHY_ID1, &phy_id_high) < 0) {
        DEBUGOUT("PHY Read Error\n");
        return -E1000_ERR_PHY;
    }
    hw->phy_id = (uint32_t) (phy_id_high << 16);
    usec_delay(2);
    if(em_read_phy_reg(hw, PHY_ID2, &phy_id_low) < 0) {
        DEBUGOUT("PHY Read Error\n");
        return -E1000_ERR_PHY;
    }
    hw->phy_id |= (uint32_t) (phy_id_low & PHY_REVISION_MASK);
    
    switch(hw->mac_type) {
    case em_82543:
        if(hw->phy_id == M88E1000_E_PHY_ID) match = TRUE;
        break;
    case em_82544:
        if(hw->phy_id == M88E1000_I_PHY_ID) match = TRUE;
        break;
    case em_82540:
    case em_82545:
    case em_82546:
        if(hw->phy_id == M88E1011_I_PHY_ID) match = TRUE;
        break;
    default:
        DEBUGOUT1("Invalid MAC type %d\n", hw->mac_type);
        return -E1000_ERR_CONFIG;
    }
    if(match) {
        DEBUGOUT1("PHY ID 0x%X detected\n", hw->phy_id);
        return 0;
    }
    DEBUGOUT1("Invalid PHY ID 0x%X\n", hw->phy_id);
    return -E1000_ERR_PHY;
}

/******************************************************************************
* Resets the PHY's DSP
*
* hw - Struct containing variables accessed by shared code
******************************************************************************/
static int32_t
em_phy_reset_dsp(struct em_hw *hw)
{
    int32_t ret_val = -E1000_ERR_PHY;
    DEBUGFUNC("em_phy_reset_dsp");
    
    do {
        if(em_write_phy_reg(hw, 29, 0x001d) < 0) break;
        if(em_write_phy_reg(hw, 30, 0x00c1) < 0) break;
        if(em_write_phy_reg(hw, 30, 0x0000) < 0) break;
        ret_val = 0;
    } while(0);

    if(ret_val < 0) DEBUGOUT("PHY Write Error\n");
    return ret_val;
}

/******************************************************************************
* Get PHY information from various PHY registers
*
* hw - Struct containing variables accessed by shared code
* phy_info - PHY information structure
******************************************************************************/
int32_t
em_phy_get_info(struct em_hw *hw,
                   struct em_phy_info *phy_info)
{
    int32_t ret_val = -E1000_ERR_PHY;
    uint16_t phy_data;

    DEBUGFUNC("em_phy_get_info");

    phy_info->cable_length = em_cable_length_undefined;
    phy_info->extended_10bt_distance = em_10bt_ext_dist_enable_undefined;
    phy_info->cable_polarity = em_rev_polarity_undefined;
    phy_info->polarity_correction = em_polarity_reversal_undefined;
    phy_info->mdix_mode = em_auto_x_mode_undefined;
    phy_info->local_rx = em_1000t_rx_status_undefined;
    phy_info->remote_rx = em_1000t_rx_status_undefined;

    if(hw->media_type != em_media_type_copper) {
        DEBUGOUT("PHY info is only valid for copper media\n");
        return -E1000_ERR_CONFIG;
    }

    do {
        if(em_read_phy_reg(hw, PHY_STATUS, &phy_data) < 0) break;
        if(em_read_phy_reg(hw, PHY_STATUS, &phy_data) < 0) break;
        if((phy_data & MII_SR_LINK_STATUS) != MII_SR_LINK_STATUS) {
            DEBUGOUT("PHY info is only valid if link is up\n");
            return -E1000_ERR_CONFIG;
        }

        if(em_read_phy_reg(hw, M88E1000_PHY_SPEC_CTRL, &phy_data) < 0)
            break;
        phy_info->extended_10bt_distance =
            (phy_data & M88E1000_PSCR_10BT_EXT_DIST_ENABLE) >>
            M88E1000_PSCR_10BT_EXT_DIST_ENABLE_SHIFT;
        phy_info->polarity_correction =
            (phy_data & M88E1000_PSCR_POLARITY_REVERSAL) >>
            M88E1000_PSCR_POLARITY_REVERSAL_SHIFT;

        if(em_read_phy_reg(hw, M88E1000_PHY_SPEC_STATUS, &phy_data) < 0)
            break;
        phy_info->cable_polarity = (phy_data & M88E1000_PSSR_REV_POLARITY) >>
            M88E1000_PSSR_REV_POLARITY_SHIFT;
        phy_info->mdix_mode = (phy_data & M88E1000_PSSR_MDIX) >>
            M88E1000_PSSR_MDIX_SHIFT;
        if(phy_data & M88E1000_PSSR_1000MBS) {
            /* Cable Length Estimation and Local/Remote Receiver Informatoion
             * are only valid at 1000 Mbps
             */
            phy_info->cable_length = ((phy_data & M88E1000_PSSR_CABLE_LENGTH) >>
                                      M88E1000_PSSR_CABLE_LENGTH_SHIFT);
            if(em_read_phy_reg(hw, PHY_1000T_STATUS, &phy_data) < 0) 
                break;
            phy_info->local_rx = (phy_data & SR_1000T_LOCAL_RX_STATUS) >>
                SR_1000T_LOCAL_RX_STATUS_SHIFT;
            phy_info->remote_rx = (phy_data & SR_1000T_REMOTE_RX_STATUS) >>
                SR_1000T_REMOTE_RX_STATUS_SHIFT;
        }
        ret_val = 0;
    } while(0);

    if(ret_val < 0) DEBUGOUT("PHY Read Error\n");
    return ret_val;
}

int32_t
em_validate_mdi_setting(struct em_hw *hw)
{
    DEBUGFUNC("em_validate_mdi_settings");

    if(!hw->autoneg && (hw->mdix == 0 || hw->mdix == 3)) {
        DEBUGOUT("Invalid MDI setting detected\n");
        hw->mdix = 1;
        return -E1000_ERR_CONFIG;
    }
    return 0;
}

/******************************************************************************
 * Raises the EEPROM's clock input.
 *
 * hw - Struct containing variables accessed by shared code
 * eecd - EECD's current value
 *****************************************************************************/
static void
em_raise_ee_clk(struct em_hw *hw,
                   uint32_t *eecd)
{
    /* Raise the clock input to the EEPROM (by setting the SK bit), and then
     * wait 50 microseconds.
     */
    *eecd = *eecd | E1000_EECD_SK;
    E1000_WRITE_REG(hw, EECD, *eecd);
    usec_delay(50);
}

/******************************************************************************
 * Lowers the EEPROM's clock input.
 *
 * hw - Struct containing variables accessed by shared code 
 * eecd - EECD's current value
 *****************************************************************************/
static void
em_lower_ee_clk(struct em_hw *hw,
                   uint32_t *eecd)
{
    /* Lower the clock input to the EEPROM (by clearing the SK bit), and then 
     * wait 50 microseconds. 
     */
    *eecd = *eecd & ~E1000_EECD_SK;
    E1000_WRITE_REG(hw, EECD, *eecd);
    usec_delay(50);
}

/******************************************************************************
 * Shift data bits out to the EEPROM.
 *
 * hw - Struct containing variables accessed by shared code
 * data - data to send to the EEPROM
 * count - number of bits to shift out
 *****************************************************************************/
static void
em_shift_out_ee_bits(struct em_hw *hw,
                        uint16_t data,
                        uint16_t count)
{
    uint32_t eecd;
    uint32_t mask;

    /* We need to shift "count" bits out to the EEPROM. So, value in the
     * "data" parameter will be shifted out to the EEPROM one bit at a time.
     * In order to do this, "data" must be broken down into bits. 
     */
    mask = 0x01 << (count - 1);
    eecd = E1000_READ_REG(hw, EECD);
    eecd &= ~(E1000_EECD_DO | E1000_EECD_DI);
    do {
        /* A "1" is shifted out to the EEPROM by setting bit "DI" to a "1",
         * and then raising and then lowering the clock (the SK bit controls
         * the clock input to the EEPROM).  A "0" is shifted out to the EEPROM
         * by setting "DI" to "0" and then raising and then lowering the clock.
         */
        eecd &= ~E1000_EECD_DI;

        if(data & mask)
            eecd |= E1000_EECD_DI;

        E1000_WRITE_REG(hw, EECD, eecd);

        usec_delay(50);

        em_raise_ee_clk(hw, &eecd);
        em_lower_ee_clk(hw, &eecd);

        mask = mask >> 1;

    } while(mask);

    /* We leave the "DI" bit set to "0" when we leave this routine. */
    eecd &= ~E1000_EECD_DI;
    E1000_WRITE_REG(hw, EECD, eecd);
}

/******************************************************************************
 * Shift data bits in from the EEPROM
 *
 * hw - Struct containing variables accessed by shared code
 *****************************************************************************/
static uint16_t
em_shift_in_ee_bits(struct em_hw *hw)
{
    uint32_t eecd;
    uint32_t i;
    uint16_t data;

    /* In order to read a register from the EEPROM, we need to shift 16 bits 
     * in from the EEPROM. Bits are "shifted in" by raising the clock input to
     * the EEPROM (setting the SK bit), and then reading the value of the "DO"
     * bit.  During this "shifting in" process the "DI" bit should always be 
     * clear..
     */

    eecd = E1000_READ_REG(hw, EECD);

    eecd &= ~(E1000_EECD_DO | E1000_EECD_DI);
    data = 0;

    for(i = 0; i < 16; i++) {
        data = data << 1;
        em_raise_ee_clk(hw, &eecd);

        eecd = E1000_READ_REG(hw, EECD);

        eecd &= ~(E1000_EECD_DI);
        if(eecd & E1000_EECD_DO)
            data |= 1;

        em_lower_ee_clk(hw, &eecd);
    }

    return data;
}

/******************************************************************************
 * Prepares EEPROM for access
 *
 * hw - Struct containing variables accessed by shared code
 *
 * Lowers EEPROM clock. Clears input pin. Sets the chip select pin. This 
 * function should be called before issuing a command to the EEPROM.
 *****************************************************************************/
static void
em_setup_eeprom(struct em_hw *hw)
{
    uint32_t eecd;

    eecd = E1000_READ_REG(hw, EECD);

    /* Clear SK and DI */
    eecd &= ~(E1000_EECD_SK | E1000_EECD_DI);
    E1000_WRITE_REG(hw, EECD, eecd);

    /* Set CS */
    eecd |= E1000_EECD_CS;
    E1000_WRITE_REG(hw, EECD, eecd);
}

/******************************************************************************
 * Returns EEPROM to a "standby" state
 * 
 * hw - Struct containing variables accessed by shared code
 *****************************************************************************/
static void
em_standby_eeprom(struct em_hw *hw)
{
    uint32_t eecd;

    eecd = E1000_READ_REG(hw, EECD);

    /* Deselct EEPROM */
    eecd &= ~(E1000_EECD_CS | E1000_EECD_SK);
    E1000_WRITE_REG(hw, EECD, eecd);
    usec_delay(50);

    /* Clock high */
    eecd |= E1000_EECD_SK;
    E1000_WRITE_REG(hw, EECD, eecd);
    usec_delay(50);

    /* Select EEPROM */
    eecd |= E1000_EECD_CS;
    E1000_WRITE_REG(hw, EECD, eecd);
    usec_delay(50);

    /* Clock low */
    eecd &= ~E1000_EECD_SK;
    E1000_WRITE_REG(hw, EECD, eecd);
    usec_delay(50);
}

/******************************************************************************
 * Raises then lowers the EEPROM's clock pin
 *
 * hw - Struct containing variables accessed by shared code
 *****************************************************************************/
static void
em_clock_eeprom(struct em_hw *hw)
{
    uint32_t eecd;

    eecd = E1000_READ_REG(hw, EECD);

    /* Rising edge of clock */
    eecd |= E1000_EECD_SK;
    E1000_WRITE_REG(hw, EECD, eecd);
    usec_delay(50);

    /* Falling edge of clock */
    eecd &= ~E1000_EECD_SK;
    E1000_WRITE_REG(hw, EECD, eecd);
    usec_delay(50);
}

/******************************************************************************
 * Terminates a command by lowering the EEPROM's chip select pin
 *
 * hw - Struct containing variables accessed by shared code
 *****************************************************************************/
static void
em_cleanup_eeprom(struct em_hw *hw)
{
    uint32_t eecd;

    eecd = E1000_READ_REG(hw, EECD);

    eecd &= ~(E1000_EECD_CS | E1000_EECD_DI);

    E1000_WRITE_REG(hw, EECD, eecd);

    em_clock_eeprom(hw);
}

/******************************************************************************
 * Reads a 16 bit word from the EEPROM.
 *
 * hw - Struct containing variables accessed by shared code
 * offset - offset of  word in the EEPROM to read
 * data - word read from the EEPROM 
 *****************************************************************************/
int32_t
em_read_eeprom(struct em_hw *hw,
                  uint16_t offset,
                  uint16_t *data)
{
    uint32_t eecd;
    uint32_t i = 0;
    boolean_t large_eeprom = FALSE;

    DEBUGFUNC("em_read_eeprom");

    /* Request EEPROM Access */
    if(hw->mac_type > em_82544) {
        eecd = E1000_READ_REG(hw, EECD);
        if(eecd & E1000_EECD_SIZE) large_eeprom = TRUE;
        eecd |= E1000_EECD_REQ;
        E1000_WRITE_REG(hw, EECD, eecd);
        eecd = E1000_READ_REG(hw, EECD);
        while((!(eecd & E1000_EECD_GNT)) && (i < 100)) {
            i++;
            usec_delay(5);
            eecd = E1000_READ_REG(hw, EECD);
        }
        if(!(eecd & E1000_EECD_GNT)) {
            eecd &= ~E1000_EECD_REQ;
            E1000_WRITE_REG(hw, EECD, eecd);
            DEBUGOUT("Could not acquire EEPROM grant\n");
            return -E1000_ERR_EEPROM;
        }
    }

    /*  Prepare the EEPROM for reading  */
    em_setup_eeprom(hw);

    /*  Send the READ command (opcode + addr)  */
    em_shift_out_ee_bits(hw, EEPROM_READ_OPCODE, 3);
    if(large_eeprom) {
        /* If we have a 256 word EEPROM, there are 8 address bits */
        em_shift_out_ee_bits(hw, offset, 8);
    } else {
        /* If we have a 64 word EEPROM, there are 6 address bits */
        em_shift_out_ee_bits(hw, offset, 6);
    }

    /* Read the data */
    *data = em_shift_in_ee_bits(hw);

    /* End this read operation */
    em_standby_eeprom(hw);

    /* Stop requesting EEPROM access */
    if(hw->mac_type > em_82544) {
        eecd = E1000_READ_REG(hw, EECD);
        eecd &= ~E1000_EECD_REQ;
        E1000_WRITE_REG(hw, EECD, eecd);
    }

    return 0;
}

/******************************************************************************
 * Verifies that the EEPROM has a valid checksum
 * 
 * hw - Struct containing variables accessed by shared code
 *
 * Reads the first 64 16 bit words of the EEPROM and sums the values read.
 * If the the sum of the 64 16 bit words is 0xBABA, the EEPROM's checksum is
 * valid.
 *****************************************************************************/
int32_t
em_validate_eeprom_checksum(struct em_hw *hw)
{
    uint16_t checksum = 0;
    uint16_t i, eeprom_data;

    DEBUGFUNC("em_validate_eeprom_checksum");

    for(i = 0; i < (EEPROM_CHECKSUM_REG + 1); i++) {
        if(em_read_eeprom(hw, i, &eeprom_data) < 0) {
            DEBUGOUT("EEPROM Read Error\n");
            return -E1000_ERR_EEPROM;
        }
        checksum += eeprom_data;
    }

    if(checksum == (uint16_t) EEPROM_SUM) {
        return 0;
    } else {
        DEBUGOUT("EEPROM Checksum Invalid\n");    
        return -E1000_ERR_EEPROM;
    }
}

/******************************************************************************
 * Calculates the EEPROM checksum and writes it to the EEPROM
 *
 * hw - Struct containing variables accessed by shared code
 *
 * Sums the first 63 16 bit words of the EEPROM. Subtracts the sum from 0xBABA.
 * Writes the difference to word offset 63 of the EEPROM.
 *****************************************************************************/
int32_t
em_update_eeprom_checksum(struct em_hw *hw)
{
    uint16_t checksum = 0;
    uint16_t i, eeprom_data;

    DEBUGFUNC("em_update_eeprom_checksum");

    for(i = 0; i < EEPROM_CHECKSUM_REG; i++) {
        if(em_read_eeprom(hw, i, &eeprom_data) < 0) {
            DEBUGOUT("EEPROM Read Error\n");
            return -E1000_ERR_EEPROM;
        }
        checksum += eeprom_data;
    }
    checksum = (uint16_t) EEPROM_SUM - checksum;
    if(em_write_eeprom(hw, EEPROM_CHECKSUM_REG, checksum) < 0) {
        DEBUGOUT("EEPROM Write Error\n");
        return -E1000_ERR_EEPROM;
    }
    return 0;
}

/******************************************************************************
 * Writes a 16 bit word to a given offset in the EEPROM.
 *
 * hw - Struct containing variables accessed by shared code
 * offset - offset within the EEPROM to be written to
 * data - 16 bit word to be writen to the EEPROM
 *
 * If em_update_eeprom_checksum is not called after this function, the 
 * EEPROM will most likely contain an invalid checksum.
 *****************************************************************************/
int32_t
em_write_eeprom(struct em_hw *hw,
                   uint16_t offset,
                   uint16_t data)
{
    uint32_t eecd;
    uint32_t i = 0;
    int32_t status = 0;
    boolean_t large_eeprom = FALSE;

    DEBUGFUNC("em_write_eeprom");

    /* Request EEPROM Access */
    if(hw->mac_type > em_82544) {
        eecd = E1000_READ_REG(hw, EECD);
        if(eecd & E1000_EECD_SIZE) large_eeprom = TRUE;
        eecd |= E1000_EECD_REQ;
        E1000_WRITE_REG(hw, EECD, eecd);
        eecd = E1000_READ_REG(hw, EECD);
        while((!(eecd & E1000_EECD_GNT)) && (i < 100)) {
            i++;
            usec_delay(5);
            eecd = E1000_READ_REG(hw, EECD);
        }
        if(!(eecd & E1000_EECD_GNT)) {
            eecd &= ~E1000_EECD_REQ;
            E1000_WRITE_REG(hw, EECD, eecd);
            DEBUGOUT("Could not acquire EEPROM grant\n");
            return -E1000_ERR_EEPROM;
        }
    }

    /* Prepare the EEPROM for writing  */
    em_setup_eeprom(hw);

    /* Send the 9-bit (or 11-bit on large EEPROM) EWEN (write enable) command
     * to the EEPROM (5-bit opcode plus 4/6-bit dummy). This puts the EEPROM
     * into write/erase mode. 
     */
    em_shift_out_ee_bits(hw, EEPROM_EWEN_OPCODE, 5);
    if(large_eeprom) 
        em_shift_out_ee_bits(hw, 0, 6);
    else
        em_shift_out_ee_bits(hw, 0, 4);

    /* Prepare the EEPROM */
    em_standby_eeprom(hw);

    /* Send the Write command (3-bit opcode + addr) */
    em_shift_out_ee_bits(hw, EEPROM_WRITE_OPCODE, 3);
    if(large_eeprom) 
        /* If we have a 256 word EEPROM, there are 8 address bits */
        em_shift_out_ee_bits(hw, offset, 8);
    else
        /* If we have a 64 word EEPROM, there are 6 address bits */
        em_shift_out_ee_bits(hw, offset, 6);

    /* Send the data */
    em_shift_out_ee_bits(hw, data, 16);

    /* Toggle the CS line.  This in effect tells to EEPROM to actually execute 
     * the command in question.
     */
    em_standby_eeprom(hw);

    /* Now read DO repeatedly until is high (equal to '1').  The EEEPROM will
     * signal that the command has been completed by raising the DO signal.
     * If DO does not go high in 10 milliseconds, then error out.
     */
    for(i = 0; i < 200; i++) {
        eecd = E1000_READ_REG(hw, EECD);
        if(eecd & E1000_EECD_DO) break;
        usec_delay(50);
    }
    if(i == 200) {
        DEBUGOUT("EEPROM Write did not complete\n");
        status = -E1000_ERR_EEPROM;
    }

    /* Recover from write */
    em_standby_eeprom(hw);

    /* Send the 9-bit (or 11-bit on large EEPROM) EWDS (write disable) command
     * to the EEPROM (5-bit opcode plus 4/6-bit dummy). This takes the EEPROM
     * out of write/erase mode.
     */
    em_shift_out_ee_bits(hw, EEPROM_EWDS_OPCODE, 5);
    if(large_eeprom) 
        em_shift_out_ee_bits(hw, 0, 6);
    else
        em_shift_out_ee_bits(hw, 0, 4);

    /* Done with writing */
    em_cleanup_eeprom(hw);

    /* Stop requesting EEPROM access */
    if(hw->mac_type > em_82544) {
        eecd = E1000_READ_REG(hw, EECD);
        eecd &= ~E1000_EECD_REQ;
        E1000_WRITE_REG(hw, EECD, eecd);
    }

    return status;
}

/******************************************************************************
 * Reads the adapter's part number from the EEPROM
 *
 * hw - Struct containing variables accessed by shared code
 * part_num - Adapter's part number
 *****************************************************************************/
int32_t
em_read_part_num(struct em_hw *hw,
                    uint32_t *part_num)
{
    uint16_t offset = EEPROM_PBA_BYTE_1;
    uint16_t eeprom_data;

    DEBUGFUNC("em_read_part_num");

    /* Get word 0 from EEPROM */
    if(em_read_eeprom(hw, offset, &eeprom_data) < 0) {
        DEBUGOUT("EEPROM Read Error\n");
        return -E1000_ERR_EEPROM;
    }
    /* Save word 0 in upper half of part_num */
    *part_num = (uint32_t) (eeprom_data << 16);

    /* Get word 1 from EEPROM */
    if(em_read_eeprom(hw, ++offset, &eeprom_data) < 0) {
        DEBUGOUT("EEPROM Read Error\n");
        return -E1000_ERR_EEPROM;
    }
    /* Save word 1 in lower half of part_num */
    *part_num |= eeprom_data;

    return 0;
}

/******************************************************************************
 * Reads the adapter's MAC address from the EEPROM and inverts the LSB for the
 * second function of dual function devices
 *
 * hw - Struct containing variables accessed by shared code
 *****************************************************************************/
int32_t
em_read_mac_addr(struct em_hw * hw)
{
    uint16_t offset;
    uint16_t eeprom_data, i;

    DEBUGFUNC("em_read_mac_addr");

    for(i = 0; i < NODE_ADDRESS_SIZE; i += 2) {
        offset = i >> 1;
        if(em_read_eeprom(hw, offset, &eeprom_data) < 0) {
            DEBUGOUT("EEPROM Read Error\n");
            return -E1000_ERR_EEPROM;
        }
        hw->perm_mac_addr[i] = (uint8_t) (eeprom_data & 0x00FF);
        hw->perm_mac_addr[i+1] = (uint8_t) (eeprom_data >> 8);
    }
    if((hw->mac_type == em_82546) &&
       (E1000_READ_REG(hw, STATUS) & E1000_STATUS_FUNC_1)) {
        if(hw->perm_mac_addr[5] & 0x01)
            hw->perm_mac_addr[5] &= ~(0x01);
        else
            hw->perm_mac_addr[5] |= 0x01;
    }
    for(i = 0; i < NODE_ADDRESS_SIZE; i++)
        hw->mac_addr[i] = hw->perm_mac_addr[i];
    return 0;
}

/******************************************************************************
 * Initializes receive address filters.
 *
 * hw - Struct containing variables accessed by shared code 
 *
 * Places the MAC address in receive address register 0 and clears the rest
 * of the receive addresss registers. Clears the multicast table. Assumes
 * the receiver is in reset when the routine is called.
 *****************************************************************************/
void
em_init_rx_addrs(struct em_hw *hw)
{
    uint32_t i;
    uint32_t addr_low;
    uint32_t addr_high;

    DEBUGFUNC("em_init_rx_addrs");

    /* Setup the receive address. */
    DEBUGOUT("Programming MAC Address into RAR[0]\n");
    addr_low = (hw->mac_addr[0] |
                (hw->mac_addr[1] << 8) |
                (hw->mac_addr[2] << 16) | (hw->mac_addr[3] << 24));

    addr_high = (hw->mac_addr[4] |
                 (hw->mac_addr[5] << 8) | E1000_RAH_AV);

    E1000_WRITE_REG_ARRAY(hw, RA, 0, addr_low);
    E1000_WRITE_REG_ARRAY(hw, RA, 1, addr_high);

    /* Zero out the other 15 receive addresses. */
    DEBUGOUT("Clearing RAR[1-15]\n");
    for(i = 1; i < E1000_RAR_ENTRIES; i++) {
        E1000_WRITE_REG_ARRAY(hw, RA, (i << 1), 0);
        E1000_WRITE_REG_ARRAY(hw, RA, ((i << 1) + 1), 0);
    }
}

/******************************************************************************
 * Updates the MAC's list of multicast addresses.
 *
 * hw - Struct containing variables accessed by shared code
 * mc_addr_list - the list of new multicast addresses
 * mc_addr_count - number of addresses
 * pad - number of bytes between addresses in the list
 *
 * The given list replaces any existing list. Clears the last 15 receive
 * address registers and the multicast table. Uses receive address registers
 * for the first 15 multicast addresses, and hashes the rest into the 
 * multicast table.
 *****************************************************************************/
void
em_mc_addr_list_update(struct em_hw *hw,
                          uint8_t *mc_addr_list,
                          uint32_t mc_addr_count,
                          uint32_t pad)
{
    uint32_t hash_value;
    uint32_t i;
    uint32_t rar_used_count = 1; /* RAR[0] is used for our MAC address */

    DEBUGFUNC("em_mc_addr_list_update");

    /* Set the new number of MC addresses that we are being requested to use. */
    hw->num_mc_addrs = mc_addr_count;

    /* Clear RAR[1-15] */
    DEBUGOUT(" Clearing RAR[1-15]\n");
    for(i = rar_used_count; i < E1000_RAR_ENTRIES; i++) {
        E1000_WRITE_REG_ARRAY(hw, RA, (i << 1), 0);
        E1000_WRITE_REG_ARRAY(hw, RA, ((i << 1) + 1), 0);
    }

    /* Clear the MTA */
    DEBUGOUT(" Clearing MTA\n");
    for(i = 0; i < E1000_NUM_MTA_REGISTERS; i++) {
        E1000_WRITE_REG_ARRAY(hw, MTA, i, 0);
    }

    /* Add the new addresses */
    for(i = 0; i < mc_addr_count; i++) {
        DEBUGOUT(" Adding the multicast addresses:\n");
        DEBUGOUT7(" MC Addr #%d =%.2X %.2X %.2X %.2X %.2X %.2X\n", i,
                  mc_addr_list[i * (ETH_LENGTH_OF_ADDRESS + pad)],
                  mc_addr_list[i * (ETH_LENGTH_OF_ADDRESS + pad) + 1],
                  mc_addr_list[i * (ETH_LENGTH_OF_ADDRESS + pad) + 2],
                  mc_addr_list[i * (ETH_LENGTH_OF_ADDRESS + pad) + 3],
                  mc_addr_list[i * (ETH_LENGTH_OF_ADDRESS + pad) + 4],
                  mc_addr_list[i * (ETH_LENGTH_OF_ADDRESS + pad) + 5]);

        hash_value = em_hash_mc_addr(hw,
                                        mc_addr_list +
                                        (i * (ETH_LENGTH_OF_ADDRESS + pad)));

        DEBUGOUT1(" Hash value = 0x%03X\n", hash_value);

        /* Place this multicast address in the RAR if there is room, *
         * else put it in the MTA            
         */
        if(rar_used_count < E1000_RAR_ENTRIES) {
            em_rar_set(hw,
                          mc_addr_list + (i * (ETH_LENGTH_OF_ADDRESS + pad)),
                          rar_used_count);
            rar_used_count++;
        } else {
            em_mta_set(hw, hash_value);
        }
    }
    DEBUGOUT("MC Update Complete\n");
}

/******************************************************************************
 * Hashes an address to determine its location in the multicast table
 *
 * hw - Struct containing variables accessed by shared code
 * mc_addr - the multicast address to hash 
 *****************************************************************************/
uint32_t
em_hash_mc_addr(struct em_hw *hw,
                   uint8_t *mc_addr)
{
    uint32_t hash_value = 0;

    /* The portion of the address that is used for the hash table is
     * determined by the mc_filter_type setting.  
     */
    switch (hw->mc_filter_type) {
    /* [0] [1] [2] [3] [4] [5]
     * 01  AA  00  12  34  56
     * LSB                 MSB
     */
    case 0:
        /* [47:36] i.e. 0x563 for above example address */
        hash_value = ((mc_addr[4] >> 4) | (((uint16_t) mc_addr[5]) << 4));
        break;
    case 1:
        /* [46:35] i.e. 0xAC6 for above example address */
        hash_value = ((mc_addr[4] >> 3) | (((uint16_t) mc_addr[5]) << 5));
        break;
    case 2:
        /* [45:34] i.e. 0x5D8 for above example address */
        hash_value = ((mc_addr[4] >> 2) | (((uint16_t) mc_addr[5]) << 6));
        break;
    case 3:
        /* [43:32] i.e. 0x634 for above example address */
        hash_value = ((mc_addr[4]) | (((uint16_t) mc_addr[5]) << 8));
        break;
    }

    hash_value &= 0xFFF;
    return hash_value;
}

/******************************************************************************
 * Sets the bit in the multicast table corresponding to the hash value.
 *
 * hw - Struct containing variables accessed by shared code
 * hash_value - Multicast address hash value
 *****************************************************************************/
void
em_mta_set(struct em_hw *hw,
              uint32_t hash_value)
{
    uint32_t hash_bit, hash_reg;
    uint32_t mta;
    uint32_t temp;

    /* The MTA is a register array of 128 32-bit registers.  
     * It is treated like an array of 4096 bits.  We want to set 
     * bit BitArray[hash_value]. So we figure out what register
     * the bit is in, read it, OR in the new bit, then write
     * back the new value.  The register is determined by the 
     * upper 7 bits of the hash value and the bit within that 
     * register are determined by the lower 5 bits of the value.
     */
    hash_reg = (hash_value >> 5) & 0x7F;
    hash_bit = hash_value & 0x1F;

    mta = E1000_READ_REG_ARRAY(hw, MTA, hash_reg);

    mta |= (1 << hash_bit);

    /* If we are on an 82544 and we are trying to write an odd offset
     * in the MTA, save off the previous entry before writing and
     * restore the old value after writing.
     */
    if((hw->mac_type == em_82544) && ((hash_reg & 0x1) == 1)) {
        temp = E1000_READ_REG_ARRAY(hw, MTA, (hash_reg - 1));
        E1000_WRITE_REG_ARRAY(hw, MTA, hash_reg, mta);
        E1000_WRITE_REG_ARRAY(hw, MTA, (hash_reg - 1), temp);
    } else {
        E1000_WRITE_REG_ARRAY(hw, MTA, hash_reg, mta);
    }
}

/******************************************************************************
 * Puts an ethernet address into a receive address register.
 *
 * hw - Struct containing variables accessed by shared code
 * addr - Address to put into receive address register
 * index - Receive address register to write
 *****************************************************************************/
void
em_rar_set(struct em_hw *hw,
              uint8_t *addr,
              uint32_t index)
{
    uint32_t rar_low, rar_high;

    /* HW expects these in little endian so we reverse the byte order
     * from network order (big endian) to little endian              
     */
    rar_low = ((uint32_t) addr[0] |
               ((uint32_t) addr[1] << 8) |
               ((uint32_t) addr[2] << 16) | ((uint32_t) addr[3] << 24));

    rar_high = ((uint32_t) addr[4] | ((uint32_t) addr[5] << 8) | E1000_RAH_AV);

    E1000_WRITE_REG_ARRAY(hw, RA, (index << 1), rar_low);
    E1000_WRITE_REG_ARRAY(hw, RA, ((index << 1) + 1), rar_high);
}

/******************************************************************************
 * Writes a value to the specified offset in the VLAN filter table.
 *
 * hw - Struct containing variables accessed by shared code
 * offset - Offset in VLAN filer table to write
 * value - Value to write into VLAN filter table
 *****************************************************************************/
void
em_write_vfta(struct em_hw *hw,
                 uint32_t offset,
                 uint32_t value)
{
    uint32_t temp;

    if((hw->mac_type == em_82544) && ((offset & 0x1) == 1)) {
        temp = E1000_READ_REG_ARRAY(hw, VFTA, (offset - 1));
        E1000_WRITE_REG_ARRAY(hw, VFTA, offset, value);
        E1000_WRITE_REG_ARRAY(hw, VFTA, (offset - 1), temp);
    } else {
        E1000_WRITE_REG_ARRAY(hw, VFTA, offset, value);
    }
}

/******************************************************************************
 * Clears the VLAN filer table
 *
 * hw - Struct containing variables accessed by shared code
 *****************************************************************************/
void
em_clear_vfta(struct em_hw *hw)
{
    uint32_t offset;

    for(offset = 0; offset < E1000_VLAN_FILTER_TBL_SIZE; offset++)
        E1000_WRITE_REG_ARRAY(hw, VFTA, offset, 0);
}

static int32_t
em_id_led_init(struct em_hw * hw)
{
    uint32_t ledctl;
    const uint32_t ledctl_mask = 0x000000FF;
    const uint32_t ledctl_on = E1000_LEDCTL_MODE_LED_ON;
    const uint32_t ledctl_off = E1000_LEDCTL_MODE_LED_OFF;
    uint16_t eeprom_data, i, temp;
    const uint16_t led_mask = 0x0F;
        
    DEBUGFUNC("em_id_led_init");
    
    if(hw->mac_type < em_82540) {
        /* Nothing to do */
        return 0;
    }
    
    ledctl = E1000_READ_REG(hw, LEDCTL);
    hw->ledctl_default = ledctl;
    hw->ledctl_mode1 = hw->ledctl_default;
    hw->ledctl_mode2 = hw->ledctl_default;
        
    if(em_read_eeprom(hw, EEPROM_ID_LED_SETTINGS, &eeprom_data) < 0) {
        DEBUGOUT("EEPROM Read Error\n");
        return -E1000_ERR_EEPROM;
    }
    if((eeprom_data== ID_LED_RESERVED_0000) || 
       (eeprom_data == ID_LED_RESERVED_FFFF)) eeprom_data = ID_LED_DEFAULT;
    for(i = 0; i < 4; i++) {
        temp = (eeprom_data >> (i << 2)) & led_mask;
        switch(temp) {
        case ID_LED_ON1_DEF2:
        case ID_LED_ON1_ON2:
        case ID_LED_ON1_OFF2:
            hw->ledctl_mode1 &= ~(ledctl_mask << (i << 3));
            hw->ledctl_mode1 |= ledctl_on << (i << 3);
            break;
        case ID_LED_OFF1_DEF2:
        case ID_LED_OFF1_ON2:
        case ID_LED_OFF1_OFF2:
            hw->ledctl_mode1 &= ~(ledctl_mask << (i << 3));
            hw->ledctl_mode1 |= ledctl_off << (i << 3);
            break;
        default:
            /* Do nothing */
            break;
        }
        switch(temp) {
        case ID_LED_DEF1_ON2:
        case ID_LED_ON1_ON2:
        case ID_LED_OFF1_ON2:
            hw->ledctl_mode2 &= ~(ledctl_mask << (i << 3));
            hw->ledctl_mode2 |= ledctl_on << (i << 3);
            break;
        case ID_LED_DEF1_OFF2:
        case ID_LED_ON1_OFF2:
        case ID_LED_OFF1_OFF2:
            hw->ledctl_mode2 &= ~(ledctl_mask << (i << 3));
            hw->ledctl_mode2 |= ledctl_off << (i << 3);
            break;
        default:
            /* Do nothing */
            break;
        }
    }
    return 0;
}

/******************************************************************************
 * Prepares SW controlable LED for use and saves the current state of the LED.
 *
 * hw - Struct containing variables accessed by shared code
 *****************************************************************************/
int32_t
em_setup_led(struct em_hw *hw)
{
    uint32_t ledctl;
 
    DEBUGFUNC("em_setup_led");
   
    switch(hw->device_id) {
    case E1000_DEV_ID_82542:
    case E1000_DEV_ID_82543GC_FIBER:
    case E1000_DEV_ID_82543GC_COPPER:
    case E1000_DEV_ID_82544EI_COPPER:
    case E1000_DEV_ID_82544EI_FIBER:
    case E1000_DEV_ID_82544GC_COPPER:
    case E1000_DEV_ID_82544GC_LOM:
        /* No setup necessary */
        break;
    case E1000_DEV_ID_82545EM_FIBER:
    case E1000_DEV_ID_82546EB_FIBER:
        ledctl = E1000_READ_REG(hw, LEDCTL);
        /* Save current LEDCTL settings */
        hw->ledctl_default = ledctl;
        /* Turn off LED0 */
        ledctl &= ~(E1000_LEDCTL_LED0_IVRT |
                    E1000_LEDCTL_LED0_BLINK | 
                    E1000_LEDCTL_LED0_MODE_MASK);
        ledctl |= (E1000_LEDCTL_MODE_LED_OFF << E1000_LEDCTL_LED0_MODE_SHIFT);
        E1000_WRITE_REG(hw, LEDCTL, ledctl);
        break;
    case E1000_DEV_ID_82540EM:
    case E1000_DEV_ID_82540EM_LOM:
    case E1000_DEV_ID_82545EM_COPPER:
    case E1000_DEV_ID_82546EB_COPPER:
        E1000_WRITE_REG(hw, LEDCTL, hw->ledctl_mode1);
        break;
    default:
        DEBUGOUT("Invalid device ID\n");
        return -E1000_ERR_CONFIG;
    }
    return 0;
}

/******************************************************************************
 * Restores the saved state of the SW controlable LED.
 *
 * hw - Struct containing variables accessed by shared code
 *****************************************************************************/
int32_t
em_cleanup_led(struct em_hw *hw)
{
    DEBUGFUNC("em_cleanup_led");

    switch(hw->device_id) {
    case E1000_DEV_ID_82542:
    case E1000_DEV_ID_82543GC_FIBER:
    case E1000_DEV_ID_82543GC_COPPER:
    case E1000_DEV_ID_82544EI_COPPER:
    case E1000_DEV_ID_82544EI_FIBER:
    case E1000_DEV_ID_82544GC_COPPER:
    case E1000_DEV_ID_82544GC_LOM:
        /* No cleanup necessary */
        break;
    case E1000_DEV_ID_82540EM:
    case E1000_DEV_ID_82540EM_LOM:
    case E1000_DEV_ID_82545EM_COPPER:
    case E1000_DEV_ID_82545EM_FIBER:
    case E1000_DEV_ID_82546EB_COPPER:
    case E1000_DEV_ID_82546EB_FIBER:
        /* Restore LEDCTL settings */
        E1000_WRITE_REG(hw, LEDCTL, hw->ledctl_default);
        break;
    default:
        DEBUGOUT("Invalid device ID\n");
        return -E1000_ERR_CONFIG;
    }
    return 0;
}
    
/******************************************************************************
 * Turns on the software controllable LED
 *
 * hw - Struct containing variables accessed by shared code
 *****************************************************************************/
int32_t
em_led_on(struct em_hw *hw)
{
    uint32_t ctrl;

    DEBUGFUNC("em_led_on");

    switch(hw->device_id) {
    case E1000_DEV_ID_82542:
    case E1000_DEV_ID_82543GC_FIBER:
    case E1000_DEV_ID_82543GC_COPPER:
    case E1000_DEV_ID_82544EI_FIBER:
        ctrl = E1000_READ_REG(hw, CTRL);
        /* Set SW Defineable Pin 0 to turn on the LED */
        ctrl |= E1000_CTRL_SWDPIN0;
        ctrl |= E1000_CTRL_SWDPIO0;
        E1000_WRITE_REG(hw, CTRL, ctrl);
        break;
    case E1000_DEV_ID_82544EI_COPPER:
    case E1000_DEV_ID_82544GC_COPPER:
    case E1000_DEV_ID_82544GC_LOM:
    case E1000_DEV_ID_82545EM_FIBER:
    case E1000_DEV_ID_82546EB_FIBER:
        ctrl = E1000_READ_REG(hw, CTRL);
        /* Clear SW Defineable Pin 0 to turn on the LED */
        ctrl &= ~E1000_CTRL_SWDPIN0;
        ctrl |= E1000_CTRL_SWDPIO0;
        E1000_WRITE_REG(hw, CTRL, ctrl);
        break;
    case E1000_DEV_ID_82540EM:
    case E1000_DEV_ID_82540EM_LOM:
    case E1000_DEV_ID_82545EM_COPPER:
    case E1000_DEV_ID_82546EB_COPPER:
        E1000_WRITE_REG(hw, LEDCTL, hw->ledctl_mode2);
        break;
    default:
        DEBUGOUT("Invalid device ID\n");
        return -E1000_ERR_CONFIG;
    }
    return 0;
}

/******************************************************************************
 * Turns off the software controllable LED
 *
 * hw - Struct containing variables accessed by shared code
 *****************************************************************************/
int32_t
em_led_off(struct em_hw *hw)
{
    uint32_t ctrl;

    DEBUGFUNC("em_led_off");

    switch(hw->device_id) {
    case E1000_DEV_ID_82542:
    case E1000_DEV_ID_82543GC_FIBER:
    case E1000_DEV_ID_82543GC_COPPER:
    case E1000_DEV_ID_82544EI_FIBER:
        ctrl = E1000_READ_REG(hw, CTRL);
        /* Clear SW Defineable Pin 0 to turn off the LED */
        ctrl &= ~E1000_CTRL_SWDPIN0;
        ctrl |= E1000_CTRL_SWDPIO0;
        E1000_WRITE_REG(hw, CTRL, ctrl);
        break;
    case E1000_DEV_ID_82544EI_COPPER:
    case E1000_DEV_ID_82544GC_COPPER:
    case E1000_DEV_ID_82544GC_LOM:
    case E1000_DEV_ID_82545EM_FIBER:
    case E1000_DEV_ID_82546EB_FIBER:
        ctrl = E1000_READ_REG(hw, CTRL);
        /* Set SW Defineable Pin 0 to turn off the LED */
        ctrl |= E1000_CTRL_SWDPIN0;
        ctrl |= E1000_CTRL_SWDPIO0;
        E1000_WRITE_REG(hw, CTRL, ctrl);
        break;
    case E1000_DEV_ID_82540EM:
    case E1000_DEV_ID_82540EM_LOM:
    case E1000_DEV_ID_82545EM_COPPER:
    case E1000_DEV_ID_82546EB_COPPER:
        E1000_WRITE_REG(hw, LEDCTL, hw->ledctl_mode1);
        break;
    default:
        DEBUGOUT("Invalid device ID\n");
        return -E1000_ERR_CONFIG;
    }
    return 0;
}

/******************************************************************************
 * Clears all hardware statistics counters. 
 *
 * hw - Struct containing variables accessed by shared code
 *****************************************************************************/
void
em_clear_hw_cntrs(struct em_hw *hw)
{
    volatile uint32_t temp;

    temp = E1000_READ_REG(hw, CRCERRS);
    temp = E1000_READ_REG(hw, SYMERRS);
    temp = E1000_READ_REG(hw, MPC);
    temp = E1000_READ_REG(hw, SCC);
    temp = E1000_READ_REG(hw, ECOL);
    temp = E1000_READ_REG(hw, MCC);
    temp = E1000_READ_REG(hw, LATECOL);
    temp = E1000_READ_REG(hw, COLC);
    temp = E1000_READ_REG(hw, DC);
    temp = E1000_READ_REG(hw, SEC);
    temp = E1000_READ_REG(hw, RLEC);
    temp = E1000_READ_REG(hw, XONRXC);
    temp = E1000_READ_REG(hw, XONTXC);
    temp = E1000_READ_REG(hw, XOFFRXC);
    temp = E1000_READ_REG(hw, XOFFTXC);
    temp = E1000_READ_REG(hw, FCRUC);
    temp = E1000_READ_REG(hw, PRC64);
    temp = E1000_READ_REG(hw, PRC127);
    temp = E1000_READ_REG(hw, PRC255);
    temp = E1000_READ_REG(hw, PRC511);
    temp = E1000_READ_REG(hw, PRC1023);
    temp = E1000_READ_REG(hw, PRC1522);
    temp = E1000_READ_REG(hw, GPRC);
    temp = E1000_READ_REG(hw, BPRC);
    temp = E1000_READ_REG(hw, MPRC);
    temp = E1000_READ_REG(hw, GPTC);
    temp = E1000_READ_REG(hw, GORCL);
    temp = E1000_READ_REG(hw, GORCH);
    temp = E1000_READ_REG(hw, GOTCL);
    temp = E1000_READ_REG(hw, GOTCH);
    temp = E1000_READ_REG(hw, RNBC);
    temp = E1000_READ_REG(hw, RUC);
    temp = E1000_READ_REG(hw, RFC);
    temp = E1000_READ_REG(hw, ROC);
    temp = E1000_READ_REG(hw, RJC);
    temp = E1000_READ_REG(hw, TORL);
    temp = E1000_READ_REG(hw, TORH);
    temp = E1000_READ_REG(hw, TOTL);
    temp = E1000_READ_REG(hw, TOTH);
    temp = E1000_READ_REG(hw, TPR);
    temp = E1000_READ_REG(hw, TPT);
    temp = E1000_READ_REG(hw, PTC64);
    temp = E1000_READ_REG(hw, PTC127);
    temp = E1000_READ_REG(hw, PTC255);
    temp = E1000_READ_REG(hw, PTC511);
    temp = E1000_READ_REG(hw, PTC1023);
    temp = E1000_READ_REG(hw, PTC1522);
    temp = E1000_READ_REG(hw, MPTC);
    temp = E1000_READ_REG(hw, BPTC);

    if(hw->mac_type < em_82543) return;

    temp = E1000_READ_REG(hw, ALGNERRC);
    temp = E1000_READ_REG(hw, RXERRC);
    temp = E1000_READ_REG(hw, TNCRS);
    temp = E1000_READ_REG(hw, CEXTERR);
    temp = E1000_READ_REG(hw, TSCTC);
    temp = E1000_READ_REG(hw, TSCTFC);

    if(hw->mac_type <= em_82544) return;

    temp = E1000_READ_REG(hw, MGTPRC);
    temp = E1000_READ_REG(hw, MGTPDC);
    temp = E1000_READ_REG(hw, MGTPTC);
}

/******************************************************************************
 * Resets Adaptive IFS to its default state.
 *
 * hw - Struct containing variables accessed by shared code
 *
 * Call this after em_init_hw. You may override the IFS defaults by setting
 * hw->ifs_params_forced to TRUE. However, you must initialize hw->
 * current_ifs_val, ifs_min_val, ifs_max_val, ifs_step_size, and ifs_ratio
 * before calling this function.
 *****************************************************************************/
void
em_reset_adaptive(struct em_hw *hw)
{
    DEBUGFUNC("em_reset_adaptive");

    if(hw->adaptive_ifs) {
        if(!hw->ifs_params_forced) {
            hw->current_ifs_val = 0;
            hw->ifs_min_val = IFS_MIN;
            hw->ifs_max_val = IFS_MAX;
            hw->ifs_step_size = IFS_STEP;
            hw->ifs_ratio = IFS_RATIO;
        }
        hw->in_ifs_mode = FALSE;
        E1000_WRITE_REG(hw, AIT, 0);
    } else {
        DEBUGOUT("Not in Adaptive IFS mode!\n");
    }
}

/******************************************************************************
 * Called during the callback/watchdog routine to update IFS value based on
 * the ratio of transmits to collisions.
 *
 * hw - Struct containing variables accessed by shared code
 * tx_packets - Number of transmits since last callback
 * total_collisions - Number of collisions since last callback
 *****************************************************************************/
void
em_update_adaptive(struct em_hw *hw)
{
    DEBUGFUNC("em_update_adaptive");

    if(hw->adaptive_ifs) {
        if((hw->collision_delta * hw->ifs_ratio) > 
           hw->tx_packet_delta) {
            if(hw->tx_packet_delta > MIN_NUM_XMITS) {
                hw->in_ifs_mode = TRUE;
                if(hw->current_ifs_val < hw->ifs_max_val) {
                    if(hw->current_ifs_val == 0)
                        hw->current_ifs_val = hw->ifs_min_val;
                    else
                        hw->current_ifs_val += hw->ifs_step_size;
                    E1000_WRITE_REG(hw, AIT, hw->current_ifs_val);
                }
            }
        } else {
            if((hw->in_ifs_mode == TRUE) && 
               (hw->tx_packet_delta <= MIN_NUM_XMITS)) {
                hw->current_ifs_val = 0;
                hw->in_ifs_mode = FALSE;
                E1000_WRITE_REG(hw, AIT, 0);
            }
        }
    } else {
        DEBUGOUT("Not in Adaptive IFS mode!\n");
    }
}

/******************************************************************************
 * Adjusts the statistic counters when a frame is accepted by TBI_ACCEPT
 * 
 * hw - Struct containing variables accessed by shared code
 * frame_len - The length of the frame in question
 * mac_addr - The Ethernet destination address of the frame in question
 *****************************************************************************/
void
em_tbi_adjust_stats(struct em_hw *hw,
                       struct em_hw_stats *stats,
                       uint32_t frame_len,
                       uint8_t *mac_addr)
{
    uint64_t carry_bit;

    /* First adjust the frame length. */
    frame_len--;
    /* We need to adjust the statistics counters, since the hardware
     * counters overcount this packet as a CRC error and undercount
     * the packet as a good packet
     */
    /* This packet should not be counted as a CRC error.    */
    stats->crcerrs--;
    /* This packet does count as a Good Packet Received.    */
    stats->gprc++;

    /* Adjust the Good Octets received counters             */
    carry_bit = 0x80000000 & stats->gorcl;
    stats->gorcl += frame_len;
    /* If the high bit of Gorcl (the low 32 bits of the Good Octets
     * Received Count) was one before the addition, 
     * AND it is zero after, then we lost the carry out, 
     * need to add one to Gorch (Good Octets Received Count High).
     * This could be simplified if all environments supported 
     * 64-bit integers.
     */
    if(carry_bit && ((stats->gorcl & 0x80000000) == 0))
        stats->gorch++;
    /* Is this a broadcast or multicast?  Check broadcast first,
     * since the test for a multicast frame will test positive on 
     * a broadcast frame.
     */
    if((mac_addr[0] == (uint8_t) 0xff) && (mac_addr[1] == (uint8_t) 0xff))
        /* Broadcast packet */
        stats->bprc++;
    else if(*mac_addr & 0x01)
        /* Multicast packet */
        stats->mprc++;

    if(frame_len == hw->max_frame_size) {
        /* In this case, the hardware has overcounted the number of
         * oversize frames.
         */
        if(stats->roc > 0)
            stats->roc--;
    }

    /* Adjust the bin counters when the extra byte put the frame in the
     * wrong bin. Remember that the frame_len was adjusted above.
     */
    if(frame_len == 64) {
        stats->prc64++;
        stats->prc127--;
    } else if(frame_len == 127) {
        stats->prc127++;
        stats->prc255--;
    } else if(frame_len == 255) {
        stats->prc255++;
        stats->prc511--;
    } else if(frame_len == 511) {
        stats->prc511++;
        stats->prc1023--;
    } else if(frame_len == 1023) {
        stats->prc1023++;
        stats->prc1522--;
    } else if(frame_len == 1522) {
        stats->prc1522++;
    }
}

/******************************************************************************
 * Gets the current PCI bus type, speed, and width of the hardware
 *
 * hw - Struct containing variables accessed by shared code
 *****************************************************************************/
void
em_get_bus_info(struct em_hw *hw)
{
    uint32_t status;

    if(hw->mac_type < em_82543) {
        hw->bus_type = em_bus_type_unknown;
        hw->bus_speed = em_bus_speed_unknown;
        hw->bus_width = em_bus_width_unknown;
        return;
    }

    status = E1000_READ_REG(hw, STATUS);
    hw->bus_type = (status & E1000_STATUS_PCIX_MODE) ?
                   em_bus_type_pcix : em_bus_type_pci;
    if(hw->bus_type == em_bus_type_pci) {
        hw->bus_speed = (status & E1000_STATUS_PCI66) ?
                        em_bus_speed_66 : em_bus_speed_33;
    } else {
        switch (status & E1000_STATUS_PCIX_SPEED) {
        case E1000_STATUS_PCIX_SPEED_66:
            hw->bus_speed = em_bus_speed_66;
            break;
        case E1000_STATUS_PCIX_SPEED_100:
            hw->bus_speed = em_bus_speed_100;
            break;
        case E1000_STATUS_PCIX_SPEED_133:
            hw->bus_speed = em_bus_speed_133;
            break;
        default:
            hw->bus_speed = em_bus_speed_reserved;
            break;
        }
    }
    hw->bus_width = (status & E1000_STATUS_BUS64) ?
                    em_bus_width_64 : em_bus_width_32;
}
/******************************************************************************
 * Reads a value from one of the devices registers using port I/O (as opposed
 * memory mapped I/O). Only 82544 and newer devices support port I/O.
 *
 * hw - Struct containing variables accessed by shared code
 * offset - offset to read from
 *****************************************************************************/
uint32_t
em_read_reg_io(struct em_hw *hw,
                  uint32_t offset)
{
    uint32_t io_addr = hw->io_base;
    uint32_t io_data = hw->io_base + 4;

    em_io_write(hw, io_addr, offset);
    return em_io_read(hw, io_data);
}

/******************************************************************************
 * Writes a value to one of the devices registers using port I/O (as opposed to
 * memory mapped I/O). Only 82544 and newer devices support port I/O.
 *
 * hw - Struct containing variables accessed by shared code
 * offset - offset to write to
 * value - value to write
 *****************************************************************************/
void
em_write_reg_io(struct em_hw *hw,
                   uint32_t offset,
                   uint32_t value)
{
    uint32_t io_addr = hw->io_base;
    uint32_t io_data = hw->io_base + 4;

    em_io_write(hw, io_addr, offset);
    em_io_write(hw, io_data, value);
}

