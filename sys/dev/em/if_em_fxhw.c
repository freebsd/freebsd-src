/*******************************************************************************

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

*******************************************************************************/

/*$FreeBSD$*/
/* if_em_fxhw.c
 * Shared functions for accessing and configuring the MAC
 */

#include <dev/em/if_em_fxhw.h>
#include <dev/em/if_em_phy.h>

/******************************************************************************
 * Raises the EEPROM's clock input.
 *
 * shared - Struct containing variables accessed by shared code
 * eecd_reg - EECD's current value
 *****************************************************************************/
static void
em_raise_clock(struct em_shared_adapter *shared,
                  uint32_t *eecd_reg)
{
    /* Raise the clock input to the EEPROM (by setting the SK bit), and then
     *  wait 50 microseconds.
     */
    *eecd_reg = *eecd_reg | E1000_EECD_SK;
    E1000_WRITE_REG(shared, EECD, *eecd_reg);
    usec_delay(50);
    return;
}

/******************************************************************************
 * Lowers the EEPROM's clock input.
 *
 * shared - Struct containing variables accessed by shared code 
 * eecd_reg - EECD's current value
 *****************************************************************************/
static void
em_lower_clock(struct em_shared_adapter *shared,
                  uint32_t *eecd_reg)
{
    /* Lower the clock input to the EEPROM (by clearing the SK bit), and then 
     * wait 50 microseconds. 
     */
    *eecd_reg = *eecd_reg & ~E1000_EECD_SK;
    E1000_WRITE_REG(shared, EECD, *eecd_reg);
    usec_delay(50);
    return;
}

/******************************************************************************
 * Shift data bits out to the EEPROM.
 *
 * shared - Struct containing variables accessed by shared code
 * data - data to send to the EEPROM
 * count - number of bits to shift out
 *****************************************************************************/
static void
em_shift_out_bits(struct em_shared_adapter *shared,
                     uint16_t data,
                     uint16_t count)
{
    uint32_t eecd_reg;
    uint32_t mask;

    /* We need to shift "count" bits out to the EEPROM. So, value in the
     * "data" parameter will be shifted out to the EEPROM one bit at a time.
     * In order to do this, "data" must be broken down into bits. 
     */
    mask = 0x01 << (count - 1);
    eecd_reg = E1000_READ_REG(shared, EECD);
    eecd_reg &= ~(E1000_EECD_DO | E1000_EECD_DI);
    do {
        /* A "1" is shifted out to the EEPROM by setting bit "DI" to a "1",
         * and then raising and then lowering the clock (the SK bit controls
         * the clock input to the EEPROM).  A "0" is shifted out to the EEPROM
         * by setting "DI" to "0" and then raising and then lowering the clock.
         */
        eecd_reg &= ~E1000_EECD_DI;

        if(data & mask)
            eecd_reg |= E1000_EECD_DI;

        E1000_WRITE_REG(shared, EECD, eecd_reg);

        usec_delay(50);

        em_raise_clock(shared, &eecd_reg);
        em_lower_clock(shared, &eecd_reg);

        mask = mask >> 1;

    } while(mask);

    /* We leave the "DI" bit set to "0" when we leave this routine. */
    eecd_reg &= ~E1000_EECD_DI;
    E1000_WRITE_REG(shared, EECD, eecd_reg);
    return;
}

/******************************************************************************
 * Shift data bits in from the EEPROM
 *
 * shared - Struct containing variables accessed by shared code
 *****************************************************************************/
static uint16_t
em_shift_in_bits(struct em_shared_adapter *shared)
{
    uint32_t eecd_reg;
    uint32_t i;
    uint16_t data;

    /* In order to read a register from the EEPROM, we need to shift 16 bits 
     * in from the EEPROM. Bits are "shifted in" by raising the clock input to
     * the EEPROM (setting the SK bit), and then reading the value of the "DO"
     * bit.  During this "shifting in" process the "DI" bit should always be 
     * clear..
     */

    eecd_reg = E1000_READ_REG(shared, EECD);

    eecd_reg &= ~(E1000_EECD_DO | E1000_EECD_DI);
    data = 0;

    for(i = 0; i < 16; i++) {
        data = data << 1;
        em_raise_clock(shared, &eecd_reg);

        eecd_reg = E1000_READ_REG(shared, EECD);

        eecd_reg &= ~(E1000_EECD_DI);
        if(eecd_reg & E1000_EECD_DO)
            data |= 1;

        em_lower_clock(shared, &eecd_reg);
    }

    return data;
}

/******************************************************************************
 * Prepares EEPROM for access
 *
 * shared - Struct containing variables accessed by shared code
 *
 * Lowers EEPROM clock. Clears input pin. Sets the chip select pin. This 
 * function should be called before issuing a command to the EEPROM.
 *****************************************************************************/
static void
em_setup_eeprom(struct em_shared_adapter *shared)
{
    uint32_t eecd_reg;

    eecd_reg = E1000_READ_REG(shared, EECD);

    /*  Clear SK and DI  */
    eecd_reg &= ~(E1000_EECD_SK | E1000_EECD_DI);
    E1000_WRITE_REG(shared, EECD, eecd_reg);

    /*  Set CS  */
    eecd_reg |= E1000_EECD_CS;
    E1000_WRITE_REG(shared, EECD, eecd_reg);
    return;
}

/******************************************************************************
 * Returns EEPROM to a "standby" state
 * 
 * shared - Struct containing variables accessed by shared code
 *****************************************************************************/
static void
em_standby_eeprom(struct em_shared_adapter *shared)
{
    uint32_t eecd_reg;

    eecd_reg = E1000_READ_REG(shared, EECD);

    /*  Deselct EEPROM  */
    eecd_reg &= ~(E1000_EECD_CS | E1000_EECD_SK);
    E1000_WRITE_REG(shared, EECD, eecd_reg);
    usec_delay(50);

    /*  Clock high  */
    eecd_reg |= E1000_EECD_SK;
    E1000_WRITE_REG(shared, EECD, eecd_reg);
    usec_delay(50);

    /*  Select EEPROM  */
    eecd_reg |= E1000_EECD_CS;
    E1000_WRITE_REG(shared, EECD, eecd_reg);
    usec_delay(50);

    /*  Clock low  */
    eecd_reg &= ~E1000_EECD_SK;
    E1000_WRITE_REG(shared, EECD, eecd_reg);
    usec_delay(50);
    return;
}

/******************************************************************************
 * Raises then lowers the EEPROM's clock pin
 *
 * shared - Struct containing variables accessed by shared code
 *****************************************************************************/
static void
em_clock_eeprom(struct em_shared_adapter *shared)
{
    uint32_t eecd_reg;

    eecd_reg = E1000_READ_REG(shared, EECD);

    /*  Rising edge of clock  */
    eecd_reg |= E1000_EECD_SK;
    E1000_WRITE_REG(shared, EECD, eecd_reg);
    usec_delay(50);

    /*  Falling edge of clock  */
    eecd_reg &= ~E1000_EECD_SK;
    E1000_WRITE_REG(shared, EECD, eecd_reg);
    usec_delay(50);
    return;
}

/******************************************************************************
 * Terminates a command by lowering the EEPROM's chip select pin
 *
 * shared - Struct containing variables accessed by shared code
 *****************************************************************************/
static void
em_cleanup_eeprom(struct em_shared_adapter *shared)
{
    uint32_t eecd_reg;

    eecd_reg = E1000_READ_REG(shared, EECD);

    eecd_reg &= ~(E1000_EECD_CS | E1000_EECD_DI);

    E1000_WRITE_REG(shared, EECD, eecd_reg);

    em_clock_eeprom(shared);
    return;
}

/******************************************************************************
 * Waits for the EEPROM to finish the current command.
 *
 * shared - Struct containing variables accessed by shared code
 *
 * The command is done when the EEPROM's data out pin goes high.
 *****************************************************************************/
static uint16_t
em_wait_eeprom_command(struct em_shared_adapter *shared)
{
    uint32_t eecd_reg;
    uint32_t i;


    /* Toggle the CS line.  This in effect tells to EEPROM to actually execute 
     * the command in question.
     */
    em_standby_eeprom(shared);

    /* Now read DO repeatedly until is high (equal to '1').  The EEEPROM will
     * signal that the command has been completed by raising the DO signal.
     * If DO does not go high in 10 milliseconds, then error out.
     */
    for(i = 0; i < 200; i++) {
        eecd_reg = E1000_READ_REG(shared, EECD);

        if(eecd_reg & E1000_EECD_DO)
            return (TRUE);

        usec_delay(50);
    }
    ASSERT(0);
    return (FALSE);
}

/******************************************************************************
 * Forces the MAC's flow control settings.
 * 
 * shared - Struct containing variables accessed by shared code
 *
 * Sets the TFCE and RFCE bits in the device control register to reflect
 * the adapter settings. TFCE and RFCE need to be explicitly set by
 * software when a Copper PHY is used because autonegotiation is managed
 * by the PHY rather than the MAC. Software must also configure these
 * bits when link is forced on a fiber connection.
 *****************************************************************************/
static void
em_force_mac_fc(struct em_shared_adapter *shared)
{
    uint32_t ctrl_reg;

    DEBUGFUNC("em_force_mac_fc");

    /* Get the current configuration of the Device Control Register */
    ctrl_reg = E1000_READ_REG(shared, CTRL);

    /* Because we didn't get link via the internal auto-negotiation
     * mechanism (we either forced link or we got link via PHY
     * auto-neg), we have to manually enable/disable transmit an
     * receive flow control.
     *
     * The "Case" statement below enables/disable flow control
     * according to the "shared->fc" parameter.
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

    switch (shared->fc) {
    case em_fc_none:        /* 0 */

        ctrl_reg &= (~(E1000_CTRL_TFCE | E1000_CTRL_RFCE));
        break;

    case em_fc_rx_pause:    /* 1 */

        ctrl_reg &= (~E1000_CTRL_TFCE);
        ctrl_reg |= E1000_CTRL_RFCE;
        break;

    case em_fc_tx_pause:    /* 2 */

        ctrl_reg &= (~E1000_CTRL_RFCE);
        ctrl_reg |= E1000_CTRL_TFCE;
        break;

    case em_fc_full:        /* 3 */

        ctrl_reg |= (E1000_CTRL_TFCE | E1000_CTRL_RFCE);
        break;

    default:

        DEBUGOUT("Flow control param set incorrectly\n");
        ASSERT(0);

        break;
    }

    /* Disable TX Flow Control for 82542 (rev 2.0) */
    if(shared->mac_type == em_82542_rev2_0)
        ctrl_reg &= (~E1000_CTRL_TFCE);


    E1000_WRITE_REG(shared, CTRL, ctrl_reg);
    return;
}

/******************************************************************************
 * Reset the transmit and receive units; mask and clear all interrupts.
 *
 * shared - Struct containing variables accessed by shared code
 *****************************************************************************/
void
em_adapter_stop(struct em_shared_adapter *shared)
{
#if DBG
    uint32_t ctrl_reg;
#endif
    uint32_t icr_reg;
    uint16_t pci_cmd_word;

    DEBUGFUNC("em_shared_adapter_stop");

    /* If we are stopped or resetting exit gracefully and wait to be
     * started again before accessing the hardware.
     */
    if(shared->adapter_stopped) {
        DEBUGOUT("Exiting because the adapter is already stopped!!!\n");
        return;
    }

    /* Set the Adapter Stopped flag so other driver functions stop
     * touching the Hardware.
     */
    shared->adapter_stopped = TRUE;

    /* For 82542 (rev 2.0), disable MWI before issuing a device reset */
    if(shared->mac_type == em_82542_rev2_0) {
        if(shared->pci_cmd_word & CMD_MEM_WRT_INVALIDATE) {
            DEBUGOUT("Disabling MWI on 82542 rev 2.0\n");

            pci_cmd_word = shared->pci_cmd_word & ~CMD_MEM_WRT_INVALIDATE;

            em_write_pci_cfg(shared, PCI_COMMAND_REGISTER, &pci_cmd_word);
        }
    }

    /* Clear interrupt mask to stop board from generating interrupts */
    DEBUGOUT("Masking off all interrupts\n");
    E1000_WRITE_REG(shared, IMC, 0xffffffff);

    /* Disable the Transmit and Receive units.  Then delay to allow
     * any pending transactions to complete before we hit the MAC with
     * the global reset.
     */
    E1000_WRITE_REG(shared, RCTL, 0);
    E1000_WRITE_REG(shared, TCTL, 0);

    /* The tbi_compatibility_on Flag must be cleared when Rctl is cleared. */
    shared->tbi_compatibility_on = FALSE;

    msec_delay(10);

    /* Issue a global reset to the MAC.  This will reset the chip's
     * transmit, receive, DMA, and link units.  It will not effect
     * the current PCI configuration.  The global reset bit is self-
     * clearing, and should clear within a microsecond.
     */
    DEBUGOUT("Issuing a global reset to MAC\n");
    E1000_WRITE_REG(shared, CTRL, E1000_CTRL_RST);

    /* Delay a few ms just to allow the reset to complete */
    msec_delay(10);

#if DBG
    /* Make sure the self-clearing global reset bit did self clear */
    ctrl_reg = E1000_READ_REG(shared, CTRL);

    ASSERT(!(ctrl_reg & E1000_CTRL_RST));
#endif

    /* Clear interrupt mask to stop board from generating interrupts */
    DEBUGOUT("Masking off all interrupts\n");
    E1000_WRITE_REG(shared, IMC, 0xffffffff);

    /* Clear any pending interrupt events. */
    icr_reg = E1000_READ_REG(shared, ICR);

    /* If MWI was previously enabled, reenable it. */
    if(shared->mac_type == em_82542_rev2_0) {
        if(shared->pci_cmd_word & CMD_MEM_WRT_INVALIDATE) {
            em_write_pci_cfg(shared,
                                PCI_COMMAND_REGISTER, &shared->pci_cmd_word);
        }
    }
    return;
}

/******************************************************************************
 * Performs basic configuration of the adapter.
 *
 * shared - Struct containing variables accessed by shared code
 * 
 * Assumes that the controller has previously been reset and is in a 
 * post-reset uninitialized state. Initializes the receive address registers,
 * multicast table, and VLAN filter table. Calls routines to setup link
 * configuration and flow control settings. Clears all on-chip counters. Leaves
 * the transmit and receive units disabled and uninitialized.
 *****************************************************************************/
boolean_t
em_init_hw(struct em_shared_adapter *shared)
{
    uint32_t status_reg;
    uint32_t i;
    uint16_t pci_cmd_word;
    boolean_t status;

    DEBUGFUNC("em_init_hw");

    /* Set the Media Type and exit with error if it is not valid. */
    if(shared->mac_type != em_82543) {
        /* tbi_compatibility is only valid on 82543 */
        shared->tbi_compatibility_en = FALSE;
    }

    if(shared->mac_type >= em_82543) {
        status_reg = E1000_READ_REG(shared, STATUS);
        if(status_reg & E1000_STATUS_TBIMODE) {
            shared->media_type = em_media_type_fiber;
            /* tbi_compatibility not valid on fiber */
            shared->tbi_compatibility_en = FALSE;
        } else {
            shared->media_type = em_media_type_copper;
        }
    } else {
        /* This is an 82542 (fiber only) */
        shared->media_type = em_media_type_fiber;
    }

    /* Disabling VLAN filtering. */
    DEBUGOUT("Initializing the IEEE VLAN\n");
    E1000_WRITE_REG(shared, VET, 0);

    em_clear_vfta(shared);

    /* For 82542 (rev 2.0), disable MWI and put the receiver into reset */
    if(shared->mac_type == em_82542_rev2_0) {
        if(shared->pci_cmd_word & CMD_MEM_WRT_INVALIDATE) {
            DEBUGOUT("Disabling MWI on 82542 rev 2.0\n");
            pci_cmd_word = shared->pci_cmd_word & ~CMD_MEM_WRT_INVALIDATE;
            em_write_pci_cfg(shared, PCI_COMMAND_REGISTER, &pci_cmd_word);
        }
        E1000_WRITE_REG(shared, RCTL, E1000_RCTL_RST);

        msec_delay(5);
    }

    /* Setup the receive address. This involves initializing all of the Receive
     * Address Registers (RARs 0 - 15).
     */
    em_init_rx_addrs(shared);

    /* For 82542 (rev 2.0), take the receiver out of reset and enable MWI */
    if(shared->mac_type == em_82542_rev2_0) {
        E1000_WRITE_REG(shared, RCTL, 0);

        msec_delay(1);

        if(shared->pci_cmd_word & CMD_MEM_WRT_INVALIDATE) {
            em_write_pci_cfg(shared,
                                PCI_COMMAND_REGISTER, &shared->pci_cmd_word);
        }
    }

    /* Zero out the Multicast HASH table */
    DEBUGOUT("Zeroing the MTA\n");
    for(i = 0; i < E1000_MC_TBL_SIZE; i++)
        E1000_WRITE_REG_ARRAY(shared, MTA, i, 0);

    /* Call a subroutine to configure the link and setup flow control. */
    status = em_setup_fc_and_link(shared);

    /* Clear all of the statistics registers (clear on read).  It is
     * important that we do this after we have tried to establish link
     * because the symbol error count will increment wildly if there
     * is no link.
     */
    em_clear_hw_cntrs(shared);
    
    shared->large_eeprom = FALSE;
    shared->low_profile = FALSE;
    if(shared->mac_type == em_82544) {
        i = em_read_eeprom(shared, E1000_EEPROM_LED_LOGIC);
        if(i & E1000_EEPROM_SWDPIN0)
            shared->low_profile = TRUE;
    }

    return (status);
}

/******************************************************************************
 * Initializes receive address filters.
 *
 * shared - Struct containing variables accessed by shared code 
 *
 * Places the MAC address in receive address register 0 and clears the rest
 * of the receive addresss registers. Clears the multicast table. Assumes
 * the receiver is in reset when the routine is called.
 *****************************************************************************/
void
em_init_rx_addrs(struct em_shared_adapter *shared)
{
    uint32_t i;
    uint32_t addr_low;
    uint32_t addr_high;

    DEBUGFUNC("em_init_rx_addrs");

    /* Setup the receive address. */
    DEBUGOUT("Programming MAC Address into RAR[0]\n");
    addr_low = (shared->mac_addr[0] |
                (shared->mac_addr[1] << 8) |
                (shared->mac_addr[2] << 16) | (shared->mac_addr[3] << 24));

    addr_high = (shared->mac_addr[4] |
                 (shared->mac_addr[5] << 8) | E1000_RAH_AV);

    E1000_WRITE_REG_ARRAY(shared, RA, 0, addr_low);
    E1000_WRITE_REG_ARRAY(shared, RA, 1, addr_high);

    /* Zero out the other 15 receive addresses. */
    DEBUGOUT("Clearing RAR[1-15]\n");
    for(i = 1; i < E1000_RAR_ENTRIES; i++) {
        E1000_WRITE_REG_ARRAY(shared, RA, (i << 1), 0);
        E1000_WRITE_REG_ARRAY(shared, RA, ((i << 1) + 1), 0);
    }

    return;
}

/******************************************************************************
 * Updates the MAC's list of multicast addresses.
 *
 * shared - Struct containing variables accessed by shared code
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
em_mc_addr_list_update(struct em_shared_adapter *shared,
                          uint8_t *mc_addr_list,
                          uint32_t mc_addr_count,
                          uint32_t pad)
{
    uint32_t hash_value;
    uint32_t i;
    uint32_t rar_used_count = 1;        /* RAR[0] is used for our MAC address */

    DEBUGFUNC("em_mc_addr_list_update");

    /* Set the new number of MC addresses that we are being requested to use. */
    shared->num_mc_addrs = mc_addr_count;

    /* Clear RAR[1-15] */
    DEBUGOUT(" Clearing RAR[1-15]\n");
    for(i = rar_used_count; i < E1000_RAR_ENTRIES; i++) {
        E1000_WRITE_REG_ARRAY(shared, RA, (i << 1), 0);
        E1000_WRITE_REG_ARRAY(shared, RA, ((i << 1) + 1), 0);
    }

    /* Clear the MTA */
    DEBUGOUT(" Clearing MTA\n");
    for(i = 0; i < E1000_NUM_MTA_REGISTERS; i++) {
        E1000_WRITE_REG_ARRAY(shared, MTA, i, 0);
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

        hash_value = em_hash_mc_addr(shared,
                                        mc_addr_list +
                                        (i * (ETH_LENGTH_OF_ADDRESS + pad)));

        DEBUGOUT1(" Hash value = 0x%03X\n", hash_value);

        /* Place this multicast address in the RAR if there is room, *
         * else put it in the MTA            
         */
        if(rar_used_count < E1000_RAR_ENTRIES) {
            em_rar_set(shared,
                          mc_addr_list + (i * (ETH_LENGTH_OF_ADDRESS + pad)),
                          rar_used_count);
            rar_used_count++;
        } else {
            em_mta_set(shared, hash_value);
        }
    }

    DEBUGOUT("MC Update Complete\n");
    return;
}

/******************************************************************************
 * Hashes an address to determine its location in the multicast table
 *
 * shared - Struct containing variables accessed by shared code
 * mc_addr - the multicast address to hash 
 *****************************************************************************/
uint32_t
em_hash_mc_addr(struct em_shared_adapter *shared,
                   uint8_t *mc_addr)
{
    uint32_t hash_value = 0;

    /* The portion of the address that is used for the hash table is
     * determined by the mc_filter_type setting.  
     */
    switch (shared->mc_filter_type) {
        /* [0] [1] [2] [3] [4] [5]
            * 01  AA  00  12  34  56
            * LSB                 MSB - According to H/W docs */
    case 0:
        /* [47:36] i.e. 0x563 for above example address */
        hash_value = ((mc_addr[4] >> 4) | (((uint16_t) mc_addr[5]) << 4));
        break;
    case 1:                   /* [46:35] i.e. 0xAC6 for above example address */
        hash_value = ((mc_addr[4] >> 3) | (((uint16_t) mc_addr[5]) << 5));
        break;
    case 2:                   /* [45:34] i.e. 0x5D8 for above example address */
        hash_value = ((mc_addr[4] >> 2) | (((uint16_t) mc_addr[5]) << 6));
        break;
    case 3:                   /* [43:32] i.e. 0x634 for above example address */
        hash_value = ((mc_addr[4]) | (((uint16_t) mc_addr[5]) << 8));
        break;
    }

    hash_value &= 0xFFF;
    return (hash_value);
}

/******************************************************************************
 * Sets the bit in the multicast table corresponding to the hash value.
 *
 * shared - Struct containing variables accessed by shared code
 * hash_value - Multicast address hash value
 *****************************************************************************/
void
em_mta_set(struct em_shared_adapter *shared,
              uint32_t hash_value)
{
    uint32_t hash_bit, hash_reg;
    uint32_t mta_reg;
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

    mta_reg = E1000_READ_REG_ARRAY(shared, MTA, hash_reg);

    mta_reg |= (1 << hash_bit);

    /* If we are on an 82544 and we are trying to write an odd offset
     * in the MTA, save off the previous entry before writing and
     * restore the old value after writing.
     */
    if((shared->mac_type == em_82544) && ((hash_reg & 0x1) == 1)) {
        temp = E1000_READ_REG_ARRAY(shared, MTA, (hash_reg - 1));
        E1000_WRITE_REG_ARRAY(shared, MTA, hash_reg, mta_reg);
        E1000_WRITE_REG_ARRAY(shared, MTA, (hash_reg - 1), temp);
    } else {
        E1000_WRITE_REG_ARRAY(shared, MTA, hash_reg, mta_reg);
    }
    return;
}

/******************************************************************************
 * Puts an ethernet address into a receive address register.
 *
 * shared - Struct containing variables accessed by shared code
 * addr - Address to put into receive address register
 * index - Receive address register to write
 *****************************************************************************/
void
em_rar_set(struct em_shared_adapter *shared,
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

    E1000_WRITE_REG_ARRAY(shared, RA, (index << 1), rar_low);
    E1000_WRITE_REG_ARRAY(shared, RA, ((index << 1) + 1), rar_high);
    return;
}

/******************************************************************************
 * Writes a value to the specified offset in the VLAN filter table.
 *
 * shared - Struct containing variables accessed by shared code
 * offset - Offset in VLAN filer table to write
 * value - Value to write into VLAN filter table
 *****************************************************************************/
void
em_write_vfta(struct em_shared_adapter *shared,
                 uint32_t offset,
                 uint32_t value)
{
    uint32_t temp;

    if((shared->mac_type == em_82544) && ((offset & 0x1) == 1)) {
        temp = E1000_READ_REG_ARRAY(shared, VFTA, (offset - 1));
        E1000_WRITE_REG_ARRAY(shared, VFTA, offset, value);
        E1000_WRITE_REG_ARRAY(shared, VFTA, (offset - 1), temp);
    } else {
        E1000_WRITE_REG_ARRAY(shared, VFTA, offset, value);
    }
    return;
}

/******************************************************************************
 * Clears the VLAN filer table
 *
 * shared - Struct containing variables accessed by shared code
 *****************************************************************************/
void
em_clear_vfta(struct em_shared_adapter *shared)
{
    uint32_t offset;

    for(offset = 0; offset < E1000_VLAN_FILTER_TBL_SIZE; offset++)
        E1000_WRITE_REG_ARRAY(shared, VFTA, offset, 0);
    return;
}

/******************************************************************************
 * Configures flow control and link settings.
 * 
 * shared - Struct containing variables accessed by shared code
 * 
 * Determines which flow control settings to use. Calls the apropriate media-
 * specific link configuration function. Configures the flow control settings.
 * Assuming the adapter has a valid link partner, a valid link should be
 * established. Assumes the hardware has previously been reset and the 
 * transmitter and receiver are not enabled.
 *****************************************************************************/
boolean_t
em_setup_fc_and_link(struct em_shared_adapter *shared)
{
    uint32_t ctrl_reg;
    uint32_t eecd_reg;
    uint32_t ctrl_ext_reg;
    boolean_t status = TRUE;

    DEBUGFUNC("em_setup_fc_and_link");

    /* Read the SWDPIO bits and the ILOS bit out of word 0x0A in the
     * EEPROM.  Store these bits in a variable that we will later write
     * to the Device Control Register (CTRL).
     */
    eecd_reg = em_read_eeprom(shared, EEPROM_INIT_CONTROL1_REG);

    ctrl_reg =
        (((eecd_reg & EEPROM_WORD0A_SWDPIO) << SWDPIO_SHIFT) |
         ((eecd_reg & EEPROM_WORD0A_ILOS) << ILOS_SHIFT));

    /* Set the PCI priority bit correctly in the CTRL register.  This
     * determines if the adapter gives priority to receives, or if it
     * gives equal priority to transmits and receives.
     */
    if(shared->dma_fairness)
        ctrl_reg |= E1000_CTRL_PRIOR;

    /* Read and store word 0x0F of the EEPROM. This word contains bits
     * that determine the hardware's default PAUSE (flow control) mode,
     * a bit that determines whether the HW defaults to enabling or
     * disabling auto-negotiation, and the direction of the
     * SW defined pins. If there is no SW over-ride of the flow
     * control setting, then the variable shared->fc will
     * be initialized based on a value in the EEPROM.
     */
    eecd_reg = em_read_eeprom(shared, EEPROM_INIT_CONTROL2_REG);

    if(shared->fc > em_fc_full) {
        if((eecd_reg & EEPROM_WORD0F_PAUSE_MASK) == 0)
            shared->fc = em_fc_none;
        else if((eecd_reg & EEPROM_WORD0F_PAUSE_MASK) == EEPROM_WORD0F_ASM_DIR)
            shared->fc = em_fc_tx_pause;
        else
            shared->fc = em_fc_full;
    }

    /* We want to save off the original Flow Control configuration just
     * in case we get disconnected and then reconnected into a different
     * hub or switch with different Flow Control capabilities.
     */
    shared->original_fc = shared->fc;

    if(shared->mac_type == em_82542_rev2_0)
        shared->fc &= (~em_fc_tx_pause);

    if((shared->mac_type < em_82543) && (shared->report_tx_early == 1))
        shared->fc &= (~em_fc_rx_pause);

    DEBUGOUT1("After fix-ups FlowControl is now = %x\n", shared->fc);

    /* Take the 4 bits from EEPROM word 0x0F that determine the initial
     * polarity value for the SW controlled pins, and setup the
     * Extended Device Control reg with that info.
     * This is needed because one of the SW controlled pins is used for
     * signal detection.  So this should be done before em_setup_pcs_link()
     * or em_phy_setup() is called.
     */
    if(shared->mac_type >= em_82543) {
        ctrl_ext_reg = ((eecd_reg & EEPROM_WORD0F_SWPDIO_EXT)
                        << SWDPIO__EXT_SHIFT);
        E1000_WRITE_REG(shared, CTRLEXT, ctrl_ext_reg);
    }

    /* Call the necessary subroutine to configure the link. */
    if(shared->media_type == em_media_type_fiber)
        status = em_setup_pcs_link(shared, ctrl_reg);
    else
        status = em_phy_setup(shared, ctrl_reg);

    /* Initialize the flow control address, type, and PAUSE timer
     * registers to their default values.  This is done even if flow
     * control is disabled, because it does not hurt anything to
     * initialize these registers.
     */
    DEBUGOUT("Initializing the Flow Control address, type and timer regs\n");

    E1000_WRITE_REG(shared, FCAL, FLOW_CONTROL_ADDRESS_LOW);
    E1000_WRITE_REG(shared, FCAH, FLOW_CONTROL_ADDRESS_HIGH);
    E1000_WRITE_REG(shared, FCT, FLOW_CONTROL_TYPE);
    E1000_WRITE_REG(shared, FCTTV, shared->fc_pause_time);

    /* Set the flow control receive threshold registers.  Normally,
     * these registers will be set to a default threshold that may be
     * adjusted later by the driver's runtime code.  However, if the
     * ability to transmit pause frames in not enabled, then these
     * registers will be set to 0. 
     */
    if(!(shared->fc & em_fc_tx_pause)) {
        E1000_WRITE_REG(shared, FCRTL, 0);
        E1000_WRITE_REG(shared, FCRTH, 0);
    } else {
       /* We need to set up the Receive Threshold high and low water
        * marks as well as (optionally) enabling the transmission of XON frames.
        */
        if(shared->fc_send_xon) {
            E1000_WRITE_REG(shared, FCRTL,
                            (shared->fc_low_water | E1000_FCRTL_XONE));
            E1000_WRITE_REG(shared, FCRTH, shared->fc_high_water);
        } else {
            E1000_WRITE_REG(shared, FCRTL, shared->fc_low_water);
            E1000_WRITE_REG(shared, FCRTH, shared->fc_high_water);
        }
    }
    return (status);
}

/******************************************************************************
 * Sets up link for a fiber based adapter
 *
 * shared - Struct containing variables accessed by shared code
 * ctrl_reg - Current value of the device control register
 *
 * Manipulates Physical Coding Sublayer functions in order to configure
 * link. Assumes the hardware has been previously reset and the transmitter
 * and receiver are not enabled.
 *****************************************************************************/
boolean_t
em_setup_pcs_link(struct em_shared_adapter *shared,
                     uint32_t ctrl_reg)
{
    uint32_t status_reg;
    uint32_t tctl_reg;
    uint32_t txcw_reg = 0;
    uint32_t i;

    DEBUGFUNC("em_setup_pcs_link");

    /* Setup the collsion distance.  Since this is configuring the
     * TBI it is assumed that we are in Full Duplex.
     */
    tctl_reg = E1000_READ_REG(shared, TCTL);
    i = E1000_FDX_COLLISION_DISTANCE;
    i <<= E1000_COLD_SHIFT;
    tctl_reg |= i;
    E1000_WRITE_REG(shared, TCTL, tctl_reg);

    /* Check for a software override of the flow control settings, and
     * setup the device accordingly.  If auto-negotiation is enabled,
     * then software will have to set the "PAUSE" bits to the correct
     * value in the Tranmsit Config Word Register (TXCW) and re-start
     * auto-negotiation.  However, if auto-negotiation is disabled,
     * then software will have to manually configure the two flow
     * control enable bits in the CTRL register.
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
    switch (shared->fc) {
    case em_fc_none:        /* 0 */
        /* Flow control (RX & TX) is completely disabled by a
         * software over-ride.
         */
        txcw_reg = (E1000_TXCW_ANE | E1000_TXCW_FD);
        break;
    case em_fc_rx_pause:    /* 1 */
        /* RX Flow control is enabled, and TX Flow control is
         * disabled, by a software over-ride.
         */
        /* Since there really isn't a way to advertise that we are
         * capable of RX Pause ONLY, we will advertise that we
         * support both symmetric and asymmetric RX PAUSE.  Later
         * we will disable the adapter's ability to send PAUSE
         * frames.
         */
        txcw_reg = (E1000_TXCW_ANE | E1000_TXCW_FD | E1000_TXCW_PAUSE_MASK);
        break;
    case em_fc_tx_pause:    /* 2 */
        /* TX Flow control is enabled, and RX Flow control is
         * disabled, by a software over-ride.
         */
        txcw_reg = (E1000_TXCW_ANE | E1000_TXCW_FD | E1000_TXCW_ASM_DIR);
        break;
    case em_fc_full:        /* 3 */
        /* Flow control (both RX and TX) is enabled by a software
         * over-ride.
         */
        txcw_reg = (E1000_TXCW_ANE | E1000_TXCW_FD | E1000_TXCW_PAUSE_MASK);
        break;
    default:
        /* We should never get here.  The value should be 0-3. */
        DEBUGOUT("Flow control param set incorrectly\n");
        ASSERT(0);
        break;
    }

    /* Since auto-negotiation is enabled, take the link out of reset.
     * (the link will be in reset, because we previously reset the
     * chip). This will restart auto-negotiation.  If auto-neogtiation
     * is successful then the link-up status bit will be set and the
     * flow control enable bits (RFCE and TFCE) will be set according
     * to their negotiated value.
     */
    DEBUGOUT("Auto-negotiation enabled\n");

    E1000_WRITE_REG(shared, TXCW, txcw_reg);
    E1000_WRITE_REG(shared, CTRL, ctrl_reg);

    shared->txcw_reg = txcw_reg;
    msec_delay(1);

    /* If we have a signal then poll for a "Link-Up" indication in the
     * Device Status Register.   Time-out if a link isn't seen in 500
     * milliseconds seconds (Auto-negotiation should complete in less
     * than 500 milliseconds even if the other end is doing it in SW).
     */
    if(!(E1000_READ_REG(shared, CTRL) & E1000_CTRL_SWDPIN1)) {

        DEBUGOUT("Looking for Link\n");
        for(i = 0; i < (LINK_UP_TIMEOUT / 10); i++) {
            msec_delay(10);
            status_reg = E1000_READ_REG(shared, STATUS);
            if(status_reg & E1000_STATUS_LU)
                break;
        }

        if(i == (LINK_UP_TIMEOUT / 10)) {
            /* AutoNeg failed to achieve a link, so we'll call the
             * "CheckForLink" routine.  This routine will force the link
             * up if we have "signal-detect".  This will allow us to
             * communicate with non-autonegotiating link partners.
             */
            DEBUGOUT("Never got a valid link from auto-neg!!!\n");

            shared->autoneg_failed = 1;
            em_check_for_link(shared);
            shared->autoneg_failed = 0;
        } else {
            shared->autoneg_failed = 0;
            DEBUGOUT("Valid Link Found\n");
        }
    } else {
        DEBUGOUT("No Signal Detected\n");
    }

    return (TRUE);
}

/******************************************************************************
 * Configures flow control settings after link is established
 * 
 * shared - Struct containing variables accessed by shared code
 *
 * Should be called immediately after a valid link has been established.
 * Forces MAC flow control settings if link was forced. When in MII/GMII mode
 * and autonegotiation is enabled, the MAC flow control settings will be set
 * based on the flow control negotiated by the PHY. In TBI mode, the TFCE
 * and RFCE bits will be automaticaly set to the negotiated flow control mode.
 *****************************************************************************/
void
em_config_fc_after_link_up(struct em_shared_adapter *shared)
{
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
    if(((shared->media_type == em_media_type_fiber)
        && (shared->autoneg_failed))
       || ((shared->media_type == em_media_type_copper)
           && (!shared->autoneg))) {
        em_force_mac_fc(shared);
    }

    /* Check for the case where we have copper media and auto-neg is
     * enabled.  In this case, we need to check and see if Auto-Neg
     * has completed, and if so, how the PHY and link partner has
     * flow control configured.
     */
    if((shared->media_type == em_media_type_copper) && shared->autoneg) {
        /* Read the MII Status Register and check to see if AutoNeg
         * has completed.  We read this twice because this reg has
         * some "sticky" (latched) bits.
         */
        mii_status_reg = em_read_phy_reg(shared, PHY_STATUS);
        mii_status_reg = em_read_phy_reg(shared, PHY_STATUS);

        if(mii_status_reg & MII_SR_AUTONEG_COMPLETE) {
            /* The AutoNeg process has completed, so we now need to
             * read both the Auto Negotiation Advertisement Register
             * (Address 4) and the Auto_Negotiation Base Page Ability
             * Register (Address 5) to determine how flow control was
             * negotiated.
             */
            mii_nway_adv_reg = em_read_phy_reg(shared,
                                                  PHY_AUTONEG_ADV);
            mii_nway_lp_ability_reg = em_read_phy_reg(shared,
                                                         PHY_LP_ABILITY);

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
                if(shared->original_fc == em_fc_full) {
                    shared->fc = em_fc_full;
                    DEBUGOUT("Flow Control = FULL.\r\n");
                } else {
                    shared->fc = em_fc_rx_pause;
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
                shared->fc = em_fc_tx_pause;
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
                shared->fc = em_fc_rx_pause;
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
            else if(shared->original_fc == em_fc_none ||
                    shared->original_fc == em_fc_tx_pause) {
                shared->fc = em_fc_none;
                DEBUGOUT("Flow Control = NONE.\r\n");
            } else {
                shared->fc = em_fc_rx_pause;
                DEBUGOUT("Flow Control = RX PAUSE frames only.\r\n");
            }

            /* Now we need to do one last check...  If we auto-
             * negotiated to HALF DUPLEX, flow control should not be
             * enabled per IEEE 802.3 spec.
             */
            em_get_speed_and_duplex(shared, &speed, &duplex);

            if(duplex == HALF_DUPLEX)
                shared->fc = em_fc_none;

            /* Now we call a subroutine to actually force the MAC
             * controller to use the correct flow control settings.
             */
            em_force_mac_fc(shared);
        } else {
            DEBUGOUT("Copper PHY and Auto Neg has not completed.\r\n");
        }
    }
    return;  
}

/******************************************************************************
 * Checks to see if the link status of the hardware has changed.
 *
 * shared - Struct containing variables accessed by shared code
 *
 * Called by any function that needs to check the link status of the adapter.
 *****************************************************************************/
void
em_check_for_link(struct em_shared_adapter *shared)
{
    uint32_t rxcw_reg;
    uint32_t ctrl_reg;
    uint32_t status_reg;
    uint32_t rctl_reg;
    uint16_t phy_data;
    uint16_t lp_capability;

    DEBUGFUNC("em_check_for_link");

    ctrl_reg = E1000_READ_REG(shared, CTRL);
    status_reg = E1000_READ_REG(shared, STATUS);
    rxcw_reg = E1000_READ_REG(shared, RXCW);

    /* If we have a copper PHY then we only want to go out to the PHY
     * registers to see if Auto-Neg has completed and/or if our link
     * status has changed.  The get_link_status flag will be set if we
     * receive a Link Status Change interrupt or we have Rx Sequence
     * Errors.
     */
    if(shared->media_type == em_media_type_copper
       && shared->get_link_status) {
        /* First we want to see if the MII Status Register reports
         * link.  If so, then we want to get the current speed/duplex
         * of the PHY.
         * Read the register twice since the link bit is sticky.
         */
        phy_data = em_read_phy_reg(shared, PHY_STATUS);
        phy_data = em_read_phy_reg(shared, PHY_STATUS);

        if(phy_data & MII_SR_LINK_STATUS) {
            shared->get_link_status = FALSE;
        } else {
            DEBUGOUT("**** CFL - No link detected. ****\r\n");
            return;
        }

        /* If we are forcing speed/duplex, then we simply return since
         * we have already determined whether we have link or not.
         */
        if(!shared->autoneg) {
            return;
        }

        switch (shared->phy_id) {
        case M88E1000_12_PHY_ID:
        case M88E1000_14_PHY_ID:
        case M88E1000_I_PHY_ID:
              /* We have a M88E1000 PHY and Auto-Neg is enabled.  If we
               * have Si on board that is 82544 or newer, Auto
               * Speed Detection takes care of MAC speed/duplex
               * configuration.  So we only need to configure Collision
               * Distance in the MAC.  Otherwise, we need to force
               * speed/duplex on the MAC to the current PHY speed/duplex
               * settings.
               */
            if(shared->mac_type >= em_82544) {
                DEBUGOUT("CFL - Auto-Neg complete.");
                DEBUGOUT("Configuring Collision Distance.");
                em_config_collision_dist(shared);
            } else {
                 /* Read the Phy Specific Status register to get the
                  * resolved speed/duplex settings.  Then call
                  * em_config_mac_to_phy which will retrieve
                  * PHY register information and configure the MAC to
                  * equal the negotiated speed/duplex.
                  */
                phy_data = em_read_phy_reg(shared, 
                                              M88E1000_PHY_SPEC_STATUS);

                DEBUGOUT1("CFL - Auto-Neg complete.  phy_data = %x\r\n",
                          phy_data);
                em_config_mac_to_phy(shared, phy_data);
            }

              /* Configure Flow Control now that Auto-Neg has completed.
               * We need to first restore the users desired Flow
               * Control setting since we may have had to re-autoneg
               * with a different link partner.
               */
            em_config_fc_after_link_up(shared);
            break;

        default:
            DEBUGOUT("CFL - Invalid PHY detected.\r\n");

        }                       /* end switch statement */

        /* At this point we know that we are on copper, link is up, 
         * and we are auto-neg'd.  These are pre-conditions for checking
         * the link parter capabilities register.  We use the link partner
         * capabilities to determine if TBI Compatibility needs to be turned on
         * or turned off.  If the link partner advertises any speed in addition
         * to Gigabit, then we assume that they are GMII-based and TBI 
         * compatibility is not needed.
         * If no other speeds are advertised, then we assume the link partner
         * is TBI-based and we turn on TBI Compatibility.
         */
        if(shared->tbi_compatibility_en) {
            lp_capability = em_read_phy_reg(shared, PHY_LP_ABILITY);
            if(lp_capability & (NWAY_LPAR_10T_HD_CAPS |
                                NWAY_LPAR_10T_FD_CAPS |
                                NWAY_LPAR_100TX_HD_CAPS |
                                NWAY_LPAR_100TX_FD_CAPS |
                                NWAY_LPAR_100T4_CAPS)) {
                /* If our link partner advertises below Gig, then they do not
                 * need the special Tbi Compatibility mode. 
                 */
                if(shared->tbi_compatibility_on) {
                    /* If we previously were in the mode, turn it off, now. */
                    rctl_reg = E1000_READ_REG(shared, RCTL);
                    rctl_reg &= ~E1000_RCTL_SBP;
                    E1000_WRITE_REG(shared, RCTL, rctl_reg);
                    shared->tbi_compatibility_on = FALSE;
                }
            } else {
                /* If the mode is was previously off, turn it on. 
                 * For compatibility with a suspected Tbi link partners, 
                 * we will store bad packets.
                 * (Certain frames have an additional byte on the end and will 
                 * look like CRC errors to to the hardware).
                 */
                if(!shared->tbi_compatibility_on) {
                    shared->tbi_compatibility_on = TRUE;
                    rctl_reg = E1000_READ_REG(shared, RCTL);
                    rctl_reg |= E1000_RCTL_SBP;
                    E1000_WRITE_REG(shared, RCTL, rctl_reg);
                }
            }
        }
    } /* end if em_media_type_copper statement */
    /* If we don't have link (auto-negotiation failed or link partner
     * cannot auto-negotiate) and the cable is plugged in since we don't
     * have Loss-Of-Signal (we HAVE a signal) and our link partner is
     * not trying to AutoNeg with us (we are receiving idles/data
     * then we need to force our link to connect to a non
     * auto-negotiating link partner.  We also need to give
     * auto-negotiation time to complete in case the cable was just
     * plugged in.  The autoneg_failed flag does this.
     */
    else if((shared->media_type == em_media_type_fiber) &&  /* Fiber PHY */
            (!(status_reg & E1000_STATUS_LU)) &&        /* no link and    */
            (!(ctrl_reg & E1000_CTRL_SWDPIN1)) &&       /* we have signal */
            (!(rxcw_reg & E1000_RXCW_C))) {     /* and rxing idle/data */
        if(shared->autoneg_failed == 0) {      /* given AutoNeg time */
            shared->autoneg_failed = 1;
            return;
        }

        DEBUGOUT("NOT RXing /C/, disable AutoNeg and force link.\r\n");

        /* Disable auto-negotiation in the TXCW register */
        E1000_WRITE_REG(shared, TXCW, (shared->txcw_reg & ~E1000_TXCW_ANE));

        /* Force link-up and also force full-duplex. */
        ctrl_reg = E1000_READ_REG(shared, CTRL);
        ctrl_reg |= (E1000_CTRL_SLU | E1000_CTRL_FD);
        E1000_WRITE_REG(shared, CTRL, ctrl_reg);

        /* Configure Flow Control after forcing link up. */
        em_config_fc_after_link_up(shared);

    } else if((shared->media_type == em_media_type_fiber) && /* Fiber */
              (ctrl_reg & E1000_CTRL_SLU) &&    /* we have forced link */
              (rxcw_reg & E1000_RXCW_C)) {      /* and Rxing /C/ ordered sets */
        /* If we are forcing link and we are receiving /C/ ordered sets,
         * then re-enable auto-negotiation in the RXCW register and
         * disable forced link in the Device Control register in an attempt
         * to AutoNeg with our link partner.
         */
        DEBUGOUT("RXing /C/, enable AutoNeg and stop forcing link.\r\n");

        /* Enable auto-negotiation in the TXCW register and stop
         * forcing link.
         */
        E1000_WRITE_REG(shared, TXCW, shared->txcw_reg);

        E1000_WRITE_REG(shared, CTRL, (ctrl_reg & ~E1000_CTRL_SLU));
    }

    return;
}                               /* CheckForLink */

/******************************************************************************
 * Clears all hardware statistics counters. 
 *
 * shared - Struct containing variables accessed by shared code
 *****************************************************************************/
void
em_clear_hw_cntrs(struct em_shared_adapter *shared)
{
    volatile uint32_t temp_reg;

    DEBUGFUNC("em_clear_hw_cntrs");

    /* if we are stopped or resetting exit gracefully */
    if(shared->adapter_stopped) {
        DEBUGOUT("Exiting because the adapter is stopped!!!\n");
        return;
    }

    temp_reg = E1000_READ_REG(shared, CRCERRS);
    temp_reg = E1000_READ_REG(shared, SYMERRS);
    temp_reg = E1000_READ_REG(shared, MPC);
    temp_reg = E1000_READ_REG(shared, SCC);
    temp_reg = E1000_READ_REG(shared, ECOL);
    temp_reg = E1000_READ_REG(shared, MCC);
    temp_reg = E1000_READ_REG(shared, LATECOL);
    temp_reg = E1000_READ_REG(shared, COLC);
    temp_reg = E1000_READ_REG(shared, DC);
    temp_reg = E1000_READ_REG(shared, SEC);
    temp_reg = E1000_READ_REG(shared, RLEC);
    temp_reg = E1000_READ_REG(shared, XONRXC);
    temp_reg = E1000_READ_REG(shared, XONTXC);
    temp_reg = E1000_READ_REG(shared, XOFFRXC);
    temp_reg = E1000_READ_REG(shared, XOFFTXC);
    temp_reg = E1000_READ_REG(shared, FCRUC);
    temp_reg = E1000_READ_REG(shared, PRC64);
    temp_reg = E1000_READ_REG(shared, PRC127);
    temp_reg = E1000_READ_REG(shared, PRC255);
    temp_reg = E1000_READ_REG(shared, PRC511);
    temp_reg = E1000_READ_REG(shared, PRC1023);
    temp_reg = E1000_READ_REG(shared, PRC1522);
    temp_reg = E1000_READ_REG(shared, GPRC);
    temp_reg = E1000_READ_REG(shared, BPRC);
    temp_reg = E1000_READ_REG(shared, MPRC);
    temp_reg = E1000_READ_REG(shared, GPTC);
    temp_reg = E1000_READ_REG(shared, GORCL);
    temp_reg = E1000_READ_REG(shared, GORCH);
    temp_reg = E1000_READ_REG(shared, GOTCL);
    temp_reg = E1000_READ_REG(shared, GOTCH);
    temp_reg = E1000_READ_REG(shared, RNBC);
    temp_reg = E1000_READ_REG(shared, RUC);
    temp_reg = E1000_READ_REG(shared, RFC);
    temp_reg = E1000_READ_REG(shared, ROC);
    temp_reg = E1000_READ_REG(shared, RJC);
    temp_reg = E1000_READ_REG(shared, TORL);
    temp_reg = E1000_READ_REG(shared, TORH);
    temp_reg = E1000_READ_REG(shared, TOTL);
    temp_reg = E1000_READ_REG(shared, TOTH);
    temp_reg = E1000_READ_REG(shared, TPR);
    temp_reg = E1000_READ_REG(shared, TPT);
    temp_reg = E1000_READ_REG(shared, PTC64);
    temp_reg = E1000_READ_REG(shared, PTC127);
    temp_reg = E1000_READ_REG(shared, PTC255);
    temp_reg = E1000_READ_REG(shared, PTC511);
    temp_reg = E1000_READ_REG(shared, PTC1023);
    temp_reg = E1000_READ_REG(shared, PTC1522);
    temp_reg = E1000_READ_REG(shared, MPTC);
    temp_reg = E1000_READ_REG(shared, BPTC);

    if(shared->mac_type < em_82543)
        return;

    temp_reg = E1000_READ_REG(shared, ALGNERRC);
    temp_reg = E1000_READ_REG(shared, RXERRC);
    temp_reg = E1000_READ_REG(shared, TNCRS);
    temp_reg = E1000_READ_REG(shared, CEXTERR);
    temp_reg = E1000_READ_REG(shared, TSCTC);
    temp_reg = E1000_READ_REG(shared, TSCTFC);
    return;
}

/******************************************************************************
 * Detects the current speed and duplex settings of the hardware.
 *
 * shared - Struct containing variables accessed by shared code
 * speed - Speed of the connection
 * duplex - Duplex setting of the connection
 *****************************************************************************/
void
em_get_speed_and_duplex(struct em_shared_adapter *shared,
                           uint16_t *speed,
                           uint16_t *duplex)
{
    uint32_t status_reg;
#if DBG
    uint16_t phy_data;
#endif

    DEBUGFUNC("em_get_speed_and_duplex");

    /* If the adapter is stopped we don't have a speed or duplex */
    if(shared->adapter_stopped) {
        *speed = 0;
        *duplex = 0;
        return;
    }

    if(shared->mac_type >= em_82543) {
        status_reg = E1000_READ_REG(shared, STATUS);
        if(status_reg & E1000_STATUS_SPEED_1000) {
            *speed = SPEED_1000;
            DEBUGOUT("1000 Mbs, ");
        } else if(status_reg & E1000_STATUS_SPEED_100) {
            *speed = SPEED_100;
            DEBUGOUT("100 Mbs, ");
        } else {
            *speed = SPEED_10;
            DEBUGOUT("10 Mbs, ");
        }

        if(status_reg & E1000_STATUS_FD) {
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

#if DBG
    if(shared->phy_id == M88E1000_12_PHY_ID ||
       shared->phy_id == M88E1000_14_PHY_ID ||
       shared->phy_id == M88E1000_I_PHY_ID) {
        /* read the phy specific status register */
        phy_data = em_read_phy_reg(shared, M88E1000_PHY_SPEC_STATUS);
        DEBUGOUT1("M88E1000 Phy Specific Status Reg contents = %x\n", phy_data);
        phy_data = em_read_phy_reg(shared, PHY_STATUS);
        DEBUGOUT1("Phy MII Status Reg contents = %x\n", phy_data);
        DEBUGOUT1("Device Status Reg contents = %x\n", 
                  E1000_READ_REG(shared, STATUS));
        /* DisplayMiiContents(Adapter, (uint8_t)Adapter->PhyAddress); */
    }
#endif
    return;
}

/******************************************************************************
 * Reads a 16 bit word from the EEPROM.
 *
 * shared - Struct containing variables accessed by shared code
 * offset - offset of 16 bit word in the EEPROM to read
 *****************************************************************************/
uint16_t
em_read_eeprom(struct em_shared_adapter *shared,
                  uint16_t offset)
{
    uint16_t data;

    
    /*  Prepare the EEPROM for reading  */
    em_setup_eeprom(shared);

    /*  Send the READ command (opcode + addr)  */
    em_shift_out_bits(shared, EEPROM_READ_OPCODE, 3);
    /* If we have a 256 word EEPROM, there are 8 address bits
     * if we have a 64 word EEPROM, there are 6 address bits
     */
    if(shared->large_eeprom) 
        em_shift_out_bits(shared, offset, 8);
    else
        em_shift_out_bits(shared, offset, 6);

    /*  Read the data  */
    data = em_shift_in_bits(shared);

    /*  End this read operation  */
    em_standby_eeprom(shared);

    return (data);
}

/******************************************************************************
 * Verifies that the EEPROM has a valid checksum
 * 
 * shared - Struct containing variables accessed by shared code
 *
 * Reads the first 64 16 bit words of the EEPROM and sums the values read.
 * If the the sum of the 64 16 bit words is 0xBABA, the EEPROM's checksum is
 * valid.
 *****************************************************************************/
boolean_t
em_validate_eeprom_checksum(struct em_shared_adapter *shared)
{
    uint16_t checksum = 0;
    uint16_t i;

    for(i = 0; i < (EEPROM_CHECKSUM_REG + 1); i++)
        checksum += em_read_eeprom(shared, i);

    if(checksum == (uint16_t) EEPROM_SUM)
        return (TRUE);
    else
        return (FALSE);
}

/******************************************************************************
 * Calculates the EEPROM checksum and writes it to the EEPROM
 *
 * shared - Struct containing variables accessed by shared code
 *
 * Sums the first 63 16 bit words of the EEPROM. Subtracts the sum from 0xBABA.
 * Writes the difference to word offset 63 of the EEPROM.
 *****************************************************************************/
void
em_update_eeprom_checksum(struct em_shared_adapter *shared)
{
    uint16_t checksum = 0;
    uint16_t i;

    for(i = 0; i < EEPROM_CHECKSUM_REG; i++)
        checksum += em_read_eeprom(shared, i);

    checksum = (uint16_t) EEPROM_SUM - checksum;

    em_write_eeprom(shared, EEPROM_CHECKSUM_REG, checksum);
    return;
}

/******************************************************************************
 * Writes a 16 bit word to a given offset in the EEPROM.
 *
 * shared - Struct containing variables accessed by shared code
 * offset - offset within the EEPROM to be written to
 * data - 16 bit word to be writen to the EEPROM
 *
 * If em_update_eeprom_checksum is not called after this function, the 
 * EEPROM will most likely contain an invalid checksum.
 *****************************************************************************/
boolean_t
em_write_eeprom(struct em_shared_adapter *shared,
                   uint16_t offset,
                   uint16_t data)
{

    /*  Prepare the EEPROM for writing  */
    em_setup_eeprom(shared);

    /*  Send the 9-bit EWEN (write enable) command to the EEPROM (5-bit opcode
     *  plus 4-bit dummy).  This puts the EEPROM into write/erase mode. 
     */
    em_shift_out_bits(shared, EEPROM_EWEN_OPCODE, 5);
    em_shift_out_bits(shared, 0, 4);

    /*  Prepare the EEPROM  */
    em_standby_eeprom(shared);

    /*  Send the Write command (3-bit opcode + addr)  */
    em_shift_out_bits(shared, EEPROM_WRITE_OPCODE, 3);
    /* If we have a 256 word EEPROM, there are 8 address bits
     * if we have a 64 word EEPROM, there are 6 address bits
     */
    if(shared->large_eeprom) 
        em_shift_out_bits(shared, offset, 8);
    else
        em_shift_out_bits(shared, offset, 6);

    /*  Send the data  */
    em_shift_out_bits(shared, data, 16);

    em_wait_eeprom_command(shared);

    /*  Recover from write  */
    em_standby_eeprom(shared);

    /* Send the 9-bit EWDS (write disable) command to the EEPROM (5-bit
     * opcode plus 4-bit dummy).  This takes the EEPROM out of write/erase
     * mode.
     */
    em_shift_out_bits(shared, EEPROM_EWDS_OPCODE, 5);
    em_shift_out_bits(shared, 0, 4);

    /*  Done with writing  */
    em_cleanup_eeprom(shared);

    return (TRUE);
}

/******************************************************************************
 * Reads the adapter's part number from the EEPROM
 *
 * shared - Struct containing variables accessed by shared code
 * part_num - Adapter's part number
 *****************************************************************************/
boolean_t
em_read_part_num(struct em_shared_adapter *shared,
                    uint32_t *part_num)
{
    uint16_t eeprom_word;

    DEBUGFUNC("em_read_part_num");

    /* Don't read the EEPROM if we are stopped */
    if(shared->adapter_stopped) {
        *part_num = 0;
        return (FALSE);
    }

    /* Get word 0 from EEPROM */
    eeprom_word = em_read_eeprom(shared, (uint16_t) (EEPROM_PBA_BYTE_1));

    DEBUGOUT("Read first part number word\n");

    /* Save word 0 in upper half is PartNumber */
    *part_num = (uint32_t) eeprom_word;
    *part_num = *part_num << 16;

    /* Get word 1 from EEPROM */
    eeprom_word =
        em_read_eeprom(shared, (uint16_t) (EEPROM_PBA_BYTE_1 + 1));

    DEBUGOUT("Read second part number word\n");

    /* Save word 1 in lower half of PartNumber */
    *part_num |= eeprom_word;

    /* read a valid part number */
    return (TRUE);
}

/******************************************************************************
 * Turns on the software controllable LED
 *
 * shared - Struct containing variables accessed by shared code
 *****************************************************************************/
void
em_led_on(struct em_shared_adapter *shared)
{
    uint32_t ctrl_reg;

    /* if we're stopped don't touch the hardware */
    if(shared->adapter_stopped)
        return;

    /* Read the content of the device control reg */
    ctrl_reg = E1000_READ_REG(shared, CTRL);

    /* Set the LED control pin to an output */
    ctrl_reg |= E1000_CTRL_SWDPIO0;

    /* Drive it high on normal boards, low on low profile boards */
    if(shared->low_profile)
        ctrl_reg &= ~E1000_CTRL_SWDPIN0;
    else
        ctrl_reg |= E1000_CTRL_SWDPIN0;

    E1000_WRITE_REG(shared, CTRL, ctrl_reg);
    return;
}

/******************************************************************************
 * Turns off the software controllable LED
 *
 * shared - Struct containing variables accessed by shared code
 *****************************************************************************/
void
em_led_off(struct em_shared_adapter *shared)
{
    uint32_t ctrl_reg;

    /* if we're stopped don't touch the hardware */
    if(shared->adapter_stopped)
        return;

    /* Read the content of the device control reg */
    ctrl_reg = E1000_READ_REG(shared, CTRL);

    /* Set the LED control pin to an output */
    ctrl_reg |= E1000_CTRL_SWDPIO0;

    /* Drive it low on normal boards, high on low profile boards */
    if(shared->low_profile)
        ctrl_reg |= E1000_CTRL_SWDPIN0;
    else
        ctrl_reg &= ~E1000_CTRL_SWDPIN0;

    /* Write the device control reg. back  */
    E1000_WRITE_REG(shared, CTRL, ctrl_reg);
    return;
}

/******************************************************************************
 * Adjusts the statistic counters when a frame is accepted by TBI_ACCEPT
 * 
 * shared - Struct containing variables accessed by shared code
 * frame_len - The length of the frame in question
 * mac_addr - The Ethernet destination address of the frame in question
 *****************************************************************************/
uint32_t
em_tbi_adjust_stats(struct em_shared_adapter *shared,
                       struct em_shared_stats *stats,
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

    if(frame_len == shared->max_frame_size) {
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
    return frame_len;
}

/******************************************************************************
 * Gets the current PCI bus type, speed, and width of the hardware
 *
 * shared - Struct containing variables accessed by shared code
 *****************************************************************************/
void
em_get_bus_info(struct em_shared_adapter *shared)
{
    uint32_t status_reg;

    if(shared->mac_type < em_82543) {
        shared->bus_type = em_bus_type_unknown;
        shared->bus_speed = em_bus_speed_unknown;
        shared->bus_width = em_bus_width_unknown;
        return;
    }

    status_reg = E1000_READ_REG(shared, STATUS);

    shared->bus_type = (status_reg & E1000_STATUS_PCIX_MODE) ?
        em_bus_type_pcix : em_bus_type_pci;

    if(shared->bus_type == em_bus_type_pci) {
        shared->bus_speed = (status_reg & E1000_STATUS_PCI66) ?
            em_bus_speed_66 : em_bus_speed_33;
    } else {
        switch (status_reg & E1000_STATUS_PCIX_SPEED) {
        case E1000_STATUS_PCIX_SPEED_66:
            shared->bus_speed = em_bus_speed_66;
            break;
        case E1000_STATUS_PCIX_SPEED_100:
            shared->bus_speed = em_bus_speed_100;
            break;
        case E1000_STATUS_PCIX_SPEED_133:
            shared->bus_speed = em_bus_speed_133;
            break;
        default:
            shared->bus_speed = em_bus_speed_reserved;
            break;
        }
    }

    shared->bus_width = (status_reg & E1000_STATUS_BUS64) ?
        em_bus_width_64 : em_bus_width_32;

    return;
}
