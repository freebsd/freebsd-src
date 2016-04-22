/*-
 * Copyright (c) 2000 Benno Rice <benno@jeamland.net>
 * Copyright (c) 2000 Stephane Potvin <sepotvin@videotron.ca>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
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
#include <sys/param.h>

#include <stand.h>

#include "api_public.h"
#include "bootstrap.h"
#include "glue.h"
#include "libuboot.h"

#ifndef nitems
#define	nitems(x)	(sizeof((x)) / sizeof((x)[0]))
#endif

struct uboot_devdesc currdev;
struct arch_switch archsw;		/* MI/MD interface boundary */
int devs_no;

uintptr_t uboot_heap_start;
uintptr_t uboot_heap_end;

struct device_type { 
	const char *name;
	int type;
} device_types[] = {
	{ "disk", DEV_TYP_STOR },
	{ "ide",  DEV_TYP_STOR | DT_STOR_IDE },
	{ "mmc",  DEV_TYP_STOR | DT_STOR_MMC },
	{ "sata", DEV_TYP_STOR | DT_STOR_SATA },
	{ "scsi", DEV_TYP_STOR | DT_STOR_SCSI },
	{ "usb",  DEV_TYP_STOR | DT_STOR_USB },
	{ "net",  DEV_TYP_NET }
};

extern char end[];
extern char bootprog_name[];
extern char bootprog_rev[];
extern char bootprog_date[];
extern char bootprog_maker[];

extern unsigned char _etext[];
extern unsigned char _edata[];
extern unsigned char __bss_start[];
extern unsigned char __sbss_start[];
extern unsigned char __sbss_end[];
extern unsigned char _end[];

#ifdef LOADER_FDT_SUPPORT
extern int command_fdt_internal(int argc, char *argv[]);
#endif

static void
dump_sig(struct api_signature *sig)
{
#ifdef DEBUG
	printf("signature:\n");
	printf("  version\t= %d\n", sig->version);
	printf("  checksum\t= 0x%08x\n", sig->checksum);
	printf("  sc entry\t= 0x%08x\n", sig->syscall);
#endif
}

static void
dump_addr_info(void)
{
#ifdef DEBUG
	printf("\naddresses info:\n");
	printf(" _etext (sdata) = 0x%08x\n", (uint32_t)_etext);
	printf(" _edata         = 0x%08x\n", (uint32_t)_edata);
	printf(" __sbss_start   = 0x%08x\n", (uint32_t)__sbss_start);
	printf(" __sbss_end     = 0x%08x\n", (uint32_t)__sbss_end);
	printf(" __sbss_start   = 0x%08x\n", (uint32_t)__bss_start);
	printf(" _end           = 0x%08x\n", (uint32_t)_end);
	printf(" syscall entry  = 0x%08x\n", (uint32_t)syscall_ptr);
#endif
}

static uint64_t
memsize(struct sys_info *si, int flags)
{
	uint64_t size;
	int i;

	size = 0;
	for (i = 0; i < si->mr_no; i++)
		if (si->mr[i].flags == flags && si->mr[i].size)
			size += (si->mr[i].size);

	return (size);
}

static void
meminfo(void)
{
	uint64_t size;
	struct sys_info *si;
	int t[3] = { MR_ATTR_DRAM, MR_ATTR_FLASH, MR_ATTR_SRAM };
	int i;

	if ((si = ub_get_sys_info()) == NULL)
		panic("could not retrieve system info");

	for (i = 0; i < 3; i++) {
		size = memsize(si, t[i]);
		if (size > 0)
			printf("%s: %juMB\n", ub_mem_type(t[i]),
			    (uintmax_t)(size / 1024 / 1024));
	}
}

static const char *
get_device_type(const char *devstr, int *devtype)
{
	int i;
	int namelen;
	struct device_type *dt;

	if (devstr) {
		for (i = 0; i < nitems(device_types); i++) {
			dt = &device_types[i];
			namelen = strlen(dt->name);
			if (strncmp(dt->name, devstr, namelen) == 0) {
				*devtype = dt->type;
				return (devstr + namelen);
			}
		}
		printf("Unknown device type '%s'\n", devstr);
	}

	*devtype = -1;
	return (NULL);
}

static const char *
device_typename(int type)
{
	int i;

	for (i = 0; i < nitems(device_types); i++)
		if (device_types[i].type == type)
			return (device_types[i].name);

	return ("<unknown>");
}

/*
 * Parse a device string into type, unit, slice and partition numbers. A
 * returned value of -1 for type indicates a search should be done for the
 * first loadable device, otherwise a returned value of -1 for unit
 * indicates a search should be done for the first loadable device of the
 * given type.
 *
 * The returned values for slice and partition are interpreted by
 * disk_open().
 *
 * Valid device strings:                     For device types:
 *
 * <type_name>                               DEV_TYP_STOR, DEV_TYP_NET
 * <type_name><unit>                         DEV_TYP_STOR, DEV_TYP_NET
 * <type_name><unit>:                        DEV_TYP_STOR, DEV_TYP_NET
 * <type_name><unit>:<slice>                 DEV_TYP_STOR
 * <type_name><unit>:<slice>.                DEV_TYP_STOR
 * <type_name><unit>:<slice>.<partition>     DEV_TYP_STOR
 *
 * For valid type names, see the device_types array, above.
 *
 * Slice numbers are 1-based.  0 is a wildcard.
 */
static void
get_load_device(int *type, int *unit, int *slice, int *partition)
{
	char *devstr;
	const char *p;
	char *endp;

	*type = -1;
	*unit = -1;
	*slice = 0;
	*partition = -1;

	devstr = ub_env_get("loaderdev");
	if (devstr == NULL) {
		printf("U-Boot env: loaderdev not set, will probe all devices.\n");
		return;
	}
	printf("U-Boot env: loaderdev='%s'\n", devstr);

	p = get_device_type(devstr, type);

	/* Ignore optional spaces after the device name. */
	while (*p == ' ')
		p++;

	/* Unknown device name, or a known name without unit number.  */
	if ((*type == -1) || (*p == '\0')) {
		return;
	}

	/* Malformed unit number. */
	if (!isdigit(*p)) {
		*type = -1;
		return;
	}

	/* Guaranteed to extract a number from the string, as *p is a digit. */
	*unit = strtol(p, &endp, 10);
	p = endp;

	/* Known device name with unit number and nothing else. */
	if (*p == '\0') {
		return;
	}

	/* Device string is malformed beyond unit number. */
	if (*p != ':') {
		*type = -1;
		*unit = -1;
		return;
	}

	p++;

	/* No slice and partition specification. */
	if ('\0' == *p )
		return;

	/* Only DEV_TYP_STOR devices can have a slice specification. */
	if (!(*type & DEV_TYP_STOR)) {
		*type = -1;
		*unit = -1;
		return;
	}

	*slice = strtoul(p, &endp, 10);

	/* Malformed slice number. */
	if (p == endp) {
		*type = -1;
		*unit = -1;
		*slice = 0;
		return;
	}

	p = endp;
	
	/* No partition specification. */
	if (*p == '\0')
		return;

	/* Device string is malformed beyond slice number. */
	if (*p != '.') {
		*type = -1;
		*unit = -1;
		*slice = 0;
		return;
	}

	p++;

	/* No partition specification. */
	if (*p == '\0')
		return;

	*partition = strtol(p, &endp, 10);
	p = endp;

	/*  Full, valid device string. */
	if (*endp == '\0')
		return;

	/* Junk beyond partition number. */
	*type = -1;
	*unit = -1;
	*slice = 0;
	*partition = -1;
} 

static void
print_disk_probe_info()
{
	char slice[32];
	char partition[32];

	if (currdev.d_disk.slice > 0)
		sprintf(slice, "%d", currdev.d_disk.slice);
	else
		strcpy(slice, "<auto>");

	if (currdev.d_disk.partition >= 0)
		sprintf(partition, "%d", currdev.d_disk.partition);
	else
		strcpy(partition, "<auto>");

	printf("  Checking unit=%d slice=%s partition=%s...",
	    currdev.d_unit, slice, partition);

}

static int
probe_disks(int devidx, int load_type, int load_unit, int load_slice, 
    int load_partition)
{
	int open_result, unit;
	struct open_file f;

	currdev.d_disk.slice = load_slice;
	currdev.d_disk.partition = load_partition;

	f.f_devdata = &currdev;
	open_result = -1;

	if (load_type == -1) {
		printf("  Probing all disk devices...\n");
		/* Try each disk in succession until one works.  */
		for (currdev.d_unit = 0; currdev.d_unit < UB_MAX_DEV;
		     currdev.d_unit++) {
			print_disk_probe_info();
			open_result = devsw[devidx]->dv_open(&f, &currdev);
			if (open_result == 0) {
				printf(" good.\n");
				return (0);
			}
			printf("\n");
		}
		return (-1);
	}

	if (load_unit == -1) {
		printf("  Probing all %s devices...\n", device_typename(load_type));
		/* Try each disk of given type in succession until one works. */
		for (unit = 0; unit < UB_MAX_DEV; unit++) {
			currdev.d_unit = uboot_diskgetunit(load_type, unit);
			if (currdev.d_unit == -1)
				break;
			print_disk_probe_info();
			open_result = devsw[devidx]->dv_open(&f, &currdev);
			if (open_result == 0) {
				printf(" good.\n");
				return (0);
			}
			printf("\n");
		}
		return (-1);
	}

	if ((currdev.d_unit = uboot_diskgetunit(load_type, load_unit)) != -1) {
		print_disk_probe_info();
		open_result = devsw[devidx]->dv_open(&f,&currdev);
		if (open_result == 0) {
			printf(" good.\n");
			return (0);
		}
		printf("\n");
	}

	printf("  Requested disk type/unit/slice/partition not found\n");
	return (-1);
}

int
main(int argc, char **argv)
{
	struct api_signature *sig = NULL;
	int load_type, load_unit, load_slice, load_partition;
	int i;
	const char *ldev;

	/*
	 * We first check if a command line argument was passed to us containing
	 * API's signature address. If it wasn't then we try to search for the
	 * API signature via the usual hinted address.
	 * If we can't find the magic signature and related info, exit with a
	 * unique error code that U-Boot reports as "## Application terminated,
	 * rc = 0xnnbadab1". Hopefully 'badab1' looks enough like "bad api" to
	 * provide a clue. It's better than 0xffffffff anyway.
	 */
	if (!api_parse_cmdline_sig(argc, argv, &sig) && !api_search_sig(&sig))
		return (0x01badab1);

	syscall_ptr = sig->syscall;
	if (syscall_ptr == NULL)
		return (0x02badab1);

	if (sig->version > API_SIG_VERSION)
		return (0x03badab1);

        /* Clear BSS sections */
	bzero(__sbss_start, __sbss_end - __sbss_start);
	bzero(__bss_start, _end - __bss_start);

	/*
	 * Initialise the heap as early as possible.  Once this is done,
	 * alloc() is usable. The stack is buried inside us, so this is safe.
	 */
	uboot_heap_start = round_page((uintptr_t)end);
	uboot_heap_end   = uboot_heap_start + 512 * 1024;
	setheap((void *)uboot_heap_start, (void *)uboot_heap_end);

	/*
	 * Set up console.
	 */
	cons_probe();
	printf("Compatible U-Boot API signature found @%p\n", sig);

	printf("\n");
	printf("%s, Revision %s\n", bootprog_name, bootprog_rev);
	printf("(%s, %s)\n", bootprog_maker, bootprog_date);
	printf("\n");

	dump_sig(sig);
	dump_addr_info();

	meminfo();

	/*
	 * Enumerate U-Boot devices
	 */
	if ((devs_no = ub_dev_enum()) == 0)
		panic("no U-Boot devices found");
	printf("Number of U-Boot devices: %d\n", devs_no);

	get_load_device(&load_type, &load_unit, &load_slice, &load_partition);

	/*
	 * March through the device switch probing for things.
	 */
	for (i = 0; devsw[i] != NULL; i++) {

		if (devsw[i]->dv_init == NULL)
			continue;
		if ((devsw[i]->dv_init)() != 0)
			continue;

		printf("Found U-Boot device: %s\n", devsw[i]->dv_name);

		currdev.d_dev = devsw[i];
		currdev.d_type = currdev.d_dev->dv_type;
		currdev.d_unit = 0;

		if ((load_type == -1 || (load_type & DEV_TYP_STOR)) &&
		    strcmp(devsw[i]->dv_name, "disk") == 0) {
			if (probe_disks(i, load_type, load_unit, load_slice, 
			    load_partition) == 0)
				break;
		}

		if ((load_type == -1 || (load_type & DEV_TYP_NET)) &&
		    strcmp(devsw[i]->dv_name, "net") == 0)
			break;
	}

	/*
	 * If we couldn't find a boot device, return an error to u-boot.
	 * U-boot may be running a boot script that can try something different
	 * so returning an error is better than forcing a reboot.
	 */
	if (devsw[i] == NULL) {
		printf("No boot device found!\n");
		return (0xbadef1ce);
	}

	ldev = uboot_fmtdev(&currdev);
	env_setenv("currdev", EV_VOLATILE, ldev, uboot_setcurrdev, env_nounset);
	env_setenv("loaddev", EV_VOLATILE, ldev, env_noset, env_nounset);
	printf("Booting from %s\n", ldev);

	setenv("LINES", "24", 1);		/* optional */
	setenv("prompt", "loader>", 1);

	archsw.arch_loadaddr = uboot_loadaddr;
	archsw.arch_getdev = uboot_getdev;
	archsw.arch_copyin = uboot_copyin;
	archsw.arch_copyout = uboot_copyout;
	archsw.arch_readin = uboot_readin;
	archsw.arch_autoload = uboot_autoload;

	interact(NULL);				/* doesn't return */

	return (0);
}


COMMAND_SET(heap, "heap", "show heap usage", command_heap);
static int
command_heap(int argc, char *argv[])
{

	printf("heap base at %p, top at %p, used %td\n", end, sbrk(0),
	    sbrk(0) - end);

	return (CMD_OK);
}

COMMAND_SET(reboot, "reboot", "reboot the system", command_reboot);
static int
command_reboot(int argc, char *argv[])
{

	printf("Resetting...\n");
	ub_reset();

	printf("Reset failed!\n");
	while(1);
}

COMMAND_SET(devinfo, "devinfo", "show U-Boot devices", command_devinfo);
static int
command_devinfo(int argc, char *argv[])
{
	int i;

	if ((devs_no = ub_dev_enum()) == 0) {
		command_errmsg = "no U-Boot devices found!?";
		return (CMD_ERROR);
	}
	
	printf("U-Boot devices:\n");
	for (i = 0; i < devs_no; i++) {
		ub_dump_di(i);
		printf("\n");
	}
	return (CMD_OK);
}

COMMAND_SET(sysinfo, "sysinfo", "show U-Boot system info", command_sysinfo);
static int
command_sysinfo(int argc, char *argv[])
{
	struct sys_info *si;

	if ((si = ub_get_sys_info()) == NULL) {
		command_errmsg = "could not retrieve U-Boot sys info!?";
		return (CMD_ERROR);
	}

	printf("U-Boot system info:\n");
	ub_dump_si(si);
	return (CMD_OK);
}

enum ubenv_action {
	UBENV_UNKNOWN,
	UBENV_SHOW,
	UBENV_IMPORT
};

static void
handle_uboot_env_var(enum ubenv_action action, const char * var)
{
	char ldvar[128];
	const char *val;
	char *wrk;
	int len;

	/*
	 * On an import with the variable name formatted as ldname=ubname,
	 * import the uboot variable ubname into the loader variable ldname,
	 * otherwise the historical behavior is to import to uboot.ubname.
	 */
	if (action == UBENV_IMPORT) { 
		len = strcspn(var, "=");
		if (len == 0) {
			printf("name cannot start with '=': '%s'\n", var);
			return;
		}
		if (var[len] == 0) {
			strcpy(ldvar, "uboot.");
			strncat(ldvar, var, sizeof(ldvar) - 7);
		} else {
			len = MIN(len, sizeof(ldvar) - 1);
			strncpy(ldvar, var, len);
			ldvar[len] = 0;
			var = &var[len + 1];
		}
	}

	/*
	 * If the user prepended "uboot." (which is how they usually see these
	 * names) strip it off as a convenience.
	 */
	if (strncmp(var, "uboot.", 6) == 0) {
		var = &var[6];
	}

	/* If there is no variable name left, punt. */
	if (var[0] == 0) {
		printf("empty variable name\n");
		return;
	}

	val = ub_env_get(var);
	if (action == UBENV_SHOW) {
		if (val == NULL)
			printf("uboot.%s is not set\n", var);
		else
			printf("uboot.%s=%s\n", var, val);
	} else if (action == UBENV_IMPORT) {
		if (val != NULL) {
			setenv(ldvar, val, 1);
		}
	}
}

static int
command_ubenv(int argc, char *argv[])
{
	enum ubenv_action action;
	const char *var;
	int i;

	action = UBENV_UNKNOWN;
	if (argc > 1) {
		if (strcasecmp(argv[1], "import") == 0)
			action = UBENV_IMPORT;
		else if (strcasecmp(argv[1], "show") == 0)
			action = UBENV_SHOW;
	}
	if (action == UBENV_UNKNOWN) {
		command_errmsg = "usage: 'ubenv <import|show> [var ...]";
		return (CMD_ERROR);
	}

	if (argc > 2) {
		for (i = 2; i < argc; i++)
			handle_uboot_env_var(action, argv[i]);
	} else {
		var = NULL;
		for (;;) {
			if ((var = ub_env_enum(var)) == NULL)
				break;
			handle_uboot_env_var(action, var);
		}
	}

	return (CMD_OK);
}
COMMAND_SET(ubenv, "ubenv", "show or import U-Boot env vars", command_ubenv);

#ifdef LOADER_FDT_SUPPORT
/*
 * Since proper fdt command handling function is defined in fdt_loader_cmd.c,
 * and declaring it as extern is in contradiction with COMMAND_SET() macro
 * (which uses static pointer), we're defining wrapper function, which
 * calls the proper fdt handling routine.
 */
static int
command_fdt(int argc, char *argv[])
{

	return (command_fdt_internal(argc, argv));
}

COMMAND_SET(fdt, "fdt", "flattened device tree handling", command_fdt);
#endif
