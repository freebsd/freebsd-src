/*-
 * Copyright (c) 2006 IronPort Systems Inc. <ambrisko@ironport.com>
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
 * $FreeBSD$
 */

struct ipmi_get_info {
	int		kcs_mode;
	int		smic_mode;
	uint64_t	address;
	int		offset;
	int		io_mode;
};

struct ipmi_softc {
	device_t		ipmi_dev;
	device_t		ipmi_smbios_dev;
	struct cdev		*ipmi_dev_t;
	int			ipmi_refcnt;
	struct smbios_table_entry *ipmi_smbios;
	struct ipmi_get_info	ipmi_bios_info;
	int			ipmi_kcs_status_reg;
	int			ipmi_kcs_command_reg;
	int			ipmi_kcs_data_out_reg;
	int			ipmi_kcs_data_in_reg;
	int			ipmi_smic_data;
	int			ipmi_smic_ctl_sts;
	int			ipmi_smic_flags;
	int			ipmi_io_rid;
	struct resource	*	ipmi_io_res;
	int			ipmi_mem_rid;
	struct resource	*	ipmi_mem_res;
	int			ipmi_irq_rid;
	struct resource	*	ipmi_irq_res;
	void			*ipmi_irq;
	u_char			ipmi_address;
	u_char			ipmi_lun;
	int			ipmi_busy;
	struct selinfo		ipmi_select;
	int			ipmi_timestamp;
	int 			ipmi_requests;
	struct callout_handle   ipmi_timeout_handle;
	TAILQ_HEAD(,ipmi_done_list)	ipmi_done;
	eventhandler_tag	ipmi_ev_tag;
};

struct ipmi_ipmb {
	u_char foo;
};

/* KCS status flags */
#define KCS_STATUS_OBF			0x01 /* Data Out ready from BMC */
#define KCS_STATUS_IBF			0x02 /* Data In from System */
#define KCS_STATUS_SMS_ATN		0x04 /* Ready in RX queue */
#define KCS_STATUS_C_D			0x08 /* Command/Data register write*/
#define KCS_STATUS_OEM1			0x10
#define KCS_STATUS_OEM2			0x20
#define KCS_STATUS_S0			0x40
#define KCS_STATUS_S1			0x80
 #define KCS_STATUS_STATE(x)		((x)>>6)
 #define KCS_STATUS_STATE_IDLE		0x0
 #define KCS_STATUS_STATE_READ		0x1
 #define KCS_STATUS_STATE_WRITE		0x2
 #define KCS_STATUS_STATE_ERROR		0x3
#define KCS_IFACE_STATUS_ABORT		0x01
#define KCS_IFACE_STATUS_ILLEGAL	0x02
#define KCS_IFACE_STATUS_LENGTH_ERR	0x06

/* KCD control codes */
#define KCS_CONTROL_GET_STATUS_ABORT	0x60
#define KCS_CONTROL_WRITE_START		0x61
#define KCS_CONTROL_WRITE_END		0x62
#define KCS_DATA_IN_READ		0x68

/* SMIC status flags */
#define SMIC_STATUS_BUSY		0x01 /* System set and BMC clears it */
#define SMIC_STATUS_SMS_ATN		0x04 /* BMC has a message */
#define SMIC_STATUS_EVT_ATN		0x08 /* Event has been RX */
#define SMIC_STATUS_SMI			0x08 /* asserted SMI */
#define SMIC_STATUS_TX_RDY		0x40 /* Ready to accept WRITE */
#define SMIC_STATUS_RX_RDY		0x80 /* Ready to read */

/* SMIC control codes */
#define SMIC_CC_SMS_GET_STATUS		0x40
#define SMIC_CC_SMS_WR_START		0x41
#define SMIC_CC_SMS_WR_NEXT		0x42
#define SMIC_CC_SMS_WR_END		0x43
#define SMIC_CC_SMS_RD_START		0x44
#define SMIC_CC_SMS_RD_NEXT		0x45
#define SMIC_CC_SMS_RD_END		0x46

/* SMIC status codes */
#define SMIC_SC_SMS_RDY			0xc0
#define SMIC_SC_SMS_WR_START		0xc1
#define SMIC_SC_SMS_WR_NEXT		0xc2
#define SMIC_SC_SMS_WR_END		0xc3
#define SMIC_SC_SMS_RD_START		0xc4
#define SMIC_SC_SMS_RD_NEXT		0xc5
#define SMIC_SC_SMS_RD_END		0xc6

#define RES(x) (x)->ipmi_io_res ? (x)->ipmi_io_res : (x)->ipmi_mem_res
#define INB(sc, x) bus_space_read_1(rman_get_bustag(RES(sc)),	\
    rman_get_bushandle(RES(sc)), (x))
#define OUTB(sc, x, value) bus_space_write_1(rman_get_bustag(RES(sc)), \
    rman_get_bushandle(RES(sc)), (x), value)

int ipmi_attach(device_t);
int ipmi_detach(device_t);
int ipmi_smbios_query(device_t);
int ipmi_smbios_probe(device_t);
int ipmi_read(device_t, u_char *, int);
void ipmi_intr(void *);

device_t ipmi_smbios_identify (driver_t *driver, device_t parent);
extern devclass_t ipmi_devclass;
extern int ipmi_attached;
