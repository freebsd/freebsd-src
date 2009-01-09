
$FreeBSD$

DESCRIPTION OF THE NEW USB API

The new USB 2.0 API consists of 5 functions. All transfer types are
managed using these functions. There is no longer need for separate
functions to setup INTERRUPT- and ISOCHRONOUS- transfers.

+--------------------------------------------------------------+
|                                                              |
|  "usb2_transfer_setup"   -  This function will allocate all  |
|                             necessary DMA memory and might   |
|                             sleep!                           |
|                                                              |
|  "usb2_transfer_unsetup" -  This function will stop the USB  |
|                             transfer, if it is currently     |
|                             active, release all DMA          |
|                             memory and might sleep!          |
|                                                              |
|  "usb2_transfer_start"   -  This function will start an USB  |
|                             transfer, if not already started.|
|                             This function is always          |
|                             non-blocking. **                 |
|                                                              |
|  "usb2_transfer_stop"    -  This function will stop an USB   |
|                             transfer, if not already stopped.|
|                             The callback function will be    |
|                             called before this function      |
|                             returns. This function is always |
|                             non-blocking. **                 |
|                                                              |
|  "usb2_transfer_drain"   -  This function will stop an USB   |
|                             transfer, if not already stopped |
|                             and wait for any additional      |
|                             DMA load operations to complete. |
|                             Buffers that are loaded into DMA |
|                             using "usb2_set_frame_data" can  |
|                             safely be freed after that       |
|                             this function has returned. This |
|                             function can block the caller.   |
|                                                              |
|  ** These functions must be called with the private driver's |
|     lock locked.                                             |
|                                                              |
|  NOTE: These USB API functions are NULL safe, with regard    |
|        to the USB transfer structure pointer.                |
+--------------------------------------------------------------+

Reference: /sys/dev/usb2/core/usb2_transfer.c 

/*
 * A simple USB callback state-machine:
 *
 *       +->-----------------------+
 *       |                         |    
 *   +-<-+-------[tr_setup]--------+-<-+-<-[start/restart]
 *   |                                 |
 *   |                                 |
 *   |                                 |
 *   +------>-[tr_transferred]---------+
 *   |                                 |
 *   +--------->-[tr_error]------------+
 */

void
usb2_default_callback(struct usb2_xfer *xfer)
{
	/*
	 * NOTE: it is not allowed to return
	 * before "USB_CHECK_STATUS()",
	 * even if the system is tearing down!
	 */
  switch (USB_GET_STATE(xfer)) {
  case USB_ST_SETUP: 
	/*
	 * Setup xfer->frlengths[], xfer->nframes
	 * and write data to xfer->frbuffers[], if any
	 */

	/**/
	usb2_start_hardware(xfer);
	return;

  case USB_ST_TRANSFERRED: 
	/* 
	 * Read data from xfer->frbuffers[], if any.
	 * "xfer->frlengths[]" should now have been
	 * updated to the actual length.
	 */
	return;

  default: /* Error */
	/* print error message and clear stall for example */
	return;
  }
}

=== Notes for USB control transfers ===

An USB control transfer has three parts. First the SETUP packet, then
DATA packet(s) and then a STATUS packet. The SETUP packet is always
pointed to by "xfer->frbuffers[0]" and the length is stored in
"xfer->frlengths[0]" also if there should not be sent any SETUP
packet! If an USB control transfer has no DATA stage, then
"xfer->nframes" should be set to 1. Else the default value is
"xfer->nframes" equal to 2.

Example1: SETUP + STATUS
 xfer->nframes = 1;
 xfer->frlenghts[0] = 8;
 usb2_start_hardware(xfer);

Example2: SETUP + DATA + STATUS
 xfer->nframes = 2;
 xfer->frlenghts[0] = 8;
 xfer->frlenghts[1] = 1;
 usb2_start_hardware(xfer);

Example3: SETUP + DATA + STATUS - split
1st callback:
 xfer->nframes = 1;
 xfer->frlenghts[0] = 8;
 usb2_start_hardware(xfer);

2nd callback:
 /* IMPORTANT: frbuffer[0] must still point at the setup packet! */
 xfer->nframes = 2;
 xfer->frlenghts[0] = 0;
 xfer->frlenghts[1] = 1;
 usb2_start_hardware(xfer);

Example4: SETUP + STATUS - split
1st callback:
 xfer->nframes = 1;
 xfer->frlenghts[0] = 8;
 xfer->flags.manual_status = 1;
 usb2_start_hardware(xfer);

2nd callback:
 xfer->nframes = 1;
 xfer->frlenghts[0] = 0;
 xfer->flags.manual_status = 0;
 usb2_start_hardware(xfer);


=== General USB transfer notes ===

 1) Something that one should be aware of is that all USB callbacks support 
recursation. That means one can start/stop whatever transfer from the callback 
of another transfer one desires. Also the transfer that is currently called 
back. Recursion is handled like this that when the callback that wants to 
recurse returns it is called one more time. 
 
 2) After that the "usb2_start_hardware()" function has been called in
the callback one can always depend on that "tr_error" or "tr_transferred"
will get jumped afterwards. Always!
 
 3) Sleeping functions can only be called from the attach routine of the 
driver. Else one should not use sleeping functions unless one has to. It is 
very difficult with sleep, because one has to think that the device might have 
detached when the thread returns from sleep. 

 4) Polling.

  use_polling
	This flag can be used with any callback and will cause the
	"usb2_transfer_start()" function to wait using "DELAY()",
	without exiting any mutexes, until the transfer is finished or
	has timed out. This flag can be changed during operation.

	NOTE: If polling is used the "timeout" field should be non-zero!
	NOTE: USB_ERR_CANCELLED is returned in case of timeout
	      instead of USB_ERR_TIMEOUT!


 
USB device driver examples: 

/sys/dev/usb2/ethernet/if_axe.c
/sys/dev/usb2/ethernet/if_aue.c

QUICK REFERENCE
===============


/*------------------------------------------------------------------------*
 * usb2_error_t 
 * usb2_transfer_setup(udev, ifaces, pxfer, setup_start,
 *                     n_setup, priv_sc, priv_mtx)
 *------------------------------------------------------------------------*/

- "udev" is a pointer to "struct usb2_device".

- "ifaces" array of interface index numbers to use. See "if_index".

- "pxfer" is a pointer to an array of USB transfer pointers that are
  initialized to NULL, and then pointed to allocated USB transfers.

- "setup_start" is a pointer to an array of USB config structures.

- "n_setup" is a number telling the USB system how many USB transfers
  should be setup.

- "priv_sc" is the private softc pointer, which will be used to
  initialize "xfer->priv_sc".

- "priv_mtx" is the private mutex protecting the transfer structure and
  the softc. This pointer is used to initialize "xfer->priv_mtx".

/*------------------------------------------------------------------------*
 * void
 * usb2_transfer_unsetup(pxfer, n_setup)
 *------------------------------------------------------------------------*/

- "pxfer" is a pointer to an array of USB transfer pointers, that may
   be NULL, that should be freed by the USB system.

- "n_setup" is a number telling the USB system how many USB transfers
  should be unsetup

NOTE: This function can sleep, waiting for active mutexes to become unlocked!
NOTE: It is not allowed to call "usb2_transfer_unsetup" from the callback
      of a USB transfer.

/*------------------------------------------------------------------------*
 * void
 * usb2_transfer_start(xfer)
 *------------------------------------------------------------------------*/

- "xfer" is pointer to a USB transfer that should be started

NOTE: this function must be called with "priv_mtx" locked

/*------------------------------------------------------------------------*
 * void
 * usb2_transfer_stop(xfer)
 *------------------------------------------------------------------------*/

- "xfer" is a pointer to a USB transfer that should be stopped

NOTE: this function must be called with "priv_mtx" locked

NOTE: if the transfer was in progress, the callback will called with 
      "xfer->error=USB_ERR_CANCELLED", before this function returns

/*------------------------------------------------------------------------*
 * struct usb2_config {
 *   type, endpoint, direction, interval, timeout, frames, index
 *   flags, bufsize, callback
 * };
 *------------------------------------------------------------------------*/

- The "type" field selects the USB pipe type. Valid values are:
  UE_INTERRUPT, UE_CONTROL, UE_BULK, UE_ISOCHRONOUS. The special
  value UE_BULK_INTR will select BULK and INTERRUPT pipes.
  This field is mandatory.

- The "endpoint" field selects the USB endpoint number. A value of
  0xFF, "-1" or "UE_ADDR_ANY" will select the first matching endpoint.
  This field is mandatory.

- The "direction" field selects the USB endpoint direction. A value of
  "UE_DIR_ANY" will select the first matching endpoint. Else valid
  values are: "UE_DIR_IN" and "UE_DIR_OUT". "UE_DIR_IN" and
  "UE_DIR_OUT" can be binary ORed by "UE_DIR_SID" which means that the
  direction will be swapped in case of USB_MODE_DEVICE. Note that
  "UE_DIR_IN" refers to the data transfer direction of the "IN" tokens
  and "UE_DIR_OUT" refers to the data transfer direction of the "OUT"
  tokens. This field is mandatory.

- The "interval" field selects the interrupt interval. The value of this 
  field is given in milliseconds and is independent of device speed. Depending
  on the endpoint type, this field has different meaning:

    UE_INTERRUPT)
	"0" use the default interrupt interval based on endpoint descriptor.
	"Else" use the given value for polling rate.

    UE_ISOCHRONOUS)
	"0" use default.
	"Else" the value is ignored.

    UE_BULK)
    UE_CONTROL)
	"0" no transfer pre-delay.
	"Else" a delay as given by this field in milliseconds is
	inserted before the hardware is started when
	"usb2_start_hardware()" is called.
	NOTE: The transfer timeout, if any, is started after that
	the pre-delay has elapsed! 

- The "timeout" field, if non-zero, will set the transfer timeout in
  milliseconds. If the "timeout" field is zero and the transfer type
  is ISOCHRONOUS a timeout of 250ms will be used.

- The "frames" field sets the maximum number of frames. If zero is
  specified it will yield the following results:

    UE_BULK)
    UE_INTERRUPT)
      xfer->nframes = 1;

    UE_CONTROL)
      xfer->nframes = 2;

    UE_ISOCHRONOUS)
      Not allowed. Will cause an error.

- The "ep_index" field allows you to give a number, in case more
  endpoints match the description, that selects which matching
  "ep_index" should be used.

- The "if_index" field allows you to select which of the interface
  numbers in the "ifaces" array parameter passed to "usb2_transfer_setup"
  that should be used when setting up the given USB transfer.

- The "flags" field has type "struct usb2_xfer_flags" and allows one
  to set initial flags an USB transfer. Valid flags are:

  force_short_xfer
	This flag forces the last transmitted USB packet to be short.
	A short packet has a length of less than "xfer->max_packet_size",
	which derives from "wMaxPacketSize". This flag can be changed
	during operation.

  short_xfer_ok
	This flag allows the received transfer length, "xfer->actlen" 
	to be less than "xfer->sumlen" upon completion of a transfer.
	This flag can be changed during operation.

  pipe_bof
	This flag causes a failing USB transfer to remain first
	in the PIPE queue except in the case of "xfer->error" equal
	to "USB_ERR_CANCELLED". No other USB transfers in the affected
	PIPE queue will be started until either:

	1) The failing USB transfer is stopped using "usb2_transfer_stop()".
	2) The failing USB transfer performs a successful transfer.

	The purpose of this flag is to avoid races when multiple
	transfers are queued for execution on an USB endpoint, and the
	first executing transfer fails leading to the need for
	clearing of stall for example. In this case this flag is used
	to prevent the following USB transfers from being executed at
	the same time the clear-stall command is executed on the USB
	control endpoint. This flag can be changed during operation.

	"BOF" is short for "Block On Failure"

	NOTE: This flag should be set on all BULK and INTERRUPT
	USB transfers which use an endpoint that can be shared
	between userland and kernel.

  proxy_buffer
	Setting this flag will cause that the total buffer size will
	be rounded up to the nearest atomic hardware transfer
	size. The maximum data length of any USB transfer is always
	stored in the "xfer->max_data_length". For control transfers
	the USB kernel will allocate additional space for the 8-bytes
	of SETUP header. These 8-bytes are not counted by the
	"xfer->max_data_length" variable. This flag can not be changed
	during operation.

  ext_buffer
	Setting this flag will cause that no data buffer will be
	allocated. Instead the USB client must supply a data buffer.
	This flag can not be changed during operation.

  manual_status
	Setting this flag prevents an USB STATUS stage to be appended
	to the end of the USB control transfer. If no control data is
	transferred this flag must be cleared. Else an error will be
	returned to the USB callback. This flag is mostly useful for
	the USB device side. This flag can be changed during
	operation.

  no_pipe_ok
	Setting this flag causes the USB_ERR_NO_PIPE error to be
	ignored. This flag can not be changed during operation.

  stall_pipe
	Setting this flag will cause STALL pids to be sent to the
	endpoint belonging to this transfer before the transfer is
	started. The transfer is started at the moment the host issues
	a clear-stall command on the STALL'ed endpoint. This flag can
	be changed during operation. This flag does only have effect
	in USB device side mode except for control endpoints. This
	flag is cleared when the stall command has been executed. This
	flag can only be changed outside the callback function by
	using the functions "usb2_transfer_set_stall()" and
	"usb2_transfer_clear_stall()" !

- The "bufsize" field sets the total buffer size in bytes. If
  this field is zero, "wMaxPacketSize" will be used, multiplied by the
  "frames" field if the transfer type is ISOCHRONOUS. This is useful for
  setting up interrupt pipes. This field is mandatory.

  NOTE: For control transfers "bufsize" includes
  the length of the request structure. 

- The "callback" pointer sets the USB callback. This field is mandatory.

MUTEX NOTE:
===========

When you create a mutex using "mtx_init()", don't forget to call
"mtx_destroy()" at detach, else you can get "freed memory accessed"
panics.

--HPS
