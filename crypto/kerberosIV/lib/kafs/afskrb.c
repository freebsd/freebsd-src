/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "kafs_locl.h"

RCSID("$Id: afskrb.c,v 1.6 1997/05/26 17:38:24 bg Exp $");

#define AUTH_SUPERUSER "afs"

/*
 * Here only ASCII characters are relevant.
 */

#define IsAsciiLower(c) ('a' <= (c) && (c) <= 'z')

#define ToAsciiUpper(c) ((c) - 'a' + 'A')

static void
foldup(char *a, const char *b)
{
  for (; *b; a++, b++)
    if (IsAsciiLower(*b))
      *a = ToAsciiUpper(*b);
    else
      *a = *b;
  *a = '\0';
}

static int
get_cred(const char *princ, const char *inst, const char *krealm, 
	 CREDENTIALS *c, KTEXT_ST *tkt)
{
  int k_errno = krb_get_cred((char*)princ, (char*)inst, (char*)krealm, c);

  if (k_errno != KSUCCESS)
    {
      k_errno = krb_mk_req(tkt, (char*)princ, (char*)inst, (char*)krealm, 0);
      if (k_errno == KSUCCESS)
	k_errno = krb_get_cred((char*)princ, (char*)inst, (char*)krealm, c);
    }
  return k_errno;
}


/* Convert a string to a 32 bit ip number in network byte order. 
   Return 0 on error
   */

static u_int32_t
ip_aton(char *ip)
{
  u_int32_t addr;
  unsigned int a, b, c, d;

  if(sscanf(ip, "%u.%u.%u.%u", &a, &b, &c, &d) != 4)
      return 0;
  if((a | b | c | d) > 255)
      return 0;
  addr = (a << 24) | (b << 16) | (c << 8) | d;
  addr = htonl(addr);
  return addr;
}

/* Try to get a db-server for an AFS cell from a AFSDB record */

static int
dns_find_cell(const char *cell, char *dbserver)
{
    struct dns_reply *r;
    int ok = -1;
    r = dns_lookup(cell, "afsdb");
    if(r){
	struct resource_record *rr = r->head;
	while(rr){
	    if(rr->type == T_AFSDB && rr->u.afsdb->preference == 1){
		strncpy(dbserver, rr->u.afsdb->domain, MaxHostNameLen);
		dbserver[MaxHostNameLen - 1] = 0;
		ok = 0;
		break;
	    }
	    rr = rr->next;
	}
	dns_free_data(r);
    }
    return ok;
}


/* Find the realm associated with cell. Do this by opening
   /usr/vice/etc/CellServDB and getting the realm-of-host for the
   first VL-server for the cell.

   This does not work when the VL-server is living in one cell, but
   the cell it is serving is living in another cell.
   */

static char*
realm_of_cell(const char *cell)
{
    FILE *F;
    char buf[1024];
    u_int32_t addr;
    struct hostent *hp;
    char *realm = NULL;

    if((F = fopen(_PATH_CELLSERVDB, "r"))){
	while(fgets(buf, sizeof(buf), F)){
	    if(buf[0] != '>')
		continue;
	    if(strncmp(buf + 1, cell, strlen(cell)) == 0){
		if(fgets(buf, sizeof(buf), F) == NULL)
		    break;
		addr = ip_aton(buf);
		if(addr == 0)
		    break;
		hp = gethostbyaddr((char*)&addr, 4, AF_INET);
		if(hp == NULL)
		    break;
		strncpy (buf, hp->h_name, sizeof(buf));
		buf[sizeof(buf) - 1] = '\0';
		realm = krb_realmofhost(buf);
		break;
	    }
	}
	fclose(F);
    }
    if(realm == NULL){
	if(dns_find_cell(cell, buf) == 0)
	    realm = krb_realmofhost(buf);
    }
    return realm;
}

/*
 * Get tokens for all cells[]
 */
static int
k_afslog_cells(char *cells[], int max, const char *krealm, uid_t uid)
{
    int err = KSUCCESS;
    int i;
    for(i = 0; i < max; i++)
	err = k_afsklog_uid(cells[i], krealm, uid);
    return err;
}

/*
 * Try to find the cells we should try to klog to in "file".
 */
static void
k_find_cells(char *file, char *cells[], int size, int *index)
{
    FILE *f;
    char cell[64];
    int i;
    f = fopen(file, "r");
    if (f == NULL)
	return;
    while (*index < size && fgets(cell, sizeof(cell), f)) {
	char *nl = strchr(cell, '\n');
	if (nl) *nl = 0;
	for(i = 0; i < *index; i++)
	    if(strcmp(cells[i], cell) == 0)
		break;
	if(i == *index)
	    cells[(*index)++] = strdup(cell);
    }
    fclose(f);
}

static int
k_afsklog_all_local_cells(const char *krealm, uid_t uid)
{
    int err;
    char *cells[32]; /* XXX */
    int num_cells = sizeof(cells) / sizeof(cells[0]);
    int index = 0;

    char *p;
    
    if ((p = getenv("HOME"))) {
	char home[MaxPathLen];

	if (k_concat(home, sizeof(home), p, "/.TheseCells", NULL) == 0)
	    k_find_cells(home, cells, num_cells, &index);
    }
    k_find_cells(_PATH_THESECELLS, cells, num_cells, &index);
    k_find_cells(_PATH_THISCELL, cells, num_cells, &index);
    
    err = k_afslog_cells(cells, index, krealm, uid);
    while(index > 0)
	free(cells[--index]);
    return err;
}

int
k_afsklog_uid(const char *cell, const char *krealm, uid_t uid)
{
  int k_errno;
  CREDENTIALS c;
  KTEXT_ST ticket;
  char realm[REALM_SZ];
  char *vl_realm; /* realm of vl-server */
  char *lrealm; /* local realm */
  char CELL[64];

  if (cell == 0 || cell[0] == 0)
    return k_afsklog_all_local_cells (krealm, uid);
  foldup(CELL, cell);

  k_errno = krb_get_lrealm(realm , 0);
  if(k_errno == KSUCCESS && (krealm == NULL || strcmp(krealm, realm)))
    lrealm = realm;
  else
    lrealm = NULL;

  /* We're about to find the the realm that holds the key for afs in
   * the specified cell. The problem is that null-instance
   * afs-principals are common and that hitting the wrong realm might
   * yield the wrong afs key. The following assumptions were made.
   *
   * Any realm passed to us is preferred.
   *
   * If there is a realm with the same name as the cell, it is most
   * likely the correct realm to talk to.
   *
   * In most (maybe even all) cases the database servers of the cell
   * will live in the realm we are looking for.
   *
   * Try the local realm, but if the previous cases fail, this is
   * really a long shot.
   *
   */
  
  /* comments on the ordering of these tests */

  /* If the user passes a realm, she probably knows something we don't
   * know and we should try afs@krealm (otherwise we're talking with a
   * blondino and she might as well have it.)
   */
  
  k_errno = -1;
  if(krealm){
    k_errno = get_cred(AUTH_SUPERUSER, cell, krealm, &c, &ticket);
    if(k_errno)
      k_errno = get_cred(AUTH_SUPERUSER, "", krealm, &c, &ticket);
  }

  if(k_errno)
    k_errno = get_cred(AUTH_SUPERUSER, cell, CELL, &c, &ticket);
  if(k_errno)
    k_errno = get_cred(AUTH_SUPERUSER, "", CELL, &c, &ticket);
  
  /* this might work in some conditions */
  if(k_errno && (vl_realm = realm_of_cell(cell))){
    k_errno = get_cred(AUTH_SUPERUSER, cell, vl_realm, &c, &ticket);
    if(k_errno)
      k_errno = get_cred(AUTH_SUPERUSER, "", vl_realm, &c, &ticket);
  }
  
  if(k_errno && lrealm){
    k_errno = get_cred(AUTH_SUPERUSER, cell, lrealm, &c, &ticket);
#if 0
    /* this is most likely never right anyway, but won't fail */
    if(k_errno)
      k_errno = get_cred(AUTH_SUPERUSER, "", lrealm, &c, &ticket);
#endif
  }
  
  if (k_errno == KSUCCESS)
    {
      struct ViceIoctl parms;
      struct ClearToken ct;
      int32_t sizeof_x;
      char buf[2048], *t;

      /*
       * Build a struct ClearToken
       */
      ct.AuthHandle = c.kvno;
      memcpy (ct.HandShakeKey, c.session, sizeof(c.session));
      ct.ViceId = uid;	/* is this always valid? */
      ct.BeginTimestamp = 1 + c.issue_date;
      ct.EndTimestamp = krb_life_to_time(c.issue_date, c.lifetime);

#define ODD(x) ((x) & 1)
      /* If we don't know the numerical ID lifetime should be even? */
      if (uid == 0 && ODD(ct.EndTimestamp - ct.BeginTimestamp))
	ct.BeginTimestamp--;

      t = buf;
      /*
       * length of secret token followed by secret token
       */
      sizeof_x = c.ticket_st.length;
      memcpy(t, &sizeof_x, sizeof(sizeof_x));
      t += sizeof(sizeof_x);
      memcpy(t, c.ticket_st.dat, sizeof_x);
      t += sizeof_x;
      /*
       * length of clear token followed by clear token
       */
      sizeof_x = sizeof(ct);
      memcpy(t, &sizeof_x, sizeof(sizeof_x));
      t += sizeof(sizeof_x);
      memcpy(t, &ct, sizeof_x);
      t += sizeof_x;

      /*
       * do *not* mark as primary cell
       */
      sizeof_x = 0;
      memcpy(t, &sizeof_x, sizeof(sizeof_x));
      t += sizeof(sizeof_x);
      /*
       * follow with cell name
       */
      sizeof_x = strlen(cell) + 1;
      memcpy(t, cell, sizeof_x);
      t += sizeof_x;

      /*
       * Build argument block
       */
      parms.in = buf;
      parms.in_size = t - buf;
      parms.out = 0;
      parms.out_size = 0;
      k_pioctl(0, VIOCSETTOK, &parms, 0);
    }
  return k_errno;
}

int
k_afsklog(const char *cell, const char *krealm)
{
  return k_afsklog_uid (cell, krealm, getuid());
}
