/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Intel Corporation
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/types.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>

#include <stand.h>
#include <bootstrap.h>
#include <net.h>

#include <efi.h>
#include <efilib.h>
#include <efiprot.h>
#include <Protocol/Http.h>
#include <Protocol/Ip4Config2.h>
#include <Protocol/ServiceBinding.h>

/* Poll timeout in milliseconds */
static const int EFIHTTP_POLL_TIMEOUT = 300000;

static EFI_GUID http_guid = EFI_HTTP_PROTOCOL_GUID;
static EFI_GUID httpsb_guid = EFI_HTTP_SERVICE_BINDING_PROTOCOL_GUID;
static EFI_GUID ip4config2_guid = EFI_IP4_CONFIG2_PROTOCOL_GUID;

static bool efihttp_init_done = false;

static int efihttp_dev_init(void);
static int efihttp_dev_strategy(void *devdata, int rw, daddr_t blk, size_t size,
    char *buf, size_t *rsize);
static int efihttp_dev_open(struct open_file *f, ...);
static int efihttp_dev_close(struct open_file *f);

static int efihttp_fs_open(const char *path, struct open_file *f);
static int efihttp_fs_close(struct open_file *f);
static int efihttp_fs_read(struct open_file *f, void *buf, size_t size,
    size_t *resid);
static int efihttp_fs_write(struct open_file *f, const void *buf, size_t size,
    size_t *resid);
static off_t efihttp_fs_seek(struct open_file *f, off_t offset, int where);
static int efihttp_fs_stat(struct open_file *f, struct stat *sb);
static int efihttp_fs_readdir(struct open_file *f, struct dirent *d);

struct open_efihttp {
	EFI_HTTP_PROTOCOL *http;
	EFI_HANDLE	http_handle;
	EFI_HANDLE	dev_handle;
	char		*uri_base;
};

struct file_efihttp {
	ssize_t		size;
	off_t		offset;
	char		*path;
	bool		is_dir;
};

struct devsw efihttp_dev = {
	.dv_name =	"http",
	.dv_type =	DEVT_NET,
	.dv_init =	efihttp_dev_init,
	.dv_strategy =	efihttp_dev_strategy,
	.dv_open =	efihttp_dev_open,
	.dv_close =	efihttp_dev_close,
	.dv_ioctl =	noioctl,
	.dv_print =	NULL,
	.dv_cleanup =	nullsys,
};

struct fs_ops efihttp_fsops = {
	.fs_name =	"efihttp",
	.fo_open =	efihttp_fs_open,
	.fo_close =	efihttp_fs_close,
	.fo_read =	efihttp_fs_read,
	.fo_write =	efihttp_fs_write,
	.fo_seek =	efihttp_fs_seek,
	.fo_stat =	efihttp_fs_stat,
	.fo_readdir =	efihttp_fs_readdir,
};

static void EFIAPI
notify(EFI_EVENT event __unused, void *context)
{
	bool *b;

	b = (bool *)context;
	*b = true;
}

static int
setup_ipv4_config2(EFI_HANDLE handle, MAC_ADDR_DEVICE_PATH *mac,
    IPv4_DEVICE_PATH *ipv4, DNS_DEVICE_PATH *dns)
{
	EFI_IP4_CONFIG2_PROTOCOL *ip4config2;
	EFI_STATUS status;

	status = BS->OpenProtocol(handle, &ip4config2_guid,
	    (void **)&ip4config2, IH, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
	if (EFI_ERROR(status))
		return (efi_status_to_errno(status));
	if (ipv4 != NULL) {
		if (mac != NULL) {
			setenv("boot.netif.hwaddr",
			    ether_sprintf((u_char *)mac->MacAddress.Addr), 1);
		}
		setenv("boot.netif.ip",
		    inet_ntoa(*(struct in_addr *)ipv4->LocalIpAddress.Addr), 1);
		setenv("boot.netif.netmask",
		    intoa(*(n_long *)ipv4->SubnetMask.Addr), 1);
		setenv("boot.netif.gateway",
		    inet_ntoa(*(struct in_addr *)ipv4->GatewayIpAddress.Addr),
		    1);
		status = ip4config2->SetData(ip4config2,
		    Ip4Config2DataTypePolicy, sizeof(EFI_IP4_CONFIG2_POLICY),
		    &(EFI_IP4_CONFIG2_POLICY) { Ip4Config2PolicyStatic });
		if (EFI_ERROR(status))
			return (efi_status_to_errno(status));

		status = ip4config2->SetData(ip4config2,
		    Ip4Config2DataTypeManualAddress,
		    sizeof(EFI_IP4_CONFIG2_MANUAL_ADDRESS),
		    &(EFI_IP4_CONFIG2_MANUAL_ADDRESS) {
			.Address = ipv4->LocalIpAddress,
			.SubnetMask = ipv4->SubnetMask });
		if (EFI_ERROR(status))
			return (efi_status_to_errno(status));

		if (ipv4->GatewayIpAddress.Addr[0] != 0) {
			status = ip4config2->SetData(ip4config2,
			    Ip4Config2DataTypeGateway, sizeof(EFI_IPv4_ADDRESS),
			    &ipv4->GatewayIpAddress);
			if (EFI_ERROR(status))
				return (efi_status_to_errno(status));
		}

		if (dns != NULL) {
			status = ip4config2->SetData(ip4config2,
			    Ip4Config2DataTypeDnsServer,
			    sizeof(EFI_IPv4_ADDRESS), &dns->DnsServerIp);
			if (EFI_ERROR(status))
				return (efi_status_to_errno(status));
		}
	} else {
		status = ip4config2->SetData(ip4config2,
		    Ip4Config2DataTypePolicy, sizeof(EFI_IP4_CONFIG2_POLICY),
		    &(EFI_IP4_CONFIG2_POLICY) { Ip4Config2PolicyDhcp });
		if (EFI_ERROR(status))
			return (efi_status_to_errno(status));
	}

	return (0);
}

static int
efihttp_dev_init(void)
{
	EFI_DEVICE_PATH *imgpath, *devpath;
	URI_DEVICE_PATH *uri;
	EFI_HANDLE handle;
	EFI_STATUS status;
	int err;
	bool found_http;

	imgpath = efi_lookup_image_devpath(IH);
	if (imgpath == NULL)
		return (ENXIO);
	devpath = imgpath;
	found_http = false;
	for (; !IsDevicePathEnd(devpath);
	    devpath = NextDevicePathNode(devpath)) {
		if (DevicePathType(devpath) != MESSAGING_DEVICE_PATH ||
		    DevicePathSubType(devpath) != MSG_URI_DP)
			continue;
		uri = (URI_DEVICE_PATH *)devpath;
		if (strncmp("http", (const char *)uri->Uri, 4) == 0)
			found_http = true;
	}
	if (!found_http)
		return (ENXIO);

	status = BS->LocateDevicePath(&httpsb_guid, &imgpath, &handle);
	if (EFI_ERROR(status))
		return (efi_status_to_errno(status));

	err = efi_register_handles(&efihttp_dev, &handle, NULL, 1);
	if (!err)
		efihttp_init_done = true;

	return (err);
}

static int
efihttp_dev_strategy(void *devdata __unused, int rw __unused,
    daddr_t blk __unused, size_t size __unused, char *buf __unused,
    size_t *rsize __unused)
{
	return (EIO);
}

static int
efihttp_dev_open(struct open_file *f, ...)
{
	EFI_HTTP_CONFIG_DATA config;
	EFI_HTTPv4_ACCESS_POINT config_access;
	DNS_DEVICE_PATH *dns;
	EFI_DEVICE_PATH *devpath, *imgpath;
	EFI_SERVICE_BINDING_PROTOCOL *sb;
	IPv4_DEVICE_PATH *ipv4;
	MAC_ADDR_DEVICE_PATH *mac;
	URI_DEVICE_PATH *uri;
	struct devdesc *dev;
	struct open_efihttp *oh;
	char *c;
	EFI_HANDLE handle;
	EFI_STATUS status;
	int err, len;

	if (!efihttp_init_done)
		return (ENXIO);

	imgpath = efi_lookup_image_devpath(IH);
	if (imgpath == NULL)
		return (ENXIO);
	devpath = imgpath;
	status = BS->LocateDevicePath(&httpsb_guid, &devpath, &handle);
	if (EFI_ERROR(status))
		return (efi_status_to_errno(status));
	mac = NULL;
	ipv4 = NULL;
	dns = NULL;
	uri = NULL;
	for (; !IsDevicePathEnd(imgpath);
	    imgpath = NextDevicePathNode(imgpath)) {
		if (DevicePathType(imgpath) != MESSAGING_DEVICE_PATH)
			continue;
		switch (DevicePathSubType(imgpath)) {
		case MSG_MAC_ADDR_DP:
			mac = (MAC_ADDR_DEVICE_PATH *)imgpath;
			break;
		case MSG_IPv4_DP:
			ipv4 = (IPv4_DEVICE_PATH *)imgpath;
			break;
		case MSG_DNS_DP:
			dns = (DNS_DEVICE_PATH *)imgpath;
			break;
		case MSG_URI_DP:
			uri = (URI_DEVICE_PATH *)imgpath;
			break;
		default:
			break;
		}
	}

	if (uri == NULL)
		return (ENXIO);

	err = setup_ipv4_config2(handle, mac, ipv4, dns);
	if (err)
		return (err);

	oh = calloc(1, sizeof(struct open_efihttp));
	if (!oh)
		return (ENOMEM);
	oh->dev_handle = handle;
	dev = (struct devdesc *)f->f_devdata;
	dev->d_opendata = oh;

	status = BS->OpenProtocol(handle, &httpsb_guid, (void **)&sb, IH, NULL,
	    EFI_OPEN_PROTOCOL_GET_PROTOCOL);
	if (EFI_ERROR(status)) {
		err = efi_status_to_errno(status);
		goto end;
	}

	status = sb->CreateChild(sb, &oh->http_handle);
	if (EFI_ERROR(status)) {
		err = efi_status_to_errno(status);
		goto end;
	}

	status = BS->OpenProtocol(oh->http_handle, &http_guid,
	    (void **)&oh->http, IH, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
	if (EFI_ERROR(status)) {
		sb->DestroyChild(sb, oh->http_handle);
		err = efi_status_to_errno(status);
		goto end;
	}

	config.HttpVersion = HttpVersion11;
	config.TimeOutMillisec = 0;
	config.LocalAddressIsIPv6 = FALSE;
	config.AccessPoint.IPv4Node = &config_access;
	config_access.UseDefaultAddress = TRUE;
	config_access.LocalPort = 0;
	status = oh->http->Configure(oh->http, &config);
	if (EFI_ERROR(status)) {
		sb->DestroyChild(sb, oh->http_handle);
		err = efi_status_to_errno(status);
		goto end;
	}

	/*
	 * Here we make attempt to construct a "base" URI by stripping
	 * the last two path components from the loaded URI under the
	 * assumption that it is something like:
	 *
	 * http://127.0.0.1/foo/boot/loader.efi
	 *
	 * hoping to arriving at:
	 *
	 * http://127.0.0.1/foo/
	 */
	len = DevicePathNodeLength(&uri->Header) - sizeof(URI_DEVICE_PATH);
	oh->uri_base = malloc(len + 1);
	if (oh->uri_base == NULL) {
		err = ENOMEM;
		goto end;
	}
	strncpy(oh->uri_base, (const char *)uri->Uri, len);
	oh->uri_base[len] = '\0';
	c = strrchr(oh->uri_base, '/');
	if (c != NULL)
		*c = '\0';
	c = strrchr(oh->uri_base, '/');
	if (c != NULL && *(c + 1) != '\0')
		*(c + 1) = '\0';

	err = 0;
end:
	if (err != 0) {
		free(dev->d_opendata);
		dev->d_opendata = NULL;
	}
	return (err);
}

static int
efihttp_dev_close(struct open_file *f)
{
	EFI_SERVICE_BINDING_PROTOCOL *sb;
	struct devdesc *dev;
	struct open_efihttp *oh;
	EFI_STATUS status;

	dev = (struct devdesc *)f->f_devdata;
	oh = (struct open_efihttp *)dev->d_opendata;
	status = BS->OpenProtocol(oh->dev_handle, &httpsb_guid, (void **)&sb,
	    IH, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
	if (EFI_ERROR(status))
		return (efi_status_to_errno(status));
	sb->DestroyChild(sb, oh->http_handle);
	free(oh->uri_base);
	free(oh);
	dev->d_opendata = NULL;
	return (0);
}

static int
_efihttp_fs_open(const char *path, struct open_file *f)
{
	EFI_HTTP_CONFIG_DATA config;
	EFI_HTTPv4_ACCESS_POINT config_access;
	EFI_HTTP_TOKEN token;
	EFI_HTTP_MESSAGE message;
	EFI_HTTP_REQUEST_DATA request;
	EFI_HTTP_RESPONSE_DATA response;
	EFI_HTTP_HEADER headers[3];
	char *host, *hostp;
	char *c;
	struct devdesc *dev;
	struct open_efihttp *oh;
	struct file_efihttp *fh;
	EFI_STATUS status;
	UINTN i;
	int polltime;
	bool done;

	dev = (struct devdesc *)f->f_devdata;
	oh = (struct open_efihttp *)dev->d_opendata;
	fh = calloc(1, sizeof(struct file_efihttp));
	if (fh == NULL)
		return (ENOMEM);
	f->f_fsdata = fh;
	fh->path = strdup(path);

	/*
	 * Reset the HTTP state.
	 *
	 * EDK II's persistent HTTP connection handling is graceless,
	 * assuming that all connections are persistent regardless of
	 * any Connection: header or HTTP version reported by the
	 * server, and failing to send requests when a more sane
	 * implementation would seem to be just reestablishing the
	 * closed connection.
	 *
	 * In the hopes of having some robustness, we indicate to the
	 * server that we will close the connection by using a
	 * Connection: close header. And then here we manually
	 * unconfigure and reconfigure the http instance to force the
	 * connection closed.
	 */
	memset(&config, 0, sizeof(config));
	memset(&config_access, 0, sizeof(config_access));
	config.AccessPoint.IPv4Node = &config_access;
	status = oh->http->GetModeData(oh->http, &config);
	if (EFI_ERROR(status))
		return (efi_status_to_errno(status));
	status = oh->http->Configure(oh->http, NULL);
	if (EFI_ERROR(status))
		return (efi_status_to_errno(status));
	status = oh->http->Configure(oh->http, &config);
	if (EFI_ERROR(status))
		return (efi_status_to_errno(status));

	/* Send the read request */
	done = false;
	status = BS->CreateEvent(EVT_NOTIFY_SIGNAL, TPL_CALLBACK, notify,
	    &done, &token.Event);
	if (EFI_ERROR(status))
		return (efi_status_to_errno(status));

	/* extract the host portion of the URL */
	host = strdup(oh->uri_base);
	if (host == NULL)
		return (ENOMEM);
	hostp = host;
	/* Remove the protocol scheme */
	c = strchr(host, '/');
	if (c != NULL && *(c + 1) == '/')
		hostp = (c + 2);

	/* Remove any path information */
	c = strchr(hostp, '/');
	if (c != NULL)
		*c = '\0';

	token.Status = EFI_NOT_READY;
	token.Message = &message;
	message.Data.Request = &request;
	message.HeaderCount = 3;
	message.Headers = headers;
	message.BodyLength = 0;
	message.Body = NULL;
	request.Method = HttpMethodGet;
	request.Url = calloc(strlen(oh->uri_base) + strlen(path) + 1, 2);
	headers[0].FieldName = (CHAR8 *)"Host";
	headers[0].FieldValue = (CHAR8 *)hostp;
	headers[1].FieldName = (CHAR8 *)"Connection";
	headers[1].FieldValue = (CHAR8 *)"close";
	headers[2].FieldName = (CHAR8 *)"Accept";
	headers[2].FieldValue = (CHAR8 *)"*/*";
	cpy8to16(oh->uri_base, request.Url, strlen(oh->uri_base));
	cpy8to16(path, request.Url + strlen(oh->uri_base), strlen(path));
	status = oh->http->Request(oh->http, &token);
	free(request.Url);
	free(host);
	if (EFI_ERROR(status)) {
		BS->CloseEvent(token.Event);
		return (efi_status_to_errno(status));
	}

	polltime = 0;
	while (!done && polltime < EFIHTTP_POLL_TIMEOUT) {
		status = oh->http->Poll(oh->http);
		if (EFI_ERROR(status))
			break;

		if (!done) {
			delay(100 * 1000);
			polltime += 100;
		}
	}
	BS->CloseEvent(token.Event);
	if (EFI_ERROR(token.Status))
		return (efi_status_to_errno(token.Status));

	/* Wait for the read response */
	done = false;
	status = BS->CreateEvent(EVT_NOTIFY_SIGNAL, TPL_CALLBACK, notify,
	    &done, &token.Event);
	if (EFI_ERROR(status))
		return (efi_status_to_errno(status));
	token.Status = EFI_NOT_READY;
	token.Message = &message;
	message.Data.Response = &response;
	message.HeaderCount = 0;
	message.Headers = NULL;
	message.BodyLength = 0;
	message.Body = NULL;
	response.StatusCode = HTTP_STATUS_UNSUPPORTED_STATUS;
	status = oh->http->Response(oh->http, &token);
	if (EFI_ERROR(status)) {
		BS->CloseEvent(token.Event);
		return (efi_status_to_errno(status));
	}

	polltime = 0;
	while (!done && polltime < EFIHTTP_POLL_TIMEOUT) {
		status = oh->http->Poll(oh->http);
		if (EFI_ERROR(status))
			break;

		if (!done) {
			delay(100 * 1000);
			polltime += 100;
		}
	}
	BS->CloseEvent(token.Event);
	if (EFI_ERROR(token.Status)) {
		BS->FreePool(message.Headers);
		return (efi_status_to_errno(token.Status));
	}
	if (response.StatusCode != HTTP_STATUS_200_OK) {
		BS->FreePool(message.Headers);
		return (EIO);
	}
	fh->size = 0;
	fh->is_dir = false;
	for (i = 0; i < message.HeaderCount; i++) {
		if (strcasecmp((const char *)message.Headers[i].FieldName,
		    "Content-Length") == 0)
			fh->size = strtoul((const char *)
			    message.Headers[i].FieldValue, NULL, 10);
		else if (strcasecmp((const char *)message.Headers[i].FieldName,
		    "Content-type") == 0) {
			if (strncmp((const char *)message.Headers[i].FieldValue,
			    "text/html", 9) == 0)
				fh->is_dir = true;
		}
	}

	return (0);
}

static int
efihttp_fs_open(const char *path, struct open_file *f)
{
	char *path_slash;
	int err;

	if (!efihttp_init_done)
		return (ENXIO);
	/*
	 * If any path fails to open, try with a trailing slash in
	 * case it's a directory.
	 */
	err = _efihttp_fs_open(path, f);
	if (err != 0) {
		/*
		 * Work around a bug in the EFI HTTP implementation which
		 * causes a crash if the http instance isn't torn down
		 * between requests.
		 * See https://bugzilla.tianocore.org/show_bug.cgi?id=1917
		 */
		efihttp_dev_close(f);
		efihttp_dev_open(f);
		path_slash = malloc(strlen(path) + 2);
		if (path_slash == NULL)
			return (ENOMEM);
		strcpy(path_slash, path);
		strcat(path_slash, "/");
		err = _efihttp_fs_open(path_slash, f);
		free(path_slash);
	}
	return (err);
}

static int
efihttp_fs_close(struct open_file *f __unused)
{
	return (0);
}

static int
_efihttp_fs_read(struct open_file *f, void *buf, size_t size, size_t *resid)
{
	EFI_HTTP_TOKEN token;
	EFI_HTTP_MESSAGE message;
	EFI_STATUS status;
	struct devdesc *dev;
	struct open_efihttp *oh;
	struct file_efihttp *fh;
	bool done;
	int polltime;

	fh = (struct file_efihttp *)f->f_fsdata;

	if (fh->size > 0 && fh->offset >= fh->size) {
		if (resid != NULL)
			*resid = size;

		return 0;
	}

	dev = (struct devdesc *)f->f_devdata;
	oh = (struct open_efihttp *)dev->d_opendata;
	done = false;
	status = BS->CreateEvent(EVT_NOTIFY_SIGNAL, TPL_CALLBACK, notify,
	    &done, &token.Event);
	if (EFI_ERROR(status)) {
		return (efi_status_to_errno(status));
	}
	token.Status = EFI_NOT_READY;
	token.Message = &message;
	message.Data.Request = NULL;
	message.HeaderCount = 0;
	message.Headers = NULL;
	message.BodyLength = size;
	message.Body = buf;
	status = oh->http->Response(oh->http, &token);
	if (status == EFI_CONNECTION_FIN) {
		if (resid)
			*resid = size;
		return (0);
	} else if (EFI_ERROR(status)) {
		BS->CloseEvent(token.Event);
		return (efi_status_to_errno(status));
	}
	polltime = 0;
	while (!done && polltime < EFIHTTP_POLL_TIMEOUT) {
		status = oh->http->Poll(oh->http);
		if (EFI_ERROR(status))
				break;

		if (!done) {
			delay(100 * 1000);
			polltime += 100;
		}
	}
	BS->CloseEvent(token.Event);
	if (token.Status == EFI_CONNECTION_FIN) {
		if (resid)
			*resid = size;
		return (0);
	} else if (EFI_ERROR(token.Status))
		return (efi_status_to_errno(token.Status));
	if (resid)
		*resid = size - message.BodyLength;
	fh->offset += message.BodyLength;
	return (0);
}

static int
efihttp_fs_read(struct open_file *f, void *buf, size_t size, size_t *resid)
{
	size_t res;
	int err = 0;

	while (size > 0) {
		err = _efihttp_fs_read(f, buf, size, &res);
		if (err != 0 || res == size)
			goto end;
		buf += (size - res);
		size = res;
	}
end:
	if (resid)
		*resid = size;
	return (err);
}

static int
efihttp_fs_write(struct open_file *f __unused, const void *buf __unused,
    size_t size __unused, size_t *resid __unused)
{
	return (EIO);
}

static off_t
efihttp_fs_seek(struct open_file *f, off_t offset, int where)
{
	struct file_efihttp *fh;
	char *path;
	void *buf;
	size_t res, res2;
	int err;

	fh = (struct file_efihttp *)f->f_fsdata;
	if (where == SEEK_SET && fh->offset == offset)
		return (0);
	if (where == SEEK_SET && fh->offset < offset) {
		buf = malloc(1500);
		if (buf == NULL)
			return (ENOMEM);
		res = offset - fh->offset;
		while (res > 0) {
			err = _efihttp_fs_read(f, buf, min(1500, res), &res2);
			if (err != 0) {
				free(buf);
				return (err);
			}
			res -= min(1500, res) - res2;
		}
		free(buf);
		return (0);
	} else if (where == SEEK_SET) {
		path = fh->path;
		fh->path = NULL;
		efihttp_fs_close(f);
		/*
		 * Work around a bug in the EFI HTTP implementation which
		 * causes a crash if the http instance isn't torn down
		 * between requests.
		 * See https://bugzilla.tianocore.org/show_bug.cgi?id=1917
		 */
		efihttp_dev_close(f);
		efihttp_dev_open(f);
		err = efihttp_fs_open(path, f);
		free(path);
		if (err != 0)
			return (err);
		return efihttp_fs_seek(f, offset, where);
	}
	return (EIO);
}

static int
efihttp_fs_stat(struct open_file *f, struct stat *sb)
{
	struct file_efihttp *fh;

	fh = (struct file_efihttp *)f->f_fsdata;
	memset(sb, 0, sizeof(*sb));
	sb->st_nlink = 1;
	sb->st_mode = 0777 | (fh->is_dir ? S_IFDIR : S_IFREG);
	sb->st_size = fh->size;
	return (0);
}

static int
efihttp_fs_readdir(struct open_file *f, struct dirent *d)
{
	static char *dirbuf = NULL, *db2, *cursor;
	static int dirbuf_len = 0;
	char *end;
	struct file_efihttp *fh;

	fh = (struct file_efihttp *)f->f_fsdata;
	if (dirbuf_len < fh->size) {
		db2 = realloc(dirbuf, fh->size);
		if (db2 == NULL) {
			free(dirbuf);
			return (ENOMEM);
		} else
			dirbuf = db2;

		dirbuf_len = fh->size;
	}

	if (fh->offset != fh->size) {
		efihttp_fs_seek(f, 0, SEEK_SET);
		efihttp_fs_read(f, dirbuf, dirbuf_len, NULL);
		cursor = dirbuf;
	}

	cursor = strstr(cursor, "<a href=\"");
	if (cursor == NULL)
		return (ENOENT);
	cursor += 9;
	end = strchr(cursor, '"');
	if (*(end - 1) == '/') {
		end--;
		d->d_type = DT_DIR;
	} else
		d->d_type = DT_REG;
	memcpy(d->d_name, cursor, end - cursor);
	d->d_name[end - cursor] = '\0';

	return (0);
}
