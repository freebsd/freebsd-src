/*
 *			PPP Secret Key Module
 *
 *	    Written by Toshiharu OHNO (tony-o@iij.ad.jp)
 *
 *   Copyright (C) 1994, Internet Initiative Japan, Inc. All rights reserverd.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the Internet Initiative Japan, Inc.  The name of the
 * IIJ may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id:$
 *
 *	TODO:
 *		o Imprement check against with registerd IP addresses.
 */
#include "fsm.h"
#include "ipcp.h"

extern FILE *OpenSecret();
extern void CloseSecret();

int
AuthValidate(fname, system, key)
char *fname, *system, *key;
{
  FILE *fp;
  int n;
  char *vector[20];
  char buff[200];
  char passwd[100];

  fp = OpenSecret(fname);
  if (fp == NULL)
    return(0);
  while (fgets(buff, sizeof(buff), fp)) {
    if (buff[0] == '#')
      continue;
    buff[strlen(buff)-1] = 0;
    bzero(vector, sizeof(vector));
    n = MakeArgs(buff, &vector);
    if (n < 2)
      continue;
    if (strcmp(vector[0], system) == 0) {
      ExpandString(vector[1], passwd, 0);
      if (strcmp(passwd, key) == 0) {
	CloseSecret(fp);
        bzero(&DefHisAddress, sizeof(DefHisAddress));
        n -= 2;
        if (n > 0) {
	  ParseAddr(n--, &vector[2],
	    &DefHisAddress.ipaddr, &DefHisAddress.mask, &DefHisAddress.width);
	}
	IpcpInit();
	return(1);	/* Valid */
      }
    }
  }
  CloseSecret(fp);
  return(0);		/* Invalid */
}

char *
AuthGetSecret(fname, system, len, setaddr)
char *fname, *system;
int len, setaddr;
{
  FILE *fp;
  int n;
  char *vector[20];
  char buff[200];
  static char passwd[100];

  fp = OpenSecret(fname);
  if (fp == NULL)
    return(NULL);
  while (fgets(buff, sizeof(buff), fp)) {
    if (buff[0] == '#')
      continue;
    buff[strlen(buff)-1] = 0;
    bzero(vector, sizeof(vector));
    n = MakeArgs(buff, &vector);
    if (n < 2)
      continue;
    if (strlen(vector[0]) == len && strncmp(vector[0], system, len) == 0) {
      ExpandString(vector[1], passwd, 0);
      if (setaddr) {
        bzero(&DefHisAddress, sizeof(DefHisAddress));
      }
      n -= 2;
      if (n > 0 && setaddr) {
#ifdef DEBUG
	LogPrintf(LOG_LCP, "*** n = %d, %s\n", n, vector[2]);
#endif
	ParseAddr(n--, &vector[2],
	  &DefHisAddress.ipaddr, &DefHisAddress.mask, &DefHisAddress.width);
	IpcpInit();
      }
      return(passwd);
    }
  }
  CloseSecret(fp);
  return(NULL);		/* Invalid */
}
