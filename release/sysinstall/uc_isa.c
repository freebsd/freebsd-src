/***************************************************
 * file: userconfig/uc_isa.c
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
 *
 * $Id$
 */

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <nlist.h>
#include <i386/isa/isa_device.h>

#include "uc_main.h"

void
get_isa_info(struct kernel *kp){
  int total, i, j;
  struct uc_isa *idp;
  struct isa_device *p, *isa_dp;
  struct isa_driver *drv;
  char *name;

  if(kp->nl[ISA_BIOTAB].n_value || kp->nl[ISA_TTYTAB].n_value || kp->nl[ISA_NETTAB].n_value ||
     kp->nl[ISA_NULLTAB].n_value || kp->nl[ISA_WDCTAB].n_value || kp->nl[ISA_FDCTAB].n_value) {

    idp = kp->isa_devp = (struct uc_isa *)malloc(sizeof(struct uc_isa));
    total=0; /* a running total of the number of isa devices */

    for (i=0; i<6; i++) { /* the isa devices */
      if(kp->nl[i].n_value) {
	p = isa_dp = (struct isa_device *)kv_to_u(kp, kp->nl[i].n_value,   /* XXX size? */
						  sizeof(struct isa_device)*30);
	/* build the device list */
	/* `total' keeps a running total of all the devices found */
	for (j=0; p->id_id; j++, p++, total++) {
	  kp->isa_devp = (struct uc_isa *)realloc(kp->isa_devp,
						  sizeof(struct uc_isa)*(total+1));
	  idp=kp->isa_devp+total;

	  drv=(struct isa_driver *)kv_to_u(kp, (u_int)p->id_driver, sizeof(struct isa_driver));
	  name=(char *)kv_to_u(kp, (u_int)drv->name, 64);

	  if (i==ISA_WDCTAB || i==ISA_FDCTAB) { /* special case the disk devices */
	    char n[10];
	    strncpy(n, name, 10);
	    n[strlen(n)-1]=0; /* chop off the trailing 'c' */
	    asprintf(&idp->device, "%s%d", n, j);
	  } else {
	    asprintf(&idp->device, "%s%d", name, p->id_unit);
	  }
	  idp->port=p->id_iobase;
	  idp->irq=p->id_irq;
	  idp->drq=p->id_drq;
	  idp->iomem=p->id_maddr;
	  idp->iosize=p->id_msize;
	  idp->flags=p->id_flags;
	  idp->alive=p->id_alive;
	  idp->enabled=p->id_enabled;
	  idp->modified=0;
	  if(!kp->incore){
	    idp->idp=p;
	  } else {
	    free(name);
	    free(drv);
	  }
	}
	if(kp->incore){
	  free(isa_dp);
	}
      }
    }

    idp=kp->isa_devp+total;
    bzero(idp, sizeof(struct uc_isa));
  } else {
    kp->isa_devp=0;
  }
}


struct list *
get_isa_devlist(struct kernel *kp){
  struct list *dl;
  struct uc_isa *kdp;

  dl=list_new();

  for(kdp=kp->isa_devp; kdp->device; kdp++){
    list_append(dl, kdp->device);
  }
  return(dl);
}


struct list *
get_isa_device(struct uc_isa *ip){
  struct list *list;
  char *tmp;

  list=list_new();

  asprintf(&tmp, "%s", ip->device );
  list_append(list, tmp);
  free(tmp);

  asprintf(&tmp, "0x%04x", ip->port );
  list_append(list, tmp);
  free(tmp);

  asprintf(&tmp, "%d", ip->irq>0 ? ffs(ip->irq)-1 : ip->irq);
  list_append(list, tmp);
  free(tmp);

  asprintf(&tmp, "%d", ip->drq );
  list_append(list, tmp);
  free(tmp);

  asprintf(&tmp, "0x%08x", ip->iomem );
  list_append(list, tmp);
  free(tmp);

  asprintf(&tmp, "0x%x", ip->iosize );
  list_append(list, tmp);
  free(tmp);

  asprintf(&tmp, "0x%x", ip->flags );
  list_append(list, tmp);
  free(tmp);

  asprintf(&tmp, "%d", ip->alive );
  list_append(list, tmp);
  free(tmp);

  asprintf(&tmp, "%d", ip->enabled );
  list_append(list, tmp);
  free(tmp);

  return(list);
}

int
isa_setdev(struct kernel *kp, struct list *list){
  int r=1, irq;
  struct uc_isa *ip;

  if(kp->isa_devp)
    for(ip=kp->isa_devp;ip->device;ip++){
      if(strcmp(list->av[0], ip->device)==0){
	ip->modified=1;
	ip->port = strtol(list->av[1], (char **)NULL, 0);
	irq=strtol(list->av[2], (char **)NULL, 0);
	ip->irq=  irq > 0 ? 1 << (irq) : irq;
	ip->drq = strtol(list->av[3], (char **)NULL, 0);
	ip->iomem = (caddr_t)strtol(list->av[4], (char **)NULL, 0);
	ip->iosize = strtol(list->av[5], (char **)NULL, 0);
	ip->flags = strtol(list->av[6], (char **)NULL, 0);
	ip->enabled = strtol(list->av[8], (char **)NULL, 0);
	r=0;
	break;
      }
    }
  return(r);
}

void
isa_free(struct kernel *kp, int writeback){
  struct uc_isa *ip;
  for(ip=kp->isa_devp; ip->device; ip++){
    if((!kp->incore) && ip->modified && writeback) {
      /* save any changes */
      ip->idp->id_iobase=ip->port;
      ip->idp->id_irq = ip->irq;
      ip->idp->id_drq = ip->drq;
      ip->idp->id_maddr = ip->iomem;
      ip->idp->id_msize = ip->iosize;
      ip->idp->id_flags = ip->flags;
      ip->idp->id_enabled = ip->enabled;
    }
    /* and, be free... */
    free(ip->device);
  }
  /* and free the whole ball of wax */
  free(kp->isa_devp);
  kp->isa_devp=0;
}

/* end of userconfig/uc_isa.c */
