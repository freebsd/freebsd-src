/*
 * Copyright (c) 1995 - 2000 Kungliga Tekniska Högskolan
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

#ifdef HAVE_CONFIG_H
#include<config.h>
RCSID("$Id: pam.c,v 1.22.2.2 2000/10/13 15:41:09 assar Exp $");
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/types.h>
#include <syslog.h>

#include <security/pam_appl.h>
#include <security/pam_modules.h>
#ifndef PAM_AUTHTOK_RECOVERY_ERR /* Fix linsux typo. */
#define PAM_AUTHTOK_RECOVERY_ERR PAM_AUTHTOK_RECOVER_ERR
#endif

#include <netinet/in.h>
#include <krb.h>
#include <kafs.h>

#if 0
/* Debugging PAM modules is a royal pain, truss helps. */
#define DEBUG(msg) (access(msg " at line", __LINE__))
#endif

static void
psyslog(int level, const char *format, ...)
{
  va_list args;
  va_start(args, format);
  openlog("pam_krb4", LOG_CONS|LOG_PID, LOG_AUTH);
  vsyslog(level, format, args);
  va_end(args);
  closelog();
}

enum {
  KRB4_DEBUG,
  KRB4_USE_FIRST_PASS,
  KRB4_TRY_FIRST_PASS,
  KRB4_IGNORE_ROOT,
  KRB4_NO_VERIFY,
  KRB4_REAFSLOG,
  KRB4_CTRLS			/* Number of ctrl arguments defined. */
};

#define KRB4_DEFAULTS  0

static int ctrl_flags = KRB4_DEFAULTS;
#define ctrl_on(x)  (krb4_args[x].flag & ctrl_flags)
#define ctrl_off(x) (!ctrl_on(x))

typedef struct
{
  const char *token;
  unsigned int flag;
} krb4_ctrls_t;

static krb4_ctrls_t krb4_args[KRB4_CTRLS] =
{
  /* KRB4_DEBUG          */ { "debug",          0x01 },
  /* KRB4_USE_FIRST_PASS */ { "use_first_pass", 0x02 },
  /* KRB4_TRY_FIRST_PASS */ { "try_first_pass", 0x04 },
  /* KRB4_IGNORE_ROOT    */ { "ignore_root",    0x08 },
  /* KRB4_NO_VERIFY      */ { "no_verify",      0x10 },
  /* KRB4_REAFSLOG       */ { "reafslog",       0x20 },
};

static void
parse_ctrl(int argc, const char **argv)
{
  int i, j;

  ctrl_flags = KRB4_DEFAULTS;
  for (i = 0; i < argc; i++)
    {
      for (j = 0; j < KRB4_CTRLS; j++)
	if (strcmp(argv[i], krb4_args[j].token) == 0)
	  break;
    
      if (j >= KRB4_CTRLS)
	psyslog(LOG_ALERT, "unrecognized option [%s]", *argv);
      else
	ctrl_flags |= krb4_args[j].flag;
    }
}

static void
pdeb(const char *format, ...)
{
  va_list args;
  if (ctrl_off(KRB4_DEBUG))
    return;
  va_start(args, format);
  openlog("pam_krb4", LOG_CONS|LOG_PID, LOG_AUTH);
  vsyslog(LOG_DEBUG, format, args);
  va_end(args);
  closelog();
}

#define ENTRY(func) pdeb("%s() flags = %d ruid = %d euid = %d", func, flags, getuid(), geteuid())

static void
set_tkt_string(uid_t uid)
{
  char buf[128];

  snprintf(buf, sizeof(buf), "%s%u", TKT_ROOT, (unsigned)uid);
  krb_set_tkt_string(buf);

#if 0
  /* pam_set_data+pam_get_data are not guaranteed to work, grr. */
  pam_set_data(pamh, "KRBTKFILE", strdup(t), cleanup);
  if (pam_get_data(pamh, "KRBTKFILE", (const void**)&tkt) == PAM_SUCCESS)
    {
      pam_putenv(pamh, var);
    }
#endif

  /* We don't want to inherit this variable.
   * If we still do, it must have a sane value. */
  if (getenv("KRBTKFILE") != 0)
    {
      char *var = malloc(sizeof(buf));
      snprintf(var, sizeof(buf), "KRBTKFILE=%s", tkt_string());
      putenv(var);
      /* free(var); XXX */
    }
}

static int
verify_pass(pam_handle_t *pamh,
	    const char *name,
	    const char *inst,
	    const char *pass)
{
  char realm[REALM_SZ];
  int ret, krb_verify, old_euid, old_ruid;

  krb_get_lrealm(realm, 1);
  if (ctrl_on(KRB4_NO_VERIFY))
    krb_verify = KRB_VERIFY_SECURE_FAIL;
  else
    krb_verify = KRB_VERIFY_SECURE;
  old_ruid = getuid();
  old_euid = geteuid();
  setreuid(0, 0);
  ret = krb_verify_user(name, inst, realm, pass, krb_verify, NULL);
  pdeb("krb_verify_user(`%s', `%s', `%s', pw, %d, NULL) returns %s",
       name, inst, realm, krb_verify,
       krb_get_err_text(ret));
  setreuid(old_ruid, old_euid);
  if (getuid() != old_ruid || geteuid() != old_euid)
    {
      psyslog(LOG_ALERT , "setreuid(%d, %d) failed at line %d",
	      old_ruid, old_euid, __LINE__);
      exit(1);
    }
    
  switch(ret) {
  case KSUCCESS:
    return PAM_SUCCESS;
  case KDC_PR_UNKNOWN:
    return PAM_USER_UNKNOWN;
  case SKDC_CANT:
  case SKDC_RETRY:
  case RD_AP_TIME:
    return PAM_AUTHINFO_UNAVAIL;
  default:
    return PAM_AUTH_ERR;
  }
}

static int
krb4_auth(pam_handle_t *pamh,
	  int flags,
	  const char *name,
	  const char *inst,
	  struct pam_conv *conv)
{
  struct pam_response *resp;
  char prompt[128];
  struct pam_message msg, *pmsg = &msg;
  int ret;

  if (ctrl_on(KRB4_TRY_FIRST_PASS) || ctrl_on(KRB4_USE_FIRST_PASS))
    {
      char *pass = 0;
      ret = pam_get_item(pamh, PAM_AUTHTOK, (void **) &pass);
      if (ret != PAM_SUCCESS)
        {
          psyslog(LOG_ERR , "pam_get_item returned error to get-password");
          return ret;
        }
      else if (pass != 0 && verify_pass(pamh, name, inst, pass) == PAM_SUCCESS)
	return PAM_SUCCESS;
      else if (ctrl_on(KRB4_USE_FIRST_PASS))
	return PAM_AUTHTOK_RECOVERY_ERR;       /* Wrong password! */
      else
	/* We tried the first password but it didn't work, cont. */;
    }

  msg.msg_style = PAM_PROMPT_ECHO_OFF;
  if (*inst == 0)
    snprintf(prompt, sizeof(prompt), "%s's Password: ", name);
  else
    snprintf(prompt, sizeof(prompt), "%s.%s's Password: ", name, inst);
  msg.msg = prompt;

  ret = conv->conv(1, &pmsg, &resp, conv->appdata_ptr);
  if (ret != PAM_SUCCESS)
    return ret;

  ret = verify_pass(pamh, name, inst, resp->resp);
  if (ret == PAM_SUCCESS)
    {
      memset(resp->resp, 0, strlen(resp->resp)); /* Erase password! */
      free(resp->resp);
      free(resp);
    }
  else
    {
      pam_set_item(pamh, PAM_AUTHTOK, resp->resp); /* Save password. */
      /* free(resp->resp); XXX */
      /* free(resp); XXX */
    }
  
  return ret;
}

int
pam_sm_authenticate(pam_handle_t *pamh,
		    int flags,
		    int argc,
		    const char **argv)
{
  char *user;
  int ret;
  struct pam_conv *conv;
  struct passwd *pw;
  uid_t uid = -1;
  const char *name, *inst;
  char realm[REALM_SZ];
  realm[0] = 0;

  parse_ctrl(argc, argv);
  ENTRY("pam_sm_authenticate");

  ret = pam_get_user(pamh, &user, "login: ");
  if (ret != PAM_SUCCESS)
    return ret;

  if (ctrl_on(KRB4_IGNORE_ROOT) && strcmp(user, "root") == 0)
    return PAM_AUTHINFO_UNAVAIL;

  ret = pam_get_item(pamh, PAM_CONV, (void*)&conv);
  if (ret != PAM_SUCCESS)
    return ret;

  pw = getpwnam(user);
  if (pw != 0)
    {
      uid = pw->pw_uid;
      set_tkt_string(uid);
    }
    
  if (strcmp(user, "root") == 0 && getuid() != 0)
    {
      pw = getpwuid(getuid());
      if (pw != 0)
	{
	  name = strdup(pw->pw_name);
	  inst = "root";
	}
    }
  else
    {
      name = user;
      inst = "";
    }

  ret = krb4_auth(pamh, flags, name, inst, conv);

  /*
   * The realm was lost inside krb_verify_user() so we can't simply do
   * a krb_kuserok() when inst != "".
   */
  if (ret == PAM_SUCCESS && inst[0] != 0)
    {
      uid_t old_euid = geteuid();
      uid_t old_ruid = getuid();

      setreuid(0, 0);		/* To read ticket file. */
      if (krb_get_tf_fullname(tkt_string(), 0, 0, realm) != KSUCCESS)
	ret = PAM_SERVICE_ERR;
      else if (krb_kuserok(name, inst, realm, user) != KSUCCESS)
	{
	  setreuid(0, uid);	/*  To read ~/.klogin. */
	  if (krb_kuserok(name, inst, realm, user) != KSUCCESS)
	    ret = PAM_PERM_DENIED;
	}

      if (ret != PAM_SUCCESS)
	{
	  dest_tkt();		/* Passwd known, ok to kill ticket. */
	  psyslog(LOG_NOTICE,
		  "%s.%s@%s is not allowed to log in as %s",
		  name, inst, realm, user);
	}

      setreuid(old_ruid, old_euid);
      if (getuid() != old_ruid || geteuid() != old_euid)
	{
	  psyslog(LOG_ALERT , "setreuid(%d, %d) failed at line %d",
		  old_ruid, old_euid, __LINE__);
	  exit(1);
	}
    }

  if (ret == PAM_SUCCESS)
    {
      psyslog(LOG_INFO,
	      "%s.%s@%s authenticated as user %s",
	      name, inst, realm, user);
      if (chown(tkt_string(), uid, -1) == -1)
	{
	  dest_tkt();
	  psyslog(LOG_ALERT , "chown(%s, %d, -1) failed", tkt_string(), uid);
	  exit(1);
	}
    }

  /*
   * Kludge alert!!! Sun dtlogin unlock screen fails to call
   * pam_setcred(3) with PAM_REFRESH_CRED after a successful
   * authentication attempt, sic.
   *
   * This hack is designed as a workaround to that problem.
   */
  if (ctrl_on(KRB4_REAFSLOG))
    if (ret == PAM_SUCCESS)
      pam_sm_setcred(pamh, PAM_REFRESH_CRED, argc, argv);
  
  return ret;
}

int 
pam_sm_setcred(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
  parse_ctrl(argc, argv);
  ENTRY("pam_sm_setcred");

  switch (flags & ~PAM_SILENT) {
  case 0:
  case PAM_ESTABLISH_CRED:
    if (k_hasafs())
      k_setpag();
    /* Fall through, fill PAG with credentials below. */
  case PAM_REINITIALIZE_CRED:
  case PAM_REFRESH_CRED:
    if (k_hasafs())
      {
	void *user = 0;

	if (pam_get_item(pamh, PAM_USER, &user) == PAM_SUCCESS)
	  {
	    struct passwd *pw = getpwnam((char *)user);
	    if (pw != 0)
	      krb_afslog_uid_home(/*cell*/ 0,/*realm_hint*/ 0,
				  pw->pw_uid, pw->pw_dir);
	  }
      }
    break;
  case PAM_DELETE_CRED:
    dest_tkt();
    if (k_hasafs())
      k_unlog();
    break;
  default:
    psyslog(LOG_ALERT , "pam_sm_setcred: unknown flags 0x%x", flags);
    break;
  }
  
  return PAM_SUCCESS;
}

int
pam_sm_open_session(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
  parse_ctrl(argc, argv);
  ENTRY("pam_sm_open_session");

  return PAM_SUCCESS;
}


int
pam_sm_close_session(pam_handle_t *pamh, int flags, int argc, const char**argv)
{
  parse_ctrl(argc, argv);
  ENTRY("pam_sm_close_session");

  /* This isn't really kosher, but it's handy. */
  pam_sm_setcred(pamh, PAM_DELETE_CRED, argc, argv);

  return PAM_SUCCESS;
}
