/***************************************************
 * file: userconfig/uc_main.c
 *
 * Copyright (c) 1996 Eric L. Hernes (erich@rrnet.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
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
 * library functions for userconfig library
 *
 * $Id: uc_main.c,v 1.13.4.2 1997/01/15 04:50:22 jkh Exp $
 */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <paths.h>
#include <sys/mman.h>
#include <nlist.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "uc_main.h"
#include "sysinstall.h"

static struct nlist _nl[] = {
    {"_isa_devtab_bio"},
    {"_isa_devtab_tty"},
    {"_isa_devtab_net"},
    {"_isa_devtab_null"},
    {"_isa_biotab_wdc"},
    {"_isa_biotab_fdc"},
    {"_eisadriver_set"},
    {"_eisa_dev_list"},
    {"_pcidevice_set"},
    {"_device_list"},
    {"_scbusses"},
    {"_scsi_cinit"},
    {"_scsi_dinit"},
    {"_scsi_tinit"},
    {""},
};

struct kernel *
uc_open(char *name){
    int kd, flags, incore;
    struct kernel *kern;
    struct stat sb;
    char kname[80];
    int size, i = 0;
    struct nlist *nl = _nl;

    if (strcmp(name, "-incore") == 0)
	incore = 1;
    else
	incore = 0;
    
    if (incore || (strcmp(name,"-bootfile") == 0))
	SAFE_STRCPY(kname, getbootfile());
    else
	SAFE_STRCPY(kname, name);

    if (isDebug())
	msgDebug("uc_open: kernel name is %s, incore = %d\n", kname, incore);
    kern = (struct kernel *)malloc(sizeof(struct kernel));

#ifdef KERN_NO_SYMBOLS
    if (incore) {
	FILE *fp;

	fp = fopen("/stand/symbols", "r");
	if (!fp) {
	    msgDebug("Couldn't open /stand/symbols file!  Punting.\n");
	    free(kern);
	    return NULL;
	}
	if (fscanf(fp, "%d\n", &size) != 1) {
	    msgDebug("Unable to get # of name list entries from symbol file.\n");
	    free(kern);
	    return NULL;
	}
	else if (isDebug())
	    msgDebug("uc_open: opened /stand/symbols file, reading %d entries.\n", size);


	kern->nl = nl = (struct nlist *)malloc((size + 1) * sizeof(struct nlist));
	bzero(nl, (size + 1) * sizeof(struct nlist));
	for (i = 0; i < size; i++) {
	    char *cp, name[255];
	    int c1;
	    unsigned int uc1;
	    short d1;
	    unsigned long v1;

	    if (fgets(name, 255, fp) == NULL) {
		msgDebug("Can't get name field for entry %d\n", i);
		free(kern);
		return NULL;
	    }
	    if ((cp = index(name, '\n')) != NULL)
		*cp = '\0';
	    nl[i].n_name = strdup(name);
	    if (fscanf(fp, "%u %d %hd %ld\n", &uc1, &c1, &d1, &v1) == 4) {
		nl[i].n_type = (unsigned char)uc1;
		nl[i].n_other = (char)c1;
		nl[i].n_desc = d1;
		nl[i].n_value = v1;
		if (isDebug())
		    msgDebug("uc_open: for entry %d, decoded: \"%s\", %u %d %hd %ld\n", i, nl[i].n_name, nl[i].n_type, nl[i].n_other, nl[i].n_desc, nl[i].n_value);
	    }
	}
	nl[i].n_name = "";
	fclose(fp);
	i = 0;
    }
    else
#endif
	i = nlist(kname, nl);
    if (i == -1) {
	msgDebug("uc_open: kernel %s does not contain symbols.\n", kname);
	free(kern);
	return NULL;
    }
#ifdef KERN_NO_SYMBOLS
    if (!incore) {
#else
    {
#endif
	kern->nl=(struct nlist *)malloc(sizeof(_nl));
	bcopy(_nl, kern->nl, sizeof(_nl));
    }

    if (incore) {
	if (isDebug())
	    msgDebug("uc_open: attempting to open /dev/kmem for incore.\n");
	if ((kd = open("/dev/kmem", O_RDONLY)) < 0) {
	    free(kern);
	    msgDebug("uc_open: Unable to open /dev/kmem.\n");
	    return NULL;
	}
	kern->core = (caddr_t)NULL;
	kern->incore = 1;
	kern->size = 0;
    }
    else {
	if (stat(kname, &sb) < 0) {
	    free(kern);
	    msgDebug("uc_open: Unable to stat %s.\n", kname);
	    return NULL;
	}
	kern->size = sb.st_size;
	flags = sb.st_flags;

	if (chflags(kname, 0) < 0) {
	    free(kern);
	    msgDebug("uc_open: Unable to chflags %s.\n", kname);
	    return NULL;
	}
	
	if (isDebug())
	    msgDebug("uc_open: attempting to open %s\n", kname);
	if ((kd = open(kname, O_RDWR, 0644)) < 0) {
	    free(kern);
	    msgDebug("uc_open: Unable to open %s.\n", kname);
	    return NULL;
	}
	
	fchflags(kd, flags);

	if (isDebug())
	    msgDebug("uc_open: attempting to mmap %d bytes\n", sb.st_size);
	kern->core = mmap((caddr_t)0, sb.st_size, PROT_READ | PROT_WRITE,
			  MAP_SHARED, kd, 0);
	kern->incore = 0;
	if (kern->core == MAP_FAILED) {
	    free(kern);
	    msgDebug("uc_open: Unable to mmap from %s.\n", kname);
	    return NULL;
	}
    }

    kern->fd = kd;
    get_isa_info(kern);
    if (isDebug())
	msgDebug("uc_open: got isa information\n");

    get_pci_info(kern);
    if (isDebug())
	msgDebug("uc_open: got pci information\n");

    get_eisa_info(kern);
    if (isDebug())
	msgDebug("uc_open: got eisa information\n");
#ifdef USE_SCSI
    get_scsi_info(kern);
    if (isDebug())
	msgDebug("uc_open: got scsi information\n");
#else
    kern->scsi_devp=(struct uc_scsi*)NULL;
    kern->scsibus_devp=(struct uc_scsibus*)NULL;
#endif
    return kern;
}
 
int
uc_close(struct kernel *kern, int writeback)
{
    if (kern->isa_devp)
	isa_free(kern, writeback);
    
    if (kern->eisa_devp)
	eisa_free(kern, writeback); /* `writeback' isn't really useful here */
    
    if (kern->pci_devp)
	pci_free(kern, writeback); /* or here */
    
    if (kern->scsi_devp)
	scsi_free(kern, writeback);
    
    if (!kern->incore)
	munmap(kern->core, kern->size);
    
    close(kern->fd);
    free(kern->nl);
    free(kern);
    
    return 0;
}

struct list *
uc_getdev(struct kernel *kern, char *dev)
{
    struct list *list = (struct list *)0;
    
    if (*dev == '-') { /* asked for -isa, -eisa, -pci, -scsi, -all */
	if (strcmp(dev, "-all") == 0) {
	    list = list_new();
	    if (kern->isa_devp)
		list_append(list, "isa");
	    
	    if (kern->eisa_devp)
		list_append(list, "eisa");
	    
	    if (kern->pci_devp)
		list_append(list, "pci");
	    
	    if (kern->scsi_devp)
		list_append(list, "scsi");
	    
	}
	else if (strcmp(dev, "-isa") == 0)
	    list = get_isa_devlist(kern);
	else if (strcmp(dev, "-eisa") == 0)
	    list = get_eisa_devlist(kern);
	else if (strcmp(dev, "-pci") == 0)
	    list = get_pci_devlist(kern);
	else if (strcmp(dev, "-scsi") == 0)
	    list = get_scsi_devlist(kern);
    }
    else {
	/* we gotta figure out which real device to report */
	struct uc_isa *ip;
	struct uc_scsi *sp;
	struct uc_pci *pp;
	struct uc_eisa *ep;
	
	if (kern->isa_devp) {
	    for (ip = kern->isa_devp; ip->device; ip++) {
		if (strcmp(dev, ip->device) == 0) {
		    list = get_isa_device(ip);
		    goto end;
		}
	    }
	}
	
	if (kern->scsi_devp) {
	    for (sp = kern->scsi_devp; sp->device; sp++) {
		if (strcmp(dev, sp->device) == 0) {
		    list = get_scsi_device(sp);
		    goto end;
		}
	    }
	}
	
	if (kern->pci_devp) {
	    for(pp = kern->pci_devp; pp->device; pp++) {
		if (strcmp(dev, pp->device) == 0) {
		    list = get_pci_device(pp);
		    goto end;
		}
	    }
	}
	
	if (kern->eisa_devp) {
	    for (ep = kern->eisa_devp; ep->device; ep++) {
		if (strcmp(dev, ep->device) == 0) {
		    list = get_eisa_device(ep);
		    goto end;
		}
	    }
	}
    }
end:
    return(list);
}
