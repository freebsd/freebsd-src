/*
 * Copyright (c) 1997 - 2004 Kungliga Tekniska Högskolan
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

RCSID("$Id: fcache.c,v 1.34.6.6 2004/03/10 13:30:59 lha Exp $");

typedef struct krb5_fcache{
    char *filename;
    int version;
}krb5_fcache;

struct fcc_cursor {
    int fd;
    krb5_storage *sp;
};

#define KRB5_FCC_FVNO_1 1
#define KRB5_FCC_FVNO_2 2
#define KRB5_FCC_FVNO_3 3
#define KRB5_FCC_FVNO_4 4

#define FCC_TAG_DELTATIME 1

#define FCACHE(X) ((krb5_fcache*)(X)->data.data)

#define FILENAME(X) (FCACHE(X)->filename)

#define FCC_CURSOR(C) ((struct fcc_cursor*)(C))

static const char*
fcc_get_name(krb5_context context,
	     krb5_ccache id)
{
    return FILENAME(id);
}

int
_krb5_xlock(krb5_context context, int fd, krb5_boolean exclusive,
	    const char *filename)
{
    int ret;
#ifdef HAVE_FCNTL
    struct flock l;

    l.l_start = 0;
    l.l_len = 0;
    l.l_type = exclusive ? F_WRLCK : F_RDLCK;
    l.l_whence = SEEK_SET;
    ret = fcntl(fd, F_SETLKW, &l);
#else
    ret = flock(fd, exclusive ? LOCK_EX : LOCK_SH);
#endif
    if(ret < 0)
	ret = errno;
    if(ret == EACCES) /* fcntl can return EACCES instead of EAGAIN */
	ret = EAGAIN;

    switch (ret) {
    case 0:
	break;
    case EINVAL: /* filesystem doesn't support locking, let the user have it */
	ret = 0; 
	break;
    case EAGAIN:
	krb5_set_error_string(context, "timed out locking cache file %s", 
			      filename);
	break;
    default:
	krb5_set_error_string(context, "error locking cache file %s: %s",
			      filename, strerror(ret));
	break;
    }
    return ret;
}

int
_krb5_xunlock(int fd)
{
#ifdef HAVE_FCNTL_LOCK
    struct flock l;
    l.l_start = 0;
    l.l_len = 0;
    l.l_type = F_UNLCK;
    l.l_whence = SEEK_SET;
    return fcntl(fd, F_SETLKW, &l);
#else
    return flock(fd, LOCK_UN);
#endif
}

static krb5_error_code
fcc_lock(krb5_context context, krb5_ccache id,
	 int fd, krb5_boolean exclusive)
{
    return _krb5_xlock(context, fd, exclusive, fcc_get_name(context, id));
}

static krb5_error_code
fcc_unlock(krb5_context context, int fd)
{
    return _krb5_xunlock(fd);
}

static krb5_error_code
fcc_resolve(krb5_context context, krb5_ccache *id, const char *res)
{
    krb5_fcache *f;
    f = malloc(sizeof(*f));
    if(f == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return KRB5_CC_NOMEM;
    }
    f->filename = strdup(res);
    if(f->filename == NULL){
	free(f);
	krb5_set_error_string(context, "malloc: out of memory");
	return KRB5_CC_NOMEM;
    }
    f->version = 0;
    (*id)->data.data = f;
    (*id)->data.length = sizeof(*f);
    return 0;
}

/*
 * Try to scrub the contents of `filename' safely.
 */

static int
scrub_file (int fd)
{
    off_t pos;
    char buf[128];

    pos = lseek(fd, 0, SEEK_END);
    if (pos < 0)
        return errno;
    if (lseek(fd, 0, SEEK_SET) < 0)
        return errno;
    memset(buf, 0, sizeof(buf));
    while(pos > 0) {
        ssize_t tmp = write(fd, buf, min(sizeof(buf), pos));

	if (tmp < 0)
	    return errno;
	pos -= tmp;
    }
    fsync (fd);
    return 0;
}

/*
 * Erase `filename' if it exists, trying to remove the contents if
 * it's `safe'.  We always try to remove the file, it it exists.  It's
 * only overwritten if it's a regular file (not a symlink and not a
 * hardlink)
 */

static krb5_error_code
erase_file(const char *filename)
{
    int fd;
    struct stat sb1, sb2;
    int ret;

    ret = lstat (filename, &sb1);
    if (ret < 0)
	return errno;

    fd = open(filename, O_RDWR | O_BINARY);
    if(fd < 0) {
	if(errno == ENOENT)
	    return 0;
	else
	    return errno;
    }
    if (unlink(filename) < 0) {
        close (fd);
        return errno;
    }
    ret = fstat (fd, &sb2);
    if (ret < 0) {
	close (fd);
	return errno;
    }

    /* check if someone was playing with symlinks */

    if (sb1.st_dev != sb2.st_dev || sb1.st_ino != sb2.st_ino) {
	close (fd);
	return EPERM;
    }

    /* there are still hard links to this file */

    if (sb2.st_nlink != 0) {
        close (fd);
        return 0;
    }

    ret = scrub_file (fd);
    close (fd);
    return ret;
}

static krb5_error_code
fcc_gen_new(krb5_context context, krb5_ccache *id)
{
    krb5_fcache *f;
    int fd;
    char *file;

    f = malloc(sizeof(*f));
    if(f == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return KRB5_CC_NOMEM;
    }
    asprintf (&file, "%sXXXXXX", KRB5_DEFAULT_CCFILE_ROOT);
    if(file == NULL) {
	free(f);
	krb5_set_error_string(context, "malloc: out of memory");
	return KRB5_CC_NOMEM;
    }
    fd = mkstemp(file);
    if(fd < 0) {
	free(f);
	free(file);
	krb5_set_error_string(context, "mkstemp %s", file);
	return errno;
    }
    close(fd);
    f->filename = file;
    f->version = 0;
    (*id)->data.data = f;
    (*id)->data.length = sizeof(*f);
    return 0;
}

static void
storage_set_flags(krb5_context context, krb5_storage *sp, int vno)
{
    int flags = 0;
    switch(vno) {
    case KRB5_FCC_FVNO_1:
	flags |= KRB5_STORAGE_PRINCIPAL_WRONG_NUM_COMPONENTS;
	flags |= KRB5_STORAGE_PRINCIPAL_NO_NAME_TYPE;
	flags |= KRB5_STORAGE_HOST_BYTEORDER;
	break;
    case KRB5_FCC_FVNO_2:
	flags |= KRB5_STORAGE_HOST_BYTEORDER;
	break;
    case KRB5_FCC_FVNO_3:
	flags |= KRB5_STORAGE_KEYBLOCK_KEYTYPE_TWICE;
	break;
    case KRB5_FCC_FVNO_4:
	break;
    default:
	krb5_abortx(context, 
		    "storage_set_flags called with bad vno (%x)", vno);
    }
    krb5_storage_set_flags(sp, flags);
}

static krb5_error_code
fcc_open(krb5_context context,
	 krb5_ccache id,
	 int *fd_ret,
	 int flags,
	 mode_t mode)
{
    krb5_boolean exclusive = ((flags | O_WRONLY) == flags ||
			      (flags | O_RDWR) == flags);
    krb5_error_code ret;
    const char *filename = FILENAME(id);
    int fd;
    fd = open(filename, flags, mode);
    if(fd < 0) {
	ret = errno;
	krb5_set_error_string(context, "open(%s): %s", filename,
			      strerror(ret));
	return ret;
    }
	
    if((ret = fcc_lock(context, id, fd, exclusive)) != 0) {
	close(fd);
	return ret;
    }
    *fd_ret = fd;
    return 0;
}

static krb5_error_code
fcc_initialize(krb5_context context,
	       krb5_ccache id,
	       krb5_principal primary_principal)
{
    krb5_fcache *f = FCACHE(id);
    int ret = 0;
    int fd;
    char *filename = f->filename;

    unlink (filename);
  
    ret = fcc_open(context, id, &fd, O_RDWR | O_CREAT | O_EXCL | O_BINARY, 0600);
    if(ret)
	return ret;
    {
	krb5_storage *sp;    
	sp = krb5_storage_from_fd(fd);
	krb5_storage_set_eof_code(sp, KRB5_CC_END);
	if(context->fcache_vno != 0)
	    f->version = context->fcache_vno;
	else
	    f->version = KRB5_FCC_FVNO_4;
	ret |= krb5_store_int8(sp, 5);
	ret |= krb5_store_int8(sp, f->version);
	storage_set_flags(context, sp, f->version);
	if(f->version == KRB5_FCC_FVNO_4 && ret == 0) {
	    /* V4 stuff */
	    if (context->kdc_sec_offset) {
		ret |= krb5_store_int16 (sp, 12); /* length */
		ret |= krb5_store_int16 (sp, FCC_TAG_DELTATIME); /* Tag */
		ret |= krb5_store_int16 (sp, 8); /* length of data */
		ret |= krb5_store_int32 (sp, context->kdc_sec_offset);
		ret |= krb5_store_int32 (sp, context->kdc_usec_offset);
	    } else {
		ret |= krb5_store_int16 (sp, 0);
	    }
	}
	ret |= krb5_store_principal(sp, primary_principal);
	
	krb5_storage_free(sp);
    }
    fcc_unlock(context, fd);
    if (close(fd) < 0)
	if (ret == 0) {
	    ret = errno;
	    krb5_set_error_string (context, "close %s: %s", 
				   FILENAME(id), strerror(ret));
	}
    return ret;
}

static krb5_error_code
fcc_close(krb5_context context,
	  krb5_ccache id)
{
    free (FILENAME(id));
    krb5_data_free(&id->data);
    return 0;
}

static krb5_error_code
fcc_destroy(krb5_context context,
	    krb5_ccache id)
{
    erase_file(FILENAME(id));
    return 0;
}

static krb5_error_code
fcc_store_cred(krb5_context context,
	       krb5_ccache id,
	       krb5_creds *creds)
{
    int ret;
    int fd;

    ret = fcc_open(context, id, &fd, O_WRONLY | O_APPEND | O_BINARY, 0);
    if(ret)
	return ret;
    {
	krb5_storage *sp;
	sp = krb5_storage_from_fd(fd);
	krb5_storage_set_eof_code(sp, KRB5_CC_END);
	storage_set_flags(context, sp, FCACHE(id)->version);
	if (krb5_config_get_bool_default(context, NULL, FALSE,
					 "libdefaults",
					 "fcc-mit-ticketflags",
					 NULL))
	    ret = _krb5_store_creds_heimdal_0_7(sp, creds);
	else
	    ret = _krb5_store_creds_heimdal_pre_0_7(sp, creds);
	krb5_storage_free(sp);
    }
    fcc_unlock(context, fd);
    if (close(fd) < 0)
	if (ret == 0) {
	    ret = errno;
	    krb5_set_error_string (context, "close %s: %s", 
				   FILENAME(id), strerror(ret));
	}
    return ret;
}

static krb5_error_code
init_fcc (krb5_context context,
	  krb5_ccache id,
	  krb5_storage **ret_sp,
	  int *ret_fd)
{
    int fd;
    int8_t pvno, tag;
    krb5_storage *sp;
    krb5_error_code ret;

    ret = fcc_open(context, id, &fd, O_RDONLY | O_BINARY, 0);

    if(ret)
	return ret;
    
    sp = krb5_storage_from_fd(fd);
    if(sp == NULL) {
	ret = ENOMEM;
	goto out;
    }
    krb5_storage_set_eof_code(sp, KRB5_CC_END);
    ret = krb5_ret_int8(sp, &pvno);
    if(ret != 0) {
	if(ret == KRB5_CC_END)
	    ret = ENOENT; /* empty file */
	goto out;
    }
    if(pvno != 5) {
	ret = KRB5_CCACHE_BADVNO;
	goto out;
    }
    ret = krb5_ret_int8(sp, &tag); /* should not be host byte order */
    if(ret != 0) {
	ret = KRB5_CC_FORMAT;
	goto out;
    }
    FCACHE(id)->version = tag;
    storage_set_flags(context, sp, FCACHE(id)->version);
    switch (tag) {
    case KRB5_FCC_FVNO_4: {
	int16_t length;

	ret = krb5_ret_int16 (sp, &length);
	if(ret) {
	    ret = KRB5_CC_FORMAT;
	    goto out;
	}
	while(length > 0) {
	    int16_t tag, data_len;
	    int i;
	    int8_t dummy;

	    ret = krb5_ret_int16 (sp, &tag);
	    if(ret) {
		ret = KRB5_CC_FORMAT;
		goto out;
	    }
	    ret = krb5_ret_int16 (sp, &data_len);
	    if(ret) {
		ret = KRB5_CC_FORMAT;
		goto out;
	    }
	    switch (tag) {
	    case FCC_TAG_DELTATIME :
		ret = krb5_ret_int32 (sp, &context->kdc_sec_offset);
		if(ret) {
		    ret = KRB5_CC_FORMAT;
		    goto out;
		}
		ret = krb5_ret_int32 (sp, &context->kdc_usec_offset);
		if(ret) {
		    ret = KRB5_CC_FORMAT;
		    goto out;
		}
		break;
	    default :
		for (i = 0; i < data_len; ++i) {
		    ret = krb5_ret_int8 (sp, &dummy);
		    if(ret) {
			ret = KRB5_CC_FORMAT;
			goto out;
		    }
		}
		break;
	    }
	    length -= 4 + data_len;
	}
	break;
    }
    case KRB5_FCC_FVNO_3:
    case KRB5_FCC_FVNO_2:
    case KRB5_FCC_FVNO_1:
	break;
    default :
	ret = KRB5_CCACHE_BADVNO;
	goto out;
    }
    *ret_sp = sp;
    *ret_fd = fd;
    
    return 0;
  out:
    if(sp != NULL)
	krb5_storage_free(sp);
    fcc_unlock(context, fd);
    close(fd);
    return ret;
}

static krb5_error_code
fcc_get_principal(krb5_context context,
		  krb5_ccache id,
		  krb5_principal *principal)
{
    krb5_error_code ret;
    int fd;
    krb5_storage *sp;

    ret = init_fcc (context, id, &sp, &fd);
    if (ret)
	return ret;
    ret = krb5_ret_principal(sp, principal);
    krb5_storage_free(sp);
    fcc_unlock(context, fd);
    close(fd);
    return ret;
}

static krb5_error_code
fcc_end_get (krb5_context context,
	     krb5_ccache id,
	     krb5_cc_cursor *cursor);

static krb5_error_code
fcc_get_first (krb5_context context,
	       krb5_ccache id,
	       krb5_cc_cursor *cursor)
{
    krb5_error_code ret;
    krb5_principal principal;

    *cursor = malloc(sizeof(struct fcc_cursor));

    ret = init_fcc (context, id, &FCC_CURSOR(*cursor)->sp, 
		    &FCC_CURSOR(*cursor)->fd);
    if (ret) {
	free(*cursor);
	return ret;
    }
    ret = krb5_ret_principal (FCC_CURSOR(*cursor)->sp, &principal);
    if(ret) {
	fcc_end_get(context, id, cursor);
	return ret;
    }
    krb5_free_principal (context, principal);
    fcc_unlock(context, FCC_CURSOR(*cursor)->fd);
    return 0;
}

static krb5_error_code
fcc_get_next (krb5_context context,
	      krb5_ccache id,
	      krb5_cc_cursor *cursor,
	      krb5_creds *creds)
{
    krb5_error_code ret;
    if((ret = fcc_lock(context, id, FCC_CURSOR(*cursor)->fd, FALSE)) != 0)
	return ret;

    ret = krb5_ret_creds(FCC_CURSOR(*cursor)->sp, creds);

    fcc_unlock(context, FCC_CURSOR(*cursor)->fd);
    return ret;
}

static krb5_error_code
fcc_end_get (krb5_context context,
	     krb5_ccache id,
	     krb5_cc_cursor *cursor)
{
    krb5_storage_free(FCC_CURSOR(*cursor)->sp);
    close (FCC_CURSOR(*cursor)->fd);
    free(*cursor);
    *cursor = NULL;
    return 0;
}

static krb5_error_code
fcc_remove_cred(krb5_context context,
		 krb5_ccache id,
		 krb5_flags which,
		 krb5_creds *cred)
{
    return 0; /* XXX */
}

static krb5_error_code
fcc_set_flags(krb5_context context,
	      krb5_ccache id,
	      krb5_flags flags)
{
    return 0; /* XXX */
}

static krb5_error_code
fcc_get_version(krb5_context context,
		krb5_ccache id)
{
    return FCACHE(id)->version;
}
		    
const krb5_cc_ops krb5_fcc_ops = {
    "FILE",
    fcc_get_name,
    fcc_resolve,
    fcc_gen_new,
    fcc_initialize,
    fcc_destroy,
    fcc_close,
    fcc_store_cred,
    NULL, /* fcc_retrieve */
    fcc_get_principal,
    fcc_get_first,
    fcc_get_next,
    fcc_end_get,
    fcc_remove_cred,
    fcc_set_flags,
    fcc_get_version
};
