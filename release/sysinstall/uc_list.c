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
 * $FreeBSD$
 */

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <nlist.h>
#include "uc_main.h"

struct list *
list_new(void){
  struct list *rv;
  rv=(struct list *)malloc(sizeof(struct list));
  rv->ac=0;
  rv->av=(char **)0;
  return(rv);
}

void
list_append(struct list *list , char *item){

  if(list->ac==0) {
    list->av=(char **)malloc(sizeof(char *)*(list->ac+1));
  } else {
    list->av=(char **)realloc(list->av, sizeof(char *)*(list->ac+1));
  }
  asprintf(list->av+list->ac, "%s", item);
  list->ac++;
}

void
list_print(struct list *list, char *separator){
  int i;
  for(i=0; i<list->ac; i++)
    printf("%s%s", list->av[i], separator);
}

void
list_destroy(struct list *list){
  int i;
  for(i=0;i<list->ac;i++){
    free(list->av[i]);
    list->av[i]=0;
  }
  free(list->av);
  list->av=0;
  free(list);
}

/* end of userconfig/uc_list.c */
