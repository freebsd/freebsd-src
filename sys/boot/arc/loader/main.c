/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
 * Copyright (c) 1998 Doug Rabson <dfr@freebsd.org>
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
 *
 * $FreeBSD$
 */


#include <stand.h>
#include <string.h>
#include <setjmp.h>

#include <sys/param.h>
#include "bootstrap.h"
#include "libarc.h"
#include "arctypes.h"
#include "arcfuncs.h"

extern	char bootprog_name[], bootprog_rev[], bootprog_date[], bootprog_maker[];

struct arc_devdesc	currdev;	/* our current device */
struct arch_switch	archsw;		/* MI/MD interface boundary */

extern char end[];
extern void halt(void);

#define	ARCENV_BOOTFILE		"OSLoadFilename"

static char *MemoryTypes[] = {
    "MemoryExceptionBlock",
    "MemorySystemBlock",
    "MemoryFree",
    "MemoryBad",
    "MemoryLoadedProgram",
    "MemoryFirmwareTemporary",
    "MemoryFirmwarePermanent",
    "MemoryFreeContiguous",
    "MemorySpecialMemory",
    "MemoryMaximum",
};

#ifdef __alpha__
#define ptob(p)	((p) << 13)
#endif

unsigned long
memsize()
{
    unsigned long amount = 0;
    MEMORY_DESCRIPTOR *desc;

    for (desc = GetMemoryDescriptor(NULL); desc;
	 desc = GetMemoryDescriptor(desc)) {
	printf("%s at %x-%x\n", MemoryTypes[desc->Type],
	       ptob(desc->BasePage),
	       ptob(desc->BasePage + desc->PageCount));
	if (desc->Type == MemoryFree
	    || desc->Type == MemoryFirmwareTemporary)
	    amount += (desc->PageCount << 13); /* XXX pagesize */
    }

    return amount;
}

static char *ConfigurationClasses[] = {
    "SystemClass",
    "ProcessorClass",
    "CacheClass",
    "AdapterClass",
    "ControllerClass",
    "PeripheralClass",
    "MemoryClass",
    "MaximumClass",
};


static char *ConfigurationTypes[] = {
    "ArcSystem",
    "CentralProcessor",
    "FloatingPointProcessor",
    "PrimaryIcache",
    "PrimaryDcache",
    "SecondaryIcache",
    "SecondaryDcache",
    "SecondaryCache",
    "EisaAdapter",
    "TcAdapter",
    "ScsiAdapter",
    "DtiAdapter",
    "MultiFunctionAdapter",
    "DiskController",
    "TapeController",
    "CdromController",
    "WormController",
    "SerialController",
    "NetworkController",
    "DisplayController",
    "ParallelController",
    "PointerController",
    "KeyboardController",
    "AudioController",
    "OtherController",
    "DiskPeripheral",
    "FloppyDiskPeripheral",
    "TapePeripheral",
    "ModemPeripheral",
    "MonitorPeripheral",
    "PrinterPeripheral",
    "PointerPeripheral",
    "KeyboardPeripheral",
    "TerminalPeripheral",
    "OtherPeripheral",
    "LinePeripheral",
    "NetworkPeripheral",
    "SystemMemory",
    "MaximumType",
};

static char *ConfigurationTypeCodes[] = {
    "ARC",
    "CPU",
    "FPC",
    "PrimaryIcache",
    "PrimaryDcache",
    "SecondaryIcache",
    "SecondaryDcache",
    "SecondaryCache",
    "eisa",
    "tc",
    "scsi",
    "dti",
    "multi",
    "disk",
    "tape",
    "cdrom",
    "worm",
    "serial",
    "network",
    "video",
    "par",
    "point",
    "key",
    "audio",
    "other",
    "rdisk",
    "fdisk",
    "tape",
    "modem",
    "monitor",
    "print",
    "pointer",
    "keyboard",
    "term",
    "other",
    "line",
    "network",
    "Memory",
    "MaximumType"
};

static void
indent(int level)
{
    while (level--)
	putchar(' ');
}

void
printconfig(unsigned int level, CONFIGURATION_COMPONENT *component)
{
    CONFIGURATION_COMPONENT *child;

    indent(level);
    printf("%s(%s,%d)",
	   ConfigurationClasses[component->Class],
	   ConfigurationTypes[component->Type],
	   component->Key);
#if 1
    if (component->IdentifierLength)
    	printf("=%d,%s\n", component->IdentifierLength,
	       ptr(component->Identifier));
    else
	putchar('\n');
#endif
    getchar();
    
    for (child = GetChild(component); child; child = GetPeer(child)) {
	printconfig(level + 2, child);
    }
}

void
dumpdisk(const char *name)
{
    u_int32_t fd, count;
    unsigned char buf[512];
    int i, j;

    printf("dump first sector of %s\n", name);
    if (Open(name, OpenReadOnly, &fd) != ESUCCESS) {
	printf("can't open disk\n");
	return;
    }
    if (Read(fd, buf, 512, &count) != ESUCCESS) {
	printf("can't read from disk\n");
	Close(fd);
	return;
    }
    for (i = 0; i < 16; i++) {
	for (j = 0; j < 32; j++)
	    printf("%02x", buf[i*32 + j]);
	putchar('\n');
    }
    Close(fd);
}

void
listdisks(char *path, CONFIGURATION_COMPONENT *component)
{
    CONFIGURATION_COMPONENT *child;
    char newpath[80];
    char keybuf[20];

    if (path == NULL) {
	printf("\nARC disk devices:\n");
	newpath[0] = '\0';
    } else {
	strcpy(newpath, path);
	strcat(newpath, ConfigurationTypeCodes[component->Type]);
	sprintf(keybuf, "(%d)", component->Key);
	strcat(newpath, keybuf);
    }
    if (!strcmp(ConfigurationTypeCodes[component->Type], "rdisk") ||
	!strcmp(ConfigurationTypeCodes[component->Type], "fdisk")) {
	printf("%s\n", newpath);
    }
    for (child = GetChild(component); child; child = GetPeer(child)) {
	listdisks(newpath, child);
    }
}

static int exit_code = 0;
jmp_buf exit_env;

void
exit(int code)
{
    exit_code = 0;
    longjmp(exit_env, 1);
}

int
main(int argc, int argv[], int envp[])
{
    int		i;
    char	*bootfile;
    
    if (setjmp(exit_env))
	return exit_code;

    /* 
     * Initialise the heap as early as possible.  Once this is done,
     * alloc() is usable. The stack is buried inside us, so this is
     * safe.
     */
    setheap((void *)end, (void *)(end + 512*1024));

    /* 
     * XXX Chicken-and-egg problem; we want to have console output
     * early, but some console attributes may depend on reading from
     * eg. the boot device, which we can't do yet.  We can use
     * printf() etc. once this is done.
     */
    cons_probe();

#if 0
    printconfig(0, GetChild(NULL));
    dumpdisk("scsi(0)disk(0)rdisk(0)partition(0)");
#endif
    listdisks(NULL, GetChild(NULL));
    printf("\n");

    make_rpb();

    /*
     * Initialise the block cache
     */
    bcache_init(32, 512);	/* 16k XXX tune this */

    /*
     * March through the device switch probing for things.
     */
    for (i = 0; devsw[i] != NULL; i++)
	if (devsw[i]->dv_init != NULL)
	    (devsw[i]->dv_init)();

    printf("\n");
    printf("%s, Revision %s\n", bootprog_name, bootprog_rev);
    printf("(%s, %s)\n", bootprog_maker, bootprog_date);
    printf("Memory: %ld k\n", memsize() / 1024);
    
    /* We're booting from an SRM disk, try to spiff this */
    /* XXX presumes that biosdisk is first in devsw */
    currdev.d_dev = devsw[0];
    currdev.d_type = currdev.d_dev->dv_type;
    currdev.d_kind.arcdisk.unit = 0;
    /* XXX should be able to detect this, default to autoprobe */
    currdev.d_kind.arcdisk.slice = -1;
    /* default to 'a' */
    currdev.d_kind.arcdisk.partition = 0;

    /* Create arc-specific variables */
    bootfile = GetEnvironmentVariable(ARCENV_BOOTFILE);
    if (bootfile)
	setenv("bootfile", bootfile, 1);

    env_setenv("currdev", EV_VOLATILE,
	       arc_fmtdev(&currdev), arc_setcurrdev, env_nounset);
    env_setenv("loaddev", EV_VOLATILE,
	       arc_fmtdev(&currdev), env_noset, env_nounset);
    setenv("LINES", "24", 1);				/* optional */
    
    archsw.arch_autoload = arc_autoload;
    archsw.arch_getdev = arc_getdev;
    archsw.arch_copyin = arc_copyin;
    archsw.arch_copyout = arc_copyout;
    archsw.arch_readin = arc_readin;

    interact();			/* doesn't return */

    return 0;			/* keep compiler happy */
}

COMMAND_SET(reboot, "reboot", "reboot the system", command_reboot);

static int
command_reboot(int argc, char *argv[])
{

    printf("Rebooting...\n");
    delay(1000000);
    FwReboot();
    /* Note: we shouldn't get to this point! */
    panic("Reboot failed!");
    exit(0);
}

COMMAND_SET(quit, "quit", "exit the loader", command_quit);

static int
command_quit(int argc, char *argv[])
{
    exit(0);
    return(CMD_OK);
}

#if 0

COMMAND_SET(stack, "stack", "show stack usage", command_stack);

static int
command_stack(int argc, char *argv[])
{
    char	*cp;

    for (cp = &stackbase; cp < &stacktop; cp++)
	if (*cp != 0)
	    break;
    
    printf("%d bytes of stack used\n", &stacktop - cp);
    return(CMD_OK);
}

#endif

COMMAND_SET(heap, "heap", "show heap usage", command_heap);

static int
command_heap(int argc, char *argv[])
{
    printf("heap base at %p, top at %p, used %ld\n", end, sbrk(0), sbrk(0) - end);
    return(CMD_OK);
}
