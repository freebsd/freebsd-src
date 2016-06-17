
/******************************************************************************
 *
 * Module Name: hwgpe - Low level GPE enable/disable/clear functions
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2004, R. Byron Moore
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 */

#include <acpi/acpi.h>
#include <acpi/acevents.h>

#define _COMPONENT          ACPI_HARDWARE
	 ACPI_MODULE_NAME    ("hwgpe")


/******************************************************************************
 *
 * FUNCTION:    acpi_hw_enable_gpe
 *
 * PARAMETERS:  gpe_event_info      - Info block for the GPE to be enabled
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Enable a single GPE.
 *
 ******************************************************************************/

acpi_status
acpi_hw_enable_gpe (
	struct acpi_gpe_event_info      *gpe_event_info)
{
	u32                             in_byte;
	acpi_status                     status;


	ACPI_FUNCTION_ENTRY ();


	/*
	 * Read the current value of the register, set the appropriate bit
	 * to enable the GPE, and write out the new register.
	 */
	status = acpi_hw_low_level_read (8, &in_byte,
			  &gpe_event_info->register_info->enable_address);
	if (ACPI_FAILURE (status)) {
		return (status);
	}

	/* Write with the new GPE bit enabled */

	status = acpi_hw_low_level_write (8, (in_byte | gpe_event_info->bit_mask),
			  &gpe_event_info->register_info->enable_address);

	return (status);
}


/******************************************************************************
 *
 * FUNCTION:    acpi_hw_enable_gpe_for_wakeup
 *
 * PARAMETERS:  gpe_event_info      - Info block for the GPE to be enabled
 *
 * RETURN:      None
 *
 * DESCRIPTION: Keep track of which GPEs the OS has requested not be
 *              disabled when going to sleep.
 *
 ******************************************************************************/

void
acpi_hw_enable_gpe_for_wakeup (
	struct acpi_gpe_event_info      *gpe_event_info)
{
	struct acpi_gpe_register_info   *gpe_register_info;


	ACPI_FUNCTION_ENTRY ();


	/* Get the info block for the entire GPE register */

	gpe_register_info = gpe_event_info->register_info;
	if (!gpe_register_info) {
		return;
	}

	/*
	 * Set the bit so we will not enable this GPE when sleeping (and disable
	 * it upon wake)
	 */
	gpe_register_info->wake_enable |= gpe_event_info->bit_mask;
	gpe_event_info->flags |= (ACPI_GPE_TYPE_WAKE | ACPI_GPE_ENABLED);
}


/******************************************************************************
 *
 * FUNCTION:    acpi_hw_disable_gpe
 *
 * PARAMETERS:  gpe_event_info      - Info block for the GPE to be disabled
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Disable a single GPE.
 *
 ******************************************************************************/

acpi_status
acpi_hw_disable_gpe (
	struct acpi_gpe_event_info      *gpe_event_info)
{
	u32                             in_byte;
	acpi_status                     status;
	struct acpi_gpe_register_info   *gpe_register_info;


	ACPI_FUNCTION_ENTRY ();


	/* Get the info block for the entire GPE register */

	gpe_register_info = gpe_event_info->register_info;
	if (!gpe_register_info) {
		return (AE_BAD_PARAMETER);
	}

	/*
	 * Read the current value of the register, clear the appropriate bit,
	 * and write out the new register value to disable the GPE.
	 */
	status = acpi_hw_low_level_read (8, &in_byte,
			  &gpe_register_info->enable_address);
	if (ACPI_FAILURE (status)) {
		return (status);
	}

	/* Write the byte with this GPE bit cleared */

	status = acpi_hw_low_level_write (8, (in_byte & ~(gpe_event_info->bit_mask)),
			  &gpe_register_info->enable_address);
	if (ACPI_FAILURE (status)) {
		return (status);
	}

	/* Make sure this GPE is disabled for wake, also */

	acpi_hw_disable_gpe_for_wakeup (gpe_event_info);
	return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    acpi_hw_disable_gpe_for_wakeup
 *
 * PARAMETERS:  gpe_event_info      - Info block for the GPE to be disabled
 *
 * RETURN:      None
 *
 * DESCRIPTION: Keep track of which GPEs the OS has requested not be
 *              disabled when going to sleep.
 *
 ******************************************************************************/

void
acpi_hw_disable_gpe_for_wakeup (
	struct acpi_gpe_event_info      *gpe_event_info)
{
	struct acpi_gpe_register_info   *gpe_register_info;


	ACPI_FUNCTION_ENTRY ();


	/* Get the info block for the entire GPE register */

	gpe_register_info = gpe_event_info->register_info;
	if (!gpe_register_info) {
		return;
	}

	/* Clear the bit so we will disable this when sleeping */

	gpe_register_info->wake_enable &= ~(gpe_event_info->bit_mask);
}


/******************************************************************************
 *
 * FUNCTION:    acpi_hw_clear_gpe
 *
 * PARAMETERS:  gpe_event_info      - Info block for the GPE to be cleared
 *
 * RETURN:      status_status
 *
 * DESCRIPTION: Clear the status bit for a single GPE.
 *
 ******************************************************************************/

acpi_status
acpi_hw_clear_gpe (
	struct acpi_gpe_event_info      *gpe_event_info)
{
	acpi_status                     status;


	ACPI_FUNCTION_ENTRY ();


	/*
	 * Write a one to the appropriate bit in the status register to
	 * clear this GPE.
	 */
	status = acpi_hw_low_level_write (8, gpe_event_info->bit_mask,
			  &gpe_event_info->register_info->status_address);

	return (status);
}


/******************************************************************************
 *
 * FUNCTION:    acpi_hw_get_gpe_status
 *
 * PARAMETERS:  gpe_event_info      - Info block for the GPE to queried
 *              event_status        - Where the GPE status is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Return the status of a single GPE.
 *
 ******************************************************************************/

acpi_status
acpi_hw_get_gpe_status (
	struct acpi_gpe_event_info      *gpe_event_info,
	acpi_event_status               *event_status)
{
	u32                             in_byte;
	u8                              bit_mask;
	struct acpi_gpe_register_info   *gpe_register_info;
	acpi_status                     status;
	acpi_event_status               local_event_status = 0;


	ACPI_FUNCTION_ENTRY ();


	if (!event_status) {
		return (AE_BAD_PARAMETER);
	}

	/* Get the info block for the entire GPE register */

	gpe_register_info = gpe_event_info->register_info;

	/* Get the register bitmask for this GPE */

	bit_mask = gpe_event_info->bit_mask;

	/* GPE Enabled? */

	status = acpi_hw_low_level_read (8, &in_byte, &gpe_register_info->enable_address);
	if (ACPI_FAILURE (status)) {
		goto unlock_and_exit;
	}

	if (bit_mask & in_byte) {
		local_event_status |= ACPI_EVENT_FLAG_ENABLED;
	}

	/* GPE Enabled for wake? */

	if (bit_mask & gpe_register_info->wake_enable) {
		local_event_status |= ACPI_EVENT_FLAG_WAKE_ENABLED;
	}

	/* GPE active (set)? */

	status = acpi_hw_low_level_read (8, &in_byte, &gpe_register_info->status_address);
	if (ACPI_FAILURE (status)) {
		goto unlock_and_exit;
	}

	if (bit_mask & in_byte) {
		local_event_status |= ACPI_EVENT_FLAG_SET;
	}

	/* Set return value */

	(*event_status) = local_event_status;


unlock_and_exit:
	return (status);
}


/******************************************************************************
 *
 * FUNCTION:    acpi_hw_disable_gpe_block
 *
 * PARAMETERS:  gpe_xrupt_info      - GPE Interrupt info
 *              gpe_block           - Gpe Block info
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Disable all GPEs within a GPE block
 *
 ******************************************************************************/

acpi_status
acpi_hw_disable_gpe_block (
	struct acpi_gpe_xrupt_info      *gpe_xrupt_info,
	struct acpi_gpe_block_info      *gpe_block)
{
	u32                             i;
	acpi_status                     status;


	/* Examine each GPE Register within the block */

	for (i = 0; i < gpe_block->register_count; i++) {
		/* Disable all GPEs in this register */

		status = acpi_hw_low_level_write (8, 0x00,
				 &gpe_block->register_info[i].enable_address);
		if (ACPI_FAILURE (status)) {
			return (status);
		}
	}

	return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    acpi_hw_clear_gpe_block
 *
 * PARAMETERS:  gpe_xrupt_info      - GPE Interrupt info
 *              gpe_block           - Gpe Block info
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Clear status bits for all GPEs within a GPE block
 *
 ******************************************************************************/

acpi_status
acpi_hw_clear_gpe_block (
	struct acpi_gpe_xrupt_info      *gpe_xrupt_info,
	struct acpi_gpe_block_info      *gpe_block)
{
	u32                             i;
	acpi_status                     status;


	/* Examine each GPE Register within the block */

	for (i = 0; i < gpe_block->register_count; i++) {
		/* Clear status on all GPEs in this register */

		status = acpi_hw_low_level_write (8, 0xFF,
				 &gpe_block->register_info[i].status_address);
		if (ACPI_FAILURE (status)) {
			return (status);
		}
	}

	return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    acpi_hw_prepare_gpe_block_for_sleep
 *
 * PARAMETERS:  gpe_xrupt_info      - GPE Interrupt info
 *              gpe_block           - Gpe Block info
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Disable all runtime GPEs and enable all wakeup GPEs -- within
 *              a single GPE block
 *
 ******************************************************************************/

static acpi_status
acpi_hw_prepare_gpe_block_for_sleep (
	struct acpi_gpe_xrupt_info      *gpe_xrupt_info,
	struct acpi_gpe_block_info      *gpe_block)
{
	u32                             i;
	struct acpi_gpe_register_info   *gpe_register_info;
	u32                             in_value;
	acpi_status                     status;


	/* Get the register info for the entire GPE block */

	gpe_register_info = gpe_block->register_info;

	/* Examine each GPE Register within the block */

	for (i = 0; i < gpe_block->register_count; i++) {
		/*
		 * Read the enabled/disabled status of all GPEs. We
		 * will be using it to restore all the GPEs later.
		 *
		 * NOTE:  Wake GPEs are are ALL disabled at this time, so when we wake
		 * and restore this register, they will be automatically disabled.
		 */
		status = acpi_hw_low_level_read (8, &in_value,
				 &gpe_register_info->enable_address);
		if (ACPI_FAILURE (status)) {
			return (status);
		}

		gpe_register_info->enable = (u8) in_value;

		/*
		 * 1) Disable all runtime GPEs
		 * 2) Enable all wakeup GPEs
		 */
		status = acpi_hw_low_level_write (8, gpe_register_info->wake_enable,
				&gpe_register_info->enable_address);
		if (ACPI_FAILURE (status)) {
			return (status);
		}

		/* Point to next GPE register */

		gpe_register_info++;
	}

	return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    acpi_hw_prepare_gpes_for_sleep
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Disable all runtime GPEs, enable all wake GPEs.
 *              Called with interrupts disabled. The interrupt handler also
 *              modifies gpe_register_info->Enable, so it should not be
 *              given the chance to run until after the runtime GPEs are
 *              re-enabled.
 *
 ******************************************************************************/

acpi_status
acpi_hw_prepare_gpes_for_sleep (
	void)
{
	acpi_status                     status;


	ACPI_FUNCTION_ENTRY ();


	status = acpi_ev_walk_gpe_list (acpi_hw_prepare_gpe_block_for_sleep);
	return (status);
}


/******************************************************************************
 *
 * FUNCTION:    acpi_hw_restore_gpe_block_on_wake
 *
 * PARAMETERS:  gpe_xrupt_info      - GPE Interrupt info
 *              gpe_block           - Gpe Block info
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Enable all runtime GPEs and disable all wake GPEs -- in one
 *              GPE block
 *
 ******************************************************************************/

static acpi_status
acpi_hw_restore_gpe_block_on_wake (
	struct acpi_gpe_xrupt_info      *gpe_xrupt_info,
	struct acpi_gpe_block_info      *gpe_block)
{
	u32                             i;
	struct acpi_gpe_register_info   *gpe_register_info;
	acpi_status                     status;


	/* This callback processes one entire GPE block */

	/* Get the register info for the entire GPE block */

	gpe_register_info = gpe_block->register_info;

	/* Examine each GPE register within the block */

	for (i = 0; i < gpe_block->register_count; i++) {
		/* Clear the entire status register */

		status = acpi_hw_low_level_write (8, 0xFF,
				 &gpe_block->register_info[i].status_address);
		if (ACPI_FAILURE (status)) {
			return (status);
		}

		/*
		 * Restore the GPE Enable register, which will do the following:
		 *
		 * 1) Disable all wakeup GPEs
		 * 2) Enable all runtime GPEs
		 *
		 *  (On sleep, we saved the enabled status of all GPEs)
		 */
		status = acpi_hw_low_level_write (8, gpe_register_info->enable,
				 &gpe_register_info->enable_address);
		if (ACPI_FAILURE (status)) {
			return (status);
		}

		/* Point to next GPE register */

		gpe_register_info++;
	}

	return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    acpi_hw_restore_gpes_on_wake
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Enable all runtime GPEs and disable all wake GPEs -- in all
 *              GPE blocks
 *
 ******************************************************************************/

acpi_status
acpi_hw_restore_gpes_on_wake (
	void)
{
	acpi_status                     status;


	ACPI_FUNCTION_ENTRY ();


	status = acpi_ev_walk_gpe_list (acpi_hw_restore_gpe_block_on_wake);
	return (status);
}
