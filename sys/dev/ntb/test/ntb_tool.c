/*-
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright (c) 2019 Advanced Micro Devices, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * BSD LICENSE
 *
 * Copyright (c) 2019 Advanced Micro Devices, Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copy
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Advanced Micro Devices, Inc nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * PCIe NTB Debugging Tool FreeBSD driver
 */

/*
 * How to use this tool, by example.
 *
 * List of sysctl for ntb_tool driver.
 * root@local# sysctl -a | grep ntb_tool
 * dev.ntb_tool.0.peer0.spad7: 0x0
 * dev.ntb_tool.0.peer0.spad6: 0x0
 * dev.ntb_tool.0.peer0.spad5: 0x0
 * dev.ntb_tool.0.peer0.spad4: 0x0
 * dev.ntb_tool.0.peer0.spad3: 0x0
 * dev.ntb_tool.0.peer0.spad2: 0x0
 * dev.ntb_tool.0.peer0.spad1: 0x0
 * dev.ntb_tool.0.peer0.spad0: 0x0
 * dev.ntb_tool.0.peer0.mw_trans2:
 * dev.ntb_tool.0.peer0.mw_trans1:
 * dev.ntb_tool.0.peer0.mw_trans0:
 * dev.ntb_tool.0.peer0.peer_mw2:
 * dev.ntb_tool.0.peer0.peer_mw1:
 * dev.ntb_tool.0.peer0.peer_mw0:
 * dev.ntb_tool.0.peer0.mw2:
 * dev.ntb_tool.0.peer0.mw1:
 * dev.ntb_tool.0.peer0.mw0:
 * dev.ntb_tool.0.peer0.link_event: 0x0
 * dev.ntb_tool.0.peer0.link: Y
 * dev.ntb_tool.0.peer0.port: 1
 * dev.ntb_tool.0.spad7: 0x0
 * dev.ntb_tool.0.spad6: 0x0
 * dev.ntb_tool.0.spad5: 0x0
 * dev.ntb_tool.0.spad4: 0x0
 * dev.ntb_tool.0.spad3: 0x0
 * dev.ntb_tool.0.spad2: 0x0
 * dev.ntb_tool.0.spad1: 0x0
 * dev.ntb_tool.0.spad0: 0x0
 * dev.ntb_tool.0.db: 0x0
 * dev.ntb_tool.0.db_event: 0x0
 * dev.ntb_tool.0.db_mask: 0xffff
 * dev.ntb_tool.0.db_valid_mask: 0xffff
 * dev.ntb_tool.0.peer_db: 0x0
 * dev.ntb_tool.0.peer_db_mask: 0xffff
 * dev.ntb_tool.0.link: Y
 * dev.ntb_tool.0.port: 0
 *
 * The above example list shows
 * 1) three memory windows,
 * 1) eight scratchpad registers.
 * 3) doorbell config.
 * 4) link config.
 * 2) One peer.
 *
 * Based on the underlined ntb_hw driver config & connection topology, these
 * things might differ.
 *-----------------------------------------------------------------------------
 * Eg: check local/peer port information.
 *
 * # Get local device port number
 * root@local# sysctl dev.ntb_tool.0.port
 *
 * # Check peer device port number
 * root@local# sysctl dev.ntb_tool.0.peer0.port
 *-----------------------------------------------------------------------------
 * Eg: NTB link tests
 *
 * # Set local link up/down
 * root@local# sysctl dev.ntb_tool.0.link=Y
 * root@local# sysctl dev.ntb_tool.0.link=N
 *
 * # Check if link with peer device is up/down:
 * root@local# sysctl dev.ntb_tool.0.peer0.link
 *
 * # Poll until the link specified as up/down. For up, value needs to be set
 * depends on peer index, i.e., for peer0 it is 0x1 and for down, value needs
 * to be set as 0x0.
 * root@local# sysctl dev.ntb_tool.0.peer0.link_event=0x1
 * root@local# sysctl dev.ntb_tool.0.peer0.link_event=0x0
 *-----------------------------------------------------------------------------
 * Eg: Doorbell registers tests
 *
 * # clear/get local doorbell
 * root@local# sysctl dev.ntb_tool.0.db="c 0x1"
 * root@local# sysctl dev.ntb_tool.0.db
 *
 * # Set/clear/get local doorbell mask
 * root@local# sysctl dev.ntb_tool.0.db_mask="s 0x1"
 * root@local# sysctl dev.ntb_tool.0.db_mask="c 0x1"
 * root@local# sysctl dev.ntb_tool.0.db_mask
 *
 * # Ring/clear/get peer doorbell
 * root@local# sysctl dev.ntb_tool.0.peer_db="s 0x1"
 * root@local# sysctl dev.ntb_tool.0.peer_db="c 0x1"
 * root@local# sysctl dev.ntb_tool.0.peer_db
 *
 * # Set/clear/get peer doorbell mask (functionality is absent)
 * root@local# sysctl dev.ntb_tool.0.peer_db_mask="s 0x1"
 * root@local# sysctl dev.ntb_tool.0.peer_db_mask="c 0x1"
 * root@local# sysctl dev.ntb_tool.0.peer_db_mask
 *
 * # Poll until local doorbell is set with the specified db bits
 * root@local# dev.ntb_tool.0.db_event=0x1
 *-----------------------------------------------------------------------------
 * Eg: Scratchpad registers tests
 *
 * # Write/read to/from local scratchpad register #0
 * root@local# sysctl dev.ntb_tool.0.spad0=0x1023457
 * root@local# sysctl dev.ntb_tool.0.spad0
 *
 * # Write/read to/from peer scratchpad register #0
 * root@local# sysctl dev.ntb_tool.0.peer0.spad0=0x01020304
 * root@local# sysctl dev.ntb_tool.0.peer0.spad0
 *-----------------------------------------------------------------------------
 * Eg: Memory windows tests (need to configure local mw_trans on both sides)
 *
 * # Create inbound memory window buffer of specified size/get its dma address
 * root@local# sysctl dev.ntb_tool.0.peer0.mw_trans0=16384
 * root@local# sysctl dev.ntb_tool.0.peer0.mw_trans0
 *
 * # Write/read data to/from inbound memory window with specific pattern/random
 * data.
 * root@local# sysctl dev.ntb_tool.0.peer0.mw0="W offset 0 nbytes 100 pattern ab"
 * root@local# sysctl dev.ntb_tool.0.peer0.mw0="R offset 0 nbytes 100"
 *
 * # Write/read data to/from outbound memory window on the local device with
 * specific pattern/random (on peer device)
 * root@local# sysctl dev.ntb_tool.0.peer0.peer_mw0="W offset 0 nbytes 100 pattern ab"
 * root@local# sysctl dev.ntb_tool.0.peer0.peer_mw0="R offset 0 nbytes 100"
 *-----------------------------------------------------------------------------
 * NOTE: *Message registers are not supported*
 *-----------------------------------------------------------------------------
 *
 * contact information:
 * Arpan Palit <arpan.palit@amd.com>
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mbuf.h>
#include <sys/sysctl.h>
#include <sys/sbuf.h>

#include <machine/bus.h>

#include <vm/vm.h>

#include "../ntb.h"

/* Buffer length for User input */
#define	TOOL_BUF_LEN 48
/* Memory window default command read and write offset. */
#define	DEFAULT_MW_OFF  0
/* Memory window default size and also max command read size. */
#define	DEFAULT_MW_SIZE 1024

MALLOC_DEFINE(M_NTB_TOOL, "ntb_tool", "ntb_tool driver memory allocation");

/*
 * Memory windows descriptor structure
 */
struct tool_mw {
	struct tool_ctx    *tc;
	int                widx;
	int                pidx;

	/* Rx buff is off virt_addr / dma_base */
	bus_addr_t         dma_base;
	caddr_t            virt_addr;
	bus_dmamap_t       dma_map;
	bus_dma_tag_t      dma_tag;

	/* Tx buff is off vbase / phys_addr */
	caddr_t            mm_base;
	vm_paddr_t         phys_addr;
	bus_addr_t         addr_limit;
	size_t             phys_size;
	size_t             xlat_align;
	size_t             xlat_align_size;

	/* Memory window configured size and limits */
	size_t             size;
	ssize_t            mw_buf_size;
	ssize_t            mw_buf_offset;
	ssize_t            mw_peer_buf_size;
	ssize_t            mw_peer_buf_offset;

	/* options to handle sysctl out */
	int                mw_cmd_rw;
	int                mw_peer_cmd_rw;
};

struct tool_spad {
	int                sidx;
	int                pidx;
	struct tool_ctx    *tc;
};

struct tool_peer {
	int                 pidx;
	struct tool_ctx     *tc;
	int                 inmw_cnt;
	struct tool_mw      *inmws;
	int                 outspad_cnt;
	struct tool_spad    *outspads;
	unsigned int        port_no;
};

struct tool_ctx {
	device_t            dev;
	struct callout      link_event_timer;
	struct callout      db_event_timer;
	int                 peer_cnt;
	struct tool_peer    *peers;
	int                 inmsg_cnt;
	struct tool_msg     *inmsgs;
	int                 inspad_cnt;
	struct tool_spad    *inspads;
	unsigned int        unsafe;

	/* sysctl read out variables */
	char                link_status;
	uint64_t            link_bits;
	uint64_t            link_mask;
	uint64_t            db_valid_mask;
	uint64_t            db_mask_val;
	uint64_t            db_event_val;
	uint64_t            peer_db_val;
	uint64_t            peer_db_mask_val;
	unsigned int        port_no;
};

/* structure to save dma_addr after dma load */
struct ntb_tool_load_cb_args {
	bus_addr_t addr;
	int error;
};

/*
 * NTB events handlers
 */
static void
tool_link_event(void *ctx)
{
	struct tool_ctx *tc = ctx;
	enum ntb_speed speed = 0;
	enum ntb_width width = 0;
	int up = 0;

	up = ntb_link_is_up(tc->dev, &speed, &width);
	if (up)
		tc->link_status = 'Y';
	else
		tc->link_status = 'N';

	device_printf(tc->dev, "link is %s speed %d width %d\n",
	    up ? "up" : "down", speed, width);
}

static void
tool_db_event(void *ctx, uint32_t vec)
{
	struct tool_ctx *tc = ctx;
	uint64_t db_bits, db_mask;

	db_mask = ntb_db_vector_mask(tc->dev, vec);
	db_bits = ntb_db_read(tc->dev);

	device_printf(tc->dev, "doorbell vec %d mask %#llx bits %#llx\n",
	    vec, (unsigned long long)db_mask, (unsigned long long)db_bits);
}

static const struct ntb_ctx_ops tool_ops = {
	.link_event = tool_link_event,
	.db_event = tool_db_event,
};

/*
 * Callout event methods
 */
static void
tool_link_event_handler(void *arg)
{
	struct tool_ctx *tc = (struct tool_ctx *)arg;
	uint64_t val;

	val = ntb_link_is_up(tc->dev, NULL, NULL) & tc->link_mask;

	if (val == tc->link_bits) {
		device_printf(tc->dev, "link_event successful for link val="
		    "0x%jx\n", tc->link_bits);
		tc->link_bits = 0x0;
		tc->link_mask = 0x0;
	} else
		callout_reset(&tc->link_event_timer, 1, tool_link_event_handler, tc);
}

static void
tool_db_event_handler(void *arg)
{
	struct tool_ctx *tc = (struct tool_ctx *)arg;
	uint64_t db_bits;

	db_bits = ntb_db_read(tc->dev);

	if (db_bits == tc->db_event_val) {
		device_printf(tc->dev, "db_event successful for db val=0x%jx\n",
		    tc->db_event_val);
		tc->db_event_val = 0x0;
	} else
		callout_reset(&tc->db_event_timer, 1, tool_db_event_handler, tc);
}

/*
 * Common read/write methods
 */
static inline int
get_ubuf(struct sysctl_req *req, char *ubuf)
{
	int rc;

	if (req->newlen >= TOOL_BUF_LEN)
		return (EINVAL);

	rc = SYSCTL_IN(req, ubuf, req->newlen);
	if (rc)
		return (rc);
	ubuf[req->newlen] = '\0';

	return (0);
}

static int
read_out(struct sysctl_req *req, uint64_t val)
{
	char ubuf[16];

	memset((void *)ubuf, 0, sizeof(ubuf));
	snprintf(ubuf, sizeof(ubuf), "0x%jx", val);

	return SYSCTL_OUT(req, ubuf, sizeof(ubuf));
}

static int
tool_fn_read(struct tool_ctx *tc, struct sysctl_req *req,
    uint64_t (*fn_read)(device_t ), uint64_t val)
{
	if (fn_read == NULL)
		return read_out(req, val);
	else if (fn_read)
		return read_out(req, (uint64_t)fn_read(tc->dev));
	else
		return (EINVAL);
}

static int
tool_fn_write(struct tool_ctx *tc, struct sysctl_oid *oidp,
    struct sysctl_req *req, char *ubuf, uint64_t *val, bool db_mask_sflag,
    void (*fn_set)(device_t , uint64_t), void (*fn_clear)(device_t , uint64_t))
{
	uint64_t db_valid_mask = tc->db_valid_mask;
	uint64_t bits;
	char cmd;

	if (fn_set == NULL && fn_clear == NULL) {
		device_printf(tc->dev, "ERR: Set & Clear both are not supported\n");
		return (EINVAL);
	}

	if (tc->db_valid_mask == 0)
		db_valid_mask = tc->db_valid_mask = ntb_db_valid_mask(tc->dev);

	bits = 0;
	sscanf(ubuf, "%c %jx", &cmd, &bits);
	if (cmd == 's') {
		if ((bits | db_valid_mask) > db_valid_mask) {
			device_printf(tc->dev, "0x%jx value is not supported\n", bits);
			return (EINVAL);
		}
		if (fn_set)
			fn_set(tc->dev, bits);
		else
			return (EINVAL);
		if (val)
			*val |= bits;
	} else if (cmd == 'c') {
		if ((bits | db_valid_mask) > db_valid_mask) {
			device_printf(tc->dev, "0x%jx value is not supported\n", bits);
			return (EINVAL);
		}
		if (fn_clear)
			fn_clear(tc->dev, bits);
		if (val)
			*val &= ~bits;
	} else {
		device_printf(tc->dev, "Wrong Write\n");
		return (EINVAL);
	}

	return (0);
}

static int
parse_mw_buf(char *buf, char *cmd, ssize_t *offset, ssize_t *buf_size,
    uint64_t *pattern, bool *s_pflag)
{
	char op1[8], op2[8], op3[8];
	uint64_t val1, val2, val3;
	bool vs1, vs2, vs3;
	int rc = 0;

	vs1 = vs2 = vs3 = false;
	sscanf(buf, "%c %s %jx %s %jx %s %jx",
	    cmd, op1, &val1, op2, &val2, op3, &val3);

	if (*cmd != 'W' && *cmd != 'R')
		return (EINVAL);

	if (!strcmp(op1, "offset")) {
		*offset = val1 ? val1 : DEFAULT_MW_OFF;
		vs1 = true;
	} else if (!strcmp(op1, "nbytes")) {
		*buf_size = val1 ? val1: DEFAULT_MW_SIZE;
		vs2 = true;
	} else if (!strcmp(op1, "pattern")) {
		*pattern = val1;
		vs3 = true;
	}

	if (!vs1 && !strcmp(op2, "offset")) {
		*offset = val2 ? val2 : DEFAULT_MW_OFF;
		vs1 = true;
	} else if (!vs2 && !strcmp(op2, "nbytes")) {
		*buf_size = val2 ? val2: DEFAULT_MW_SIZE;
		vs2 = true;
	} else if (!vs3 && !strcmp(op2, "pattern")) {
		*pattern = val2;
		vs3 = true;
	}

	if (!vs1 && !strcmp(op3, "offset")) {
		*offset = val3 ? val3 : DEFAULT_MW_OFF;
	} else if (!vs2 && !strcmp(op3, "nbytes")) {
		*buf_size = val3 ? val3: DEFAULT_MW_SIZE;
	} else if (!vs3 && !strcmp(op3, "pattern")) {
		*pattern = val3;
		vs3 = true;
	}

	*s_pflag = vs3;
	if (vs3 && *cmd == 'R')
		printf("NTB_TOOL_WARN: pattern is not supported with read "
		    "command\n");

	return (rc);
}

static int
tool_mw_read_fn(struct sysctl_req *req, struct tool_mw *inmw, char *read_addr,
    int *cmd_op, ssize_t buf_off, ssize_t buf_size, char *type)
{
	ssize_t index, size;
	struct sbuf *sb;
	int i, loop, rc;
	char *tmp;

	/* The below check is made to ignore sysctl read call. */
	if (*cmd_op == 0)
		return (0);

	/* Proceeds only when command R/W is requested using sysctl. */
	index = buf_off;
	tmp = read_addr;
	tmp += index;
	loop = ((buf_size == 0) || (buf_size > DEFAULT_MW_SIZE)) ?
	    DEFAULT_MW_SIZE : buf_size;
	/*
	 * 256 bytes of extra buffer has been allocated to print details like
	 * summary, size, notes, i.e., excluding data part.
	 */
	size = loop + 256;
	sb = sbuf_new_for_sysctl(NULL, NULL, size, req);
	if (sb == NULL) {
		rc = sb->s_error;
		return (rc);
	}

	if (!strcmp(type, "mw"))
		sbuf_printf(sb, "\nConfigured MW size\t: %zu\n", inmw->size);
	else if (!strcmp(type, "peer_mw"))
		sbuf_printf(sb, "\nConfigured Peer MW size\t: %zu\n",
		    inmw->size);
	sbuf_printf(sb, "R/W size\t\t: %zi\nR/W Offset\t\t: %zi\n\nData\n----"
	    "->", buf_size, buf_off);

	/*
	 * Data will be read based on MW size provided by the user using nbytes,
	 * which is limited to 1024 bytes if user req bigger size to read, check
	 * above loop calculation which is limiting or setting the MW read size.
	 * Below for loop prints data where in each line contains 32 bytes data
	 * and after each 8 bytes of data we used four spaces which ensures one
	 * data block.
	 */
	for (i = 0 ; i < loop; i++) {
		if ((i % 32) == 0) {
			sbuf_printf(sb, "\n%08zx:", index);
			index += 32;
		}
		if ((i % 8) == 0)
			sbuf_printf(sb, "    ");
		sbuf_printf(sb, "%02hhx", *(tmp+i));
	}
	if (buf_size > DEFAULT_MW_SIZE)
		sbuf_printf(sb, "\n\nNOTE: Truncating read size %zi->1024 "
		    "bytes\n", buf_size);

	/* cmd_op is set to zero after completion of each R/W command. */
	*cmd_op -= 1;
	rc = sbuf_finish(sb);
	sbuf_delete(sb);

	return (rc);
}

static int
tool_mw_write_fn(struct sysctl_oid *oidp, struct sysctl_req *req,
    struct tool_mw *inmw, char *ubuf, caddr_t write_buf, int *cmd_op,
    ssize_t *buf_offset, ssize_t *buf_size)
{
	ssize_t data_buf_size;
	uint64_t pattern = 0;
	bool s_pflag = false;
	void *data_buf;
	char cmd;
	int rc;

	if (!write_buf)
		return (ENXIO);

	/* buf_offset and buf_size set to default in case user does not req */
	*buf_offset = DEFAULT_MW_OFF;
	*buf_size = DEFAULT_MW_SIZE;
	rc = parse_mw_buf(ubuf, &cmd, buf_offset, buf_size, &pattern, &s_pflag);
	if (rc) {
		device_printf(inmw->tc->dev, "Wrong Command \"%c\" provided\n",
		    cmd);
		return (rc);
	}

	/* Check for req size and buffer limit */
	if ((*buf_offset + *buf_size) > inmw->size) {
		device_printf(inmw->tc->dev, "%s: configured mw size :%zi and "
		    "requested size :%zi.\n", __func__, inmw->size,
		    (*buf_offset + *buf_size));
		*buf_offset = DEFAULT_MW_OFF;
		*buf_size = DEFAULT_MW_SIZE;
		rc = EINVAL;
		goto out;
	}

	if (cmd == 'R')
		goto read_out;
	else if (cmd == 'W')
		goto write;
	else
		goto out;

write:
	data_buf_size = *buf_size;
	data_buf = malloc(data_buf_size, M_NTB_TOOL, M_WAITOK | M_ZERO);

	if (s_pflag)
		memset(data_buf, pattern, data_buf_size);
	else
		arc4rand(data_buf, data_buf_size, 1);

	memcpy(write_buf + *buf_offset, data_buf, data_buf_size);

	free(data_buf, M_NTB_TOOL);

read_out:
	/* cmd_op value is set to two as sysctl read call executes twice */
	*cmd_op = 2;
out:
	return (rc);
}

/*
 * Port sysctl read/write methods
 */
static int
sysctl_peer_port_number(SYSCTL_HANDLER_ARGS)
{
	struct tool_ctx *tc = (struct tool_ctx *)arg1;
	int rc, pidx = arg2, peer_port;

	peer_port = ntb_peer_port_number(tc->dev, pidx);
	rc = sysctl_handle_int(oidp, &peer_port, 0, req);
	if (rc)
		device_printf(tc->dev, "Peer port sysctl set failed with err="
		    "(%d).\n", rc);
	else
		tc->peers[pidx].port_no = peer_port;

	return (rc);
}

static int
sysctl_local_port_number(SYSCTL_HANDLER_ARGS)
{
	struct tool_ctx *tc = (struct tool_ctx *)arg1;
	int rc, local_port;

	local_port = ntb_port_number(tc->dev);
	rc = sysctl_handle_int(oidp, &local_port, 0, req);
	if (rc)
		device_printf(tc->dev, "Local port sysctl set failed with err="
		    "(%d).\n", rc);
	else
		tc->port_no = local_port;

	return (rc);
}

static void
tool_init_peers(struct tool_ctx *tc)
{
	int pidx;

	tc->peer_cnt = ntb_peer_port_count(tc->dev);
	tc->peers = malloc(tc->peer_cnt * sizeof(*tc->peers), M_NTB_TOOL,
	    M_WAITOK | M_ZERO);
	for (pidx = 0; pidx < tc->peer_cnt; pidx++) {
		tc->peers[pidx].pidx = pidx;
		tc->peers[pidx].tc = tc;
	}
}

static void
tool_clear_peers(struct tool_ctx *tc)
{

	free(tc->peers, M_NTB_TOOL);
}

/*
 * Link state sysctl read/write methods
 */
static int
sysctl_link_handle(SYSCTL_HANDLER_ARGS)
{
	struct tool_ctx *tc = (struct tool_ctx *)arg1;
	char buf[TOOL_BUF_LEN];
	int rc;

	if (req->newptr == NULL) {
		snprintf(buf, 2, "%c", tc->link_status);

		return SYSCTL_OUT(req, buf, 2);
	}

	rc = get_ubuf(req, buf);
	if (rc)
		return (rc);

	if (buf[0] == 'Y')
		rc = ntb_link_enable(tc->dev, NTB_SPEED_AUTO, NTB_WIDTH_AUTO);
	else if (buf[0] == 'N')
		rc = ntb_link_disable(tc->dev);
	else
		rc = EINVAL;

	sscanf(buf, "%c", &tc->link_status);

	return (0);
}

static int
sysctl_peer_link_handle(SYSCTL_HANDLER_ARGS)
{
	struct tool_ctx *tc = (struct tool_ctx *)arg1;
	int up = 0, pidx = arg2;
	char buf[TOOL_BUF_LEN];

	if (req->newptr)
		return (0);

	up = ntb_link_is_up(tc->dev, NULL, NULL);
	memset((void *)buf, 0, TOOL_BUF_LEN);
	if (up & (1UL << pidx))
		buf[0] = 'Y';
	else
		buf[0] = 'N';

	return SYSCTL_OUT(req, buf, sizeof(buf));
}

static int
sysctl_peer_link_event_handle(SYSCTL_HANDLER_ARGS)
{
	struct tool_ctx *tc = (struct tool_ctx *)arg1;
	char buf[TOOL_BUF_LEN];
	int rc, pidx = arg2;
	uint64_t bits;

	if (req->newptr == NULL)
		return read_out(req, tc->link_bits);

	rc = get_ubuf(req, buf);
	if (rc)
		return (rc);

	sscanf(buf, "0x%jx", &bits);
	tc->link_bits = bits;
	tc->link_mask = (1ULL << ((pidx) % 64));

	callout_reset(&tc->link_event_timer, 1, tool_link_event_handler, tc);
	return (0);
}

/*
 * Memory windows read/write/setting methods
 */
static void
ntb_tool_load_cb(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
	struct ntb_tool_load_cb_args *cba = (struct ntb_tool_load_cb_args *)arg;

	if (!(cba->error = error))
		cba->addr = segs[0].ds_addr;
}

static int
sysctl_mw_handle(SYSCTL_HANDLER_ARGS)
{
	struct tool_mw *inmw = (struct tool_mw *)arg1;
	char buf[TOOL_BUF_LEN];
	int rc;

	if (req->newptr == NULL)
		return tool_mw_read_fn(req, inmw, (char *)inmw->mm_base,
		    &inmw->mw_cmd_rw, inmw->mw_buf_offset, inmw->mw_buf_size,
		    "mw");

	rc = get_ubuf(req, buf);
	if (!rc)
		return tool_mw_write_fn(oidp, req, inmw, buf, inmw->mm_base,
		    &inmw->mw_cmd_rw, &inmw->mw_buf_offset, &inmw->mw_buf_size);

	return (rc);
}

static int
tool_setup_mw(struct tool_ctx *tc, unsigned int pidx, unsigned int widx,
    size_t req_size)
{
	struct tool_mw *inmw = &tc->peers[pidx].inmws[widx];
	struct ntb_tool_load_cb_args cba;
	int rc;

	if (req_size == 0)
		inmw->size = roundup(inmw->phys_size, inmw->xlat_align_size);
	else
		inmw->size = roundup(req_size, inmw->xlat_align_size);

	device_printf(tc->dev, "mw_size %zi req_size %zi buff %zi\n",
	    inmw->phys_size, req_size, inmw->size);

	if (bus_dma_tag_create(bus_get_dma_tag(tc->dev), inmw->xlat_align, 0,
	    inmw->addr_limit, BUS_SPACE_MAXADDR, NULL, NULL, inmw->size, 1,
	    inmw->size, 0, NULL, NULL, &inmw->dma_tag)) {
		device_printf(tc->dev, "Unable to create MW tag of size "
		    "%zu/%zu\n", inmw->phys_size, inmw->size);
		rc = ENOMEM;
		goto err_free_dma_var;
	}

	if (bus_dmamem_alloc(inmw->dma_tag, (void **)&inmw->virt_addr,
	    BUS_DMA_WAITOK | BUS_DMA_ZERO, &inmw->dma_map)) {
		device_printf(tc->dev, "Unable to allocate MW buffer of size "
		    "%zu/%zu\n", inmw->phys_size, inmw->size);
		rc = ENOMEM;
		goto err_free_tag_rem;
	}

	if (bus_dmamap_load(inmw->dma_tag, inmw->dma_map, inmw->virt_addr,
	    inmw->size, ntb_tool_load_cb, &cba, BUS_DMA_NOWAIT) || cba.error) {
		device_printf(tc->dev, "Unable to load MW buffer of size "
		    "%zu/%zu\n", inmw->phys_size, inmw->size);
		rc = ENOMEM;
		goto err_free_dma;
	}
	inmw->dma_base = cba.addr;

	rc = ntb_mw_set_trans(tc->dev, widx, inmw->dma_base, inmw->size);
	if (rc)
		goto err_free_mw;

	return (0);

err_free_mw:
	bus_dmamap_unload(inmw->dma_tag, inmw->dma_map);

err_free_dma:
	bus_dmamem_free(inmw->dma_tag, inmw->virt_addr, inmw->dma_map);

err_free_tag_rem:
	bus_dma_tag_destroy(inmw->dma_tag);

err_free_dma_var:
	inmw->size = 0;
	inmw->virt_addr = 0;
	inmw->dma_base = 0;
	inmw->dma_tag = 0;
	inmw->dma_map = 0;

	return (rc);
}

static void
tool_free_mw(struct tool_ctx *tc, int pidx, int widx)
{
	struct tool_mw *inmw = &tc->peers[pidx].inmws[widx];

	if (inmw->dma_base)
		ntb_mw_clear_trans(tc->dev, widx);

	if (inmw->virt_addr && inmw->dma_tag) {
		bus_dmamap_unload(inmw->dma_tag, inmw->dma_map);
		bus_dmamem_free(inmw->dma_tag, inmw->virt_addr, inmw->dma_map);
		bus_dma_tag_destroy(inmw->dma_tag);
	}

	inmw->virt_addr = 0;
	inmw->dma_base = 0;
	inmw->dma_tag = 0;
	inmw->dma_map = 0;
	inmw->mm_base = 0;
	inmw->size = 0;
}

static int
tool_mw_trans_read(struct tool_mw *inmw, struct sysctl_req *req)
{
	ssize_t buf_size = 512;
	struct sbuf *sb;
	int rc = 0;

	sb = sbuf_new_for_sysctl(NULL, NULL, buf_size, req);
	if (sb == NULL) {
		rc = sb->s_error;
		return (rc);
	}

	sbuf_printf(sb, "\nInbound MW     \t%d\n", inmw->widx);
	sbuf_printf(sb, "Port           \t%d (%d)\n",
	    ntb_peer_port_number(inmw->tc->dev, inmw->pidx), inmw->pidx);
	sbuf_printf(sb, "Window Address \t%p\n", inmw->mm_base);
	sbuf_printf(sb, "DMA Address    \t0x%016llx\n", (long long)inmw->dma_base);
	sbuf_printf(sb, "Window Size    \t0x%016zx[p]\n", inmw->size);
	sbuf_printf(sb, "Alignment      \t0x%016zx[p]\n", inmw->xlat_align);
	sbuf_printf(sb, "Size Alignment \t0x%016zx[p]\n",
	    inmw->xlat_align_size);
	sbuf_printf(sb, "Size Max       \t0x%016zx[p]\n", inmw->phys_size);

	rc = sbuf_finish(sb);
	sbuf_delete(sb);

	return (rc);
}

static int
tool_mw_trans_write(struct sysctl_oid *oidp, struct sysctl_req *req,
    struct tool_mw *inmw, size_t wsize)
{
	struct tool_ctx *tc = inmw->tc;
	int rc = 0;

	if (wsize == 0)
		return (EINVAL);

	/* No need to re-setup mw */
	if (inmw->size == wsize)
		return (0);

	/* free mw dma buffer */
	if (inmw->size)
		tool_free_mw(tc, inmw->pidx, inmw->widx);

	rc = tool_setup_mw(tc, inmw->pidx, inmw->widx, wsize);

	return (rc);
}

static int
sysctl_mw_trans_handler(SYSCTL_HANDLER_ARGS)
{
	struct tool_mw *inmw = (struct tool_mw *)arg1;
	char buf[TOOL_BUF_LEN];
	ssize_t wsize;
	int rc;

	if (req->newptr == NULL)
		return tool_mw_trans_read(inmw, req);

	rc = get_ubuf(req, buf);
	if (rc == 0) {
		sscanf(buf, "%zi", &wsize);
		return tool_mw_trans_write(oidp, req, inmw, wsize);
	}

	return (rc);
}

static int
sysctl_peer_mw_handle(SYSCTL_HANDLER_ARGS)
{
	struct tool_mw *inmw = (struct tool_mw *)arg1;
	char buf[TOOL_BUF_LEN];
	int rc;

	if (req->newptr == NULL)
		return tool_mw_read_fn(req, inmw, (char *)inmw->virt_addr,
		    &inmw->mw_peer_cmd_rw, inmw->mw_peer_buf_offset,
		    inmw->mw_peer_buf_size, "mw");

	rc = get_ubuf(req, buf);
	if (rc == 0)
		return tool_mw_write_fn(oidp, req, inmw, buf, inmw->virt_addr,
		    &inmw->mw_peer_cmd_rw, &inmw->mw_peer_buf_offset,
		    &inmw->mw_peer_buf_size);

	return (rc);
}

static void tool_clear_mws(struct tool_ctx *tc)
{
	int widx, pidx;

	/* Free outbound memory windows */
	for (pidx = 0; pidx < tc->peer_cnt; pidx++) {
		for (widx = 0; widx < tc->peers[pidx].inmw_cnt; widx++)
			tool_free_mw(tc, pidx, widx);
		free(tc->peers[pidx].inmws, M_NTB_TOOL);
	}
}

static int
tool_init_mws(struct tool_ctx *tc)
{
	struct tool_mw *mw;
	int widx, pidx, rc;

	/* Initialize inbound memory windows and outbound MWs wrapper */
	for (pidx = 0; pidx < tc->peer_cnt; pidx++) {
		tc->peers[pidx].inmw_cnt = ntb_mw_count(tc->dev);
		tc->peers[pidx].inmws = malloc(tc->peers[pidx].inmw_cnt *
		    sizeof(*tc->peers[pidx].inmws), M_NTB_TOOL,
		    M_WAITOK | M_ZERO);

		for (widx = 0; widx < tc->peers[pidx].inmw_cnt; widx++) {
			mw = &tc->peers[pidx].inmws[widx];
			memset((void *)mw, 0, sizeof(*mw));
			mw->tc = tc;
			mw->widx = widx;
			mw->pidx = pidx;
			mw->mw_buf_offset = DEFAULT_MW_OFF;
			mw->mw_buf_size = DEFAULT_MW_SIZE;
			/* get the tx buff details for each mw attached with each peer */
			rc = ntb_mw_get_range(tc->dev, widx, &mw->phys_addr,
			    &mw->mm_base, &mw->phys_size, &mw->xlat_align,
			    &mw->xlat_align_size, &mw->addr_limit);
			if (rc)
				goto free_mws;
		}
	}

	return (0);

free_mws:
	tool_clear_mws(tc);
	return (rc);
}

/*
 * Doorbell handler for read/write
 */
static int
sysctl_db_handle(SYSCTL_HANDLER_ARGS)
{
	struct tool_ctx *tc = (struct tool_ctx *)arg1;
	char buf[TOOL_BUF_LEN];
	uint64_t db_bits;
	int rc;

	if (req->newptr == NULL) {
		db_bits = ntb_db_read(tc->dev);
		return read_out(req, db_bits);
	}

	rc = get_ubuf(req, buf);
	if (rc == 0)
		return tool_fn_write(tc, oidp, req, buf, NULL, false, NULL,
		    ntb_db_clear);

	return (rc);
}

static int
sysctl_db_valid_mask_handle(SYSCTL_HANDLER_ARGS)
{
	struct tool_ctx *tc = (struct tool_ctx *)arg1;

	tc->db_valid_mask = ntb_db_valid_mask(tc->dev);
	if (!tc->db_valid_mask) {
		device_printf(tc->dev, "Error getting db_valid_mask from "
		    "hw driver\n");
		return (EINVAL);
	} else {
		return read_out(req, tc->db_valid_mask);
	}
}

static int
sysctl_db_mask_handle(SYSCTL_HANDLER_ARGS)
{
	struct tool_ctx *tc = (struct tool_ctx *)arg1;
	char buf[TOOL_BUF_LEN];
	int rc;

	if (req->newptr == NULL) {
		if (tc->db_mask_val == 0)
		     ntb_db_valid_mask(tc->dev);
		return tool_fn_read(tc, req, NULL, tc->db_mask_val);
	}

	rc = get_ubuf(req, buf);
	if (rc == 0)
		return tool_fn_write(tc, oidp, req, buf, &tc->db_mask_val, true,
		    ntb_db_set_mask, ntb_db_clear_mask);

	return (rc);
}

static int
sysctl_peer_db_handle(SYSCTL_HANDLER_ARGS)
{
	struct tool_ctx *tc = (struct tool_ctx *)arg1;
	char buf[TOOL_BUF_LEN];
	int rc;

	if (req->newptr == NULL)
		return tool_fn_read(tc, req, NULL, tc->peer_db_val);

	rc = get_ubuf(req, buf);
	if (rc == 0)
		return tool_fn_write(tc, oidp, req, buf, &tc->peer_db_val,
		    false, ntb_peer_db_set, NULL);

	return (rc);
}

static int
sysctl_peer_db_mask_handle(SYSCTL_HANDLER_ARGS)
{
	struct tool_ctx *tc = (struct tool_ctx *)arg1;
	char buf[TOOL_BUF_LEN];
	int rc;

	if (req->newptr == NULL){
		if (tc->peer_db_mask_val == 0)
			ntb_db_valid_mask(tc->dev);
		return tool_fn_read(tc, req, NULL, tc->peer_db_mask_val);
	}

	rc = get_ubuf(req, buf);
	if (rc == 0)
		return tool_fn_write(tc, oidp, req, buf, &tc->peer_db_mask_val,
		    true, NULL, NULL);

	return (rc);
}

static int
sysctl_db_event_handle(SYSCTL_HANDLER_ARGS)
{
	struct tool_ctx *tc = (struct tool_ctx *)arg1;
	char buf[TOOL_BUF_LEN];
	uint64_t bits;
	int rc;

	if (req->newptr == NULL)
		return read_out(req, tc->db_event_val);

	rc = get_ubuf(req, buf);
	if (rc)
		return (rc);

	sscanf(buf, "%ju", &bits);
	tc->db_event_val = bits;
	callout_reset(&tc->db_event_timer, 1, tool_db_event_handler, tc);

	return (0);
}

/*
 * Scratchpads read/write methods
 */
static int
sysctl_spad_handle(SYSCTL_HANDLER_ARGS)
{
	struct tool_ctx *tc = (struct tool_ctx *)arg1;
	unsigned int sidx = arg2;
	char buf[TOOL_BUF_LEN];
	uint32_t bits;
	int rc;

	if (req->newptr == NULL) {
		rc = ntb_spad_read(tc->dev, sidx, &bits);
		if (rc)
			return (rc);
		else
			return read_out(req, (uint64_t )bits);
	}

	rc = get_ubuf(req, buf);
	if (rc == 0) {
		sscanf(buf, "%i", &bits);
		return ntb_spad_write(tc->dev, sidx, bits);
	}

	return (rc);
}

static int
sysctl_peer_spad_handle(SYSCTL_HANDLER_ARGS)
{
	struct tool_ctx *tc = (struct tool_ctx *)arg1;
	unsigned int sidx = arg2;
	char buf[TOOL_BUF_LEN];
	uint32_t bits;
	int rc;

	if (req->newptr == NULL) {
		rc = ntb_peer_spad_read(tc->dev, sidx, &bits);
		if (rc)
			return (rc);
		else
			return read_out(req, (uint64_t )bits);
	}

	rc = get_ubuf(req, buf);
	if (rc == 0) {
		sscanf(buf, "%i", &bits);
		return ntb_peer_spad_write(tc->dev, sidx, bits);
	}

	return (rc);
}

static void
tool_init_spads(struct tool_ctx *tc)
{
	int sidx, pidx;

	/* Initialize inbound scratchpad structures */
	tc->inspad_cnt = ntb_spad_count(tc->dev);
	tc->inspads = malloc(tc->inspad_cnt * sizeof(*tc->inspads), M_NTB_TOOL,
	    M_WAITOK | M_ZERO);

	for (sidx = 0; sidx < tc->inspad_cnt; sidx++) {
		tc->inspads[sidx].sidx = sidx;
		tc->inspads[sidx].pidx = -1;
		tc->inspads[sidx].tc = tc;
	}

	/* Initialize outbound scratchpad structures */
	for (pidx = 0; pidx < tc->peer_cnt; pidx++) {
		tc->peers[pidx].outspad_cnt = ntb_spad_count(tc->dev);
		tc->peers[pidx].outspads =  malloc(tc->peers[pidx].outspad_cnt *
		    sizeof(*tc->peers[pidx].outspads), M_NTB_TOOL, M_WAITOK |
		    M_ZERO);

		for (sidx = 0; sidx < tc->peers[pidx].outspad_cnt; sidx++) {
			tc->peers[pidx].outspads[sidx].sidx = sidx;
			tc->peers[pidx].outspads[sidx].pidx = pidx;
			tc->peers[pidx].outspads[sidx].tc = tc;
		}
	}
}

static void
tool_clear_spads(struct tool_ctx *tc)
{
	int pidx;

	/* Free local inspads. */
	free(tc->inspads, M_NTB_TOOL);

	/* Free outspads for each peer. */
	for (pidx = 0; pidx < tc->peer_cnt; pidx++)
		free(tc->peers[pidx].outspads, M_NTB_TOOL);
}

/*
 * Initialization methods
 */
static int
tool_check_ntb(struct tool_ctx *tc)
{

	/* create and initialize link callout handler */
	callout_init(&tc->link_event_timer, 1);

	/* create and initialize db callout handler */
	callout_init(&tc->db_event_timer, 1);

	/* Initialize sysctl read out values to default */
	tc->link_status = 'U';
	tc->db_mask_val = 0;
	tc->peer_db_val = 0;
	tc->peer_db_mask_val = 0;
	tc->db_event_val = 0;
	tc->link_bits = 0;

	return (0);
}

static void
tool_clear_data(struct tool_ctx *tc)
{

	callout_drain(&tc->link_event_timer);
	callout_drain(&tc->db_event_timer);
}

static int
tool_init_ntb(struct tool_ctx *tc)
{

	return ntb_set_ctx(tc->dev, tc, &tool_ops);
}

static void
tool_clear_ntb(struct tool_ctx *tc)
{

	ntb_clear_ctx(tc->dev);
	ntb_link_disable(tc->dev);
}

/*
 *  Current sysctl implementation is made such that it gets attached to the
 *  device and while detach it gets cleared automatically.
 */
static void
tool_setup_sysctl(struct tool_ctx *tc)
{
	char buf[TOOL_BUF_LEN], desc[TOOL_BUF_LEN];
	struct sysctl_oid_list *top, *peer_top;
	struct sysctl_oid *parent, *peer;
	struct sysctl_ctx_list *clist;
	unsigned int pidx, sidx, widx;

	clist = device_get_sysctl_ctx(tc->dev);
	parent = device_get_sysctl_tree(tc->dev);
	top = SYSCTL_CHILDREN(parent);

	SYSCTL_ADD_PROC(clist, top, OID_AUTO, "port", CTLTYPE_UINT |
	    CTLFLAG_RDTUN | CTLFLAG_MPSAFE, tc, 0, sysctl_local_port_number,
	    "IU", "local port number");

	SYSCTL_ADD_PROC(clist, top, OID_AUTO, "link", CTLTYPE_STRING |
	    CTLFLAG_RWTUN | CTLFLAG_MPSAFE, tc, 0, sysctl_link_handle,
	    "IU", "link info");

	SYSCTL_ADD_PROC(clist, top, OID_AUTO, "db", CTLTYPE_STRING |
	    CTLFLAG_RWTUN | CTLFLAG_MPSAFE, tc, 0, sysctl_db_handle,
	    "A", "db info");

	SYSCTL_ADD_PROC(clist, top, OID_AUTO, "db_valid_mask", CTLTYPE_STRING |
	    CTLFLAG_RD | CTLFLAG_MPSAFE, tc, 0, sysctl_db_valid_mask_handle,
	    "A", "db valid mask");

	SYSCTL_ADD_PROC(clist, top, OID_AUTO, "db_mask", CTLTYPE_STRING |
	    CTLFLAG_RWTUN | CTLFLAG_MPSAFE, tc, 0, sysctl_db_mask_handle,
	    "A", "db mask");

	SYSCTL_ADD_PROC(clist, top, OID_AUTO, "db_event", CTLTYPE_STRING |
	    CTLFLAG_WR | CTLFLAG_MPSAFE, tc, 0, sysctl_db_event_handle,
	    "A", "db event");

	SYSCTL_ADD_PROC(clist, top, OID_AUTO, "peer_db", CTLTYPE_STRING |
	    CTLFLAG_RWTUN | CTLFLAG_MPSAFE, tc, 0, sysctl_peer_db_handle,
	    "A", "peer db");

	SYSCTL_ADD_PROC(clist, top, OID_AUTO, "peer_db_mask", CTLTYPE_STRING |
	    CTLFLAG_RWTUN | CTLFLAG_MPSAFE, tc, 0, sysctl_peer_db_mask_handle,
	    "IU", "peer db mask info");

	if (tc->inspad_cnt != 0) {
		for (sidx = 0; sidx < tc->inspad_cnt; sidx++) {
			snprintf(buf, sizeof(buf), "spad%d", sidx);
			snprintf(desc, sizeof(desc), "spad%d info", sidx);

			SYSCTL_ADD_PROC(clist, top, OID_AUTO, buf,
			    CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
			    tc, sidx, sysctl_spad_handle, "IU", desc);
		}
	}

	for (pidx = 0; pidx < tc->peer_cnt; pidx++) {
		snprintf(buf, sizeof(buf), "peer%d", pidx);

		peer = SYSCTL_ADD_NODE(clist, top, OID_AUTO, buf,
		    CTLFLAG_RW | CTLFLAG_MPSAFE, 0, buf);
		peer_top = SYSCTL_CHILDREN(peer);

		SYSCTL_ADD_PROC(clist, peer_top, OID_AUTO, "port",
		    CTLTYPE_UINT | CTLFLAG_RDTUN | CTLFLAG_MPSAFE, tc, pidx,
		    sysctl_peer_port_number, "IU", "peer port number");

		SYSCTL_ADD_PROC(clist, peer_top, OID_AUTO, "link",
		    CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE, tc, pidx,
		    sysctl_peer_link_handle, "IU", "peer_link info");

		SYSCTL_ADD_PROC(clist, peer_top, OID_AUTO, "link_event",
		    CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE, tc, pidx,
		    sysctl_peer_link_event_handle, "IU", "link event");

		for (widx = 0; widx < tc->peers[pidx].inmw_cnt; widx++) {
			snprintf(buf, sizeof(buf), "mw_trans%d", widx);
			snprintf(desc, sizeof(desc), "mw trans%d info", widx);

			SYSCTL_ADD_PROC(clist, peer_top, OID_AUTO, buf,
			    CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
			    &tc->peers[pidx].inmws[widx], 0,
			    sysctl_mw_trans_handler, "IU", desc);

			snprintf(buf, sizeof(buf), "mw%d", widx);
			snprintf(desc, sizeof(desc), "mw%d info", widx);

			SYSCTL_ADD_PROC(clist, peer_top, OID_AUTO, buf,
			    CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
			    &tc->peers[pidx].inmws[widx], 0,
			    sysctl_mw_handle, "IU", desc);

			snprintf(buf, sizeof(buf), "peer_mw%d", widx);
			snprintf(desc, sizeof(desc), "peer_mw%d info", widx);

			SYSCTL_ADD_PROC(clist, peer_top, OID_AUTO, buf,
			    CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
			    &tc->peers[pidx].inmws[widx], 0,
			    sysctl_peer_mw_handle, "IU", desc);
		}

		for (sidx = 0; sidx < tc->peers[pidx].outspad_cnt; sidx++) {
			snprintf(buf, sizeof(buf), "spad%d", sidx);
			snprintf(desc, sizeof(desc), "spad%d info", sidx);

			SYSCTL_ADD_PROC(clist, peer_top, OID_AUTO, buf,
			    CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
			    tc, sidx, sysctl_peer_spad_handle, "IU", desc);
		}
	}
}

static int
ntb_tool_probe(device_t dev)
{
	device_set_desc(dev, "NTB TOOL");
	return (0);
}

static int
ntb_tool_attach(device_t dev)
{
	struct tool_ctx *tc = device_get_softc(dev);
	int rc = 0;

	tc->dev = dev;
	rc = tool_check_ntb(tc);
	if (rc)
		goto out;

	tool_init_peers(tc);

	rc = tool_init_mws(tc);
	if (rc)
		goto err_clear_data;

	tool_init_spads(tc);

	rc = tool_init_ntb(tc);
	if (rc)
		goto err_clear_spads;

	tool_setup_sysctl(tc);

	return (0);

err_clear_spads:
	tool_clear_spads(tc);
	tool_clear_mws(tc);
	tool_clear_peers(tc);
err_clear_data:
	tool_clear_data(tc);
out:
	device_printf(dev, "ntb_tool attached failed with err=(%d).\n", rc);
	return (rc);
}

static int
ntb_tool_detach(device_t dev)
{
	struct tool_ctx *tc = device_get_softc(dev);

	tool_clear_ntb(tc);

	tool_clear_spads(tc);

	tool_clear_mws(tc);

	tool_clear_peers(tc);

	tool_clear_data(tc);

	return (0);
}

static device_method_t ntb_tool_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,     ntb_tool_probe),
	DEVMETHOD(device_attach,    ntb_tool_attach),
	DEVMETHOD(device_detach,    ntb_tool_detach),
	DEVMETHOD_END
};

devclass_t ntb_tool_devclass;
static DEFINE_CLASS_0(ntb_tool, ntb_tool_driver, ntb_tool_methods,
    sizeof(struct tool_ctx));
DRIVER_MODULE(ntb_tool, ntb_hw, ntb_tool_driver, ntb_tool_devclass, NULL, NULL);
MODULE_DEPEND(ntb_tool, ntb, 1, 1, 1);
MODULE_VERSION(ntb_tool, 1.0);
