/*-
 * Copyright (c) 1998 Michael Smith (msmith@freebsd.org)
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

#include <stand.h>
#include <sys/errno.h>
#include <bootstrap.h>
#include <stdbool.h>

#include <efi.h>
#include <efilib.h>

static EFI_GUID serial = SERIAL_IO_PROTOCOL;

#define	COMC_TXWAIT	0x40000		/* transmit timeout */

#define	PNP0501		0x501		/* 16550A-compatible COM port */

struct serial {
	uint64_t	newbaudrate;
	uint64_t	baudrate;
	uint32_t	timeout;
	uint32_t	receivefifodepth;
	uint32_t	databits;
	EFI_PARITY_TYPE	parity;
	EFI_STOP_BITS_TYPE stopbits;
	int		ioaddr;		/* index in handles array */
	EFI_HANDLE	currdev;	/* current serial device */
	EFI_HANDLE	condev;		/* EFI Console device */
	SERIAL_IO_INTERFACE *sio;
};

static void	comc_probe(struct console *);
static int	comc_init(int);
static void	comc_putchar(int);
static int	comc_getchar(void);
static int	comc_ischar(void);
static bool	comc_setup(void);
static int	comc_parse_intval(const char *, unsigned *);
static int	comc_port_set(struct env_var *, int, const void *);
static int	comc_speed_set(struct env_var *, int, const void *);

static struct serial	*comc_port;
extern struct console efi_console;

struct console eficom = {
	.c_name = "eficom",
	.c_desc = "serial port",
	.c_flags = 0,
	.c_probe = comc_probe,
	.c_init = comc_init,
	.c_out = comc_putchar,
	.c_in = comc_getchar,
	.c_ready = comc_ischar,
};

#if defined(__aarch64__) && __FreeBSD_version < 1500000
static void	comc_probe_compat(struct console *);
struct console comconsole = {
	.c_name = "comconsole",
	.c_desc = "serial port",
	.c_flags = 0,
	.c_probe = comc_probe_compat,
	.c_init = comc_init,
	.c_out = comc_putchar,
	.c_in = comc_getchar,
	.c_ready = comc_ischar,
};
#endif

static EFI_STATUS
efi_serial_init(EFI_HANDLE **handlep, int *nhandles)
{
	UINTN bufsz = 0;
	EFI_STATUS status;
	EFI_HANDLE *handles;

	/*
	 * get buffer size
	 */
	*nhandles = 0;
	handles = NULL;
	status = BS->LocateHandle(ByProtocol, &serial, NULL, &bufsz, handles);
	if (status != EFI_BUFFER_TOO_SMALL)
		return (status);

	if ((handles = malloc(bufsz)) == NULL)
		return (ENOMEM);

	*nhandles = (int)(bufsz / sizeof (EFI_HANDLE));
	/*
	 * get handle array
	 */
	status = BS->LocateHandle(ByProtocol, &serial, NULL, &bufsz, handles);
	if (EFI_ERROR(status)) {
		free(handles);
		*nhandles = 0;
	} else
		*handlep = handles;
	return (status);
}

/*
 * Find serial device number from device path.
 * Return -1 if not found.
 */
static int
efi_serial_get_index(EFI_DEVICE_PATH *devpath, int idx)
{
	ACPI_HID_DEVICE_PATH  *acpi;
	CHAR16 *text;

	while (!IsDevicePathEnd(devpath)) {
		if (DevicePathType(devpath) == MESSAGING_DEVICE_PATH &&
		    DevicePathSubType(devpath) == MSG_UART_DP)
			return (idx);

		if (DevicePathType(devpath) == ACPI_DEVICE_PATH &&
		    (DevicePathSubType(devpath) == ACPI_DP ||
		    DevicePathSubType(devpath) == ACPI_EXTENDED_DP)) {

			acpi = (ACPI_HID_DEVICE_PATH *)devpath;
			if (acpi->HID == EISA_PNP_ID(PNP0501)) {
				return (acpi->UID);
			}
		}

		devpath = NextDevicePathNode(devpath);
	}
	return (-1);
}

/*
 * The order of handles from LocateHandle() is not known, we need to
 * iterate handles, pick device path for handle, and check the device
 * number.
 */
static EFI_HANDLE
efi_serial_get_handle(int port, EFI_HANDLE condev)
{
	EFI_STATUS status;
	EFI_HANDLE *handles, handle;
	EFI_DEVICE_PATH *devpath;
	int index, nhandles;

	if (port == -1)
		return (NULL);

	handles = NULL;
	nhandles = 0;
	status = efi_serial_init(&handles, &nhandles);
	if (EFI_ERROR(status))
		return (NULL);

	/*
	 * We have console handle, set ioaddr for it.
	 */
	if (condev != NULL) {
		for (index = 0; index < nhandles; index++) {
			if (condev == handles[index]) {
				devpath = efi_lookup_devpath(condev);
				comc_port->ioaddr =
				    efi_serial_get_index(devpath, index);
				efi_close_devpath(condev);
				free(handles);
				return (condev);
			}
		}
	}

	handle = NULL;
	for (index = 0; handle == NULL && index < nhandles; index++) {
		devpath = efi_lookup_devpath(handles[index]);
		if (port == efi_serial_get_index(devpath, index))
			handle = (handles[index]);
		efi_close_devpath(handles[index]);
	}

	/*
	 * In case we did fail to identify the device by path, use port as
	 * array index. Note, we did check port == -1 above.
	 */
	if (port < nhandles && handle == NULL)
		handle = handles[port];

	free(handles);
	return (handle);
}

static EFI_HANDLE
comc_get_con_serial_handle(const char *name)
{
	EFI_HANDLE handle;
	EFI_DEVICE_PATH *node;
	EFI_STATUS status;
	char *buf, *ep;
	size_t sz;

	buf = NULL;
	sz = 0;
	status = efi_global_getenv(name, buf, &sz);
	if (status == EFI_BUFFER_TOO_SMALL) {
		buf = malloc(sz);
		if (buf == NULL)
			return (NULL);
		status = efi_global_getenv(name, buf, &sz);
	}
	if (status != EFI_SUCCESS) {
		free(buf);
		return (NULL);
	}

	ep = buf + sz;
	node = (EFI_DEVICE_PATH *)buf;
	while ((char *)node < ep) {
		status = BS->LocateDevicePath(&serial, &node, &handle);
		if (status == EFI_SUCCESS) {
			free(buf);
			return (handle);
		}

		/* Sanity check the node before moving to the next node. */
		if (DevicePathNodeLength(node) < sizeof(*node))
			break;

		/* Start of next device path in list. */
		node = NextDevicePathNode(node);
	}
	free(buf);
	return (NULL);
}

static void
comc_probe(struct console *sc)
{
	EFI_STATUS status;
	EFI_HANDLE handle;
	char name[20];
	char value[20];
	unsigned val;
	char *env, *buf, *ep;
	size_t sz;

	if (comc_port == NULL) {
		comc_port = calloc(1, sizeof (struct serial));
		if (comc_port == NULL)
			return;
	}

	/* Use defaults from firmware */
	comc_port->databits = 8;
	comc_port->parity = DefaultParity;
	comc_port->stopbits = DefaultStopBits;

	handle = NULL;
	env = getenv("efi_com_port");
	if (comc_parse_intval(env, &val) == CMD_OK) {
		comc_port->ioaddr = val;
	} else {
		/*
		 * efi_com_port is not set, we need to select default.
		 * First, we consult ConOut variable to see if
		 * we have serial port redirection. If not, we just
		 * pick first device.
		 */
		handle = comc_get_con_serial_handle("ConOut");
		comc_port->condev = handle;
	}

	handle = efi_serial_get_handle(comc_port->ioaddr, handle);
	if (handle != NULL) {
		comc_port->currdev = handle;
		status = BS->OpenProtocol(handle, &serial,
		    (void**)&comc_port->sio, IH, NULL,
		    EFI_OPEN_PROTOCOL_GET_PROTOCOL);

		if (EFI_ERROR(status)) {
			comc_port->sio = NULL;
		} else {
			comc_port->newbaudrate =
			    comc_port->baudrate = comc_port->sio->Mode->BaudRate;
			comc_port->timeout = comc_port->sio->Mode->Timeout;
			comc_port->receivefifodepth =
			    comc_port->sio->Mode->ReceiveFifoDepth;
			comc_port->databits = comc_port->sio->Mode->DataBits;
			comc_port->parity = comc_port->sio->Mode->Parity;
			comc_port->stopbits = comc_port->sio->Mode->StopBits;
		}
	}

	if (env != NULL) 
		unsetenv("efi_com_port");
	snprintf(value, sizeof (value), "%u", comc_port->ioaddr);
	env_setenv("efi_com_port", EV_VOLATILE, value,
	    comc_port_set, env_nounset);

	env = getenv("efi_com_speed");
	if (env == NULL)
		/* fallback to comconsole setting */
		env = getenv("comconsole_speed");

	if (comc_parse_intval(env, &val) == CMD_OK)
		comc_port->newbaudrate = val;

	if (env != NULL)
		unsetenv("efi_com_speed");
	snprintf(value, sizeof (value), "%ju", (uintmax_t)comc_port->baudrate);
	env_setenv("efi_com_speed", EV_VOLATILE, value,
	    comc_speed_set, env_nounset);

	eficom.c_flags = 0;
	if (comc_setup()) {
		sc->c_flags = C_PRESENTIN | C_PRESENTOUT;
	}
}

#if defined(__aarch64__) && __FreeBSD_version < 1500000
static void
comc_probe_compat(struct console *sc)
{
	comc_probe(sc);
	if (sc->c_flags & (C_PRESENTIN | C_PRESENTOUT)) {
		printf("comconsole: comconsole device name is deprecated, switch to eficom\n");
	}
}
#endif

static int
comc_init(int arg __unused)
{

	if (comc_setup())
		return (CMD_OK);

	eficom.c_flags = 0;
	return (CMD_ERROR);
}

static void
comc_putchar(int c)
{
	int wait;
	EFI_STATUS status;
	UINTN bufsz = 1;
	char cb = c;

	if (comc_port->sio == NULL)
		return;

	for (wait = COMC_TXWAIT; wait > 0; wait--) {
		status = comc_port->sio->Write(comc_port->sio, &bufsz, &cb);
		if (status != EFI_TIMEOUT)
			break;
	}
}

static int
comc_getchar(void)
{
	EFI_STATUS status;
	UINTN bufsz = 1;
	char c;


	/*
	 * if this device is also used as ConIn, some firmwares
	 * fail to return all input via SIO protocol.
	 */
	if (comc_port->currdev == comc_port->condev) {
		if ((efi_console.c_flags & C_ACTIVEIN) == 0)
			return (efi_console.c_in());
		return (-1);
	}

	if (comc_port->sio == NULL)
		return (-1);

	status = comc_port->sio->Read(comc_port->sio, &bufsz, &c);
	if (EFI_ERROR(status) || bufsz == 0)
		return (-1);

	return (c);
}

static int
comc_ischar(void)
{
	EFI_STATUS status;
	uint32_t control;

	/*
	 * if this device is also used as ConIn, some firmwares
	 * fail to return all input via SIO protocol.
	 */
	if (comc_port->currdev == comc_port->condev) {
		if ((efi_console.c_flags & C_ACTIVEIN) == 0)
			return (efi_console.c_ready());
		return (0);
	}

	if (comc_port->sio == NULL)
		return (0);

	status = comc_port->sio->GetControl(comc_port->sio, &control);
	if (EFI_ERROR(status))
		return (0);

	return (!(control & EFI_SERIAL_INPUT_BUFFER_EMPTY));
}

static int
comc_parse_intval(const char *value, unsigned *valp)
{
	unsigned n;
	char *ep;

	if (value == NULL || *value == '\0')
		return (CMD_ERROR);

	errno = 0;
	n = strtoul(value, &ep, 10);
	if (errno != 0 || *ep != '\0')
		return (CMD_ERROR);
	*valp = n;

	return (CMD_OK);
}

static int
comc_port_set(struct env_var *ev, int flags, const void *value)
{
	unsigned port;
	SERIAL_IO_INTERFACE *sio;
	EFI_HANDLE handle;
	EFI_STATUS status;

	if (value == NULL)
		return (CMD_ERROR);

	if (comc_parse_intval(value, &port) != CMD_OK) 
		return (CMD_ERROR);

	handle = efi_serial_get_handle(port, NULL);
	if (handle == NULL) {
		printf("no handle\n");
		return (CMD_ERROR);
	}

	status = BS->OpenProtocol(handle, &serial,
	    (void**)&sio, IH, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);

	if (EFI_ERROR(status)) {
		printf("OpenProtocol: %lu\n", EFI_ERROR_CODE(status));
		return (CMD_ERROR);
	}

	comc_port->currdev = handle;
	comc_port->ioaddr = port;
	comc_port->sio = sio;
	
	(void) comc_setup();

	env_setenv(ev->ev_name, flags | EV_NOHOOK, value, NULL, NULL);
	return (CMD_OK);
}

static int
comc_speed_set(struct env_var *ev, int flags, const void *value)
{
	unsigned speed;

	if (value == NULL)
		return (CMD_ERROR);

	if (comc_parse_intval(value, &speed) != CMD_OK) 
		return (CMD_ERROR);

	comc_port->newbaudrate = speed;
	if (comc_setup())
		env_setenv(ev->ev_name, flags | EV_NOHOOK, value, NULL, NULL);

	return (CMD_OK);
}

/*
 * In case of error, we also reset ACTIVE flags, so the console
 * framefork will try alternate consoles.
 */
static bool
comc_setup(void)
{
	EFI_STATUS status;
	char *ev;

	/* port is not usable */
	if (comc_port->sio == NULL)
		return (false);

	if (comc_port->sio->Reset != NULL) {
		status = comc_port->sio->Reset(comc_port->sio);
		if (EFI_ERROR(status))
			return (false);
	}

	/*
	 * Avoid setting the baud rate on Hyper-V. Also, only set the baud rate
	 * if the baud rate has changed from the default. And pass in '0' or
	 * DefaultFoo when we're not changing those values. Some EFI
	 * implementations get cranky when you set things to the values reported
	 * back even when they are unchanged.
	 */
	if (comc_port->sio->SetAttributes != NULL &&
	    comc_port->newbaudrate != comc_port->baudrate) {
		ev = getenv("smbios.bios.version");
		if (ev != NULL && strncmp(ev, "Hyper-V", 7) != 0) {
			status = comc_port->sio->SetAttributes(comc_port->sio,
			    comc_port->newbaudrate, 0, 0, DefaultParity, 0,
			    DefaultStopBits);
			if (EFI_ERROR(status))
				return (false);
			comc_port->baudrate = comc_port->newbaudrate;
		}
	}

#ifdef EFI_FORCE_RTS
	if (comc_port->sio->GetControl != NULL && comc_port->sio->SetControl != NULL) {
		UINT32 control;

		status = comc_port->sio->GetControl(comc_port->sio, &control);
		if (EFI_ERROR(status))
			return (false);
		control |= EFI_SERIAL_REQUEST_TO_SEND;
		(void) comc_port->sio->SetControl(comc_port->sio, control);
	}
#endif
	/* Mark this port usable. */
	eficom.c_flags |= (C_PRESENTIN | C_PRESENTOUT);
	return (true);
}
