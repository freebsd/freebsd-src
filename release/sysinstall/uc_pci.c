/***************************************************
 * file: userconfig/uc_pci.c
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
#include <nlist.h>
#include <pci/pcivar.h>
#include <tcl.h>

#include "uc_main.h"

void
get_pci_info(struct kernel *kp){
  int i, total;
  u_int *ls, ndev;
  struct pci_device *pd;
  struct uc_pci *pp,*ppc;
  char *name;

  if(kp->nl[PCI_SET].n_value){
    pp = ppc = (struct uc_pci *)malloc(sizeof(struct uc_pci));
    ls=(u_int *)kv_to_u(kp, kp->nl[PCI_SET].n_value, sizeof(u_int)*30); /* XXX, size? */
    ndev=ls[0];
    total=0;
    for(i=1;i<(ndev+1);i++){
      pp=(struct uc_pci *)realloc(pp, sizeof(struct uc_pci)*(total+1));
      ppc = pp+(total);
      pd=(struct pci_device *)kv_to_u(kp, ls[i], sizeof(struct pci_device));
      /* don't try to dereference a null pointer */
      name=pd->pd_name ? (char *)kv_to_u(kp, (u_int)pd->pd_name, 10) :
	pd->pd_name; /* XXX, size? */
      if(kp->incore){
	int u, k;
	/* incore, we can get unit numbers */

	u=kv_dref_p(kp, (u_int)pd->pd_count);
	for(k=0;k<u;k++,total++){
	  pp=(struct uc_pci *)realloc(pp, sizeof(struct uc_pci)*(total+1));
	  ppc = pp+(total);
	  asprintf(&ppc->device, "%s%d", name, k);
	}
	free(pd);
	if(name)
	  free(name);
      } else {
	asprintf(&ppc->device, "%s?", name);
	total++;
      }
    }
    pp=(struct uc_pci *)realloc(pp, sizeof(struct uc_pci)*(total+1));
    ppc = pp+(total);
    bzero(ppc, sizeof(struct uc_pci));
    kp->pci_devp=pp;
  } else {
    kp->pci_devp=(struct uc_pci *)0;
  }
}


struct list *
get_pci_devlist(struct kernel *kp){
  struct list *dl;
  struct uc_pci *kdp;

  dl=list_new();

  for(kdp=kp->pci_devp; kdp->device; kdp++){
    list_append(dl, kdp->device);
  }
  return(dl);
}


struct list *
get_pci_device(struct uc_pci *pp){
  struct list *list;
  list=list_new();

  list_append(list, pp->device);

  return(list);
}

void
pci_free(struct kernel *kp, int writeback){
  struct uc_pci *pp;

  for(pp=kp->pci_devp;pp->device;pp++){
    free(pp->device);
  }
  free(kp->pci_devp);
  kp->pci_devp=0;
  
}

/* end of userconfig/uc_pci.c */
