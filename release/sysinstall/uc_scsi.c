/***************************************************
 * file: userconfig/uc_scsi.c
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
#include <scsi/scsiconf.h>
#include <tcl.h>

#include "uc_main.h"

/* this stuff is hidden under an #ifdef KERNEL in scsiconf.h */
#define SCCONF_UNSPEC 255
#define SCCONF_ANY 254

struct scsi_ctlr_config {
  int scbus;
  char *driver;
  int unit;
  int bus;
};

struct scsi_device_config {
  char *name;		/* SCSI device name (sd, st, etc) */
  int unit;		/* desired device unit */
  int cunit;		/* Controller unit */
  int target;		/* SCSI ID (target) */
  int lun;		/* SCSI lun */
  int flags;		/* Flags from config */
};

/* module prototypes */
static void get_sl_info(struct kernel *kp, struct uc_scsi *spc,
			struct scsi_link *sl);

void
get_scsi_info(struct kernel *kp){
  int i, j, k;

  if(kp->incore){
    if (kp->nl[SCSI_BUSSES].n_value) {
      u_int *es, *sba;
      struct scsibus_data *sbd;
      int nsbd, nscsibus, total;
      struct uc_scsi *sp, *spc;
      struct uc_scsibus *sbp, *sbpc;
      struct scsi_link *s_l;
      u_int *slp;
      char *temp;
      struct scsi_device *sdev;
      u_int t;
      char name[10];

      sp=(struct uc_scsi*)malloc(sizeof(struct uc_scsi));
      total=0;
      sbp=(struct uc_scsibus*)malloc(sizeof(struct uc_scsibus));
      nscsibus=0;

      es=(u_int *)kv_to_u(kp, kv_dref_p(kp,kp->nl[SCSI_BUSSES].n_value),
			  sizeof(u_int)*2);
      nsbd=es[0];
      sba=(u_int *)kv_to_u(kp, es[1], sizeof(u_int)*nsbd);
      free(es);

      for(i=0;i<nsbd;i++){
	if(sba[i]){
	  /* first grab the adapter info */
	  sbd=(struct scsibus_data *)kv_to_u(kp, sba[i],
					     sizeof(struct scsibus_data));
	  sbp=(struct uc_scsibus *)realloc(sbp, sizeof(struct uc_scsibus)*
					   (nscsibus+1));
	  
	  sp=(struct uc_scsi*)realloc(sp, (sizeof(struct uc_scsi)*(total+1)));
	  spc=sp+total;
	  s_l=(struct scsi_link*)kv_to_u(kp, (u_int)sbd->adapter_link,
					 sizeof(struct scsi_link));
	  get_sl_info(kp, spc, s_l);
	  free(s_l);

	  sbpc=sbp+nscsibus;
	  sbpc->bus_no=nscsibus;
	  sscanf(spc->device, "%[a-z]%d", name, &sbpc->unit);
	  asprintf(&sbpc->driver, "%s", name);

	  total++;
	  t=kv_dref_p(kp, (u_int)sbd->sc_link);
	  t=(u_int)sbd->sc_link;
	  for(j=0;j<8;j++) {
	    slp=(u_int *)kv_to_u(kp, t+(j*8*sizeof(u_int)), /* XXX */
				 (sizeof(u_int)*sbd->maxlun));
	    for(k=0;k<sbd->maxlun && slp[k]; k++){
	      struct scsi_link *slt;
	      sp=(struct uc_scsi*)realloc(sp,
					  (sizeof(struct uc_scsi)*(total+1)));
	      spc=sp+total;
	      slt=(struct scsi_link*)kv_to_u(kp, slp[k],
					     sizeof(struct scsi_link));
	      get_sl_info(kp, spc, slt);
	      free(slt);
	      spc->config=(struct scsi_device_config *)0;
	      total++;
	    }
	    free(slp);
	  }
	  free(sbd);
	}
      }
      /* now stuff in the list of drivers configured in the system */
      t=kv_dref_p(kp, kp->nl[SCSI_LIST].n_value);
      while(t) {
	sdev=(struct scsi_device*)kv_to_u(kp, t, sizeof(struct scsi_device));
	sp=(struct uc_scsi*)realloc(sp, (sizeof(struct uc_scsi)*(total+1)));
	spc=sp+total;
	total++;
	temp=(char *)kv_to_u(kp, (u_int)sdev->name, 10);
	asprintf(&spc->device, "%s*", temp);
	free(temp);
	asprintf(&spc->adapter, "any");

	spc->target=SCCONF_ANY;
	spc->lun=SCCONF_ANY;

	temp=(char *)kv_to_u(kp, (u_int)sdev->desc, 20);
	asprintf(&spc->desc, "%s", temp);
	free(temp);

	spc->config=(struct scsi_device_config *)0;

	t=(u_int)sdev->next;
	free(sdev);
      }
      sp=(struct uc_scsi*)realloc(sp, (sizeof(struct uc_scsi)*(total+1)));
      spc=sp+total;
      bzero(spc, sizeof(struct uc_scsi));
      kp->scsi_devp=sp;
      kp->scsibus_devp=sbp;
    } else { /* no symbol, and incore, no scsi */
      kp->scsi_devp=(struct uc_scsi *)0;
      kp->scsibus_devp=(struct uc_scsibus *)0;
    }
  } else { /* on disk */

    if (kp->nl[SCSI_CINIT].n_value || kp->nl[SCSI_DINIT].n_value ||
	kp->nl[SCSI_TINIT].n_value) {
      int total=0;
      struct uc_scsi *sp, *spc;
      struct scsi_ctlr_config *sctl_c;
      struct scsi_device_config *sdev_c;
      struct scsi_device *sdev;

      struct uc_scsibus *uc_scbus, *uc_scbusc;

      u_int t, ctrl_total;
      char *temp;
      u_int initp;

      spc=sp=(struct uc_scsi*)malloc(sizeof(struct uc_scsi));
      total=0;
      ctrl_total=0;

      /* static kernel, we'll first get the wired controllers/devices */
      if((t=kp->nl[SCSI_CINIT].n_value)){
	/* get controller info*/
	sctl_c=(struct scsi_ctlr_config*)kv_to_u(kp, t, sizeof(struct scsi_ctlr_config));
	uc_scbus=(struct uc_scsibus*)malloc(sizeof(struct uc_scsibus));

	while(sctl_c->driver){

	  /* remember the bus info, for later */
	  uc_scbus=(struct uc_scsibus*)realloc(uc_scbus, sizeof(struct uc_scsibus)*(ctrl_total+1));
	  uc_scbusc=uc_scbus+ctrl_total;
	  uc_scbusc->bus_no=sctl_c->scbus;
	  temp=(char *)kv_to_u(kp, (u_int)sctl_c->driver, 20);
	  uc_scbusc->driver=temp;
	  uc_scbusc->unit=sctl_c->unit;
	  uc_scbusc->config=sctl_c;

	  sp=(struct uc_scsi*)realloc(sp,sizeof(struct uc_scsi)*(total+1));
	  spc=sp+total;
	  asprintf(&spc->device, "%s%d", temp, sctl_c->unit);
	  asprintf(&spc->adapter, "%s%d", temp, sctl_c->unit);
	  spc->target=0;
	  spc->lun=0;
	  spc->modified=0;
	  asprintf(&spc->desc, "%s", temp);
	  total++;
	  ctrl_total++;
	  sctl_c++;
	}
      }

      if((t=kp->nl[SCSI_DINIT].n_value)){
	/* get wired device info */
	sdev_c=(struct scsi_device_config*)kv_to_u(kp, t, sizeof(struct scsi_device_config));
	while(sdev_c->name){
	  sp=(struct uc_scsi*)realloc(sp, sizeof(struct uc_scsi)*(total+1));
	  spc=sp+total;
	  temp=(char*)kv_to_u(kp, (u_int)sdev_c->name, 10);
	  asprintf(&spc->device, "%s%d", temp, sdev_c->unit);
	  /* figure out controller */
	  if(sdev_c->cunit == SCCONF_ANY){
	    asprintf(&spc->adapter, "any");
	  } else {
	    if(ctrl_total){
	      for(i=0;i<ctrl_total;i++){
		if(sdev_c->cunit==uc_scbus[i].bus_no){
		  asprintf(&spc->adapter, "%s%d",
			   uc_scbus[i].driver, uc_scbus[i].unit);
		  break;
		}
	      }
	      if(i==ctrl_total) { /* made it through the whole list */
		asprintf(&spc->adapter, "any?");
	      }
	    } else {
	    asprintf(&spc->adapter, "any?");
	    }
	  }
	  spc->target= sdev_c->target;
	  spc->lun= sdev_c->lun;
	  spc->desc=(char *)0; /* filled in later */
	  spc->config=sdev_c;
	  spc->modified=0;
	  sdev_c++;
	  total++;
	}
      }
      kp->scsibus_devp=uc_scbus;

      if((t=kp->nl[SCSI_TINIT].n_value)) {
	/* WARNING:  This is teetering on the brink of stupid.

	   this ugly little hack only works because the
	   <scsi driver>init routines are macro-generated,
	   so the offset of the device pointers will be
	   the same (hopefully).
	   */

	while((initp=kv_dref_p(kp,t))) {
	  u_int tadr;
	  u_int sl;

	  t+=4;
	  tadr=kv_dref_t(kp, initp+4); /* offset in *.text* */
	  sdev=(struct scsi_device*)kv_to_u(kp, tadr, sizeof(struct scsi_device));
	  sp=(struct uc_scsi*)realloc(sp, (sizeof(struct uc_scsi)*(total+1)));
	  spc=sp+total;
	  total++;
	  temp=(char *)kv_to_u(kp, (u_int)sdev->name, 10);
	  asprintf(&spc->device, "%s*", temp);
	  
	  asprintf(&spc->adapter, "any");

	  spc->target=SCCONF_ANY;
	  spc->lun=SCCONF_ANY;
	  spc->modified=0;
	  temp=(char *)kv_to_u(kp, (u_int)sdev->desc, 20);
	  asprintf(&spc->desc, "%s", temp);
	  /* now try to fill in any device descriptions from above */
	  sl=strlen(spc->device)-1;
	  for(i=0;i<(total-1);i++){ /* don't look at this device */
	    struct uc_scsi *usp;

	    usp=sp+i;
	    if(strncmp(usp->device, spc->device, sl)==0 && usp->desc==0) {
	      asprintf(&usp->desc, "%s", spc->desc);
	    }
	  }
	}
	if(total){
	  sp=(struct uc_scsi*)realloc(sp, (sizeof(struct uc_scsi)*(total+1)));
	  spc=sp+total;
	  bzero(spc, sizeof(struct uc_scsi));
	  kp->scsi_devp=sp;
	} else {
	  free(sp);
	  kp->scsi_devp=(struct uc_scsi *)0;
	}
      }
    } else {
	kp->scsi_devp=(struct uc_scsi *)0;
    }
  }
}

struct list *
get_scsi_devlist(struct kernel *kp){
  struct list *dl;
  struct uc_scsi *kdp;

  dl=list_new();

  for(kdp=kp->scsi_devp; kdp->device; kdp++){
    list_append(dl, kdp->device);
  }

  return(dl);
}

struct list *
get_scsi_device(struct uc_scsi *sp){
  struct list *list;
  char *tmp;

  list=list_new();

  list_append(list, sp->device);
  list_append(list, sp->adapter);

  asprintf(&tmp, "%d", sp->target );
  list_append(list, tmp);
  free(tmp);

  asprintf(&tmp, "%d", sp->lun );
  list_append(list, tmp);
  free(tmp);

  list_append(list, sp->desc);

  return(list);
}

/* given a scsi_link and a uc_scsi pointer, fill it in */
static void
get_sl_info(struct kernel *kp, struct uc_scsi *spc, struct scsi_link *sl){

  struct scsi_adapter *sadp;
  struct scsi_device *sdev;
  char *temp;

  sadp=(struct scsi_adapter*)kv_to_u(kp, (u_int)sl->adapter,
				     sizeof(struct scsi_adapter));

  sdev=(struct scsi_device*)kv_to_u(kp, (u_int)sl->device,
				    sizeof(struct scsi_device));

  temp=(char *)kv_to_u(kp, (u_int)sdev->name, 20);
  asprintf(&spc->device, "%s%d", temp, sl->dev_unit);
  free(temp);
  temp=(char *)kv_to_u(kp, (u_int)sadp->name, 20);
  asprintf(&spc->adapter, "%s%d", temp, sl->adapter_unit);
  free(temp);
  spc->target = sl->target;
  spc->lun = sl->lun;

  temp=(char *)kv_to_u(kp, (u_int)sdev->desc, 30);
  asprintf(&spc->desc, "%s", temp);
  free(temp);

}

int
scsi_setdev(struct kernel *kp, struct list *list){
  int r=1, bus_valid=0;
  struct uc_scsi *sp;
  struct uc_scsibus *sbp;
  char *t;

  if(kp->scsi_devp)
    for(sp=kp->scsi_devp;sp->device;sp++){
      if(strcmp(list->av[0], sp->device)==0){
	for(sbp=kp->scsibus_devp;sbp->driver; sbp++){
	  asprintf(&t, "%s%d", sbp->driver, sbp->unit);
	  if(strcmp(list->av[1], t)==0) {
	    bus_valid=1;
	  }
	  free(t);
	}
	if(bus_valid){
	  sp->modified=1;
	  free(sp->adapter);
	  asprintf(&sp->adapter, "%s", list->av[1]);
	  sp->target = strtol(list->av[2], (char **)NULL, 0);
	  sp->lun = strtol(list->av[3], (char **)NULL, 0);
	  r=0;
	  goto done;
	} else {
	  r=2;
	}
      }
    }
done:
  return(r);
}

void
scsi_free(struct kernel *kp, int writeback){
  struct uc_scsi *sp;
  struct uc_scsibus *sbp;
  char *t;
  int scbus, i;

  for(sp=kp->scsi_devp; sp->device; sp++){
    if((!kp->incore) && sp->modified && writeback) {
      /* save info */

      /* I'm not sure this is necessary */
#if 0
      sscanf(sp->device, "%[a-z]%d", name, &unit);
      sp->config->unit= unit;
#endif

      /* figger out the controller, which may have changed */
      scbus=-1;
      for(sbp=kp->scsibus_devp, i=0;sbp->driver; sbp++,i++){
	asprintf(&t, "%s%d", sbp->driver, sbp->unit);
	if(strcmp(sp->adapter, t)==0) {
	  scbus=i;
	}
	free(t);
      }

      /* if we fell through, don't change anything */
      if(scbus!=-1){
	sp->config->cunit= scbus;
      }

      sp->config->target= sp->target;
      sp->config->lun= sp->lun;
      /* sp->config->flags= ; XXX this should be here*/
	
    }
    free(sp->device);
    free(sp->adapter);
    free(sp->desc);
  }
  free(kp->scsi_devp);
  kp->scsi_devp=(struct uc_scsi *)0;
#define WANT_TO_COREDUMP 1
#if WANT_TO_COREDUMP /* ugly hack until scsi_getdev() gets -incore
			busses correctly */
  /* now free the bus info */
  if(kp->incore){
    for(sbp=kp->scsibus_devp;sbp->driver; sbp++){
      free(sbp->driver);
    }
  }
#endif
  if (kp->scsibus_devp)
    free(kp->scsibus_devp);
  kp->scsibus_devp=(struct uc_scsibus *)0;
}

/* end of userconfig/uc_scsi.c */
