/*
 * Copyright (c) 1998 Kungliga Tekniska Högskolan
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

#include <popper.h>
#include <dirent.h>
RCSID("$Id: maildir.c,v 1.5 1999/12/02 16:58:33 joda Exp $");

static void
make_path(POP *p, MsgInfoList *mp, int new, char *buf, size_t len)
{
    snprintf(buf, len, "%s/%s%s%s", p->drop_name, 
	     new ? "new" : "cur", mp ? "/" : "", mp ? mp->name : "");
}

static int
scan_file(POP *p, MsgInfoList *mp)
{
    char path[MAXDROPLEN];
    FILE *f;
    char buf[1024];
    int eoh = 0;

    make_path(p, mp, mp->flags & NEW_FLAG, path, sizeof(path));
    f = fopen(path, "r");

    if(f == NULL) {
#ifdef DEBUG
	if(p->debug)
	    pop_log(p, POP_DEBUG, 
		    "Failed to open message file `%s': %s", 
		    path, strerror(errno));
#endif
	return pop_msg (p, POP_FAILURE,
			"Failed to open message file `%s'", path);
    }
    while(fgets(buf, sizeof(buf), f)) {
	if(buf[strlen(buf) - 1] == '\n')
	    mp->lines++;
	mp->length += strlen(buf);
	if(eoh)
	    continue;
	if(strcmp(buf, "\n") == 0)
	    eoh = 1;
	parse_header(mp, buf);
    }
    fclose(f);
    return add_missing_headers(p, mp);
}

static int
scan_dir(POP *p, int new)
{
    char tmp[MAXDROPLEN];
    DIR *dir;
    struct dirent *dent;
    MsgInfoList *mp = p->mlp;
    int n_mp = p->msg_count;
    int e;

    make_path(p, NULL, new, tmp, sizeof(tmp));
    mkdir(tmp, 0700);
    dir = opendir(tmp);
    while((dent = readdir(dir)) != NULL) {
	if(strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0)
	    continue;
	mp = realloc(mp, (n_mp + 1) * sizeof(*mp));
	if(mp == NULL) {
	    p->msg_count = 0;
	    return pop_msg (p, POP_FAILURE,
			    "Can't build message list for '%s': Out of memory",
                            p->user);
	}
	memset(mp + n_mp, 0, sizeof(*mp));
	mp[n_mp].name = strdup(dent->d_name);
	if(mp[n_mp].name == NULL) {
	    p->msg_count = 0;
	    return pop_msg (p, POP_FAILURE,
			    "Can't build message list for '%s': Out of memory",
                            p->user);
	}
	mp[n_mp].number = n_mp + 1;
	mp[n_mp].flags = 0;
	if(new)
	    mp[n_mp].flags |= NEW_FLAG;
	e = scan_file(p, &mp[n_mp]);
	if(e != POP_SUCCESS)
	    return e;
        p->drop_size += mp[n_mp].length;
	n_mp++;
    }
    closedir(dir);
    p->mlp = mp;
    p->msg_count = n_mp;
    return POP_SUCCESS;
}

int
pop_maildir_info(POP *p)
{
    int e;
    
    p->temp_drop[0] = '\0';
    p->mlp = NULL;
    p->msg_count = 0;

    e = scan_dir(p, 0);
    if(e != POP_SUCCESS) return e;

    e = scan_dir(p, 1);
    if(e != POP_SUCCESS) return e;
    return POP_SUCCESS;
}

int
pop_maildir_update(POP *p)
{
    int i;
    char tmp1[MAXDROPLEN], tmp2[MAXDROPLEN];
    for(i = 0; i < p->msg_count; i++) {
	make_path(p, &p->mlp[i], p->mlp[i].flags & NEW_FLAG, 
		  tmp1, sizeof(tmp1));
	if(p->mlp[i].flags & DEL_FLAG) {
#ifdef DEBUG
	    if(p->debug)
		pop_log(p, POP_DEBUG, "Removing `%s'", tmp1);
#endif
	    if(unlink(tmp1) < 0) {
#ifdef DEBUG
		if(p->debug)
		    pop_log(p, POP_DEBUG, "Failed to remove `%s': %s", 
			    tmp1, strerror(errno));
#endif
		/* return failure? */
	    }
	} else if((p->mlp[i].flags & NEW_FLAG) && 
		  (p->mlp[i].flags & RETR_FLAG)) {
	    make_path(p, &p->mlp[i], 0, tmp2, sizeof(tmp2));
#ifdef DEBUG
	    if(p->debug)
		pop_log(p, POP_DEBUG, "Linking `%s' to `%s'", tmp1, tmp2);
#endif
	    if(link(tmp1, tmp2) == 0) {
#ifdef DEBUG
		if(p->debug)
		    pop_log(p, POP_DEBUG, "Removing `%s'", tmp1);
#endif
		if(unlink(tmp1) < 0) {
#ifdef DEBUG
		    if(p->debug)
			pop_log(p, POP_DEBUG, "Failed to remove `%s'", tmp1);
#endif
		    /* return failure? */
		}
	    } else {
		if(errno == EXDEV) {
#ifdef DEBUG
		    if(p->debug)
			pop_log(p, POP_DEBUG, "Trying to rename `%s' to `%s'", 
				tmp1, tmp2);
#endif
		    if(rename(tmp1, tmp2) < 0) {
#ifdef DEBUG
		    if(p->debug)
			pop_log(p, POP_DEBUG, "Failed to rename `%s' to `%s'", 
				tmp1, tmp2);
#endif
		    }
		}
	    }
	}
    }
    return(pop_quit(p));
}

int
pop_maildir_open(POP *p, MsgInfoList *mp)
{
    char tmp[MAXDROPLEN];
    make_path(p, mp, mp->flags & NEW_FLAG, tmp, sizeof(tmp));
    if(p->drop)
	fclose(p->drop);
    p->drop = fopen(tmp, "r");
    if(p->drop == NULL)
	return pop_msg(p, POP_FAILURE, "Failed to open message file");
    return POP_SUCCESS;
}
