/*-
 * Copyright (c) 2003-04 3ware, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$FreeBSD$
 */

/*
 * 3ware driver for 9000 series storage controllers.
 *
 * Author: Vinod Kashyap
 */


#include <dev/twa/twa_includes.h>

/* AEN messages. */
struct twa_message	twa_aen_table[] = {
	{0x0000, "AEN queue empty"},
	{0x0001, "Controller reset occurred"},
	{0x0002, "Degraded unit detected"},
	{0x0003, "Controller error occured"},
	{0x0004, "Background rebuild failed"},
	{0x0005, "Background rebuild done"},
	{0x0006, "Incomplete unit detected"},
	{0x0007, "Background initialize done"},
	{0x0008, "Unclean shutdown detected"},
	{0x0009, "Drive timeout detected"},
	{0x000A, "Drive error detected"},
	{0x000B, "Rebuild started"},
	{0x000C, "Background initialize started"},
	{0x000D, "Entire logical unit was deleted"},
	{0x000E, "Background initialize failed"},
	{0x000F, "SMART attribute exceeded threshold"},
	{0x0010, "Power supply reported AC under range"},
	{0x0011, "Power supply reported DC out of range"},
	{0x0012, "Power supply reported a malfunction"},
	{0x0013, "Power supply predicted malfunction"},
	{0x0014, "Battery charge is below threshold"},
	{0x0015, "Fan speed is below threshold"},
	{0x0016, "Temperature sensor is above threshold"},
	{0x0017, "Power supply was removed"},
	{0x0018, "Power supply was inserted"},
	{0x0019, "Drive was removed from a bay"},
	{0x001A, "Drive was inserted into a bay"},
	{0x001B, "Drive bay cover door was opened"},
	{0x001C, "Drive bay cover door was closed"},
	{0x001D, "Product case was opened"},
	{0x0020, "Prepare for shutdown (power-off)"},
	{0x0021, "Downgrade UDMA mode to lower speed"},
	{0x0022, "Upgrade UDMA mode to higher speed"},
	{0x0023, "Sector repair completed"},
	{0x0024, "Sbuf memory test failed"},
	{0x0025, "Error flushing cached write data to array"},
	{0x0026, "Drive reported data ECC error"},
	{0x0027, "DCB has checksum error"},
	{0x0028, "DCB version is unsupported"},
	{0x0029, "Background verify started"},
	{0x002A, "Background verify failed"},
	{0x002B, "Background verify done"},
	{0x002C, "Bad sector overwritten during rebuild"},
	{0x002E, "Replace failed because replacement drive too small"},
	{0x002F, "Verify failed because array was never initialized"},
	{0x0030, "Unsupported ATA drive"},
	{0x0031, "Synchronize host/controller time"},
	{0x0032, "Spare capacity is inadequate for some units"},
	{0x0033, "Background migration started"},
	{0x0034, "Background migration failed"},
	{0x0035, "Background migration done"},
	{0x0036, "Verify detected and fixed data/parity mismatch"},
	{0x0037, "SO-DIMM incompatible"},
	{0x0038, "SO-DIMM not detected"},
	{0x0039, "Corrected Sbuf ECC error"},
	{0x003A, "Drive power on reset detected"},
	{0x003B, "Background rebuild paused"},
	{0x003C, "Background initialize paused"},
	{0x003D, "Background verify paused"},
	{0x003E, "Background migration paused"},
	{0x003F, "Corrupt flash file system detected"},
	{0x0040, "Flash file system repaired"},
	{0x0041, "Unit number assignments were lost"},
	{0x0042, "Error during read of primary DCB"},
	{0x0043, "Latent error found in backup DCB"},
	{0x00FC, "Recovered/finished array membership update"},
	{0x00FD, "Handler lockup"},
	{0x00FE, "Retrying PCI transfer"},
	{0x00FF, "AEN queue is full"},
	{0xFFFFFFFF, (char *)NULL}
};

/* AEN severity table. */
char	*twa_aen_severity_table[] = {
	"None",
	"ERROR",
	"WARNING",
	"INFO",
	"DEBUG",
	(char *)NULL
};

/* Error messages. */
struct twa_message	twa_error_table[] = {
	{0x0100, "SGL entry contains zero data"},
	{0x0101, "Invalid command opcode"},
	{0x0102, "SGL entry has unaligned address"},
	{0x0103, "SGL size does not match command"},
	{0x0104, "SGL entry has illegal length"},
	{0x0105, "Command packet is not aligned"},
	{0x0106, "Invalid request ID"},
	{0x0107, "Duplicate request ID"},
	{0x0108, "ID not locked"},
	{0x0109, "LBA out of range"},
	{0x010A, "Logical unit not supported"},
	{0x010B, "Parameter table does not exist"},
	{0x010C, "Parameter index does not exist"},
	{0x010D, "Invalid field in CDB"},
	{0x010E, "Specified port has invalid drive"},
	{0x010F, "Parameter item size mismatch"},
	{0x0110, "Failed memory allocation"},
	{0x0111, "Memory request too large"},
	{0x0112, "Out of memory segments"},
	{0x0113, "Invalid address to deallocate"},
	{0x0114, "Out of memory"},
	{0x0115, "Out of heap"},
	{0x0120, "Double degrade"},
	{0x0121, "Drive not degraded"},
	{0x0122, "Reconstruct error"},
	{0x0123, "Replace not accepted"},
	{0x0124, "Replace drive capacity too small"},
	{0x0125, "Sector count not allowed"},
	{0x0126, "No spares left"},
	{0x0127, "Reconstruct error"},
	{0x0128, "Unit is offline"},
	{0x0129, "Cannot update status to DCB"},
	{0x0130, "Invalid stripe handle"},
	{0x0131, "Handle that was not locked"},
	{0x0132, "Handle that was not empy"},
	{0x0133, "Handle has different owner"},
	{0x0140, "IPR has parent"},
	{0x0150, "Illegal Pbuf address alignment"},
	{0x0151, "Illegal Pbuf transfer length"},
	{0x0152, "Illegal Sbuf address alignment"},
	{0x0153, "Illegal Sbuf transfer length"},
	{0x0160, "Command packet too large"},
	{0x0161, "SGL exceeds maximum length"},
	{0x0162, "SGL has too many entries"},
	{0x0170, "Insufficient resources for rebuilder"},
	{0x0171, "Verify error (data != parity)"},
	{0x0180, "Requested segment not in directory of this DCB"},
	{0x0181, "DCB segment has unsupported version"},
	{0x0182, "DCB segment has checksum error"},
	{0x0183, "DCB support (settings) segment invalid"},
	{0x0184, "DCB UDB (unit descriptor block) segment invalid"},
	{0x0185, "DCB GUID (globally unique identifier) segment invalid"},
	{0x01A0, "Could not clear Sbuf"},
	{0x01C0, "Flash identify failed"},
	{0x01C1, "Flash out of bounds"},
	{0x01C2, "Flash verify error"},
	{0x01C3, "Flash file object not found"},
	{0x01C4, "Flash file already present"},
	{0x01C5, "Flash file system full"},
	{0x01C6, "Flash file not present"},
	{0x01C7, "Flash file size error"},
	{0x01C8, "Bad flash file checksum"},
	{0x01CA, "Corrupt flash file system detected"},
	{0x01D0, "Invalid field in parameter list"},
	{0x01D1, "Parameter list length error"},
	{0x01D2, "Parameter item is not changeable"},
	{0x01D3, "Parameter item is not saveable"},
	{0x0200, "UDMA CRC error"},
	{0x0201, "Internal CRC error"},
	{0x0202, "Data ECC error"},
	{0x0203, "ADP level 1 error"},
	{0x0204, "Port timeout"},
	{0x0205, "Drive power on reset"},
	{0x0206, "ADP level 2 error"},
	{0x0207, "Soft reset failed"},
	{0x0208, "Drive not ready"},
	{0x0209, "Unclassified port error"},
	{0x020A, "Drive aborted command"},
	{0x0210, "Internal CRC error"},
	{0x0211, "Host PCI bus abort"},
	{0x0212, "Host PCI parity error"},
	{0x0213, "Port handler error"},
	{0x0214, "Token interrupt count error"},
	{0x0215, "Timeout waiting for PCI transfer"},
	{0x0216, "Corrected buffer ECC"},
	{0x0217, "Uncorrected buffer ECC"},
	{0x0230, "Unsupported command during flash recovery"},
	{0x0231, "Next image buffer expected"},
	{0x0232, "Binary image architecture incompatible"},
	{0x0233, "Binary image has no signature"},
	{0x0234, "Binary image has bad checksum"},
	{0x0235, "Image downloaded overflowed buffer"},
	{0x0240, "I2C device not found"},
	{0x0241, "I2C transaction aborted"},
	{0x0242, "SO-DIMM parameter(s) incompatible using defaults"},
	{0x0243, "SO-DIMM unsupported"},
	{0x0248, "SPI transfer status error"},
	{0x0249, "SPI transfer timeout error"},
	{0x0250, "Invalid unit descriptor size in CreateUnit"},
	{0x0251, "Unit descriptor size exceeds data buffer in CreateUnit"},
	{0x0252, "Invalid value in CreateUnit descriptor"},
	{0x0253, "Inadequate disk space to support descriptor in CreateUnit"},
	{0x0254, "Unable to create data channel for this unit descriptor"},
	{0x0255, "CreateUnit descriptor specifies a drive already in use"},
	{0x0256, "Unable to write configuration to all disks during CreateUnit"},
	{0x0257, "CreateUnit does not support this descriptor version"},
	{0x0258, "Invalid subunit for RAID 0 or 5 in CreateUnit"},
	{0x0259, "Too many descriptors in CreateUnit"},
	{0x025A, "Invalid configuration specified in CreateUnit descriptor"},
	{0x025B, "Invalid LBA offset specified in CreateUnit descriptor"},
	{0x025C, "Invalid stripelet size specified in CreateUnit descriptor"},
	{0x0260, "SMART attribute exceeded threshold"},
	{0xFFFFFFFF, (char *)NULL}
};
    
#ifdef TWA_DEBUG
/*
 * Save the debug levels in global variables, so that
 * their values can be changed from the debugger.
 */
u_int8_t	twa_dbg_level = TWA_DEBUG;
u_int8_t	twa_call_dbg_level = TWA_DEBUG;

#endif /* TWA_DEBUG */
