/*
 * Copyright (c) 1997 - 2005 Kungliga Tekniska Högskolan
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
RCSID("$Id: v4_glue.c 22071 2007-11-14 20:04:50Z lha $");

#include "krb5-v4compat.h"

/*
 *
 */

#define RCHECK(r,func,label) \
	do { (r) = func ; if (r) goto label; } while(0);


/* include this here, to avoid dependencies on libkrb */

static const int _tkt_lifetimes[TKTLIFENUMFIXED] = {
   38400,   41055,   43894,   46929,   50174,   53643,   57352,   61318,
   65558,   70091,   74937,   80119,   85658,   91581,   97914,  104684,
  111922,  119661,  127935,  136781,  146239,  156350,  167161,  178720,
  191077,  204289,  218415,  233517,  249664,  266926,  285383,  305116,
  326213,  348769,  372885,  398668,  426234,  455705,  487215,  520904,
  556921,  595430,  636601,  680618,  727680,  777995,  831789,  889303,
  950794, 1016537, 1086825, 1161973, 1242318, 1328218, 1420057, 1518247,
 1623226, 1735464, 1855462, 1983758, 2120925, 2267576, 2424367, 2592000
};

int KRB5_LIB_FUNCTION
_krb5_krb_time_to_life(time_t start, time_t end)
{
    int i;
    time_t life = end - start;

    if (life > MAXTKTLIFETIME || life <= 0) 
	return 0;
#if 0    
    if (krb_no_long_lifetimes) 
	return (life + 5*60 - 1)/(5*60);
#endif
    
    if (end >= NEVERDATE)
	return TKTLIFENOEXPIRE;
    if (life < _tkt_lifetimes[0]) 
	return (life + 5*60 - 1)/(5*60);
    for (i=0; i<TKTLIFENUMFIXED; i++)
	if (life <= _tkt_lifetimes[i])
	    return i + TKTLIFEMINFIXED;
    return 0;
    
}

time_t KRB5_LIB_FUNCTION
_krb5_krb_life_to_time(int start, int life_)
{
    unsigned char life = (unsigned char) life_;

#if 0    
    if (krb_no_long_lifetimes)
	return start + life*5*60;
#endif

    if (life == TKTLIFENOEXPIRE)
	return NEVERDATE;
    if (life < TKTLIFEMINFIXED)
	return start + life*5*60;
    if (life > TKTLIFEMAXFIXED)
	return start + MAXTKTLIFETIME;
    return start + _tkt_lifetimes[life - TKTLIFEMINFIXED];
}

/*
 * Get the name of the krb4 credentials cache, will use `tkfile' as
 * the name if that is passed in. `cc' must be free()ed by caller,
 */

static krb5_error_code
get_krb4_cc_name(const char *tkfile, char **cc)
{

    *cc = NULL;
    if(tkfile == NULL) {
	char *path;
	if(!issuid()) {
	    path = getenv("KRBTKFILE");
	    if (path)
		*cc = strdup(path);
	}
	if(*cc == NULL)
	    if (asprintf(cc, "%s%u", TKT_ROOT, (unsigned)getuid()) < 0)
		return errno;
    } else {
	*cc = strdup(tkfile);
	if (*cc == NULL)
	    return ENOMEM;
    }
    return 0;
}

/*
 * Write a Kerberos 4 ticket file
 */

#define KRB5_TF_LCK_RETRY_COUNT 50
#define KRB5_TF_LCK_RETRY 1

static krb5_error_code
write_v4_cc(krb5_context context, const char *tkfile, 
	    krb5_storage *sp, int append)
{
    krb5_error_code ret;
    struct stat sb;
    krb5_data data;
    char *path;
    int fd, i;

    ret = get_krb4_cc_name(tkfile, &path);
    if (ret) {
	krb5_set_error_string(context, 
			      "krb5_krb_tf_setup: failed getting "
			      "the krb4 credentials cache name"); 
	return ret;
    }

    fd = open(path, O_WRONLY|O_CREAT, 0600);
    if (fd < 0) {
	ret = errno;
	krb5_set_error_string(context, 
			      "krb5_krb_tf_setup: error opening file %s", 
			      path);
	free(path);
	return ret;
    }

    if (fstat(fd, &sb) != 0 || !S_ISREG(sb.st_mode)) {
	krb5_set_error_string(context, 
			      "krb5_krb_tf_setup: tktfile %s is not a file",
			      path);
	free(path);
	close(fd);
	return KRB5_FCC_PERM;
    }

    for (i = 0; i < KRB5_TF_LCK_RETRY_COUNT; i++) {
	if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
	    sleep(KRB5_TF_LCK_RETRY);
	} else
	    break;
    }
    if (i == KRB5_TF_LCK_RETRY_COUNT) {
	krb5_set_error_string(context,
			      "krb5_krb_tf_setup: failed to lock %s",
			      path);
	free(path);
	close(fd);
	return KRB5_FCC_PERM;
    }

    if (!append) {
	ret = ftruncate(fd, 0);
	if (ret < 0) {
	    flock(fd, LOCK_UN);
	    krb5_set_error_string(context,
				  "krb5_krb_tf_setup: failed to truncate %s",
				  path);
	    free(path);
	    close(fd);
	    return KRB5_FCC_PERM;
	}
    }
    ret = lseek(fd, 0L, SEEK_END);
    if (ret < 0) {
	ret = errno;
	flock(fd, LOCK_UN);
	free(path);
	close(fd);
	return ret;
    }

    krb5_storage_to_data(sp, &data);

    ret = write(fd, data.data, data.length);
    if (ret != data.length)
	ret = KRB5_CC_IO;

    krb5_free_data_contents(context, &data);

    flock(fd, LOCK_UN);
    free(path);
    close(fd);

    return 0;
}

/*
 *
 */

krb5_error_code KRB5_LIB_FUNCTION
_krb5_krb_tf_setup(krb5_context context, 
		   struct credentials *v4creds, 
		   const char *tkfile,
		   int append)
{
    krb5_error_code ret;
    krb5_storage *sp;

    sp = krb5_storage_emem();
    if (sp == NULL)
	return ENOMEM;

    krb5_storage_set_byteorder(sp, KRB5_STORAGE_BYTEORDER_HOST);
    krb5_storage_set_eof_code(sp, KRB5_CC_IO);

    krb5_clear_error_string(context);

    if (!append) {
	RCHECK(ret, krb5_store_stringz(sp, v4creds->pname), error);
	RCHECK(ret, krb5_store_stringz(sp, v4creds->pinst), error);
    }

    /* cred */
    RCHECK(ret, krb5_store_stringz(sp, v4creds->service), error);
    RCHECK(ret, krb5_store_stringz(sp, v4creds->instance), error);
    RCHECK(ret, krb5_store_stringz(sp, v4creds->realm), error);
    ret = krb5_storage_write(sp, v4creds->session, 8);
    if (ret != 8) {
	ret = KRB5_CC_IO;
	goto error;
    }
    RCHECK(ret, krb5_store_int32(sp, v4creds->lifetime), error);
    RCHECK(ret, krb5_store_int32(sp, v4creds->kvno), error);
    RCHECK(ret, krb5_store_int32(sp, v4creds->ticket_st.length), error);

    ret = krb5_storage_write(sp, v4creds->ticket_st.dat, 
			     v4creds->ticket_st.length);
    if (ret != v4creds->ticket_st.length) {
	ret = KRB5_CC_IO;
	goto error;
    }
    RCHECK(ret, krb5_store_int32(sp, v4creds->issue_date), error);

    ret = write_v4_cc(context, tkfile, sp, append);

 error:
    krb5_storage_free(sp);

    return ret;
}

/*
 *
 */

krb5_error_code KRB5_LIB_FUNCTION
_krb5_krb_dest_tkt(krb5_context context, const char *tkfile)
{
    krb5_error_code ret;
    char *path;

    ret = get_krb4_cc_name(tkfile, &path);
    if (ret) {
	krb5_set_error_string(context, 
			      "krb5_krb_tf_setup: failed getting "
			      "the krb4 credentials cache name"); 
	return ret;
    }

    if (unlink(path) < 0) {
	ret = errno;
	krb5_set_error_string(context, 
			      "krb5_krb_dest_tkt failed removing the cache "
			      "with error %s", strerror(ret));
    }
    free(path);

    return ret;
}

/*
 *
 */

static krb5_error_code
decrypt_etext(krb5_context context, const krb5_keyblock *key,
	      const krb5_data *cdata, krb5_data *data)
{
    krb5_error_code ret;
    krb5_crypto crypto;

    ret = krb5_crypto_init(context, key, ETYPE_DES_PCBC_NONE, &crypto);
    if (ret)
	return ret;

    ret = krb5_decrypt(context, crypto, 0, cdata->data, cdata->length, data);
    krb5_crypto_destroy(context, crypto);

    return ret;
}


/*
 *
 */

static const char eightzeros[8] = "\x00\x00\x00\x00\x00\x00\x00\x00";

static krb5_error_code
storage_to_etext(krb5_context context,
		 krb5_storage *sp,
		 const krb5_keyblock *key, 
		 krb5_data *enc_data)
{
    krb5_error_code ret;
    krb5_crypto crypto;
    krb5_ssize_t size;
    krb5_data data;

    /* multiple of eight bytes */

    size = krb5_storage_seek(sp, 0, SEEK_END);
    if (size < 0)
	return KRB4ET_RD_AP_UNDEC;
    size = 8 - (size & 7);

    ret = krb5_storage_write(sp, eightzeros, size);
    if (ret != size)
	return KRB4ET_RD_AP_UNDEC;

    ret = krb5_storage_to_data(sp, &data);
    if (ret)
	return ret;

    ret = krb5_crypto_init(context, key, ETYPE_DES_PCBC_NONE, &crypto);
    if (ret) {
	krb5_data_free(&data);
	return ret;
    }

    ret = krb5_encrypt(context, crypto, 0, data.data, data.length, enc_data);

    krb5_data_free(&data);
    krb5_crypto_destroy(context, crypto);

    return ret;
}

/*
 *
 */

static krb5_error_code
put_nir(krb5_storage *sp, const char *name,
	const char *instance, const char *realm)
{
    krb5_error_code ret;

    RCHECK(ret, krb5_store_stringz(sp, name), error);
    RCHECK(ret, krb5_store_stringz(sp, instance), error);
    if (realm) {
	RCHECK(ret, krb5_store_stringz(sp, realm), error);
    }
 error:
    return ret;
}

/*
 *
 */

krb5_error_code KRB5_LIB_FUNCTION
_krb5_krb_create_ticket(krb5_context context,
			unsigned char flags,
			const char *pname,
			const char *pinstance,
			const char *prealm,
			int32_t paddress,
			const krb5_keyblock *session,
			int16_t life,
			int32_t life_sec,
			const char *sname,
			const char *sinstance,
			const krb5_keyblock *key,
			krb5_data *enc_data)
{
    krb5_error_code ret;
    krb5_storage *sp;

    krb5_data_zero(enc_data);

    sp = krb5_storage_emem();
    if (sp == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }
    krb5_storage_set_byteorder(sp, KRB5_STORAGE_BYTEORDER_BE);

    RCHECK(ret, krb5_store_int8(sp, flags), error);
    RCHECK(ret, put_nir(sp, pname, pinstance, prealm), error);
    RCHECK(ret, krb5_store_int32(sp, ntohl(paddress)), error);

    /* session key */
    ret = krb5_storage_write(sp,
			     session->keyvalue.data, 
			     session->keyvalue.length);
    if (ret != session->keyvalue.length) {
	ret = KRB4ET_INTK_PROT;
	goto error;
    }

    RCHECK(ret, krb5_store_int8(sp, life), error);
    RCHECK(ret, krb5_store_int32(sp, life_sec), error);
    RCHECK(ret, put_nir(sp, sname, sinstance, NULL), error);

    ret = storage_to_etext(context, sp, key, enc_data);

 error:
    krb5_storage_free(sp);
    if (ret)
	krb5_set_error_string(context, "Failed to encode kerberos 4 ticket");

    return ret;
}

/*
 *
 */

krb5_error_code KRB5_LIB_FUNCTION
_krb5_krb_create_ciph(krb5_context context,
		      const krb5_keyblock *session,
		      const char *service,
		      const char *instance,
		      const char *realm,
		      uint32_t life,
		      unsigned char kvno,
		      const krb5_data *ticket,
		      uint32_t kdc_time,
		      const krb5_keyblock *key,
		      krb5_data *enc_data)
{
    krb5_error_code ret;
    krb5_storage *sp;

    krb5_data_zero(enc_data);

    sp = krb5_storage_emem();
    if (sp == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }
    krb5_storage_set_byteorder(sp, KRB5_STORAGE_BYTEORDER_BE);

    /* session key */
    ret = krb5_storage_write(sp,
			     session->keyvalue.data, 
			     session->keyvalue.length);
    if (ret != session->keyvalue.length) {
	ret = KRB4ET_INTK_PROT;
	goto error;
    }

    RCHECK(ret, put_nir(sp, service, instance, realm), error);
    RCHECK(ret, krb5_store_int8(sp, life), error);
    RCHECK(ret, krb5_store_int8(sp, kvno), error);
    RCHECK(ret, krb5_store_int8(sp, ticket->length), error);
    ret = krb5_storage_write(sp, ticket->data, ticket->length);
    if (ret != ticket->length) {
	ret = KRB4ET_INTK_PROT;
	goto error;
    }
    RCHECK(ret, krb5_store_int32(sp, kdc_time), error);

    ret = storage_to_etext(context, sp, key, enc_data);

 error:
    krb5_storage_free(sp);
    if (ret)
	krb5_set_error_string(context, "Failed to encode kerberos 4 ticket");

    return ret;
}

/*
 *
 */

krb5_error_code KRB5_LIB_FUNCTION
_krb5_krb_create_auth_reply(krb5_context context,
			    const char *pname,
			    const char *pinst,
			    const char *prealm,
			    int32_t time_ws,
			    int n,
			    uint32_t x_date,
			    unsigned char kvno,
			    const krb5_data *cipher,
			    krb5_data *data)
{
    krb5_error_code ret;
    krb5_storage *sp;

    krb5_data_zero(data);

    sp = krb5_storage_emem();
    if (sp == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }
    krb5_storage_set_byteorder(sp, KRB5_STORAGE_BYTEORDER_BE);

    RCHECK(ret, krb5_store_int8(sp, KRB_PROT_VERSION), error);
    RCHECK(ret, krb5_store_int8(sp, AUTH_MSG_KDC_REPLY), error);
    RCHECK(ret, put_nir(sp, pname, pinst, prealm), error);
    RCHECK(ret, krb5_store_int32(sp, time_ws), error);
    RCHECK(ret, krb5_store_int8(sp, n), error);
    RCHECK(ret, krb5_store_int32(sp, x_date), error);
    RCHECK(ret, krb5_store_int8(sp, kvno), error);
    RCHECK(ret, krb5_store_int16(sp, cipher->length), error);
    ret = krb5_storage_write(sp, cipher->data, cipher->length);
    if (ret != cipher->length) {
	ret = KRB4ET_INTK_PROT;
	goto error;
    }

    ret = krb5_storage_to_data(sp, data);

 error:
    krb5_storage_free(sp);
    if (ret)
	krb5_set_error_string(context, "Failed to encode kerberos 4 ticket");
	
    return ret;
}

/*
 *
 */

krb5_error_code KRB5_LIB_FUNCTION
_krb5_krb_cr_err_reply(krb5_context context,
		       const char *name,
		       const char *inst,
		       const char *realm,
		       uint32_t time_ws,
		       uint32_t e,
		       const char *e_string,
		       krb5_data *data)
{
    krb5_error_code ret;
    krb5_storage *sp;

    krb5_data_zero(data);

    if (name == NULL) name = "";
    if (inst == NULL) inst = "";
    if (realm == NULL) realm = "";
    if (e_string == NULL) e_string = "";

    sp = krb5_storage_emem();
    if (sp == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }
    krb5_storage_set_byteorder(sp, KRB5_STORAGE_BYTEORDER_BE);

    RCHECK(ret, krb5_store_int8(sp, KRB_PROT_VERSION), error);
    RCHECK(ret, krb5_store_int8(sp, AUTH_MSG_ERR_REPLY), error);
    RCHECK(ret, put_nir(sp, name, inst, realm), error);
    RCHECK(ret, krb5_store_int32(sp, time_ws), error);
    /* If it is a Kerberos 4 error-code, remove the et BASE */
    if (e >= ERROR_TABLE_BASE_krb && e <= ERROR_TABLE_BASE_krb + 255)
	e -= ERROR_TABLE_BASE_krb;
    RCHECK(ret, krb5_store_int32(sp, e), error);
    RCHECK(ret, krb5_store_stringz(sp, e_string), error);

    ret = krb5_storage_to_data(sp, data);

 error:
    krb5_storage_free(sp);
    if (ret)
	krb5_set_error_string(context, "Failed to encode kerberos 4 error");
	
    return 0;
}

static krb5_error_code
get_v4_stringz(krb5_storage *sp, char **str, size_t max_len)
{
    krb5_error_code ret;

    ret = krb5_ret_stringz(sp, str);
    if (ret)
	return ret;
    if (strlen(*str) > max_len) {
	free(*str);
	*str = NULL;
	return KRB4ET_INTK_PROT;
    }
    return 0;
}

/*
 *
 */

krb5_error_code KRB5_LIB_FUNCTION
_krb5_krb_decomp_ticket(krb5_context context,
			const krb5_data *enc_ticket,
			const krb5_keyblock *key,
			const char *local_realm,
			char **sname,
			char **sinstance,
			struct _krb5_krb_auth_data *ad)
{
    krb5_error_code ret;
    krb5_ssize_t size;
    krb5_storage *sp = NULL;
    krb5_data ticket;
    unsigned char des_key[8];

    memset(ad, 0, sizeof(*ad));
    krb5_data_zero(&ticket);

    *sname = NULL;
    *sinstance = NULL;

    RCHECK(ret, decrypt_etext(context, key, enc_ticket, &ticket), error);

    sp = krb5_storage_from_data(&ticket);
    if (sp == NULL) {
	krb5_data_free(&ticket);
	krb5_set_error_string(context, "alloc: out of memory");
	return ENOMEM;
    }

    krb5_storage_set_eof_code(sp, KRB4ET_INTK_PROT);

    RCHECK(ret, krb5_ret_int8(sp, &ad->k_flags), error);
    RCHECK(ret, get_v4_stringz(sp, &ad->pname, ANAME_SZ), error);
    RCHECK(ret, get_v4_stringz(sp, &ad->pinst, INST_SZ), error);
    RCHECK(ret, get_v4_stringz(sp, &ad->prealm, REALM_SZ), error);
    RCHECK(ret, krb5_ret_uint32(sp, &ad->address), error);
	
    size = krb5_storage_read(sp, des_key, sizeof(des_key));
    if (size != sizeof(des_key)) {
	ret = KRB4ET_INTK_PROT;
	goto error;
    }

    RCHECK(ret, krb5_ret_uint8(sp, &ad->life), error);

    if (ad->k_flags & 1)
	krb5_storage_set_byteorder(sp, KRB5_STORAGE_BYTEORDER_LE);
    else
	krb5_storage_set_byteorder(sp, KRB5_STORAGE_BYTEORDER_BE);

    RCHECK(ret, krb5_ret_uint32(sp, &ad->time_sec), error);

    RCHECK(ret, get_v4_stringz(sp, sname, ANAME_SZ), error);
    RCHECK(ret, get_v4_stringz(sp, sinstance, INST_SZ), error);

    ret = krb5_keyblock_init(context, ETYPE_DES_PCBC_NONE,
			     des_key, sizeof(des_key), &ad->session);
    if (ret)
	goto error;

    if (strlen(ad->prealm) == 0) {
	free(ad->prealm);
	ad->prealm = strdup(local_realm);
	if (ad->prealm == NULL) {
	    ret = ENOMEM;
	    goto error;
	}
    }

 error:
    memset(des_key, 0, sizeof(des_key));
    if (sp)
	krb5_storage_free(sp);
    krb5_data_free(&ticket);
    if (ret) {
	if (*sname) {
	    free(*sname);
	    *sname = NULL;
	}
	if (*sinstance) {
	    free(*sinstance);
	    *sinstance = NULL;
	}
	_krb5_krb_free_auth_data(context, ad);
	krb5_set_error_string(context, "Failed to decode v4 ticket");
    }
    return ret;
}

/*
 *
 */

krb5_error_code KRB5_LIB_FUNCTION
_krb5_krb_rd_req(krb5_context context,
		 krb5_data *authent,
		 const char *service,
		 const char *instance,
		 const char *local_realm,
		 int32_t from_addr,
		 const krb5_keyblock *key,
		 struct _krb5_krb_auth_data *ad)
{
    krb5_error_code ret;
    krb5_storage *sp;
    krb5_data ticket, eaut, aut;
    krb5_ssize_t size;
    int little_endian;
    int8_t pvno;
    int8_t type;
    int8_t s_kvno;
    uint8_t ticket_length;
    uint8_t eaut_length;
    uint8_t time_5ms;
    char *realm = NULL;
    char *sname = NULL;
    char *sinstance = NULL;
    char *r_realm = NULL;
    char *r_name = NULL;
    char *r_instance = NULL;

    uint32_t r_time_sec;	/* Coarse time from authenticator */
    unsigned long delta_t;      /* Time in authenticator - local time */
    long tkt_age;		/* Age of ticket */

    struct timeval tv;

    krb5_data_zero(&ticket);
    krb5_data_zero(&eaut);
    krb5_data_zero(&aut);

    sp = krb5_storage_from_data(authent);
    if (sp == NULL) {
	krb5_set_error_string(context, "alloc: out of memory");
	return ENOMEM;
    }

    krb5_storage_set_eof_code(sp, KRB4ET_INTK_PROT);

    ret = krb5_ret_int8(sp, &pvno);
    if (ret) {
	krb5_set_error_string(context, "Failed reading v4 pvno");
	goto error;
    }

    if (pvno != KRB_PROT_VERSION) {
	ret = KRB4ET_RD_AP_VERSION;
	krb5_set_error_string(context, "Failed v4 pvno not 4");
	goto error;
    }

    ret = krb5_ret_int8(sp, &type);
    if (ret) {
	krb5_set_error_string(context, "Failed readin v4 type");
	goto error;
    }

    little_endian = type & 1;
    type &= ~1;
    
    if(type != AUTH_MSG_APPL_REQUEST && type != AUTH_MSG_APPL_REQUEST_MUTUAL) {
	ret = KRB4ET_RD_AP_MSG_TYPE;
	krb5_set_error_string(context, "Not a valid v4 request type");
	goto error;
    }

    RCHECK(ret, krb5_ret_int8(sp, &s_kvno), error);
    RCHECK(ret, get_v4_stringz(sp, &realm, REALM_SZ), error);
    RCHECK(ret, krb5_ret_uint8(sp, &ticket_length), error);
    RCHECK(ret, krb5_ret_uint8(sp, &eaut_length), error);
    RCHECK(ret, krb5_data_alloc(&ticket, ticket_length), error);

    size = krb5_storage_read(sp, ticket.data, ticket.length);
    if (size != ticket.length) {
	ret = KRB4ET_INTK_PROT;
	krb5_set_error_string(context, "Failed reading v4 ticket");
	goto error;
    }

    /* Decrypt and take apart ticket */
    ret = _krb5_krb_decomp_ticket(context, &ticket, key, local_realm, 
				  &sname, &sinstance, ad);
    if (ret)
	goto error;

    RCHECK(ret, krb5_data_alloc(&eaut, eaut_length), error);

    size = krb5_storage_read(sp, eaut.data, eaut.length);
    if (size != eaut.length) {
	ret = KRB4ET_INTK_PROT;
	krb5_set_error_string(context, "Failed reading v4 authenticator");
	goto error;
    }

    krb5_storage_free(sp);
    sp = NULL;

    ret = decrypt_etext(context, &ad->session, &eaut, &aut);
    if (ret)
	goto error;

    sp = krb5_storage_from_data(&aut);
    if (sp == NULL) {
	ret = ENOMEM;
	krb5_set_error_string(context, "alloc: out of memory");
	goto error;
    }

    if (little_endian)
	krb5_storage_set_byteorder(sp, KRB5_STORAGE_BYTEORDER_LE);
    else
	krb5_storage_set_byteorder(sp, KRB5_STORAGE_BYTEORDER_BE);

    RCHECK(ret, get_v4_stringz(sp, &r_name, ANAME_SZ), error);
    RCHECK(ret, get_v4_stringz(sp, &r_instance, INST_SZ), error);
    RCHECK(ret, get_v4_stringz(sp, &r_realm, REALM_SZ), error);

    RCHECK(ret, krb5_ret_uint32(sp, &ad->checksum), error);
    RCHECK(ret, krb5_ret_uint8(sp, &time_5ms), error);
    RCHECK(ret, krb5_ret_uint32(sp, &r_time_sec), error);

    if (strcmp(ad->pname, r_name) != 0 ||
	strcmp(ad->pinst, r_instance) != 0 ||
	strcmp(ad->prealm, r_realm) != 0) {
	krb5_set_error_string(context, "v4 principal mismatch");
	ret = KRB4ET_RD_AP_INCON;
	goto error;
    }
    
    if (from_addr && ad->address && from_addr != ad->address) {
	krb5_set_error_string(context, "v4 bad address in ticket");
	ret = KRB4ET_RD_AP_BADD;
	goto error;
    }

    gettimeofday(&tv, NULL);
    delta_t = abs((int)(tv.tv_sec - r_time_sec));
    if (delta_t > CLOCK_SKEW) {
        ret = KRB4ET_RD_AP_TIME;
	krb5_set_error_string(context, "v4 clock skew");
	goto error;
    }

    /* Now check for expiration of ticket */

    tkt_age = tv.tv_sec - ad->time_sec;
    
    if ((tkt_age < 0) && (-tkt_age > CLOCK_SKEW)) {
        ret = KRB4ET_RD_AP_NYV;
	krb5_set_error_string(context, "v4 clock skew for expiration");
	goto error;
    }

    if (tv.tv_sec > _krb5_krb_life_to_time(ad->time_sec, ad->life)) {
	ret = KRB4ET_RD_AP_EXP;
	krb5_set_error_string(context, "v4 ticket expired");
	goto error;
    }

    ret = 0;
 error:
    krb5_data_free(&ticket);
    krb5_data_free(&eaut);
    krb5_data_free(&aut);
    if (realm)
	free(realm);
    if (sname)
	free(sname);
    if (sinstance)
	free(sinstance);
    if (r_name)
	free(r_name);
    if (r_instance)
	free(r_instance);
    if (r_realm)
	free(r_realm);
    if (sp)
	krb5_storage_free(sp);

    if (ret)
	krb5_clear_error_string(context);

    return ret;
}

/*
 *
 */

void KRB5_LIB_FUNCTION
_krb5_krb_free_auth_data(krb5_context context, struct _krb5_krb_auth_data *ad)
{
    if (ad->pname)
	free(ad->pname);
    if (ad->pinst)
	free(ad->pinst);
    if (ad->prealm)
	free(ad->prealm);
    krb5_free_keyblock_contents(context, &ad->session);
    memset(ad, 0, sizeof(*ad));
}
