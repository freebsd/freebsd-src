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

#include <sys/types.h>

#include <crc32.h>
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
	spend = sp + 0x00300000 - API_SIG_MAGLEN;
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

static struct mem_region mr[UB_MAX_MR];
static struct sys_info si;

struct sys_info *
ub_get_sys_info(void)
{
	int err = 0;

	memset(&si, 0, sizeof(struct sys_info));
	si.mr = mr;
	si.mr_no = UB_MAX_MR;
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
 * Devices are identified by handles: numbers 0, 1, 2, ..., UB_MAX_DEV-1
 *
 ***************************************************************************/

static struct device_info devices[UB_MAX_DEV];

struct device_info *
ub_dev_get(int i)
{

	return ((i < 0 || i >= UB_MAX_DEV) ? NULL : &devices[i]);
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

	memset(&devices, 0, sizeof(struct device_info) * UB_MAX_DEV);
	di = &devices[0];

	if (!syscall(API_DEV_ENUM, NULL, di))
		return (0);

	while (di->cookie != NULL) {

		if (++n >= UB_MAX_DEV)
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

	if (handle < 0 || handle >= UB_MAX_DEV)
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

	if (handle < 0 || handle >= UB_MAX_DEV)
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

	if (handle < 0 || handle >= UB_MAX_DEV)
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
ub_dev_read(int handle, void *buf, lbasize_t len, lbastart_t start,
    lbasize_t *rlen)
{
	struct device_info *di;
	lbasize_t act_len;
	int err = 0;

	if (!dev_stor_valid(handle))
		return (API_ENODEV);

	di = &devices[handle];
	if (!syscall(API_DEV_READ, &err, di, buf, &len, &start, &act_len))
		return (API_ESYSC);

	if (!err && rlen)
		*rlen = act_len;

	return (err);
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
ub_dev_recv(int handle, void *buf, int len, int *rlen)
{
	struct device_info *di;
	int err = 0, act_len;

	if (!dev_net_valid(handle))
		return (API_ENODEV);

	di = &devices[handle];
	if (!syscall(API_DEV_READ, &err, di, buf, &len, &act_len))
		return (API_ESYSC);

	if (!err)
		*rlen = act_len;

	return (err);
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
		return (API_ESYSC);

	return (err);
}

char *
ub_stor_type(int type)
{

	if (type & DT_STOR_IDE)
		return ("IDE");

	if (type & DT_STOR_SCSI)
		return ("SCSI");

	if (type & DT_STOR_USB)
		return ("USB");

	if (type & DT_STOR_MMC)
		return ("MMC");

	if (type & DT_STOR_NAND)
		return ("NAND");

	return ("Unknown");
}

char *
ub_mem_type(int flags)
{

	switch (flags & 0x000F) {
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

	/*
	 * It's OK to pass only the name piece as last (and not the whole
	 * 'name=val' string), since the API_ENUM_ENV call uses envmatch()
	 * internally, which handles such case
	 */
	env = NULL;
	if (!syscall(API_ENV_ENUM, NULL, (uint32_t)last, (uint32_t)&env))
		return (NULL);

	if (env == NULL)
		/* no more env. variables to enumerate */
		return (NULL);

	/* next enumerated env var */
	memset(env_name, 0, 256);
	for (i = 0, str = env; *str != '=' && *str != '\0';)
		env_name[i++] = *str++;

	env_name[i] = '\0';

	return (env_name);
}
