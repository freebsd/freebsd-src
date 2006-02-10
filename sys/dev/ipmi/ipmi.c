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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/poll.h>
#include <sys/selinfo.h>

#include <sys/disk.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/clock.h>
#include <sys/rman.h>
#include <sys/watchdog.h>
#include <sys/sysctl.h>

#ifdef LOCAL_MODULE
#include <ipmi.h>
#include <ipmivars.h>
#else
#include <sys/ipmi.h>
#include <dev/ipmi/ipmivars.h>
#endif

struct ipmi_done_list {
	u_char		*data;
	int		channel;
	int		msgid;
	int		len;
	TAILQ_ENTRY(ipmi_done_list) list;
};

#define MAX_TIMEOUT 3 * hz

static int ipmi_wait_for_ibf(device_t, int);
static int ipmi_wait_for_obf(device_t, int);
static void ipmi_clear_obf(device_t, int);
static void ipmi_error(device_t);
static void ipmi_check_read(device_t);
static int ipmi_write(device_t, u_char *, int);
static void ipmi_wait_for_tx_okay(device_t);
static void ipmi_wait_for_rx_okay(device_t);
static void ipmi_wait_for_not_busy(device_t);
static void ipmi_set_busy(device_t);
static int ipmi_ready_to_read(device_t);
#ifdef IPMB
static int ipmi_handle_attn(device_t dev);
static int ipmi_ipmb_checksum(u_char, int);
static int ipmi_ipmb_send_message(device_t, u_char, u_char, u_char,
     u_char, u_char, int)
#endif

static d_ioctl_t ipmi_ioctl;
static d_poll_t ipmi_poll;
static d_open_t ipmi_open;
static d_close_t ipmi_close;

int ipmi_attached = 0;

#define IPMI_MINOR	0

static int on = 1;
SYSCTL_NODE(_hw, OID_AUTO, ipmi, CTLFLAG_RD, 0, "IPMI driver parameters");
SYSCTL_INT(_hw_ipmi, OID_AUTO, on, CTLFLAG_RW,
        &on, 0, "");

static struct cdevsw ipmi_cdevsw = {
	.d_version =    D_VERSION,
	.d_flags =      D_NEEDGIANT,
	.d_open =	ipmi_open,
	.d_close =	ipmi_close,
	.d_ioctl =	ipmi_ioctl,
	.d_poll =	ipmi_poll,
	.d_name =	"ipmi",
};

MALLOC_DEFINE(M_IPMI, "ipmi", "ipmi");

static	int
ipmi_open(struct cdev *dev, int flags, int fmt, struct thread *td)
{
	struct ipmi_softc *sc;

	if (!on)
		return ENOENT;

	sc = dev->si_drv1;
	if (sc->ipmi_refcnt) {
		return EBUSY;
	}
	sc->ipmi_refcnt = 1;

	return 0;
}

static	int
ipmi_poll(struct cdev *dev, int poll_events, struct thread *td)
{
	struct ipmi_softc *sc;
	int revents = 0;

	sc = dev->si_drv1;

	ipmi_check_read(sc->ipmi_dev);

	if (poll_events & (POLLIN | POLLRDNORM)) {
		if (!TAILQ_EMPTY(&sc->ipmi_done))
		    revents |= poll_events & (POLLIN | POLLRDNORM);
		if (TAILQ_EMPTY(&sc->ipmi_done) && sc->ipmi_requests == 0) {
		    revents |= POLLERR;
		}
	}

	if (revents == 0) {
		if (poll_events & (POLLIN | POLLRDNORM))
			selrecord(td, &sc->ipmi_select);
	}

	return revents;
}

static	int
ipmi_close(struct cdev *dev, int flags, int fmt, struct thread *td)
{
	struct ipmi_softc *sc;
	int error = 0;

	sc = dev->si_drv1;

	sc->ipmi_refcnt = 0;

	return error;
}

#ifdef IPMB
static int
ipmi_ipmb_checksum(u_char *data, int len)
{
	u_char sum = 0;

	for (; len; len--) {
		sum += *data++;
	}
	return -sum;
}

static int
ipmi_ipmb_send_message(device_t dev, u_char channel, u_char netfn,
    u_char command, u_char seq, u_char *data, int data_len)
{
	u_char *temp;
	struct ipmi_softc *sc = device_get_softc(dev);
	int error;
	u_char slave_addr = 0x52;

	temp = malloc(data_len + 10, M_IPMI, M_WAITOK);
	bzero(temp, data_len + 10);
	temp[0] = IPMI_APP_REQUEST << 2;
	temp[1] = IPMI_SEND_MSG;
	temp[2] = channel;
	temp[3] = slave_addr;
	temp[4] = netfn << 2;
	temp[5] = ipmi_ipmb_check_sum(&temp[3], 2);
	temp[6] = sc->ipmi_address;
	temp[7] = seq << 2 | sc->ipmi_lun;
	temp[8] = command;

	bcopy(data, &temp[9], data_len);
	temp[data_len + 9] = ipmi_ipmb_check(&temp[6], data_len + 3);
	ipmi_write(sc->ipmi_dev, temp, data_len + 9);
	free(temp, M_IPMI);

	while (!ipmi_ready_to_read(dev))
		DELAY(1000);
	temp = malloc(IPMI_MAX_RX, M_IPMI, M_WAITOK);
	bzero(temp, IPMI_MAX_RX);
	error = ipmi_read(dev, temp, IPMI_MAX_RX);
	free(temp, M_IPMI);

	return error;
}

static int
ipmi_handle_attn(device_t dev)
{
	u_char temp[IPMI_MAX_RX];
	struct ipmi_softc *sc = device_get_softc(dev);
	int error;

	device_printf(sc->ipmi_dev, "BMC has a message\n");
	temp[0] = IPMI_APP_REQUEST << 2;
	temp[1] = IPMI_GET_MSG_FLAGS;
	ipmi_write(sc->ipmi_dev, temp, 2);
	while (!ipmi_ready_to_read(dev))
		DELAY(1000);
	bzero(temp, IPMI_MAX_RX);
	error = ipmi_read(dev, temp, IPMI_MAX_RX);

	if (temp[2] == 0) {
		if (temp[3] & IPMI_MSG_BUFFER_FULL) {
			device_printf(sc->ipmi_dev, "message buffer full");
		}
		if (temp[3] & IPMI_WDT_PRE_TIMEOUT) {
			device_printf(sc->ipmi_dev,
			    "watchdog about to go off");
		}
		if (temp[3] & IPMI_MSG_AVAILABLE) {
			temp[0] = IPMI_APP_REQUEST << 2;
			temp[1] = IPMI_GET_MSG;
			ipmi_write(sc->ipmi_dev, temp, 2);
			while (!ipmi_ready_to_read(dev))
				DELAY(1000);
			bzero(temp, IPMI_MAX_RX);
			error = ipmi_read(dev, temp, IPMI_MAX_RX);

			device_printf(sc->ipmi_dev, "throw out message ");
			dump_buf(temp, 16);
		}
	} else
		return -1;
	return error;
}
#endif

static int
ipmi_ready_to_read(device_t dev)
{
	struct ipmi_softc *sc = device_get_softc(dev);
	int status, flags;

	if (sc->ipmi_bios_info.smic_mode) {
		flags = INB(sc, sc->ipmi_smic_flags);
#ifdef IPMB
		if (flags & SMIC_STATUS_SMS_ATN) {
			ipmi_handle_attn(dev);
			return 0;
		}
#endif
		if (flags & SMIC_STATUS_RX_RDY)
			 return 1;
	} else if (sc->ipmi_bios_info.kcs_mode) {
		status = INB(sc, sc->ipmi_kcs_status_reg);
#ifdef IPMB
		if (status & KCS_STATUS_SMS_ATN) {
			ipmi_handle_attn(dev);
			return 0;
		}
#endif
		if (status & KCS_STATUS_OBF)
			 return 1;
	} else {
		device_printf(dev,"Unsupported mode\n");
	}

	return 0;
}

void
ipmi_intr(void *arg) {
	device_t  dev = arg;

	ipmi_check_read(dev);
}

static void
ipmi_check_read(device_t dev){
	struct ipmi_softc *sc = device_get_softc(dev);
	struct ipmi_done_list *item;
	int status;
	u_char *temp;

	if (!sc->ipmi_requests)
		return;

	untimeout((timeout_t *)ipmi_check_read, dev, sc->ipmi_timeout_handle);

	if(ipmi_ready_to_read(dev)) {
		sc->ipmi_requests--;
		temp = malloc(IPMI_MAX_RX, M_IPMI, M_WAITOK);
		bzero(temp, IPMI_MAX_RX);
		status = ipmi_read(dev, temp, IPMI_MAX_RX);
		item = malloc(sizeof(struct ipmi_done_list), M_IPMI, M_WAITOK);
		bzero(item, sizeof(struct ipmi_done_list));
		item->data = temp;
		item->len  = status;
		if (ticks - sc->ipmi_timestamp > MAX_TIMEOUT) {
			device_printf(dev, "read timeout when ready\n");
			TAILQ_INSERT_TAIL(&sc->ipmi_done, item, list);
			selwakeup(&sc->ipmi_select);
		} else if (status) {
			TAILQ_INSERT_TAIL(&sc->ipmi_done, item, list);
			selwakeup(&sc->ipmi_select);
		}
	} else {
		if (ticks - sc->ipmi_timestamp > MAX_TIMEOUT) {
			sc->ipmi_requests--;
			device_printf(dev, "read timeout when not ready\n");
			temp = malloc(IPMI_MAX_RX, M_IPMI, M_WAITOK);
			bzero(temp, IPMI_MAX_RX);
			sc->ipmi_busy = 0;
			wakeup(&sc->ipmi_busy);
			status = -1;
			item = malloc(sizeof(struct ipmi_done_list),
			    M_IPMI, M_WAITOK);
			bzero(item, sizeof(struct ipmi_done_list));
			item->data = temp;
			item->len  = status;
			TAILQ_INSERT_TAIL(&sc->ipmi_done, item, list);
			selwakeup(&sc->ipmi_select);
		}
	}
	if (sc->ipmi_requests)
		sc->ipmi_timeout_handle
			= timeout((timeout_t *)ipmi_check_read, dev, hz/30);
}

static int
ipmi_ioctl(struct cdev *dev __unused, u_long cmd, caddr_t data,
    int flags, struct thread *td)
{
	struct ipmi_softc *sc;
	struct ipmi_req *req = (struct ipmi_req *)data;
	struct ipmi_recv *recv = (struct ipmi_recv *)data;
	struct ipmi_addr addr;
	struct ipmi_done_list *item;
	u_char *temp;
	int error, len;

	sc = dev->si_drv1;

	switch (cmd) {
	case IPMICTL_SEND_COMMAND:
		/* clear out old stuff in queue of stuff done */
		while((item = TAILQ_FIRST(&sc->ipmi_done))) {
			TAILQ_REMOVE(&sc->ipmi_done, item, list);
			free(item->data, M_IPMI);
			free(item, M_IPMI);
		}

		error = copyin(req->addr, &addr, sizeof(addr));
		temp = malloc(req->msg.data_len + 2, M_IPMI, M_WAITOK);
		if (temp == NULL) {
			return ENOMEM;
		}
		temp[0] = req->msg.netfn << 2;
		temp[1] = req->msg.cmd;
		error = copyin(req->msg.data, &temp[2],
		    req->msg.data_len);
		if (error != 0) {
			free(temp, M_IPMI);
			return error;
		}
		error = ipmi_write(sc->ipmi_dev,
		    temp, req->msg.data_len + 2);
		free(temp, M_IPMI);

		if (error != 1)
			return EIO;
		sc->ipmi_requests++;
		sc->ipmi_timestamp = ticks;
		ipmi_check_read(sc->ipmi_dev);

		return 0;
	case IPMICTL_RECEIVE_MSG_TRUNC:
	case IPMICTL_RECEIVE_MSG:
		item = TAILQ_FIRST(&sc->ipmi_done);
		if (!item) {
			return EAGAIN;
		}

		error = copyin(recv->addr, &addr, sizeof(addr));
		if (error != 0)
			return error;
		TAILQ_REMOVE(&sc->ipmi_done, item, list);
		addr.channel = IPMI_BMC_CHANNEL;
		recv->recv_type = IPMI_RESPONSE_RECV_TYPE;
		recv->msgid = item->msgid;
		recv->msg.netfn = item->data[0] >> 2;
		recv->msg.cmd = item->data[1];
		error = len = item->len;
		len -= 2;
		if (len < 0)
			len = 1;
		if (recv->msg.data_len < len && cmd == IPMICTL_RECEIVE_MSG) {
			TAILQ_INSERT_HEAD(&sc->ipmi_done, item, list);
			return EMSGSIZE;
		}
		len = min(recv->msg.data_len, len);
		recv->msg.data_len = len;
		error = copyout(&addr, recv->addr,sizeof(addr));
		if (error == 0)
			error = copyout(&item->data[2], recv->msg.data, len);
		free(item->data, M_IPMI);
		free(item, M_IPMI);

		if (error != 0)
			return error;
		return 0;
	case IPMICTL_SET_MY_ADDRESS_CMD:
		sc->ipmi_address = *(int*)data;
		return 0;
	case IPMICTL_GET_MY_ADDRESS_CMD:
		*(int*)data = sc->ipmi_address;
		return 0;
	case IPMICTL_SET_MY_LUN_CMD:
		sc->ipmi_lun = *(int*)data & 0x3;
		return 0;
	case IPMICTL_GET_MY_LUN_CMD:
		*(int*)data = sc->ipmi_lun;
		return 0;
	case IPMICTL_SET_GETS_EVENTS_CMD:
		/*
		device_printf(sc->ipmi_dev,
		    "IPMICTL_SET_GETS_EVENTS_CMD NA\n");
		*/
		return 0;
	case IPMICTL_REGISTER_FOR_CMD:
	case IPMICTL_UNREGISTER_FOR_CMD:
		return EOPNOTSUPP;
	}

	device_printf(sc->ipmi_dev, "Unknown IOCTL %lX\n", cmd);

	return ENOIOCTL;
}

static int
ipmi_wait_for_ibf(device_t dev, int state) {
	struct ipmi_softc *sc = device_get_softc(dev);
	int status, start = ticks;
	int first = 1;

	if (state == 0) {
		/* WAIT FOR IBF = 0 */
		do {
			if (first)
				first =0;
			else
				DELAY(100);
			status = INB(sc, sc->ipmi_kcs_status_reg);
		} while (ticks - start < MAX_TIMEOUT
		    && status & KCS_STATUS_IBF);
	} else {
		/* WAIT FOR IBF = 1 */
		do {
			if (first)
				first =0;
			else
				DELAY(100);
			status = INB(sc, sc->ipmi_kcs_status_reg);
		} while (ticks - start < MAX_TIMEOUT
		    && !(status & KCS_STATUS_IBF));
	}
	return status;
}

static int
ipmi_wait_for_obf(device_t dev, int state) {
	struct ipmi_softc *sc = device_get_softc(dev);
	int status, start = ticks;
	int first = 1;

	if (state == 0) {
		/* WAIT FOR OBF = 0 */
		do {
			if (first)
				first = 0;
			else
				DELAY(100);
			status = INB(sc, sc->ipmi_kcs_status_reg);
		} while (ticks - start < MAX_TIMEOUT
		    && status & KCS_STATUS_OBF);
	} else {
		/* WAIT FOR OBF = 1 */
		do {
			if (first)
				first =0;
			else
				DELAY(100);
			status = INB(sc, sc->ipmi_kcs_status_reg);
		} while (ticks - start < MAX_TIMEOUT
		    && !(status & KCS_STATUS_OBF));
	}
	return status;
}

static void
ipmi_clear_obf(device_t dev, int status) {
	struct ipmi_softc *sc = device_get_softc(dev);
	int data;

	/* Clear OBF */
	if (status & KCS_STATUS_OBF) {
		data = INB(sc, sc->ipmi_kcs_data_out_reg);
	}
}

static void
ipmi_error(device_t dev) {
	struct ipmi_softc *sc = device_get_softc(dev);
	int status, data = 0;
	int retry = 0;

	for(;;){
		status = ipmi_wait_for_ibf(dev, 0);

		/* ABORT */
		OUTB(sc, sc->ipmi_kcs_command_reg,
		     KCS_CONTROL_GET_STATUS_ABORT);

		/* Wait for IBF = 0 */
		status = ipmi_wait_for_ibf(dev, 0);

		/* Clear OBF */
		ipmi_clear_obf(dev, status);

		if (status & KCS_STATUS_OBF) {
			data = INB(sc, sc->ipmi_kcs_data_out_reg);
			device_printf(dev, "Data %x\n", data);
		}

		/* 0x00 to DATA_IN */
		OUTB(sc, sc->ipmi_kcs_data_in_reg, 0x00);

		/* Wait for IBF = 0 */
		status = ipmi_wait_for_ibf(dev, 0);

		if (KCS_STATUS_STATE(status) == KCS_STATUS_STATE_READ) {

			/* Wait for OBF = 1 */
			status = ipmi_wait_for_obf(dev, 1);

			/* Read error status */
			data = INB(sc, sc->ipmi_kcs_data_out_reg);

			/* Write READ into Data_in */
			OUTB(sc, sc->ipmi_kcs_data_in_reg, KCS_DATA_IN_READ);

			/* Wait for IBF = 0 */
			status = ipmi_wait_for_ibf(dev, 0);
		}

		/* IDLE STATE */
		if (KCS_STATUS_STATE(status) == KCS_STATUS_STATE_IDLE) {
			/* Wait for OBF = 1 */
			status = ipmi_wait_for_obf(dev, 1);

			/* Clear OBF */
			ipmi_clear_obf(dev, status);
			break;
		}

		retry++;
		if (retry > 2) {
			device_printf(dev, "Retry exhausted %x\n", retry);
			break;
		}
	}
}

static void
ipmi_wait_for_tx_okay(device_t dev) {
	struct ipmi_softc *sc = device_get_softc(dev);
	int flags;

	do {
		flags = INB(sc, sc->ipmi_smic_flags);
	} while(!flags & SMIC_STATUS_TX_RDY);
}

static void
ipmi_wait_for_rx_okay(device_t dev) {
	struct ipmi_softc *sc = device_get_softc(dev);
	int flags;

	do {
		flags = INB(sc, sc->ipmi_smic_flags);
	} while(!flags & SMIC_STATUS_RX_RDY);
}

static void
ipmi_wait_for_not_busy(device_t dev) {
	struct ipmi_softc *sc = device_get_softc(dev);
	int flags;

	do {
		flags = INB(sc, sc->ipmi_smic_flags);
	} while(flags & SMIC_STATUS_BUSY);
}

static void
ipmi_set_busy(device_t dev) {
	struct ipmi_softc *sc = device_get_softc(dev);
	int flags;

	flags = INB(sc, sc->ipmi_smic_flags);
	flags |= SMIC_STATUS_BUSY;
	OUTB(sc, sc->ipmi_smic_flags, flags);
}

int
ipmi_read(device_t dev, u_char *bytes, int len){
	struct ipmi_softc *sc = device_get_softc(dev);
	int status, flags, data, i = -1, error;

	bzero(bytes, len);
	if (sc->ipmi_bios_info.smic_mode) {
		ipmi_wait_for_not_busy(dev);
		do {
			flags = INB(sc, sc->ipmi_smic_flags);
		} while(!flags & SMIC_STATUS_RX_RDY);

		OUTB(sc, sc->ipmi_smic_ctl_sts, SMIC_CC_SMS_RD_START);
		ipmi_wait_for_rx_okay(dev);
		ipmi_set_busy(dev);
		ipmi_wait_for_not_busy(dev);
		status = INB(sc, sc->ipmi_smic_ctl_sts);
		if (status != SMIC_SC_SMS_RD_START) {
			error = INB(sc, sc->ipmi_smic_data);
			device_printf(dev, "Read did not start %x %x\n",
			    status, error);
			sc->ipmi_busy = 0;
			return -1;
		}
		for (i = -1; ; len--) {
			i++;
			data = INB(sc, sc->ipmi_smic_data);
			if (len > 0)
				*bytes++ = data;
			else {
				device_printf(dev, "Read short %x\n", data);
				break;
			}
			do {
				flags = INB(sc, sc->ipmi_smic_flags);
			} while(!flags & SMIC_STATUS_RX_RDY);

			OUTB(sc, sc->ipmi_smic_ctl_sts, SMIC_CC_SMS_RD_NEXT);
			ipmi_wait_for_rx_okay(dev);
			ipmi_set_busy(dev);
			ipmi_wait_for_not_busy(dev);
			status = INB(sc, sc->ipmi_smic_ctl_sts);
			if (status == SMIC_SC_SMS_RD_NEXT) {
				continue;
			} else if (status == SMIC_SC_SMS_RD_END) {
				break;
			} else {
				device_printf(dev, "Read did not next %x\n",
				    status);
			}
		}
		i++;
		data = INB(sc, sc->ipmi_smic_data);
		if (len > 0)
			*bytes++ = data;
		else
			device_printf(dev, "Read short %x\n", data);

		OUTB(sc, sc->ipmi_smic_ctl_sts, SMIC_CC_SMS_RD_END);
		i++;

	} else if (sc->ipmi_bios_info.kcs_mode) {
		for (i = -1; ; len--) {
			/* Wait for IBF = 0 */
			status = ipmi_wait_for_ibf(dev, 0);

			/* Read State */
			if (KCS_STATUS_STATE(status)
			    == KCS_STATUS_STATE_READ) {
				i++;

				/* Wait for OBF = 1 */
				status = ipmi_wait_for_obf(dev, 1);

				/* Read Data_out */
				data = INB(sc, sc->ipmi_kcs_data_out_reg);
				if (len > 0)
					*bytes++ = data;
				else {
					device_printf(dev, "Read short %x byte %d\n", data, i);
					break;
				}

				/* Write READ into Data_in */
				OUTB(sc, sc->ipmi_kcs_data_in_reg,
				    KCS_DATA_IN_READ);

				/* Idle State */
			} else if (KCS_STATUS_STATE(status)
			    == KCS_STATUS_STATE_IDLE) {
				i++;

				/* Wait for OBF = 1*/
				status = ipmi_wait_for_obf(dev, 1);

				/* Read Dummy */
				data = INB(sc, sc->ipmi_kcs_data_out_reg);
				break;

				/* error state */
			} else {
				device_printf(dev,
				    "read status error %x byte %d\n",
				    status, i);
				sc->ipmi_busy = 0;
				ipmi_error(dev);
				return -1;
			}
		}
	} else {
		device_printf(dev, "Unsupported mode\n");
	}
	sc->ipmi_busy = 0;
	wakeup(&sc->ipmi_busy);

	return i;
}


static int
ipmi_write(device_t dev, u_char *bytes, int len){
	struct ipmi_softc *sc = device_get_softc(dev);
	int status, flags, retry;

	while(sc->ipmi_busy){
		status = tsleep(&sc->ipmi_busy, PCATCH, "ipmi", 0);
		if (status)
			return status;
	}
	sc->ipmi_busy = 1;
	if (sc->ipmi_bios_info.smic_mode) {
		ipmi_wait_for_not_busy(dev);

		OUTB(sc, sc->ipmi_smic_ctl_sts, SMIC_CC_SMS_WR_START);
		ipmi_wait_for_tx_okay(dev);
		OUTB(sc, sc->ipmi_smic_data, *bytes++);
		len--;
		ipmi_set_busy(dev);
		ipmi_wait_for_not_busy(dev);
		status = INB(sc, sc->ipmi_smic_ctl_sts);
		if (status != SMIC_SC_SMS_WR_START) {
			device_printf(dev, "Write did not start %x\n",status);
			sc->ipmi_busy = 0;
			return -1;
		}
		for(len--; len; len--) {
			OUTB(sc, sc->ipmi_smic_ctl_sts, SMIC_CC_SMS_WR_NEXT);
			ipmi_wait_for_tx_okay(dev);
			OUTB(sc, sc->ipmi_smic_data, *bytes++);
			ipmi_set_busy(dev);
			ipmi_wait_for_not_busy(dev);
			status = INB(sc, sc->ipmi_smic_ctl_sts);
			if (status != SMIC_SC_SMS_WR_NEXT) {
				device_printf(dev, "Write did not next %x\n",
				    status);
				sc->ipmi_busy = 0;
				return -1;
			}
		}
		do {
			flags = INB(sc, sc->ipmi_smic_flags);
		} while(!flags & SMIC_STATUS_TX_RDY);
		OUTB(sc, sc->ipmi_smic_ctl_sts, SMIC_CC_SMS_WR_END);
		ipmi_wait_for_tx_okay(dev);
		OUTB(sc, sc->ipmi_smic_data, *bytes);
		ipmi_set_busy(dev);
		ipmi_wait_for_not_busy(dev);
		status = INB(sc, sc->ipmi_smic_ctl_sts);
		if (status != SMIC_SC_SMS_WR_END) {
			device_printf(dev, "Write did not end %x\n",status);
			return -1;
		}
	} else if (sc->ipmi_bios_info.kcs_mode) {
		for (retry = 0; retry < 10; retry++) {
			/* Wait for IBF = 0 */
			status = ipmi_wait_for_ibf(dev, 0);

			/* Clear OBF */
			ipmi_clear_obf(dev, status);

			/* Write start to command */
			OUTB(sc, sc->ipmi_kcs_command_reg,
			     KCS_CONTROL_WRITE_START);

			/* Wait for IBF = 0 */
			status = ipmi_wait_for_ibf(dev, 0);
			if (KCS_STATUS_STATE(status) == KCS_STATUS_STATE_WRITE)
				break;
			DELAY(1000000);
		}

		for(len--; len; len--) {
			if (KCS_STATUS_STATE(status)
			    != KCS_STATUS_STATE_WRITE) {
				/* error state */
				device_printf(dev, "status error %x\n",status);
				ipmi_error(dev);
				sc->ipmi_busy = 0;
				return -1;
				break;
			} else {
				/* Clear OBF */
				ipmi_clear_obf(dev, status);

				/* Data to Data */
				OUTB(sc, sc->ipmi_kcs_data_out_reg, *bytes++);

				/* Wait for IBF = 0 */
				status = ipmi_wait_for_ibf(dev, 0);

				if (KCS_STATUS_STATE(status)
				    != KCS_STATUS_STATE_WRITE) {
					device_printf(dev, "status error %x\n"
					    ,status);
					ipmi_error(dev);
					return -1;
				} else {
					/* Clear OBF */
					ipmi_clear_obf(dev, status);
				}
			}
		}
		/* Write end to command */
		OUTB(sc, sc->ipmi_kcs_command_reg, KCS_CONTROL_WRITE_END);

		/* Wait for IBF = 0 */
		status = ipmi_wait_for_ibf(dev, 0);

		if (KCS_STATUS_STATE(status) != KCS_STATUS_STATE_WRITE) {
			/* error state */
			device_printf(dev, "status error %x\n",status);
			ipmi_error(dev);
			sc->ipmi_busy = 0;
			return -1;
		} else {
			/* Clear OBF */
			ipmi_clear_obf(dev, status);
			OUTB(sc, sc->ipmi_kcs_data_out_reg, *bytes++);
		}
	} else {
		device_printf(dev, "Unsupported mode\n");
	}
	sc->ipmi_busy = 2;
	return 1;
}

/*
 * Watchdog event handler.
 */

static void
ipmi_set_watchdog(device_t dev, int sec) {
	u_char *temp;
	int s;

	temp = malloc(IPMI_MAX_RX, M_IPMI, M_WAITOK);

	temp[0] = IPMI_APP_REQUEST << 2;
	if (sec) {
		temp[1] = IPMI_SET_WDOG;
		temp[2] = IPMI_SET_WD_TIMER_DONT_STOP
		    | IPMI_SET_WD_TIMER_SMS_OS;
		temp[3] = IPMI_SET_WD_ACTION_RESET;
		temp[4] = 0;
		temp[5] = 0;	/* Timer use */
		temp[6] = (sec * 10) & 0xff;
		temp[7] = (sec * 10) / 2550;
	} else {
 		temp[1] = IPMI_SET_WDOG;
		temp[2] = IPMI_SET_WD_TIMER_SMS_OS;
		temp[3] = 0;
		temp[4] = 0;
		temp[5] = 0;	/* Timer use */
		temp[6] = 0;
		temp[7] = 0;
	}

	s = splhigh();
	ipmi_write(dev, temp, 8);

	while (!ipmi_ready_to_read(dev))
		DELAY(1000);
	bzero(temp, IPMI_MAX_RX);
	ipmi_read(dev, temp, IPMI_MAX_RX);

	if (sec) {
		temp[0] = IPMI_APP_REQUEST << 2;
		temp[1] = IPMI_RESET_WDOG;

		ipmi_write(dev, temp, 2);

		while (!ipmi_ready_to_read(dev))
			DELAY(1000);
		bzero(temp, IPMI_MAX_RX);
		ipmi_read(dev, temp, IPMI_MAX_RX);
	}
	splx(s);

	free(temp, M_IPMI);
	/*
	dump_watchdog(dev);
	*/
}

static void
ipmi_wd_event(void *arg, unsigned int cmd, int *error)
{
	struct ipmi_softc *sc = arg;
	unsigned int timeout;

	/* disable / enable */
	if (!(cmd & WD_ACTIVE)) {
		ipmi_set_watchdog(sc->ipmi_dev, 0);
		*error = 0;
		return;
	}

	cmd &= WD_INTERVAL;
	/* convert from power-of-to-ns to WDT ticks */
	if (cmd >= 64) {
		*error = EINVAL;
		return;
	}
	timeout = ((uint64_t)1 << cmd) / 1800000000;

	/* reload */
	ipmi_set_watchdog(sc->ipmi_dev, timeout);

	*error = 0;
}

int
ipmi_attach(device_t dev)
{
	struct ipmi_softc *sc = device_get_softc(dev);
	u_char temp[1024];
	int i;
	int status;
	int unit;

	TAILQ_INIT(&sc->ipmi_done);
	sc->ipmi_address = IPMI_BMC_SLAVE_ADDR;
	sc->ipmi_lun = IPMI_BMC_SMS_LUN;
	temp[0] = IPMI_APP_REQUEST << 2;
	temp[1] = IPMI_GET_DEVICE_ID;
	ipmi_write(dev, temp, 2);

	while (!ipmi_ready_to_read(dev))
		DELAY(1000);
	bzero(temp, sizeof(temp));
	ipmi_read(dev, temp, sizeof(temp));
	device_printf(dev, "IPMI device rev. %d, firmware rev. %d.%d, "
	    "version %d.%d\n",
	     temp[4] & 0x0f,
	     temp[5] & 0x0f, temp[7],
	     temp[7] & 0x0f, temp[7] >> 4);

	temp[0] = IPMI_APP_REQUEST << 2;
	temp[1] = IPMI_CLEAR_FLAGS;
	temp[2] = 8;
	ipmi_write(dev, temp, 3);

	while (!ipmi_ready_to_read(dev))
		DELAY(1000);
	bzero(temp, sizeof(temp));
	ipmi_read(dev, temp, sizeof(temp));
	if (temp[2] == 0xc0) {
		device_printf(dev, "Clear flags is busy\n");
	}
	if (temp[2] == 0xc1) {
		device_printf(dev, "Clear flags illegal\n");
	}

	for(i = 0; i < 8; i++){
		temp[0] = IPMI_APP_REQUEST << 2;
		temp[1] = IPMI_GET_CHANNEL_INFO;
		temp[2] = i;
		ipmi_write(dev, temp, 3);
		while (!ipmi_ready_to_read(dev))
			DELAY(1000);
		bzero(temp, sizeof(temp));
		ipmi_read(dev, temp, sizeof(temp));
		if (temp[2]) {
			break;
		}
	}
	device_printf(dev, "Number of channels %d\n", i);

	/* probe for watchdog */
	bzero(temp, sizeof(temp));
        temp[0] = IPMI_APP_REQUEST << 2;
        temp[1] = IPMI_GET_WDOG;
        status = ipmi_write(dev, temp, 2);
        while (!ipmi_ready_to_read(dev))
                DELAY(1000);
        bzero(temp, sizeof(temp));
        ipmi_read(dev, temp, sizeof(temp));
        if (temp[0] == 0x1c && temp[2] == 0x00) {
		device_printf(dev, "Attached watchdog\n");
		/* register the watchdog event handler */
		sc->ipmi_ev_tag = EVENTHANDLER_REGISTER(watchdog_list,
						   ipmi_wd_event, sc, 0);
	}
	unit = device_get_unit(sc->ipmi_dev);
	/* force device to be ipmi0 since that is what ipmitool expects */
	sc->ipmi_dev_t = make_dev(&ipmi_cdevsw, unit, UID_ROOT, GID_OPERATOR,
			     0660, "ipmi%d", 0);
	sc->ipmi_dev_t->si_drv1 = sc;

	ipmi_attached = 1;

	return 0;
}

int
ipmi_detach(device_t dev)
{
	struct ipmi_softc *sc;

	sc = device_get_softc(dev);
	if (sc->ipmi_requests)
		untimeout((timeout_t *)ipmi_check_read, dev,
		    sc->ipmi_timeout_handle);
	destroy_dev(sc->ipmi_dev_t);
	return 0;
}

#ifdef DEBUG
static void
dump_buf(u_char *data, int len){
	char buf[20];
	char line[1024];
	char temp[30];
	int count = 0;
	int i=0;

	printf("Address %p len %d\n", data, len);
	if (len > 256)
		len = 256;
	line[0] = '\000';
	for (; len > 0; len--, data++) {
		sprintf(temp, "%02x ", *data);
		strcat(line, temp);
		if (*data >= ' ' && *data <= '~')
			buf[count] = *data;
		else if (*data >= 'A' && *data <= 'Z')
			buf[count] = *data;
		else
			buf[count] = '.';
		if (++count == 16) {
			buf[count] = '\000';
			count = 0;
			printf("  %3x  %s %s\n", i, line, buf);
			i+=16;
			line[0] = '\000';
		}
	}
	buf[count] = '\000';

	for (; count != 16; count++) {
		strcat(line, "   ");
	}
	printf("  %3x  %s %s\n", i, line, buf);
}
#endif
