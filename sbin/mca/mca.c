/*
 * Copyright (c) 2002 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/sysctl.h>
#include <sys/uuid.h>

/*
 * Hack to make this compile on non-ia64 machines.
 */
#ifdef __ia64__
#include <machine/mca.h>
#else
#include "../../sys/ia64/include/mca.h"
#endif

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <uuid.h>

#define	BCD(x)	((x >> 4) * 10 + (x & 15))

#define	HW_MCA_MAX_CPUID	255

static char hw_mca_count[] = "hw.mca.count";
static char hw_mca_first[] = "hw.mca.first";
static char hw_mca_last[] = "hw.mca.last";
static char hw_mca_recid[] = "hw.mca.%lu.%u";

static char default_dumpfile[] = "/var/log/mca.log";

int fl_dump;
char *file;

static const char *
severity(int error)
{

	switch (error) {
	case MCA_RH_ERROR_RECOVERABLE:
		return ("recoverable");
	case MCA_RH_ERROR_FATAL:
		return ("fatal");
	case MCA_RH_ERROR_CORRECTED:
		return ("corrected");
	}

	return ("unknown");
}

static const char *
uuid(uuid_t *id)
{
	static char buffer[64];
	char *s;

	uuid_to_string(id, &s, NULL);
	strcpy(buffer, s);
	free(s);
	return (buffer);
}

static int
show_value(int indent, const char *var, const char *fmt, ...)
{
	va_list ap;
	int len;

	len = indent;
	while (indent--)
		putchar(' ');
	len += printf("<%s>", var);
	va_start(ap, fmt);
	len += vprintf(fmt, ap);
	len += printf("</%s>\n", var);
	return (len);
}

static size_t
show_header(struct mca_record_header *rh)
{

	printf("  <header>\n");
	show_value(4, "seqnr", "%lld", (long long)rh->rh_seqnr);
	show_value(4, "revision", "%d.%d", BCD(rh->rh_major),
	    BCD(rh->rh_minor));
	show_value(4, "severity", "%s", severity(rh->rh_error));
	show_value(4, "length", "%lld", (long long)rh->rh_length);
	show_value(4, "date", "%d%02d/%02d/%02d",
	    BCD(rh->rh_time[MCA_RH_TIME_CENT]),
	    BCD(rh->rh_time[MCA_RH_TIME_YEAR]),
	    BCD(rh->rh_time[MCA_RH_TIME_MON]),
	    BCD(rh->rh_time[MCA_RH_TIME_MDAY]));
	show_value(4, "time", "%02d:%02d:%02d",
	    BCD(rh->rh_time[MCA_RH_TIME_HOUR]),
	    BCD(rh->rh_time[MCA_RH_TIME_MIN]),
	    BCD(rh->rh_time[MCA_RH_TIME_SEC]));
	if (rh->rh_flags & MCA_RH_FLAGS_PLATFORM_ID)
		show_value(4, "platform", "%s", uuid(&rh->rh_platform));
	printf("  </header>\n");
	return (rh->rh_length);
}

static void
show_cpu_mod(const char *what, int idx, struct mca_cpu_mod *cpu_mod)
{
	printf("      <%s-%d>\n", what, idx);
	if (cpu_mod->cpu_mod_flags & MCA_CPU_MOD_FLAGS_INFO)
		show_value(8, "info", "0x%016llx",
		    (long long)cpu_mod->cpu_mod_info);
	if (cpu_mod->cpu_mod_flags & MCA_CPU_MOD_FLAGS_REQID)
		show_value(8, "requester", "0x%016llx",
		    (long long)cpu_mod->cpu_mod_reqid);
	if (cpu_mod->cpu_mod_flags & MCA_CPU_MOD_FLAGS_RSPID)
		show_value(8, "responder", "0x%016llx",
		    (long long)cpu_mod->cpu_mod_rspid);
	if (cpu_mod->cpu_mod_flags & MCA_CPU_MOD_FLAGS_TGTID)
		show_value(8, "target", "0x%016llx",
		    (long long)cpu_mod->cpu_mod_tgtid);
	if (cpu_mod->cpu_mod_flags & MCA_CPU_MOD_FLAGS_IP)
		show_value(8, "ip", "0x%016llx",
		    (long long)cpu_mod->cpu_mod_ip);
	printf("      </%s-%d>\n", what, idx);
}

static void
show_cpu(struct mca_cpu_record *cpu)
{
	char var[16];
	struct mca_cpu_mod *mod;
	struct mca_cpu_cpuid *cpuid;
	struct mca_cpu_psi *psi;
	int i, n;

	printf("    <cpu>\n");

	if (cpu->cpu_flags & MCA_CPU_FLAGS_ERRMAP)
		show_value(6, "errmap", "0x%016llx",
		    (long long)cpu->cpu_errmap);
	if (cpu->cpu_flags & MCA_CPU_FLAGS_STATE)
		show_value(6, "state", "0x%016llx",
		    (long long)cpu->cpu_state);
	if (cpu->cpu_flags & MCA_CPU_FLAGS_CR_LID)
		show_value(6, "cr_lid", "0x%016llx",
		    (long long)cpu->cpu_cr_lid);

	mod = (struct mca_cpu_mod*)(cpu + 1);
	n = MCA_CPU_FLAGS_CACHE(cpu->cpu_flags);
	for (i = 0; i < n; i++)
		show_cpu_mod("cache", i, mod++);
	n = MCA_CPU_FLAGS_TLB(cpu->cpu_flags);
	for (i = 0; i < n; i++)
		show_cpu_mod("tlb", i, mod++);
	n = MCA_CPU_FLAGS_BUS(cpu->cpu_flags);
	for (i = 0; i < n; i++)
		show_cpu_mod("bus", i, mod++);
	n = MCA_CPU_FLAGS_REG(cpu->cpu_flags);
	for (i = 0; i < n; i++)
		show_cpu_mod("reg", i, mod++);
	n = MCA_CPU_FLAGS_MS(cpu->cpu_flags);
	for (i = 0; i < n; i++)
		show_cpu_mod("ms", i, mod++);

	cpuid = (struct mca_cpu_cpuid*)mod;
	for (i = 0; i < 6; i++) {
		sprintf(var, "cpuid-%d", i);
		show_value(6, var, "0x%016llx", (long long)cpuid->cpuid[i]);
	}

	psi = (struct mca_cpu_psi*)(cpuid + 1);
	/* TODO: Dump PSI */

	printf("    </cpu>\n");
}

static void
show_memory(struct mca_mem_record *mem)
{
	printf("    <memory>\n");

	if (mem->mem_flags & MCA_MEM_FLAGS_STATUS)
		show_value(6, "status", "0x%016llx",
		    (long long)mem->mem_status);
	if (mem->mem_flags & MCA_MEM_FLAGS_ADDR)
		show_value(6, "address", "0x%016llx",
		    (long long)mem->mem_addr);
	if (mem->mem_flags & MCA_MEM_FLAGS_ADDRMASK)
		show_value(6, "mask", "0x%016llx",
		    (long long)mem->mem_addrmask);
	if (mem->mem_flags & MCA_MEM_FLAGS_NODE)
		show_value(6, "node", "0x%04x", mem->mem_node);
	if (mem->mem_flags & MCA_MEM_FLAGS_CARD)
		show_value(6, "card", "0x%04x", mem->mem_card);
	if (mem->mem_flags & MCA_MEM_FLAGS_MODULE)
		show_value(6, "module", "0x%04x", mem->mem_module);
	if (mem->mem_flags & MCA_MEM_FLAGS_BANK)
		show_value(6, "bank", "0x%04x", mem->mem_bank);
	if (mem->mem_flags & MCA_MEM_FLAGS_DEVICE)
		show_value(6, "device", "0x%04x", mem->mem_device);
	if (mem->mem_flags & MCA_MEM_FLAGS_ROW)
		show_value(6, "row", "0x%04x", mem->mem_row);
	if (mem->mem_flags & MCA_MEM_FLAGS_COLUMN)
		show_value(6, "column", "0x%04x", mem->mem_column);
	if (mem->mem_flags & MCA_MEM_FLAGS_BITPOS)
		show_value(6, "bit", "0x%04x", mem->mem_bitpos);
	if (mem->mem_flags & MCA_MEM_FLAGS_REQID)
		show_value(6, "requester", "0x%016llx",
		    (long long)mem->mem_reqid);
	if (mem->mem_flags & MCA_MEM_FLAGS_RSPID)
		show_value(6, "responder", "0x%016llx",
		    (long long)mem->mem_rspid);
	if (mem->mem_flags & MCA_MEM_FLAGS_TGTID)
		show_value(6, "target", "0x%016llx",
		    (long long)mem->mem_tgtid);
	if (mem->mem_flags & MCA_MEM_FLAGS_BUSDATA)
		show_value(6, "status", "0x%016llx",
		    (long long)mem->mem_busdata);
	if (mem->mem_flags & MCA_MEM_FLAGS_OEM_ID)
		show_value(6, "oem", "%s", uuid(&mem->mem_oem_id));
	/* TODO: Dump OEM data */

	printf("    </memory>\n");
}

static void
show_sel(void)
{
	printf("    # SEL\n");
}

static void
show_pci_bus(struct mca_pcibus_record *pcibus)
{
	printf("    <pci-bus>\n");

	if (pcibus->pcibus_flags & MCA_PCIBUS_FLAGS_STATUS)
		show_value(6, "status", "0x%016llx",
		    (long long)pcibus->pcibus_status);
	if (pcibus->pcibus_flags & MCA_PCIBUS_FLAGS_ERROR)
		show_value(6, "error", "0x%04x", pcibus->pcibus_error);
	if (pcibus->pcibus_flags & MCA_PCIBUS_FLAGS_BUS)
		show_value(6, "bus", "0x%04x", pcibus->pcibus_bus);
	if (pcibus->pcibus_flags & MCA_PCIBUS_FLAGS_ADDR)
		show_value(6, "address", "0x%016llx",
		    (long long)pcibus->pcibus_addr);
	if (pcibus->pcibus_flags & MCA_PCIBUS_FLAGS_DATA)
		show_value(6, "data", "0x%016llx",
		    (long long)pcibus->pcibus_data);
	if (pcibus->pcibus_flags & MCA_PCIBUS_FLAGS_CMD)
		show_value(6, "cmd", "0x%016llx",
		    (long long)pcibus->pcibus_cmd);
	if (pcibus->pcibus_flags & MCA_PCIBUS_FLAGS_REQID)
		show_value(6, "requester", "0x%016llx",
		    (long long)pcibus->pcibus_reqid);
	if (pcibus->pcibus_flags & MCA_PCIBUS_FLAGS_RSPID)
		show_value(6, "responder", "0x%016llx",
		    (long long)pcibus->pcibus_rspid);
	if (pcibus->pcibus_flags & MCA_PCIBUS_FLAGS_TGTID)
		show_value(6, "target", "0x%016llx",
		    (long long)pcibus->pcibus_tgtid);
	if (pcibus->pcibus_flags & MCA_PCIBUS_FLAGS_OEM_ID)
		show_value(6, "oem", "%s", uuid(&pcibus->pcibus_oem_id));
	/* TODO: Dump OEM data */

	printf("    </pci-bus>\n");
}

static void
show_smbios(void)
{
	printf("    # SMBIOS\n");
}

static void
show_pci_dev(struct mca_pcidev_record *pcidev)
{
	printf("    <pci-dev>\n");

	if (pcidev->pcidev_flags & MCA_PCIDEV_FLAGS_STATUS)
		show_value(6, "status", "0x%016llx",
		    (long long)pcidev->pcidev_status);
	if (pcidev->pcidev_flags & MCA_PCIDEV_FLAGS_INFO) {
		show_value(6, "vendor", "0x%04x",
		    pcidev->pcidev_info.info_vendor);
		show_value(6, "device", "0x%04x",
		    pcidev->pcidev_info.info_device);
		show_value(6, "class", "0x%06x",
		    MCA_PCIDEV_INFO_CLASS(pcidev->pcidev_info.info_ccfn));
		show_value(6, "function", "0x%02x",
		    MCA_PCIDEV_INFO_FUNCTION(pcidev->pcidev_info.info_ccfn));
		show_value(6, "slot", "0x%02x", pcidev->pcidev_info.info_slot);
		show_value(6, "bus", "0x%04x", pcidev->pcidev_info.info_bus);
		show_value(6, "segment", "0x%04x",
		    pcidev->pcidev_info.info_segment);
	}
	/* TODO: dump registers */
	/* TODO: Dump OEM data */

	printf("    </pci-dev>\n");
}

static void
show_generic(void)
{
	printf("    # GENERIC\n");
}

static size_t
show_section(struct mca_section_header *sh)
{
	static uuid_t uuid_cpu = MCA_UUID_CPU;
	static uuid_t uuid_memory = MCA_UUID_MEMORY;
	static uuid_t uuid_sel = MCA_UUID_SEL;
	static uuid_t uuid_pci_bus = MCA_UUID_PCI_BUS;
	static uuid_t uuid_smbios = MCA_UUID_SMBIOS;
	static uuid_t uuid_pci_dev = MCA_UUID_PCI_DEV;
	static uuid_t uuid_generic = MCA_UUID_GENERIC;

	printf("  <section>\n");
	show_value(4, "uuid", "%s", uuid(&sh->sh_uuid));
	show_value(4, "revision", "%d.%d", BCD(sh->sh_major),
	    BCD(sh->sh_minor));

	if (uuid_equal(&sh->sh_uuid, &uuid_cpu, NULL))
		show_cpu((void*)(sh + 1));
	else if (uuid_equal(&sh->sh_uuid, &uuid_memory, NULL))
		show_memory((void*)(sh + 1));
	else if (uuid_equal(&sh->sh_uuid, &uuid_sel, NULL))
		show_sel();
	else if (uuid_equal(&sh->sh_uuid, &uuid_pci_bus, NULL))
		show_pci_bus((void*)(sh + 1));
	else if (uuid_equal(&sh->sh_uuid, &uuid_smbios, NULL))
		show_smbios();
	else if (uuid_equal(&sh->sh_uuid, &uuid_pci_dev, NULL))
		show_pci_dev((void*)(sh + 1));
	else if (uuid_equal(&sh->sh_uuid, &uuid_generic, NULL))
		show_generic();

	printf("  </section>\n");
	return (sh->sh_length);
}

static void
show(char *data, const char *mib)
{
	size_t reclen, seclen;

	if (mib != NULL)
		printf("<!-- MIB: %s -->\n", mib);

	printf("<record>\n");
	reclen = show_header((void*)data) - sizeof(struct mca_record_header);
	data += sizeof(struct mca_record_header);
	while (reclen > sizeof(struct mca_section_header)) {
		seclen = show_section((void*)data);
		reclen -= seclen;
		data += seclen;
	}
	printf("</record>\n");
}

static void
showall(char *buf, size_t buflen)
{
	struct mca_record_header *rh;
	size_t reclen;

	do {
		if (buflen < sizeof(struct mca_record_header))
			return;

		rh = (void*)buf;
		reclen = rh->rh_length;
		if (buflen < reclen)
			return;

		show(buf, NULL);

		buf += reclen;
		buflen -= reclen;
	}
	while (1);
}

static void
dump(char *data)
{
	struct mca_record_header *rh;
	const char *fn;
	int fd;

	rh = (void*)data;
	fn = (file) ? file : default_dumpfile;
	fd = open(fn, O_WRONLY|O_CREAT|O_APPEND, 0660);
	if (fd == -1)
		err(2, "open(%s)", fn);
	if (write(fd, (void*)rh, rh->rh_length) == -1)
		err(2, "write(%s)", fn);
	close(fd);
}

static void
usage(void)
{

	fprintf(stderr, "usage: mca [-df]\n");
	exit (1);
}

int
main(int argc, char **argv)
{
	char mib[32];
	char *buf;
	size_t len;
	int ch, error, fd;
	int count, first, last, cpuid;

	while ((ch = getopt(argc, argv, "df:")) != -1) {
		switch(ch) {
		case 'd':	/* dump */
			fl_dump = 1;
			break;
		case 'f':
			if (file)
				free(file);		/* XXX complain! */
			file = strdup(optarg);
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (file == NULL || fl_dump) {
		len = sizeof(count);
		if (sysctlbyname(hw_mca_count, &count, &len, NULL, 0) == -1)
			err(1, hw_mca_count);

		if (count == 0)
			errx(0, "no error records found");

		len = sizeof(first);
		if (sysctlbyname(hw_mca_first, &first, &len, NULL, 0) == -1)
			err(1, hw_mca_first);

		len = sizeof(last);
		if (sysctlbyname(hw_mca_last, &last, &len, NULL, 0) == -1)
			err(1, hw_mca_last);

		cpuid = 0;
		error = 0;
		while (count && first <= last) {
			do {
				sprintf(mib, hw_mca_recid, first, cpuid);
				len = 0;
				ch = sysctlbyname(mib, NULL, &len, NULL, 0);
				error = (ch == -1) ? errno : 0;
				if (error != ENOENT)
					break;
				cpuid++;
			} while (cpuid <= HW_MCA_MAX_CPUID);
			if (error == ENOENT && cpuid > HW_MCA_MAX_CPUID) {
				first++;
				cpuid = 0;
				continue;
			}
			if (error)
				errc(1, error, "%s(1)", mib);

			buf = malloc(len);
			if (buf == NULL)
				err(1, "buffer");

			if (sysctlbyname(mib, buf, &len, NULL, 0) == -1)
				err(1, "%s(2)", mib);

			if (fl_dump)
				dump(buf);
			else
				show(buf, mib);

			free(buf);
			count--;
			if (cpuid == HW_MCA_MAX_CPUID) {
				first++;
				cpuid = 0;
			} else
				cpuid++;
		}
	} else {
		fd = open(file, O_RDONLY);
		if (fd == -1)
			err(1, "open(%s)", file);

		len = lseek(fd, 0LL, SEEK_END);
		buf = mmap(NULL, len, PROT_READ, 0U, fd, 0LL);
		if (buf == MAP_FAILED)
			err(1, "mmap(%s)", file);

		showall(buf, len);

		munmap(buf, len);
		close(fd);
	}

	return (0);
}
