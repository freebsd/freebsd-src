/*
 * Edgeport USB Serial Converter driver
 *
 * Copyright (C) 2000-2002 Inside Out Networks, All rights reserved.
 * Copyright (C) 2001-2002 Greg Kroah-Hartman <greg@kroah.com>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 * Supports the following devices:
 *	EP/1 EP/2 EP/4
 *
 * Version history:
 *
 *	July 11, 2002 	Removed 4 port device structure since all TI UMP 
 *			chips have only 2 ports 
 *			David Iacovelli (davidi@ionetworks.com)
 *
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/serial.h>
#include <linux/ioctl.h>
#include <asm/uaccess.h>
#include <linux/usb.h>

#ifdef CONFIG_USB_SERIAL_DEBUG
	static int debug = 1;
#else
	static int debug;
#endif

#include "usb-serial.h"

#include "io_16654.h"
#include "io_usbvend.h"
#include "io_ti.h"

/*
 * Version Information
 */
#define DRIVER_VERSION "v0.2"
#define DRIVER_AUTHOR "Greg Kroah-Hartman <greg@kroah.com> and David Iacovelli"
#define DRIVER_DESC "Edgeport USB Serial Driver"


/* firmware image code */
#define IMAGE_VERSION_NAME	PagableOperationalCodeImageVersion
#define IMAGE_ARRAY_NAME	PagableOperationalCodeImage
#define IMAGE_SIZE		PagableOperationalCodeSize
#include "io_fw_down3.h"	/* Define array OperationalCodeImage[] */

#define EPROM_PAGE_SIZE		64


struct edgeport_uart_buf_desc {
	__u32 count;		// Number of bytes currently in buffer
};

/* different hardware types */
#define HARDWARE_TYPE_930	0
#define HARDWARE_TYPE_TIUMP	1

// IOCTL_PRIVATE_TI_GET_MODE Definitions
#define	TI_MODE_CONFIGURING	0   // Device has not entered start device 
#define	TI_MODE_BOOT		1   // Staying in boot mode
#define TI_MODE_DOWNLOAD	2   // Made it to download mode
#define TI_MODE_TRANSITIONING	3   // Currently in boot mode but transitioning to download mode


/* Product information read from the Edgeport */
struct product_info
{
	int	TiMode;			// Current TI Mode
	__u8	hardware_type;		// Type of hardware
} __attribute__((packed));


struct edgeport_port {
	__u16 uart_base;
	__u16 dma_address;
	__u8 shadow_msr;
	__u8 shadow_mcr;
	__u8 shadow_lsr;
	__u8 lsr_mask;
	__u32 ump_read_timeout;		/* Number of miliseconds the UMP will
					   wait without data before completing
					   a read short */
	int baud_rate;
	int close_pending;
	int lsr_event;
	struct edgeport_uart_buf_desc tx;
	struct async_icount	icount;
	wait_queue_head_t	delta_msr_wait;	/* for handling sleeping while
						   waiting for msr change to
						   happen */
	struct edgeport_serial	*edge_serial;
	struct usb_serial_port	*port;
};

struct edgeport_serial {
	struct product_info product_info;
	u8 TI_I2C_Type;			// Type of I2C in UMP
	u8 TiReadI2C;			// Set to TRUE if we have read the I2c in Boot Mode
	int num_ports_open;
	struct usb_serial *serial;
};


/* Devices that this driver supports */
static struct usb_device_id edgeport_1port_id_table [] = {
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_TI_EDGEPORT_1) },
	{ }
};

static struct usb_device_id edgeport_2port_id_table [] = {
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_TI_EDGEPORT_2) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_TI_EDGEPORT_2C) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_TI_EDGEPORT_2I) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_TI_EDGEPORT_421) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_TI_EDGEPORT_421_BOOT) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_TI_EDGEPORT_421_DOWN) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_TI_EDGEPORT_21) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_TI_EDGEPORT_21_BOOT) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_TI_EDGEPORT_21_DOWN) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_TI_EDGEPORT_42) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_TI_EDGEPORT_4) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_TI_EDGEPORT_4I) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_TI_EDGEPORT_22) },
	{ }
};

/* Devices that this driver supports */
static __devinitdata struct usb_device_id id_table_combined [] = {
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_TI_EDGEPORT_1) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_TI_EDGEPORT_2) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_TI_EDGEPORT_2C) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_TI_EDGEPORT_2I) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_TI_EDGEPORT_421) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_TI_EDGEPORT_421_BOOT) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_TI_EDGEPORT_421_DOWN) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_TI_EDGEPORT_21) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_TI_EDGEPORT_21_BOOT) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_TI_EDGEPORT_21_DOWN) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_TI_EDGEPORT_42) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_TI_EDGEPORT_4) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_TI_EDGEPORT_4I) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_TI_EDGEPORT_22) },
	{ }
};

MODULE_DEVICE_TABLE (usb, id_table_combined);


static struct EDGE_FIRMWARE_VERSION_INFO OperationalCodeImageVersion;

static int TIStayInBootMode = 0;
static int ignore_cpu_rev = 0;



static void edge_set_termios (struct usb_serial_port *port, struct termios *old_termios);

static int TIReadVendorRequestSync (struct usb_device *dev,
				__u8		request,
				__u16		value,
				__u16		index,
				u8 		*data,
				int		size)
{
	int status;

	status = usb_control_msg (dev,
				usb_rcvctrlpipe(dev, 0),
				request,
				(USB_TYPE_VENDOR | 
				 USB_RECIP_DEVICE | 
				 USB_DIR_IN),
				value,
				index,
				data,
				size,
				HZ);
	if (status < 0)
		return status;
	if (status != size) {
		dbg ("%s - wanted to write %d, but only wrote %d",
		     __FUNCTION__, size, status);
		return -ECOMM;
	}
	return 0;
}

static int TISendVendorRequestSync (struct usb_device *dev,
				__u8		request,
				__u16		value,
				__u16		index,
				u8 		*data,
				int		size)
{
	int status;

	status = usb_control_msg (dev,
				usb_sndctrlpipe(dev, 0),
				request,
				(USB_TYPE_VENDOR | 
				 USB_RECIP_DEVICE | 
				 USB_DIR_OUT),
				value,
				index,
				data,
				size,
				HZ);

	if (status < 0)
		return status;
	if (status != size) {
		dbg ("%s - wanted to write %d, but only wrote %d",
		     __FUNCTION__, size, status);
		return -ECOMM;
	}
	return 0;
}

static int TIWriteCommandSync (struct usb_device *dev, __u8 command,
				__u8 moduleid, __u16 value, u8 *data,
				int size)
{
	return TISendVendorRequestSync (dev,
					  command,	  		// Request
					  value,			// wValue 
					  moduleid,			// wIndex
					  data,				// TransferBuffer
					  size);			// TransferBufferLength

}


/* clear tx/rx buffers and fifo in TI UMP */
static int TIPurgeDataSync (struct usb_serial_port *port, __u16 mask)
{
	int port_number = port->number - port->serial->minor;

	dbg ("%s - port %d, mask %x", __FUNCTION__, port_number, mask);

	return TIWriteCommandSync (port->serial->dev,
					UMPC_PURGE_PORT,
					(__u8)(UMPM_UART1_PORT + port_number),
					mask,
					NULL,
					0);
}

/**
 * TIReadDownloadMemory - Read edgeport memory from TI chip
 * @dev: usb device pointer
 * @address: Device CPU address at which to read
 * @length: Length of above data
 * @address_type: Can read both XDATA and I2C
 * @buffer: pointer to input data buffer
 */
int TIReadDownloadMemory (struct usb_device *dev, int start_address, int length,
			  __u8 address_type, __u8 *buffer)
{
	int status = 0;
	__u8 read_length;
	__u16 be_start_address;
	
	dbg ("%s - @ %x for %d", __FUNCTION__, start_address, length);

	/* Read in blocks of 64 bytes
	 * (TI firmware can't handle more than 64 byte reads)
	 */
	while (length) {
		if (length > 64)
			read_length= 64;
		else
			read_length = (__u8)length;

		if (read_length > 1) {
			dbg ("%s - @ %x for %d", __FUNCTION__, 
			     start_address, read_length);
		}
		be_start_address = cpu_to_be16 (start_address);
		status = TIReadVendorRequestSync (dev,
						  UMPC_MEMORY_READ,	// Request
						  (__u16)address_type,	// wValue (Address type)
						  be_start_address,	// wIndex (Address to read)
						  buffer,		// TransferBuffer
						  read_length);	// TransferBufferLength

		if (status) {
			dbg ("%s - ERROR %x", __FUNCTION__, status);
			return status;
		}

		if (read_length > 1) {
			usb_serial_debug_data (__FILE__, __FUNCTION__,
					       read_length, buffer);
		}

		/* Update pointers/length */
		start_address += read_length;
		buffer += read_length;
		length -= read_length;
	}
	
	return status;
}

int TIReadRam (struct usb_device *dev, int start_address, int length, __u8 *buffer)
{
	return TIReadDownloadMemory (dev,
				     start_address,
				     length,
				     DTK_ADDR_SPACE_XDATA,
				     buffer);
}

/* Read edgeport memory to a given block */
static int TIReadBootMemory (struct edgeport_serial *serial, int start_address, int length, __u8 * buffer)
{
	int status = 0;
	int i;

	for (i=0; i< length; i++) {
		status = TIReadVendorRequestSync (serial->serial->dev,
					UMPC_MEMORY_READ,		// Request
					serial->TI_I2C_Type,		// wValue (Address type)
					(__u16)(start_address+i),	// wIndex
					&buffer[i],			// TransferBuffer
					0x01);				// TransferBufferLength
		if (status) {
			dbg ("%s - ERROR %x", __FUNCTION__, status);
			return status;
		}
	}

	dbg ("%s - start_address = %x, length = %d", __FUNCTION__, start_address, length);
	usb_serial_debug_data (__FILE__, __FUNCTION__, length, buffer);

	serial->TiReadI2C = 1;

	return status;
}

/* Write given block to TI EPROM memory */
static int TIWriteBootMemory (struct edgeport_serial *serial, int start_address, int length, __u8 *buffer)
{
	int status = 0;
	int i;
	__u8 temp;

	/* Must do a read before write */
	if (!serial->TiReadI2C) {
		status = TIReadBootMemory(serial, 0, 1, &temp);
		if (status)
			return status;
	}

	for (i=0; i < length; ++i) {
		status = TISendVendorRequestSync (serial->serial->dev,
						UMPC_MEMORY_WRITE,		// Request
						buffer[i],			// wValue
						(__u16)(i+start_address),	// wIndex
						NULL,				// TransferBuffer
						0);				// TransferBufferLength
		if (status)
			return status;
	}

  	dbg ("%s - start_sddr = %x, length = %d", __FUNCTION__, start_address, length);
	usb_serial_debug_data (__FILE__, __FUNCTION__, length, buffer);

	return status;
}


/* Write edgeport I2C memory to TI chip	*/
static int TIWriteDownloadI2C (struct edgeport_serial *serial, int start_address, int length, __u8 address_type, __u8 *buffer)
{
	int status = 0;
	int write_length;
	__u16 be_start_address;

	/* We can only send a maximum of 1 aligned byte page at a time */
	
	/* calulate the number of bytes left in the first page */
	write_length = EPROM_PAGE_SIZE - (start_address & (EPROM_PAGE_SIZE - 1));

	if (write_length > length)
		write_length = length;

	dbg ("%s - BytesInFirstPage Addr = %x, length = %d", __FUNCTION__, start_address, write_length);
	usb_serial_debug_data (__FILE__, __FUNCTION__, write_length, buffer);

	/* Write first page */
	be_start_address = cpu_to_be16 (start_address);
	status = TISendVendorRequestSync (serial->serial->dev,
					UMPC_MEMORY_WRITE,	// Request
					(__u16)address_type,	// wValue
					be_start_address,	// wIndex
					buffer,			// TransferBuffer
					write_length);
	if (status) {
		dbg ("%s - ERROR %d", __FUNCTION__, status);
		return status;
	}

	length		-= write_length;
	start_address	+= write_length;
	buffer		+= write_length;

	/* We should be aligned now -- can write max page size bytes at a time */
	while (length) {
		if (length > EPROM_PAGE_SIZE)
			write_length = EPROM_PAGE_SIZE;
		else
			write_length = length;

		dbg ("%s - Page Write Addr = %x, length = %d", __FUNCTION__, start_address, write_length);
		usb_serial_debug_data (__FILE__, __FUNCTION__, write_length, buffer);

		/* Write next page */
		be_start_address = cpu_to_be16 (start_address);
		status = TISendVendorRequestSync (serial->serial->dev,
						UMPC_MEMORY_WRITE,	// Request
						(__u16)address_type,	// wValue
						be_start_address,	// wIndex
						buffer,	  		// TransferBuffer
						write_length);		// TransferBufferLength
		if (status) {
			dbg ("%s - ERROR %d", __FUNCTION__, status);
			return status;
		}
		
		length		-= write_length;
		start_address	+= write_length;
		buffer		+= write_length;
	}
	return status;
}

/* Examine the UMP DMA registers and LSR
 * 
 * Check the MSBit of the X and Y DMA byte count registers.
 * A zero in this bit indicates that the TX DMA buffers are empty
 * then check the TX Empty bit in the UART.
 */
static int TIIsTxActive (struct edgeport_port *port)
{
	int status;
	struct out_endpoint_desc_block *oedb;
	__u8 *lsr;
	int bytes_left = 0;

	oedb = kmalloc (sizeof (* oedb), GFP_KERNEL);
	if (!oedb) {
		err ("%s - out of memory", __FUNCTION__);
		return -ENOMEM;
	}

	lsr = kmalloc (1, GFP_KERNEL);	/* Sigh, that's right, just one byte,
					   as not all platforms can do DMA
					   from stack */
	if (!lsr) {
		kfree(oedb);
		return -ENOMEM;
	}
	/* Read the DMA Count Registers */
	status = TIReadRam (port->port->serial->dev,
			    port->dma_address,
			    sizeof( *oedb),
			    (void *)oedb);

	if (status)
		goto exit_is_tx_active;

	dbg ("%s - XByteCount    0x%X", __FUNCTION__, oedb->XByteCount);

	/* and the LSR */
	status = TIReadRam (port->port->serial->dev, 
			    port->uart_base + UMPMEM_OFFS_UART_LSR,
			    1,
			    lsr);

	if (status)
		goto exit_is_tx_active;
	dbg ("%s - LSR = 0x%X", __FUNCTION__, *lsr);
	
	/* If either buffer has data or we are transmitting then return TRUE */
	if ((oedb->XByteCount & 0x80 ) != 0 )
		bytes_left += 64;

	if ((*lsr & UMP_UART_LSR_TX_MASK ) == 0 )
		bytes_left += 1;

	/* We return Not Active if we get any kind of error */
exit_is_tx_active:
	dbg ("%s - return %d", __FUNCTION__, bytes_left );

	kfree(lsr);
	kfree(oedb);
	return bytes_left;
}

static void TIChasePort(struct edgeport_port *port)
{
	int loops;
	int last_count;
	int write_size;

restart_tx_loop:
	// Base the LoopTime on the baud rate
	if (port->baud_rate == 0)
		port->baud_rate = 1200;

	write_size = port->tx.count;
	loops = max(100, (100*write_size)/(port->baud_rate/10));
	dbg ("%s - write_size %d, baud %d loop = %d", __FUNCTION__,
	     write_size, port->baud_rate, loops);

	while (1) {
		// Save Last count
		last_count = port->tx.count;

		dbg ("%s - Tx Buffer Size = %d loops = %d", __FUNCTION__,
		     last_count, loops);

		/* Is the Edgeport Buffer empty? */
		if (port->tx.count == 0)
			break;

		/* Block the thread for 10ms */
		wait_ms (10);

		if (last_count == port->tx.count) {
			/* No activity.. count down. */
			--loops;
			if (loops == 0) {
				dbg ("%s - Wait for TxEmpty - TIMEOUT",
				     __FUNCTION__);
				return;
			}
		} else {
			/* Reset timeout value back to a minimum of 1 second */
			dbg ("%s - Wait for TxEmpty  Reset Count", __FUNCTION__);
			goto restart_tx_loop;
		}
	}

	dbg ("%s - Local Tx Buffer Empty -- Waiting for TI UMP to EMPTY X/Y and FIFO",
	     __FUNCTION__);

	write_size = TIIsTxActive (port);
	loops = max(50, (100*write_size)/(port->baud_rate/10));
	dbg ("%s - write_size %d, baud %d loop = %d", __FUNCTION__, 
	     write_size, port->baud_rate, loops);

	while (1) {
		/* This function takes 4 ms; */
		if (!TIIsTxActive (port)) {
			/* Delay a few char times */
			wait_ms (50);
			dbg ("%s - Empty", __FUNCTION__);
			return;
		}

		--loops;
		if (loops == 0) {
			dbg ("%s - TIMEOUT", __FUNCTION__);
			return;
		}
	}
}

static int TIChooseConfiguration (struct usb_device *dev)
{
	// There may be multiple configurations on this device, in which case
	// we would need to read and parse all of them to find out which one
	// we want. However, we just support one config at this point,
	// configuration # 1, which is Config Descriptor 0.

	dbg ("%s - Number of Interfaces = %d", __FUNCTION__, dev->config->bNumInterfaces);
	dbg ("%s - MAX Power            = %d", __FUNCTION__, dev->config->MaxPower*2);

	if (dev->config->bNumInterfaces != 1) {
		err ("%s - bNumInterfaces is not 1, ERROR!", __FUNCTION__);
		return -ENODEV;
	}

	return 0;
}

int TIReadRom (struct edgeport_serial *serial, int start_address, int length, __u8 *buffer)
{
	int status;

	if (serial->product_info.TiMode == TI_MODE_DOWNLOAD) {
		status = TIReadDownloadMemory (serial->serial->dev,
					       start_address,
					       length,
					       serial->TI_I2C_Type,
					       buffer);
	} else {
		status = TIReadBootMemory (serial,
					   start_address,
					   length,
					   buffer);
	}

	return status;
}

int TIWriteRom (struct edgeport_serial *serial, int start_address, int length, __u8 *buffer)
{
	if (serial->product_info.TiMode == TI_MODE_BOOT)
		return TIWriteBootMemory (serial,
					  start_address,
					  length,
					  buffer);

	if (serial->product_info.TiMode == TI_MODE_DOWNLOAD)
		return TIWriteDownloadI2C (serial,
					   start_address,
					   length,
					   serial->TI_I2C_Type,
					   buffer);

	return -EINVAL;
}



/* Read a descriptor header from I2C based on type */
static int TIGetDescriptorAddress (struct edgeport_serial *serial, int desc_type, struct ti_i2c_desc *rom_desc)
{
	int start_address;
	int status;

	/* Search for requested descriptor in I2C */
	start_address = 2;
	do {
		status = TIReadRom (serial,
				   start_address,
				   sizeof(struct ti_i2c_desc),
				   (__u8 *)rom_desc );
		if (status)
			return 0;

		if (rom_desc->Type == desc_type)
			return start_address;

		start_address = start_address + sizeof(struct ti_i2c_desc) +  rom_desc->Size;

	} while ((start_address < TI_MAX_I2C_SIZE) && rom_desc->Type);
	
	return 0;
}

/* Validate descriptor checksum */
static int ValidChecksum(struct ti_i2c_desc *rom_desc, __u8 *buffer)
{
	__u16 i;
	__u8 cs = 0;

	for (i=0; i < rom_desc->Size; i++) {
		cs = (__u8)(cs + buffer[i]);
	}
	if (cs != rom_desc->CheckSum) {
		dbg ("%s - Mismatch %x - %x", __FUNCTION__, rom_desc->CheckSum, cs);
		return -EINVAL;
	}
	return 0;
}

/* Make sure that the I2C image is good */
static int TiValidateI2cImage (struct edgeport_serial *serial)
{
	int status = 0;
	struct ti_i2c_desc *rom_desc;
	int start_address = 2;
	__u8 *buffer;

	rom_desc = kmalloc (sizeof (*rom_desc), GFP_KERNEL);
	if (!rom_desc) {
		err ("%s - out of memory", __FUNCTION__);
		return -ENOMEM;
	}
	buffer = kmalloc (TI_MAX_I2C_SIZE, GFP_KERNEL);
	if (!buffer) {
		err ("%s - out of memory when allocating buffer", __FUNCTION__);
		kfree (rom_desc);
		return -ENOMEM;
	}

	// Read the first byte (Signature0) must be 0x52
	status = TIReadRom (serial, 0, 1, buffer);
	if (status)
		goto ExitTiValidateI2cImage; 

	if (*buffer != 0x52) {
		err ("%s - invalid buffer signature", __FUNCTION__);
		status = -ENODEV;
		goto ExitTiValidateI2cImage;
	}

	do {
		// Validate the I2C
		status = TIReadRom (serial,
				start_address,
				sizeof(struct ti_i2c_desc),
				(__u8 *)rom_desc);
		if (status)
			break;

		if ((start_address + sizeof(struct ti_i2c_desc) + rom_desc->Size) > TI_MAX_I2C_SIZE) {
			status = -ENODEV;
			dbg ("%s - structure too big, erroring out.", __FUNCTION__);
			break;
		}

		dbg ("%s Type = 0x%x", __FUNCTION__, rom_desc->Type);

		// Skip type 2 record
		if ((rom_desc->Type & 0x0f) != I2C_DESC_TYPE_FIRMWARE_BASIC) {
			// Read the descriptor data
			status = TIReadRom(serial,
						start_address+sizeof(struct ti_i2c_desc),
						rom_desc->Size,
						buffer);
			if (status)
				break;

			status = ValidChecksum(rom_desc, buffer);
			if (status)
				break;
		}
		start_address = start_address + sizeof(struct ti_i2c_desc) + rom_desc->Size;

	} while ((rom_desc->Type != I2C_DESC_TYPE_ION) && (start_address < TI_MAX_I2C_SIZE));

	if ((rom_desc->Type != I2C_DESC_TYPE_ION) || (start_address > TI_MAX_I2C_SIZE))
		status = -ENODEV;

ExitTiValidateI2cImage:	
	kfree (buffer);
	kfree (rom_desc);
	return status;
}

static int TIReadManufDescriptor (struct edgeport_serial *serial, __u8 *buffer)
{
	int status;
	int start_address;
	struct ti_i2c_desc *rom_desc;
	struct edge_ti_manuf_descriptor *desc;

	rom_desc = kmalloc (sizeof (*rom_desc), GFP_KERNEL);
	if (!rom_desc) {
		err ("%s - out of memory", __FUNCTION__);
		return -ENOMEM;
	}
	start_address = TIGetDescriptorAddress (serial, I2C_DESC_TYPE_ION, rom_desc);

	if (!start_address) {
		dbg ("%s - Edge Descriptor not found in I2C", __FUNCTION__);
		status = -ENODEV;
		goto exit;
	}

	// Read the descriptor data
	status = TIReadRom (serial,
				start_address+sizeof(struct ti_i2c_desc),
				rom_desc->Size,
				buffer);
	if (status)
		goto exit;
	
	status = ValidChecksum(rom_desc, buffer);
	
	desc = (struct edge_ti_manuf_descriptor *)buffer;
	dbg ( "%s - IonConfig      0x%x", __FUNCTION__, desc->IonConfig 	);
	dbg ( "%s - Version          %d", __FUNCTION__, desc->Version	  	);
	dbg ( "%s - Cpu/Board      0x%x", __FUNCTION__, desc->CpuRev_BoardRev	);
	dbg ( "%s - NumPorts         %d", __FUNCTION__, desc->NumPorts  	);	
	dbg ( "%s - NumVirtualPorts  %d", __FUNCTION__, desc->NumVirtualPorts	);	
	dbg ( "%s - TotalPorts       %d", __FUNCTION__, desc->TotalPorts  	);	

exit:
	kfree (rom_desc);
	return status;
}

/* Build firmware header used for firmware update */
static int BuildI2CFirmwareHeader (__u8 *header)
{
	__u8 *buffer;
	int buffer_size;
	int i;
	__u8 cs = 0;
	struct ti_i2c_desc *i2c_header;
	struct ti_i2c_image_header *img_header;
	struct ti_i2c_firmware_rec *firmware_rec;

	// In order to update the I2C firmware we must change the type 2 record to type 0xF2.
	// This will force the UMP to come up in Boot Mode.  Then while in boot mode, the driver 
	// will download the latest firmware (padded to 15.5k) into the UMP ram. 
	// And finally when the device comes back up in download mode the driver will cause 
	// the new firmware to be copied from the UMP Ram to I2C and the firmware will update
	// the record type from 0xf2 to 0x02.
	
	// Allocate a 15.5k buffer + 2 bytes for version number (Firmware Record)
	buffer_size = (((1024 * 16) - 512 )+ sizeof(struct ti_i2c_firmware_rec));

	buffer = kmalloc (buffer_size, GFP_KERNEL);
	if (!buffer) {
		err ("%s - out of memory", __FUNCTION__);
		return -ENOMEM;
	}
	
	// Set entire image of 0xffs
	memset (buffer, 0xff, buffer_size);

	// Copy version number into firmware record
	firmware_rec = (struct ti_i2c_firmware_rec *)buffer;

	firmware_rec->Ver_Major	= OperationalCodeImageVersion.MajorVersion;
	firmware_rec->Ver_Minor	= OperationalCodeImageVersion.MinorVersion;

	// Pointer to fw_down memory image
	img_header = (struct ti_i2c_image_header *)&PagableOperationalCodeImage[0];

	memcpy (buffer + sizeof(struct ti_i2c_firmware_rec),
		&PagableOperationalCodeImage[sizeof(struct ti_i2c_image_header)],
		img_header->Length);

	for (i=0; i < buffer_size; i++) {
		cs = (__u8)(cs + buffer[i]);
	}

	kfree (buffer);

	// Build new header
	i2c_header =  (struct ti_i2c_desc *)header;
	firmware_rec =  (struct ti_i2c_firmware_rec*)i2c_header->Data;
	
	i2c_header->Type	= I2C_DESC_TYPE_FIRMWARE_BLANK;
	i2c_header->Size	= (__u16)buffer_size;
	i2c_header->CheckSum	= cs;
	firmware_rec->Ver_Major	= OperationalCodeImageVersion.MajorVersion;
	firmware_rec->Ver_Minor	= OperationalCodeImageVersion.MinorVersion;

	return 0;
}

/* Try to figure out what type of I2c we have */
static int TIGetI2cTypeInBootMode (struct edgeport_serial *serial)
{
	int status;
	__u8 data;
		
	// Try to read type 2
	status = TIReadVendorRequestSync (serial->serial->dev,
					UMPC_MEMORY_READ,		// Request
					DTK_ADDR_SPACE_I2C_TYPE_II,	// wValue (Address type)
					0,		 		// wIndex
					&data,				// TransferBuffer
					0x01);				// TransferBufferLength
	if (status)
		dbg ("%s - read 2 status error = %d", __FUNCTION__, status);
	else
		dbg ("%s - read 2 data = 0x%x", __FUNCTION__, data);
	if ((!status) && data == 0x52) {
		dbg ("%s - ROM_TYPE_II", __FUNCTION__);
		serial->TI_I2C_Type = DTK_ADDR_SPACE_I2C_TYPE_II;
		return 0;
	}

	// Try to read type 3
	status = TIReadVendorRequestSync (serial->serial->dev,
					UMPC_MEMORY_READ,		// Request
					DTK_ADDR_SPACE_I2C_TYPE_III,	// wValue (Address type)
					0,				// wIndex
					&data,				// TransferBuffer
					0x01);				// TransferBufferLength
	if (status)
		dbg ("%s - read 3 status error = %d", __FUNCTION__, status);
	else
		dbg ("%s - read 2 data = 0x%x", __FUNCTION__, data);
	if ((!status) && data == 0x52) {
		dbg ("%s - ROM_TYPE_III", __FUNCTION__);
		serial->TI_I2C_Type = DTK_ADDR_SPACE_I2C_TYPE_III;
		return 0;
	}

	dbg ("%s - Unknown", __FUNCTION__);
	serial->TI_I2C_Type = DTK_ADDR_SPACE_I2C_TYPE_II;
	return -ENODEV;
}

static int TISendBulkTransferSync (struct usb_serial *serial, void *buffer, int length, int *num_sent)
{
	int status;

	status = usb_bulk_msg (serial->dev,
				usb_sndbulkpipe(serial->dev,
						serial->port[0].bulk_out_endpointAddress),
				buffer,
				length,
				num_sent,
				HZ);
	return status;
}

/* Download given firmware image to the device (IN BOOT MODE) */
static int TIDownloadCodeImage (struct edgeport_serial *serial, __u8 *image, int image_length)
{
	int status = 0;
	int pos;
	int transfer;
	int done;

	// Transfer firmware image
	for (pos = 0; pos < image_length; ) {
		// Read the next buffer from file
		transfer = image_length - pos;
		if (transfer > EDGE_FW_BULK_MAX_PACKET_SIZE)
			transfer = EDGE_FW_BULK_MAX_PACKET_SIZE;

		// Transfer data
		status = TISendBulkTransferSync (serial->serial, &image[pos], transfer, &done);
		if (status)
			break;
		// Advance buffer pointer
		pos += done;
	}

	return status;
}

// FIXME!!!
static int TIConfigureBootDevice (struct usb_device *dev)
{
	return 0;
}

/**
 * DownloadTIFirmware - Download run-time operating firmware to the TI5052
 * 
 * This routine downloads the main operating code into the TI5052, using the
 * boot code already burned into E2PROM or ROM.
 */
static int TIDownloadFirmware (struct edgeport_serial *serial)
{
	int status = 0;
	int start_address;
	struct edge_ti_manuf_descriptor *ti_manuf_desc;
	struct usb_interface_descriptor *interface;
	int download_cur_ver;
	int download_new_ver;

	/* This routine is entered by both the BOOT mode and the Download mode
	 * We can determine which code is running by the reading the config
	 * descriptor and if we have only one bulk pipe it is in boot mode
	 */
	serial->product_info.hardware_type = HARDWARE_TYPE_TIUMP;

	/* Default to type 2 i2c */
	serial->TI_I2C_Type = DTK_ADDR_SPACE_I2C_TYPE_II;

	status = TIChooseConfiguration (serial->serial->dev);
	if (status)
		return status;

	interface = serial->serial->dev->config->interface->altsetting;
	if (!interface) {
		err ("%s - no interface set, error!", __FUNCTION__);
		return -ENODEV;
	}

	// Setup initial mode -- the default mode 0 is TI_MODE_CONFIGURING
	// if we have more than one endpoint we are definitely in download mode
	if (interface->bNumEndpoints > 1)
		serial->product_info.TiMode = TI_MODE_DOWNLOAD;
	else
		// Otherwise we will remain in configuring mode
		serial->product_info.TiMode = TI_MODE_CONFIGURING;

	// Save Download Version Number
	OperationalCodeImageVersion.MajorVersion = PagableOperationalCodeImageVersion.MajorVersion;
	OperationalCodeImageVersion.MinorVersion = PagableOperationalCodeImageVersion.MinorVersion;
	OperationalCodeImageVersion.BuildNumber	 = PagableOperationalCodeImageVersion.BuildNumber;

	/********************************************************************/
	/* Download Mode */
	/********************************************************************/
	if (serial->product_info.TiMode == TI_MODE_DOWNLOAD) {
		struct ti_i2c_desc *rom_desc;

		dbg ("%s - <<<<<<<<<<<<<<<RUNNING IN DOWNLOAD MODE>>>>>>>>>>", __FUNCTION__);

		status = TiValidateI2cImage (serial);
		if (status) {
			dbg ("%s - <<<<<<<<<<<<<<<DOWNLOAD MODE -- BAD I2C >>>>>>>>>>",
			     __FUNCTION__);
			return status;
		}
		
		/* Validate Hardware version number
		 * Read Manufacturing Descriptor from TI Based Edgeport
		 */
		ti_manuf_desc = kmalloc (sizeof (*ti_manuf_desc), GFP_KERNEL);
		if (!ti_manuf_desc) {
			err ("%s - out of memory.", __FUNCTION__);
			return -ENOMEM;
		}
		status = TIReadManufDescriptor (serial, (__u8 *)ti_manuf_desc);
		if (status) {
			kfree (ti_manuf_desc);
			return status;
		}

		// Check version number of ION descriptor
		if (!ignore_cpu_rev && TI_GET_CPU_REVISION(ti_manuf_desc->CpuRev_BoardRev) < 2) {
			dbg ( "%s - Wrong CPU Rev %d (Must be 2)", __FUNCTION__, 
			     TI_GET_CPU_REVISION(ti_manuf_desc->CpuRev_BoardRev));
			kfree (ti_manuf_desc);
		   	return -EINVAL;
		}

		rom_desc = kmalloc (sizeof (*rom_desc), GFP_KERNEL);
		if (!rom_desc) {
			err ("%s - out of memory.", __FUNCTION__);
			kfree (ti_manuf_desc);
			return -ENOMEM;
		}

		// Search for type 2 record (firmware record)
		if ((start_address = TIGetDescriptorAddress (serial, I2C_DESC_TYPE_FIRMWARE_BASIC, rom_desc)) != 0) {
			struct ti_i2c_firmware_rec *firmware_version;
			__u8 record;

			dbg ("%s - Found Type FIRMWARE (Type 2) record", __FUNCTION__);

			firmware_version = kmalloc (sizeof (*firmware_version), GFP_KERNEL);
			if (!firmware_version) {
				err ("%s - out of memory.", __FUNCTION__);
				kfree (rom_desc);
				kfree (ti_manuf_desc);
				return -ENOMEM;
			}

			// Validate version number				
			// Read the descriptor data
			status = TIReadRom (serial,
					start_address+sizeof(struct ti_i2c_desc),
					sizeof(struct ti_i2c_firmware_rec),
					(__u8 *)firmware_version);
			if (status) {
				kfree (firmware_version);
				kfree (rom_desc);
				kfree (ti_manuf_desc);
				return status;
			}

			// Check version number of download with current version in I2c
			download_cur_ver = (firmware_version->Ver_Major << 8) + 
					   (firmware_version->Ver_Minor);
			download_new_ver = (OperationalCodeImageVersion.MajorVersion << 8) +
					   (OperationalCodeImageVersion.MinorVersion);

			dbg ("%s - >>>Firmware Versions Device %d.%d  Driver %d.%d",
			     __FUNCTION__,
			     firmware_version->Ver_Major,
			     firmware_version->Ver_Minor,
			     OperationalCodeImageVersion.MajorVersion,
			     OperationalCodeImageVersion.MinorVersion);

			// Check if we have an old version in the I2C and update if necessary
			if (download_cur_ver != download_new_ver) {
				dbg ("%s - Update I2C Download from %d.%d to %d.%d",
				     __FUNCTION__,
				     firmware_version->Ver_Major,
				     firmware_version->Ver_Minor,
				     OperationalCodeImageVersion.MajorVersion,
				     OperationalCodeImageVersion.MinorVersion);

				// In order to update the I2C firmware we must change the type 2 record to type 0xF2.
				// This will force the UMP to come up in Boot Mode.  Then while in boot mode, the driver 
				// will download the latest firmware (padded to 15.5k) into the UMP ram. 
				// And finally when the device comes back up in download mode the driver will cause 
				// the new firmware to be copied from the UMP Ram to I2C and the firmware will update
				// the record type from 0xf2 to 0x02.

				record = I2C_DESC_TYPE_FIRMWARE_BLANK;

				// Change the I2C Firmware record type to 0xf2 to trigger an update
				status = TIWriteRom (serial,
							start_address,
							sizeof(record),
							&record);
				if (status) {
					kfree (firmware_version);
					kfree (rom_desc);
					kfree (ti_manuf_desc);
					return status;
				}

				// verify the write -- must do this in order for write to 
				// complete before we do the hardware reset
				status = TIReadRom (serial,
							start_address,
							sizeof(record),
							&record);

				if (status) {
					kfree (firmware_version);
					kfree (rom_desc);
					kfree (ti_manuf_desc);
					return status;
				}

				if (record != I2C_DESC_TYPE_FIRMWARE_BLANK) {
					err ("%s - error resetting device", __FUNCTION__);
					kfree (firmware_version);
					kfree (rom_desc);
					kfree (ti_manuf_desc);
					return -ENODEV;
				}

				dbg ("%s - HARDWARE RESET", __FUNCTION__);

				// Reset UMP -- Back to BOOT MODE
				status = TISendVendorRequestSync (serial->serial->dev,
								UMPC_HARDWARE_RESET,	// Request
								0,			// wValue
								0,			// wIndex
								NULL,			// TransferBuffer
								0);			// TransferBufferLength

				dbg ( "%s - HARDWARE RESET return %d", __FUNCTION__, status);

				/* return an error on purpose. */
				kfree (firmware_version);
				kfree (rom_desc);
				kfree (ti_manuf_desc);
				return -ENODEV;
			}
			kfree (firmware_version);
		}
		// Search for type 0xF2 record (firmware blank record)
		else if ((start_address = TIGetDescriptorAddress (serial, I2C_DESC_TYPE_FIRMWARE_BLANK, rom_desc)) != 0) {
			#define HEADER_SIZE	(sizeof(struct ti_i2c_desc) + sizeof(struct ti_i2c_firmware_rec))
			__u8 *header;
			__u8 *vheader;

			header  = kmalloc (HEADER_SIZE, GFP_KERNEL);
			if (!header) {
				err ("%s - out of memory.", __FUNCTION__);
				kfree (rom_desc);
				kfree (ti_manuf_desc);
				return -ENOMEM;
			}
				
			vheader = kmalloc (HEADER_SIZE, GFP_KERNEL);
			if (!vheader) {
				err ("%s - out of memory.", __FUNCTION__);
				kfree (header);
				kfree (rom_desc);
				kfree (ti_manuf_desc);
				return -ENOMEM;
			}
			
			dbg ("%s - Found Type BLANK FIRMWARE (Type F2) record", __FUNCTION__);

			// In order to update the I2C firmware we must change the type 2 record to type 0xF2.
			// This will force the UMP to come up in Boot Mode.  Then while in boot mode, the driver 
			// will download the latest firmware (padded to 15.5k) into the UMP ram. 
			// And finally when the device comes back up in download mode the driver will cause 
			// the new firmware to be copied from the UMP Ram to I2C and the firmware will update
			// the record type from 0xf2 to 0x02.
			status = BuildI2CFirmwareHeader(header);
			if (status) {
				kfree (vheader);
				kfree (header);
				kfree (rom_desc);
				kfree (ti_manuf_desc);
				return status;
			}

			// Update I2C with type 0xf2 record with correct size and checksum
			status = TIWriteRom (serial,
						start_address,
						HEADER_SIZE,
						header);
			if (status) {
				kfree (vheader);
				kfree (header);
				kfree (rom_desc);
				kfree (ti_manuf_desc);
				return status;
			}

			// verify the write -- must do this in order for write to 
			// complete before we do the hardware reset
			status = TIReadRom (serial,
						start_address,
						HEADER_SIZE,
						vheader);

			if (status) {
				dbg ("%s - can't read header back", __FUNCTION__);
				kfree (vheader);
				kfree (header);
				kfree (rom_desc);
				kfree (ti_manuf_desc);
				return status;
			}
			if (memcmp(vheader, header, HEADER_SIZE)) {
				dbg ("%s - write download record failed", __FUNCTION__);
				kfree (vheader);
				kfree (header);
				kfree (rom_desc);
				kfree (ti_manuf_desc);
				return status;
			}

			kfree (vheader);
			kfree (header);

			dbg ("%s - Start firmware update", __FUNCTION__);

			// Tell firmware to copy download image into I2C 
			status = TISendVendorRequestSync (serial->serial->dev,
						UMPC_COPY_DNLD_TO_I2C,	// Request
						0,			// wValue 
						0,			// wIndex
						NULL,			// TransferBuffer
						0);			// TransferBufferLength

		  	dbg ("%s - Update complete 0x%x", __FUNCTION__, status);
			if (status) {
				dbg ("%s - UMPC_COPY_DNLD_TO_I2C failed", __FUNCTION__);
				kfree (rom_desc);
				kfree (ti_manuf_desc);
				return status;
			}
		}

		// The device is running the download code
		kfree (rom_desc);
		kfree (ti_manuf_desc);
		return 0;
	}

	/********************************************************************/
	/* Boot Mode */
	/********************************************************************/
	dbg ("%s - <<<<<<<<<<<<<<<RUNNING IN BOOT MODE>>>>>>>>>>>>>>>",
	     __FUNCTION__);

	// Configure the TI device so we can use the BULK pipes for download
	status = TIConfigureBootDevice (serial->serial->dev);
	if (status)
		return status;

	if (serial->serial->dev->descriptor.idVendor != USB_VENDOR_ID_ION) {
		dbg ("%s - VID = 0x%x", __FUNCTION__,
		     serial->serial->dev->descriptor.idVendor);
		serial->TI_I2C_Type = DTK_ADDR_SPACE_I2C_TYPE_II;
		goto StayInBootMode;
	}

	// We have an ION device (I2c Must be programmed)
	// Determine I2C image type
	if (TIGetI2cTypeInBootMode(serial)) {
		goto StayInBootMode;
	}

	// Registry variable set?
	if (TIStayInBootMode) {
		dbg ("%s - TIStayInBootMode", __FUNCTION__);
		goto StayInBootMode;
	}

	// Check for ION Vendor ID and that the I2C is valid
	if (!TiValidateI2cImage(serial)) {
		struct ti_i2c_image_header *header;
		int i;
		__u8 cs = 0;
		__u8 *buffer;
		int buffer_size;

		/* Validate Hardware version number
		 * Read Manufacturing Descriptor from TI Based Edgeport
		 */
		ti_manuf_desc = kmalloc (sizeof (*ti_manuf_desc), GFP_KERNEL);
		if (!ti_manuf_desc) {
			err ("%s - out of memory.", __FUNCTION__);
			return -ENOMEM;
		}
		status = TIReadManufDescriptor (serial, (__u8 *)ti_manuf_desc);
		if (status) {
			kfree (ti_manuf_desc);
			goto StayInBootMode;
		}

		// Check for version 2
		if (!ignore_cpu_rev && TI_GET_CPU_REVISION(ti_manuf_desc->CpuRev_BoardRev) < 2) {
			dbg ("%s - Wrong CPU Rev %d (Must be 2)", __FUNCTION__,
			     TI_GET_CPU_REVISION(ti_manuf_desc->CpuRev_BoardRev));
			kfree (ti_manuf_desc);
			goto StayInBootMode;
		}

		kfree (ti_manuf_desc);

		// In order to update the I2C firmware we must change the type 2 record to type 0xF2.
		// This will force the UMP to come up in Boot Mode.  Then while in boot mode, the driver 
		// will download the latest firmware (padded to 15.5k) into the UMP ram. 
		// And finally when the device comes back up in download mode the driver will cause 
		// the new firmware to be copied from the UMP Ram to I2C and the firmware will update
		// the record type from 0xf2 to 0x02.
		
		/*
		 * Do we really have to copy the whole firmware image,
		 * or could we do this in place!
		 */

		// Allocate a 15.5k buffer + 3 byte header
		buffer_size = (((1024 * 16) - 512) + sizeof(struct ti_i2c_image_header));
		buffer = kmalloc (buffer_size, GFP_KERNEL);
		if (!buffer) {
			err ("%s - out of memory", __FUNCTION__);
			return -ENOMEM;
		}
		
		// Initialize the buffer to 0xff (pad the buffer)
		memset (buffer, 0xff, buffer_size);

		memcpy (buffer, &PagableOperationalCodeImage[0], PagableOperationalCodeSize);

		for(i = sizeof(struct ti_i2c_image_header); i < buffer_size; i++) {
			cs = (__u8)(cs + buffer[i]);
		}
		
		header = (struct ti_i2c_image_header *)buffer;
		
		// update length and checksum after padding
		header->Length 	 = (__u16)(buffer_size - sizeof(struct ti_i2c_image_header));
		header->CheckSum = cs;

		// Download the operational code 
		dbg ("%s - Downloading operational code image (TI UMP)", __FUNCTION__);
		status = TIDownloadCodeImage (serial, buffer, buffer_size);

		kfree (buffer);

		if (status) {
	  		dbg ("%s - Error downloading operational code image", __FUNCTION__);
			return status;
		}

		// Device will reboot
		serial->product_info.TiMode = TI_MODE_TRANSITIONING;

  		dbg ("%s - Download successful -- Device rebooting...", __FUNCTION__);

		/* return an error on purpose */
		return -ENODEV;
	}

StayInBootMode:
	// Eprom is invalid or blank stay in boot mode
	dbg ("%s - <<<<<<<<<<<<<<<STAYING IN BOOT MODE>>>>>>>>>>>>", __FUNCTION__);
	serial->product_info.TiMode = TI_MODE_BOOT;

	return 0;
}


static int TISetDtr (struct edgeport_port *port)
{
	int port_number = port->port->number - port->port->serial->minor;

	dbg ("%s", __FUNCTION__);
	port->shadow_mcr |= MCR_DTR;

	return TIWriteCommandSync (port->port->serial->dev,
				UMPC_SET_CLR_DTR,
				(__u8)(UMPM_UART1_PORT + port_number),
				1,	/* set */
				NULL,
				0);
}

static int TIClearDtr (struct edgeport_port *port)
{
	int port_number = port->port->number - port->port->serial->minor;

	dbg ("%s", __FUNCTION__);
	port->shadow_mcr &= ~MCR_DTR;

	return TIWriteCommandSync (port->port->serial->dev,
				UMPC_SET_CLR_DTR,
				(__u8)(UMPM_UART1_PORT + port_number),
				0,	/* clear */
				NULL,
				0);
}

static int TISetRts (struct edgeport_port *port)
{
	int port_number = port->port->number - port->port->serial->minor;

	dbg ("%s", __FUNCTION__);
	port->shadow_mcr |= MCR_RTS;

	return TIWriteCommandSync (port->port->serial->dev,
				UMPC_SET_CLR_RTS,
				(__u8)(UMPM_UART1_PORT + port_number),
				1,	/* set */
				NULL,
				0);
}

static int TIClearRts (struct edgeport_port *port)
{
	int port_number = port->port->number - port->port->serial->minor;

	dbg ("%s", __FUNCTION__);
	port->shadow_mcr &= ~MCR_RTS;

	return TIWriteCommandSync (port->port->serial->dev,
				UMPC_SET_CLR_RTS,
				(__u8)(UMPM_UART1_PORT + port_number),
				0,	/* clear */
				NULL,
				0);
}

static int TISetLoopBack (struct edgeport_port *port)
{
	int port_number = port->port->number - port->port->serial->minor;

	dbg ("%s", __FUNCTION__);

	return TIWriteCommandSync (port->port->serial->dev,
				UMPC_SET_CLR_LOOPBACK,
				(__u8)(UMPM_UART1_PORT + port_number),
				1,	/* set */
				NULL,
				0);
}

static int TIClearLoopBack (struct edgeport_port *port)
{
	int port_number = port->port->number - port->port->serial->minor;

	dbg ("%s", __FUNCTION__);

	return TIWriteCommandSync (port->port->serial->dev,
				UMPC_SET_CLR_LOOPBACK,
				(__u8)(UMPM_UART1_PORT + port_number),
				0,	/* clear */
				NULL,
				0);
}

static int TISetBreak (struct edgeport_port *port)
{
	int port_number = port->port->number - port->port->serial->minor;

	dbg ("%s", __FUNCTION__);

	return TIWriteCommandSync (port->port->serial->dev,
				UMPC_SET_CLR_BREAK,
				(__u8)(UMPM_UART1_PORT + port_number),
				1,	/* set */
				NULL,
				0);
}

static int TIClearBreak (struct edgeport_port *port)
{
	int port_number = port->port->number - port->port->serial->minor;

	dbg ("%s", __FUNCTION__);

	return TIWriteCommandSync (port->port->serial->dev,
				UMPC_SET_CLR_BREAK,
				(__u8)(UMPM_UART1_PORT + port_number),
				0,	/* clear */
				NULL,
				0);
}

static int TIRestoreMCR (struct edgeport_port *port, __u8 mcr)
{
	int status = 0;

	dbg ("%s - %x", __FUNCTION__, mcr);

	if (mcr & MCR_DTR)
		status = TISetDtr (port);
	else
		status = TIClearDtr (port);

	if (status)
		return status;

	if (mcr & MCR_RTS)
		status = TISetRts (port);
	else
		status = TIClearRts (port);

	if (status)
		return status;

	if (mcr & MCR_LOOPBACK)
		status = TISetLoopBack (port);
	else
		status = TIClearLoopBack (port);

	return status;
}



/* Convert TI LSR to standard UART flags */
static __u8 MapLineStatus (__u8 ti_lsr)
{
	__u8 lsr = 0;

#define MAP_FLAG(flagUmp, flagUart)    \
	if (ti_lsr & flagUmp) lsr |= flagUart;

	MAP_FLAG(UMP_UART_LSR_OV_MASK, LSR_OVER_ERR)	/* overrun */
	MAP_FLAG(UMP_UART_LSR_PE_MASK, LSR_PAR_ERR)	/* parity error */
	MAP_FLAG(UMP_UART_LSR_FE_MASK, LSR_FRM_ERR)	/* framing error */
	MAP_FLAG(UMP_UART_LSR_BR_MASK, LSR_BREAK)	/* break detected */
	MAP_FLAG(UMP_UART_LSR_RX_MASK, LSR_RX_AVAIL)	/* receive data available */
	MAP_FLAG(UMP_UART_LSR_TX_MASK, LSR_TX_EMPTY)	/* transmit holding register empty */

#undef MAP_FLAG

	return lsr;
}

static void handle_new_msr (struct edgeport_port *edge_port, __u8 msr)
{
	struct async_icount *icount;

	dbg ("%s - %02x", __FUNCTION__, msr);

	if (msr & (EDGEPORT_MSR_DELTA_CTS | EDGEPORT_MSR_DELTA_DSR | EDGEPORT_MSR_DELTA_RI | EDGEPORT_MSR_DELTA_CD)) {
		icount = &edge_port->icount;

		/* update input line counters */
		if (msr & EDGEPORT_MSR_DELTA_CTS)
			icount->cts++;
		if (msr & EDGEPORT_MSR_DELTA_DSR)
			icount->dsr++;
		if (msr & EDGEPORT_MSR_DELTA_CD)
			icount->dcd++;
		if (msr & EDGEPORT_MSR_DELTA_RI)
			icount->rng++;
		wake_up_interruptible (&edge_port->delta_msr_wait);
	}

	/* Save the new modem status */
	edge_port->shadow_msr = msr & 0xf0;

	return;
}

static void handle_new_lsr (struct edgeport_port *edge_port, int lsr_data, __u8 lsr, __u8 data)
{
	struct async_icount *icount;
	__u8 new_lsr = (__u8)(lsr & (__u8)(LSR_OVER_ERR | LSR_PAR_ERR | LSR_FRM_ERR | LSR_BREAK));

	dbg ("%s - %02x", __FUNCTION__, new_lsr);

	edge_port->shadow_lsr = lsr;

	if (new_lsr & LSR_BREAK) {
		/*
		 * Parity and Framing errors only count if they
		 * occur exclusive of a break being received.
		 */
		new_lsr &= (__u8)(LSR_OVER_ERR | LSR_BREAK);
	}

	/* Place LSR data byte into Rx buffer */
	if (lsr_data && edge_port->port->tty) {
		tty_insert_flip_char(edge_port->port->tty, data, 0);
		tty_flip_buffer_push(edge_port->port->tty);
	}

	/* update input line counters */
	icount = &edge_port->icount;
	if (new_lsr & LSR_BREAK)
		icount->brk++;
	if (new_lsr & LSR_OVER_ERR)
		icount->overrun++;
	if (new_lsr & LSR_PAR_ERR)
		icount->parity++;
	if (new_lsr & LSR_FRM_ERR)
		icount->frame++;
}


static void edge_interrupt_callback (struct urb *urb)
{
	struct edgeport_serial	*edge_serial = (struct edgeport_serial *)urb->context;
	struct usb_serial_port *port;
	struct edgeport_port *edge_port;
	unsigned char *data = urb->transfer_buffer;
	int length = urb->actual_length;
	int port_number;
	int function;
	__u8 lsr;
	__u8 msr;

	dbg("%s", __FUNCTION__);

	if (serial_paranoia_check (edge_serial->serial, __FUNCTION__)) {
		return;
	}

	if (urb->status) {
		dbg("%s - nonzero control read status received: %d", __FUNCTION__, urb->status);
		return;
	}

	if (!length) {
		dbg ("%s - no data in urb", __FUNCTION__);
		return;
	}
		
	usb_serial_debug_data (__FILE__, __FUNCTION__, length, data);
		
	if (length != 2) {
		dbg ("%s - expecting packet of size 2, got %d", __FUNCTION__, length);
		return;
	}

	port_number = TIUMP_GET_PORT_FROM_CODE (data[0]);
	function    = TIUMP_GET_FUNC_FROM_CODE (data[0]);
	dbg ("%s - port_number %d, function %d, info 0x%x",
	     __FUNCTION__, port_number, function, data[1]);
	port = &edge_serial->serial->port[port_number];
	if (port_paranoia_check (port, __FUNCTION__)) {
		dbg ("%s - change found for port that is not present",
		     __FUNCTION__);
		return;
	}
	edge_port = port->private;
	if (!edge_port) {
		dbg ("%s - edge_port not found", __FUNCTION__);
		return;
	}
	switch (function) {
	case TIUMP_INTERRUPT_CODE_LSR:
		lsr = MapLineStatus(data[1]);
		if (lsr & UMP_UART_LSR_DATA_MASK) {
			/* Save the LSR event for bulk read completion routine */
			dbg ("%s - LSR Event Port %u LSR Status = %02x",
			     __FUNCTION__, port_number, lsr);
			edge_port->lsr_event = 1;
			edge_port->lsr_mask = lsr;
		} else {
			dbg ("%s - ===== Port %d LSR Status = %02x ======",
			     __FUNCTION__, port_number, lsr);
			handle_new_lsr (edge_port, 0, lsr, 0);
		}
		break;

	case TIUMP_INTERRUPT_CODE_MSR:	// MSR
		/* Copy MSR from UMP */
		msr = data[1];
		dbg ("%s - ===== Port %u MSR Status = %02x ======\n",
		     __FUNCTION__, port_number, msr);
		handle_new_msr (edge_port, msr);
		break;

	default:
		err ("%s - Unknown Interrupt code from UMP %x\n",
		     __FUNCTION__, data[1]);
		break;
		
	}
}

static void edge_bulk_in_callback (struct urb *urb)
{
	struct edgeport_port *edge_port = (struct edgeport_port *)urb->context;
	unsigned char *data = urb->transfer_buffer;
	struct tty_struct *tty;
	int status;
	int i;
	int port_number;

	dbg("%s", __FUNCTION__);

	if (port_paranoia_check (edge_port->port, __FUNCTION__))
		return;

	if (urb->status) {
		dbg ("%s - nonzero read bulk status received: %d",
		     __FUNCTION__, urb->status);

		if (urb->status == -EPIPE) {
			/* clear any problem that might have happened on this pipe */
			usb_clear_halt (edge_port->port->serial->dev, urb->pipe);
			goto exit;
		}
		return;
	}

	port_number = edge_port->port->number - edge_port->port->serial->minor;

	if (edge_port->lsr_event) {
		edge_port->lsr_event = 0;
		dbg ("%s ===== Port %u LSR Status = %02x, Data = %02x ======",
		     __FUNCTION__, port_number, edge_port->lsr_mask, *data);
		handle_new_lsr (edge_port, 1, edge_port->lsr_mask, *data);
		/* Adjust buffer length/pointer */
		--urb->actual_length;
		++data;
	}

	tty = edge_port->port->tty;
	if (tty && urb->actual_length) {
		usb_serial_debug_data (__FILE__, __FUNCTION__, urb->actual_length, data);

		if (edge_port->close_pending) {
			dbg ("%s - close is pending, dropping data on the floor.", __FUNCTION__);
		} else {
			for (i = 0; i < urb->actual_length ; ++i) {
				/* if we insert more than TTY_FLIPBUF_SIZE characters,
				 * we drop them. */
				if (tty->flip.count >= TTY_FLIPBUF_SIZE) {
					tty_flip_buffer_push(tty);
				}
				/* this doesn't actually push the data through unless
				 * tty->low_latency is set */
				tty_insert_flip_char(tty, data[i], 0);
			}
			tty_flip_buffer_push(tty);
		}
		edge_port->icount.rx += urb->actual_length;
	}

exit:
	/* continue always trying to read */
	urb->dev = edge_port->port->serial->dev;
	status = usb_submit_urb (urb);
	if (status)
		err ("%s - usb_submit_urb failed with result %d",
		     __FUNCTION__, status);
}

static void edge_bulk_out_callback (struct urb *urb)
{
	struct usb_serial_port *port = (struct usb_serial_port *)urb->context;
	struct usb_serial *serial = get_usb_serial (port, __FUNCTION__);
	struct tty_struct *tty;

	dbg ("%s - port %d", __FUNCTION__, port->number);

	if (!serial) {
		dbg ("%s - bad serial pointer, exiting", __FUNCTION__);
		return;
	}

	if (urb->status) {
		dbg ("%s - nonzero write bulk status received: %d",
		     __FUNCTION__, urb->status);

		if (urb->status == -EPIPE) {
			/* clear any problem that might have happened on this pipe */
			usb_clear_halt (serial->dev, urb->pipe);
		}
		return;
	}

	tty = port->tty;
	if (tty) {
		/* let the tty driver wakeup if it has a special write_wakeup function */
		if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) && tty->ldisc.write_wakeup) {
			(tty->ldisc.write_wakeup)(tty);
		}

		/* tell the tty driver that something has changed */
		wake_up_interruptible(&tty->write_wait);
	}
}

static int edge_open (struct usb_serial_port *port, struct file * filp)
{
	struct edgeport_port *edge_port = (struct edgeport_port *)port->private;
	struct edgeport_serial *edge_serial;
	struct usb_device *dev;
	struct urb *urb;
	int port_number;
	int status;
	u16 open_settings;
	u8 transaction_timeout;

	if (port_paranoia_check (port, __FUNCTION__))
		return -ENODEV;
	
	dbg("%s - port %d", __FUNCTION__, port->number);

	if (edge_port == NULL)
		return -ENODEV;

	/* force low_latency on so that our tty_push actually forces the data through, 
	   otherwise it is scheduled, and with high data rates (like with OHCI) data
	   can get lost. */
	if (port->tty)
		port->tty->low_latency = 1;

	port_number = port->number - port->serial->minor;
	switch (port_number) {
		case 0:
			edge_port->uart_base = UMPMEM_BASE_UART1;
			edge_port->dma_address = UMPD_OEDB1_ADDRESS;
			break;
		case 1:
			edge_port->uart_base = UMPMEM_BASE_UART2;
			edge_port->dma_address = UMPD_OEDB2_ADDRESS;
			break;
		default:
			err ("Unknown port number!!!");
			return -ENODEV;
	}

	dbg ("%s - port_number = %d, uart_base = %04x, dma_address = %04x",
	     __FUNCTION__, port_number, edge_port->uart_base, edge_port->dma_address);

	dev = port->serial->dev;

	memset (&(edge_port->icount), 0x00, sizeof(edge_port->icount));
	init_waitqueue_head (&edge_port->delta_msr_wait);

	/* turn off loopback */
	status = TIClearLoopBack (edge_port);
	if (status)
		return status;
	
	/* set up the port settings */
	edge_set_termios (port, NULL);

	/* open up the port */

	/* milliseconds to timeout for DMA transfer */
	transaction_timeout = 2;

	edge_port->ump_read_timeout = max (20, ((transaction_timeout * 3) / 2) );

	// milliseconds to timeout for DMA transfer
	open_settings = (u8)(UMP_DMA_MODE_CONTINOUS | 
			     UMP_PIPE_TRANS_TIMEOUT_ENA | 
			     (transaction_timeout << 2));

	dbg ("%s - Sending UMPC_OPEN_PORT", __FUNCTION__);

	/* Tell TI to open and start the port */
	status = TIWriteCommandSync (dev,
					UMPC_OPEN_PORT,
					(u8)(UMPM_UART1_PORT + port_number),
					open_settings,
					NULL,
					0);
	if (status)
		return status;

	/* Start the DMA? */
	status = TIWriteCommandSync (dev,
					UMPC_START_PORT,
					(u8)(UMPM_UART1_PORT + port_number),
					0,
					NULL,
					0);
	if (status)
		return status;

	/* Clear TX and RX buffers in UMP */
	status = TIPurgeDataSync (port, UMP_PORT_DIR_OUT | UMP_PORT_DIR_IN);
	if (status)
		return status;

	/* Read Initial MSR */
	status = TIReadVendorRequestSync (dev,
					UMPC_READ_MSR,	// Request
					0,		// wValue
					(__u16)(UMPM_UART1_PORT + port_number),	// wIndex (Address)
					&edge_port->shadow_msr,			// TransferBuffer
					1);					// TransferBufferLength
	if (status)
		return status;

	dbg ("ShadowMSR 0x%X", edge_port->shadow_msr);
 
	edge_serial = edge_port->edge_serial;
	if (edge_serial->num_ports_open == 0) {
		dbg ("%s - setting up bulk in urb", __FUNCTION__);
		/* we are the first port to be opened, let's post the interrupt urb */
		urb = edge_serial->serial->port[0].interrupt_in_urb;
		if (!urb) {
			err ("%s - no interrupt urb present, exiting", __FUNCTION__);
			return -EINVAL;
		}
		urb->complete = edge_interrupt_callback;
		urb->context = edge_serial;
		urb->dev = dev;
		status = usb_submit_urb (urb);
		if (status) {
			err ("%s - usb_submit_urb failed with value %d", __FUNCTION__, status);
			return status;
		}
	}

	/*
	 * reset the data toggle on the bulk endpoints to work around bug in
	 * host controllers where things get out of sync some times
	 */
	usb_clear_halt (dev, port->write_urb->pipe);
	usb_clear_halt (dev, port->read_urb->pipe);

	/* start up our bulk read urb */
	urb = port->read_urb;
	if (!urb) {
		err ("%s - no read urb present, exiting", __FUNCTION__);
		return -EINVAL;
	}
	urb->complete = edge_bulk_in_callback;
	urb->context = edge_port;
	urb->dev = dev;
	status = usb_submit_urb (urb);
	if (status) {
		err ("%s - read bulk usb_submit_urb failed with value %d", __FUNCTION__, status);
		return status;
	}

	++edge_serial->num_ports_open;

	dbg("%s - exited", __FUNCTION__);

	return 0;
}

static void edge_close (struct usb_serial_port *port, struct file * filp)
{
	struct usb_serial *serial;
	struct edgeport_serial *edge_serial;
	struct edgeport_port *edge_port;
	int port_number;
	int status;

	if (port_paranoia_check (port, __FUNCTION__))
		return;
	
	dbg("%s - port %d", __FUNCTION__, port->number);
			 
	serial = get_usb_serial (port, __FUNCTION__);
	if (!serial)
		return;
	
	edge_serial = (struct edgeport_serial *)serial->private;
	edge_port = (struct edgeport_port *)port->private;
	if ((edge_serial == NULL) || (edge_port == NULL))
		return;
	
	if (serial->dev) {
		/* The bulkreadcompletion routine will check 
		 * this flag and dump add read data */
		edge_port->close_pending = 1;

		/* chase the port close */
		TIChasePort (edge_port);

		usb_unlink_urb (port->read_urb);

		/* assuming we can still talk to the device,
		 * send a close port command to it */
		dbg("%s - send umpc_close_port", __FUNCTION__);
		port_number = port->number - port->serial->minor;
		status = TIWriteCommandSync (port->serial->dev,
					     UMPC_CLOSE_PORT,
					     (__u8)(UMPM_UART1_PORT + port_number),
					     0,
					     NULL,
					     0);
		--edge_port->edge_serial->num_ports_open;
		if (edge_port->edge_serial->num_ports_open <= 0) {
			/* last port is now closed, let's shut down our interrupt urb */
			usb_unlink_urb (serial->port[0].interrupt_in_urb);
			edge_port->edge_serial->num_ports_open = 0;
		}
	edge_port->close_pending = 0;
	}

	dbg("%s - exited", __FUNCTION__);
}

static int edge_write (struct usb_serial_port *port, int from_user, const unsigned char *data, int count)
{
	struct usb_serial *serial = port->serial;
	struct edgeport_port *edge_port = port->private;
	int result;

	dbg("%s - port %d", __FUNCTION__, port->number);

	if (count == 0) {
		dbg("%s - write request of 0 bytes", __FUNCTION__);
		return 0;
	}

	if (edge_port == NULL)
		return -ENODEV;
	if (edge_port->close_pending == 1)
		return -ENODEV;
	
	if (port->write_urb->status == -EINPROGRESS) {
		dbg ("%s - already writing", __FUNCTION__);
		return 0;
	}

	count = min (count, port->bulk_out_size);

	if (from_user) {
		if (copy_from_user(port->write_urb->transfer_buffer, data, count))
			return -EFAULT;
	} else {
		memcpy (port->write_urb->transfer_buffer, data, count);
	}

	usb_serial_debug_data (__FILE__, __FUNCTION__, count, port->write_urb->transfer_buffer);

	/* set up our urb */
	usb_fill_bulk_urb (port->write_urb, serial->dev,
			   usb_sndbulkpipe (serial->dev,
					    port->bulk_out_endpointAddress),
			   port->write_urb->transfer_buffer, count,
			   edge_bulk_out_callback,
			   port);

	/* send the data out the bulk port */
	result = usb_submit_urb(port->write_urb);
	if (result)
		err("%s - failed submitting write urb, error %d", __FUNCTION__, result);
	else
		result = count;

	if (result > 0)
		edge_port->icount.tx += count;

	return result;
}

static int edge_write_room (struct usb_serial_port *port)
{
	struct edgeport_port *edge_port = (struct edgeport_port *)(port->private);
	int room = 0;

	dbg("%s", __FUNCTION__);

	if (edge_port == NULL)
		return -ENODEV;
	if (edge_port->close_pending == 1)
		return -ENODEV;
	
	dbg("%s - port %d", __FUNCTION__, port->number);

	if (port->write_urb->status != -EINPROGRESS)
		room = port->bulk_out_size;

	dbg("%s - returns %d", __FUNCTION__, room);
	return room;
}

static int edge_chars_in_buffer (struct usb_serial_port *port)
{
	struct edgeport_port *edge_port = (struct edgeport_port *)(port->private);
	int chars = 0;

	dbg("%s", __FUNCTION__);

	if (edge_port == NULL)
		return -ENODEV;
	if (edge_port->close_pending == 1)
		return -ENODEV;

	dbg("%s - port %d", __FUNCTION__, port->number);

	if (port->write_urb->status == -EINPROGRESS)
		chars = port->write_urb->transfer_buffer_length;

	dbg ("%s - returns %d", __FUNCTION__, chars);
	return chars;
}

static void edge_throttle (struct usb_serial_port *port)
{
	struct edgeport_port *edge_port = (struct edgeport_port *)(port->private);
	struct tty_struct *tty;
	int status;

	dbg("%s - port %d", __FUNCTION__, port->number);

	if (edge_port == NULL)
		return;

	tty = port->tty;
	if (!tty) {
		dbg ("%s - no tty available", __FUNCTION__);
		return;
	}
	/* if we are implementing XON/XOFF, send the stop character */
	if (I_IXOFF(tty)) {
		unsigned char stop_char = STOP_CHAR(tty);
		status = edge_write (port, 0, &stop_char, 1);
		if (status <= 0) {
			return;
		}
	}

	/* if we are implementing RTS/CTS, toggle that line */
	if (tty->termios->c_cflag & CRTSCTS) {
		status = TIClearRts (edge_port);
	}

	usb_unlink_urb (port->read_urb);
}

static void edge_unthrottle (struct usb_serial_port *port)
{
	struct edgeport_port *edge_port = (struct edgeport_port *)(port->private);
	struct tty_struct *tty;
	int status;

	dbg("%s - port %d", __FUNCTION__, port->number);

	if (edge_port == NULL)
		return;

	tty = port->tty;
	if (!tty) {
		dbg ("%s - no tty available", __FUNCTION__);
		return;
	}

	/* if we are implementing XON/XOFF, send the start character */
	if (I_IXOFF(tty)) {
		unsigned char start_char = START_CHAR(tty);
		status = edge_write (port, 0, &start_char, 1);
		if (status <= 0) {
			return;
		}
	}

	/* if we are implementing RTS/CTS, toggle that line */
	if (tty->termios->c_cflag & CRTSCTS) {
		status = TISetRts (edge_port);
	}

	port->read_urb->dev = port->serial->dev;
	status = usb_submit_urb (port->read_urb);
	if (status) {
		err ("%s - usb_submit_urb failed with value %d", __FUNCTION__, status);
	}
}


static void change_port_settings (struct edgeport_port *edge_port, struct termios *old_termios)
{
	struct ump_uart_config *config;
	struct tty_struct *tty;
	int baud;
	int round;
	unsigned cflag;
	int status;
	int port_number = edge_port->port->number - edge_port->port->serial->minor;

	dbg("%s - port %d", __FUNCTION__, edge_port->port->number);

	tty = edge_port->port->tty;
	if ((!tty) ||
	    (!tty->termios)) {
		dbg("%s - no tty structures", __FUNCTION__);
		return;
	}

	config = kmalloc (sizeof (*config), GFP_KERNEL);
	if (!config) {
		err ("%s - out of memory", __FUNCTION__);
		return;
	}

	cflag = tty->termios->c_cflag;

	config->wFlags = 0;

	/* These flags must be set */
	config->wFlags |= UMP_MASK_UART_FLAGS_RECEIVE_MS_INT;
	config->wFlags |= UMP_MASK_UART_FLAGS_AUTO_START_ON_ERR;
	config->bUartMode = 0;

	switch (cflag & CSIZE) {
		case CS5:
			    config->bDataBits = UMP_UART_CHAR5BITS;
			    dbg ("%s - data bits = 5", __FUNCTION__);
			    break;
		case CS6:
			    config->bDataBits = UMP_UART_CHAR6BITS;
			    dbg ("%s - data bits = 6", __FUNCTION__);
			    break;
		case CS7:
			    config->bDataBits = UMP_UART_CHAR7BITS;
			    dbg ("%s - data bits = 7", __FUNCTION__);
			    break;
		default:
		case CS8:
			    config->bDataBits = UMP_UART_CHAR8BITS;
			    dbg ("%s - data bits = 8", __FUNCTION__);
			    break;
	}

	if (cflag & PARENB) {
		if (cflag & PARODD) {
			config->wFlags |= UMP_MASK_UART_FLAGS_PARITY;
			config->bParity = UMP_UART_ODDPARITY;
			dbg("%s - parity = odd", __FUNCTION__);
		} else {
			config->wFlags |= UMP_MASK_UART_FLAGS_PARITY;
			config->bParity = UMP_UART_EVENPARITY;
			dbg("%s - parity = even", __FUNCTION__);
		}
	} else {
		config->bParity = UMP_UART_NOPARITY; 	
		dbg("%s - parity = none", __FUNCTION__);
	}

	if (cflag & CSTOPB) {
		config->bStopBits = UMP_UART_STOPBIT2;
		dbg("%s - stop bits = 2", __FUNCTION__);
	} else {
		config->bStopBits = UMP_UART_STOPBIT1;
		dbg("%s - stop bits = 1", __FUNCTION__);
	}

	/* figure out the flow control settings */
	if (cflag & CRTSCTS) {
		config->wFlags |= UMP_MASK_UART_FLAGS_OUT_X_CTS_FLOW;
		config->wFlags |= UMP_MASK_UART_FLAGS_RTS_FLOW;
		dbg("%s - RTS/CTS is enabled", __FUNCTION__);
	} else {
		dbg("%s - RTS/CTS is disabled", __FUNCTION__);
	}

	/* if we are implementing XON/XOFF, set the start and stop character in the device */
	if (I_IXOFF(tty) || I_IXON(tty)) {
		config->cXon  = START_CHAR(tty);
		config->cXoff = STOP_CHAR(tty);

		/* if we are implementing INBOUND XON/XOFF */
		if (I_IXOFF(tty)) {
			config->wFlags |= UMP_MASK_UART_FLAGS_IN_X;
			dbg ("%s - INBOUND XON/XOFF is enabled, XON = %2x, XOFF = %2x",
			     __FUNCTION__, config->cXon, config->cXoff);
		} else {
			dbg ("%s - INBOUND XON/XOFF is disabled", __FUNCTION__);
		}

		/* if we are implementing OUTBOUND XON/XOFF */
		if (I_IXON(tty)) {
			config->wFlags |= UMP_MASK_UART_FLAGS_OUT_X;
			dbg ("%s - OUTBOUND XON/XOFF is enabled, XON = %2x, XOFF = %2x",
			     __FUNCTION__, config->cXon, config->cXoff);
		} else {
			dbg ("%s - OUTBOUND XON/XOFF is disabled", __FUNCTION__);
		}
	}

	/* Round the baud rate */
	baud = tty_get_baud_rate(tty);
	if (!baud) {
		/* pick a default, any default... */
		baud = 9600;
	}
	config->wBaudRate = (__u16)(461550L / baud);
	round = 4615500L / baud;
	if ((round - (config->wBaudRate * 10)) >= 5)
		config->wBaudRate++;

	dbg ("%s - baud rate = %d, wBaudRate = %d", __FUNCTION__, baud, config->wBaudRate);

	dbg ("wBaudRate:   %d", (int)(461550L / config->wBaudRate));
	dbg ("wFlags:    0x%x", config->wFlags);
	dbg ("bDataBits:   %d", config->bDataBits);
	dbg ("bParity:     %d", config->bParity);
	dbg ("bStopBits:   %d", config->bStopBits);
	dbg ("cXon:        %d", config->cXon);
	dbg ("cXoff:       %d", config->cXoff);
	dbg ("bUartMode:   %d", config->bUartMode);

	/* move the word values into big endian mode */
	cpu_to_be16s (&config->wFlags);
	cpu_to_be16s (&config->wBaudRate);

	status = TIWriteCommandSync (edge_port->port->serial->dev,
				UMPC_SET_CONFIG,
				(__u8)(UMPM_UART1_PORT + port_number),
				0,
				(__u8 *)config,
				sizeof(*config));
	if (status) {
		dbg ("%s - error %d when trying to write config to device",
		     __FUNCTION__, status);
	}

	kfree (config);
	
	return;
}

static void edge_set_termios (struct usb_serial_port *port, struct termios *old_termios)
{
	struct edgeport_port *edge_port = (struct edgeport_port *)(port->private);
	struct tty_struct *tty = port->tty;
	unsigned int cflag;

	if (!port->tty || !port->tty->termios) {
		dbg ("%s - no tty or termios", __FUNCTION__);
		return;
	}

	cflag = tty->termios->c_cflag;
	/* check that they really want us to change something */
	if (old_termios) {
		if ((cflag == old_termios->c_cflag) &&
		    (RELEVANT_IFLAG(tty->termios->c_iflag) == RELEVANT_IFLAG(old_termios->c_iflag))) {
			dbg ("%s - nothing to change", __FUNCTION__);
			return;
		}
	}

	dbg("%s - clfag %08x iflag %08x", __FUNCTION__, 
	    tty->termios->c_cflag,
	    RELEVANT_IFLAG(tty->termios->c_iflag));
	if (old_termios) {
		dbg("%s - old clfag %08x old iflag %08x", __FUNCTION__,
		    old_termios->c_cflag,
		    RELEVANT_IFLAG(old_termios->c_iflag));
	}

	dbg("%s - port %d", __FUNCTION__, port->number);

	if (edge_port == NULL)
		return;

	/* change the port settings to the new ones specified */
	change_port_settings (edge_port, old_termios);

	return;
}

static int set_modem_info (struct edgeport_port *edge_port, unsigned int cmd, unsigned int *value)
{
	unsigned int mcr = edge_port->shadow_mcr;
	unsigned int arg;

	if (copy_from_user(&arg, value, sizeof(int)))
		return -EFAULT;

	switch (cmd) {
		case TIOCMBIS:
			if (arg & TIOCM_RTS)
				mcr |= MCR_RTS;
			if (arg & TIOCM_DTR)
				mcr |= MCR_RTS;
			if (arg & TIOCM_LOOP)
				mcr |= MCR_LOOPBACK;
			break;

		case TIOCMBIC:
			if (arg & TIOCM_RTS)
				mcr &= ~MCR_RTS;
			if (arg & TIOCM_DTR)
				mcr &= ~MCR_RTS;
			if (arg & TIOCM_LOOP)
				mcr &= ~MCR_LOOPBACK;
			break;

		case TIOCMSET:
			/* turn off the RTS and DTR and LOOPBACK 
			 * and then only turn on what was asked to */
			mcr &=  ~(MCR_RTS | MCR_DTR | MCR_LOOPBACK);
			mcr |= ((arg & TIOCM_RTS) ? MCR_RTS : 0);
			mcr |= ((arg & TIOCM_DTR) ? MCR_DTR : 0);
			mcr |= ((arg & TIOCM_LOOP) ? MCR_LOOPBACK : 0);
			break;
	}

	edge_port->shadow_mcr = mcr;

	TIRestoreMCR (edge_port, mcr);

	return 0;
}

static int get_modem_info (struct edgeport_port *edge_port, unsigned int *value)
{
	unsigned int result = 0;
	unsigned int msr = edge_port->shadow_msr;
	unsigned int mcr = edge_port->shadow_mcr;

	result = ((mcr & MCR_DTR)	? TIOCM_DTR: 0)	  /* 0x002 */
		  | ((mcr & MCR_RTS)	? TIOCM_RTS: 0)   /* 0x004 */
		  | ((msr & EDGEPORT_MSR_CTS)	? TIOCM_CTS: 0)   /* 0x020 */
		  | ((msr & EDGEPORT_MSR_CD)	? TIOCM_CAR: 0)   /* 0x040 */
		  | ((msr & EDGEPORT_MSR_RI)	? TIOCM_RI:  0)   /* 0x080 */
		  | ((msr & EDGEPORT_MSR_DSR)	? TIOCM_DSR: 0);  /* 0x100 */


	dbg("%s -- %x", __FUNCTION__, result);

	if (copy_to_user(value, &result, sizeof(int)))
		return -EFAULT;
	return 0;
}

static int get_serial_info (struct edgeport_port *edge_port, struct serial_struct * retinfo)
{
	struct serial_struct tmp;

	if (!retinfo)
		return -EFAULT;

	memset(&tmp, 0, sizeof(tmp));

	tmp.type		= PORT_16550A;
	tmp.line		= edge_port->port->serial->minor;
	tmp.port		= edge_port->port->number;
	tmp.irq			= 0;
	tmp.flags		= ASYNC_SKIP_TEST | ASYNC_AUTO_IRQ;
	tmp.xmit_fifo_size	= edge_port->port->bulk_out_size;
	tmp.baud_base		= 9600;
	tmp.close_delay		= 5*HZ;
	tmp.closing_wait	= 30*HZ;
//	tmp.custom_divisor	= state->custom_divisor;
//	tmp.hub6		= state->hub6;
//	tmp.io_type		= state->io_type;


	if (copy_to_user(retinfo, &tmp, sizeof(*retinfo)))
		return -EFAULT;
	return 0;
}

static int edge_ioctl (struct usb_serial_port *port, struct file *file, unsigned int cmd, unsigned long arg)
{
	struct edgeport_port *edge_port = (struct edgeport_port *)(port->private);
	struct async_icount cnow;
	struct async_icount cprev;

	dbg("%s - port %d, cmd = 0x%x", __FUNCTION__, port->number, cmd);

	switch (cmd) {
		case TIOCINQ:
			dbg("%s - (%d) TIOCINQ", __FUNCTION__, port->number);
//			return get_number_bytes_avail(edge_port, (unsigned int *) arg);
			break;

		case TIOCSERGETLSR:
			dbg("%s - (%d) TIOCSERGETLSR", __FUNCTION__, port->number);
//			return get_lsr_info(edge_port, (unsigned int *) arg);
			break;

		case TIOCMBIS:
		case TIOCMBIC:
		case TIOCMSET:
			dbg("%s - (%d) TIOCMSET/TIOCMBIC/TIOCMSET", __FUNCTION__, port->number);
			return set_modem_info(edge_port, cmd, (unsigned int *) arg);
			break;

		case TIOCMGET:  
			dbg("%s - (%d) TIOCMGET", __FUNCTION__, port->number);
			return get_modem_info(edge_port, (unsigned int *) arg);
			break;

		case TIOCGSERIAL:
			dbg("%s - (%d) TIOCGSERIAL", __FUNCTION__, port->number);
			return get_serial_info(edge_port, (struct serial_struct *) arg);
			break;

		case TIOCSSERIAL:
			dbg("%s - (%d) TIOCSSERIAL", __FUNCTION__, port->number);
			break;

		case TIOCMIWAIT:
			dbg("%s - (%d) TIOCMIWAIT", __FUNCTION__, port->number);
			cprev = edge_port->icount;
			while (1) {
				interruptible_sleep_on(&edge_port->delta_msr_wait);
				/* see if a signal did it */
				if (signal_pending(current))
					return -ERESTARTSYS;
				cnow = edge_port->icount;
				if (cnow.rng == cprev.rng && cnow.dsr == cprev.dsr &&
				    cnow.dcd == cprev.dcd && cnow.cts == cprev.cts)
					return -EIO; /* no change => error */
				if (((arg & TIOCM_RNG) && (cnow.rng != cprev.rng)) ||
				    ((arg & TIOCM_DSR) && (cnow.dsr != cprev.dsr)) ||
				    ((arg & TIOCM_CD)  && (cnow.dcd != cprev.dcd)) ||
				    ((arg & TIOCM_CTS) && (cnow.cts != cprev.cts)) ) {
					return 0;
				}
				cprev = cnow;
			}
			/* not reached */
			break;

		case TIOCGICOUNT:
			dbg ("%s - (%d) TIOCGICOUNT RX=%d, TX=%d", __FUNCTION__,
			     port->number, edge_port->icount.rx, edge_port->icount.tx);
			if (copy_to_user((void *)arg, &edge_port->icount, sizeof(edge_port->icount)))
				return -EFAULT;
			return 0;
	}

	return -ENOIOCTLCMD;
}

static void edge_break (struct usb_serial_port *port, int break_state)
{
	struct edgeport_port *edge_port = (struct edgeport_port *)(port->private);
	int status;

	dbg ("%s - state = %d", __FUNCTION__, break_state);

	/* chase the port close */
	TIChasePort (edge_port);

	if (break_state == -1) {
		status = TISetBreak (edge_port);
	} else {
		status = TIClearBreak (edge_port);
	}
	if (status) {
		dbg ("%s - error %d sending break set/clear command.",
		     __FUNCTION__, status);
	}
}

static int edge_startup (struct usb_serial *serial)
{
	struct edgeport_serial *edge_serial;
	struct edgeport_port *edge_port;
	struct usb_device *dev;
	int status;
	int i;

	dev = serial->dev;

	/* create our private serial structure */
	edge_serial = kmalloc (sizeof(struct edgeport_serial), GFP_KERNEL);
	if (edge_serial == NULL) {
		err("%s - Out of memory", __FUNCTION__);
		return -ENOMEM;
	}
	memset (edge_serial, 0, sizeof(struct edgeport_serial));
	edge_serial->serial = serial;
	serial->private = edge_serial;

	status = TIDownloadFirmware (edge_serial);
	if (status) {
		kfree (edge_serial);
		return status;
	}

	/* set up our port private structures */
	for (i = 0; i < serial->num_ports; ++i) {
		edge_port = kmalloc (sizeof(struct edgeport_port), GFP_KERNEL);
		if (edge_port == NULL) {
			err("%s - Out of memory", __FUNCTION__);
			return -ENOMEM;
		}
		memset (edge_port, 0, sizeof(struct edgeport_port));
		edge_port->port = &serial->port[i];
		edge_port->edge_serial = edge_serial;
		serial->port[i].private = edge_port;
	}
	
	return 0;
}

static void edge_shutdown (struct usb_serial *serial)
{
	int i;

	dbg ("%s", __FUNCTION__);

	for (i=0; i < serial->num_ports; ++i) {
		kfree (serial->port[i].private);
		serial->port[i].private = NULL;
	}
	kfree (serial->private);
	serial->private = NULL;
}


static struct usb_serial_device_type edgeport_1port_device = {
	owner:			THIS_MODULE,
	name:			"Edgeport TI 1 port adapter",
	id_table:		edgeport_1port_id_table,
	num_interrupt_in:	1,
	num_bulk_in:		1,
	num_bulk_out:		1,
	num_ports:		1,
	open:			edge_open,
	close:			edge_close,
	throttle:		edge_throttle,
	unthrottle:		edge_unthrottle,
	startup:		edge_startup,
	shutdown:		edge_shutdown,
	ioctl:			edge_ioctl,
	set_termios:		edge_set_termios,
	write:			edge_write,
	write_room:		edge_write_room,
	chars_in_buffer:	edge_chars_in_buffer,
	break_ctl:		edge_break,
};

static struct usb_serial_device_type edgeport_2port_device = {
	owner:			THIS_MODULE,
	name:			"Edgeport TI 2 port adapter",
	id_table:		edgeport_2port_id_table,
	num_interrupt_in:	1,
	num_bulk_in:		2,
	num_bulk_out:		2,
	num_ports:		2,
	open:			edge_open,
	close:			edge_close,
	throttle:		edge_throttle,
	unthrottle:		edge_unthrottle,
	startup:		edge_startup,
	shutdown:		edge_shutdown,
	ioctl:			edge_ioctl,
	set_termios:		edge_set_termios,
	write:			edge_write,
	write_room:		edge_write_room,
	chars_in_buffer:	edge_chars_in_buffer,
	break_ctl:		edge_break,
};


static int __init edgeport_init(void)
{
	usb_serial_register (&edgeport_1port_device);
	usb_serial_register (&edgeport_2port_device);
	info(DRIVER_DESC " " DRIVER_VERSION);
	return 0;
}

static void __exit edgeport_exit (void)
{
	usb_serial_deregister (&edgeport_1port_device);
	usb_serial_deregister (&edgeport_2port_device);
}

module_init(edgeport_init);
module_exit(edgeport_exit);

/* Module information */
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

MODULE_PARM(debug, "i");
MODULE_PARM_DESC(debug, "Debug enabled or not");

MODULE_PARM(ignore_cpu_rev, "i");
MODULE_PARM_DESC(ignore_cpu_rev, "Ignore the cpu revision when connecting to a device");

