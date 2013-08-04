/*-
 * Copyright (c) 2007 David Crawshaw <david@zentus.com>
 * Copyright (c) 2008 David Gwynne <dlg@openbsd.org>
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
 *
 * $OpenBSD: src/sys/dev/vmt.c,v 1.13 2013/07/03 15:26:02 sf Exp $
 */

/*
 * Protocol reverse engineered by Ken Kato:
 * http://chitchat.at.infoseek.co.jp/vmware/backdoor.html
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/callout.h>
#include <sys/reboot.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/time.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/proc.h>

#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>

#include <machine/stdarg.h>

#include "vmtreg.h"

#define VMT_RPC_BUFLEN			256

struct vmt_softc {
	struct mtx		 sc_mtx;
	device_t		 sc_dev;
	struct vm_rpc		 sc_tclo_rpc;
	char			*sc_rpc_buf;
	int			 sc_rpc_error;
	int			 sc_tclo_ping;
	int			 sc_set_guest_os;
	int			 sc_removing;
	struct callout		 sc_tick;
	struct callout		 sc_tclo_tick;
	char			 sc_hostname[MAXHOSTNAMELEN];
};

#define VMT_LOCK(_sc)	mtx_lock(&(_sc)->sc_mtx)
#define VMT_UNLOCK(_sc)	mtx_unlock(&(_sc)->sc_mtx)

static void	vmt_identify(driver_t *, device_t);
static int	vmt_probe(device_t);
static int	vmt_attach(device_t);
static int	vmt_detach(device_t);
static int	vmt_shutdown(device_t);

static void	vm_cmd(struct vm_backdoor *);
static void	vm_ins(struct vm_backdoor *);
static void	vm_outs(struct vm_backdoor *);

/* Functions for communicating with the VM Host. */
static int	vm_rpc_open(struct vm_rpc *, uint32_t);
static int	vm_rpc_close(struct vm_rpc *);
static int	vm_rpc_send(const struct vm_rpc *, const uint8_t *, uint32_t);
static int	vm_rpc_send_str(const struct vm_rpc *, const uint8_t *);
static int	vm_rpc_get_length(const struct vm_rpc *, uint32_t *,
		    uint16_t *);
static int	vm_rpc_get_data(const struct vm_rpc *, char *, uint32_t,
		    uint16_t);
static int	vm_rpc_send_rpci_tx_buf(struct vmt_softc *, const uint8_t *,
		    uint32_t);
static int	vm_rpc_send_rpci_tx(struct vmt_softc *, const char *, ...)
		    __printflike(2 ,3);
static int	vm_rpci_response_successful(struct vmt_softc *);

static void	vmt_probe_cmd(struct vm_backdoor *, uint16_t);
static void	vmt_tclo_state_change_success(struct vmt_softc *, int, char);
static void	vmt_do_reboot(struct vmt_softc *);
static void	vmt_do_shutdown(struct vmt_softc *);

static void	vmt_disconnect(struct vmt_softc *);

static void	vmt_update_guest_info(struct vmt_softc *);
static void	vmt_update_guest_uptime(struct vmt_softc *);

static void	vmt_tick(void *);
static void	vmt_tclo_tick(void *);

extern char hostname[MAXHOSTNAMELEN];	/* prison0.pr_hostname */

static device_method_t vmt_methods[] = {
	DEVMETHOD(device_identify,	vmt_identify),
	DEVMETHOD(device_probe,		vmt_probe),
	DEVMETHOD(device_attach,	vmt_attach),
	DEVMETHOD(device_detach,	vmt_detach),
	DEVMETHOD(device_shutdown,	vmt_shutdown),

	DEVMETHOD_END
};

static driver_t vmt_driver = {
	"vmt", vmt_methods, sizeof(struct vmt_softc)
};

static devclass_t vmt_devclass;
DRIVER_MODULE(vmt, nexus, vmt_driver, vmt_devclass, 0, 0);

static void
vmt_probe_cmd(struct vm_backdoor *frame, uint16_t cmd)
{

	bzero(frame, sizeof(*frame));

	frame->eax.word = VM_MAGIC;
	frame->ebx.word = ~VM_MAGIC;
	frame->ecx.part.low = cmd;
	frame->ecx.part.high = 0xFFFF;
	frame->edx.part.low  = VM_PORT_CMD;
	frame->edx.part.high = 0;

	vm_cmd(frame);
}

static void
vmt_identify(driver_t *driver, device_t parent)
{
	struct vm_backdoor frame;

	if (vm_guest != VM_GUEST_VM)
		return;

	if (device_find_child(parent, driver->name, -1) != NULL)
		return;

	vmt_probe_cmd(&frame, VM_CMD_GET_VERSION);
	if (frame.eax.word == 0XFFFFFFFF || frame.ebx.word != VM_MAGIC)
		return;

	vmt_probe_cmd(&frame, VM_CMD_GET_SPEED);
	if (frame.eax.word == VM_MAGIC)
		return;

	if (BUS_ADD_CHILD(parent, 0, driver->name, 0) == NULL)
		device_printf(parent, "add vmt child failed\n");
}

static int
vmt_probe(device_t dev)
{

	device_set_desc(dev, "VMware Tools Device");
	return (0);
}

static int
vmt_attach(device_t dev)
{
	struct vmt_softc *sc;
	int error;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;

	mtx_init(&sc->sc_mtx, "vmt", NULL, MTX_DEF);

	sc->sc_rpc_buf = malloc(VMT_RPC_BUFLEN, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (sc->sc_rpc_buf == NULL) {
		error = ENOMEM;
		device_printf(dev, "unable to allocate buffer for RPC\n");
		goto fail;
	}

	error = vm_rpc_open(&sc->sc_tclo_rpc, VM_RPC_OPEN_TCLO);
	if (error) {
		device_printf(dev,
		    "failed to open backdoor RPC channel (TCLO protocol)\n");
		goto fail;
	}

	/* Don't know if this is important at all yet. */
	error = vm_rpc_send_rpci_tx(sc,
	    "tools.capability.hgfs_server toolbox 1");
	if (error) {
		device_printf(dev, "failed to set HGFS server capability\n");
		goto fail;
	}

	callout_init_mtx(&sc->sc_tick, &sc->sc_mtx, 0);
	callout_reset(&sc->sc_tick, hz, vmt_tick, sc);

	sc->sc_tclo_ping = 1;
	callout_init_mtx(&sc->sc_tclo_tick, &sc->sc_mtx, 0);
	callout_reset(&sc->sc_tclo_tick, hz, vmt_tclo_tick, sc);

fail:
	if (error)
		vmt_detach(dev);

	return (error);
}

static int
vmt_detach(device_t dev)
{
	struct vmt_softc *sc;

	sc = device_get_softc(dev);

	if (device_is_attached(dev)) {
		VMT_LOCK(sc);
		sc->sc_removing = 1;
		vmt_disconnect(sc);
		VMT_UNLOCK(sc);

		callout_drain(&sc->sc_tick);
		callout_drain(&sc->sc_tclo_tick);
	}

	if (sc->sc_rpc_buf != NULL) {
		free(sc->sc_rpc_buf, M_DEVBUF);
		sc->sc_rpc_buf = NULL;
	}

	mtx_destroy(&sc->sc_mtx);

	return (0);
}

static int
vmt_shutdown(device_t dev)
{

	return (0);
}

static void
vmt_update_guest_uptime(struct vmt_softc *sc)
{

	/* Host wants uptime in hundredths of a second. */
	if (vm_rpc_send_rpci_tx(sc, "SetGuestInfo  %d %lld00",
	    VM_GUEST_INFO_UPTIME, (long long)time_uptime) != 0) {
		device_printf(sc->sc_dev, "unable to set guest uptime\n");
		sc->sc_rpc_error = 1;
	}
}

static void
vmt_update_guest_info(struct vmt_softc *sc)
{
	device_t dev;
	int error;

	dev = sc->sc_dev;

	if (strncmp(sc->sc_hostname, hostname, sizeof(sc->sc_hostname)) != 0) {
		strlcpy(sc->sc_hostname, hostname, sizeof(sc->sc_hostname));

		error = vm_rpc_send_rpci_tx(sc, "SetGuestInfo  %d %s",
		    VM_GUEST_INFO_DNS_NAME, sc->sc_hostname);
		if (error) {
			device_printf(dev, "unable to set hostname\n");
			sc->sc_rpc_error = 1;
		}
	}

	/*
	 * We're supposed to pass the full network address information back
	 * here, but that involves xdr (sunrpc) data encoding, which seems
	 * a bit unreasonable.
	 */

	if (sc->sc_set_guest_os == 0) {
		/* See linux_misc.c for this ... */
		error = vm_rpc_send_rpci_tx(sc, "SetGuestInfo  %d %s %s %s",
		    VM_GUEST_INFO_OS_NAME_FULL, ostype, osrelease, version);
		if (error) {
			device_printf(dev, "unable to set full guest OS\n");
			sc->sc_rpc_error = 1;
		}

		error = vm_rpc_send_rpci_tx(sc, "SetGuestInfo  %d %s",
		    VM_GUEST_INFO_OS_NAME, "FreeBSD");
		if (error) {
			device_printf(dev, "unable to set guest OS\n");
			sc->sc_rpc_error = 1;
		}

		sc->sc_set_guest_os = 1;
	}
}

static void
vmt_tick(void *xsc)
{
	struct vmt_softc *sc;
	struct vm_backdoor frame;
	struct timeval guest;
	struct timeval host, diff __unused;

	sc = xsc;

	if (sc->sc_removing != 0)
		return;

	microtime(&guest);

	bzero(&frame, sizeof(frame));
	frame.eax.word = VM_MAGIC;
	frame.ecx.part.low = VM_CMD_GET_TIME_FULL;
	frame.edx.part.low = VM_PORT_CMD;
	vm_cmd(&frame);

	if (frame.eax.word != 0XFFFFFFFF) {
		host.tv_sec = ((uint64_t)frame.esi.word << 32) | frame.edx.word;
		host.tv_usec = frame.ebx.word;

#if 0
		timersub(&guest, &host, &diff);
		sc->sc_sensor.value = (u_int64_t)diff.tv_sec * 1000000000LL +
		    (u_int64_t)diff.tv_usec * 1000LL;
		sc->sc_sensor.status = SENSOR_S_OK;
#endif
	}
#if 0
	else
		sc->sc_sensor.status = SENSOR_S_UNKNOWN;
#endif

	vmt_update_guest_info(sc);
	vmt_update_guest_uptime(sc);

	callout_schedule(&sc->sc_tick, 15 * hz);
}

static void
vmt_tclo_state_change_success(struct vmt_softc *sc, int success, char state)
{

	if (vm_rpc_send_rpci_tx(sc, "tools.os.statechange.status %d %d",
	    success, state) != 0) {
		device_printf(sc->sc_dev,
		    "unable to send state change result\n");
		sc->sc_rpc_error = 1;
	}
}

static void
vmt_do_reboot(struct vmt_softc *sc)
{

	vmt_tclo_state_change_success(sc, 1, VM_STATE_CHANGE_REBOOT);
	vm_rpc_send_str(&sc->sc_tclo_rpc, VM_RPC_REPLY_OK);

	log(LOG_KERN | LOG_NOTICE,
	    "Rebooting in response to request from VMware host\n");
	shutdown_nice(0);
}

static void
vmt_do_shutdown(struct vmt_softc *sc)
{

	vmt_tclo_state_change_success(sc, 1, VM_STATE_CHANGE_HALT);
	vm_rpc_send_str(&sc->sc_tclo_rpc, VM_RPC_REPLY_OK);

	log(LOG_KERN | LOG_NOTICE,
	    "Shutting down in response to request from VMware host\n");
	shutdown_nice(RB_POWEROFF | RB_HALT);
}

static void
vmt_disconnect(struct vmt_softc *sc)
{
	device_t dev;
	int error;

	dev = sc->sc_dev;

	error = vm_rpc_send_rpci_tx(sc,
	    "tools.capability.hgfs_server toolbox 0");
	if (error)
		device_printf(dev, "failed to disable hgfs server capability\n");

	if (vm_rpc_send(&sc->sc_tclo_rpc, NULL, 0) != 0)
		device_printf(dev, "failed to send shutdown ping\n");

	vm_rpc_close(&sc->sc_tclo_rpc);
}

static void
vmt_tclo_tick(void *xsc)
{
	struct vmt_softc *sc;
	device_t dev;
	uint32_t rlen;
	uint16_t ack;
	int error;

	sc = xsc;
	dev = sc->sc_dev;

	if (sc->sc_removing != 0)
		return;

	/* Reopen tclo channel if it's currently closed. */
	if (sc->sc_tclo_rpc.channel == 0 && sc->sc_tclo_rpc.cookie1 == 0 &&
	    sc->sc_tclo_rpc.cookie2 == 0) {
		if (vm_rpc_open(&sc->sc_tclo_rpc, VM_RPC_OPEN_TCLO) != 0) {
			device_printf(dev, "unable to reopen TCLO channel\n");
			callout_schedule(&sc->sc_tclo_tick, 15 * hz);
			return;
		}

		if (vm_rpc_send_str(&sc->sc_tclo_rpc, VM_RPC_RESET_REPLY) != 0) {
			device_printf(dev, "failed to send reset reply\n");
			sc->sc_rpc_error = 1;
			goto out;
		} else
			sc->sc_rpc_error = 0;
	}

	if (sc->sc_tclo_ping) {
		if (vm_rpc_send(&sc->sc_tclo_rpc, NULL, 0) != 0) {
			device_printf(dev, "failed to send TCLO outgoing ping\n");
			sc->sc_rpc_error = 1;
			goto out;
		}
	}

	if (vm_rpc_get_length(&sc->sc_tclo_rpc, &rlen, &ack) != 0) {
		device_printf(dev,
		    "failed to get length of incoming TCLO data\n");
		sc->sc_rpc_error = 1;
		goto out;
	}

	if (rlen == 0) {
		sc->sc_tclo_ping = 1;
		goto out;
	} else if (rlen >= VMT_RPC_BUFLEN)
		rlen = VMT_RPC_BUFLEN - 1;

	if (vm_rpc_get_data(&sc->sc_tclo_rpc, sc->sc_rpc_buf, rlen, ack) != 0) {
		device_printf(dev, "failed to get incoming TCLO data\n");
		sc->sc_rpc_error = 1;
		goto out;
	}

	sc->sc_tclo_ping = 0;

	if (strcmp(sc->sc_rpc_buf, "reset") == 0) {
		if (sc->sc_rpc_error != 0) {
			device_printf(dev, "resetting rpc\n");
			vm_rpc_close(&sc->sc_tclo_rpc);
			/* Reopen and send the reset reply next time around. */
			goto out;
		}

		if (vm_rpc_send_str(&sc->sc_tclo_rpc, VM_RPC_RESET_REPLY) != 0) {
			device_printf(dev, "failed to send reset reply\n");
			sc->sc_rpc_error = 1;
		}
	} else if (strcmp(sc->sc_rpc_buf, "ping") == 0) {
		vmt_update_guest_info(sc);
		if (vm_rpc_send_str(&sc->sc_tclo_rpc, VM_RPC_REPLY_OK) != 0) {
			device_printf(dev, "error sending ping response\n");
			sc->sc_rpc_error = 1;
		}
	} else if (strcmp(sc->sc_rpc_buf, "OS_Halt") == 0) {
		vmt_do_shutdown(sc);
	} else if (strcmp(sc->sc_rpc_buf, "OS_Reboot") == 0) {
		vmt_do_reboot(sc);
	} else if (strcmp(sc->sc_rpc_buf, "OS_PowerOn") == 0) {
		vmt_tclo_state_change_success(sc, 1, VM_STATE_CHANGE_POWERON);
		if (vm_rpc_send_str(&sc->sc_tclo_rpc, VM_RPC_REPLY_OK) != 0) {
			device_printf(dev, "error sending poweron response\n");
			sc->sc_rpc_error = 1;
		}
	} else if (strcmp(sc->sc_rpc_buf, "OS_Suspend") == 0) {
		log(LOG_KERN | LOG_NOTICE,
		    "VMware guest entering suspended state\n");

		vmt_tclo_state_change_success(sc, 1, VM_STATE_CHANGE_SUSPEND);
		if (vm_rpc_send_str(&sc->sc_tclo_rpc, VM_RPC_REPLY_OK) != 0) {
			device_printf(dev, "error sending suspend response\n");
			sc->sc_rpc_error = 1;
		}
	} else if (strcmp(sc->sc_rpc_buf, "OS_Resume") == 0) {
		log(LOG_KERN | LOG_NOTICE,
		    "VMware guest resuming from suspended state\n");

		/* Force guest info update. */
		sc->sc_hostname[0] = '\0';
		sc->sc_set_guest_os = 0;
		vmt_update_guest_info(sc);

		vmt_tclo_state_change_success(sc, 1, VM_STATE_CHANGE_RESUME);
		if (vm_rpc_send_str(&sc->sc_tclo_rpc, VM_RPC_REPLY_OK) != 0) {
			device_printf(dev, "error sending resume response\n");
			sc->sc_rpc_error = 1;
		}
	} else if (strcmp(sc->sc_rpc_buf, "Capabilities_Register") == 0) {
		/* Don't know if this is important at all. */
		if (vm_rpc_send_rpci_tx(sc,
		    "vmx.capability.unified_loop toolbox") != 0) {
			device_printf(dev, "unable to set unified loop\n");
			sc->sc_rpc_error = 1;
		} else if (vm_rpci_response_successful(sc) == 0)
			device_printf(dev, "host rejected unified loop setting\n");

		/* The trailing space is apparently important here. */
		if (vm_rpc_send_rpci_tx(sc, "tools.capability.statechange ") != 0) {
			device_printf(dev,
			    "unable to send statechange capability\n");
			sc->sc_rpc_error = 1;
		} else if (vm_rpci_response_successful(sc) == 0)
			device_printf(dev, "host rejected statechange capability\n");

		if (vm_rpc_send_rpci_tx(sc, "tools.set.version %u",
		    VM_VERSION_UNMANAGED) != 0) {
			device_printf(dev, "unable to set tools version\n");
			sc->sc_rpc_error = 1;
		}

		vmt_update_guest_uptime(sc);

		if (vm_rpc_send_str(&sc->sc_tclo_rpc, VM_RPC_REPLY_OK) != 0) {
			device_printf(dev,
			    "error sending capabilities_register response\n");
			sc->sc_rpc_error = 1;
		}
	} else if (strcmp(sc->sc_rpc_buf, "Set_Option broadcastIP 1") == 0) {
#if 0
		struct ifnet *iface;
		struct sockaddr_in *guest_ip;

		/* Find first available ipv4 address. */
		guest_ip = NULL;
		TAILQ_FOREACH(iface, &ifnet, if_list) {
			struct ifaddr *iface_addr;

			/* skip loopback */
			if (strncmp(iface->if_xname, "lo", 2) == 0 &&
			    iface->if_xname[2] >= '0' && iface->if_xname[2] <= '9') {
				continue;
			}

			TAILQ_FOREACH(iface_addr, &iface->if_addrlist, ifa_list) {
				if (iface_addr->ifa_addr->sa_family != AF_INET) {
					continue;
				}

				guest_ip = satosin(iface_addr->ifa_addr);
				break;
			}
		}

		if (guest_ip != NULL) {
			if (vm_rpc_send_rpci_tx(sc, "info-set guestinfo.ip %s",
			    inet_ntoa(guest_ip->sin_addr)) != 0) {
				device_printf(dev,
				    "unable to send guest IP address\n");
				sc->sc_rpc_error = 1;
			}

			if (vm_rpc_send_str(&sc->sc_tclo_rpc,
			    VM_RPC_REPLY_OK) != 0) {
				device_printf(dev,
				    "error sending broadcastIP response\n");
				sc->sc_rpc_error = 1;
			}
		} else {
			if (vm_rpc_send_str(&sc->sc_tclo_rpc,
			    VM_RPC_REPLY_ERROR_IP_ADDR) != 0) {
				device_printf(dev,
				    "error sending broadcastIP error response\n");
				sc->sc_rpc_error = 1;
			}
		}
#endif
	} else {
		error = vm_rpc_send_str(&sc->sc_tclo_rpc, VM_RPC_REPLY_ERROR);
		if (error) {
			device_printf(dev,
			    "error sending unknown command reply\n");
			sc->sc_rpc_error = 1;
		}
	}

out:
	callout_schedule(&sc->sc_tclo_tick, hz);
}

#define BACKDOOR_OP_I386(op, frame)		\
	__asm__ __volatile__ (			\
		"pushal;"			\
		"pushl %%eax;"			\
		"movl 0x18(%%eax), %%ebp;"	\
		"movl 0x14(%%eax), %%edi;"	\
		"movl 0x10(%%eax), %%esi;"	\
		"movl 0x0c(%%eax), %%edx;"	\
		"movl 0x08(%%eax), %%ecx;"	\
		"movl 0x04(%%eax), %%ebx;"	\
		"movl 0x00(%%eax), %%eax;"	\
		op				\
		"xchgl %%eax, 0x00(%%esp);"	\
		"movl %%ebp, 0x18(%%eax);"	\
		"movl %%edi, 0x14(%%eax);"	\
		"movl %%esi, 0x10(%%eax);"	\
		"movl %%edx, 0x0c(%%eax);"	\
		"movl %%ecx, 0x08(%%eax);"	\
		"movl %%ebx, 0x04(%%eax);"	\
		"popl 0x00(%%eax);"		\
		"popal;"			\
		::"a"(frame)			\
	)

#define BACKDOOR_OP_AMD64(op, frame)		\
	__asm__ __volatile__ (			\
		"pushq %%rbp;			\n\t" \
		"pushq %%rax;			\n\t" \
		"movq 0x30(%%rax), %%rbp;	\n\t" \
		"movq 0x28(%%rax), %%rdi;	\n\t" \
		"movq 0x20(%%rax), %%rsi;	\n\t" \
		"movq 0x18(%%rax), %%rdx;	\n\t" \
		"movq 0x10(%%rax), %%rcx;	\n\t" \
		"movq 0x08(%%rax), %%rbx;	\n\t" \
		"movq 0x00(%%rax), %%rax;	\n\t" \
		op				"\n\t" \
		"xchgq %%rax, 0x00(%%rsp);	\n\t" \
		"movq %%rbp, 0x30(%%rax);	\n\t" \
		"movq %%rdi, 0x28(%%rax);	\n\t" \
		"movq %%rsi, 0x20(%%rax);	\n\t" \
		"movq %%rdx, 0x18(%%rax);	\n\t" \
		"movq %%rcx, 0x10(%%rax);	\n\t" \
		"movq %%rbx, 0x08(%%rax);	\n\t" \
		"popq 0x00(%%rax);		\n\t" \
		"popq %%rbp;			\n\t" \
		: /* No outputs. */ : "a" (frame) \
	/* No pushal on amd64 so warn gcc about the clobbered registers. */ \
		: "rbx", "rcx", "rdx", "rdi", "rsi", "cc", "memory" \
	)

#ifdef __i386__
#define BACKDOOR_OP(op, frame) BACKDOOR_OP_I386(op, frame)
#else
#define BACKDOOR_OP(op, frame) BACKDOOR_OP_AMD64(op, frame)
#endif

static void
vm_cmd(struct vm_backdoor *frame)
{

	BACKDOOR_OP("inl %%dx, %%eax;", frame);
}

static void
vm_ins(struct vm_backdoor *frame)
{

	BACKDOOR_OP("cld;\n\trep insb;", frame);
}

static void
vm_outs(struct vm_backdoor *frame)
{

	BACKDOOR_OP("cld;\n\trep outsb;", frame);
}

static int
vm_rpc_open(struct vm_rpc *rpc, uint32_t proto)
{
	struct vm_backdoor frame;

	bzero(&frame, sizeof(frame));
	frame.eax.word = VM_MAGIC;
	frame.ebx.word = proto | VM_RPC_FLAG_COOKIE;
	frame.ecx.part.low = VM_CMD_RPC;
	frame.ecx.part.high = VM_RPC_OPEN;
	frame.edx.part.low = VM_PORT_CMD;
	frame.edx.part.high = 0;

	vm_cmd(&frame);

	if (frame.ecx.part.high != 1 || frame.edx.part.low != 0) {
		/* open-vm-tools retries without VM_RPC_FLAG_COOKIE here.. */
		printf("vmt: open failed, eax=%08x, ecx=%08x, edx=%08x\n",
		    frame.eax.word, frame.ecx.word, frame.edx.word);
		return (EIO);
	}

	rpc->channel = frame.edx.part.high;
	rpc->cookie1 = frame.esi.word;
	rpc->cookie2 = frame.edi.word;

	return (0);
}

static int
vm_rpc_close(struct vm_rpc *rpc)
{
	struct vm_backdoor frame;

	bzero(&frame, sizeof(frame));
	frame.eax.word = VM_MAGIC;
	frame.ebx.word = 0;
	frame.ecx.part.low = VM_CMD_RPC;
	frame.ecx.part.high = VM_RPC_CLOSE;
	frame.edx.part.low = VM_PORT_CMD;
	frame.edx.part.high = rpc->channel;
	frame.edi.word = rpc->cookie2;
	frame.esi.word = rpc->cookie1;

	vm_cmd(&frame);

	if (frame.ecx.part.high == 0 || frame.ecx.part.low != 0) {
		printf("vmt: close failed, eax=%08x, ecx=%08x\n",
		    frame.eax.word, frame.ecx.word);
		return (EIO);
	}

	rpc->channel = 0;
	rpc->cookie1 = 0;
	rpc->cookie2 = 0;

	return (0);
}

static int
vm_rpc_send(const struct vm_rpc *rpc, const uint8_t *buf, uint32_t length)
{
	struct vm_backdoor frame;

	/* Send the length of the command. */
	bzero(&frame, sizeof(frame));
	frame.eax.word = VM_MAGIC;
	frame.ebx.word = length;
	frame.ecx.part.low = VM_CMD_RPC;
	frame.ecx.part.high = VM_RPC_SET_LENGTH;
	frame.edx.part.low = VM_PORT_CMD;
	frame.edx.part.high = rpc->channel;
	frame.esi.word = rpc->cookie1;
	frame.edi.word = rpc->cookie2;

	vm_cmd(&frame);

	if ((frame.ecx.part.high & VM_RPC_REPLY_SUCCESS) == 0) {
		printf("vmt: sending length failed, eax=%08x, ecx=%08x\n",
		    frame.eax.word, frame.ecx.word);
		return (EIO);
	}

	/* Only need to poke once if command is null. */
	if (length == 0)
		return (0);

	/* Send the command using enhanced RPC. */
	bzero(&frame, sizeof(frame));
	frame.eax.word = VM_MAGIC;
	frame.ebx.word = VM_RPC_ENH_DATA;
	frame.ecx.word = length;
	frame.edx.part.low = VM_PORT_RPC;
	frame.edx.part.high = rpc->channel;
	frame.ebp.word = rpc->cookie1;
	frame.edi.word = rpc->cookie2;
#ifdef __amd64__
	frame.esi.quad = (uint64_t)buf;
#else
	frame.esi.word = (uint32_t)buf;
#endif

	vm_outs(&frame);

	if (frame.ebx.word != VM_RPC_ENH_DATA) {
		/* open-vm-tools retries on VM_RPC_REPLY_CHECKPOINT */
		printf("vmt: send failed, ebx=%08x\n", frame.ebx.word);
		return (EIO);
	}

	return (0);
}

static int
vm_rpc_send_str(const struct vm_rpc *rpc, const uint8_t *str)
{

	return (vm_rpc_send(rpc, str, strlen(str)));
}

static int
vm_rpc_get_data(const struct vm_rpc *rpc, char *data, uint32_t length,
    uint16_t dataid)
{
	struct vm_backdoor frame;

	/* Get data using enhanced RPC. */
	bzero(&frame, sizeof(frame));
	frame.eax.word = VM_MAGIC;
	frame.ebx.word = VM_RPC_ENH_DATA;
	frame.ecx.word = length;
	frame.edx.part.low = VM_PORT_RPC;
	frame.edx.part.high = rpc->channel;
	frame.esi.word = rpc->cookie1;
#ifdef __amd64__
	frame.edi.quad = (uint64_t)data;
#else
	frame.edi.word = (uint32_t)data;
#endif
	frame.ebp.word = rpc->cookie2;

	vm_ins(&frame);

	/* NUL-terminate the data. */
	data[length] = '\0';

	if (frame.ebx.word != VM_RPC_ENH_DATA) {
		printf("vmt: get data failed, ebx=%08x\n", frame.ebx.word);
		return (EIO);
	}

	/* Acknowledge data received. */
	bzero(&frame, sizeof(frame));
	frame.eax.word = VM_MAGIC;
	frame.ebx.word = dataid;
	frame.ecx.part.low = VM_CMD_RPC;
	frame.ecx.part.high = VM_RPC_GET_END;
	frame.edx.part.low = VM_PORT_CMD;
	frame.edx.part.high = rpc->channel;
	frame.esi.word = rpc->cookie1;
	frame.edi.word = rpc->cookie2;

	vm_cmd(&frame);

	if (frame.ecx.part.high == 0) {
		printf("vmt: ack data failed, eax=%08x, ecx=%08x\n",
		    frame.eax.word, frame.ecx.word);
		return (EIO);
	}

	return (0);
}

static int
vm_rpc_get_length(const struct vm_rpc *rpc, uint32_t *length, uint16_t *dataid)
{
	struct vm_backdoor frame;

	bzero(&frame, sizeof(frame));
	frame.eax.word = VM_MAGIC;
	frame.ebx.word = 0;
	frame.ecx.part.low = VM_CMD_RPC;
	frame.ecx.part.high = VM_RPC_GET_LENGTH;
	frame.edx.part.low = VM_PORT_CMD;
	frame.edx.part.high = rpc->channel;
	frame.esi.word = rpc->cookie1;
	frame.edi.word = rpc->cookie2;

	vm_cmd(&frame);

	if ((frame.ecx.part.high & VM_RPC_REPLY_SUCCESS) == 0) {
		printf("vmt: get length failed, eax=%08x, ecx=%08x\n",
		    frame.eax.word, frame.ecx.word);
		return (EIO);
	}

	if ((frame.ecx.part.high & VM_RPC_REPLY_DORECV) == 0) {
		*length = 0;
		*dataid = 0;
	} else {
		*length = frame.ebx.word;
		*dataid = frame.edx.part.high;
	}

	return (0);
}

static int
vm_rpci_response_successful(struct vmt_softc *sc)
{

	return (sc->sc_rpc_buf[0] == '1' && sc->sc_rpc_buf[1] == ' ');
}

static int
vm_rpc_send_rpci_tx_buf(struct vmt_softc *sc, const uint8_t *buf,
    uint32_t length)
{
	device_t dev;
	struct vm_rpc rpci;
	uint32_t rlen;
	uint16_t ack;
	int error;

	dev = sc->sc_dev;
	error = 0;

	if (vm_rpc_open(&rpci, VM_RPC_OPEN_RPCI) != 0) {
		device_printf(dev, "rpci channel open failed\n");
		return (EIO);
	}

	if (vm_rpc_send(&rpci, sc->sc_rpc_buf, length) != 0) {
		device_printf(dev, "unable to send rpci command\n");
		error = EIO;
		goto out;
	}

	if (vm_rpc_get_length(&rpci, &rlen, &ack) != 0) {
		device_printf(dev,
		    "failed to get length of rpci response data\n");
		error = EIO;
		goto out;
	}

	if (rlen > 0) {
		if (rlen >= VMT_RPC_BUFLEN)
			rlen = VMT_RPC_BUFLEN - 1;

		if (vm_rpc_get_data(&rpci, sc->sc_rpc_buf, rlen, ack) != 0) {
			device_printf(dev,
			    "failed to get rpci response data\n");
			error = EIO;
			goto out;
		}
	}

out:
	if (vm_rpc_close(&rpci) != 0)
		device_printf(dev, "unable to close rpci channel\n");

	return (error);
}

static int
vm_rpc_send_rpci_tx(struct vmt_softc *sc, const char *fmt, ...)
{
	va_list args;
	int len, error;

	va_start(args, fmt);
	len = vsnprintf(sc->sc_rpc_buf, VMT_RPC_BUFLEN, fmt, args);
	va_end(args);

	if (len >= VMT_RPC_BUFLEN) {
		device_printf(sc->sc_dev,
		    "rpci command didn't fit in buffer\n");
		error = EIO;
	} else
		error = vm_rpc_send_rpci_tx_buf(sc, sc->sc_rpc_buf, len);

	return (error);
}

#if 0
	struct vm_backdoor frame;

	bzero(&frame, sizeof(frame));

	frame.eax.word = VM_MAGIC;
	frame.ecx.part.low = VM_CMD_GET_VERSION;
	frame.edx.part.low  = VM_PORT_CMD;

	printf("\n");
	printf("eax 0x%08x\n", frame.eax.word);
	printf("ebx 0x%08x\n", frame.ebx.word);
	printf("ecx 0x%08x\n", frame.ecx.word);
	printf("edx 0x%08x\n", frame.edx.word);
	printf("ebp 0x%08x\n", frame.ebp.word);
	printf("edi 0x%08x\n", frame.edi.word);
	printf("esi 0x%08x\n", frame.esi.word);

	vm_cmd(&frame);

	printf("-\n");
	printf("eax 0x%08x\n", frame.eax.word);
	printf("ebx 0x%08x\n", frame.ebx.word);
	printf("ecx 0x%08x\n", frame.ecx.word);
	printf("edx 0x%08x\n", frame.edx.word);
	printf("ebp 0x%08x\n", frame.ebp.word);
	printf("edi 0x%08x\n", frame.edi.word);
	printf("esi 0x%08x\n", frame.esi.word);
#endif

/*
 * Notes on tracing backdoor activity in vmware-guestd:
 *
 * - Find the addresses of the inl / rep insb / rep outsb
 *   instructions used to perform backdoor operations.
 *   One way to do this is to disassemble vmware-guestd:
 *
 *   $ objdump -S /emul/freebsd/sbin/vmware-guestd > vmware-guestd.S
 *
 *   and search for '<tab>in ' in the resulting file.  The rep insb and
 *   rep outsb code is directly below that.
 *
 * - Run vmware-guestd under gdb, setting up breakpoints as follows:
 *   (the addresses shown here are the ones from VMware-server-1.0.10-203137,
 *   the last version that actually works in FreeBSD emulation on OpenBSD)
 *
 * break *0x805497b   (address of 'in' instruction)
 * commands 1
 * silent
 * echo INOUT\n
 * print/x $ecx
 * print/x $ebx
 * print/x $edx
 * continue
 * end
 * break *0x805497c   (address of instruction after 'in')
 * commands 2
 * silent
 * echo ===\n
 * print/x $ecx
 * print/x $ebx
 * print/x $edx
 * echo \n
 * continue
 * end
 * break *0x80549b7   (address of instruction before 'rep insb')
 * commands 3
 * silent
 * set variable $inaddr = $edi
 * set variable $incount = $ecx
 * continue
 * end
 * break *0x80549ba   (address of instruction after 'rep insb')
 * commands 4
 * silent
 * echo IN\n
 * print $incount
 * x/s $inaddr
 * echo \n
 * continue
 * end
 * break *0x80549fb    (address of instruction before 'rep outsb')
 * commands 5
 * silent
 * echo OUT\n
 * print $ecx
 * x/s $esi
 * echo \n
 * continue
 * end
 *
 * This will produce a log of the backdoor operations, including the
 * data sent and received and the relevant register values.  You can then
 * match the register values to the various constants in this file.
 */
