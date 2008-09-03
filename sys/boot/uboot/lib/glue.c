/*-
 * Copyright (c) 2007-2008 Semihalf, Rafal Jaworowski <raj@semihalf.com>
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

#include <stand.h>
#include "api_public.h"
#include "glue.h"

#define DEBUG
#undef DEBUG

#ifdef DEBUG
#define	debugf(fmt, args...) do { printf("%s(): ", __func__); printf(fmt,##args); } while (0)
#else
#define	debugf(fmt, args...)
#endif

/* Some random address used by U-Boot. */
extern long uboot_address;

/* crc32 stuff stolen from lib/libdisk/write_ia64_disk.c */
static uint32_t crc32_tab[] = {
	0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
	0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
	0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
	0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
	0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
	0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
	0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
	0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
	0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
	0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
	0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
	0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
	0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
	0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
	0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
	0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
	0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
	0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
	0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
	0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
	0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
	0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
	0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
	0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
	0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
	0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
	0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
	0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
	0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
	0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
	0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
	0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
	0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
	0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
	0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
	0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
	0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
	0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
	0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
	0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
	0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
	0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
	0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

static uint32_t
crc32(const void *buf, size_t size)
{
	const uint8_t *p;
	uint32_t crc;

	p = buf;
	crc = ~0U;

	while (size--)
		crc = crc32_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8);

	return (crc ^ ~0U);
}


static int
valid_sig(struct api_signature *sig)
{
	uint32_t checksum;
	struct api_signature s;

	if (sig == NULL)
		return (0);
	/*
	 * Clear the checksum field (in the local copy) so as to calculate the
	 * CRC with the same initial contents as at the time when the sig was
	 * produced
	 */
	s = *sig;
	s.checksum = 0;

	checksum = crc32((void *)&s, sizeof(struct api_signature));

	if (checksum != sig->checksum)
		return (0);

	return (1);
}

/*
 * Searches for the U-Boot API signature
 *
 * returns 1/0 depending on found/not found result
 */
int
api_search_sig(struct api_signature **sig)
{
	unsigned char *sp, *spend;

	if (sig == NULL)
		return (0);

	if (uboot_address == 0)
		uboot_address = 255 * 1024 * 1024;

	sp = (void *)(uboot_address & ~0x000fffff);
	spend = sp + 0x00100000 - API_SIG_MAGLEN;
	while (sp < spend) {
		if (!bcmp(sp, API_SIG_MAGIC, API_SIG_MAGLEN)) {
			*sig = (struct api_signature *)sp;
			if (valid_sig(*sig))
				return (1);
		}
		sp += API_SIG_MAGLEN;
	}

	*sig = NULL;
	return (0);
}

/****************************************
 *
 * console
 *
 ****************************************/

int
ub_getc(void)
{
	int c;

	if (!syscall(API_GETC, NULL, (uint32_t)&c))
		return (-1);

	return (c);
}

int
ub_tstc(void)
{
	int t;

	if (!syscall(API_TSTC, NULL, (uint32_t)&t))
		return (-1);

	return (t);
}

void
ub_putc(char c)
{

	syscall(API_PUTC, NULL, (uint32_t)&c);
}

void
ub_puts(const char *s)
{

	syscall(API_PUTS, NULL, (uint32_t)s);
}

/****************************************
 *
 * system
 *
 ****************************************/

void
ub_reset(void)
{

	syscall(API_RESET, NULL);
}


#define	MR_MAX 5
static struct mem_region mr[MR_MAX];
static struct sys_info si;

struct sys_info *
ub_get_sys_info(void)
{
	int err = 0;

	memset(&si, 0, sizeof(struct sys_info));
	si.mr = mr;
	si.mr_no = MR_MAX;
	memset(&mr, 0, sizeof(mr));

	if (!syscall(API_GET_SYS_INFO, &err, (u_int32_t)&si))
		return (NULL);

	return ((err) ? NULL : &si);
}


/****************************************
 *
 * timing
 *
 ****************************************/

void
ub_udelay(unsigned long usec)
{

	syscall(API_UDELAY, NULL, &usec);
}

unsigned long
ub_get_timer(unsigned long base)
{
	unsigned long cur;

	if (!syscall(API_GET_TIMER, NULL, &cur, &base))
		return (0);

	return (cur);
}


/****************************************************************************
 *
 * devices
 *
 * Devices are identified by handles: numbers 0, 1, 2, ..., MAX_DEVS-1
 *
 ***************************************************************************/

#define	MAX_DEVS 6

static struct device_info devices[MAX_DEVS];

struct device_info *
ub_dev_get(int i)
{

	return ((i < 0 || i >= MAX_DEVS) ? NULL : &devices[i]);
}

/*
 * Enumerates the devices: fills out device_info elements in the devices[]
 * array.
 *
 * returns:		number of devices found
 */
int
ub_dev_enum(void)
{
	struct device_info *di;
	int n = 0;

	memset(&devices, 0, sizeof(struct device_info) * MAX_DEVS);
	di = &devices[0];

	if (!syscall(API_DEV_ENUM, NULL, di))
		return (0);

	while (di->cookie != NULL) {

		if (++n >= MAX_DEVS)
			break;

		/* take another device_info */
		di++;

		/* pass on the previous cookie */
		di->cookie = devices[n - 1].cookie;

		if (!syscall(API_DEV_ENUM, NULL, di))
			return (0);
	}

	return (n);
}


/*
 * handle:	0-based id of the device
 *
 * returns:	0 when OK, err otherwise
 */
int
ub_dev_open(int handle)
{
	struct device_info *di;
	int err = 0;

	if (handle < 0 || handle >= MAX_DEVS)
		return (API_EINVAL);

	di = &devices[handle];
	if (!syscall(API_DEV_OPEN, &err, di))
		return (-1);

	return (err);
}

int
ub_dev_close(int handle)
{
	struct device_info *di;

	if (handle < 0 || handle >= MAX_DEVS)
		return (API_EINVAL);

	di = &devices[handle];
	if (!syscall(API_DEV_CLOSE, NULL, di))
		return (-1);

	return (0);
}

/*
 * Validates device for read/write, it has to:
 *
 * - have sane handle
 * - be opened
 *
 * returns:	0/1 accordingly
 */
static int
dev_valid(int handle)
{

	if (handle < 0 || handle >= MAX_DEVS)
		return (0);

	if (devices[handle].state != DEV_STA_OPEN)
		return (0);

	return (1);
}

static int
dev_stor_valid(int handle)
{

	if (!dev_valid(handle))
		return (0);

	if (!(devices[handle].type & DEV_TYP_STOR))
		return (0);

	return (1);
}

int
ub_dev_read(int handle, void *buf, lbasize_t len, lbastart_t start)
{
	struct device_info *di;
	lbasize_t act_len;
	int err = 0;

	if (!dev_stor_valid(handle))
		return (API_ENODEV);

	di = &devices[handle];
	if (!syscall(API_DEV_READ, &err, di, buf, &len, &start, &act_len))
		return (-1);

	if (err)
		return (err);

	if (act_len != len)
		return (API_EIO);

	return (0);
}

static int
dev_net_valid(int handle)
{

	if (!dev_valid(handle))
		return (0);

	if (devices[handle].type != DEV_TYP_NET)
		return (0);

	return (1);
}

int
ub_dev_recv(int handle, void *buf, int len)
{
	struct device_info *di;
	int err = 0, act_len;

	if (!dev_net_valid(handle))
		return (API_ENODEV);

	di = &devices[handle];
	if (!syscall(API_DEV_READ, &err, di, buf, &len, &act_len))
		return (-1);

	if (err)
		return (-1);

	return (act_len);
}

int
ub_dev_send(int handle, void *buf, int len)
{
	struct device_info *di;
	int err = 0;

	if (!dev_net_valid(handle))
		return (API_ENODEV);

	di = &devices[handle];
	if (!syscall(API_DEV_WRITE, &err, di, buf, &len))
		return (-1);

	return (err);
}

static char *
ub_stor_type(int type)
{

	if (type & DT_STOR_IDE)
		return ("IDE");

	if (type & DT_STOR_SCSI)
		return ("SCSI");

	if (type & DT_STOR_USB)
		return ("USB");

	if (type & DT_STOR_MMC);
		return ("MMC");

	return ("Unknown");
}

char *
ub_mem_type(int flags)
{

	switch(flags & 0x000F) {
	case MR_ATTR_FLASH:
		return ("FLASH");
	case MR_ATTR_DRAM:
		return ("DRAM");
	case MR_ATTR_SRAM:
		return ("SRAM");
	default:
		return ("Unknown");
	}
}

void
ub_dump_di(int handle)
{
	struct device_info *di = ub_dev_get(handle);
	int i;

	printf("device info (%d):\n", handle);
	printf("  cookie\t= 0x%08x\n", (uint32_t)di->cookie);
	printf("  type\t\t= 0x%08x\n", di->type);

	if (di->type == DEV_TYP_NET) {
		printf("  hwaddr\t= ");
		for (i = 0; i < 6; i++)
			printf("%02x ", di->di_net.hwaddr[i]);

		printf("\n");

	} else if (di->type & DEV_TYP_STOR) {
		printf("  type\t\t= %s\n", ub_stor_type(di->type));
		printf("  blk size\t\t= %ld\n", di->di_stor.block_size);
		printf("  blk count\t\t= %ld\n", di->di_stor.block_count);
	}
}

void
ub_dump_si(struct sys_info *si)
{
	int i;

	printf("sys info:\n");
	printf("  clkbus\t= %ld MHz\n", si->clk_bus / 1000 / 1000);
	printf("  clkcpu\t= %ld MHz\n", si->clk_cpu / 1000 / 1000);
	printf("  bar\t\t= 0x%08lx\n", si->bar);

	printf("---\n");
	for (i = 0; i < si->mr_no; i++) {
		if (si->mr[i].flags == 0)
			break;

		printf("  start\t= 0x%08lx\n", si->mr[i].start);
		printf("  size\t= 0x%08lx\n", si->mr[i].size);
		printf("  type\t= %s\n", ub_mem_type(si->mr[i].flags));
		printf("---\n");
	}
}

/****************************************
 *
 * env vars
 *
 ****************************************/

char *
ub_env_get(const char *name)
{
	char *value;

	if (!syscall(API_ENV_GET, NULL, (uint32_t)name, (uint32_t)&value))
		return (NULL);

	return (value);
}

void
ub_env_set(const char *name, char *value)
{

	syscall(API_ENV_SET, NULL, (uint32_t)name, (uint32_t)value);
}


static char env_name[256];

const char *
ub_env_enum(const char *last)
{
	const char *env, *str;
	int i;

	env = NULL;

	/*
	 * It's OK to pass only the name piece as last (and not the whole
	 * 'name=val' string), since the API_ENUM_ENV call uses envmatch()
	 * internally, which handles such case
	 */
	if (!syscall(API_ENV_ENUM, NULL, (uint32_t)last, (uint32_t)&env))
		return (NULL);

	if (!env)
		/* no more env. variables to enumerate */
		return (NULL);
#if 0
	if (last && strncmp(env, last, strlen(last)) == 0);
		/* error, trying to enumerate non existing env. variable */
		return NULL;
#endif

	/* next enumerated env var */
	memset(env_name, 0, 256);
	for (i = 0, str = env; *str != '=' && *str != '\0';)
		env_name[i++] = *str++;

	env_name[i] = '\0';

	return (env_name);
}
