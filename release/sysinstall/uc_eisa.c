/***************************************************
 * file: userconfig/uc_eisa.c
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
 * $FreeBSD$
 */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <nlist.h>
#include <i386/eisa/eisaconf.h>

#include "uc_main.h"

struct eisa_device_node {
  struct  eisa_device dev;
  struct  eisa_device_node *next;
};

/* module prototypes */
static void eisa_fill_in(struct kernel *, struct uc_eisa *, struct eisa_device_node *);

void
get_eisa_info(struct kernel *kp){
  int i, total;
  u_int *ls;
  struct eisa_driver *ed;
  struct uc_eisa *ep, *epc;
  char *name;

  if(kp->nl[EISA_SET].n_value || kp->nl[EISA_LIST].n_value) {
    ep=epc=(struct uc_eisa *)malloc(sizeof(struct uc_eisa));
    if(!kp->incore) {
      if(kp->nl[EISA_SET].n_value) {
	u_int ndev;
	ls=(u_int *)kv_to_u(kp, kp->nl[EISA_SET].n_value, sizeof(u_int)*10); /* XXX, size? */
	ndev=ls[0];
	for(i=1;i<(ndev+1);i++){
	  ep=(struct uc_eisa *)realloc(ep, sizeof(struct uc_eisa)*i);
	  epc = ep+(i-1);
	  ed=(struct eisa_driver *)kv_to_u(kp, ls[i], sizeof(struct eisa_driver));
	  name=(char *)kv_to_u(kp, (u_int)ed->name, 10); /* XXX, size? */
	  asprintf(&epc->device, "%s", name);
	  asprintf(&epc->full_name, "?");
	}
	ep=(struct uc_eisa *)realloc(ep, sizeof(struct uc_eisa)*i);
	epc = ep+(i-1);
	bzero(epc, sizeof(struct uc_eisa));
	kp->eisa_devp=ep;
      } else { /*  not incore and no symbol, we have no EISA devs... */
	kp->eisa_devp=(struct uc_eisa *)0;
      }
    } else {
      /* if we're incore, we can get data from _eisa_dev_list, */
      /* which should be much more useful, but I'll need a machine */
      /* to test :( */
      if(kp->nl[EISA_LIST].n_value) {
	u_int t;
	struct eisa_device_node *edn;

	t=kv_dref_p(kp, kp->nl[EISA_LIST].n_value);
	total=0;
	while(t) {
	  edn=(struct eisa_device_node *)
	    kv_to_u(kp, t,sizeof(struct eisa_device_node));
	  ep=(struct uc_eisa *)realloc(ep, sizeof(struct uc_eisa)*(total+1));
	  epc=ep+total;
	  eisa_fill_in(kp, epc, edn);
	  t=(u_int)edn->next;
	  free(edn);
	  total++;
	}

	ep=(struct uc_eisa *)realloc(ep, sizeof(struct uc_eisa)*(total+1));
	epc=ep+total;
	bzero(epc, sizeof(struct uc_eisa));
	kp->eisa_devp=ep;
      } else {
	kp->eisa_devp=(struct uc_eisa *)0;
      }
    }
  } else {
    kp->eisa_devp=(struct uc_eisa *)0;
  }
}

struct list *
get_eisa_devlist(struct kernel *kp){
  struct list *dl;
  struct uc_eisa *kdp;

  dl=list_new();

  for(kdp=kp->eisa_devp; kdp->device; kdp++){
    list_append(dl, kdp->device);
  }
  return(dl);
}


struct list *
get_eisa_device(struct uc_eisa *ep){
  struct list *list;
  list=list_new();

  list_append(list, ep->device);
  list_append(list, ep->full_name);

  return(list);
}


static void
eisa_fill_in(struct kernel *kp, struct uc_eisa *epc, struct eisa_device_node *edn){
  struct eisa_driver *edrv;
  char *n;

  edrv=(struct eisa_driver *)kv_to_u(kp, (u_int)edn->dev.driver,
				     sizeof(struct eisa_driver));

  n=(char *)kv_to_u(kp, (u_int)edrv->name, 20);
  asprintf(&epc->device, "%s%d", n, edn->dev.unit);
  free(n);

  n=(char *)kv_to_u(kp, (u_int)edn->dev.full_name, 40); /*XXX*/
  asprintf(&epc->full_name, "%s", n);
  free(n);
  free(edrv);
}

void
eisa_free(struct kernel *kp, int writeback){
  struct uc_eisa *ep;

  for(ep=kp->eisa_devp;ep->device;ep++){
    free(ep->device);
    free(ep->full_name);
  }
  free(kp->eisa_devp);
  kp->eisa_devp=0;
}

/* end of userconfig/uc_eisa.c */
