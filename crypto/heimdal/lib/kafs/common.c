/*
 * Copyright (c) 1997 - 2002 Kungliga Tekniska Högskolan
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
 * 3. Neither the name of the Institute nor the names of its contributors 
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

RCSID("$Id: common.c,v 1.24 2002/05/31 02:43:51 assar Exp $");

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

int
kafs_settoken(const char *cell, uid_t uid, CREDENTIALS *c)
{
    struct ViceIoctl parms;
    struct ClearToken ct;
    int32_t sizeof_x;
    char buf[2048], *t;
    int ret;
    
    /*
     * Build a struct ClearToken
     */
    ct.AuthHandle = c->kvno;
    memcpy (ct.HandShakeKey, c->session, sizeof(c->session));
    ct.ViceId = uid;
    ct.BeginTimestamp = c->issue_date;
    ct.EndTimestamp = krb_life_to_time(c->issue_date, c->lifetime);
    if(ct.EndTimestamp < time(NULL))
	return 0; /* don't store tokens that has expired (and possibly
		     overwriting valid tokens)*/

#define ODD(x) ((x) & 1)
    /* According to Transarc conventions ViceId is valid iff
     * (EndTimestamp - BeginTimestamp) is odd. By decrementing EndTime
     * the transformations:
     *
     * (issue_date, life) -> (StartTime, EndTime) -> (issue_date, life)
     * preserves the original values.
     */
    if (uid != 0)		/* valid ViceId */
      {
	if (!ODD(ct.EndTimestamp - ct.BeginTimestamp))
	  ct.EndTimestamp--;
      }
    else			/* not valid ViceId */
      {
	if (ODD(ct.EndTimestamp - ct.BeginTimestamp))
	  ct.EndTimestamp--;
      }

    t = buf;
    /*
     * length of secret token followed by secret token
     */
    sizeof_x = c->ticket_st.length;
    memcpy(t, &sizeof_x, sizeof(sizeof_x));
    t += sizeof(sizeof_x);
    memcpy(t, c->ticket_st.dat, sizeof_x);
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
    ret = k_pioctl(0, VIOCSETTOK, &parms, 0);
    return ret;
}

/* Try to get a db-server for an AFS cell from a AFSDB record */

static int
dns_find_cell(const char *cell, char *dbserver, size_t len)
{
    struct dns_reply *r;
    int ok = -1;
    r = dns_lookup(cell, "afsdb");
    if(r){
	struct resource_record *rr = r->head;
	while(rr){
	    if(rr->type == T_AFSDB && rr->u.afsdb->preference == 1){
		strlcpy(dbserver,
				rr->u.afsdb->domain,
				len);
		ok = 0;
		break;
	    }
	    rr = rr->next;
	}
	dns_free_data(r);
    }
    return ok;
}


/*
 * Try to find the cells we should try to klog to in "file".
 */
static void
find_cells(const char *file, char ***cells, int *index)
{
    FILE *f;
    char cell[64];
    int i;
    int ind = *index;

    f = fopen(file, "r");
    if (f == NULL)
	return;
    while (fgets(cell, sizeof(cell), f)) {
	char *t;
	t = cell + strlen(cell);
	for (; t >= cell; t--)
	  if (*t == '\n' || *t == '\t' || *t == ' ')
	    *t = 0;
	if (cell[0] == '\0' || cell[0] == '#')
	    continue;
	for(i = 0; i < ind; i++)
	    if(strcmp((*cells)[i], cell) == 0)
		break;
	if(i == ind){
	    char **tmp;

	    tmp = realloc(*cells, (ind + 1) * sizeof(**cells));
	    if (tmp == NULL)
		break;
	    *cells = tmp;
	    (*cells)[ind] = strdup(cell);
	    if ((*cells)[ind] == NULL)
		break;
	    ++ind;
	}
    }
    fclose(f);
    *index = ind;
}

/*
 * Get tokens for all cells[]
 */
static int
afslog_cells(kafs_data *data, char **cells, int max, uid_t uid,
	     const char *homedir)
{
    int ret = 0;
    int i;
    for (i = 0; i < max; i++) {
        int er = (*data->afslog_uid)(data, cells[i], 0, uid, homedir);
	if (er)
	    ret = er;
    }
    return ret;
}

int
_kafs_afslog_all_local_cells(kafs_data *data, uid_t uid, const char *homedir)
{
    int ret;
    char **cells = NULL;
    int index = 0;

    if (homedir == NULL)
	homedir = getenv("HOME");
    if (homedir != NULL) {
	char home[MaxPathLen];
	snprintf(home, sizeof(home), "%s/.TheseCells", homedir);
	find_cells(home, &cells, &index);
    }
    find_cells(_PATH_THESECELLS, &cells, &index);
    find_cells(_PATH_THISCELL, &cells, &index);
    find_cells(_PATH_ARLA_THESECELLS, &cells, &index);
    find_cells(_PATH_ARLA_THISCELL, &cells, &index);
    find_cells(_PATH_OPENAFS_DEBIAN_THESECELLS, &cells, &index);
    find_cells(_PATH_OPENAFS_DEBIAN_THISCELL, &cells, &index);
    find_cells(_PATH_ARLA_DEBIAN_THESECELLS, &cells, &index);
    find_cells(_PATH_ARLA_DEBIAN_THISCELL, &cells, &index);
    
    ret = afslog_cells(data, cells, index, uid, homedir);
    while(index > 0)
	free(cells[--index]);
    free(cells);
    return ret;
}


static int
file_find_cell(kafs_data *data, const char *cell, char **realm, int exact)
{
    FILE *F;
    char buf[1024];
    char *p;
    int ret = -1;

    if ((F = fopen(_PATH_CELLSERVDB, "r"))
	|| (F = fopen(_PATH_ARLA_CELLSERVDB, "r"))
	|| (F = fopen(_PATH_OPENAFS_DEBIAN_CELLSERVDB, "r"))
	|| (F = fopen(_PATH_ARLA_DEBIAN_CELLSERVDB, "r"))) {
	while (fgets(buf, sizeof(buf), F)) {
	    int cmp;

	    if (buf[0] != '>')
		continue; /* Not a cell name line, try next line */
	    p = buf;
	    strsep(&p, " \t\n#");

	    if (exact)
		cmp = strcmp(buf + 1, cell);
	    else
		cmp = strncmp(buf + 1, cell, strlen(cell));

	    if (cmp == 0) {
		/*
		 * We found the cell name we're looking for.
		 * Read next line on the form ip-address '#' hostname
		 */
		if (fgets(buf, sizeof(buf), F) == NULL)
		    break;	/* Read failed, give up */
		p = strchr(buf, '#');
		if (p == NULL)
		    break;	/* No '#', give up */
		p++;
		if (buf[strlen(buf) - 1] == '\n')
		    buf[strlen(buf) - 1] = '\0';
		*realm = (*data->get_realm)(data, p);
		if (*realm && **realm != '\0')
		    ret = 0;
		break;		/* Won't try any more */
	    }
	}
	fclose(F);
    }
    return ret;
}

/* Find the realm associated with cell. Do this by opening
   /usr/vice/etc/CellServDB and getting the realm-of-host for the
   first VL-server for the cell.

   This does not work when the VL-server is living in one realm, but
   the cell it is serving is living in another realm.

   Return 0 on success, -1 otherwise.
   */

int
_kafs_realm_of_cell(kafs_data *data, const char *cell, char **realm)
{
    char buf[1024];
    int ret;

    ret = file_find_cell(data, cell, realm, 1);
    if (ret == 0)
	return ret;
    if (dns_find_cell(cell, buf, sizeof(buf)) == 0) {
	*realm = (*data->get_realm)(data, buf);
	if(*realm != NULL)
	    return 0;
    }
    return file_find_cell(data, cell, realm, 0);
}

int
_kafs_get_cred(kafs_data *data,
	      const char *cell, 
	      const char *realm_hint,
	      const char *realm,
	      CREDENTIALS *c)
{
    int ret = -1;
    char *vl_realm;
    char CELL[64];

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
     * know and we should try afs@realm_hint.
     */
  
    if (realm_hint) {
	ret = (*data->get_cred)(data, AUTH_SUPERUSER, cell, realm_hint, c);
	if (ret == 0) return 0;
	ret = (*data->get_cred)(data, AUTH_SUPERUSER, "", realm_hint, c);
	if (ret == 0) return 0;
    }

    foldup(CELL, cell);

    /*
     * If cell == realm we don't need no cross-cell authentication.
     * Try afs@REALM.
     */
    if (strcmp(CELL, realm) == 0) {
        ret = (*data->get_cred)(data, AUTH_SUPERUSER, "", realm, c);
	if (ret == 0) return 0;
	/* Try afs.cell@REALM below. */
    }

    /*
     * If the AFS servers have a file /usr/afs/etc/krb.conf containing
     * REALM we still don't have to resort to cross-cell authentication.
     * Try afs.cell@REALM.
     */
    ret = (*data->get_cred)(data, AUTH_SUPERUSER, cell, realm, c);
    if (ret == 0) return 0;

    /*
     * We failed to get ``first class tickets'' for afs,
     * fall back to cross-cell authentication.
     * Try afs@CELL.
     * Try afs.cell@CELL.
     */
    ret = (*data->get_cred)(data, AUTH_SUPERUSER, "", CELL, c);
    if (ret == 0) return 0;
    ret = (*data->get_cred)(data, AUTH_SUPERUSER, cell, CELL, c);
    if (ret == 0) return 0;

    /*
     * Perhaps the cell doesn't correspond to any realm?
     * Use realm of first volume location DB server.
     * Try afs.cell@VL_REALM.
     * Try afs@VL_REALM???
     */
    if (_kafs_realm_of_cell(data, cell, &vl_realm) == 0
	&& strcmp(vl_realm, realm) != 0
	&& strcmp(vl_realm, CELL) != 0) {
	ret = (*data->get_cred)(data, AUTH_SUPERUSER, cell, vl_realm, c);
	if (ret)
	    ret = (*data->get_cred)(data, AUTH_SUPERUSER, "", vl_realm, c);
	free(vl_realm);
	if (ret == 0) return 0;
    }

    return ret;
}
