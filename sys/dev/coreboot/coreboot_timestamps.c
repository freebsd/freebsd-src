/*
 * Copyright (c) 2026 Abdelkader Boudih <freebsd@seuros.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * coreboot(4) timestamp CBMEM reader
 *
 * Maps the timestamp CBMEM region on demand and formats boot stage
 * timing as a human-readable sysctl.  The mapping is transient —
 * created when the sysctl is read and released immediately after.
 */

#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sbuf.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <dev/coreboot/coreboot.h>

/*
 * Timestamp ID to name mapping.
 * IDs from coreboot's src/commonlib/include/commonlib/timestamp_serialized.h.
 */
static const struct cb_id_name ts_names[] = {
	{    1, "start of romstage" },
	{    2, "before RAM initialization" },
	{    3, "after RAM initialization" },
	{    4, "end of romstage" },
	{    5, "start of verified boot" },
	{    6, "end of verified boot" },
	{    8, "starting to load ramstage" },
	{    9, "finished loading ramstage" },
	{   10, "start of ramstage" },
	{   11, "start of bootblock" },
	{   12, "end of bootblock" },
	{   13, "starting to load romstage" },
	{   14, "finished loading romstage" },
	{   15, "starting LZMA decompress" },
	{   16, "finished LZMA decompress" },
	{   17, "starting LZ4 decompress" },
	{   18, "finished LZ4 decompress" },
	{   19, "starting ZSTD decompress" },
	{   20, "finished ZSTD decompress" },
	{   30, "early chipset initialization" },
	{   31, "device enumeration" },
	{   40, "device configure" },
	{   50, "device enable" },
	{   60, "device initialization" },
	{   65, "option ROM initialization" },
	{   66, "option ROM copy done" },
	{   67, "option ROM run done" },
	{   70, "device setup done" },
	{   75, "cbmem post" },
	{   80, "write tables" },
	{   85, "finalize chips" },
	{   90, "starting to load payload" },
	{   98, "acpi wake jump" },
	{   99, "selfboot jump" },
	{  100, "start of postcar" },
	{  101, "end of postcar" },
	{  110, "forced delay start" },
	{  111, "forced delay end" },
	{  112, "read microcode start" },
	{  113, "read microcode end" },
	{  114, "elog init start" },
	{  115, "elog init end" },
	{  950, "fsp memory init start" },
	{  951, "fsp memory init end" },
	{  952, "fsp temp RAM exit start" },
	{  953, "fsp temp RAM exit end" },
	{  954, "fsp silicon init start" },
	{  955, "fsp silicon init end" },
	{  956, "fsp enumerate start" },
	{  957, "fsp enumerate end" },
	{  958, "fsp finalize start" },
	{  959, "fsp finalize end" },
	{  960, "fsp end-of-firmware start" },
	{  961, "fsp end-of-firmware end" },
	{  962, "fsp multi-phase silicon init start" },
	{  963, "fsp multi-phase silicon init end" },
	{  964, "fsp multi-phase memory init start" },
	{  965, "fsp multi-phase memory init end" },
	{  970, "fsp memory init load" },
	{  971, "fsp silicon init load" },
	{ 1000, "depthcharge start" },
	{ 1100, "vboot done" },
	{ 1101, "kernel start" },
	{ 1102, "kernel decompression" },
	{ 0, NULL }
};

static const char *
ts_id_to_name(uint32_t id)
{

	return (cb_id_lookup(ts_names, id, NULL));
}

/*
 * Sysctl handler for hw.coreboot.timestamps.
 * Maps the CBMEM timestamp region, formats entries, and unmaps.
 */
static int
coreboot_timestamps_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct coreboot_softc *sc = arg1;
	struct cb_timestamp_table *tst;
	struct sbuf *sb;
	void *va;
	vm_size_t map_size;
	uint32_t i, max, num;
	uint64_t base;
	uint64_t total;
	uint32_t freq;
	int error;

	if (sc->timestamps_paddr == 0)
		return (ENXIO);

	va = pmap_mapbios(sc->timestamps_paddr,
	    sizeof(struct cb_timestamp_table));
	if (va == NULL)
		return (ENOMEM);

	tst = (struct cb_timestamp_table *)va;
	max = tst->max_entries;
	num = tst->num_entries;
	freq = tst->tick_freq_mhz;
	base = tst->base_time;

	if (max > 1024)
		max = 1024;
	if (num > max)
		num = max;

	pmap_unmapbios(va, sizeof(struct cb_timestamp_table));

	total = (uint64_t)sizeof(struct cb_timestamp_table) +
	    (uint64_t)num * sizeof(struct cb_timestamp_entry);
	if (total > SIZE_MAX)
		return (EINVAL);
	map_size = (vm_size_t)total;
	va = pmap_mapbios(sc->timestamps_paddr, map_size);
	if (va == NULL)
		return (ENOMEM);

	tst = (struct cb_timestamp_table *)va;

	sb = sbuf_new_for_sysctl(NULL, NULL, 0, req);
	if (sb == NULL) {
		pmap_unmapbios(va, map_size);
		return (ENOMEM);
	}

	sbuf_printf(sb, "\n%-4s %-40s %12s %12s\n",
	    "ID", "Stage", "Stamp (us)", "Delta (us)");
	sbuf_printf(sb, "---- ----------------------------------------"
	    " ------------ ------------\n");

	for (i = 0; i < num; i++) {
		uint32_t eid = tst->entries[i].entry_id;
		int64_t stamp = tst->entries[i].entry_stamp;
		int64_t us, delta_us;
		const char *name;

		if (freq > 0)
			us = (stamp - (int64_t)base) / (int64_t)freq;
		else
			us = stamp;

		if (i > 0 && freq > 0) {
			int64_t prev = tst->entries[i - 1].entry_stamp;
			delta_us = (stamp - prev) / (int64_t)freq;
		} else {
			delta_us = 0;
		}

		name = ts_id_to_name(eid);
		if (name != NULL)
			sbuf_printf(sb, "%4u %-40s %12jd %12jd\n",
			    eid, name,
			    (intmax_t)us, (intmax_t)delta_us);
		else
			sbuf_printf(sb, "%4u %-40s %12jd %12jd\n",
			    eid, "(unknown)",
			    (intmax_t)us, (intmax_t)delta_us);
	}

	error = sbuf_finish(sb);
	sbuf_delete(sb);
	pmap_unmapbios(va, map_size);

	return (error);
}

int
coreboot_timestamps_register(struct coreboot_softc *sc,
    struct sysctl_oid *parent)
{

	SYSCTL_ADD_PROC(&sc->sysctl_ctx,
	    SYSCTL_CHILDREN(parent), OID_AUTO, "timestamps",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    sc, 0, coreboot_timestamps_sysctl, "A",
	    "Boot stage timing from coreboot timestamps");

	return (0);
}
