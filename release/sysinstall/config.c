/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $FreeBSD: src/release/sysinstall/config.c,v 1.156.2.6 2000/09/24 06:55:55 jkh Exp $
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
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mount.h>

static Chunk *chunk_list[MAX_CHUNKS];
static int nchunks;
static int rootdev_is_od;

/* arg to sort */
static int
chunk_compare(Chunk *c1, Chunk *c2)
{
    if (!c1 && !c2)
	return 0;
    else if (!c1 && c2)
	return 1;
    else if (c1 && !c2)
	return -1;
    else if (!c1->private_data && !c2->private_data)
	return 0;
    else if (c1->private_data && !c2->private_data)
	return 1;
    else if (!c1->private_data && c2->private_data)
	return -1;
    else
	return strcmp(((PartInfo *)(c1->private_data))->mountpoint, ((PartInfo *)(c2->private_data))->mountpoint);
}

static void
chunk_sort(void)
{
    int i, j;

    for (i = 0; i < nchunks; i++) {
	for (j = 0; j < nchunks; j++) {
	    if (chunk_compare(chunk_list[j], chunk_list[j + 1]) > 0) {
		Chunk *tmp = chunk_list[j];

		chunk_list[j] = chunk_list[j + 1];
		chunk_list[j + 1] = tmp;
	    }
	}
    }
}

static void
check_rootdev(Chunk **list, int n)
{
	int i;
	Chunk *c;

	rootdev_is_od = 0;
	for (i = 0; i < n; i++) {
		c = *list++;
		if (c->type == part && (c->flags & CHUNK_IS_ROOT)
		    && strncmp(c->disk->name, "od", 2) == 0)
			rootdev_is_od = 1;
	}
}

static char *
name_of(Chunk *c1)
{
    return c1->name;
}

static char *
mount_point(Chunk *c1)
{
    if (c1->type == part && c1->subtype == FS_SWAP)
	return "none";
    else if (c1->type == part || c1->type == fat)
	return ((PartInfo *)c1->private_data)->mountpoint;
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
	if (c1->subtype != FS_SWAP) {
	    if (rootdev_is_od == 0 && strncmp(c1->name, "od", 2) == 0)
		return "rw,noauto";
	    else
		return "rw";
	}
	else
	    return "sw";
    }
    else if (c1->type == fat) {
	if (strncmp(c1->name, "od", 2) == 0)
	    return "ro,noauto";
	else
	    return "ro";
    }
    return "bog";
}

static int
seq_num(Chunk *c1)
{
    if (c1->type == part && c1->subtype != FS_SWAP) {
	if (rootdev_is_od == 0 && strncmp(c1->name, "od", 2) == 0)
	    return 0;
	else if (c1->flags & CHUNK_IS_ROOT)
	    return 1;
	else
	    return 2;
    }
    return 0;
}

int
configFstab(dialogMenuItem *self)
{
    Device **devs;
    Disk *disk;
    FILE *fstab;
    int i, cnt;
    Chunk *c1, *c2;

    if (!RunningAsInit) {
	if (file_readable("/etc/fstab"))
	    return DITEM_SUCCESS;
	else {
	    msgConfirm("Attempting to rebuild your /etc/fstab file.  Warning: If you had\n"
		       "any CD devices in use before running sysinstall then they may NOT\n"
		       "be found by this run!");
	}
    }

    devs = deviceFind(NULL, DEVICE_TYPE_DISK);
    if (!devs) {
	msgConfirm("No disks found!");
	return DITEM_FAILURE;
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
		    if (c2->type == part && (c2->subtype == FS_SWAP || c2->private_data))
			chunk_list[nchunks++] = c2;
		}
	    }
	    else if (c1->type == fat && c1->private_data)
		chunk_list[nchunks++] = c1;
	}
    }
    chunk_list[nchunks] = 0;
    chunk_sort();
    
    fstab = fopen("/etc/fstab", "w");
    if (!fstab) {
	msgConfirm("Unable to create a new /etc/fstab file!  Manual intervention\n"
		   "will be required.");
	return DITEM_FAILURE;
    }
    
    check_rootdev(chunk_list, nchunks);
    
    /* Go for the burn */
    msgDebug("Generating /etc/fstab file\n");
    fprintf(fstab, "# Device\t\tMountpoint\tFStype\tOptions\t\tDump\tPass#\n");
    for (i = 0; i < nchunks; i++)
	fprintf(fstab, "/dev/%s\t\t%s\t\t%s\t%s\t\t%d\t%d\n", name_of(chunk_list[i]), mount_point(chunk_list[i]),
		fstype(chunk_list[i]), fstype_short(chunk_list[i]), seq_num(chunk_list[i]), seq_num(chunk_list[i]));
    
    /* Now look for the CDROMs */
    devs = deviceFind(NULL, DEVICE_TYPE_CDROM);
    cnt = deviceCount(devs);
    
    /* Write out the CDROM entries */
    for (i = 0; i < cnt; i++) {
	char cdname[10];
	
	sprintf(cdname, "/cdrom%s", i ? itoa(i) : "");
	if (Mkdir(cdname))
	    msgConfirm("Unable to make mount point for: %s", cdname);
	else
	    fprintf(fstab, "/dev/%s\t\t%s\t\tcd9660\tro,noauto\t0\t0\n", devs[i]->name, cdname);
    }
    
    /* And finally, a /proc. */
    fprintf(fstab, "proc\t\t\t/proc\t\tprocfs\trw\t\t0\t0\n");
    Mkdir("/proc");

    fclose(fstab);
    if (isDebug())
	msgDebug("Wrote out /etc/fstab file\n");
    return DITEM_SUCCESS;
}

/* Do the work of sucking in a config file.
 * config is the filename to read in.
 * lines is a fixed (max) sized array of char*
 * returns number of lines read.  line contents
 * are malloc'd and must be freed by the caller.
 */
int
readConfig(char *config, char **lines, int max)
{
    FILE *fp;
    char line[256];
    int i, nlines;

    fp = fopen(config, "r");
    if (!fp)
	return -1;

    nlines = 0;
    /* Read in the entire file */
    for (i = 0; i < max; i++) {
	if (!fgets(line, sizeof line, fp))
	    break;
	lines[nlines++] = strdup(line);
    }
    fclose(fp);
    if (isDebug())
	msgDebug("readConfig: Read %d lines from %s.\n", nlines, config);
    return nlines;
}

#define MAX_LINES  2000 /* Some big number we're not likely to ever reach - I'm being really lazy here, I know */

static void
readConfigFile(char *config, int marked)
{
    char *lines[MAX_LINES], *cp, *cp2;
    int i, nlines;

    nlines = readConfig(config, lines, MAX_LINES);
    if (nlines == -1)
	return;

    for (i = 0; i < nlines; i++) {
	/* Skip the comments & non-variable settings */
	if (lines[i][0] == '#' || !(cp = index(lines[i], '='))) {
	    free(lines[i]);
	    continue;
	}
	*cp++ = '\0';
	/* Find quotes */
	if ((cp2 = index(cp, '"')) || (cp2 = index(cp, '\047'))) {
	    cp = cp2 + 1;
	    cp2 = index(cp, *cp2);
	}
	/* If valid quotes, use it */
	if (cp2) {
	    *cp2 = '\0';
 	    /* If we have a legit value, set it */
	    if (strlen(cp))
		variable_set2(lines[i], cp, marked);
	}
	free(lines[i]);
    }
}

/* Load the environment from rc.conf file(s) */
void
configEnvironmentRC_conf(void)
{
    static struct {
	char *fname;
	int marked;
    } configs[] = {
	{ "/etc/defaults/rc.conf", 0 },
	{ "/etc/rc.conf", 0 },
	{ "/etc/rc.conf.local", 0 },
	{ NULL, 0 },
    };
    int i;

    for (i = 0; configs[i].fname; i++) {
	if (file_readable(configs[i].fname))
	    readConfigFile(configs[i].fname, configs[i].marked);
    }
}

/* Load the environment from a resolv.conf file */
void
configEnvironmentResolv(char *config)
{
    char *lines[MAX_LINES];
    int i, nlines;

    nlines = readConfig(config, lines, MAX_LINES);
    if (nlines == -1)
	return;
    for (i = 0; i < nlines; i++) {
	Boolean name_set = (Boolean)variable_get(VAR_NAMESERVER);

	if (!strncmp(lines[i], "domain", 6) && !variable_get(VAR_DOMAINNAME))
	    variable_set2(VAR_DOMAINNAME, string_skipwhite(string_prune(lines[i] + 6)), 0);
	else if (!name_set && !strncmp(lines[i], "nameserver", 10)) {
	    /* Only take the first nameserver setting - we're lame */
	    variable_set2(VAR_NAMESERVER, string_skipwhite(string_prune(lines[i] + 10)), 0);
	}
	free(lines[i]);
    }
}

/* Version of below for dispatch routines */
int
configRC(dialogMenuItem *unused)
{
    configRC_conf();
    return DITEM_SUCCESS;
}

void
configRC_conf(void)
{
    FILE *rcSite;
    Variable *v;
    int write_header;
    static int did_marker = 0;

    write_header = !file_readable("/etc/rc.conf");
    rcSite = fopen("/etc/rc.conf", "a");
    if (!rcSite)
	return;
    if (write_header) {
	fprintf(rcSite, "# This file now contains just the overrides from /etc/defaults/rc.conf\n");
	fprintf(rcSite, "# please make all changes to this file.\n\n");
	fprintf(rcSite, "# Enable network daemons for user convenience.\n");
    }

    /* Now do variable substitutions */
    for (v = VarHead; v; v = v->next) {
	if (v->dirty) {
	    if (!did_marker) {
		fprintf(rcSite, "# -- sysinstall generated deltas -- #\n");
		did_marker = 1;
	    }
	    fprintf(rcSite, "%s=\"%s\"\n", v->name, v->value);
	    v->dirty = 0;
	}
    }
    fclose(rcSite);
}

int
configSaver(dialogMenuItem *self)
{
    variable_set((char *)self->data, 1);
    if (!variable_get(VAR_BLANKTIME))
	variable_set2(VAR_BLANKTIME, "300", 1);
    return DITEM_SUCCESS;
}

int
configSaverTimeout(dialogMenuItem *self)
{
    return (variable_get_value(VAR_BLANKTIME,
	    "Enter time-out period in seconds for screen saver", 1) ?
	DITEM_SUCCESS : DITEM_FAILURE);
}

int
configNTP(dialogMenuItem *self)
{
    int status;

    status = variable_get_value(VAR_NTPDATE_FLAGS,
				"Enter the name of an NTP server", 1)
	     ? DITEM_SUCCESS : DITEM_FAILURE;
    if (status == DITEM_SUCCESS) {
	static char tmp[255];

	snprintf(tmp, sizeof(tmp), "ntpdate_enable=YES,ntpdate_flags=%s",
		 variable_get(VAR_NTPDATE_FLAGS));
	self->data = tmp;
	dmenuSetVariables(self);
    }
    return status;
}

int
configUsers(dialogMenuItem *self)
{
    WINDOW *w = savescr();

    dialog_clear_norefresh();
    dmenuOpenSimple(&MenuUsermgmt, FALSE); 
    restorescr(w);
    return DITEM_SUCCESS;
}

int
configLinux(dialogMenuItem *self)
{
    WINDOW *w = savescr();
    int i;

    dialog_clear_norefresh();
    variable_set2(VAR_LINUX_ENABLE, "YES", 1);
    Mkdir("/compat/linux");
    msgNotify("Installing Linux compatibility library...");
    i = package_add("linux_base");
    restorescr(w);
    return i;
}

int
configSecurityProfile(dialogMenuItem *self)
{
    WINDOW *w = savescr();

    dialog_clear_norefresh();
    dmenuOpenSimple(&MenuSecurityProfile, FALSE); 
    restorescr(w);
    return DITEM_SUCCESS;
}

/* Use the most fascist security settings */
int
configSecurityFascist(dialogMenuItem *self)
{
    WINDOW *w = savescr();

    variable_set2("inetd_enable", "NO", 1);
    variable_set2("portmap_enable", "NO", 1);
    variable_set2("sendmail_enable", "NO", 1);
    variable_set2("sshd_enable", "NO", 1);
    variable_set2("nfs_server_enable", "NO", 1);
    variable_set2("kern_securelevel_enable", "YES", 1);
    variable_set2("kern_securelevel", "2", 1);
    /* More fascist stuff should go here */

    if (self)
	msgConfirm("High security settings have been selected.\n\n"
	    "This means that most \"popular\" network services and\n"
	    "mechanisms like inetd(8) have been DISABLED by default.\n\n"
	    "PLEASE NOTE that this still does not save you from having\n"
	    "to properly secure your system in other ways or exercise\n"
	    "due diligence in your administration, this simply picks\n"
	    "a more secure set of out-of-box defaults to start with.\n\n"
	    "To change any of these settings later, edit /etc/rc.conf");

    restorescr(w);
    return DITEM_SUCCESS;
}

int
configSecurityModerate(dialogMenuItem *self)
{
    WINDOW *w = savescr();

    variable_set2("inetd_enable", "YES", 1);
    if (!variable_cmp("nfs_client_enable", "YES") ||
	!variable_cmp("nfs_server_enable", "YES"))
    	variable_set2("portmap_enable", "YES", 1);
    if (!variable_cmp("nfs_server_enable", "YES"))
	variable_set2("nfs_reserved_port_only", "YES", 1);
    variable_set2("sendmail_enable", "YES", 1);
    variable_set2("sshd_enable", "YES", 1);

    if (self)
	msgConfirm("Moderate security settings have been selected.\n\n"
	    "This means that most \"popular\" network services and\n"
	    "mechanisms like inetd(8) have been enabled by default\n"
	    "for a comfortable user experience but with possible\n"
	    "trade-offs in system security.  If this bothers you and\n"
	    "you know exactly what you are doing, select the high\n"
	    "high security profile instead.\n\n"
	    "To change any of these settings later, edit /etc/rc.conf");

    restorescr(w);
    return DITEM_SUCCESS;
}

int
configSecurityLiberal(dialogMenuItem *self)
{
    WINDOW *w = savescr();

    variable_set2("inetd_enable", "YES", 1);
    variable_set2("portmap_enable", "YES", 1);
    variable_set2("sendmail_enable", "YES", 1);
    variable_set2("sshd_enable", "YES", 1);

    if (self)
	msgConfirm("Liberal security settings have been selected.\n\n"
	    "This means that most \"popular\" network services and\n"
	    "mechanisms like inetd(8) have been enabled by default\n"
	    "for the most comfortable user experience but with possible\n"
	    "trade-offs in system security.  If this bothers you, select\n"
	    "the medium security profile or, if you know exactly what you\n"
	    "are doing, the high security profile instead.\n\n"
	    "To change any of these settings later, edit /etc/rc.conf");

    restorescr(w);
    return DITEM_SUCCESS;
}

static void
write_root_xprofile(char *str)
{
    FILE *fp;
    int len;
    char **cp;
    static char *flist[] = { /* take care of both xdm and startx */
	"/root/.xinitrc",
	"/root/.xsession",
	"/usr/share/skel/dot.xinitrc",
	"/usr/share/skel/dot.xsession",
	NULL,
    };

    len = strlen(str);
    for (cp = flist; *cp; cp++) {
	fp = fopen(*cp, "w");
	if (fp) {
	    fwrite(str, 1, len, fp);
	    fchmod(fileno(fp), 0755);
	    fclose(fp);
	}
    }
}

static int
gotit(char *fname)
{
    char tmp[FILENAME_MAX];

    snprintf(tmp, sizeof tmp, "/usr/X11R6/bin/%s", fname);
    if (file_executable(tmp))
	return TRUE;
    snprintf(tmp, sizeof tmp, "/usr/local/bin/%s", fname);
    return file_executable(tmp);
}

int
configXDesktop(dialogMenuItem *self)
{
    char *desk;
    int ret = DITEM_SUCCESS;
    WINDOW *w = savescr();
    
    dialog_clear_norefresh();
    if (!dmenuOpenSimple(&MenuXDesktops, FALSE) || !(desk = variable_get(VAR_DESKSTYLE))) {
	restorescr(w);
	return DITEM_FAILURE;
    }
    if (!strcmp(desk, "kde")) {
	ret = package_add("kde");
	if (DITEM_STATUS(ret) != DITEM_FAILURE && gotit("startkde"))
	    write_root_xprofile("exec startkde\n");
    }
    else if (!strcmp(desk, "gnome")) {
	ret = package_add("gnomecore");
	if (DITEM_STATUS(ret) != DITEM_FAILURE && gotit("gnome-session")) {
	    ret = package_add("afterstep");
	    if (DITEM_STATUS(ret) != DITEM_FAILURE && gotit("afterstep"))
		write_root_xprofile("gnome-session &\nexec afterstep");
	}
    }
    else if (!strcmp(desk, "enlightenment")) {
	ret = package_add("gnomecore");
	if (DITEM_STATUS(ret) != DITEM_FAILURE && gotit("gnome-session")) {
	    ret = package_add("enlightenment");
	    if (DITEM_STATUS(ret) != DITEM_FAILURE && gotit("enlightenment"))
		write_root_xprofile("exec gnome-session\n");
	}
    }
    else if (!strcmp(desk, "afterstep")) {
	ret = package_add("afterstep");
	if (DITEM_STATUS(ret) != DITEM_FAILURE && gotit("afterstep"))
	    write_root_xprofile("xterm &\nexec afterstep\n");
    }
    else if (!strcmp(desk, "windowmaker")) {
	ret = package_add("windowmaker");
	if (DITEM_STATUS(ret) != DITEM_FAILURE && gotit("wmaker.inst")) {
	    write_root_xprofile("xterm &\n[ ! -d $HOME/GNUstep/Library/WindowMaker ] && /usr/X11R6/bin/wmaker.inst\nexec /usr/X11R6/bin/wmaker\n");
	}
    }
    else if (!strcmp(desk, "fvwm2")) {
	ret = package_add("fvwm");
	if (DITEM_STATUS(ret) != DITEM_FAILURE && gotit("fvwm2"))
	    write_root_xprofile("xterm &\nexec fvwm2\n");
    }
    if (DITEM_STATUS(ret) == DITEM_FAILURE)
	msgConfirm("An error occurred while adding the package(s) required\n"
		   "by this desktop type.  Please change installation media\n"
		   "and/or select a different, perhaps simpler, desktop\n"
		   "environment and try again.");
    restorescr(w);
    return ret;
}

int
configXSetup(dialogMenuItem *self)
{
    char *config, *execfile, *style;
    char *moused;
    WINDOW *w = savescr();
    
    setenv("XWINHOME", "/usr/X11R6", 1);
tryagain:
    variable_unset(VAR_DESKSTYLE);
    variable_unset(VAR_XF86_CONFIG);
    dialog_clear_norefresh();
    if (!dmenuOpenSimple(&MenuXF86Config, FALSE)) {
	restorescr(w);
	return DITEM_FAILURE;
    }
    config = variable_get(VAR_XF86_CONFIG);
    style = variable_get(VAR_DESKSTYLE);
    if (!config) {
	if (style)
	    goto config_desktop;
	else {
	    restorescr(w);
	    return DITEM_FAILURE;
	}
    }

    if (file_readable("/var/run/ld-elf.so.hints"))
	vsystem("/sbin/ldconfig -m /usr/lib /usr/X11R6/lib /usr/local/lib /usr/lib/compat");
    else
	vsystem("/sbin/ldconfig /usr/lib /usr/X11R6/lib /usr/local/lib /usr/lib/compat");
    if (file_readable("/var/run/ld.so.hints"))
	vsystem("ldconfig -m -aout /usr/lib/aout /usr/lib/compat/aout /usr/local/lib/aout /usr/X11R6/lib/aout");
    else
	vsystem("ldconfig -aout /usr/lib/aout /usr/lib/compat/aout /usr/local/lib/aout /usr/X11R6/lib/aout");

    vsystem("/sbin/ifconfig lo0 127.0.0.1");
    execfile = string_concat("/usr/X11R6/bin/", config);
    if (file_executable(execfile)) {
	moused = variable_get(VAR_MOUSED);
	while (!moused || strcmp(moused, "YES")) {
	    if (msgYesNo("The X server may access the mouse in two ways: direct access\n"
			 "or indirect access via the mouse daemon.  You have not\n"
			 "configured the mouse daemon.  Would you like to configure it\n"
			 "now?  If you intend to let the X server access the mouse\n"
			 "directly, choose \"No\" at this time."))
		break;
	    dialog_clear_norefresh();
	    dmenuOpenSimple(&MenuMouse, FALSE); 
	    moused = variable_get(VAR_MOUSED);
	}
	if (moused && !strcmp(moused, "YES"))
	    msgConfirm("You have configured and are now running the mouse daemon.\n"
	   	       "Choose \"/dev/sysmouse\" as the mouse port and \"SysMouse\" or\n"
		       "\"MouseSystems\" as the mouse protocol in the X configuration\n"
		       "utility.");
	systemExecute(execfile);
	if (!file_readable("/etc/XF86Config")) {
	    if (!msgYesNo("The XFree86 configuration process seems to have\nfailed.  Would you like to try again?"))
		goto tryagain;
	    else {
		restorescr(w);
		return DITEM_FAILURE;
	    }
	}
config_desktop:
	configXDesktop(self);
	restorescr(w);
	return DITEM_SUCCESS;
    }
    else {
	msgConfirm("The XFree86 setup utility you chose does not appear to be installed!\n"
		   "Please install this before attempting to configure XFree86.");
	restorescr(w);
	return DITEM_FAILURE;
    }
}

int
configResolv(dialogMenuItem *ditem)
{
    FILE *fp;
    char *cp, *c6p, *dp, *hp;

    cp = variable_get(VAR_NAMESERVER);
    if (!cp || !*cp)
	goto skip;
    Mkdir("/etc");
    fp = fopen("/etc/resolv.conf", "w");
    if (!fp)
	return DITEM_FAILURE;
    if (variable_get(VAR_DOMAINNAME))
	fprintf(fp, "domain\t%s\n", variable_get(VAR_DOMAINNAME));
    fprintf(fp, "nameserver\t%s\n", cp);
    fclose(fp);
    if (isDebug())
	msgDebug("Wrote out /etc/resolv.conf\n");

skip:
    dp = variable_get(VAR_DOMAINNAME);
    cp = variable_get(VAR_IPADDR);
    c6p = variable_get(VAR_IPV6ADDR);
    hp = variable_get(VAR_HOSTNAME);
    /* Tack ourselves into /etc/hosts */
    fp = fopen("/etc/hosts", "w");
    if (!fp)
	return DITEM_FAILURE;
    /* Add an entry for localhost */
    if (!variable_cmp(VAR_IPV6_ENABLE, "YES")) {
	if (dp)
	    fprintf(fp, "::1\t\t\tlocalhost.%s localhost\n", dp);
	else
	    fprintf(fp, "::1\t\t\tlocalhost\n");
    }
    if (dp)
	fprintf(fp, "127.0.0.1\t\tlocalhost.%s localhost\n", dp);
    else
	fprintf(fp, "127.0.0.1\t\tlocalhost\n");
    /* Now the host entries, if applicable */
    if (((cp && cp[0] != '0') || (c6p && c6p[0] != '0')) && hp) {
	char cp2[255];

	if (!index(hp, '.'))
	    cp2[0] = '\0';
	else {
	    SAFE_STRCPY(cp2, hp);
	    *(index(cp2, '.')) = '\0';
	}
	if (c6p && c6p[0] != '0') {
	    fprintf(fp, "%s\t%s %s\n", c6p, hp, cp2);
	    fprintf(fp, "%s\t%s.\n", c6p, hp);
	}
	if (cp && cp[0] != '0') {
	    fprintf(fp, "%s\t\t%s %s\n", cp, hp, cp2);
	    fprintf(fp, "%s\t\t%s.\n", cp, hp);
	}
    }
    fclose(fp);
    if (isDebug())
	msgDebug("Wrote out /etc/hosts\n");
    return DITEM_SUCCESS;
}

int
configRouter(dialogMenuItem *self)
{
    int ret;

    ret = variable_get_value(VAR_ROUTER,
			     "Please specify the router you wish to use.  Routed is\n"
			     "provided with the stock system and gated is provided\n"
			     "as an optional package which this installation system\n"
			     "will attempt to load if you select gated.  Any other\n"
			     "choice of routing daemon will be assumed to be something\n"
			     "the user intends to install themselves before rebooting\n"
			     "the system.  If you don't want any routing daemon, choose NO", 1)
      ? DITEM_SUCCESS : DITEM_FAILURE;
  
    if (ret == DITEM_SUCCESS) {
	char *cp = variable_get(VAR_ROUTER);
    
	if (cp && strcmp(cp, "NO")) {
	    variable_set2(VAR_ROUTER_ENABLE, "YES", 1);
	    if (!strcmp(cp, "gated")) {
		if (package_add("gated") != DITEM_SUCCESS) {
		    msgConfirm("Unable to load gated package.  Falling back to no router.");
		    variable_unset(VAR_ROUTER);
		    variable_unset(VAR_ROUTERFLAGS);
		    variable_set2(VAR_ROUTER_ENABLE, "NO", 1);
		    cp = NULL;
		}
	    }
	    if (cp) {
		/* Now get the flags, if they chose a router */
		ret = variable_get_value(VAR_ROUTERFLAGS, 
					 "Please Specify the routing daemon flags; if you're running routed\n"
					 "then -q is the right choice for nodes and -s for gateway hosts.\n", 1)
		  ? DITEM_SUCCESS : DITEM_FAILURE;
		if (ret != DITEM_SUCCESS)
		    variable_unset(VAR_ROUTERFLAGS);
	    }
	}
	else {
	    /* No router case */
	    variable_set2(VAR_ROUTER_ENABLE, "NO", 1);
	    variable_unset(VAR_ROUTERFLAGS);
	    variable_unset(VAR_ROUTER);
	}
    }
    else {
	variable_set2(VAR_ROUTER_ENABLE, "NO", 1);
	variable_unset(VAR_ROUTERFLAGS);
	variable_unset(VAR_ROUTER);
    }
    return ret;
}

/* Shared between us and index_initialize() */
extern PkgNode Top, Plist;

int
configPackages(dialogMenuItem *self)
{
    int i, restoreflag = 0;
    PkgNodePtr tmp;

    /* Did we get an INDEX? */
    i = index_initialize("packages/INDEX");
    if (DITEM_STATUS(i) == DITEM_FAILURE)
	return i;

    while (1) {
	int ret, pos, scroll;

	/* Bring up the packages menu */
	pos = scroll = 0;
	index_menu(&Top, &Top, &Plist, &pos, &scroll);

	if (Plist.kids && Plist.kids->name) {
	    /* Now show the packing list menu */
	    pos = scroll = 0;
	    ret = index_menu(&Plist, &Plist, NULL, &pos, &scroll);
	    if (ret & DITEM_LEAVE_MENU)
		break;
	    else if (DITEM_STATUS(ret) != DITEM_FAILURE) {
		dialog_clear();
		restoreflag = 1;
		for (tmp = Plist.kids; tmp && tmp->name; tmp = tmp->next)
		    (void)index_extract(mediaDevice, &Top, tmp, FALSE);
		break;
	    }
	}
	else {
	    msgConfirm("No packages were selected for extraction.");
	    break;
	}
    }
    tmp = Plist.kids;
    while (tmp) {
        PkgNodePtr tmp2 = tmp->next;
           
        safe_free(tmp);
        tmp = tmp2;
    }
    index_init(NULL, &Plist);
    return DITEM_SUCCESS | (restoreflag ? DITEM_RESTORE : 0);
}

/* Load pcnfsd package */
int
configPCNFSD(dialogMenuItem *self)
{
    int ret;

    ret = package_add("pcnfsd");
    if (DITEM_STATUS(ret) == DITEM_SUCCESS) {
	variable_set2(VAR_PCNFSD, "YES", 0);
	variable_set2("mountd_flags", "-n", 1);
    }
    return ret;
}

int
configNFSServer(dialogMenuItem *self)
{
    char cmd[256];

    /* If we're an NFS server, we need an exports file */
    if (!file_readable("/etc/exports")) {
	WINDOW *w = savescr();

	if (file_readable("/etc/exports.disabled"))
	    vsystem("mv /etc/exports.disabled /etc/exports");
	else {
	    dialog_clear_norefresh();
	    msgConfirm("Operating as an NFS server means that you must first configure\n"
		       "an /etc/exports file to indicate which hosts are allowed certain\n"
		       "kinds of access to your local file systems.\n"
		       "Press [ENTER] now to invoke an editor on /etc/exports\n");
	    vsystem("echo '#The following examples export /usr to 3 machines named after ducks,' > /etc/exports");
	    vsystem("echo '#/home and all directories under it to machines named after dead rock stars' >> /etc/exports");
	    vsystem("echo '#and, finally, /a to 2 privileged machines allowed to write on it as root.' >> /etc/exports");
	    vsystem("echo '#/usr                huey louie dewie' >> /etc/exports");
	    vsystem("echo '#/home   -alldirs    janice jimmy frank' >> /etc/exports");
	    vsystem("echo '#/a      -maproot=0  bill albert' >> /etc/exports");
	    vsystem("echo '#' >> /etc/exports");
	    vsystem("echo '# You should replace these lines with your actual exported filesystems.' >> /etc/exports");
	    vsystem("echo >> /etc/exports");
	    sprintf(cmd, "%s /etc/exports", variable_get(VAR_EDITOR));
	    dialog_clear();
	    systemExecute(cmd);
	}
	variable_set2(VAR_NFS_SERVER, "YES", 1);
	restorescr(w);
    }
    else if (variable_get(VAR_NFS_SERVER)) { /* We want to turn it off again? */
	vsystem("mv -f /etc/exports /etc/exports.disabled");
	variable_unset(VAR_NFS_SERVER);
    }
    return DITEM_SUCCESS;
}
