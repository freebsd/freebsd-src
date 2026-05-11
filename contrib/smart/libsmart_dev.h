/*
 * Copyright (c) 2017-2021 Chuck Tuffli <chuck@tuffli.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifndef _LIBSMART_DEV_H
#define _LIBSMART_DEV_H

/**
 * Open a device to gather SMART information
 *
 * The call performs OS specific functions necessary to prepare the device
 * to receive read log requests.
 *
 * Although opaque to the user, the handle must be a pointer to a structure
 * with the first member being struct smart_s. The remaining members are OS
 * specific and are not used by the library.
 *
 * @param protocol The desired protocol or "auto" to automatically detect it
 * @param devname  The device name to open
 *
 * @return An opaque handle to the device or NULL on failure
 */
extern smart_h device_open(smart_protocol_e, char *);

/**
 * Close a device and release the associated resources
 *
 * @param handle The handle returned from device_open()
 *
 * @return None
 */
extern void device_close(smart_h);

/**
 * Read the log page
 *
 * This call reads the specified log page in the protocol specific manner
 * needed by the device. The results are placed in the provided buffer.
 *
 * @param h SMART handle returned from device_open()
 * @param page The log page ID
 * @param buf Pointer to buffer containing the results of the read
 * @param bsize Size of the buffer in bytes
 *
 * @return Zero on success, errno on failure
 */
extern int32_t device_read_log(smart_h, uint32_t, void *, size_t);

#endif /* !_LIBSMART_DEV_H */
