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
/* if_em_phy.c
 * Shared functions for accessing and configuring the PHY
 */

#include <dev/em/if_em_fxhw.h>
#include <dev/em/if_em_phy.h>

/******************************************************************************
* Raises the Management Data Clock
*
* shared - Struct containing variables accessed by shared code
* ctrl_reg - Device control register's current value
******************************************************************************/
static void
em_raise_mdc(struct em_shared_adapter *shared,
                uint32_t *ctrl_reg)
{
    /* Raise the clock input to the Management Data Clock (by setting
     * the MDC bit), and then delay 2 microseconds.
     */
    E1000_WRITE_REG(shared, CTRL, (*ctrl_reg | E1000_CTRL_MDC));
    usec_delay(2);
    return;
}

/******************************************************************************
* Lowers the Management Data Clock
*
* shared - Struct containing variables accessed by shared code
* ctrl_reg - Device control register's current value
******************************************************************************/
static void
em_lower_mdc(struct em_shared_adapter *shared,
                uint32_t *ctrl_reg)
{
    /* Lower the clock input to the Management Data Clock (by clearing
     * the MDC bit), and then delay 2 microseconds.
     */
    E1000_WRITE_REG(shared, CTRL, (*ctrl_reg & ~E1000_CTRL_MDC));
    usec_delay(2);
    return;
}

/******************************************************************************
* Shifts data bits out to the PHY
*
* shared - Struct containing variables accessed by shared code
* data - Data to send out to the PHY
* count - Number of bits to shift out
*
* Bits are shifted out in MSB to LSB order.
******************************************************************************/
static void
em_phy_shift_out(struct em_shared_adapter *shared,
                    uint32_t data,
                    uint16_t count)
{
    uint32_t ctrl_reg;
    uint32_t mask;

    ASSERT(count <= 32);

    /* We need to shift "count" number of bits out to the PHY.  So, the
     * value in the "Data" parameter will be shifted out to the PHY
     * one bit at a time.  In order to do this, "Data" must be broken
     * down into bits, which is what the "while" logic does below.
     */
    mask = 0x01;
    mask <<= (count - 1);

    ctrl_reg = E1000_READ_REG(shared, CTRL);

    /* Set MDIO_DIR (SWDPIO1) and MDC_DIR (SWDPIO2) direction bits to
     * be used as output pins.
     */
    ctrl_reg |= (E1000_CTRL_MDIO_DIR | E1000_CTRL_MDC_DIR);

    while(mask) {
        /* A "1" is shifted out to the PHY by setting the MDIO bit to
         * "1" and then raising and lowering the Management Data Clock
         * (MDC).  A "0" is shifted out to the PHY by setting the MDIO
         * bit to "0" and then raising and lowering the clock.
         */
        if(data & mask)
            ctrl_reg |= E1000_CTRL_MDIO;
        else
            ctrl_reg &= ~E1000_CTRL_MDIO;

        E1000_WRITE_REG(shared, CTRL, ctrl_reg);

        usec_delay(2);

        em_raise_mdc(shared, &ctrl_reg);
        em_lower_mdc(shared, &ctrl_reg);

        mask = mask >> 1;
    }

    /* Clear the data bit just before leaving this routine. */
    ctrl_reg &= ~E1000_CTRL_MDIO;
    return;
}

/******************************************************************************
* Shifts data bits in from the PHY
*
* shared - Struct containing variables accessed by shared code
*
* Bits are shifted in in MSB to LSB order. 
******************************************************************************/
static uint16_t
em_phy_shift_in(struct em_shared_adapter *shared)
{
    uint32_t ctrl_reg;
    uint16_t data = 0;
    uint8_t i;

    /* In order to read a register from the PHY, we need to shift in a
     * total of 18 bits from the PHY.  The first two bit (TurnAround)
     * times are used to avoid contention on the MDIO pin when a read
     * operation is performed.  These two bits are ignored by us and
     * thrown away.  Bits are "shifted in" by raising the clock input
     * to the Management Data Clock (setting the MDC bit), and then
     * reading the value of the MDIO bit.
     */ 
    ctrl_reg = E1000_READ_REG(shared, CTRL);

    /* Clear MDIO_DIR (SWDPIO1) to indicate this bit is to be used as
     * input.
     */ 
    ctrl_reg &= ~E1000_CTRL_MDIO_DIR;
    ctrl_reg &= ~E1000_CTRL_MDIO;

    E1000_WRITE_REG(shared, CTRL, ctrl_reg);

    /* Raise and Lower the clock before reading in the data.  This
     * accounts for the TurnAround bits.  The first clock occurred
     * when we clocked out the last bit of the Register Address.
     */
    em_raise_mdc(shared, &ctrl_reg);
    em_lower_mdc(shared, &ctrl_reg);

    for(data = 0, i = 0; i < 16; i++) {
        data = data << 1;
        em_raise_mdc(shared, &ctrl_reg);

        ctrl_reg = E1000_READ_REG(shared, CTRL);

        /* Check to see if we shifted in a "1". */
        if(ctrl_reg & E1000_CTRL_MDIO)
            data |= 1;

        em_lower_mdc(shared, &ctrl_reg);
    }

    em_raise_mdc(shared, &ctrl_reg);
    em_lower_mdc(shared, &ctrl_reg);

    /* Clear the MDIO bit just before leaving this routine. */
    ctrl_reg &= ~E1000_CTRL_MDIO;

    return (data);
}

/******************************************************************************
* Force PHY speed and duplex settings to shared->forced_speed_duplex
*
* shared - Struct containing variables accessed by shared code
******************************************************************************/
static void
em_phy_force_speed_duplex(struct em_shared_adapter *shared)
{
    uint32_t tctl_reg;
    uint32_t ctrl_reg;
    uint32_t shift;
    uint16_t mii_ctrl_reg;
    uint16_t mii_status_reg;
    uint16_t phy_data;
    uint16_t i;

    DEBUGFUNC("em_phy_force_speed_duplex");

    /* Turn off Flow control if we are forcing speed and duplex. */
    shared->fc = em_fc_none;

    DEBUGOUT1("shared->fc = %d\n", shared->fc);

    /* Read the Device Control Register. */
    ctrl_reg = E1000_READ_REG(shared, CTRL);

    /* Set the bits to Force Speed and Duplex in the Device Ctrl Reg. */
    ctrl_reg |= (E1000_CTRL_FRCSPD | E1000_CTRL_FRCDPX);
    ctrl_reg &= ~(DEVICE_SPEED_MASK);

    /* Clear the Auto Speed Detect Enable bit. */
    ctrl_reg &= ~E1000_CTRL_ASDE;

    /* Read the MII Control Register. */
    mii_ctrl_reg = em_read_phy_reg(shared, PHY_CTRL);

    /* We need to disable Autoneg in order to force link and duplex. */

    mii_ctrl_reg &= ~MII_CR_AUTO_NEG_EN;

    /* Are we forcing Full or Half Duplex? */
    if(shared->forced_speed_duplex == em_100_full ||
       shared->forced_speed_duplex == em_10_full) {

        /* We want to force full duplex so we SET the full duplex bits
         * in the Device and MII Control Registers.
         */
        ctrl_reg |= E1000_CTRL_FD;
        mii_ctrl_reg |= MII_CR_FULL_DUPLEX;

        DEBUGOUT("Full Duplex\n");
    } else {

        /* We want to force half duplex so we CLEAR the full duplex
         * bits in the Device and MII Control Registers.
         */
        ctrl_reg &= ~E1000_CTRL_FD;
        mii_ctrl_reg &= ~MII_CR_FULL_DUPLEX;    /* Do this implies HALF */

        DEBUGOUT("Half Duplex\n");
    }

    /* Are we forcing 100Mbps??? */
    if(shared->forced_speed_duplex == em_100_full ||
       shared->forced_speed_duplex == em_100_half) {

        /* Set the 100Mb bit and turn off the 1000Mb and 10Mb bits. */
        ctrl_reg |= E1000_CTRL_SPD_100;
        mii_ctrl_reg |= MII_CR_SPEED_100;
        mii_ctrl_reg &= ~(MII_CR_SPEED_1000 | MII_CR_SPEED_10);

        DEBUGOUT("Forcing 100mb ");
    } else {                    /* Force 10MB Full or Half */

        /* Set the 10Mb bit and turn off the 1000Mb and 100Mb bits. */
        ctrl_reg &= ~(E1000_CTRL_SPD_1000 | E1000_CTRL_SPD_100);
        mii_ctrl_reg |= MII_CR_SPEED_10;
        mii_ctrl_reg &= ~(MII_CR_SPEED_1000 | MII_CR_SPEED_100);

        DEBUGOUT("Forcing 10mb ");
    }

    /* Now we need to configure the Collision Distance.  We need to read
     * the Transmit Control Register to do this.
     * Note: This must be done for both Half or Full Duplex.
     */
    tctl_reg = E1000_READ_REG(shared, TCTL);
    DEBUGOUT1("tctl_reg = %x\n", tctl_reg);

    if(!(mii_ctrl_reg & MII_CR_FULL_DUPLEX)) {

       /* We are in Half Duplex mode so we need to set up our collision
        * distance for 10/100.
        */
        tctl_reg &= ~E1000_TCTL_COLD;
        shift = E1000_HDX_COLLISION_DISTANCE;
        shift <<= E1000_COLD_SHIFT;
        tctl_reg |= shift;
    } else {
        /* We are in Full Duplex mode.  We have the same collision
         * distance regardless of speed.
         */
        tctl_reg &= ~E1000_TCTL_COLD;
        shift = E1000_FDX_COLLISION_DISTANCE;
        shift <<= E1000_COLD_SHIFT;
        tctl_reg |= shift;
    }

    /* Write the configured values back to the Transmit Control Reg. */
    E1000_WRITE_REG(shared, TCTL, tctl_reg);

    /* Write the configured values back to the Device Control Reg. */
    E1000_WRITE_REG(shared, CTRL, ctrl_reg);

    /* Write the MII Control Register with the new PHY configuration. */
    phy_data = em_read_phy_reg(shared, M88E1000_PHY_SPEC_CTRL);

    /* Clear Auto-Crossover to force MDI manually.
     * M88E1000 requires MDI forced whenever speed/duplex is forced
     */
    phy_data &= ~M88E1000_PSCR_AUTO_X_MODE;

    em_write_phy_reg(shared, M88E1000_PHY_SPEC_CTRL, phy_data);

    DEBUGOUT1("M88E1000 PSCR: %x \n", phy_data);

    /* Need to reset the PHY or these bits will get ignored. */
    mii_ctrl_reg |= MII_CR_RESET;

    em_write_phy_reg(shared, PHY_CTRL, mii_ctrl_reg);

    /* The wait_autoneg_complete flag may be a little misleading here.
     * Since we are forcing speed and duplex, Auto-Neg is not enabled.
     * But we do want to delay for a period while forcing only so we
     * don't generate false No Link messages.  So we will wait here
     * only if the user has set wait_autoneg_complete to 1, which is
     * the default.
     */
    if(shared->wait_autoneg_complete) {
        /* We will wait for AutoNeg to complete. */
        DEBUGOUT("Waiting for forced speed/duplex link.\n");
        mii_status_reg = 0;

        /* We will wait for AutoNeg to complete or 4.5 seconds to expire. */
        for(i = PHY_FORCE_TIME; i > 0; i--) {
            /* Read the MII Status Register and wait for Auto-Neg
             * Complete bit to be set.
             */
            mii_status_reg = em_read_phy_reg(shared, PHY_STATUS);
            mii_status_reg = em_read_phy_reg(shared, PHY_STATUS);

            if(mii_status_reg & MII_SR_LINK_STATUS)
                break;

            msec_delay(100);
        }                       /* end for loop */

        if(i == 0) {            /* We didn't get link   */

            /* Reset the DSP and wait again for link.   */
            em_phy_reset_dsp(shared);
        }

        /* This loop will early-out if the link condition has been met.  */
        for(i = PHY_FORCE_TIME; i > 0; i--) {
            if(mii_status_reg & MII_SR_LINK_STATUS)
                break;

            msec_delay(100);
            /* Read the MII Status Register and wait for Auto-Neg
             * Complete bit to be set.
             */
            mii_status_reg = em_read_phy_reg(shared, PHY_STATUS);
            mii_status_reg = em_read_phy_reg(shared, PHY_STATUS);

        }                       /* end for loop */
    }    /* end if wait_autoneg_complete */
    /*
     * Because we reset the PHY above, we need to re-force TX_CLK in the
     * Extended PHY Specific Control Register to 25MHz clock.  This
     * value defaults back to a 2.5MHz clock when the PHY is reset.
     */
    phy_data = em_read_phy_reg(shared, M88E1000_EXT_PHY_SPEC_CTRL);

    phy_data |= M88E1000_EPSCR_TX_CLK_25;

    em_write_phy_reg(shared, M88E1000_EXT_PHY_SPEC_CTRL, phy_data);

    /* In addition, because of the s/w reset above, we need to enable
     * CRS on TX.  This must be set for both full and half duplex
     * operation.
     */
    phy_data = em_read_phy_reg(shared, M88E1000_PHY_SPEC_CTRL);

    phy_data |= M88E1000_PSCR_ASSERT_CRS_ON_TX;

    em_write_phy_reg(shared, M88E1000_PHY_SPEC_CTRL, phy_data);
    DEBUGOUT1("M88E1000 Phy Specific Ctrl Reg = %4x\r\n", phy_data);

    return;
}

/*****************************************************************************
* Reads the value from a PHY register
*
* shared - Struct containing variables accessed by shared code
* reg_addr - address of the PHY register to read
******************************************************************************/
uint16_t
em_read_phy_reg(struct em_shared_adapter *shared,
                   uint32_t reg_addr)
{
    uint32_t i;
    uint32_t data = 0;
    uint32_t command = 0;

    DEBUGFUNC("em_read_phy_reg");

    ASSERT(reg_addr <= MAX_PHY_REG_ADDRESS);

    if(shared->mac_type > em_82543) {
        /* Set up Op-code, Phy Address, and
         * register address in the MDI Control register.  The MAC will
         * take care of interfacing with the PHY to retrieve the
         * desired data.
         */
        command = ((reg_addr << E1000_MDIC_REG_SHIFT) |
                   (shared->phy_addr << E1000_MDIC_PHY_SHIFT) | 
                   (E1000_MDIC_OP_READ));

        DEBUGOUT1("Writing 0x%X to MDIC\n", command);
        E1000_WRITE_REG(shared, MDIC, command);

        /* Check every 10 usec to see if the read completed.  The read
         * may take as long as 64 usecs (we'll wait 100 usecs max)
         * from the CPU Write to the Ready bit assertion.
         */
        for(i = 0; i < 64; i++) {
            usec_delay(10);

            data = E1000_READ_REG(shared, MDIC);

            DEBUGOUT1("Read 0x%X from MDIC\n", data);
            if(data & E1000_MDIC_READY)
                break;
        }
    } else {
        /* We must first send a preamble through the MDIO pin to signal the
         * beginning of an MII instruction.  This is done by sending 32
         * consecutive "1" bits.
         */
        em_phy_shift_out(shared, PHY_PREAMBLE, PHY_PREAMBLE_SIZE);

        /* Now combine the next few fields that are required for a read
         * operation.  We use this method instead of calling the
         * em_phy_shift_out routine five different times.  The format of
         * a MII read instruction consists of a shift out of 14 bits and is
         * defined as follows:
         *    <Preamble><SOF><Op Code><Phy Addr><Reg Addr>
         * followed by a shift in of 18 bits.  This first two bits shifted
         * in are TurnAround bits used to avoid contention on the MDIO pin
         * when a READ operation is performed.  These two bits are thrown
         * away followed by a shift in of 16 bits which contains the
         * desired data.
         */
        command = ((reg_addr) |
                   (shared->phy_addr << 5) |
                   (PHY_OP_READ << 10) | (PHY_SOF << 12));

        em_phy_shift_out(shared, command, 14);

        /* Now that we've shifted out the read command to the MII, we need
         * to "shift in" the 16-bit value (18 total bits) of the requested
         * PHY register address.
         */
        data = (uint32_t) em_phy_shift_in(shared);
    }

    ASSERT(!(data & E1000_MDIC_ERROR));

    return ((uint16_t) data);
}

/******************************************************************************
* Writes a value to a PHY register
*
* shared - Struct containing variables accessed by shared code
* reg_addr - address of the PHY register to write
* data - data to write to the PHY
******************************************************************************/
void
em_write_phy_reg(struct em_shared_adapter *shared,
                    uint32_t reg_addr,
                    uint16_t data)
{
    uint32_t i;
    uint32_t command = 0;
    uint32_t mdic_reg;

    ASSERT(reg_addr <= MAX_PHY_REG_ADDRESS);

    if(shared->mac_type > em_82543) {
        /* Set up Op-code, Phy Address, register
         * address, and data intended for the PHY register in the MDI
         * Control register.  The MAC will take care of interfacing
         * with the PHY to send the desired data.
         */
        command = (((uint32_t) data) |
                   (reg_addr << E1000_MDIC_REG_SHIFT) |
                   (shared->phy_addr << E1000_MDIC_PHY_SHIFT) | 
                   (E1000_MDIC_OP_WRITE));

        E1000_WRITE_REG(shared, MDIC, command);

        /* Check every 10 usec to see if the read completed.  The read
         * may take as long as 64 usecs (we'll wait 100 usecs max)
         * from the CPU Write to the Ready bit assertion.
         */
        for(i = 0; i < 10; i++) {
            usec_delay(10);

            mdic_reg = E1000_READ_REG(shared, MDIC);

            if(mdic_reg & E1000_MDIC_READY)
                break;
        }
    } else {
        /* We'll need to use the SW defined pins to shift the write command
         *  out to the PHY. We first send a preamble to the PHY to signal the
         * beginning of the MII instruction.  This is done by sending 32 
         * consecutive "1" bits.
         */
        em_phy_shift_out(shared, PHY_PREAMBLE, PHY_PREAMBLE_SIZE);

        /* Now combine the remaining required fields that will indicate
         * a write operation.  We use this method instead of calling the
         * em_phy_shift_out routine for each field in the command.  The
         * format of a MII write instruction is as follows:
         * <Preamble><SOF><Op Code><Phy Addr><Reg Addr><Turnaround><Data>.
         */
        command = ((PHY_TURNAROUND) |
                   (reg_addr << 2) |
                   (shared->phy_addr << 7) |
                   (PHY_OP_WRITE << 12) | (PHY_SOF << 14));
        command <<= 16;
        command |= ((uint32_t) data);

        em_phy_shift_out(shared, command, 32);
    }
    return;
}

/******************************************************************************
* Returns the PHY to the power-on reset state
*
* shared - Struct containing variables accessed by shared code
******************************************************************************/
void
em_phy_hw_reset(struct em_shared_adapter *shared)
{
    uint32_t ctrl_reg;
    uint32_t ctrl_ext_reg;

    DEBUGFUNC("em_phy_hw_reset");

    DEBUGOUT("Resetting Phy...\n");

    if(shared->mac_type > em_82543) {
        /* Read the device control register and assert the
         * E1000_CTRL_PHY_RST bit.  Hold for 20ms and then take it out
         * of reset.
         */
        ctrl_reg = E1000_READ_REG(shared, CTRL);

        ctrl_reg |= E1000_CTRL_PHY_RST;

        E1000_WRITE_REG(shared, CTRL, ctrl_reg);

        msec_delay(20);

        ctrl_reg &= ~E1000_CTRL_PHY_RST;

        E1000_WRITE_REG(shared, CTRL, ctrl_reg);

        msec_delay(20);
    } else {
        /* Read the Extended Device Control Register, assert the
         * PHY_RESET_DIR bit.  Then clock it out to the PHY.
         */
        ctrl_ext_reg = E1000_READ_REG(shared, CTRLEXT);

        ctrl_ext_reg |= E1000_CTRL_PHY_RESET_DIR4;

        E1000_WRITE_REG(shared, CTRLEXT, ctrl_ext_reg);

        msec_delay(20);

        /* Set the reset bit in the device control register and clock
         * it out to the PHY.
         */
        ctrl_ext_reg = E1000_READ_REG(shared, CTRLEXT);

        ctrl_ext_reg &= ~E1000_CTRL_PHY_RESET4;

        E1000_WRITE_REG(shared, CTRLEXT, ctrl_ext_reg);

        msec_delay(20);

        ctrl_ext_reg = E1000_READ_REG(shared, CTRLEXT);

        ctrl_ext_reg |= E1000_CTRL_PHY_RESET4;

        E1000_WRITE_REG(shared, CTRLEXT, ctrl_ext_reg);

        msec_delay(20);
    }
    return;
}

/******************************************************************************
* Resets the PHY
*
* shared - Struct containing variables accessed by shared code
*
* Sets bit 15 of the MII Control regiser
******************************************************************************/
boolean_t
em_phy_reset(struct em_shared_adapter *shared)
{
    uint16_t reg_data;
    uint16_t i;

    DEBUGFUNC("em_phy_reset");

    /* Read the MII control register, set the reset bit and write the
     * value back by clocking it out to the PHY.
     */
    reg_data = em_read_phy_reg(shared, PHY_CTRL);

    reg_data |= MII_CR_RESET;

    em_write_phy_reg(shared, PHY_CTRL, reg_data);

    /* Wait for bit 15 of the MII Control Register to be cleared
     * indicating the PHY has been reset.
     */
    i = 0;
    while((reg_data & MII_CR_RESET) && i++ < 500) {
        reg_data = em_read_phy_reg(shared, PHY_CTRL);
        usec_delay(1);
    }

    if(i >= 500) {
        DEBUGOUT("Timeout waiting for PHY to reset.\n");
        return FALSE;
    }
    return TRUE;
}

/******************************************************************************
* Detects which PHY is present and the speed and duplex
*
* shared - Struct containing variables accessed by shared code
* ctrl_reg - current value of the device control register
******************************************************************************/
boolean_t
em_phy_setup(struct em_shared_adapter *shared,
                uint32_t ctrl_reg)
{
    uint16_t mii_ctrl_reg;
    uint16_t mii_status_reg;
    uint16_t phy_specific_ctrl_reg;
    uint16_t mii_autoneg_adv_reg;
    uint16_t mii_1000t_ctrl_reg;
    uint16_t i;
    uint16_t data;
    uint16_t autoneg_hw_setting;
    uint16_t autoneg_fc_setting;
    boolean_t restart_autoneg = FALSE;
    boolean_t force_autoneg_restart = FALSE;

    DEBUGFUNC("em_phy_setup");

    /* We want to enable the Auto-Speed Detection bit in the Device
     * Control Register.  When set to 1, the MAC automatically detects
     * the resolved speed of the link and self-configures appropriately.
     * The Set Link Up bit must also be set for this behavior work
     * properly.
     */
    /* Nothing but 82543 and newer */
    ASSERT(shared->mac_type >= em_82543);

    /* With 82543, we need to force speed/duplex
     * on the MAC equal to what the PHY speed/duplex configuration is.
     * In addition, on 82543, we need to perform a hardware reset
     * on the PHY to take it out of reset.
     */
    if(shared->mac_type >= em_82544) {
        ctrl_reg |= (E1000_CTRL_ASDE | E1000_CTRL_SLU);
        E1000_WRITE_REG(shared, CTRL, ctrl_reg);
    } else {
        ctrl_reg |= (E1000_CTRL_FRCSPD | E1000_CTRL_FRCDPX | E1000_CTRL_SLU);
        E1000_WRITE_REG(shared, CTRL, ctrl_reg);

        if(shared->mac_type == em_82543)
            em_phy_hw_reset(shared);
    }

    if(!em_detect_gig_phy(shared)) {
        /* No PHY detected, return FALSE */
        DEBUGOUT("PhySetup failure, did not detect valid phy.\n");
        return (FALSE);
    }

    DEBUGOUT1("Phy ID = %x \n", shared->phy_id);

    /* Read the MII Control Register. */
    mii_ctrl_reg = em_read_phy_reg(shared, PHY_CTRL);

    DEBUGOUT1("MII Ctrl Reg contents = %x\n", mii_ctrl_reg);

    /* Check to see if the Auto Neg Enable bit is set in the MII Control
     * Register.  If not, we could be in a situation where a driver was
     * loaded previously and was forcing speed and duplex.  Then the
     * driver was unloaded but a em_phy_hw_reset was not performed, so
     * link was still being forced and link was still achieved.  Then
     * the driver was reloaded with the intention to auto-negotiate, but
     * since link is already established we end up not restarting
     * auto-neg.  So if the auto-neg bit is not enabled and the driver
     * is being loaded with the desire to auto-neg, we set this flag to
     * to ensure the restart of the auto-neg engine later in the logic.
     */
    if(!(mii_ctrl_reg & MII_CR_AUTO_NEG_EN))
        force_autoneg_restart = TRUE;

    /* Clear the isolate bit for normal operation and write it back to
     * the MII Control Reg.  Although the spec says this doesn't need
     * to be done when the PHY address is not equal to zero, we do it
     * anyway just to be safe.
     */
    mii_ctrl_reg &= ~(MII_CR_ISOLATE);

    em_write_phy_reg(shared, PHY_CTRL, mii_ctrl_reg);

    data = em_read_phy_reg(shared, M88E1000_PHY_SPEC_CTRL);

    /* Enable CRS on TX.  This must be set for half-duplex operation. */
    data |= M88E1000_PSCR_ASSERT_CRS_ON_TX;

    DEBUGOUT1("M88E1000 PSCR: %x \n", data);

    em_write_phy_reg(shared, M88E1000_PHY_SPEC_CTRL, data);

    data = em_read_phy_reg(shared, M88E1000_EXT_PHY_SPEC_CTRL);

    /* Force TX_CLK in the Extended PHY Specific Control Register
     * to 25MHz clock.
     */
    data |= M88E1000_EPSCR_TX_CLK_25;

    em_write_phy_reg(shared, M88E1000_EXT_PHY_SPEC_CTRL, data);

    /* Certain PHYs will set the default of MII register 4 differently.
     * We need to check this against our fc value.  If it is
     * different, we need to setup up register 4 correctly and restart
     * autonegotiation.
     */
    /* Read the MII Auto-Neg Advertisement Register (Address 4). */
    mii_autoneg_adv_reg = em_read_phy_reg(shared, PHY_AUTONEG_ADV);

    /* Shift left to put 10T-Half bit in bit 0
     * Isolate the four bits for 100/10 Full/Half.
     */ 
    autoneg_hw_setting = (mii_autoneg_adv_reg >> 5) & 0xF;

    /* Get the 1000T settings. */
    mii_1000t_ctrl_reg = em_read_phy_reg(shared, PHY_1000T_CTRL);

    /* Isolate and OR in the 1000T settings. */
    autoneg_hw_setting |= ((mii_1000t_ctrl_reg & 0x0300) >> 4);

    /* mask all bits in the MII Auto-Neg Advertisement Register
     * except for ASM_DIR and PAUSE and shift.  This value
     * will be used later to see if we need to restart Auto-Negotiation.
     */
    autoneg_fc_setting = ((mii_autoneg_adv_reg & 0x0C00) >> 10);

    /* Perform some bounds checking on the shared->autoneg_advertised
     * parameter.  If this variable is zero, then set it to the default.
     */
    shared->autoneg_advertised &= AUTONEG_ADVERTISE_SPEED_DEFAULT;

    /* If autoneg_advertised is zero, we assume it was not defaulted
     * by the calling code so we set to advertise full capability.
     */
    if(shared->autoneg_advertised == 0)
        shared->autoneg_advertised = AUTONEG_ADVERTISE_SPEED_DEFAULT;

    /* We could be in the situation where Auto-Neg has already completed
     * and the user has not indicated any overrides.  In this case we
     * simply need to call em_get_speed_and_duplex to obtain the Auto-
     * Negotiated speed and duplex, then return.
     */
    if(!force_autoneg_restart && shared->autoneg &&
       (shared->autoneg_advertised == autoneg_hw_setting) &&
       (shared->fc == autoneg_fc_setting)) {

        DEBUGOUT("No overrides - Reading MII Status Reg..\n");

        /* Read the MII Status Register.  We read this twice because
         * certain bits are "sticky" and need to be read twice.
         */
        mii_status_reg = em_read_phy_reg(shared, PHY_STATUS);
        mii_status_reg = em_read_phy_reg(shared, PHY_STATUS);

        DEBUGOUT1("MII Status Reg contents = %x\n", mii_status_reg);

        /* Do we have link now? (if so, auto-neg has completed) */
        if(mii_status_reg & MII_SR_LINK_STATUS) {
            data = em_read_phy_reg(shared, M88E1000_PHY_SPEC_STATUS);
            DEBUGOUT1("M88E1000 Phy Specific Status Reg contents = %x\n", data);

            /* We have link, so we need to finish the config process:
             *   1) Set up the MAC to the current PHY speed/duplex
             *      if we are on 82543.  If we
             *      are on newer silicon, we only need to configure
             *      collision distance in the Transmit Control Register.
             *   2) Set up flow control on the MAC to that established
             *      with the link partner.
             */
            if(shared->mac_type >= em_82544)
                em_config_collision_dist(shared);
            else
                em_config_mac_to_phy(shared, data);

            em_config_fc_after_link_up(shared);

            return (TRUE);
        }
    }

    /* Options:
     *   MDI/MDI-X = 0 (default)
     *   0 - Auto for all speeds
     *   1 - MDI mode
     *   2 - MDI-X mode
     *   3 - Auto for 1000Base-T only (MDI-X for 10/100Base-T modes)
     */
    phy_specific_ctrl_reg = em_read_phy_reg(shared, M88E1000_PHY_SPEC_CTRL);

    phy_specific_ctrl_reg &= ~M88E1000_PSCR_AUTO_X_MODE;

    switch (shared->mdix) {
    case 1:
        phy_specific_ctrl_reg |= M88E1000_PSCR_MDI_MANUAL_MODE;
        break;
    case 2:
        phy_specific_ctrl_reg |= M88E1000_PSCR_MDIX_MANUAL_MODE;
        break;
    case 3:
        phy_specific_ctrl_reg |= M88E1000_PSCR_AUTO_X_1000T;
        break;
    case 0:
    default:
        phy_specific_ctrl_reg |= M88E1000_PSCR_AUTO_X_MODE;
        break;
    }

    em_write_phy_reg(shared, M88E1000_PHY_SPEC_CTRL, phy_specific_ctrl_reg);

    /* Options:
     *   disable_polarity_correction = 0 (default)
     *       Automatic Correction for Reversed Cable Polarity
     *   0 - Disabled
     *   1 - Enabled
     */
    phy_specific_ctrl_reg = em_read_phy_reg(shared, M88E1000_PHY_SPEC_CTRL);

    phy_specific_ctrl_reg &= ~M88E1000_PSCR_POLARITY_REVERSAL;

    if(shared->disable_polarity_correction == 1)
        phy_specific_ctrl_reg |= M88E1000_PSCR_POLARITY_REVERSAL;

    em_write_phy_reg(shared, M88E1000_PHY_SPEC_CTRL, phy_specific_ctrl_reg);

    /* Options:
     *   autoneg = 1 (default)
     *      PHY will advertise value(s) parsed from
     *      autoneg_advertised and fc
     *   autoneg = 0
     *      PHY will be set to 10H, 10F, 100H, or 100F
     *      depending on value parsed from forced_speed_duplex.
     */

    /* Is AutoNeg enabled?  This is enabled by default or by software
     * override.  If so,
     * call PhySetupAutoNegAdvertisement routine to parse the
     * autoneg_advertised and fc options.
     * If AutoNeg is NOT enabled, then the user should have provided
     * a Speed/Duplex override.  If so, then call the
     * PhyForceSpeedAndDuplex to parse and set this up.  Otherwise,
     * we are in an error situation and need to bail.
     */
    if(shared->autoneg) {
        DEBUGOUT("Reconfiguring auto-neg advertisement params\n");
        restart_autoneg = em_phy_setup_autoneg(shared);
    } else {
        DEBUGOUT("Forcing speed and duplex\n");
        em_phy_force_speed_duplex(shared);
    }

    /* Based on information parsed above, check the flag to indicate
     * whether we need to restart Auto-Neg.
     */
    if(restart_autoneg) {
        DEBUGOUT("Restarting Auto-Neg\n");

        /* Read the MII Control Register. */
        mii_ctrl_reg = em_read_phy_reg(shared, PHY_CTRL);

        /* Restart auto-negotiation by setting the Auto Neg Enable bit and
         * the Auto Neg Restart bit.
         */
        mii_ctrl_reg |= (MII_CR_AUTO_NEG_EN | MII_CR_RESTART_AUTO_NEG);

        em_write_phy_reg(shared, PHY_CTRL, mii_ctrl_reg);

        /* Does the user want to wait for Auto-Neg to complete here, or
         * check at a later time (for example, callback routine).
         */
        if(shared->wait_autoneg_complete)
            em_wait_autoneg(shared);
    } /* end if restart_autoneg */

    /* Read the MII Status Register. */
    mii_status_reg = em_read_phy_reg(shared, PHY_STATUS);
    mii_status_reg = em_read_phy_reg(shared, PHY_STATUS);

    DEBUGOUT1("Checking for link status - MII Status Reg contents = %x\n",
              mii_status_reg);

    /* Check link status.  Wait up to 100 microseconds for link to
     * become valid.
     */
    for(i = 0; i < 10; i++) {
        if(mii_status_reg & MII_SR_LINK_STATUS)
            break;
        usec_delay(10);
        DEBUGOUT(". ");

        mii_status_reg = em_read_phy_reg(shared, PHY_STATUS);
        mii_status_reg = em_read_phy_reg(shared, PHY_STATUS);
    }

    if(mii_status_reg & MII_SR_LINK_STATUS) {
        /* Yes, so configure MAC to PHY settings as well as flow control
         * registers.
         */
        data = em_read_phy_reg(shared, M88E1000_PHY_SPEC_STATUS);

        DEBUGOUT1("M88E1000 Phy Specific Status Reg contents = %x\n", data);

        /* We have link, so we need to finish the config process:
         *   1) Set up the MAC to the current PHY speed/duplex
         *      if we are on 82543.  If we
         *      are on newer silicon, we only need to configure
         *      collision distance in the Transmit Control Register.
         *   2) Set up flow control on the MAC to that established with
         *      the link partner.
         */
        if(shared->mac_type >= em_82544)
            em_config_collision_dist(shared);
        else
            em_config_mac_to_phy(shared, data);

        em_config_fc_after_link_up(shared);

        DEBUGOUT("Valid link established!!!\n");
    } else {
        DEBUGOUT("Unable to establish link!!!\n");
    }

    return (TRUE);
}

/******************************************************************************
* Configures PHY autoneg and flow control advertisement settings
*
* shared - Struct containing variables accessed by shared code
******************************************************************************/
boolean_t
em_phy_setup_autoneg(struct em_shared_adapter *shared)
{
    uint16_t mii_autoneg_adv_reg;
    uint16_t mii_1000t_ctrl_reg;

    DEBUGFUNC("em_phy_setup_autoneg");

    /* Read the MII Auto-Neg Advertisement Register (Address 4). */
    mii_autoneg_adv_reg = em_read_phy_reg(shared, PHY_AUTONEG_ADV);

    /* Read the MII 1000Base-T Control Register (Address 9). */
    mii_1000t_ctrl_reg = em_read_phy_reg(shared, PHY_1000T_CTRL);

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

    DEBUGOUT1("autoneg_advertised %x\n", shared->autoneg_advertised);

    /* Do we want to advertise 10 Mb Half Duplex? */
    if(shared->autoneg_advertised & ADVERTISE_10_HALF) {
        DEBUGOUT("Advertise 10mb Half duplex\n");
        mii_autoneg_adv_reg |= NWAY_AR_10T_HD_CAPS;
    }

    /* Do we want to advertise 10 Mb Full Duplex? */
    if(shared->autoneg_advertised & ADVERTISE_10_FULL) {
        DEBUGOUT("Advertise 10mb Full duplex\n");
        mii_autoneg_adv_reg |= NWAY_AR_10T_FD_CAPS;
    }

    /* Do we want to advertise 100 Mb Half Duplex? */
    if(shared->autoneg_advertised & ADVERTISE_100_HALF) {
        DEBUGOUT("Advertise 100mb Half duplex\n");
        mii_autoneg_adv_reg |= NWAY_AR_100TX_HD_CAPS;
    }

    /* Do we want to advertise 100 Mb Full Duplex? */
    if(shared->autoneg_advertised & ADVERTISE_100_FULL) {
        DEBUGOUT("Advertise 100mb Full duplex\n");
        mii_autoneg_adv_reg |= NWAY_AR_100TX_FD_CAPS;
    }

    /* We do not allow the Phy to advertise 1000 Mb Half Duplex */
    if(shared->autoneg_advertised & ADVERTISE_1000_HALF) {
        DEBUGOUT("Advertise 1000mb Half duplex requested, request denied!\n");
    }

    /* Do we want to advertise 1000 Mb Full Duplex? */
    if(shared->autoneg_advertised & ADVERTISE_1000_FULL) {
        DEBUGOUT("Advertise 1000mb Full duplex\n");
        mii_1000t_ctrl_reg |= CR_1000T_FD_CAPS;
    }

    /* Check for a software override of the flow control settings, and
     * setup the PHY advertisement registers accordingly.  If
     * auto-negotiation is enabled, then software will have to set the
     * "PAUSE" bits to the correct value in the Auto-Negotiation
     * Advertisement Register (PHY_AUTONEG_ADVERTISEMENT) and re-start
     * auto-negotiation.
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
        mii_autoneg_adv_reg &= ~(NWAY_AR_ASM_DIR | NWAY_AR_PAUSE);
        break;
    case em_fc_rx_pause:    /* 1 */
            /* RX Flow control is enabled, and TX Flow control is
             * disabled, by a software over-ride.
             */

            /* Since there really isn't a way to advertise that we are
             * capable of RX Pause ONLY, we will advertise that we
             * support both symmetric and asymmetric RX PAUSE.  Later
             * (in em_config_fc_after_link_up) we will disable the
             *shared's ability to send PAUSE frames.
             */
        mii_autoneg_adv_reg |= (NWAY_AR_ASM_DIR | NWAY_AR_PAUSE);
        break;
    case em_fc_tx_pause:    /* 2 */
            /* TX Flow control is enabled, and RX Flow control is
             * disabled, by a software over-ride.
             */
        mii_autoneg_adv_reg |= NWAY_AR_ASM_DIR;
        mii_autoneg_adv_reg &= ~NWAY_AR_PAUSE;
        break;
    case em_fc_full:        /* 3 */
            /* Flow control (both RX and TX) is enabled by a software
             * over-ride.
             */
        mii_autoneg_adv_reg |= (NWAY_AR_ASM_DIR | NWAY_AR_PAUSE);
        break;
    default:
            /* We should never get here.  The value should be 0-3. */
        DEBUGOUT("Flow control param set incorrectly\n");
        ASSERT(0);
        break;
    }

    /* Write the MII Auto-Neg Advertisement Register (Address 4). */
    em_write_phy_reg(shared, PHY_AUTONEG_ADV, mii_autoneg_adv_reg);

    DEBUGOUT1("Auto-Neg Advertising %x\n", mii_autoneg_adv_reg);

    /* Write the MII 1000Base-T Control Register (Address 9). */
    em_write_phy_reg(shared, PHY_1000T_CTRL, mii_1000t_ctrl_reg);
    return (TRUE);
}

/******************************************************************************
* Sets MAC speed and duplex settings to reflect the those in the PHY
*
* shared - Struct containing variables accessed by shared code
* mii_reg - data to write to the MII control register
*
* The contents of the PHY register containing the needed information need to
* be passed in.
******************************************************************************/
void
em_config_mac_to_phy(struct em_shared_adapter *shared,
                        uint16_t mii_reg)
{
    uint32_t ctrl_reg;
    uint32_t tctl_reg;
    uint32_t shift;

    DEBUGFUNC("em_config_mac_to_phy");

    /* We need to read the Transmit Control register to configure the
     * collision distance.
     * Note: This must be done for both Half or Full Duplex.
     */
    tctl_reg = E1000_READ_REG(shared, TCTL);
    DEBUGOUT1("tctl_reg = %x\n", tctl_reg);

    /* Read the Device Control Register and set the bits to Force Speed
     * and Duplex.
     */
    ctrl_reg = E1000_READ_REG(shared, CTRL);

    ctrl_reg |= (E1000_CTRL_FRCSPD | E1000_CTRL_FRCDPX);
    ctrl_reg &= ~(DEVICE_SPEED_MASK);

    DEBUGOUT1("MII Register Data = %x\r\n", mii_reg);

    /* Clear the ILOS bit. */
    ctrl_reg &= ~E1000_CTRL_ILOS;

    /* Set up duplex in the Device Control and Transmit Control
     * registers depending on negotiated values.
     */
    if(mii_reg & M88E1000_PSSR_DPLX) {
        ctrl_reg |= E1000_CTRL_FD;

        /* We are in Full Duplex mode.  We have the same collision
         * distance regardless of speed.
         */
        tctl_reg &= ~E1000_TCTL_COLD;
        shift = E1000_FDX_COLLISION_DISTANCE;
        shift <<= E1000_COLD_SHIFT;
        tctl_reg |= shift;
    } else {
        ctrl_reg &= ~E1000_CTRL_FD;

        /* We are in Half Duplex mode.  Our Half Duplex collision
         * distance is different for Gigabit than for 10/100 so we will
         * set accordingly.
         */
        if((mii_reg & M88E1000_PSSR_SPEED) == M88E1000_PSSR_1000MBS) { 
            /* 1000Mbs HDX */
            tctl_reg &= ~E1000_TCTL_COLD;
            shift = E1000_GB_HDX_COLLISION_DISTANCE;
            shift <<= E1000_COLD_SHIFT;
            tctl_reg |= shift;
            tctl_reg |= E1000_TCTL_PBE; /* Enable Packet Bursting */
        } else {
            /* 10/100Mbs HDX */
            tctl_reg &= ~E1000_TCTL_COLD;
            shift = E1000_HDX_COLLISION_DISTANCE;
            shift <<= E1000_COLD_SHIFT;
            tctl_reg |= shift;
        }
    }

    /* Set up speed in the Device Control register depending on
     * negotiated values.
     */
    if((mii_reg & M88E1000_PSSR_SPEED) == M88E1000_PSSR_1000MBS)
        ctrl_reg |= E1000_CTRL_SPD_1000;
    else if((mii_reg & M88E1000_PSSR_SPEED) == M88E1000_PSSR_100MBS)
        ctrl_reg |= E1000_CTRL_SPD_100;
    else
        ctrl_reg &= ~(E1000_CTRL_SPD_1000 | E1000_CTRL_SPD_100);

    /* Write the configured values back to the Transmit Control Reg. */
    E1000_WRITE_REG(shared, TCTL, tctl_reg);

    /* Write the configured values back to the Device Control Reg. */
    E1000_WRITE_REG(shared, CTRL, ctrl_reg);

    return;
}

/******************************************************************************
* Sets the collision distance in the Transmit Control register
*
* shared - Struct containing variables accessed by shared code
*
* Link should have been established previously. Reads the speed and duplex
* information from the Device Status register.
******************************************************************************/
void
em_config_collision_dist(struct em_shared_adapter *shared)
{
    uint32_t tctl_reg;
    uint16_t speed;
    uint16_t duplex;
    uint32_t shift;

    DEBUGFUNC("em_config_collision_dist");

    /* Get our current speed and duplex from the Device Status Register. */
    em_get_speed_and_duplex(shared, &speed, &duplex);

    /* We need to configure the Collision Distance for both Full or
     * Half Duplex.
     */
    tctl_reg = E1000_READ_REG(shared, TCTL);
    DEBUGOUT1("tctl_reg = %x\n", tctl_reg);

    /* mask the Collision Distance bits in the Transmit Control Reg. */
    tctl_reg &= ~E1000_TCTL_COLD;

    if(duplex == FULL_DUPLEX) {
        /* We are in Full Duplex mode.  Therefore, the collision distance
         * is the same regardless of speed.
         */
        shift = E1000_FDX_COLLISION_DISTANCE;
        shift <<= E1000_COLD_SHIFT;
        tctl_reg |= shift;
    } else {
        /* We are in Half Duplex mode.  Half Duplex collision distance is
         * different for Gigabit vs. 10/100, so we will set accordingly.
         */
        if(speed == SPEED_1000) {       /* 1000Mbs HDX */
            shift = E1000_GB_HDX_COLLISION_DISTANCE;
            shift <<= E1000_COLD_SHIFT;
            tctl_reg |= shift;
            tctl_reg |= E1000_TCTL_PBE; /* Enable Packet Bursting */
        } else {                /* 10/100Mbs HDX */
            shift = E1000_HDX_COLLISION_DISTANCE;
            shift <<= E1000_COLD_SHIFT;
            tctl_reg |= shift;
        }
    }

    /* Write the configured values back to the Transmit Control Reg. */
    E1000_WRITE_REG(shared, TCTL, tctl_reg);

    return;
}

#if DBG
/******************************************************************************
* Displays the contents of all of the MII registers
*
* shared - Struct containing variables accessed by shared code
*
* For debugging.
******************************************************************************/
void
em_display_mii(struct em_shared_adapter *shared)
{
    uint16_t data;
    uint16_t phy_id_high;
    uint16_t phy_id_low;
    uint32_t phy_id;

    DEBUGFUNC("em_display_mii");

    DEBUGOUT1("adapter Base Address = %p\n", shared->hw_addr);

    /* This will read each PHY Reg address and display its contents. */

    data = em_read_phy_reg(shared, PHY_CTRL);
    DEBUGOUT1("MII Ctrl Reg contents = %x\n", data);

    data = em_read_phy_reg(shared, PHY_STATUS);
    data = em_read_phy_reg(shared, PHY_STATUS);
    DEBUGOUT1("MII Status Reg contents = %x\n", data);

    phy_id_high = em_read_phy_reg(shared, PHY_ID1);
    usec_delay(2);
    phy_id_low = em_read_phy_reg(shared, PHY_ID2);
    phy_id = (phy_id_low | (phy_id_high << 16)) & PHY_REVISION_MASK;
    DEBUGOUT1("Phy ID = %x \n", phy_id);

    data = em_read_phy_reg(shared, PHY_AUTONEG_ADV);
    DEBUGOUT1("Reg 4 contents = %x\n", data);

    data = em_read_phy_reg(shared, PHY_LP_ABILITY);
    DEBUGOUT1("Reg 5 contents = %x\n", data);

    data = em_read_phy_reg(shared, PHY_AUTONEG_EXP);
    DEBUGOUT1("Reg 6 contents = %x\n", data);

    data = em_read_phy_reg(shared, PHY_NEXT_PAGE_TX);
    DEBUGOUT1("Reg 7 contents = %x\n", data);

    data = em_read_phy_reg(shared, PHY_LP_NEXT_PAGE);
    DEBUGOUT1("Reg 8 contents = %x\n", data);

    data = em_read_phy_reg(shared, PHY_1000T_CTRL);
    DEBUGOUT1("Reg 9 contents = %x\n", data);

    data = em_read_phy_reg(shared, PHY_1000T_STATUS);
    DEBUGOUT1("Reg A contents = %x\n", data);

    data = em_read_phy_reg(shared, PHY_EXT_STATUS);
    DEBUGOUT1("Reg F contents = %x\n", data);

    data = em_read_phy_reg(shared, M88E1000_PHY_SPEC_CTRL);
    DEBUGOUT1("M88E1000 Specific Control Reg (0x10) = %x\n", data);

    data = em_read_phy_reg(shared, M88E1000_PHY_SPEC_STATUS);
    DEBUGOUT1("M88E1000 Specific Status Reg (0x11) = %x\n", data);

    /*
     * data = em_read_phy_reg(shared, M88E1000_INT_ENABLE_REG);
     * DEBUGOUT1("M88E1000 Interrupt Enable Reg (0x12) = %x\n", data);
     */

    /*
     * data = em_read_phy_reg(shared, M88E1000_INT_STATUS_REG);
     * DEBUGOUT1("M88E1000 Interrupt Status Reg (0x13) = %x\n", data);
     */
     
    data = em_read_phy_reg(shared, M88E1000_EXT_PHY_SPEC_CTRL);
    DEBUGOUT1("M88E1000 Ext. Phy Specific Control (0x14) = %x\n", data);

    data = em_read_phy_reg(shared, M88E1000_RX_ERR_CNTR);
    DEBUGOUT1("M88E1000 Receive Error Counter (0x15) = %x\n", data);

    /*
     * data = em_read_phy_reg(shared, M88E1000_LED_CTRL_REG);
     * DEBUGOUT1("M88E1000 LED control reg (0x18) = %x\n", data);
     */

    return;
}
#endif // DBG

/******************************************************************************
* Probes the expected PHY address for known PHY IDs
*
* shared - Struct containing variables accessed by shared code
******************************************************************************/
boolean_t
em_detect_gig_phy(struct em_shared_adapter *shared)
{
    uint32_t phy_id_high;
    uint16_t phy_id_low;

    DEBUGFUNC("em_detect_gig_phy");

    /* Read the PHY ID Registers to identify which PHY is onboard. */
    shared->phy_addr = 1;

    phy_id_high = em_read_phy_reg(shared, PHY_ID1);

    usec_delay(2);

    phy_id_low = em_read_phy_reg(shared, PHY_ID2);

    shared->phy_id = (phy_id_low | (phy_id_high << 16)) & PHY_REVISION_MASK;

    if(shared->phy_id == M88E1000_12_PHY_ID ||
       shared->phy_id == M88E1000_14_PHY_ID ||
       shared->phy_id == M88E1000_I_PHY_ID) {

        DEBUGOUT2("phy_id 0x%x detected at address 0x%x\n",
                  shared->phy_id, shared->phy_addr);
        return (TRUE);
    } else {
        DEBUGOUT("Could not auto-detect Phy!\n");
        return (FALSE);
    }
}

/******************************************************************************
* Resets the PHY's DSP
*
* shared - Struct containing variables accessed by shared code
******************************************************************************/
void
em_phy_reset_dsp(struct em_shared_adapter *shared)
{
    em_write_phy_reg(shared, 29, 0x1d);
    em_write_phy_reg(shared, 30, 0xc1);
    em_write_phy_reg(shared, 30, 0x00);
    return;
}

/******************************************************************************
* Blocks until autoneg completes or times out (~4.5 seconds)
*
* shared - Struct containing variables accessed by shared code
******************************************************************************/
boolean_t
em_wait_autoneg(struct em_shared_adapter *shared)
{
    uint16_t i;
    uint16_t mii_status_reg;
    boolean_t autoneg_complete = FALSE;

    DEBUGFUNC("em_wait_autoneg");

    /* We will wait for AutoNeg to complete. */
    DEBUGOUT("Waiting for Auto-Neg to complete.\n");
    mii_status_reg = 0;

    /* We will wait for AutoNeg to complete or 4.5 seconds to expire. */

    for(i = PHY_AUTO_NEG_TIME; i > 0; i--) {
        /* Read the MII Status Register and wait for Auto-Neg
         * Complete bit to be set.
         */
        mii_status_reg = em_read_phy_reg(shared, PHY_STATUS);
        mii_status_reg = em_read_phy_reg(shared, PHY_STATUS);

        if(mii_status_reg & MII_SR_AUTONEG_COMPLETE) {
            autoneg_complete = TRUE;
            break;
        }

        msec_delay(100);
    }

    return (autoneg_complete);
}

/******************************************************************************
* Get PHY information from various PHY registers
*
* shared - Struct containing variables accessed by shared code
* phy_status_info - PHY information structure
******************************************************************************/
boolean_t
em_phy_get_info(struct em_shared_adapter *shared,
                   struct em_phy_info *phy_status_info)
{
    uint16_t phy_mii_shatus_reg;
    uint16_t phy_specific_ctrl_reg;
    uint16_t phy_specific_status_reg;
    uint16_t phy_specific_ext_ctrl_reg;
    uint16_t phy_1000t_stat_reg;

    phy_status_info->cable_length = em_cable_length_undefined;
    phy_status_info->extended_10bt_distance =
        em_10bt_ext_dist_enable_undefined;
    phy_status_info->cable_polarity = em_rev_polarity_undefined;
    phy_status_info->polarity_correction = em_polarity_reversal_undefined;
    phy_status_info->link_reset = em_down_no_idle_undefined;
    phy_status_info->mdix_mode = em_auto_x_mode_undefined;
    phy_status_info->local_rx = em_1000t_rx_status_undefined;
    phy_status_info->remote_rx = em_1000t_rx_status_undefined;

    /* PHY info only valid for copper media. */
    if(shared == NULL || shared->media_type != em_media_type_copper)
        return FALSE;

    /* PHY info only valid for LINK UP.  Read MII status reg 
     * back-to-back to get link status.
     */
    phy_mii_shatus_reg = em_read_phy_reg(shared, PHY_STATUS);
    phy_mii_shatus_reg = em_read_phy_reg(shared, PHY_STATUS);
    if((phy_mii_shatus_reg & MII_SR_LINK_STATUS) != MII_SR_LINK_STATUS)
        return FALSE;

    /* Read various PHY registers to get the PHY info. */
    phy_specific_ctrl_reg = em_read_phy_reg(shared, M88E1000_PHY_SPEC_CTRL);
    phy_specific_status_reg =
        em_read_phy_reg(shared, M88E1000_PHY_SPEC_STATUS);
    phy_specific_ext_ctrl_reg =
        em_read_phy_reg(shared, M88E1000_EXT_PHY_SPEC_CTRL);
    phy_1000t_stat_reg = em_read_phy_reg(shared, PHY_1000T_STATUS);

    phy_status_info->cable_length =
        ((phy_specific_status_reg & M88E1000_PSSR_CABLE_LENGTH) >>
         M88E1000_PSSR_CABLE_LENGTH_SHIFT);

    phy_status_info->extended_10bt_distance =
        (phy_specific_ctrl_reg & M88E1000_PSCR_10BT_EXT_DIST_ENABLE) >>
        M88E1000_PSCR_10BT_EXT_DIST_ENABLE_SHIFT;

    phy_status_info->cable_polarity =
        (phy_specific_status_reg & M88E1000_PSSR_REV_POLARITY) >>
        M88E1000_PSSR_REV_POLARITY_SHIFT;

    phy_status_info->polarity_correction =
        (phy_specific_ctrl_reg & M88E1000_PSCR_POLARITY_REVERSAL) >>
        M88E1000_PSCR_POLARITY_REVERSAL_SHIFT;

    phy_status_info->link_reset =
        (phy_specific_ext_ctrl_reg & M88E1000_EPSCR_DOWN_NO_IDLE) >>
        M88E1000_EPSCR_DOWN_NO_IDLE_SHIFT;

    phy_status_info->mdix_mode =
        (phy_specific_status_reg & M88E1000_PSSR_MDIX) >>
        M88E1000_PSSR_MDIX_SHIFT;

    phy_status_info->local_rx =
        (phy_1000t_stat_reg & SR_1000T_LOCAL_RX_STATUS) >>
        SR_1000T_LOCAL_RX_STATUS_SHIFT;

    phy_status_info->remote_rx =
        (phy_1000t_stat_reg & SR_1000T_REMOTE_RX_STATUS) >>
        SR_1000T_REMOTE_RX_STATUS_SHIFT;

    return TRUE;
}

boolean_t
em_validate_mdi_setting(struct em_shared_adapter *shared)
{
    if(!shared->autoneg && (shared->mdix == 0 || shared->mdix == 3)) {
        shared->mdix = 1;
        return FALSE;
    }
    return TRUE;
}
