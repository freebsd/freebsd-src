/*
 * Implements Kerberos 4 authentication
 */

#ifdef KRB4
#include <windows.h>
#include <time.h>
#include <string.h>
#include "winsock.h"
#include "kerberos.h"
#endif
#ifdef KRB5
#include <time.h>
#include <string.h>
#include "krb5.h"
#include "com_err.h"
#endif

#include "telnet.h"
#include "telnet_arpa.h"

#ifdef ENCRYPTION
#include "encrypt.h"
#endif

/*
 * Constants
 */
#ifdef KRB4
#define KRB_AUTH		0
#define KRB_REJECT		1
#define KRB_ACCEPT		2
#define KRB_CHALLENGE		3
#define KRB_RESPONSE		4
#endif
#ifdef KRB5
#define	KRB_AUTH		0	/* Authentication data follows */
#define	KRB_REJECT		1	/* Rejected (reason might follow) */
#define	KRB_ACCEPT		2	/* Accepted */
#define	KRB_RESPONSE		3	/* Response for mutual auth. */

#define KRB_FORWARD             4       /* Forwarded credentials follow */
#define KRB_FORWARD_ACCEPT      5       /* Forwarded credentials accepted */
#define KRB_FORWARD_REJECT      6       /* Forwarded credentials rejected */
#endif

#ifndef KSUCCESS                            /* Let K5 use K4 constants */
#define KSUCCESS    0
#define KFAILURE    255
#endif

/*
 * Globals
 */
#ifdef KRB4
static CREDENTIALS cred;
static KTEXT_ST auth;

#define KRB_SERVICE_NAME    "rcmd"
#define KERBEROS_VERSION    KERBEROS_V4

static int auth_how;
static int k4_auth_send(kstream);
static int k4_auth_reply(kstream, unsigned char *, int);
#endif

#ifdef KRB5
static krb5_data          auth;
static int                auth_how;
static krb5_auth_context  auth_context;
krb5_keyblock	         *session_key = NULL;
#ifdef FORWARD
void kerberos5_forward(kstream);
#endif

#define KRB_SERVICE_NAME    "host"
#define KERBEROS_VERSION    AUTHTYPE_KERBEROS_V5

static int k5_auth_send(kstream, int);
static int k5_auth_reply(kstream, int, unsigned char *, int);
#endif

static int Data(kstream, int, void *, int);

#ifdef ENCRYPTION
BOOL encrypt_flag = 1;
#endif
#ifdef FORWARD
BOOL forward_flag = 1;       /* forward tickets? */
BOOL forwardable_flag = 1;   /* get forwardable tickets to forward? */
BOOL forwarded_tickets = 0;  /* were tickets forwarded? */
#endif

static unsigned char str_data[1024] = { IAC, SB, TELOPT_AUTHENTICATION, 0,
			  		AUTHTYPE_KERBEROS_V5, };

static int
Data(kstream ks, int type, void *d, int c)
{
  unsigned char *p = str_data + 4;
  unsigned char *cd = (unsigned char *)d;

  if (c == -1)
    c = strlen((char *)cd);

  *p++ = AUTHTYPE_KERBEROS_V5;
  *p = AUTH_WHO_CLIENT|AUTH_HOW_MUTUAL;
#ifdef ENCRYPTION
  *p |= AUTH_ENCRYPT_ON;
#endif
  p++;
  *p++ = type;
  while (c-- > 0) {
    if ((*p++ = *cd++) == IAC)
      *p++ = IAC;
        }
  *p++ = IAC;
  *p++ = SE;

  return(TelnetSend(ks, (LPSTR)str_data, p - str_data, 0));
}

#ifdef ENCRYPTION
/*
 * Function: Enable or disable the encryption process.
 *
 * Parameters:
 *	enable - TRUE to enable, FALSE to disable.
 */
static void
auth_encrypt_enable(BOOL enable)
{
  encrypt_flag = enable;
}
#endif

/*
 * Function: Abort the authentication process
 *
 * Parameters:
 *	ks - kstream to send abort message to.
 */
static void
auth_abort(kstream ks, char *errmsg, long r)
{
  char buf[9];

  wsprintf(buf, "%c%c%c%c%c%c%c%c", IAC, SB, TELOPT_AUTHENTICATION,
	   TELQUAL_IS, AUTHTYPE_NULL,
	   AUTHTYPE_NULL, IAC, SE);
  TelnetSend(ks, (LPSTR)buf, 8, 0);

  if (errmsg != NULL) {
    strTmp[sizeof(strTmp) - 1] = '\0';
    strncpy(strTmp, errmsg, sizeof(strTmp) - 1);

    if (r != KSUCCESS) {
      strncat(strTmp, "\n", sizeof(strTmp) - 1 - strlen(strTmp));
#ifdef KRB4
      lstrcat(strTmp, krb_get_err_text((int)r));
#endif
#ifdef KRB5
      lstrcat(strTmp, error_message(r));
#endif
    }

    MessageBox(HWND_DESKTOP, strTmp, "Kerberos authentication failed!",
	       MB_OK | MB_ICONEXCLAMATION);
  }
}


/*
 * Function: Copy data to buffer, doubling IAC character if present.
 *
 * Parameters:
 *	kstream - kstream to send abort message to.
 */
static int
copy_for_net(unsigned char *to, unsigned char *from, int c)
{
  int n;

  n = c;

  while (c-- > 0) {
    if ((*to++ = *from++) == IAC) {
      n++;
      *to++ = IAC;
    }
  }

  return n;
}


/*
 * Function: Parse authentication send command
 *
 * Parameters:
 *	ks - kstream to send abort message to.
 *
 *  parsedat - the sub-command data.
 *
 *	end_sub - index of the character in the 'parsedat' array which
 *		is the last byte in a sub-negotiation
 *
 * Returns: Kerberos error code.
 */
static int
auth_send(kstream ks, unsigned char *parsedat, int end_sub)
{
  char buf[2048];	/* be sure that this is > auth.length+9 */
  char *pname;
  int plen;
  int r;
  int i;

  auth_how = -1;

  for (i = 2; i+1 <= end_sub; i += 2) {
    if (parsedat[i] == KERBEROS_VERSION)
      if ((parsedat[i+1] & AUTH_WHO_MASK) == AUTH_WHO_CLIENT) {
	auth_how = parsedat[i+1] & AUTH_HOW_MASK;
	break;
      }
  }

  if (auth_how == -1) {
    auth_abort(ks, NULL, 0);
    return KFAILURE;
  }

#ifdef KRB4
  r = k4_auth_send(ks);
#endif /* KRB4 */

#ifdef KRB5
  r = k5_auth_send(ks, auth_how);
#endif /* KRB5 */

  if (!r)
    return KFAILURE;

  plen = strlen(szUserName);                 /* Set by k#_send if needed */
  pname = szUserName;

  wsprintf(buf, "%c%c%c%c", IAC, SB, TELOPT_AUTHENTICATION, TELQUAL_NAME);
  memcpy(&buf[4], pname, plen);
  wsprintf(&buf[plen + 4], "%c%c", IAC, SE);
  TelnetSend(ks, (LPSTR)buf, lstrlen(pname)+6, 0);

  wsprintf(buf, "%c%c%c%c%c%c%c", IAC, SB, TELOPT_AUTHENTICATION, TELQUAL_IS,
	   KERBEROS_VERSION, auth_how | AUTH_WHO_CLIENT, KRB_AUTH);

#if KRB4
  auth.length = copy_for_net(&buf[7], auth.dat, auth.length);
#endif /* KRB4 */
#if KRB5
  auth.length = copy_for_net(&buf[7], auth.data, auth.length);
#endif /* KRB5 */

  wsprintf(&buf[auth.length+7], "%c%c", IAC, SE);

  TelnetSend(ks, (LPSTR)buf, auth.length+9, 0);

  return KSUCCESS;
}

/*
 * Function: Parse authentication reply command
 *
 * Parameters:
 *	ks - kstream to send abort message to.
 *
 *  parsedat - the sub-command data.
 *
 *	end_sub - index of the character in the 'parsedat' array which
 *		is the last byte in a sub-negotiation
 *
 * Returns: Kerberos error code.
 */
static int
auth_reply(kstream ks, unsigned char *parsedat, int end_sub)
{
  int n;

#ifdef KRB4
  n = k4_auth_reply(ks, parsedat, end_sub);
#endif

#ifdef KRB5
  n = k5_auth_reply(ks, auth_how, parsedat, end_sub);
#endif

  return n;
}

/*
 * Function: Parse the athorization sub-options and reply.
 *
 * Parameters:
 *	ks - kstream to send abort message to.
 *
 *	parsedat - sub-option string to parse.
 *
 *	end_sub - last charcter position in parsedat.
 */
void
auth_parse(kstream ks, unsigned char *parsedat, int end_sub)
{
  if (parsedat[1] == TELQUAL_SEND)
    auth_send(ks, parsedat, end_sub);

  if (parsedat[1] == TELQUAL_REPLY)
    auth_reply(ks, parsedat, end_sub);
}


/*
 * Function: Initialization routine called kstream encryption system.
 *
 * Parameters:
 *	str - kstream to send abort message to.
 *
 *  data - user data.
 */
int
auth_init(kstream str, kstream_ptr data)
{
#ifdef ENCRYPTION
  encrypt_init(str, data);
#endif
  return 0;
}


/*
 * Function: Destroy routine called kstream encryption system.
 *
 * Parameters:
 *	str - kstream to send abort message to.
 *
 *  data - user data.
 */
void
auth_destroy(kstream str)
{
}


/*
 * Function: Callback to encrypt a block of characters
 *
 * Parameters:
 *	out - return as pointer to converted buffer.
 *
 *  in - the buffer to convert
 *
 *  str - the stream being encrypted
 *
 * Returns: number of characters converted.
 */
int
auth_encrypt(struct kstream_data_block *out,
	     struct kstream_data_block *in,
	     kstream str)
{
  out->ptr = in->ptr;

  out->length = in->length;

  return(out->length);
}


/*
 * Function: Callback to decrypt a block of characters
 *
 * Parameters:
 *	out - return as pointer to converted buffer.
 *
 *  in - the buffer to convert
 *
 *  str - the stream being encrypted
 *
 * Returns: number of characters converted.
 */
int
auth_decrypt(struct kstream_data_block *out,
	     struct kstream_data_block *in,
	     kstream str)
{
  out->ptr = in->ptr;

  out->length = in->length;

  return(out->length);
}

#ifdef KRB4
/*
 *
 * K4_auth_send - gets authentication bits we need to send to KDC.
 *
 * Result is left in auth
 *
 * Returns: 0 on failure, 1 on success
 */
static int
k4_auth_send(kstream ks)
{
  int r;                                      /* Return value */
  char instance[INST_SZ];
  char *realm;
  char buf[256];

  memset(instance, 0, sizeof(instance));

  if (realm = krb_get_phost(szHostName))
    lstrcpy(instance, realm);

  realm = krb_realmofhost(szHostName);

  if (!realm) {
    strcpy(buf, "Can't find realm for host \"");
    strncat(buf, szHostName, sizeof(buf) - 1 - strlen(buf));
    strncat(buf, "\"", sizeof(buf) - 1 - strlen(buf));
    auth_abort(ks, buf, 0);
    return KFAILURE;
  }

  r = krb_mk_req(&auth, KRB_SERVICE_NAME, instance, realm, 0);

  if (r == 0)
    r = krb_get_cred(KRB_SERVICE_NAME, instance, realm, &cred);

  if (r) {
    strcpy(buf, "Can't get \"");
    strncat(buf, KRB_SERVICE_NAME, sizeof(buf) - 1 - strlen(buf));
    if (instance[0] != 0) {
      strncat(buf, ".", sizeof(buf) - 1 - strlen(buf));
      lstrcat(buf, instance);
    }
    strncat(buf, "@", sizeof(buf) - 1 - strlen(buf));
    lstrcat(buf, realm);
    strncat(buf, "\" ticket", sizeof(buf) - 1 - strlen(buf));
    auth_abort(ks, buf, r);

    return r;
  }

  if (!szUserName[0])			/* Copy if not there */
    strcpy(szUserName, cred.pname);

  return(1);
}

/*
 * Function: K4 parse authentication reply command
 *
 * Parameters:
 *	ks - kstream to send abort message to.
 *
 *  parsedat - the sub-command data.
 *
 *	end_sub - index of the character in the 'parsedat' array which
 *		is the last byte in a sub-negotiation
 *
 * Returns: Kerberos error code.
 */
static int
k4_auth_reply(kstream ks, unsigned char *parsedat, int end_sub)
{
  time_t t;
  int x;
  char buf[512];
  int i;
  des_cblock session_key;
  des_key_schedule sched;
  static des_cblock challenge;

  if (end_sub < 4)
    return KFAILURE;

  if (parsedat[2] != KERBEROS_V4)
    return KFAILURE;

  if (parsedat[4] == KRB_REJECT) {
    buf[0] = 0;

    for (i = 5; i <= end_sub; i++) {
      if (parsedat[i] == IAC)
	break;
      buf[i-5] = parsedat[i];
      buf[i-4] = 0;
    }

    if (!buf[0])
      strcpy(buf, "Authentication rejected by remote machine!");
    MessageBox(HWND_DESKTOP, buf, NULL, MB_OK | MB_ICONEXCLAMATION);

    return KFAILURE;
  }

  if (parsedat[4] == KRB_ACCEPT) {
    if ((parsedat[3] & AUTH_HOW_MASK) == AUTH_HOW_ONE_WAY)
      return KSUCCESS;

    if ((parsedat[3] & AUTH_HOW_MASK) != AUTH_HOW_MUTUAL)
      return KFAILURE;

    des_key_sched(cred.session, sched);

    t = time(NULL);
    memcpy(challenge, &t, 4);
    memcpy(&challenge[4], &t, 4);
    des_ecb_encrypt(&challenge, &session_key, sched, 1);

    /*
     * Increment the challenge by 1, and encrypt it for
     * later comparison.
     */
    for (i = 7; i >= 0; --i) {
      x = (unsigned int)challenge[i] + 1;
      challenge[i] = x;	/* ignore overflow */
      if (x < 256)		/* if no overflow, all done */
	break;
    }

    des_ecb_encrypt(&challenge, &challenge, sched, 1);

    wsprintf(buf, "%c%c%c%c%c%c%c", IAC, SB, TELOPT_AUTHENTICATION, TELQUAL_IS,
	     KERBEROS_V4, AUTH_WHO_CLIENT|AUTH_HOW_MUTUAL, KRB_CHALLENGE);
    memcpy(&buf[7], session_key, 8);
    wsprintf(&buf[15], "%c%c", IAC, SE);
    TelnetSend(ks, (LPSTR)buf, 17, 0);

    return KSUCCESS;
  }

  if (parsedat[4] == KRB_RESPONSE) {
    if (end_sub < 12)
      return KFAILURE;

    if (memcmp(&parsedat[5], challenge, sizeof(challenge)) != 0) {
      MessageBox(HWND_DESKTOP, "Remote machine is being impersonated!",
		 NULL, MB_OK | MB_ICONEXCLAMATION);

      return KFAILURE;
    }

    return KSUCCESS;
  }

  return KFAILURE;

}

#endif /* KRB4 */

#ifdef KRB5

/*
 *
 * K5_auth_send - gets authentication bits we need to send to KDC.
 *
 * Code lifted from telnet sample code in the appl directory.
 *
 * Result is left in auth
 *
 * Returns: 0 on failure, 1 on success
 *
 */

static int
k5_auth_send(kstream ks, int how)
{
  krb5_error_code r;
  krb5_ccache ccache;
  krb5_creds creds;
  krb5_creds * new_creds;
  extern krb5_flags krb5_kdc_default_options;
  krb5_flags ap_opts;
  char type_check[2];
  krb5_data check_data;
  int len;
#ifdef ENCRYPTION
  krb5_keyblock *newkey = 0;
#endif

  if (r = krb5_cc_default(k5_context, &ccache)) {
    com_err(NULL, r, "while authorizing.");
    return(0);
  }

  memset((char *)&creds, 0, sizeof(creds));
  if (r = krb5_sname_to_principal(k5_context, szHostName, KRB_SERVICE_NAME,
				  KRB5_NT_SRV_HST, &creds.server)) {
    com_err(NULL, r, "while authorizing.");
    return(0);
  }

  if (r = krb5_cc_get_principal(k5_context, ccache, &creds.client)) {
    com_err(NULL, r, "while authorizing.");
    krb5_free_cred_contents(k5_context, &creds);
    return(0);
  }
  if (szUserName[0] == '\0') {                /* Get user name now */
    len  = krb5_princ_component(k5_context, creds.client, 0)->length;
    memcpy(szUserName,
	   krb5_princ_component(k5_context, creds.client, 0)->data,
	   len);
    szUserName[len] = '\0';
  }

  if (r = krb5_get_credentials(k5_context, 0,
			       ccache, &creds, &new_creds)) {
    com_err(NULL, r, "while authorizing.");
    krb5_free_cred_contents(k5_context, &creds);
    return(0);
  }

  ap_opts = 0;
  if ((how & AUTH_HOW_MASK) == AUTH_HOW_MUTUAL)
    ap_opts = AP_OPTS_MUTUAL_REQUIRED;

#ifdef ENCRYPTION
  ap_opts |= AP_OPTS_USE_SUBKEY;
#endif

  if (auth_context) {
    krb5_auth_con_free(k5_context, auth_context);
    auth_context = 0;
  }
  if ((r = krb5_auth_con_init(k5_context, &auth_context))) {
    com_err(NULL, r, "while initializing auth context");
    return(0);
  }

  krb5_auth_con_setflags(k5_context, auth_context,
			 KRB5_AUTH_CONTEXT_RET_TIME);

  type_check[0] = AUTHTYPE_KERBEROS_V5;
  type_check[1] = AUTH_WHO_CLIENT| (how & AUTH_HOW_MASK);
#ifdef ENCRYPTION
  type_check[1] |= AUTH_ENCRYPT_ON;
#endif
  check_data.magic = KV5M_DATA;
  check_data.length = 2;
  check_data.data = (char *)&type_check;

  r = krb5_mk_req_extended(k5_context, &auth_context, ap_opts,
			   NULL, new_creds, &auth);

#ifdef ENCRYPTION
  krb5_auth_con_getlocalsubkey(k5_context, auth_context, &newkey);
  if (session_key) {
    krb5_free_keyblock(k5_context, session_key);
    session_key = 0;
  }

  if (newkey) {
    /*
     * keep the key in our private storage, but don't use it
     * yet---see kerberos5_reply() below
     */
    if ((newkey->enctype != ENCTYPE_DES_CBC_CRC) &&
	(newkey-> enctype != ENCTYPE_DES_CBC_MD5)) {
      if ((new_creds->keyblock.enctype == ENCTYPE_DES_CBC_CRC) ||
	  (new_creds->keyblock.enctype == ENCTYPE_DES_CBC_MD5))
	/* use the session key in credentials instead */
	krb5_copy_keyblock(k5_context, &new_creds->keyblock, &session_key);
      else
	; 	/* What goes here? XXX */
    } else {
      krb5_copy_keyblock(k5_context, newkey, &session_key);
    }
    krb5_free_keyblock(k5_context, newkey);
  }
#endif  /* ENCRYPTION */

  krb5_free_cred_contents(k5_context, &creds);
  krb5_free_creds(k5_context, new_creds);

  if (r) {
    com_err(NULL, r, "while authorizing.");
    return(0);
  }

  return(1);
}

/*
 *
 * K5_auth_reply -- checks the reply for mutual authentication.
 *
 * Code lifted from telnet sample code in the appl directory.
 *
 */
static int
k5_auth_reply(kstream ks, int how, unsigned char *data, int cnt)
{
#ifdef ENCRYPTION
  Session_Key skey;
#endif
  static int mutual_complete = 0;

  data += 4;                                  /* Point to status byte */

  switch (*data++) {
  case KRB_REJECT:
    if (cnt > 0) {
      char *s;
      wsprintf(strTmp,	"Kerberos V5 refuses authentication because\n\t");
      s = strTmp + strlen(strTmp);
      strncpy(s, data, cnt);
      s[cnt] = 0;
    } else
      wsprintf(strTmp, "Kerberos V5 refuses authentication");
    MessageBox(HWND_DESKTOP, strTmp, "", MB_OK | MB_ICONEXCLAMATION);

    return KFAILURE;

  case KRB_ACCEPT:
    if (!mutual_complete) {
      if ((how & AUTH_HOW_MASK) == AUTH_HOW_MUTUAL && !mutual_complete) {
	wsprintf(strTmp,
		 "Kerberos V5 accepted you, but didn't provide"
		 " mutual authentication");
	MessageBox(HWND_DESKTOP, strTmp, "", MB_OK | MB_ICONEXCLAMATION);
	return KFAILURE;
      }
#ifdef ENCRYPTION
      if (session_key) {
	skey.type = SK_DES;
	skey.length = 8;
	skey.data = session_key->contents;
	encrypt_session_key(&skey, 0);
      }
#endif
    }

#ifdef FORWARD
    if (forward_flag)
      kerberos5_forward(ks);
#endif

    return KSUCCESS;
    break;

  case KRB_RESPONSE:
    if ((how & AUTH_HOW_MASK) == AUTH_HOW_MUTUAL) {
      /* the rest of the reply should contain a krb_ap_rep */
      krb5_ap_rep_enc_part *reply;
      krb5_data inbuf;
      krb5_error_code r;

      inbuf.length = cnt;
      inbuf.data = (char *)data;

      if (r = krb5_rd_rep(k5_context, auth_context, &inbuf, &reply)) {
	com_err(NULL, r, "while authorizing.");
	return KFAILURE;
      }
      krb5_free_ap_rep_enc_part(k5_context, reply);

#ifdef ENCRYPTION
      if (encrypt_flag && session_key) {
	skey.type = SK_DES;
	skey.length = 8;
	skey.data = session_key->contents;
	encrypt_session_key(&skey, 0);
      }
#endif
      mutual_complete = 1;
    }
    return KSUCCESS;

#ifdef FORWARD
  case KRB_FORWARD_ACCEPT:
    forwarded_tickets = 1;
    return KSUCCESS;

  case KRB_FORWARD_REJECT:
    forwarded_tickets = 0;
    if (cnt > 0) {
      char *s;

      wsprintf(strTmp,
	       "Kerberos V5 refuses forwarded credentials because\n\t");
      s = strTmp + strlen(strTmp);
      strncpy(s, data, cnt);
      s[cnt] = 0;
    } else
      wsprintf(strTmp, "Kerberos V5 refuses forwarded credentials");

    MessageBox(HWND_DESKTOP, strTmp, "", MB_OK | MB_ICONEXCLAMATION);
    return KFAILURE;
#endif	/* FORWARD */

  default:
    return KFAILURE;                        /* Unknown reply type */
  }
}

#ifdef FORWARD
void
kerberos5_forward(kstream ks)
{
    krb5_error_code r;
    krb5_ccache ccache;
    krb5_principal client = 0;
    krb5_principal server = 0;
    krb5_data forw_creds;

    forw_creds.data = 0;

    if ((r = krb5_cc_default(k5_context, &ccache))) {
      com_err(NULL, r, "Kerberos V5: could not get default ccache");
      return;
    }

    if ((r = krb5_cc_get_principal(k5_context, ccache, &client))) {
      com_err(NULL, r, "Kerberos V5: could not get default principal");
      goto cleanup;
    }

    if ((r = krb5_sname_to_principal(k5_context, szHostName, KRB_SERVICE_NAME,
				     KRB5_NT_SRV_HST, &server))) {
      com_err(NULL, r, "Kerberos V5: could not make server principal");
      goto cleanup;
    }

    if ((r = krb5_auth_con_genaddrs(k5_context, auth_context, ks->fd,
			    KRB5_AUTH_CONTEXT_GENERATE_LOCAL_FULL_ADDR))) {
      com_err(NULL, r, "Kerberos V5: could not gen local full address");
      goto cleanup;
    }

    if (r = krb5_fwd_tgt_creds(k5_context, auth_context, 0, client, server,
			        ccache, forwardable_flag, &forw_creds)) {
      com_err(NULL, r, "Kerberos V5: error getting forwarded creds");
      goto cleanup;
    }

    /* Send forwarded credentials */
    if (!Data(ks, KRB_FORWARD, forw_creds.data, forw_creds.length)) {
      MessageBox(HWND_DESKTOP,
		 "Not enough room for authentication data", "",
		 MB_OK | MB_ICONEXCLAMATION);
    }

cleanup:
    if (client)
      krb5_free_principal(k5_context, client);
    if (server)
      krb5_free_principal(k5_context, server);
#if 0 /* XXX */
	if (forw_creds.data)
      free(forw_creds.data);
#endif
    krb5_cc_close(k5_context, ccache);
}
#endif /* FORWARD */

#endif /* KRB5 */
