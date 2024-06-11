/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2013-2024, Juniper Networks, Inc.
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

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/bus.h>
#include <sys/sbuf.h>
#include <sys/errno.h>
#include <sys/module.h>

#include <x86/vmware.h>
#include <x86/vmware_guestrpc.h>

/* GuestRPC Subcommands */
#define		VMW_HVGUESTRPC_OPEN			0x00
#define		VMW_HVGUESTRPC_SEND_LEN			0x01
#define		VMW_HVGUESTRPC_SEND_DATA		0x02
#define		VMW_HVGUESTRPC_RECV_LEN			0x03
#define		VMW_HVGUESTRPC_RECV_DATA		0x04
#define		VMW_HVGUESTRPC_FINISH_RECV		0x05
#define		VMW_HVGUESTRPC_CLOSE			0x06
/* GuestRPC Parameters */
#define		VMW_HVGUESTRPC_OPEN_MAGIC		0x49435052
/* GuestRPC Status */
#define		VMW_HVGUESTRPC_FAILURE			0x00000000
#define		VMW_HVGUESTRPC_OPEN_SUCCESS		0x00010000
#define		VMW_HVGUESTRPC_SEND_LEN_SUCCESS		0x00810000
#define		VMW_HVGUESTRPC_SEND_DATA_SUCCESS	0x00010000
#define		VMW_HVGUESTRPC_RECV_LEN_SUCCESS		0x00830000
#define		VMW_HVGUESTRPC_RECV_DATA_SUCCESS	0x00010000
#define		VMW_HVGUESTRPC_FINISH_RECV_SUCCESS	0x00010000
#define		VMW_HVGUESTRPC_CLOSE_SUCCESS		0x00010000

#define	VMW_GUESTRPC_EBX(_p)	((_p)[1])
#define	VMW_GUESTRPC_EDXHI(_p)	((_p)[3] >> 16)
#define	VMW_GUESTRPC_STATUS(_p)	((_p)[2])

static __inline void
vmware_guestrpc(int chan, uint16_t subcmd, uint32_t param, u_int *p)
{

#ifdef DEBUG_VMGUESTRPC
	printf("%s(%d, %#x, %#x, %p)\n", __func__, chan, subcmd, param, p);
#endif
	vmware_hvcall(chan, VMW_HVCMD_GUESTRPC | (subcmd << 16), param, p);
#ifdef DEBUG_VMGUESTRPC
	printf("p[0] = %#x\n", p[0]);
	printf("p[1] = %#x\n", p[1]);
	printf("p[2] = %#x\n", p[2]);
	printf("p[3] = %#x\n", p[3]);
#endif
}

/*
 * Start a GuestRPC request
 *
 * Channel number is returned in the EDXHI parameter.
 *
 * This channel number must be used in successive GuestRPC requests for
 * sending and receiving RPC data.
 */
static int
vmware_guestrpc_open(void)
{
	u_int p[4];

	vmware_guestrpc(0, VMW_HVGUESTRPC_OPEN, VMW_HVGUESTRPC_OPEN_MAGIC,
	    p);
	if (VMW_GUESTRPC_STATUS(p) != VMW_HVGUESTRPC_OPEN_SUCCESS)
		return (-1);

	return (VMW_GUESTRPC_EDXHI(p));
}

/*
 * Send the length of the GuestRPC request
 *
 * In a GuestRPC request, the total length of the request must be sent
 * before any data can be sent.
 */
static int
vmware_guestrpc_send_len(int channel, size_t len)
{
	u_int p[4];

	vmware_guestrpc(channel, VMW_HVGUESTRPC_SEND_LEN, len, p);
	if (VMW_GUESTRPC_STATUS(p) != VMW_HVGUESTRPC_SEND_LEN_SUCCESS)
		return (-1);

	return (0);
}

/*
 * Send the data for the GuestRPC request
 *
 * The total length of the GuestRPC request must be sent before any data.
 * Data is sent 32-bit values at a time and therefore may require multiple
 * calls to send all the data.
 */
static int
vmware_guestrpc_send_data(int channel, uint32_t data)
{
	u_int p[4];

	vmware_guestrpc(channel, VMW_HVGUESTRPC_SEND_DATA, data, p);
	if (VMW_GUESTRPC_STATUS(p) != VMW_HVGUESTRPC_SEND_DATA_SUCCESS)
		return (-1);

	return (0);
}

/*
 * Receive the length of the GuestRPC reply.
 *
 * Length of the reply data is returned in the EBX parameter.
 * The reply identifier is returned in the EDXHI parameter.
 *
 * The reply identifier must be used as the GuestRPC parameter in calls
 * to vmware_guestrpc_recv_data()
 */
static int
vmware_guestrpc_recv_len(int channel, size_t *lenp)
{
	u_int p[4];

	vmware_guestrpc(channel, VMW_HVGUESTRPC_RECV_LEN, 0, p);
	if (VMW_GUESTRPC_STATUS(p) != VMW_HVGUESTRPC_RECV_LEN_SUCCESS)
		return (-1);

	*lenp = VMW_GUESTRPC_EBX(p);
	return (VMW_GUESTRPC_EDXHI(p));
}

/*
 * Receive the GuestRPC reply data.
 *
 * Data is received in 32-bit values at a time and therefore may
 * require multiple requests to get all the data.
 */
static int
vmware_guestrpc_recv_data(int channel, int id, uint32_t *datap)
{
	u_int p[4];

	vmware_guestrpc(channel, VMW_HVGUESTRPC_RECV_DATA, id, p);
	if (VMW_GUESTRPC_STATUS(p) != VMW_HVGUESTRPC_RECV_DATA_SUCCESS)
		return (-1);

	*datap = VMW_GUESTRPC_EBX(p);
	return (0);
}

/*
 * Close the GuestRPC channel.
 */
static int
vmware_guestrpc_close(int channel)
{
	u_int p[4];

	vmware_guestrpc(channel, VMW_HVGUESTRPC_CLOSE, 0, p);
	if (VMW_GUESTRPC_STATUS(p) != VMW_HVGUESTRPC_CLOSE_SUCCESS)
		return (-1);

	return (0);
}

/*
 * Send a GuestRPC command.
 */
int
vmware_guestrpc_cmd(struct sbuf *sbufp)
{
	char *buf;
	size_t cnt, len;
	int chan, id, status;
	uint32_t data;

	/* Make sure we are running under VMware hypervisor */
	if (vm_guest != VM_GUEST_VMWARE)
		return (ENXIO);

	/* Open the GuestRPC channel */
	chan = vmware_guestrpc_open();
	if (chan == -1)
		return (EIO);

	/* Send the length */
	buf = sbuf_data(sbufp);
	len = sbuf_len(sbufp);
	status = vmware_guestrpc_send_len(chan, len);
	if (status == -1)
		goto done;

	/* Send the data */
	while (len > 0) {
		data = 0;
		cnt = min(4, len);
		memcpy(&data, buf, cnt);
		status = vmware_guestrpc_send_data(chan, data);
		if (status == -1)
			goto done;
		buf += cnt;
		len -= cnt;
	}

	/* Receive the length of the reply data */
	id = vmware_guestrpc_recv_len(chan, &len);
	if (id == -1)
		goto done;

	/* Receive the reply data */
	sbuf_clear(sbufp);
	while (len > 0) {
		status = vmware_guestrpc_recv_data(chan, id, &data);
		if (status == -1)
			goto done;
		sbuf_bcat(sbufp, &data, 4);
		len -= min(4, len);
	}

done:
	/* Close the GuestRPC channel */
	vmware_guestrpc_close(chan);
	return (status == -1 ? EIO : 0);
}

/*
 * Set guest information key/value pair
 */
int
vmware_guestrpc_set_guestinfo(const char *keyword, const char *val)
{
	struct sbuf sb;
	char *buf;
	int error;

#ifdef DEBUG_VMGUESTRPC
	printf("%s: %s=%s\n", __func__, keyword, val);
#endif

	/* Send "info-set" GuestRPC command */
	sbuf_new(&sb, NULL, 256, SBUF_AUTOEXTEND);
	sbuf_printf(&sb, "info-set guestinfo.fbsd.%s %s", keyword, val);
	sbuf_trim(&sb);
	sbuf_finish(&sb);

	error = vmware_guestrpc_cmd(&sb);
	if (error)
		return (error);

	sbuf_finish(&sb);
	buf = sbuf_data(&sb);

#ifdef DEBUG_VMGUESTRPC
	printf("%s: result: %s\n", __func__, buf);
#endif

	/* Buffer will contain 1 on sucess or 0 on failure */
	return ((buf[0] == '0') ? EINVAL : 0);
}

/*
 * Get guest information key/value pair.
 */
int
vmware_guestrpc_get_guestinfo(const char *keyword, struct sbuf *sbufp)
{
	struct sbuf sb;
	char *buf;
	int error;

#ifdef DEBUG_VMGUESTRPC
	printf("%s: %s\n", __func__, keyword);
#endif

	/* Send "info-get" GuestRPC command */
	sbuf_new(&sb, NULL, 256, SBUF_AUTOEXTEND);
	sbuf_printf(&sb, "info-get guestinfo.fbsd.%s", keyword);
	sbuf_trim(&sb);
	sbuf_finish(&sb);

	error = vmware_guestrpc_cmd(&sb);
	if (error)
		return (error);

	sbuf_finish(&sb);
	buf = sbuf_data(&sb);

#ifdef DEBUG_VMGUESTRPC
	printf("%s: result: %s\n", __func__, buf);
#endif

	/*
	 * Buffer will contain "1 <value>" on success or
	 * "0 No value found" on failure
	 */
	if (buf[0] == '0')
		return (ENOENT);

	/*
	 * Add value from buffer to the sbuf
	 */
	sbuf_cat(sbufp, buf + 2);
	return (0);
}

MODULE_VERSION(vmware_guestrpc, 1);
