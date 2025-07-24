/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Netflix, Inc.
 */

/*
 * tests/sys/tpm also compiles this source with loader shim headers. Build and
 * run those tests when changing this file, including its header dependencies.
 */

#include <stand.h>
#include <bootstrap.h>
#include <sys/linker.h>
#include <efi.h>
#include <efilib.h>
#include <Protocol/Tcg2Protocol.h>
#include <dev/tpm/tpm_eventlog.h>

struct efi_tpm_eventlog {
	struct tpm_eventlog_algorithms algorithms;
	struct preloaded_file *file;
	struct file_metadata *metadata;
	size_t size;
	uint64_t final_count;
	uint32_t flags;
};

static EFI_GUID tcg2_guid = EFI_TCG2_PROTOCOL_GUID;
static EFI_GUID final_guid = EFI_TCG2_FINAL_EVENTS_TABLE_GUID;

/* Restrict every firmware read to a readable RAM descriptor and our cap. */
static size_t
efi_tpm_log_avail(EFI_PHYSICAL_ADDRESS addr, EFI_MEMORY_DESCRIPTOR *map,
    size_t mapsize, size_t descsize)
{
	EFI_MEMORY_DESCRIPTOR *md;
	uint64_t length, offset;
	size_t off;

	if (addr == 0 || addr > UINTPTR_MAX || descsize < sizeof(*md))
		return (0);
	for (off = 0; mapsize - off >= descsize; off += descsize) {
		md = (void *)((uint8_t *)map + off);
		switch (md->Type) {
		case EfiLoaderCode:
		case EfiLoaderData:
		case EfiBootServicesCode:
		case EfiBootServicesData:
		case EfiRuntimeServicesCode:
		case EfiRuntimeServicesData:
		case EfiACPIReclaimMemory:
		case EfiACPIMemoryNVS:
			break;
		default:
			continue;
		}
		if ((md->Attribute & EFI_MEMORY_RP) != 0 ||
		    md->NumberOfPages > UINT64_MAX / EFI_PAGE_SIZE ||
		    addr < md->PhysicalStart)
			continue;
		length = md->NumberOfPages * EFI_PAGE_SIZE;
		offset = addr - md->PhysicalStart;
		if (offset >= length)
			continue;
		length -= offset;
		/* Keep the exclusive end representable by the loader. */
		length = MIN(length, UINTPTR_MAX - (uintptr_t)addr);
		return (MIN(length, TPM_EVENTLOG_MAX_SIZE));
	}
	return (0);
}

static EFI_MEMORY_DESCRIPTOR *
efi_tpm_log_map(UINTN *size, UINTN *descsize)
{
	EFI_MEMORY_DESCRIPTOR *map;
	EFI_STATUS status;
	UINTN key, capacity;
	UINT32 version;
	int retry;

	map = NULL;
	capacity = 0;
	*size = 0;
	*descsize = sizeof(*map);
	for (retry = 0; retry < 3; retry++) {
		status = BS->GetMemoryMap(size, map, &key, descsize, &version);
		if (!EFI_ERROR(status)) {
			if (*size <= capacity && *descsize >= sizeof(*map) &&
			    *size % *descsize == 0)
				return (map);
			break;
		}
		free(map);
		map = NULL;
		if (status != EFI_BUFFER_TOO_SMALL ||
		    *descsize < sizeof(*map) ||
		    *descsize > TPM_EVENTLOG_MAX_SIZE / 2 ||
		    *size > TPM_EVENTLOG_MAX_SIZE - 2 * *descsize)
			break;
		capacity = *size + 2 * *descsize;
		map = malloc(capacity);
		if (map == NULL)
			break;
		*size = capacity;
	}
	free(map);
	return (NULL);
}

static void
efi_tpm_log_remove(struct preloaded_file *fp)
{
	struct file_metadata *md, **prev;

	for (prev = &fp->f_metadata; (md = *prev) != NULL;) {
		if (md->md_type == MODINFOMD_TPM_EVENTLOG) {
			*prev = md->md_next;
			free(md);
		} else
			prev = &md->md_next;
	}
}

void
efi_tpm_log_free(struct efi_tpm_eventlog *log)
{

	if (log != NULL) {
		if (log->metadata != NULL)
			efi_tpm_log_remove(log->file);
		free(log);
	}
}

/*
 * Snapshot the primary log and the final-table prefix already represented in
 * it. Do not call boot services between GetEventLog and taking that snapshot.
 * efi_tpm_measure_boot() must precede this call in bi_load().
 */
struct efi_tpm_eventlog *
efi_tpm_log_prepare(struct preloaded_file *fp)
{
	EFI_TCG2_PROTOCOL *tcg2;
	EFI_MEMORY_DESCRIPTOR *map;
	EFI_PHYSICAL_ADDRESS first, last;
	EFI_STATUS status;
	UINTN mapsize, descsize;
	BOOLEAN truncated;
	struct efi_tpm_eventlog *log;
	struct file_metadata *md;
	struct tpm_eventlog_header hdr;
	const uint8_t *data, *final;
	size_t avail, off, size, end, start;

	efi_tpm_log_remove(fp);
	status = BS->LocateProtocol(&tcg2_guid, NULL, (void **)&tcg2);
	if (EFI_ERROR(status))
		return (NULL);
	map = efi_tpm_log_map(&mapsize, &descsize);
	if (map == NULL)
		return (NULL);
	log = calloc(1, sizeof(*log));
	if (log == NULL) {
		free(map);
		return (NULL);
	}
	first = last = 0;
	truncated = false;
	status = tcg2->GetEventLog(tcg2, EFI_TCG2_EVENT_LOG_FORMAT_TCG_2,
	    &first, &last, &truncated);
	if (EFI_ERROR(status))
		goto fail;
	if (first == 0 || last == 0)
		goto absent;
	avail = efi_tpm_log_avail(first, map, mapsize, descsize);
	data = (const void *)(uintptr_t)first;
	off = tpm_eventlog_specid(data, avail, &log->algorithms);
	if (off == 0 || last < first)
		goto fail;
	if (last > first) {
		if (last - first >= avail || last - first < off)
			goto fail;
		while (off <= last - first) {
			start = off;
			size = tpm_eventlog_event2(data + off, avail - off,
			    &log->algorithms);
			if (size == 0)
				goto fail;
			off += size;
			if (start == last - first)
				break;
			if (off > last - first)
				goto fail;
		}
		if (start != last - first)
			goto fail;
	}
	log->size = off;
	log->flags = truncated ? TPM_EVENTLOG_TRUNCATED : 0;
	final = efi_get_table(&final_guid);
	if (final != NULL) {
		avail = efi_tpm_log_avail((uintptr_t)final, map, mapsize,
		    descsize);
		if (!tpm_eventlog_final(final, avail, &log->algorithms,
		    0, &start, &end))
			goto fail;
		log->final_count = le64dec(final + 8);
	}
	/* Reserve the largest exported record before boot services exit. */
	md = malloc(offsetof(struct file_metadata, md_data) + sizeof(hdr) +
	    TPM_EVENTLOG_MAX_SIZE);
	if (md == NULL)
		goto fail;
	hdr.version = TPM_EVENTLOG_VERSION;
	hdr.format = TPM_EVENTLOG_TCG2;
	hdr.length = log->size;
	hdr.flags = log->flags;
	md->md_type = MODINFOMD_TPM_EVENTLOG;
	md->md_size = sizeof(hdr) + TPM_EVENTLOG_MAX_SIZE;
	memcpy(md->md_data, &hdr, sizeof(hdr));
	memcpy(md->md_data + sizeof(hdr), data, log->size);
	md->md_next = fp->f_metadata;
	fp->f_metadata = md;
	log->file = fp;
	log->metadata = md;
	free(map);
	return (log);
fail:
	printf("Unable to snapshot TPM firmware event log\n");
absent:
	free(map);
	efi_tpm_log_free(log);
	return (NULL);
}

/*
 * Boot services have exited. The final memory map bounds the now immutable
 * final-events table. Copy its new suffix into metadata before RAM is reused
 * by the kernel; the kernel never needs a firmware mapping or an EFI runtime
 * service to read the log. The buffer was allocated by prepare(), which
 * advertised its maximum size for staging reservation. Shrink it now to the
 * actual length. This also works when exit_bs is false.
 */
void
efi_tpm_log_finish(struct efi_tpm_eventlog *log, EFI_MEMORY_DESCRIPTOR *map,
    size_t mapsize, size_t descsize)
{
	struct file_metadata *md;
	struct tpm_eventlog_header hdr;
	const uint8_t *final;
	size_t avail, start, end, size;

	if (log == NULL)
		return;
	start = end = 0;
	final = efi_get_table(&final_guid);
	if (final != NULL) {
		avail = efi_tpm_log_avail((uintptr_t)final, map, mapsize,
		    descsize);
		if (!tpm_eventlog_final(final, avail, &log->algorithms,
		    log->final_count, &start, &end))
			goto out;
	} else if (log->final_count != 0)
		goto out;
	if (end - start > TPM_EVENTLOG_MAX_SIZE - log->size)
		goto out;
	size = log->size + end - start;
	md = log->metadata;
	memcpy(&hdr, md->md_data, sizeof(hdr));
	hdr.length = size;
	md->md_size = sizeof(hdr) + size;
	memcpy(md->md_data, &hdr, sizeof(hdr));
	if (end != start)
		memcpy(md->md_data + sizeof(hdr) + log->size,
		    final + start, end - start);
	/* Transfer the completed record to the module metadata list. */
	log->metadata = NULL;
out:
	/* No firmware calls or console output after ExitBootServices. */
	efi_tpm_log_free(log);
}
