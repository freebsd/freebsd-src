/*-
 * Copyright (c) 2005-2009 Jung-uk Kim <jkim@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	notice, this list of conditions and the following disclaimer in the
 *	documentation and/or other materials provided with the distribution.
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

#include <stand.h>
#include <sys/endian.h>

#define PTOV(x)		ptov(x)

/* Only enable 64-bit entry point if it makes sense */
#if __SIZEOF_POINTER__ > 4
#define	HAS_SMBV3	1
#endif

/*
 * Detect SMBIOS and export information about the SMBIOS into the
 * environment.
 *
 * System Management BIOS Reference Specification, v2.6 Final
 * http://www.dmtf.org/standards/published_documents/DSP0134_2.6.0.pdf
 *
 * System Management BIOS (SMBIOS) Reference Specification, 3.6.0
 * https://www.dmtf.org/sites/default/files/standards/documents/DSP0134_3.6.0.pdf
 */

/*
 * The first quoted paragraph below can also be found in section 2.1.1 SMBIOS
 * Structure Table Entry Point of System Management BIOS Reference
 * Specification, v2.6 Final
 *
 * (From System Management BIOS (SMBIOS) Reference Specification, 3.6.0)
 * 5.2.1 SMBIOS 2.1 (32-bit) Entry Point
 *
 * "On non-UEFI systems, the 32-bit SMBIOS Entry Point structure, can be
 * located by application software by searching for the anchor-string on
 * paragraph (16-byte) boundaries within the physical memory address
 * range 000F0000h to 000FFFFFh. This entry point encapsulates an intermediate
 * anchor string that is used by some existing DMI browsers.
 *
 * On UEFI-based systems, the SMBIOS Entry Point structure can be located by
 * looking in the EFI Configuration Table for the SMBIOS GUID
 * (SMBIOS_TABLE_GUID, {EB9D2D31-2D88-11D3-9A16-0090273FC14D}) and using the
 * associated pointer. See section 4.6 of the UEFI Specification for details.
 * See section 2.3 of the UEFI Specification for how to report the containing
 * memory type.
 *
 * NOTE While the SMBIOS Major and Minor Versions (offsets 06h and 07h)
 * currently duplicate the information that is present in the SMBIOS BCD
 * Revision (offset 1Eh), they provide a path for future growth in this
 * specification. The BCD Revision, for example, provides only a single digit
 * for each of the major and minor version numbers."
 *
 * 5.2.2 SMBIOS 860 3.0 (64-bit) Entry Point
 *
 * "On non-UEFI systems, the 64-bit SMBIOS Entry Point structure can be located
 * by application software by searching for the anchor-string on paragraph
 * (16-byte) boundaries within the physical memory address range 000F0000h to
 * 000FFFFFh.
 *
 * On UEFI-based systems, the SMBIOS Entry Point structure can be located by
 * looking in the EFI Configuration Table for the SMBIOS 3.x GUID
 * (SMBIOS3_TABLE_GUID, {F2FD1544-9794-4A2C-992E-E5BBCF20E394}) and using the
 * associated pointer. See section 4.6 of the UEFI Specification for details.
 * See section 2.3 of the UEFI Specification for how to report the containing
 * memory type."
 */
#define	SMBIOS_START		0xf0000
#define	SMBIOS_LENGTH		0x10000
#define	SMBIOS_STEP		0x10
#define	SMBIOS_SIG		"_SM_"
#define	SMBIOS3_SIG		"_SM3_"
#define	SMBIOS_DMI_SIG		"_DMI_"

/*
 * 5.1 General
 *...
 * NOTE The Entry Point Structure and all SMBIOS structures assume a
 * little-endian ordering convention...
 * ...
 *
 * We use memcpy to avoid unaligned access to memory. To normal memory, this is
 * fine, but the memory we are using might be mmap'd /dev/mem which under Linux
 * on aarch64 doesn't allow unaligned access. leXdec and friends can't be used
 * because those can optimize to an unaligned load (which often is fine, but not
 * for mmap'd /dev/mem which has special memory attributes).
 */
static inline uint8_t SMBIOS_GET8(const caddr_t base, int off) { return (base[off]); }

static inline uint16_t
SMBIOS_GET16(const caddr_t base, int off)
{
	uint16_t v;

	memcpy(&v, base + off, sizeof(v));
	return (le16toh(v));
}

static inline uint32_t
SMBIOS_GET32(const caddr_t base, int off)
{
	uint32_t v;

	memcpy(&v, base + off, sizeof(v));
	return (le32toh(v));
}

static inline uint64_t
SMBIOS_GET64(const caddr_t base, int off)
{
	uint64_t v;

	memcpy(&v, base + off, sizeof(v));
	return (le64toh(v));
}

#define	SMBIOS_GETLEN(base)	SMBIOS_GET8(base, 0x01)
#define	SMBIOS_GETSTR(base)	((base) + SMBIOS_GETLEN(base))

struct smbios_attr {
	int		probed;
	caddr_t 	addr;
	size_t		length;
	size_t		count;
	int		major;
	int		minor;
	int		ver;
	const char*	bios_vendor;
	const char*	maker;
	const char*	product;
	uint32_t	enabled_memory;
	uint32_t	old_enabled_memory;
	uint8_t		enabled_sockets;
	uint8_t		populated_sockets;
};

static struct smbios_attr smbios;
#ifdef HAS_SMBV3
static int isv3;
#endif

static uint8_t
smbios_checksum(const caddr_t addr, const uint8_t len)
{
	uint8_t		sum;
	int		i;

	for (sum = 0, i = 0; i < len; i++)
		sum += SMBIOS_GET8(addr, i);
	return (sum);
}

static caddr_t
smbios_sigsearch(const caddr_t addr, const uint32_t len)
{
	caddr_t		cp;

	/* Search on 16-byte boundaries. */
	for (cp = addr; cp < addr + len; cp += SMBIOS_STEP) {
		/* v2.1, 32-bit Entry point */
		if (strncmp(cp, SMBIOS_SIG, sizeof(SMBIOS_SIG) - 1) == 0 &&
		    smbios_checksum(cp, SMBIOS_GET8(cp, 0x05)) == 0 &&
		    strncmp(cp + 0x10, SMBIOS_DMI_SIG, 5) == 0 &&
		    smbios_checksum(cp + 0x10, 0x0f) == 0)
			return (cp);

#ifdef HAS_SMBV3
		/* v3.0, 64-bit Entry point */
		if (strncmp(cp, SMBIOS3_SIG, sizeof(SMBIOS3_SIG) - 1) == 0 &&
		    smbios_checksum(cp, SMBIOS_GET8(cp, 0x06)) == 0) {
			isv3 = 1;
			return (cp);
		}
#endif
	}
	return (NULL);
}

static const char*
smbios_getstring(caddr_t addr, const int offset)
{
	caddr_t		cp;
	int		i, idx;

	idx = SMBIOS_GET8(addr, offset);
	if (idx != 0) {
		cp = SMBIOS_GETSTR(addr);
		for (i = 1; i < idx; i++)
			cp += strlen(cp) + 1;
		return cp;
	}
	return (NULL);
}

static void
smbios_setenv(const char *name, caddr_t addr, const int offset)
{
	const char*	val;

	val = smbios_getstring(addr, offset);
	if (val != NULL)
		setenv(name, val, 1);
}

#ifdef SMBIOS_SERIAL_NUMBERS

#define	UUID_SIZE		16
#define	UUID_TYPE		uint32_t
#define	UUID_STEP		sizeof(UUID_TYPE)
#define	UUID_ALL_BITS		(UUID_SIZE / UUID_STEP)
#define	UUID_GET(base, off)	SMBIOS_GET32(base, off)

static void
smbios_setuuid(const char *name, const caddr_t addr, const int ver __unused)
{
	char		uuid[37];
	int		byteorder, i, ones, zeros;
	UUID_TYPE	n;
	uint32_t	f1;
	uint16_t	f2, f3;

	for (i = 0, ones = 0, zeros = 0; i < UUID_SIZE; i += UUID_STEP) {
		n = UUID_GET(addr, i) + 1;
		if (zeros == 0 && n == 0)
			ones++;
		else if (ones == 0 && n == 1)
			zeros++;
		else
			break;
	}

	if (ones != UUID_ALL_BITS && zeros != UUID_ALL_BITS) {
		/*
		 * 3.3.2.1 System UUID
		 *
		 * "Although RFC 4122 recommends network byte order for all
		 * fields, the PC industry (including the ACPI, UEFI, and
		 * Microsoft specifications) has consistently used
		 * little-endian byte encoding for the first three fields:
		 * time_low, time_mid, time_hi_and_version. The same encoding,
		 * also known as wire format, should also be used for the
		 * SMBIOS representation of the UUID."
		 *
		 * Note: We use network byte order for backward compatibility
		 * unless SMBIOS version is 2.6+ or little-endian is forced.
		 */
#if defined(SMBIOS_LITTLE_ENDIAN_UUID)
		byteorder = LITTLE_ENDIAN;
#elif defined(SMBIOS_NETWORK_ENDIAN_UUID)
		byteorder = BIG_ENDIAN;
#else
		byteorder = ver < 0x0206 ? BIG_ENDIAN : LITTLE_ENDIAN;
#endif
		if (byteorder != LITTLE_ENDIAN) {
			f1 = ntohl(SMBIOS_GET32(addr, 0));
			f2 = ntohs(SMBIOS_GET16(addr, 4));
			f3 = ntohs(SMBIOS_GET16(addr, 6));
		} else {
			f1 = le32toh(SMBIOS_GET32(addr, 0));
			f2 = le16toh(SMBIOS_GET16(addr, 4));
			f3 = le16toh(SMBIOS_GET16(addr, 6));
		}
		sprintf(uuid,
		    "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
		    f1, f2, f3, SMBIOS_GET8(addr, 8), SMBIOS_GET8(addr, 9),
		    SMBIOS_GET8(addr, 10), SMBIOS_GET8(addr, 11),
		    SMBIOS_GET8(addr, 12), SMBIOS_GET8(addr, 13),
		    SMBIOS_GET8(addr, 14), SMBIOS_GET8(addr, 15));
		setenv(name, uuid, 1);
	}
}

#undef UUID_SIZE
#undef UUID_TYPE
#undef UUID_STEP
#undef UUID_ALL_BITS
#undef UUID_GET

#endif

static const char *
smbios_parse_chassis_type(caddr_t addr)
{
	int		type;

	type = SMBIOS_GET8(addr, 0x5);
	switch (type) {
	case 0x1:
		return ("Other");
	case 0x2:
		return ("Unknown");
	case 0x3:
		return ("Desktop");
	case 0x4:
		return ("Low Profile Desktop");
	case 0x5:
		return ("Pizza Box");
	case 0x6:
		return ("Mini Tower");
	case 0x7:
		return ("Tower");
	case 0x8:
		return ("Portable");
	case 0x9:
		return ("Laptop");
	case 0xA:
		return ("Notebook");
	case 0xB:
		return ("Hand Held");
	case 0xC:
		return ("Docking Station");
	case 0xD:
		return ("All in One");
	case 0xE:
		return ("Sub Notebook");
	case 0xF:
		return ("Lunch Box");
	case 0x10:
		return ("Space-saving");
	case 0x11:
		return ("Main Server Chassis");
	case 0x12:
		return ("Expansion Chassis");
	case 0x13:
		return ("SubChassis");
	case 0x14:
		return ("Bus Expansion Chassis");
	case 0x15:
		return ("Peripheral Chassis");
	case 0x16:
		return ("RAID Chassis");
	case 0x17:
		return ("Rack Mount Chassis");
	case 0x18:
		return ("Sealed-case PC");
	case 0x19:
		return ("Multi-system chassis");
	case 0x1A:
		return ("Compact PCI");
	case 0x1B:
		return ("Advanced TCA");
	case 0x1C:
		return ("Blade");
	case 0x1D:
		return ("Blade Enclosure");
	case 0x1E:
		return ("Tablet");
	case 0x1F:
		return ("Convertible");
	case 0x20:
		return ("Detachable");
	case 0x21:
		return ("IoT Gateway");
	case 0x22:
		return ("Embedded PC");
	case 0x23:
		return ("Mini PC");
	case 0x24:
		return ("Stick PC");
	}

	return ("Undefined");
}

static caddr_t
smbios_parse_table(const caddr_t addr)
{
	caddr_t		cp;
	int		proc, size, osize, type;
	uint8_t		bios_minor, bios_major;
	char		buf[16];

	type = SMBIOS_GET8(addr, 0);	/* 3.1.2 Structure Header Format */
	switch(type) {
	case 0:		/* 3.3.1 BIOS Information (Type 0) */
		smbios_setenv("smbios.bios.vendor", addr, 0x04);
		smbios_setenv("smbios.bios.version", addr, 0x05);
		smbios_setenv("smbios.bios.reldate", addr, 0x08);
		bios_major = SMBIOS_GET8(addr, 0x14);
		bios_minor = SMBIOS_GET8(addr, 0x15);
		if (bios_minor != 0xFF && bios_major != 0xFF) {
			snprintf(buf, sizeof(buf), "%u.%u",
			    bios_major, bios_minor);
			setenv("smbios.bios.revision", buf, 1);
		}
		break;

	case 1:		/* 3.3.2 System Information (Type 1) */
		smbios_setenv("smbios.system.maker", addr, 0x04);
		smbios_setenv("smbios.system.product", addr, 0x05);
		smbios_setenv("smbios.system.version", addr, 0x06);
#ifdef SMBIOS_SERIAL_NUMBERS
		smbios_setenv("smbios.system.serial", addr, 0x07);
		smbios_setuuid("smbios.system.uuid", addr + 0x08, smbios.ver);
#endif
		if (smbios.major > 2 ||
		    (smbios.major == 2 && smbios.minor >= 4)) {
			smbios_setenv("smbios.system.sku", addr, 0x19);
			smbios_setenv("smbios.system.family", addr, 0x1a);
		}
		break;

	case 2:		/* 3.3.3 Base Board (or Module) Information (Type 2) */
		smbios_setenv("smbios.planar.maker", addr, 0x04);
		smbios_setenv("smbios.planar.product", addr, 0x05);
		smbios_setenv("smbios.planar.version", addr, 0x06);
#ifdef SMBIOS_SERIAL_NUMBERS
		smbios_setenv("smbios.planar.serial", addr, 0x07);
		smbios_setenv("smbios.planar.tag", addr, 0x08);
#endif
		smbios_setenv("smbios.planar.location", addr, 0x0a);
		break;

	case 3:		/* 3.3.4 System Enclosure or Chassis (Type 3) */
		smbios_setenv("smbios.chassis.maker", addr, 0x04);
		setenv("smbios.chassis.type", smbios_parse_chassis_type(addr), 1);
		smbios_setenv("smbios.chassis.version", addr, 0x06);
#ifdef SMBIOS_SERIAL_NUMBERS
		smbios_setenv("smbios.chassis.serial", addr, 0x07);
		smbios_setenv("smbios.chassis.tag", addr, 0x08);
#endif
		break;

	case 4:		/* 3.3.5 Processor Information (Type 4) */
		/*
		 * Offset 18h: Processor Status
		 *
		 * Bit 7	Reserved, must be 0
		 * Bit 6	CPU Socket Populated
		 *		1 - CPU Socket Populated
		 *		0 - CPU Socket Unpopulated
		 * Bit 5:3	Reserved, must be zero
		 * Bit 2:0	CPU Status
		 *		0h - Unknown
		 *		1h - CPU Enabled
		 *		2h - CPU Disabled by User via BIOS Setup
		 *		3h - CPU Disabled by BIOS (POST Error)
		 *		4h - CPU is Idle, waiting to be enabled
		 *		5-6h - Reserved
		 *		7h - Other
		 */
		proc = SMBIOS_GET8(addr, 0x18);
		if ((proc & 0x07) == 1)
			smbios.enabled_sockets++;
		if ((proc & 0x40) != 0)
			smbios.populated_sockets++;
		break;

	case 6:		/* 3.3.7 Memory Module Information (Type 6, Obsolete) */
		/*
		 * Offset 0Ah: Enabled Size
		 *
		 * Bit 7	Bank connection
		 *		1 - Double-bank connection
		 *		0 - Single-bank connection
		 * Bit 6:0	Size (n), where 2**n is the size in MB
		 *		7Dh - Not determinable (Installed Size only)
		 *		7Eh - Module is installed, but no memory
		 *		      has been enabled
		 *		7Fh - Not installed
		 */
		osize = SMBIOS_GET8(addr, 0x0a) & 0x7f;
		if (osize > 0 && osize < 22)
			smbios.old_enabled_memory += 1 << (osize + 10);
		break;

	case 17:	/* 3.3.18 Memory Device (Type 17) */
		/*
		 * Offset 0Ch: Size
		 *
		 * Bit 15	Granularity
		 *		1 - Value is in kilobytes units
		 *		0 - Value is in megabytes units
		 * Bit 14:0	Size
		 */
		size = SMBIOS_GET16(addr, 0x0c);
		if (size != 0 && size != 0xffff)
			smbios.enabled_memory += (size & 0x8000) != 0 ?
			    (size & 0x7fff) : (size << 10);
		break;

	default:	/* skip other types */
		break;
	}

	/* Find structure terminator. */
	cp = SMBIOS_GETSTR(addr);
	while (SMBIOS_GET16(cp, 0) != 0)
		cp++;

	return (cp + 2);
}

static caddr_t
smbios_find_struct(int type)
{
	caddr_t		dmi;
	size_t		i;
	caddr_t		ep;

	if (smbios.addr == NULL)
		return (NULL);

	ep = smbios.addr + smbios.length;
	for (dmi = smbios.addr, i = 0;
	     dmi < ep && i < smbios.count; i++) {
		if (SMBIOS_GET8(dmi, 0) == type) {
			return dmi;
		}
		/* Find structure terminator. */
		dmi = SMBIOS_GETSTR(dmi);
		while (SMBIOS_GET16(dmi, 0) != 0 && dmi < ep) {
			dmi++;
		}
		dmi += 2;	/* For checksum */
	}

	return (NULL);
}

static void
smbios_probe(const caddr_t addr)
{
	caddr_t		saddr, info;
	uintptr_t	paddr;
	int		maj_off;
	int		min_off;

	if (smbios.probed)
		return;
	smbios.probed = 1;

	/* Search signatures and validate checksums. */
	saddr = smbios_sigsearch(addr ? addr : PTOV(SMBIOS_START),
	    SMBIOS_LENGTH);
	if (saddr == NULL)
		return;

#ifdef HAS_SMBV3
	if (isv3) {
		smbios.length = SMBIOS_GET32(saddr, 0x0c);	/* Structure Table Length */
		paddr = SMBIOS_GET64(saddr, 0x10);		/* Structure Table Address */
		smbios.count = -1;				/* not present in V3 */
		smbios.ver = 0;					/* not present in V3 */
		maj_off = 0x07;
		min_off = 0x08;
	} else
#endif
	{
		smbios.length = SMBIOS_GET16(saddr, 0x16);	/* Structure Table Length */
		paddr = SMBIOS_GET32(saddr, 0x18);		/* Structure Table Address */
		smbios.count = SMBIOS_GET16(saddr, 0x1c);	/* No of SMBIOS Structures */
		smbios.ver = SMBIOS_GET8(saddr, 0x1e);		/* SMBIOS BCD Revision */
		maj_off = 0x06;
		min_off = 0x07;
	}


	if (smbios.ver != 0) {
		smbios.major = smbios.ver >> 4;
		smbios.minor = smbios.ver & 0x0f;
		if (smbios.major > 9 || smbios.minor > 9)
			smbios.ver = 0;
	}
	if (smbios.ver == 0) {
		smbios.major = SMBIOS_GET8(saddr, maj_off);/* SMBIOS Major Version */
		smbios.minor = SMBIOS_GET8(saddr, min_off);/* SMBIOS Minor Version */
	}
	smbios.ver = (smbios.major << 8) | smbios.minor;
	smbios.addr = PTOV(paddr);

	/* Get system information from SMBIOS */
	info = smbios_find_struct(0x00);
	if (info != NULL) {
		smbios.bios_vendor = smbios_getstring(info, 0x04);
	}
	info = smbios_find_struct(0x01);
	if (info != NULL) {
		smbios.maker = smbios_getstring(info, 0x04);
		smbios.product = smbios_getstring(info, 0x05);
	}
}

void
smbios_detect(const caddr_t addr)
{
	char		buf[16];
	caddr_t		dmi;
	size_t		i;

	smbios_probe(addr);
	if (smbios.addr == NULL)
		return;

	for (dmi = smbios.addr, i = 0;
	     dmi < smbios.addr + smbios.length && i < smbios.count; i++)
		dmi = smbios_parse_table(dmi);

	sprintf(buf, "%d.%d", smbios.major, smbios.minor);
	setenv("smbios.version", buf, 1);
	if (smbios.enabled_memory > 0 || smbios.old_enabled_memory > 0) {
		sprintf(buf, "%u", smbios.enabled_memory > 0 ?
		    smbios.enabled_memory : smbios.old_enabled_memory);
		setenv("smbios.memory.enabled", buf, 1);
	}
	if (smbios.enabled_sockets > 0) {
		sprintf(buf, "%u", smbios.enabled_sockets);
		setenv("smbios.socket.enabled", buf, 1);
	}
	if (smbios.populated_sockets > 0) {
		sprintf(buf, "%u", smbios.populated_sockets);
		setenv("smbios.socket.populated", buf, 1);
	}
}

static int
smbios_match_str(const char* s1, const char* s2)
{
	return (s1 == NULL || (s2 != NULL && !strcmp(s1, s2)));
}

int
smbios_match(const char* bios_vendor, const char* maker,
    const char* product)
{
	/* XXXRP currently, only called from non-EFI. */
	smbios_probe(NULL);
	return (smbios_match_str(bios_vendor, smbios.bios_vendor) &&
	    smbios_match_str(maker, smbios.maker) &&
	    smbios_match_str(product, smbios.product));
}
