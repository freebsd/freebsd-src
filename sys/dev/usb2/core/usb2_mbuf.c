/* $FreeBSD$ */
/*-
 * Copyright (c) 2008 Hans Petter Selasky. All rights reserved.
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
 */

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_mbuf.h>

/*------------------------------------------------------------------------*
 *      usb2_alloc_mbufs - allocate mbufs to an usbd interface queue
 *
 * Returns:
 *   A pointer that should be passed to "free()" when the buffer(s)
 *   should be released.
 *------------------------------------------------------------------------*/
void   *
usb2_alloc_mbufs(struct malloc_type *type, struct usb2_ifqueue *ifq,
    uint32_t block_size, uint16_t nblocks)
{
	struct usb2_mbuf *m_ptr;
	uint8_t *data_ptr;
	void *free_ptr = NULL;
	uint32_t alloc_size;

	/* align data */
	block_size += ((-block_size) & (USB_HOST_ALIGN - 1));

	if (nblocks && block_size) {

		alloc_size = (block_size + sizeof(struct usb2_mbuf)) * nblocks;

		free_ptr = malloc(alloc_size, type, M_WAITOK | M_ZERO);

		if (free_ptr == NULL) {
			goto done;
		}
		m_ptr = free_ptr;
		data_ptr = (void *)(m_ptr + nblocks);

		while (nblocks--) {

			m_ptr->cur_data_ptr =
			    m_ptr->min_data_ptr = data_ptr;

			m_ptr->cur_data_len =
			    m_ptr->max_data_len = block_size;

			USB_IF_ENQUEUE(ifq, m_ptr);

			m_ptr++;
			data_ptr += block_size;
		}
	}
done:
	return (free_ptr);
}
