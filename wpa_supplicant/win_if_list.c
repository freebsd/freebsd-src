/*
 * win_if_list - Display network interfaces with description (for Windows)
 * Copyright (c) 2004-2006, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 *
 * This small tool is for the Windows build to provide an easy way of fetching
 * a list of available network interfaces.
 */

#include "includes.h"
#include <stdio.h>
#ifdef CONFIG_USE_NDISUIO
#include <winsock2.h>
#include <ntddndis.h>
#else /* CONFIG_USE_NDISUIO */
#include "pcap.h"
#include <winsock.h>
#endif /* CONFIG_USE_NDISUIO */

#ifdef CONFIG_USE_NDISUIO

/* from nuiouser.h */
#define FSCTL_NDISUIO_BASE      FILE_DEVICE_NETWORK

#define _NDISUIO_CTL_CODE(_Function, _Method, _Access) \
	CTL_CODE(FSCTL_NDISUIO_BASE, _Function, _Method, _Access)

#define IOCTL_NDISUIO_QUERY_BINDING \
	_NDISUIO_CTL_CODE(0x203, METHOD_BUFFERED, \
			  FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_NDISUIO_BIND_WAIT \
	_NDISUIO_CTL_CODE(0x204, METHOD_BUFFERED, \
			  FILE_READ_ACCESS | FILE_WRITE_ACCESS)

typedef struct _NDISUIO_QUERY_BINDING
{
	ULONG BindingIndex;
	ULONG DeviceNameOffset;
	ULONG DeviceNameLength;
	ULONG DeviceDescrOffset;
	ULONG DeviceDescrLength;
} NDISUIO_QUERY_BINDING, *PNDISUIO_QUERY_BINDING;


static HANDLE ndisuio_open(void)
{
	DWORD written;
	HANDLE h;

	h = CreateFile(TEXT("\\\\.\\\\Ndisuio"),
		       GENERIC_READ | GENERIC_WRITE, 0, NULL,
		       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
		       INVALID_HANDLE_VALUE);
	if (h == INVALID_HANDLE_VALUE)
		return h;

#ifndef _WIN32_WCE
	if (!DeviceIoControl(h, IOCTL_NDISUIO_BIND_WAIT, NULL, 0, NULL, 0,
			     &written, NULL)) {
		printf("IOCTL_NDISUIO_BIND_WAIT failed: %d",
		       (int) GetLastError());
		CloseHandle(h);
		return INVALID_HANDLE_VALUE;
	}
#endif /* _WIN32_WCE */

	return h;
}


static void ndisuio_query_bindings(HANDLE ndisuio)
{
	NDISUIO_QUERY_BINDING *b;
	size_t blen = sizeof(*b) + 1024;
	int i, error;
	DWORD written;
	char name[256], desc[256];
	WCHAR *pos;
	size_t j, len;

	b = malloc(blen);
	if (b == NULL)
		return;

	for (i = 0; ; i++) {
		memset(b, 0, blen);
		b->BindingIndex = i;
		if (!DeviceIoControl(ndisuio, IOCTL_NDISUIO_QUERY_BINDING,
				     b, sizeof(NDISUIO_QUERY_BINDING), b,
				     (DWORD) blen, &written, NULL)) {
			error = (int) GetLastError();
			if (error == ERROR_NO_MORE_ITEMS)
				break;
			printf("IOCTL_NDISUIO_QUERY_BINDING failed: %d",
			       error);
			break;
		}

		pos = (WCHAR *) ((char *) b + b->DeviceNameOffset);
		len = b->DeviceNameLength;
		if (len >= sizeof(name))
			len = sizeof(name) - 1;
		for (j = 0; j < len; j++)
			name[j] = (char) pos[j];
		name[len] = '\0';

		pos = (WCHAR *) ((char *) b + b->DeviceDescrOffset);
		len = b->DeviceDescrLength;
		if (len >= sizeof(desc))
			len = sizeof(desc) - 1;
		for (j = 0; j < len; j++)
			desc[j] = (char) pos[j];
		desc[len] = '\0';

		printf("ifname: %s\ndescription: %s\n\n", name, desc);
	}

	free(b);
}


static void ndisuio_enum_bindings(void)
{
	HANDLE ndisuio = ndisuio_open();
	if (ndisuio == INVALID_HANDLE_VALUE)
		return;

	ndisuio_query_bindings(ndisuio);
	CloseHandle(ndisuio);
}

#else /* CONFIG_USE_NDISUIO */

static void show_dev(pcap_if_t *dev)
{
	printf("ifname: %s\ndescription: %s\n\n",
	       dev->name, dev->description);
}


static void pcap_enum_devs(void)
{
	pcap_if_t *devs, *dev;
	char err[PCAP_ERRBUF_SIZE + 1];

	if (pcap_findalldevs(&devs, err) < 0) {
		fprintf(stderr, "Error - pcap_findalldevs: %s\n", err);
		return;
	}

	for (dev = devs; dev; dev = dev->next) {
		show_dev(dev);
	}

	pcap_freealldevs(devs);
}

#endif /* CONFIG_USE_NDISUIO */


int main(int argc, char *argv[])
{
#ifdef CONFIG_USE_NDISUIO
	ndisuio_enum_bindings();
#else /* CONFIG_USE_NDISUIO */
	pcap_enum_devs();
#endif /* CONFIG_USE_NDISUIO */

	return 0;
}
