/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated to essentially a complete rewrite.
 *
 * $Id: media_strategy.c,v 1.24 1995/05/26 10:20:47 jkh Exp $
 *
 * Copyright (c) 1995
 *	Jordan Hubbard.  All rights reserved.
 * Copyright (c) 1995
 * 	Gary J Palmer. All rights reserved.
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

#include <stdio.h>
#include "sysinstall.h"
#include <ctype.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/dkbad.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "ftp.h"

#define MSDOSFS
#define CD9660
#define NFS
#include <sys/mount.h>
#undef MSDOSFS
#undef CD9660
#undef NFS

#define MAX_ATTRIBS	200
#define MAX_NAME	511
#define MAX_VALUE	4095

struct attribs {
    char 		*name;
    char 		*value;
};

static int 		lno;
static int		num_attribs;

static int
attr_parse(struct attribs **attr, char *file)
{
    char hold_n[MAX_NAME+1];
    char hold_v[MAX_VALUE+1];
    int n, v, ch = 0;
    enum { LOOK, COMMENT, NAME, VALUE, COMMIT } state;
    FILE *fp;
    
    num_attribs = n = v = lno = 0;
    state = LOOK;
    
    if ((fp=fopen(file, "r")) == NULL)
    {
	msgConfirm("Cannot open the information file `%s': %s (%d)", file, strerror(errno), errno);
	return 0;
    }

    while (state == COMMIT || (ch = fgetc(fp)) != EOF) {
	/* Count lines */
	if (ch == '\n')
	    ++lno;
	switch(state) {
	case LOOK:
	    if (isspace(ch))
		continue;
	    /* Allow shell or lisp style comments */
	    else if (ch == '#' || ch == ';') {
		state = COMMENT;
		continue;
	    }
	    else if (isalpha(ch)) {
		hold_n[n++] = ch;
		state = NAME;
	    }
	    else
		msgFatal("Invalid character '%c' at line %d\n", ch, lno);
	    break;
	    
	case COMMENT:
	    if (ch == '\n')
		state = LOOK;
	    break;
	    
	case NAME:
	    if (ch == '\n') {
		hold_n[n] = '\0';
		hold_v[v = 0] = '\0';
		state = COMMIT;
	    }
	    else if (isspace(ch))
		continue;
	    else if (ch == '=') {
		hold_n[n] = '\0';
		state = VALUE;
	    }
	    else
		hold_n[n++] = ch;
	    break;
	    
	case VALUE:
	    if (v == 0 && isspace(ch))
		continue;
	    else if (ch == '{') {
		/* multiline value */
		while ((ch = fgetc(fp)) != '}') {
		    if (ch == EOF)
			msgFatal("Unexpected EOF on line %d", lno);
		    else {
		    	if (v == MAX_VALUE)
			    msgFatal("Value length overflow at line %d", lno);
		        hold_v[v++] = ch;
		    }
		}
		hold_v[v] = '\0';
		state = COMMIT;
	    }
	    else if (ch == '\n') {
		hold_v[v] = '\0';
		state = COMMIT;
	    }
	    else {
		if (v == MAX_VALUE)
		    msgFatal("Value length overflow at line %d", lno);
		else
		    hold_v[v++] = ch;
	    }
	    break;
	    
	case COMMIT:
	    (*attr)[num_attribs].name = strdup(hold_n);
	    (*attr)[num_attribs++].value = strdup(hold_v);
	    state = LOOK;
	    v = n = 0;
	    break;

	default:
	    msgFatal("Unknown state at line %d??\n", lno);
	}
    }
    fclose(fp);
    return 1;
}

static const char *
attr_match(struct attribs *attr, char *name)
{
    int n = 0;

    while((strcasecmp(attr[n].name, name)!=0) && (n < num_attribs) && (n < 20))
	n++;

    if (strcasecmp(attr[n].name, name)==0)
	return((const char *) attr[n].value);

    return NULL;
}

static pid_t getDistpid = 0;
static Device *floppyDev;

static int
floppyChoiceHook(char *str)
{
    Device **devs;

    /* Clip garbage off the ends */
    string_prune(str);
    str = string_skipwhite(str);
    if (!*str)
	return 0;
    devs = deviceFind(str, DEVICE_TYPE_FLOPPY);
    if (devs)
	floppyDev = devs[0];
    return devs ? 1 : 0;
}

int
genericGetDist(char *path, void *attrs, Boolean prompt)
{
    int 	fd;
    char 	buf[512];
    struct stat	sb;
    int		pfd[2], numchunks;
    const char *tmp;
    Device *devp;
    struct attribs *dist_attrib = (struct attribs *)attrs;

  top:
    fd = -1;
    /* Floppy is always last-ditch device */
    while (!mediaDevice && (prompt && floppyDev == NULL)) {
	Device **devs;
	int cnt;
		    
	devs = deviceFind(NULL, DEVICE_TYPE_FLOPPY);
	cnt = deviceCount(devs);
	if (cnt == 1)
	    devp = devs[0];
	else if (cnt > 1) {
	    DMenu *menu;
	    
	    menu = deviceCreateMenu(&MenuMediaFloppy, DEVICE_TYPE_FLOPPY, floppyChoiceHook);
	    menu->title = "Please insert the ROOT floppy";
	    dmenuOpenSimple(menu);
	}
	else {
	    msgConfirm("No floppy devices found!  Something is seriously wrong!");
	    return -1;
	}
	if (!floppyDev)
	    continue;
	fd = open(floppyDev->devname, O_RDONLY);
	if (fd != -1)
	    return fd;
	else
	    floppyDev = NULL;
    }
    if (stat(path, &sb) == 0)
    {
	fd = open(path, O_RDONLY, 0);
	return(fd);
    }

    snprintf(buf, 512, "%s.tgz", path);
    if (stat(buf, &sb) == 0)
    {
	fd = open(buf, O_RDONLY, 0);
	return(fd);
    }

    snprintf(buf, 512, "%s.aa", path);
    if (stat(buf, &sb) != 0 && !prompt)
    {
	msgConfirm("Cannot find file(s) for distribution in ``%s''!\n", path);
	return -1;
    }

    if (fd == -1 && prompt) {
	if (mediaDevice->shutdown)
	    (*mediaDevice->shutdown)(mediaDevice);

	if (mediaDevice->init)
	    if (!(*mediaDevice->init)(mediaDevice))
		return -1;
	msgConfirm("Please put distribution files for %s\nin %s and press return", path, mediaDevice->description);
	goto top;
    }

    if (dist_attrib) {
	tmp = attr_match(dist_attrib, "pieces");
	numchunks = atoi(tmp);
    }
    else
	numchunks = 1;

    /* reap the previous child corpse - yuck! */
    if (getDistpid) {
	int i, j;

	i = waitpid(getDistpid, &j, 0);
	if (i < 0 || WEXITSTATUS(j)) {
	    msgNotify("Warning: Previous extraction returned status code %d.", WEXITSTATUS(j));
	    getDistpid = 0;
	    return -1;
	}
	getDistpid = 0;
    }

    msgDebug("Attempting to concatenate %u chunks\n", numchunks);
    pipe(pfd);
    getDistpid = fork();
    if (!getDistpid) {
	caddr_t		memory;
	int		chunk;
	int		retval;

	dup2(pfd[1], 1); close(pfd[1]);
	close(pfd[0]);
	
	for (chunk = 0; chunk < numchunks; chunk++) {
	    int			fd;
	    unsigned long	len, val;
	    
	    retval = stat(buf, &sb);
	    if ((retval != 0) && (prompt != TRUE))
	    {
		msgConfirm("Cannot find file(s) for distribution in ``%s''!\n", path);
		return -1;
	    } else {
		char *tmp = index(buf, '/');
		tmp++;
		    
		while (retval != 0)
		{
		    msgConfirm("Please insert the disk with the `%s' file on it\n", tmp);
		    retval = stat(buf, &sb);
		}
	    }
	    
	    snprintf(buf, 512, "%s.%c%c", path, (chunk / 26) + 'a', (chunk % 26) + 'a');
	    if ((fd = open(buf, O_RDONLY)) == -1)
		msgFatal("Cannot find file `%s'!", buf);
	    
	    if (prompt == TRUE)
	    {
		extern int crc(int, unsigned long *, unsigned long *);

		crc(fd, &val, &len);
		msgDebug("crc for %s is %lu %lu\n", buf, val, len);
	    }
	    
	    fstat(fd, &sb);
	    msgDebug("mmap()ing %s (%d)\n", buf, fd);
	    memory = mmap(0, sb.st_size, PROT_READ, MAP_SHARED, fd, (off_t) 0);
	    if (memory == (caddr_t) -1)
		msgFatal("mmap error: %s\n", strerror(errno));
	
	    retval = write(1, memory, sb.st_size);
	    if (retval != sb.st_size)
	    {
		msgConfirm("write didn't write out the complete file!\n(wrote %d bytes of %d bytes)", retval,
			   sb.st_size);
		exit(1);
	    }
	    
	    retval = munmap(memory, sb.st_size);
	    if (retval != 0)
	    {
		msgConfirm("munmap() returned %d", retval);
		exit(1);
	    }
	    close(fd);
	}
	close(1);
	msgDebug("Extract of %s finished!!!\n", path);
	exit(0);
    }
    close(pfd[1]);
    return(pfd[0]);
}

/* Various media "strategy" routines */

static Boolean cdromMounted;

Boolean
mediaInitCDROM(Device *dev)
{
    struct iso_args	args;
    struct stat		sb;

    if (cdromMounted)
	return TRUE;

    if (Mkdir("/cdrom", NULL))
	return FALSE;

    args.fspec = dev->devname;
    args.flags = 0;

    if (mount(MOUNT_CD9660, "/cdrom", MNT_RDONLY, (caddr_t) &args) == -1)
    {
	msgConfirm("Error mounting %s on /cdrom: %s (%u)\n", dev, strerror(errno), errno);
	return FALSE;
    }

    /* Do a very simple check to see if this looks roughly like a 2.0.5 CDROM
       Unfortunately FreeBSD won't let us read the ``label'' AFAIK, which is one
       sure way of telling the disc version :-( */
    if (stat("/cdrom/dists", &sb))
    {
	if (errno == ENOENT)
	{
	    msgConfirm("Couldn't locate the directory `dists' on the CD.\nIs this a 2.0.5 CDROM?\n");
	    return FALSE;
	} else {
	    msgConfirm("Couldn't stat directory %s: %s", "/cdrom/dists", strerror(errno));
	    return FALSE;
	}
    }
    cdromMounted = TRUE;
    return TRUE;
}

int
mediaGetCDROM(char *dist, char *path)
{
    char		buf[PATH_MAX];
    struct attribs	*dist_attr;
    int			retval;

    dist_attr = safe_malloc(sizeof(struct attribs) * MAX_ATTRIBS);

    snprintf(buf, PATH_MAX, "/stand/info/%s.inf", dist);

    if (!access(buf, R_OK) && attr_parse(&dist_attr, buf) == 0)
    {
	msgConfirm("Cannot load information file for %s distribution!\nPlease verify that your media is valid and try again.", dist);
	free(dist_attr);
	return FALSE;
    }
   
    snprintf(buf, PATH_MAX, "/cdrom/%s%s", path ? path : "", dist);

    retval = genericGetDist(buf, dist_attr, FALSE);
    free(dist_attr);
    return retval;
}

void
mediaShutdownCDROM(Device *dev)
{
    
    msgDebug("In mediaShutdownCDROM\n");
    if (getDistpid) {
	int i, j;

	i = waitpid(getDistpid, &j, 0);
	if (i < 0 || WEXITSTATUS(j)) {
	    msgConfirm("Warning: Last extraction returned status code %d.", WEXITSTATUS(j));
	    getDistpid = 0;
	}
	getDistpid = 0;
    }
    if (unmount("/cdrom", 0) != 0)
	msgConfirm("Could not unmount the CDROM: %s\n", strerror(errno));
    msgDebug("Unmount returned\n");
    cdromMounted = FALSE;
    return;
}

static Boolean floppyMounted;

Boolean
mediaInitFloppy(Device *dev)
{
    struct ufs_args ufsargs;
    char mountpoint[FILENAME_MAX];

    if (floppyMounted)
	return TRUE;
    memset(&ufsargs,0,sizeof ufsargs);

    if (Mkdir("/mnt", NULL)) {
	msgConfirm("Unable to make directory mountpoint for %s!", mountpoint);
	return FALSE;
    }
    msgDebug("initFloppy:  mount floppy %s on /mnt\n", dev->devname); 
    ufsargs.fspec = dev->devname;
    if (mount(MOUNT_MSDOS, "/mnt", 0, (caddr_t)&ufsargs) == -1) {
	msgConfirm("Error mounting floppy %s (%s) on /mnt : %s\n", dev->name,
		   dev->devname, mountpoint, strerror(errno));
	return FALSE;
    }
    floppyMounted = TRUE;
    return TRUE;
}

int
mediaGetFloppy(char *dist, char *path)
{
    char		buf[PATH_MAX];
    char		*fname;
    struct attribs	*dist_attr;
    int			retval;

    dist_attr = safe_malloc(sizeof(struct attribs) * MAX_ATTRIBS);

    snprintf(buf, PATH_MAX, "/stand/info/%s.inf", dist);
    if (!access(buf, R_OK) && attr_parse(&dist_attr, buf) == 0)
    {
	msgConfirm("Cannot load information file for %s distribution!\nPlease verify that your media is valid and try again.", dist);
	free(dist_attr);
	return FALSE;
    }
    fname = index(dist, '/') + 1;
    snprintf(buf, PATH_MAX, "/mnt/%s", fname);

    retval = genericGetDist(buf, dist_attr, TRUE);
    free(dist_attr);
    return retval;
}

void
mediaShutdownFloppy(Device *dev)
{
    if (floppyMounted) {
	if (vsystem("umount /mnt") != 0)
	    msgDebug("Umount of floppy on /mnt failed: %s (%d)\n", strerror(errno), errno);
	else
	    floppyMounted = FALSE;
    }
}

Boolean
mediaInitTape(Device *dev)
{
    return TRUE;
}

static Boolean networkInitialized;

Boolean
mediaInitNetwork(Device *dev)
{
    int i;
    char *rp;

    if (networkInitialized)
	return TRUE;

    configResolv();
    if (!strncmp("cuaa", dev->name, 4)) {
	if (tcpStartPPP()) {
	    msgConfirm("You have selected a serial device as your network installation device.\nThe PPP dialer is now running on the 3rd screen (type ALT-F3 to interact\nwith it) and should be used to establish the link BEFORE YOU HIT RETURN\nhere!  Once you hit return in this screen (type ALT-F1 to return to this\nscreen from the PPP screen) the installation will assume that your link\nis set up and begin transfering the distributions over PPP.");
	}
	else {
	    msgConfirm("Unable to start PPP!  This installation method\ncannot be used.");
	    return FALSE;
	}
    }
    else {
	char *cp, ifconfig[64];

	snprintf(ifconfig, 64, "%s%s", VAR_IFCONFIG, dev->name);
	cp = getenv(ifconfig);
	if (!cp) {
	    msgConfirm("The %s device is not configured.  You will need to do so\nin the Networking configuration menu before proceeding.");
	    return FALSE;
	}
	i = vsystem("ifconfig %s %s", dev->name, cp);
	if (i) {
	    msgConfirm("Unable to configure the %s interface!\nThis installation method cannot be used.", dev->name);
	    return FALSE;
	}
    }

    rp = getenv(VAR_GATEWAY);
    if (!rp)
	msgConfirm("No gateway has been set. You will not be able to access hosts\n
not on the local network\n");
    else
	vsystem("route add default %s", rp);
    networkInitialized = TRUE;
    return TRUE;
}

int
mediaGetTape(char *dist, char *path)
{
    return -1;
}

void
mediaShutdownTape(Device *dev)
{
    return;
}

void
mediaShutdownNetwork(Device *dev)
{
    char *cp;

    if (!networkInitialized)
	return;

    if (!strncmp("cuaa", dev->name, 4)) {
	msgConfirm("You may now go to the 3rd screen (ALT-F3) and shut down\nyour PPP connection.  It shouldn't be needed any longer\n(unless you wish to create a shell by typing ESC and\nexperiment with it further, in which case go right ahead!)");
	return;
    }
    else {
	int i;
	char ifconfig[64];

	snprintf(ifconfig, 64, "%s%s", VAR_IFCONFIG, dev->name);
	cp = getenv(ifconfig);
	if (!cp)
	    return;
	i = vsystem("ifconfig %s down", dev->name);
	if (i)
	    msgConfirm("Warning: Unable to down the %s interface properly", dev->name);
    }

    cp = getenv(VAR_GATEWAY);
    if (cp)
	vsystem("route delete default");
    networkInitialized = FALSE;
}

static FTP_t ftp;

Boolean
mediaInitFTP(Device *dev)
{
    int i;
    char *url, *hostname, *dir;
    char *my_name, email[BUFSIZ];
    Device *netDevice = (Device *)dev->private;

    if (netDevice->init)
	if (!(*netDevice->init)(netDevice))
	    return FALSE;

    if ((ftp = FtpInit()) == NULL) {
	msgConfirm("FTP initialisation failed!");
	return FALSE;
    }

    url = getenv("ftp");
    if (!url)
	return FALSE;
    if (!strcmp(url, "other")) {
	url = msgGetInput("ftp://", "Please specify the URL of a FreeBSD distribution on a\nremote ftp site.  This site must accept anonymous ftp!\nA URL looks like this:  ftp://<hostname>/<path>");
	if (!url || strncmp("ftp://", url, 6))
	    return FALSE;
	else
	    variable_set2("ftp", url);
    }

    my_name = getenv(VAR_HOSTNAME);
    if (strncmp("ftp://", url, 6) != NULL) {
	msgConfirm("Invalid URL (`%s') passed to FTP routines!\n(must start with `ftp://')", url);
	return FALSE;
    }

    msgDebug("Using URL `%s'\n", url);
    hostname = url + 6;
    if ((dir = index(hostname, '/')) != NULL)
	*(dir++) = '\0';
    strcpy(dev->name, hostname);
    msgDebug("hostname = `%s'\n", hostname);
    msgDebug("dir = `%s'\n", dir ? dir : "/");
    msgNotify("Looking up host %s..", hostname);
    if ((gethostbyname(hostname) == NULL) && (inet_addr(hostname) == INADDR_NONE)) {
	msgConfirm("Cannot resolve hostname `%s'!  Are you sure your name server\nand/or gateway values are set properly?", hostname);
	return FALSE;
    }

    snprintf(email, BUFSIZ, "installer@%s", my_name);
    msgDebug("Using fake e-mail `%s'\n", email);

    msgNotify("Logging in as anonymous.");
    if ((i = FtpOpen(ftp, hostname, "anonymous", email)) != 0) {
	msgConfirm("Couldn't open FTP connection to %s: %s (%u)\n", hostname, strerror(i), i);
	return FALSE;
    }

    if (getenv("ftpPassive"))
	FtpPassive(ftp, 1);
    FtpBinary(ftp, 1);
    if (dir && *dir != '\0') {
	msgNotify("CD to distribution in ~ftp/%s", dir);
	FtpChdir(ftp, dir);
    }
    msgDebug("leaving mediaInitFTP!\n");
    return TRUE;
}

static pid_t ftppid = 0;

int
mediaGetFTP(char *dist, char *path)
{
    int 	fd;
    char 	buf[512];
    int		pfd[2], numchunks;
    const char *tmp;
    struct attribs	*dist_attr;

    msgNotify("Attempting to retreive `%s' over FTP", dist);
    snprintf(buf, PATH_MAX, "/stand/info/%s%s.inf", path ? path : "", dist);
    if (!access(buf, R_OK)) {
	msgDebug("Parsing attributes file for %s\n", dist);
	dist_attr = safe_malloc(sizeof(struct attribs) * MAX_ATTRIBS);
	if (attr_parse(&dist_attr, buf) == 0) {
	    msgConfirm("Cannot load information file for %s distribution!\nPlease verify that your media is valid and try again.", dist);
	    return -1;
	}
   
	msgDebug("Looking for attribute `pieces'\n");
	tmp = attr_match(dist_attr, "pieces");
	numchunks = atoi(tmp);
    }
    else
	numchunks = 0;
    msgDebug("Attempting to extract distribution from %u files\n", numchunks ? numchunks : 1);

    /* Take the lack of an info file to mean we're a fully qualified name */
    if (!numchunks) {
	sprintf(buf, "%s%s", path ? path : "", dist);
	return(FtpGet(ftp, buf));
    }
    else if (numchunks == 1) {
	snprintf(buf, 512, "%s.aa", dist);
	return(FtpGet(ftp, buf));
    }

    /* reap the previous child corpse - yuck! */
    if (ftppid) {
	int i, j;

	i = waitpid(ftppid, &j, 0);
	if (i < 0 || WEXITSTATUS(j)) {
	    msgConfirm("Previous FTP transaction returned status code %d - aborting\ntransfer.", WEXITSTATUS(j));
	    ftppid = 0;
	    return -1;
	}
	ftppid = 0;
    }
    pipe(pfd);
    ftppid = fork();
    if (!ftppid) {
	int		chunk;
	int		retval;

	dup2(pfd[1], 1); close(pfd[1]);
	close(pfd[0]);
	
	for (chunk = 0; chunk < numchunks; chunk++) {
	    char buffer[10240];
	    int n;
	    
	    snprintf(buf, 512, "%s.%c%c", dist, (chunk / 26) + 'a', (chunk % 26) + 'a');
	    fd = FtpGet(ftp, buf);

	    if (fd < 0)
	    {
		msgConfirm("FtpGet failed to retreive piece `%s' in the %s distribution!\nAborting the transfer", chunk, dist);
		exit(1);
	    }
	    
	    while ((n = read(fd, buffer, 10240))>0)
	    {
		retval = write(1, buffer, n);
		if (retval != n)
		{
		    msgConfirm("Write failure on transfer! (wrote %d bytes of %d bytes)", retval, n);
		    exit(1);
		}
		
	    }
	    FtpEOF(ftp);
	}
	close(1);
	msgDebug("Extract of %s finished with success!!!\n", dist);
	exit(0);
    }
    close(pfd[1]);
    return(pfd[0]);
}

void
mediaShutdownFTP(Device *dev)
{
    Device *netdev = (Device *)dev->private;

    if (ftp != NULL) {
	FtpClose(ftp);
	ftp = NULL;
    }
    if (ftppid) {
	int i, j;

	i = waitpid(ftppid, &j, 0);
	if (i < 0 || WEXITSTATUS(j))
	    msgConfirm("Warning: Last FTP transaction returned status code %d.", WEXITSTATUS(j));
	ftppid = 0;
    }
    if (netdev->shutdown)
	(*netdev->shutdown)(netdev);
}

Boolean
mediaInitUFS(Device *dev)
{
    return TRUE;
}

int
mediaGetUFS(char *dist, char *path)
{
    return -1;
}

/* UFS has no Shutdown routine since this is handled at the device level */


Boolean
mediaInitDOS(Device *dev)
{
    return TRUE;
}

int
mediaGetDOS(char *dist, char *path)
{
    return -1;
}

void
mediaShutdownDOS(Device *dev)
{
}
