/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: config.c,v 1.16.2.14 1995/10/16 09:25:07 jkh Exp $
 *
 * Copyright (c) 1995
 *	Jordan Hubbard.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    verbatim and that no modifications are made prior to this
 *    point in the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Jordan Hubbard
 *	for the FreeBSD Project.
 * 4. The name of Jordan Hubbard or the FreeBSD project may not be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JORDAN HUBBARD ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL JORDAN HUBBARD OR HIS PETS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, LIFE OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "sysinstall.h"
#include <sys/disklabel.h>
#include <sys/wait.h>

static Chunk *chunk_list[MAX_CHUNKS];
static int nchunks;

/* arg to sort */
static int
chunk_compare(const void *p1, const void *p2)
{
    Chunk *c1, *c2;

    c1 = (Chunk *)p1;
    c2 = (Chunk *)p2;
    if (!c1 && !c2)
	return 0;
    else if (!c1 && c2)
	return 1;
    else if (c1 && !c2)
	return -1;
    else if (!c1->private && !c2->private)
	return 0;
    else if (c1->private && !c2->private)
	return -1;
    else if (!c1->private && c2->private)
	return 1;
    else
	return strcmp(((PartInfo *)(c1->private))->mountpoint, ((PartInfo *)(c2->private))->mountpoint);
}

static char *
name_of(Chunk *c1)
{
    static char rootname[32];

    /* Our boot blocks can't deal with root partitions on slices - need the compatbility name */
    if (c1->type == part && c1->flags & CHUNK_IS_ROOT) {
        sprintf(rootname, "%sa", c1->disk->name);
        return rootname;
    }
    else
        return c1->name;
}

static char *
mount_point(Chunk *c1)
{
    if (c1->type == part && c1->subtype == FS_SWAP)
	return "none";
    else if (c1->type == part || c1->type == fat)
	return ((PartInfo *)c1->private)->mountpoint;
    return "/bogus";
}

static char *
fstype(Chunk *c1)
{
    if (c1->type == fat)
	return "msdos";
    else if (c1->type == part) {
	if (c1->subtype != FS_SWAP)
	    return "ufs";
	else
	    return "swap";
    }
    return "bogus";
}

static char *
fstype_short(Chunk *c1)
{
    if (c1->type == part) {
	if (c1->subtype != FS_SWAP)
	    return "rw";
	else
	    return "sw";
    }
    else if (c1->type == fat)
	return "ro";
    return "bog";
}

static int
seq_num(Chunk *c1)
{
    if (c1->type == part && c1->subtype != FS_SWAP)
	return 1;
    return 0;
}

int
configFstab(void)
{
    Device **devs;
    Disk *disk;
    FILE *fstab;
    int i, cnt;
    Chunk *c1, *c2;

    if (!RunningAsInit) {
	if (file_readable("/etc/fstab"))
	    return RET_SUCCESS;
	else
	    msgConfirm("Attempting to rebuild your /etc/fstab file.\n"
		       "Warning: If you had any CD devices in use before running\n"
		       "sysinstall then they may NOT be found in this run!");
    }

    devs = deviceFind(NULL, DEVICE_TYPE_DISK);
    if (!devs) {
	msgConfirm("No disks found!");
	return RET_FAIL;
    }

    /* Record all the chunks */
    nchunks = 0;
    for (i = 0; devs[i]; i++) {
	if (!devs[i]->enabled)
	    continue;
	disk = (Disk *)devs[i]->private;
	if (!disk->chunks)
	    msgFatal("No chunk list found for %s!", disk->name);
	for (c1 = disk->chunks->part; c1; c1 = c1->next) {
	    if (c1->type == freebsd) {
		for (c2 = c1->part; c2; c2 = c2->next) {
		    if (c2->type == part && (c2->subtype == FS_SWAP || c2->private))
			chunk_list[nchunks++] = c2;
		}
	    }
	    else if (c1->type == fat && c1->private)
		chunk_list[nchunks++] = c1;
	}
    }
    chunk_list[nchunks] = 0;
    qsort(chunk_list, nchunks, sizeof(Chunk *), chunk_compare);

    fstab = fopen("/etc/fstab", "w");
    if (!fstab) {
	msgConfirm("Unable to create a new /etc/fstab file!\n"
		   "Manual intervention will be required.");
	return RET_FAIL;
    }

    /* Go for the burn */
    msgDebug("Generating /etc/fstab file\n");
    for (i = 0; i < nchunks; i++)
	fprintf(fstab, "/dev/%s\t\t\t%s\t\t%s\t%s %d %d\n", name_of(chunk_list[i]), mount_point(chunk_list[i]),
		fstype(chunk_list[i]), fstype_short(chunk_list[i]), seq_num(chunk_list[i]), seq_num(chunk_list[i]));
    Mkdir("/proc", NULL);
    fprintf(fstab, "proc\t\t\t\t/proc\t\tprocfs\trw 0 0\n");

    /* Now look for the CDROMs */
    devs = deviceFind(NULL, DEVICE_TYPE_CDROM);
    cnt = deviceCount(devs);

    /* Write the first one out as /cdrom */
    if (cnt) {
	if (Mkdir("/cdrom", NULL))
	    msgConfirm("Unable to make mount point for: /cdrom\n");
	else
	    fprintf(fstab, "/dev/%s\t\t\t/cdrom\t\tcd9660\tro 0 0\n", devs[0]->name);
    }

    /* Write the others out as /cdrom<n> */
    for (i = 1; i < cnt; i++) {
	char cdname[10];

	sprintf(cdname, "/cdrom%d", i);
	if (Mkdir(cdname, NULL))
	    msgConfirm("Unable to make mount point for: %s\n", cdname);
	else
	    fprintf(fstab, "/dev/%s\t\t\t%s\t\tcd9660\tro 0 0\n", devs[i]->name, cdname);
    }
    fclose(fstab);
    if (isDebug())
	msgDebug("Wrote out /etc/fstab file\n");
    return RET_SUCCESS;
}

/*
 * This sucks in /etc/sysconfig, substitutes anything needing substitution, then
 * writes it all back out.  It's pretty gross and needs re-writing at some point.
 */
void
configSysconfig(void)
{
    FILE *fp;
    char *lines[5001];	/* Some big number we're not likely to ever reach - I'm being really lazy here, I know */
    char line[256];
    Variable *v;
    int i, nlines = 0;

    fp = fopen("/etc/sysconfig", "r");
    if (!fp) {
	msgConfirm("Unable to open /etc/sysconfig file!  Things may work\n"
		   "rather strangely as a result of this.");
	return;
    }
    for (i = 0; i < 5000; i++) {
	if (!fgets(line, 255, fp))
	    break;
	lines[nlines++] = strdup(line);
    }
    lines[nlines] = NULL;
    fclose(fp);
    for (v = VarHead; v; v = v->next) {
	for (i = 0; i < nlines; i++) {
	    char modify[256], *cp;

	    if (lines[i][0] == '#' || lines[i][0] == ';')
		continue;
	    strncpy(modify, lines[i], 255);
	    cp = index(modify, '=');
	    if (!cp)
		continue;
	    *(cp++) = '\0';
	    if (!strcmp(modify, v->name)) {
		free(lines[i]);
		lines[i] = (char *)malloc(strlen(v->name) + strlen(v->value) + 3);
		sprintf(lines[i], "%s=\"%s\"\n", v->name, v->value);
	    }

	}
    }
    fp = fopen("/etc/sysconfig", "w");
    if (!fp) {
	msgConfirm("Unable to re-write /etc/sysconfig file!  Things may work\n"
		   "rather strangely as a result of this.");
	return;
    }
    for (i = 0; i < nlines; i++) {
	fprintf(fp, lines[i]);
	free(lines[i]);

	/* Stand by for bogus special case handling - we try to dump the interface specs here */
	if (!strncmp(lines[i], VAR_INTERFACES, strlen(VAR_INTERFACES))) {
	    Device **devp;
	    int j, cnt;

	    devp = deviceFind(NULL, DEVICE_TYPE_NETWORK);
	    cnt = deviceCount(devp);
	    for (j = 0; j < cnt; j++) {
		if (devp[j]->private) {
		    char iname[64];

		    snprintf(iname, 64, "%s%s", VAR_IFCONFIG, devp[j]->name);
		    if (variable_get(iname))
			fprintf(fp, "%s=\"%s\"\n", iname, variable_get(iname));
		}
	    }
	}
    }
    fclose(fp);
}

int
configSaverTimeout(char *str)
{
    return variable_get_value("blanktime", "Enter time-out period in seconds for screen saver");
}

int
configNTP(char *str)
{
    return variable_get_value("ntpdate", "Enter the name of an NTP server");
}

void
configResolv(void)
{
    FILE *fp;
    char *cp;

    if (!RunningAsInit && file_readable("/etc/resolv.conf"))
	return;

    if (!variable_get(VAR_NAMESERVER)) {
	if (mediaDevice && (mediaDevice->type == DEVICE_TYPE_NFS || mediaDevice->type == DEVICE_TYPE_FTP))
	    msgConfirm("Warning:  Missing name server value - network operations\n"
		       "may fail as a result!");
	goto skip;
    }
    if (Mkdir("/etc", NULL)) {
	msgConfirm("Unable to create /etc directory.  Network configuration\n"
		   "files will therefore not be written!");
	return;
    }
    fp = fopen("/etc/resolv.conf", "w");
    if (!fp) {
	msgConfirm("Unable to open /etc/resolv.conf!  You will need to do this manually.");
	return;
    }
    if (variable_get(VAR_DOMAINNAME))
	fprintf(fp, "domain\t%s\n", variable_get(VAR_DOMAINNAME));
    fprintf(fp, "nameserver\t%s\n", variable_get(VAR_NAMESERVER));
    fclose(fp);
    if (isDebug())
	msgDebug("Wrote out /etc/resolv.conf\n");

skip:
    /* Tack ourselves at the end of /etc/hosts */
    cp = variable_get(VAR_IPADDR);
    if (cp && *cp != '0' && variable_get(VAR_HOSTNAME)) {
	fp = fopen("/etc/hosts", "a");
	fprintf(fp, "%s\t\t%s\n", cp, variable_get(VAR_HOSTNAME));
	fclose(fp);
	if (isDebug())
	    msgDebug("Appended entry for %s to /etc/hosts\n", cp);
    }
}

int
configRoutedFlags(char *str)
{
    return variable_get_value("routedflags", 
			      "Specify the flags for routed; -q is the default, -s is\n"
			      "a good choice for gateway machines.");
}

int
configPackages(char *str)
{
    PkgNode top, plist;
    int fd;

    if (!mediaVerify())
	return RET_FAIL;

    if (!mediaDevice->init(mediaDevice))
	return RET_FAIL;
    
    msgNotify("Attempting to fetch packages/INDEX file from selected media.");
    fd = mediaDevice->get(mediaDevice, "packages/INDEX", NULL);
    if (fd < 0) {
	msgNotify("Can't find packages/INDEX file - looking for ports/INDEX.");
	fd = mediaDevice->get(mediaDevice, "ports/INDEX", NULL);
	if (fd < 0) {
	    msgConfirm("Unable to get ports/INDEX file from selected media.\n"
		       "Please verify media (or path to media) and try again.");
	    return RET_FAIL;
	}
    }
    index_init(&top, &plist);
    if (index_read(fd, &top)) {
	msgConfirm("I/O or format error on packages/INDEX file.\n"
		   "Please verify media (or path to media) and try again.");
	mediaDevice->close(mediaDevice, fd);
	return RET_FAIL;
    }
    mediaDevice->close(mediaDevice, fd);
    index_sort(&top);
    while (1) {
	int ret, pos, scroll;

	/* Bring up the packages menu */
	pos = scroll = 0;
	index_menu(&top, &plist, &pos, &scroll);

	/* Now show the packling list menu */
	pos = scroll = 0;
	ret = index_menu(&plist, NULL, &pos, &scroll);
	if (ret == RET_DONE)
	    break;
	else if (ret != RET_FAIL) {
	    index_extract(mediaDevice, &plist);
	    break;
	}
    }
    index_node_free(&top, &plist);
    mediaDevice->shutdown(mediaDevice);
    return RET_SUCCESS;
}

int
configPorts(char *str)
{
    char *cp, *dist;

    if (file_executable("/usr/X11R6/bin/lndir")) {
	dist = "/cdrom/ports";
	while (!file_readable(dist)) {
	    dist = msgGetInput("/cdrom/ports",
			       "Unable to locate a ports tree on CDROM.  Please specify the\n"
			       "location of the master ports directory you wish to create the\n"
			       "link tree to.");
	    if (!dist)
		break;
	}
	if (dist) {
	    cp = msgGetInput("/usr/ports",
			     "Where would you like to create the link tree?"
			     "(press [ENTER] for default location).");
	    if (!cp)
		return RET_FAIL;
	    if (Mkdir(cp, NULL))
		msgConfirm("Unable to make %s directory!", cp);
	    else {
		msgNotify("Making link tree from %s to %s area.", dist, cp);
		if (vsystem("lndir %s %s", dist, cp))
		    msgConfirm("The lndir command returned an error status and may not have.\n"
			       "successfully generated the link tree.  You may wish to inspect\n"
			       "%s carefully for any signs of corruption.", cp);
	    }
	}
	else
	    return RET_FAIL;
    }
    else
	msgConfirm("You are missing the lndir command from /usr/X11R6/bin and\n"
		   "cannot run this utility.  You may wish to do this manually\n"
		   "later by extracting just lndir from the X distribution and\n"
		   "using it to create the link tree.");
    return RET_SUCCESS;
}
