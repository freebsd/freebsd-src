/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 BBOX.io
 */

#include <sys/param.h>
#include <sys/linker.h>

#include <stand.h>
#include <bootstrap.h>

#include <efi.h>
#include <efilib.h>
#include <Protocol/Tcg2Protocol.h>
#include <Protocol/TcgService.h>

#include "loader_efi.h"

#define	FREEBSD_LOADER_CONFIG_PCR	8
#define	FREEBSD_LOADER_FILE_PCR		9
#define	EV_IPL			((TCG_EVENTTYPE)0x0000000d)

static const char efi_tpm_environment_domain[] =
    "FreeBSD loader environment v1";
static const char efi_tpm_file_identity_domain[] =
    "FreeBSD loader file identity v1";

struct efi_tpm_file_measurement {
	vm_offset_t	addr;
	size_t		len;
};

enum efi_tpm_protocol {
	EFI_TPM_UNKNOWN,
	EFI_TPM_NONE,
	EFI_TPM_1,
	EFI_TPM_2,
};

static enum efi_tpm_protocol tpm_protocol;
static EFI_TCG_PROTOCOL *tcg;
static EFI_TCG2_PROTOCOL *tcg2;

static void
efi_tpm_disable(const char *description, EFI_STATUS status)
{

	printf("TPM measurement of %s failed: %lu\n", description,
	    DECODE_ERROR(status));
	tpm_protocol = EFI_TPM_NONE;
}

static bool
efi_tpm_sensitive_variable(const char *name, size_t len)
{

	/*
	 * The measured environment is recorded in the public firmware event
	 * log.  Add future loader variables containing secrets here before
	 * storing them in the kernel environment.
	 */
	return ((len == sizeof("password") - 1 &&
	    strncmp(name, "password", len) == 0) ||
	    (len == sizeof("bootlock_password") - 1 &&
	    strncmp(name, "bootlock_password", len) == 0) ||
	    (len == sizeof("kern.geom.eli.passphrase") - 1 &&
	    strncmp(name, "kern.geom.eli.passphrase", len) == 0));
}

static bool
efi_tpm_measureable(const struct preloaded_file *fp)
{

	/*
	 * Entropy is deliberately different on every boot and key material must
	 * not acquire a public event-log digest.  Neither is executable boot
	 * content suitable for an attestation baseline.
	 */
	if (strcmp(fp->f_type, "boot_entropy_cache") == 0 ||
	    strcmp(fp->f_type, "boot_entropy_platform") == 0 ||
	    strcmp(fp->f_type, "TSLOG data") == 0 ||
	    strstr(fp->f_type, ":geli_keyfile") != NULL)
		return (false);
	return (true);
}

static bool
efi_tpm2_available(void)
{
	static EFI_GUID tcg2_guid = EFI_TCG2_PROTOCOL_GUID;
	EFI_TCG2_BOOT_SERVICE_CAPABILITY cap;
	EFI_STATUS status;

	status = BS->LocateProtocol(&tcg2_guid, NULL, (void **)&tcg2);
	if (EFI_ERROR(status))
		return (false);

	bzero(&cap, sizeof(cap));
	cap.Size = sizeof(cap);
	status = tcg2->GetCapability(tcg2, &cap);
	return (!EFI_ERROR(status) && cap.TPMPresentFlag);
}

static bool
efi_tpm1_available(void)
{
	static EFI_GUID tcg_guid = EFI_TCG_PROTOCOL_GUID;
	TCG_EFI_BOOT_SERVICE_CAPABILITY cap;
	EFI_PHYSICAL_ADDRESS log, last;
	EFI_STATUS status;
	UINT32 features;

	status = BS->LocateProtocol(&tcg_guid, NULL, (void **)&tcg);
	if (EFI_ERROR(status))
		return (false);

	bzero(&cap, sizeof(cap));
	cap.Size = sizeof(cap);
	status = tcg->StatusCheck(tcg, &cap, &features, &log, &last);
	return (!EFI_ERROR(status) && cap.TPMPresentFlag &&
	    !cap.TPMDeactivatedFlag);
}

static enum efi_tpm_protocol
efi_tpm_probe(void)
{

	if (tpm_protocol != EFI_TPM_UNKNOWN)
		return (tpm_protocol);
	if (efi_tpm2_available())
		tpm_protocol = EFI_TPM_2;
	else if (efi_tpm1_available())
		tpm_protocol = EFI_TPM_1;
	else
		tpm_protocol = EFI_TPM_NONE;
	return (tpm_protocol);
}

static EFI_STATUS
efi_tpm2_measure(UINT32 pcr, const void *buf, size_t len,
    const void *event_data, size_t event_len)
{
	EFI_TCG2_EVENT *event;
	EFI_STATUS status;
	size_t eventsz;

	if (event_len > UINT32_MAX - offsetof(EFI_TCG2_EVENT, Event))
		return (EFI_BAD_BUFFER_SIZE);
	eventsz = offsetof(EFI_TCG2_EVENT, Event) + event_len;
	event = malloc(eventsz);
	if (event == NULL)
		return (EFI_OUT_OF_RESOURCES);

	event->Size = (UINT32)eventsz;
	event->Header.HeaderSize = sizeof(event->Header);
	event->Header.HeaderVersion = EFI_TCG2_EVENT_HEADER_VERSION;
	event->Header.PCRIndex = pcr;
	event->Header.EventType = EV_IPL;
	bcopy(event_data, event->Event, event_len);
	status = tcg2->HashLogExtendEvent(tcg2, 0,
	    (EFI_PHYSICAL_ADDRESS)(uintptr_t)buf, len, event);
	free(event);
	return (status);
}

static EFI_STATUS
efi_tpm1_measure(UINT32 pcr, const void *buf, size_t len,
    const void *event_data, size_t event_len)
{
	TCG_PCR_EVENT *event;
	EFI_PHYSICAL_ADDRESS last;
	EFI_STATUS status;
	size_t eventsz;
	UINT32 event_number;

	if (event_len > UINT32_MAX - offsetof(TCG_PCR_EVENT, Event))
		return (EFI_BAD_BUFFER_SIZE);
	eventsz = offsetof(TCG_PCR_EVENT, Event) + event_len;
	event = calloc(1, eventsz);
	if (event == NULL)
		return (EFI_OUT_OF_RESOURCES);

	event->PCRIndex = pcr;
	event->EventType = EV_IPL;
	event->EventSize = (UINT32)event_len;
	bcopy(event_data, event->Event, event_len);
	event_number = 0;
	status = tcg->HashLogExtendEvent(tcg,
	    (EFI_PHYSICAL_ADDRESS)(uintptr_t)buf, len, TPM_ALG_SHA, event,
	    &event_number, &last);
	free(event);
	return (status);
}

static void
efi_tpm_measure(UINT32 pcr, const void *buf, size_t len,
    const void *event_data, size_t event_len, const char *description)
{
	EFI_STATUS status;

	if (len == 0 || efi_tpm_probe() == EFI_TPM_NONE)
		return;
	if (tpm_protocol == EFI_TPM_2)
		status = efi_tpm2_measure(pcr, buf, len, event_data,
		    event_len);
	else
		status = efi_tpm1_measure(pcr, buf, len, event_data,
		    event_len);
	if (EFI_ERROR(status))
		efi_tpm_disable(description, status);
}

static int
efi_tpm_file_compare(const void *a, const void *b)
{
	const struct preloaded_file *fa, *fb;
	int error;

	fa = *(const struct preloaded_file * const *)a;
	fb = *(const struct preloaded_file * const *)b;
	error = strcmp(fa->f_name, fb->f_name);
	if (error == 0)
		error = strcmp(fa->f_type, fb->f_type);
	return (error);
}

static int
efi_tpm_measurement_compare(const void *a, const void *b)
{
	const struct efi_tpm_file_measurement *ma, *mb;

	ma = a;
	mb = b;
	if (ma->addr < mb->addr)
		return (-1);
	if (ma->addr > mb->addr)
		return (1);
	return (0);
}

static struct preloaded_file **
efi_tpm_sorted_files(size_t *countp)
{
	struct preloaded_file **files, *fp, *key;
	size_t count, i, j;

	count = 0;
	for (fp = file_findfile(NULL, NULL); fp != NULL; fp = fp->f_next)
		count++;
	files = malloc(count * sizeof(*files));
	if (files == NULL) {
		efi_tpm_disable("loaded-file list", EFI_OUT_OF_RESOURCES);
		return (NULL);
	}
	i = 0;
	for (fp = file_findfile(NULL, NULL); fp != NULL; fp = fp->f_next)
		files[i++] = fp;
	for (i = 1; i < count; i++) {
		key = files[i];
		for (j = i; j > 0 &&
		    efi_tpm_file_compare(&files[j - 1], &key) > 0; j--)
			files[j] = files[j - 1];
		files[j] = key;
	}
	*countp = count;
	return (files);
}

static void
efi_tpm_measure_file_identity(struct preloaded_file *fp)
{
	char description[256];
	char *identity, *cp;
	size_t alen, len, nlen, tlen;

	if (!efi_tpm_measureable(fp))
		return;
	tlen = strlen(fp->f_type);
	nlen = strlen(fp->f_name);
	alen = fp->f_args == NULL ? 0 : strlen(fp->f_args);
	if (tlen > SIZE_MAX - sizeof(efi_tpm_file_identity_domain) - 3 ||
	    nlen > SIZE_MAX - sizeof(efi_tpm_file_identity_domain) - tlen - 3 ||
	    alen > SIZE_MAX - sizeof(efi_tpm_file_identity_domain) - tlen -
	    nlen - 3) {
		efi_tpm_disable("loaded-file identity", EFI_BAD_BUFFER_SIZE);
		return;
	}
	len = sizeof(efi_tpm_file_identity_domain) + tlen + nlen + alen + 3;
	identity = malloc(len);
	if (identity == NULL) {
		efi_tpm_disable("loaded-file identity", EFI_OUT_OF_RESOURCES);
		return;
	}
	cp = identity;
	bcopy(efi_tpm_file_identity_domain, cp,
	    sizeof(efi_tpm_file_identity_domain));
	cp += sizeof(efi_tpm_file_identity_domain);
	bcopy(fp->f_type, cp, tlen + 1);
	cp += tlen + 1;
	bcopy(fp->f_name, cp, nlen + 1);
	cp += nlen + 1;
	if (fp->f_args != NULL)
		bcopy(fp->f_args, cp, alen);
	cp[alen] = '\0';
	snprintf(description, sizeof(description), "FreeBSD loaded file: %s",
	    fp->f_name);
	efi_tpm_measure(FREEBSD_LOADER_CONFIG_PCR, identity, len,
	    identity, len, description);
	free(identity);
}

static void
efi_tpm_measure_file_contents(struct preloaded_file *fp)
{
	struct efi_tpm_file_measurement *measurements;
	struct efi_tpm_file_measurement key;
	struct file_metadata *md;
	char description[256];
	size_t count, i, j;
	void *buf;

	count = 0;
	for (md = fp->f_metadata; md != NULL; md = md->md_next) {
		if (md->md_type == MODINFOMD_TPM_MEASURE)
			count++;
	}
	if (count == 0)
		return;
	measurements = malloc(count * sizeof(*measurements));
	if (measurements == NULL) {
		efi_tpm_disable(fp->f_name, EFI_OUT_OF_RESOURCES);
		return;
	}
	i = 0;
	for (md = fp->f_metadata; md != NULL; md = md->md_next) {
		if (md->md_type != MODINFOMD_TPM_MEASURE)
			continue;
		bcopy(md->md_data, &measurements[i++],
		    sizeof(measurements[0]));
	}
	for (i = 1; i < count; i++) {
		key = measurements[i];
		for (j = i; j > 0 && efi_tpm_measurement_compare(
		    &measurements[j - 1], &key) > 0; j--)
			measurements[j] = measurements[j - 1];
		measurements[j] = key;
	}
	for (i = 0; i < count; i++) {
		snprintf(description, sizeof(description),
		    "FreeBSD %s: %s+%#jx", fp->f_type, fp->f_name,
		    (uintmax_t)(measurements[i].addr - fp->f_addr));
		buf = efi_translate(measurements[i].addr);
		efi_tpm_measure(FREEBSD_LOADER_FILE_PCR, buf,
		    measurements[i].len, description,
		    strlen(description) + 1, description);
	}
	free(measurements);
}

void
efi_tpm_measure_boot(void)
{
	struct env_var *ep;
	struct preloaded_file **files;
	struct preloaded_file *fp;
	char *config, *cp;
	size_t i, len, nfiles, nlen, vlen;

	if (efi_tpm_probe() == EFI_TPM_NONE)
		return;

	/*
	 * Commit PCR 8 to the final environment passed to the kernel.  The
	 * environment list is sorted, making this representation deterministic.
	 * Loader passwords and the GELI passphrase are omitted to avoid exposing a
	 * useful digest of a low-entropy secret.
	 */
	len = sizeof(efi_tpm_environment_domain) + 1;
	for (ep = environ; ep != NULL; ep = ep->ev_next) {
		if ((ep->ev_flags & EV_NOKENV) != 0 ||
		    efi_tpm_sensitive_variable(ep->ev_name,
		    strlen(ep->ev_name)))
			continue;
		nlen = strlen(ep->ev_name);
		vlen = ep->ev_value == NULL ? 0 : strlen(ep->ev_value);
		if (nlen > SIZE_MAX - 2 || vlen > SIZE_MAX - nlen - 2 ||
		    len > SIZE_MAX - nlen - vlen - 2) {
			efi_tpm_disable("kernel environment",
			    EFI_BAD_BUFFER_SIZE);
			return;
		}
		len += nlen + vlen + 2;
	}
	config = malloc(len);
	if (config == NULL) {
		efi_tpm_disable("kernel environment", EFI_OUT_OF_RESOURCES);
		return;
	}
	cp = config;
	bcopy(efi_tpm_environment_domain, cp,
	    sizeof(efi_tpm_environment_domain));
	cp += sizeof(efi_tpm_environment_domain);
	for (ep = environ; ep != NULL; ep = ep->ev_next) {
		if ((ep->ev_flags & EV_NOKENV) != 0 ||
		    efi_tpm_sensitive_variable(ep->ev_name,
		    strlen(ep->ev_name)))
			continue;
		nlen = strlen(ep->ev_name);
		bcopy(ep->ev_name, cp, nlen);
		cp += nlen;
		*cp++ = '=';
		if (ep->ev_value != NULL) {
			vlen = strlen(ep->ev_value);
			bcopy(ep->ev_value, cp, vlen);
			cp += vlen;
		}
		*cp++ = '\0';
	}
	*cp++ = '\0';
	efi_tpm_measure(FREEBSD_LOADER_CONFIG_PCR, config, cp - config,
	    config, cp - config, "kernel environment");
	free(config);

	/*
	 * The Lua module table has no defined traversal order.  Sort the final
	 * files so both PCRs describe effective boot policy, not hash-table order.
	 */
	files = efi_tpm_sorted_files(&nfiles);
	if (files == NULL)
		return;
	for (i = 0; i < nfiles; i++)
		efi_tpm_measure_file_identity(files[i]);
	for (i = 0; i < nfiles; i++) {
		fp = files[i];
		efi_tpm_measure_file_contents(fp);
	}
	free(files);
}

void
efi_tpm_measure_file(struct preloaded_file *fp, vm_offset_t addr, size_t len)
{
	struct efi_tpm_file_measurement measurement;
	struct file_metadata *md;

	if (len == 0 || !efi_tpm_measureable(fp) ||
	    efi_tpm_probe() == EFI_TPM_NONE)
		return;
	measurement.addr = addr;
	measurement.len = len;
	md = malloc(sizeof(*md) - sizeof(md->md_data) + sizeof(measurement));
	if (md == NULL) {
		efi_tpm_disable(fp->f_name, EFI_OUT_OF_RESOURCES);
		return;
	}
	md->md_size = sizeof(measurement);
	md->md_type = MODINFOMD_TPM_MEASURE;
	bcopy(&measurement, md->md_data, sizeof(measurement));
	md->md_next = fp->f_metadata;
	fp->f_metadata = md;
}
