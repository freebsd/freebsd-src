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

#include "krb5_locl.h"

RCSID("$Id: keytab_krb4.c,v 1.9 2002/02/11 14:05:10 joda Exp $");

struct krb4_kt_data {
    char *filename;
};

static krb5_error_code
krb4_kt_resolve(krb5_context context, const char *name, krb5_keytab id)
{
    struct krb4_kt_data *d;

    d = malloc (sizeof(*d));
    if (d == NULL) {
	krb5_set_error_string (context, "malloc: out of memory");
	return ENOMEM;
    }
    d->filename = strdup (name);
    if (d->filename == NULL) {
	free(d);
	krb5_set_error_string (context, "malloc: out of memory");
	return ENOMEM;
    }
    id->data = d;
    return 0;
}

static krb5_error_code
krb4_kt_get_name (krb5_context context,
		  krb5_keytab id,
		  char *name,
		  size_t name_sz)
{
    struct krb4_kt_data *d = id->data;

    strlcpy (name, d->filename, name_sz);
    return 0;
}

static krb5_error_code
krb4_kt_close (krb5_context context,
	       krb5_keytab id)
{
    struct krb4_kt_data *d = id->data;

    free (d->filename);
    free (d);
    return 0;
}

struct krb4_cursor_extra_data {
    krb5_keytab_entry entry;
    int num;
};

static int
open_flock(const char *filename, int flags, int mode)
{
    int lock_mode;
    int tries = 0;
    int fd = open(filename, flags, mode);
    if(fd < 0)
	return fd;
    if((flags & O_ACCMODE) == O_RDONLY)
	lock_mode = LOCK_SH | LOCK_NB;
    else
	lock_mode = LOCK_EX | LOCK_NB;
    while(flock(fd, lock_mode) < 0) {
	if(++tries < 5) {
	    sleep(1);
	} else {
	    close(fd);
	    return -1;
	}
    }
    return fd;
}



static krb5_error_code
krb4_kt_start_seq_get_int (krb5_context context,
			   krb5_keytab id,
			   int flags,
			   krb5_kt_cursor *c)
{
    struct krb4_kt_data *d = id->data;
    struct krb4_cursor_extra_data *ed;
    int ret;

    ed = malloc (sizeof(*ed));
    if (ed == NULL) {
	krb5_set_error_string (context, "malloc: out of memory");
	return ENOMEM;
    }
    ed->entry.principal = NULL;
    ed->num = -1;
    c->data = ed;
    c->fd = open_flock (d->filename, flags, 0);
    if (c->fd < 0) {
	ret = errno;
	free (ed);
	krb5_set_error_string(context, "open(%s): %s", d->filename,
			      strerror(ret));
	return ret;
    }
    c->sp = krb5_storage_from_fd(c->fd);
    return 0;
}

static krb5_error_code
krb4_kt_start_seq_get (krb5_context context,
		       krb5_keytab id,
		       krb5_kt_cursor *c)
{
    return krb4_kt_start_seq_get_int (context, id, O_BINARY | O_RDONLY, c);
}

static krb5_error_code
read_v4_entry (krb5_context context,
	       struct krb4_kt_data *d,
	       krb5_kt_cursor *c,
	       struct krb4_cursor_extra_data *ed)
{
    krb5_error_code ret;
    char *service, *instance, *realm;
    int8_t kvno;
    des_cblock key;

    ret = krb5_ret_stringz(c->sp, &service);
    if (ret)
	return ret;
    ret = krb5_ret_stringz(c->sp, &instance);
    if (ret) {
	free (service);
	return ret;
    }
    ret = krb5_ret_stringz(c->sp, &realm);
    if (ret) {
	free (service);
	free (instance);
	return ret;
    }
    ret = krb5_425_conv_principal (context, service, instance, realm,
				   &ed->entry.principal);
    free (service);
    free (instance);
    free (realm);
    if (ret)
	return ret;
    ret = krb5_ret_int8(c->sp, &kvno);
    if (ret) {
	krb5_free_principal (context, ed->entry.principal);
	return ret;
    }
    ret = c->sp->fetch(c->sp, key, 8);
    if (ret < 0) {
	krb5_free_principal(context, ed->entry.principal);
	return ret;
    }
    if (ret < 8) {
	krb5_free_principal(context, ed->entry.principal);
	return EINVAL;
    }
    ed->entry.vno = kvno;
    ret = krb5_data_copy (&ed->entry.keyblock.keyvalue,
			  key, 8);
    if (ret)
	return ret;
    ed->entry.timestamp = time(NULL);
    ed->num = 0;
    return 0;
}

static krb5_error_code
krb4_kt_next_entry (krb5_context context,
		    krb5_keytab id,
		    krb5_keytab_entry *entry,
		    krb5_kt_cursor *c)
{
    krb5_error_code ret;
    struct krb4_kt_data *d = id->data;
    struct krb4_cursor_extra_data *ed = c->data;
    const krb5_enctype keytypes[] = {ETYPE_DES_CBC_MD5,
				     ETYPE_DES_CBC_MD4,
				     ETYPE_DES_CBC_CRC};

    if (ed->num == -1) {
	ret = read_v4_entry (context, d, c, ed);
	if (ret)
	    return ret;
    }
    ret = krb5_kt_copy_entry_contents (context,
				       &ed->entry,
				       entry);
    if (ret)
	return ret;
    entry->keyblock.keytype = keytypes[ed->num];
    if (++ed->num == 3) {
	krb5_kt_free_entry (context, &ed->entry);
	ed->num = -1;
    }
    return 0;
}

static krb5_error_code
krb4_kt_end_seq_get (krb5_context context,
		     krb5_keytab id,
		     krb5_kt_cursor *c)
{
    struct krb4_cursor_extra_data *ed = c->data;

    krb5_storage_free (c->sp);
    if (ed->num != -1)
	krb5_kt_free_entry (context, &ed->entry);
    free (c->data);
    close (c->fd);
    return 0;
}

static krb5_error_code
krb4_store_keytab_entry(krb5_context context, 
			krb5_keytab_entry *entry, 
			krb5_storage *sp)
{
    krb5_error_code ret;
#define ANAME_SZ 40
#define INST_SZ 40
#define REALM_SZ 40
    char service[ANAME_SZ];
    char instance[INST_SZ];
    char realm[REALM_SZ];
    ret = krb5_524_conv_principal (context, entry->principal,
				   service, instance, realm);
    if (ret)
	return ret;
    if (entry->keyblock.keyvalue.length == 8
	&& entry->keyblock.keytype == ETYPE_DES_CBC_MD5) {
	ret = krb5_store_stringz(sp, service);
	ret = krb5_store_stringz(sp, instance);
	ret = krb5_store_stringz(sp, realm);
	ret = krb5_store_int8(sp, entry->vno);
	ret = (*sp->store)(sp, entry->keyblock.keyvalue.data, 8);
    }
    return 0;
}

static krb5_error_code
krb4_kt_add_entry (krb5_context context,
		   krb5_keytab id,
		   krb5_keytab_entry *entry)
{
    struct krb4_kt_data *d = id->data;
    krb5_storage *sp;
    krb5_error_code ret;
    int fd;

    fd = open_flock (d->filename, O_WRONLY | O_APPEND | O_BINARY, 0);
    if (fd < 0) {
	fd = open_flock (d->filename,
		   O_WRONLY | O_APPEND | O_BINARY | O_CREAT, 0600);
	if (fd < 0) {
	    ret = errno;
	    krb5_set_error_string(context, "open(%s): %s", d->filename,
				  strerror(ret));
	    return ret;
	}
    }
    sp = krb5_storage_from_fd(fd);
    if(sp == NULL) {
	close(fd);
	return ENOMEM;
    }
    ret = krb4_store_keytab_entry(context, entry, sp);
    krb5_storage_free(sp);
    if(close (fd) < 0)
	return errno;
    return ret;
}

static krb5_error_code
krb4_kt_remove_entry(krb5_context context,
		 krb5_keytab id,
		 krb5_keytab_entry *entry)
{
    struct krb4_kt_data *d = id->data;
    krb5_error_code ret;
    krb5_keytab_entry e;
    krb5_kt_cursor cursor;
    krb5_storage *sp;
    int remove_flag = 0;
    
    sp = krb5_storage_emem();
    ret = krb5_kt_start_seq_get(context, id, &cursor);
    while(krb5_kt_next_entry(context, id, &e, &cursor) == 0) {
	if(!krb5_kt_compare(context, &e, entry->principal, 
			    entry->vno, entry->keyblock.keytype)) {
	    ret = krb4_store_keytab_entry(context, &e, sp);
	    if(ret) {
		krb5_storage_free(sp);
		return ret;
	    }
	} else
	    remove_flag = 1;
    }
    krb5_kt_end_seq_get(context, id, &cursor);
    if(remove_flag) {
	int fd;
	unsigned char buf[1024];
	ssize_t n;
	krb5_data data;
	struct stat st;

	krb5_storage_to_data(sp, &data);
	krb5_storage_free(sp);

	fd = open_flock (d->filename, O_RDWR | O_BINARY, 0);
	if(fd < 0) {
	    memset(data.data, 0, data.length);
	    krb5_data_free(&data);
	    if(errno == EACCES || errno == EROFS) 
		return KRB5_KT_NOWRITE;
	    return errno;
	}

	if(write(fd, data.data, data.length) != data.length) {
	    memset(data.data, 0, data.length);
	    close(fd);
	    krb5_set_error_string(context, "failed writing to \"%s\"", d->filename);
	    return errno;
	}
	memset(data.data, 0, data.length);
	if(fstat(fd, &st) < 0) {
	    close(fd);
	    krb5_set_error_string(context, "failed getting size of \"%s\"", d->filename);
	    return errno;
	}
	st.st_size -= data.length;
	memset(buf, 0, sizeof(buf));
	while(st.st_size > 0) {
	    n = min(st.st_size, sizeof(buf));
	    n = write(fd, buf, n);
	    if(n <= 0) {
		close(fd);
		krb5_set_error_string(context, "failed writing to \"%s\"", d->filename);
		return errno;
		
	    }
	    st.st_size -= n;
	}
	if(ftruncate(fd, data.length) < 0) {
	    close(fd);
	    krb5_set_error_string(context, "failed truncating \"%s\"", d->filename);
	    return errno;
	}
	krb5_data_free(&data);
	if(close(fd) < 0) {
	    krb5_set_error_string(context, "error closing \"%s\"", d->filename);
	    return errno;
	}
	return 0;
    } else
	return KRB5_KT_NOTFOUND;
}


const krb5_kt_ops krb4_fkt_ops = {
    "krb4",
    krb4_kt_resolve,
    krb4_kt_get_name,
    krb4_kt_close,
    NULL,			/* get */
    krb4_kt_start_seq_get,
    krb4_kt_next_entry,
    krb4_kt_end_seq_get,
    krb4_kt_add_entry,		/* add_entry */
    krb4_kt_remove_entry	/* remove_entry */
};

const krb5_kt_ops krb5_srvtab_fkt_ops = {
    "SRVTAB",
    krb4_kt_resolve,
    krb4_kt_get_name,
    krb4_kt_close,
    NULL,			/* get */
    krb4_kt_start_seq_get,
    krb4_kt_next_entry,
    krb4_kt_end_seq_get,
    krb4_kt_add_entry,		/* add_entry */
    krb4_kt_remove_entry	/* remove_entry */
};
