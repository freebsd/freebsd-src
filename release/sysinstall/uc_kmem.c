/***************************************************
 * file: userconfig/uc_kmem.c
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
#include <unistd.h>
#include <a.out.h>

#include <stdio.h>

#include "uc_main.h"

/* translate a kv pointer to user space */
/* malloc()-ing if we aren't mmaped */
u_int
kv_to_u(struct kernel *kp, u_int adr, u_int size){
  u_int tadr;
  if(!kp->incore){
    struct exec *ep;
    ep=(struct exec *)kp->core;
    tadr=(u_int)((adr - ep->a_entry + (N_DATOFF(*ep) - ep->a_text))+kp->core);
  } else {
    caddr_t ptr;
    ptr = malloc(size);
    lseek(kp->fd, adr, SEEK_SET);
    read(kp->fd, ptr, size);
    tadr=(u_int)ptr;
  }
  return(tadr);
}

/* dereference a pointer to kernel space */
u_int
kv_dref_p(struct kernel *kp, u_int adr){
  u_int tadr;
  if(!kp->incore){
    struct exec *ep;
    ep=(struct exec *)kp->core;
    tadr=*(u_int*)((adr - ep->a_entry + (N_DATOFF(*ep) - ep->a_text))+kp->core);
  } else {
    lseek(kp->fd, adr, SEEK_SET);
    read(kp->fd, &tadr, sizeof(tadr));
  }
  return(tadr);
}

/* deref a pointer to kernel text */
u_int
kv_dref_t(struct kernel *kp, u_int adr){
  u_int tadr;
  if(!kp->incore){
    struct exec *ep;
    ep=(struct exec *)kp->core;
    tadr=*(u_int*)((adr - ep->a_entry) + N_TXTOFF(*ep) + (u_int)kp->core);
  } else {
    lseek(kp->fd, adr, SEEK_SET);
    read(kp->fd, &tadr, sizeof(tadr));
  }
  return(tadr);
}

/* end of userconfig/uc_kmem.c */
