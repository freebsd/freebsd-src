/*
 * Copyright (c) 2000 Damien Miller.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 */

#include "includes.h"

#ifdef USE_PAM
#include <security/pam_appl.h>
#include "ssh.h"
#include "xmalloc.h"
#include "log.h"
#include "servconf.h"
#include "readpass.h"
#include "canohost.h"

RCSID("$FreeBSD$");

#define NEW_AUTHTOK_MSG \
	"Warning: Your password has expired, please change it now"

#define SSHD_PAM_SERVICE "sshd"
#define PAM_STRERROR(a, b) pam_strerror((a), (b))

/* Callbacks */
static int pamconv(int num_msg, const struct pam_message **msg,
	  struct pam_response **resp, void *appdata_ptr);
void pam_cleanup_proc(void *context);
void pam_msg_cat(const char *msg);

/* module-local variables */
static struct pam_conv conv = {
	pamconv,
	NULL
};
static pam_handle_t *pamh = NULL;
static const char *pampasswd = NULL;
static char *pam_msg = NULL;
extern ServerOptions options;

/* states for pamconv() */
typedef enum { INITIAL_LOGIN, OTHER } pamstates;
static pamstates pamstate = INITIAL_LOGIN;
/* remember whether pam_acct_mgmt() returned PAM_NEWAUTHTOK_REQD */
static int password_change_required = 0;

/*
 * PAM conversation function.
 * There are two states this can run in.
 *
 * INITIAL_LOGIN mode simply feeds the password from the client into
 * PAM in response to PAM_PROMPT_ECHO_OFF, and collects output
 * messages with pam_msg_cat().  This is used during initial
 * authentication to bypass the normal PAM password prompt.
 *
 * OTHER mode handles PAM_PROMPT_ECHO_OFF with read_passphrase(prompt, 1)
 * and outputs messages to stderr. This mode is used if pam_chauthtok()
 * is called to update expired passwords.
 */
static int pamconv(int num_msg, const struct pam_message **msg,
	struct pam_response **resp, void *appdata_ptr)
{
	struct pam_response *reply;
	int count;
	char buf[1024];

	/* PAM will free this later */
	reply = malloc(num_msg * sizeof(*reply));
	if (reply == NULL)
		return PAM_CONV_ERR; 

	for (count = 0; count < num_msg; count++) {
		switch ((*msg)[count].msg_style) {
			case PAM_PROMPT_ECHO_ON:
				if (pamstate == INITIAL_LOGIN) {
					free(reply);
					return PAM_CONV_ERR;
				} else {
					fputs((*msg)[count].msg, stderr);
					fgets(buf, sizeof(buf), stdin);
					reply[count].resp = xstrdup(buf);
					reply[count].resp_retcode = PAM_SUCCESS;
					break;
				}
			case PAM_PROMPT_ECHO_OFF:
				if (pamstate == INITIAL_LOGIN) {
					if (pampasswd == NULL) {
						free(reply);
						return PAM_CONV_ERR;
					}
					reply[count].resp = xstrdup(pampasswd);
				} else {
					reply[count].resp = 
						xstrdup(read_passphrase((*msg)[count].msg, 1));
				}
				reply[count].resp_retcode = PAM_SUCCESS;
				break;
			case PAM_ERROR_MSG:
			case PAM_TEXT_INFO:
				if ((*msg)[count].msg != NULL) {
					if (pamstate == INITIAL_LOGIN)
						pam_msg_cat((*msg)[count].msg);
					else {
						fputs((*msg)[count].msg, stderr);
						fputs("\n", stderr);
					}
				}
				reply[count].resp = xstrdup("");
				reply[count].resp_retcode = PAM_SUCCESS;
				break;
			default:
				free(reply);
				return PAM_CONV_ERR;
		}
	}

	*resp = reply;

	return PAM_SUCCESS;
}

/* Called at exit to cleanly shutdown PAM */
void pam_cleanup_proc(void *context)
{
	int pam_retval;

	if (pamh != NULL)
	{
		pam_retval = pam_close_session(pamh, 0);
		if (pam_retval != PAM_SUCCESS) {
			log("Cannot close PAM session[%d]: %.200s", 
				pam_retval, PAM_STRERROR(pamh, pam_retval));
		}

		pam_retval = pam_setcred(pamh, PAM_DELETE_CRED);
		if (pam_retval != PAM_SUCCESS) {
			debug("Cannot delete credentials[%d]: %.200s", 
				pam_retval, PAM_STRERROR(pamh, pam_retval));
		}

		pam_retval = pam_end(pamh, pam_retval);
		if (pam_retval != PAM_SUCCESS) {
			log("Cannot release PAM authentication[%d]: %.200s", 
				pam_retval, PAM_STRERROR(pamh, pam_retval));
		}
	}
}

/* Attempt password authentation using PAM */
int auth_pam_password(Authctxt *authctxt, const char *password)
{
	struct passwd *pw = authctxt->pw;
	int pam_retval;

	/* deny if no user. */
	if (pw == NULL)
		return 0;
	if (pw->pw_uid == 0 && options.permit_root_login == 2)
		return 0;
	if (*password == '\0' && options.permit_empty_passwd == 0)
		return 0;

	pampasswd = password;
	
	pamstate = INITIAL_LOGIN;
	pam_retval = pam_authenticate(pamh, 0);
	if (pam_retval == PAM_SUCCESS) {
		debug("PAM Password authentication accepted for user \"%.100s\"", 
			pw->pw_name);
		return 1;
	} else {
		debug("PAM Password authentication for \"%.100s\" failed[%d]: %s", 
			pw->pw_name, pam_retval, PAM_STRERROR(pamh, pam_retval));
		return 0;
	}
}

/* Do account management using PAM */
int do_pam_account(char *username, char *remote_user)
{
	int pam_retval;
	
	debug("PAM setting rhost to \"%.200s\"", 
	    get_canonical_hostname(options.reverse_mapping_check));
	pam_retval = pam_set_item(pamh, PAM_RHOST, 
		get_canonical_hostname(options.reverse_mapping_check));
	if (pam_retval != PAM_SUCCESS) {
		fatal("PAM set rhost failed[%d]: %.200s", 
			pam_retval, PAM_STRERROR(pamh, pam_retval));
	}

	if (remote_user != NULL) {
		debug("PAM setting ruser to \"%.200s\"", remote_user);
		pam_retval = pam_set_item(pamh, PAM_RUSER, remote_user);
		if (pam_retval != PAM_SUCCESS) {
			fatal("PAM set ruser failed[%d]: %.200s", 
				pam_retval, PAM_STRERROR(pamh, pam_retval));
		}
	}

	pam_retval = pam_acct_mgmt(pamh, 0);
	switch (pam_retval) {
		case PAM_SUCCESS:
			/* This is what we want */
			break;
		case PAM_NEW_AUTHTOK_REQD:
			pam_msg_cat(NEW_AUTHTOK_MSG);
			/* flag that password change is necessary */
			password_change_required = 1;
			break;
		default:
			log("PAM rejected by account configuration[%d]: %.200s", 
				pam_retval, PAM_STRERROR(pamh, pam_retval));
			return(0);
	}
	
	return(1);
}

/* Do PAM-specific session initialisation */
void do_pam_session(char *username, const char *ttyname)
{
	int pam_retval;

	if (ttyname != NULL) {
		debug("PAM setting tty to \"%.200s\"", ttyname);
		pam_retval = pam_set_item(pamh, PAM_TTY, ttyname);
		if (pam_retval != PAM_SUCCESS) {
			fatal("PAM set tty failed[%d]: %.200s", 
				pam_retval, PAM_STRERROR(pamh, pam_retval));
		}
	}

	debug("do_pam_session: euid %u, uid %u", geteuid(), getuid());
	pam_retval = pam_open_session(pamh, 0);
	if (pam_retval != PAM_SUCCESS) {
		fatal("PAM session setup failed[%d]: %.200s", 
			pam_retval, PAM_STRERROR(pamh, pam_retval));
	}
}

/* Set PAM credentials */ 
void do_pam_setcred(void)
{
	int pam_retval;
 
	debug("PAM establishing creds");
	pam_retval = pam_setcred(pamh, PAM_ESTABLISH_CRED);
	if (pam_retval != PAM_SUCCESS) {
		debug("PAM setcred failed[%d]: %.200s", 
			pam_retval, PAM_STRERROR(pamh, pam_retval));
	}
}

/* accessor function for file scope static variable */
int pam_password_change_required(void)
{
	return password_change_required;
}

/* 
 * Have user change authentication token if pam_acct_mgmt() indicated
 * it was expired.  This needs to be called after an interactive
 * session is established and the user's pty is connected to
 * stdin/stout/stderr.
 */
void do_pam_chauthtok(void)
{
	int pam_retval;

	if (password_change_required) {
		pamstate = OTHER;
		/*
		 * XXX: should we really loop forever?
		 */
		do {
			pam_retval = pam_chauthtok(pamh, PAM_CHANGE_EXPIRED_AUTHTOK);
			if (pam_retval != PAM_SUCCESS) {
				log("PAM pam_chauthtok failed[%d]: %.200s", 
					pam_retval, PAM_STRERROR(pamh, pam_retval));
			}
		} while (pam_retval != PAM_SUCCESS);
	}
}

/* Cleanly shutdown PAM */
void finish_pam(void)
{
	pam_cleanup_proc(NULL);
	fatal_remove_cleanup(&pam_cleanup_proc, NULL);
}

/* Start PAM authentication for specified account */
void start_pam(struct passwd *pw)
{
	int pam_retval;

	debug("Starting up PAM with username \"%.200s\"", pw->pw_name);

	pam_retval = pam_start(SSHD_PAM_SERVICE, pw->pw_name, &conv, &pamh);

	if (pam_retval != PAM_SUCCESS) {
		fatal("PAM initialisation failed[%d]: %.200s", 
			pam_retval, PAM_STRERROR(pamh, pam_retval));
	}

#ifdef PAM_TTY_KLUDGE
	/*
	 * Some PAM modules (e.g. pam_time) require a TTY to operate,
	 * and will fail in various stupid ways if they don't get one. 
	 * sshd doesn't set the tty until too late in the auth process and may
	 * not even need one (for tty-less connections)
	 * Kludge: Set a fake PAM_TTY 
	 */
	pam_retval = pam_set_item(pamh, PAM_TTY, "ssh");
	if (pam_retval != PAM_SUCCESS) {
		fatal("PAM set tty failed[%d]: %.200s", 
			pam_retval, PAM_STRERROR(pamh, pam_retval));
	}
#endif /* PAM_TTY_KLUDGE */

	fatal_add_cleanup(&pam_cleanup_proc, NULL);
}

/* Return list of PAM enviornment strings */
char **fetch_pam_environment(void)
{
#ifdef HAVE_PAM_GETENVLIST
	return(pam_getenvlist(pamh));
#else /* HAVE_PAM_GETENVLIST */
	return(NULL);
#endif /* HAVE_PAM_GETENVLIST */
}

/* Print any messages that have been generated during authentication */
/* or account checking to stderr */
void print_pam_messages(void)
{
	if (pam_msg != NULL)
		fputs(pam_msg, stderr);
}

/* Append a message to the PAM message buffer */
void pam_msg_cat(const char *msg)
{
	char *p;
	size_t new_msg_len;
	size_t pam_msg_len;
	
	new_msg_len = strlen(msg);
	
	if (pam_msg) {
		pam_msg_len = strlen(pam_msg);
		pam_msg = xrealloc(pam_msg, new_msg_len + pam_msg_len + 2);
		p = pam_msg + pam_msg_len;
	} else {
		pam_msg = p = xmalloc(new_msg_len + 2);
	}

	memcpy(p, msg, new_msg_len);
	p[new_msg_len] = '\n';
	p[new_msg_len + 1] = '\0';
}

struct inverted_pam_userdata {
    /*
     * Pipe for telling whether we are doing conversation or sending
     * authentication results.
     */
    int statefd[2];
    int challengefd[2];
    int responsefd[2];

    /* Whether we have sent off our challenge */
    int state;
};

#define STATE_CONV	1
#define STATE_AUTH_OK	2
#define STATE_AUTH_FAIL	3

int
ssh_conv(int num_msg, const struct pam_message **msg, struct pam_response **resp,
	 void *userdata) {
	int i;
	FILE *reader;
	char buf[1024];
	struct pam_response *reply = NULL;
	char state_to_write = STATE_CONV; /* One char to write */
	struct inverted_pam_userdata *ud = userdata;
	char *response = NULL;
	
	/* The stdio functions are more convenient for the read half */
	reader = fdopen(ud->responsefd[0], "rb");
	if (reader == NULL)
		goto protocol_failure;

	reply = malloc(num_msg * sizeof(struct pam_response));
	if (reply == NULL)
		return PAM_CONV_ERR;

	if (write(ud->statefd[1], &state_to_write, 1) != 1)
		goto protocol_failure;
	
	/*
	 * Re-package our data and send it off to our better half (the actual SSH
	 * process)
	 */
	if (write(ud->challengefd[1], buf,
		  sprintf(buf, "%d\n", num_msg)) == -1)
		goto protocol_failure;
	for (i = 0; i < num_msg; i++) {
		if (write(ud->challengefd[1], buf,
			  sprintf(buf, "%d\n", msg[i]->msg_style)) == -1)
			goto protocol_failure;
		if (write(ud->challengefd[1], buf,
			  sprintf(buf, "%d\n", strlen(msg[i]->msg))) == -1)
			goto protocol_failure;
		if (write(ud->challengefd[1], msg[i]->msg,
			  strlen(msg[i]->msg)) == -1)
			goto protocol_failure;
	}
	/*
	 * Read back responses.  These may not be as nice as we want, as the SSH
	 * protocol isn't exactly a perfect fit with PAM.
	 */

	for (i = 0; i < num_msg; i++) {
		char buf[1024];
		char *endptr;
		size_t len;	/* Length of the response */

		switch (msg[i]->msg_style) {
		case PAM_PROMPT_ECHO_OFF:
		case PAM_PROMPT_ECHO_ON:
			if (fgets(buf, sizeof(buf), reader) == NULL)
				goto protocol_failure;
			len = (size_t)strtoul(buf, &endptr, 10);
			/* The length is supposed to stand on a line by itself */
			if (endptr == NULL || *endptr != '\n')
				goto protocol_failure;
			response = malloc(len+1);
			if (response == NULL)
				goto protocol_failure;
			if (fread(response, len, 1, reader) != 1)
				goto protocol_failure;
			response[len] = '\0';
			reply[i].resp = response;
			response = NULL;
			break;
		default:
			reply[i].resp = NULL;
			break;
		}
	}
	*resp = reply;
	return PAM_SUCCESS;
 protocol_failure:
	free(reply);
	return PAM_CONV_ERR;
}

void
ipam_free_cookie(struct inverted_pam_cookie *cookie) {
	struct inverted_pam_userdata *ud;
	int i;

	if (cookie == NULL)
		return;
	ud = cookie->userdata;
	cookie->userdata = NULL;
	/* Free userdata if allocated */
	if (ud) {
		/* Close any opened file descriptors */
		if (ud->statefd[0] != -1)
			close(ud->statefd[0]);
		if (ud->statefd[1] != -1)
			close(ud->statefd[1]);
		if (ud->challengefd[0] != -1)
			close(ud->challengefd[0]);
		if (ud->challengefd[1] != -1)
			close(ud->challengefd[1]);
		if (ud->responsefd[0] != -1)
			close(ud->responsefd[0]);
		if (ud->responsefd[1] != -1)
			close(ud->responsefd[1]);
		free(ud);
		ud = NULL;
	}
	/* Now free the normal cookie */
	if (cookie->pid != 0 && cookie->pid != -1) {
		int status;

		/* XXX Use different signal? */
		kill(cookie->pid, SIGKILL);
		waitpid(cookie->pid, &status, 0);
	}
	for (i = 0; i < cookie->num_msg; i++) {
		if (cookie->resp && cookie->resp[i]) {
			free(cookie->resp[i]->resp);
			free(cookie->resp[i]);
		}
		if (cookie->msg && cookie->msg[i]) {
			free((void *)cookie->msg[i]->msg);
			free(cookie->msg[i]);
		}
	}
	free(cookie->msg);
	free(cookie->resp);
	free(cookie);
}

/*
 * Do first half of PAM authentication - this comes to the point where
 * you get a message to send to the user.
 */
struct inverted_pam_cookie *
ipam_start_auth(const char *service, const char *username) {
	struct inverted_pam_cookie *cookie;
	struct inverted_pam_userdata *ud;
	static struct pam_conv conv = {
		ssh_conv,
		NULL
	};

	cookie = malloc(sizeof(*cookie));
	if (cookie == NULL)
		return NULL;
	cookie->state = 0;
	/* Set up the cookie so ipam_freecookie can be used on it */
	cookie->num_msg = 0;
	cookie->msg = NULL;
	cookie->resp = NULL;
	cookie->pid = -1;

	ud = calloc(sizeof(*ud), 1);
	if (ud == NULL) {
		free(cookie);
		return NULL;
	}
	cookie->userdata = ud;
	ud->statefd[0] = ud->statefd[1] = -1;
	ud->challengefd[0] = ud->challengefd[1] = -1;
	ud->responsefd[0] = ud->responsefd[1] = -1;

	if (pipe(ud->statefd) != 0) {
		ud->statefd[0] = ud->statefd[1] = -1;
		ipam_free_cookie(cookie);
		return NULL;
	}
	if (pipe(ud->challengefd) != 0) {
		ud->challengefd[0] = ud->challengefd[1] = -1;
		ipam_free_cookie(cookie);
		return NULL;
	}
	if (pipe(ud->responsefd) != 0) {
		ud->responsefd[0] = ud->responsefd[1] = -1;
		ipam_free_cookie(cookie);
		return NULL;
	}
	cookie->pid = fork();
	if (cookie->pid == -1) {
		ipam_free_cookie(cookie);
		return NULL;
	} else if (cookie->pid != 0) {
		int num_msgs;	/* Number of messages from PAM */
		char *endptr;
		char buf[1024];
		FILE *reader;
		size_t num_msg;
		int i;
		char state;	/* Which state did the connection just enter? */

		/* We are the parent - wait for a call to the communications
		   function to turn up, or the challenge to be finished */
		if (read(ud->statefd[0], &state, 1) != 1) {
			ipam_free_cookie(cookie);
			return NULL;
		}
		cookie->state = state;
		switch (state) {
		case STATE_CONV:
			/* We are running the conversation function */
			/* The stdio functions are more convenient for read */
			reader = fdopen(ud->challengefd[0], "r");
			if (reader == NULL) {
				ipam_free_cookie(cookie);
				return NULL;
			}
			if (fgets(buf, 4, reader) == NULL) {
				fclose(reader);
				ipam_free_cookie(cookie);
				return NULL;
			}
			num_msg = (size_t)strtoul(buf, &endptr, 10);
			/* The length is supposed to stand on a line by itself */
			if (endptr == NULL || *endptr != '\n') {
				fclose(reader);
				ipam_free_cookie(cookie);
				return NULL;
			}
			cookie->msg =
				malloc(sizeof(struct pam_message *) * num_msg);
			cookie->resp =
				malloc(sizeof(struct pam_response *) * num_msg);
			if (cookie->msg == NULL || cookie->resp == NULL) {
				fclose(reader);
				ipam_free_cookie(cookie);
				return NULL;
			}
			for (i = 0; i < num_msg; i++) {
				cookie->msg[i] =
					malloc(sizeof(struct pam_message));
				cookie->resp[i] =
					malloc(sizeof(struct pam_response));
				if (cookie->msg[i] == NULL ||
				    cookie->resp[i] == NULL) {
					for (;;) {
						free(cookie->msg[i]);
						free(cookie->resp[i]);
						if (i == 0)
							break;
						i--;
					}
					fclose(reader);
					ipam_free_cookie(cookie);
					return NULL;
				}
				cookie->msg[i]->msg = NULL;
				cookie->resp[i]->resp = NULL;
				cookie->resp[i]->resp_retcode = 0;
			}
			/* Set up so the above will be freed on failure */
			cookie->num_msg = num_msg;
			/*
			 * We have a an allocated response and message for
			 * each of the entries in the PAM structure - transfer
			 * the data sent to the conversation function over.
			 */
			for (i = 0; i < num_msg; i++) {
				size_t len;
			
				if (fgets(buf, sizeof(buf), reader) == NULL) {
					fclose(reader);
					ipam_free_cookie(cookie);
					return NULL;
				}
				cookie->msg[i]->msg_style =
					(size_t)strtoul(buf, &endptr, 10);
				if (endptr == NULL || *endptr != '\n') {
					fclose(reader);
					ipam_free_cookie(cookie);
					return NULL;
				}
				if (fgets(buf, sizeof(buf), reader) == NULL) {
					fclose(reader);
					ipam_free_cookie(cookie);
					return NULL;
				}
				len = (size_t)strtoul(buf, &endptr, 10);
				if (endptr == NULL || *endptr != '\n') {
					fclose(reader);
					ipam_free_cookie(cookie);
					return NULL;
				}
				cookie->msg[i]->msg = malloc(len + 1);
				if (cookie->msg[i]->msg == NULL) {
					fclose(reader);
					ipam_free_cookie(cookie);
					return NULL;
				}
				if (fread((char *)cookie->msg[i]->msg, len, 1, reader) !=
				    1) {
					fclose(reader);
					ipam_free_cookie(cookie);
					return NULL;
				}
				*(char *)&(cookie->msg[i]->msg[len]) = '\0';
			}
			break;
		case STATE_AUTH_OK:
		case STATE_AUTH_FAIL:
			break;
		default:
			/* Internal failure, somehow */
			fclose(reader);
			ipam_free_cookie(cookie);
			return NULL;
		}
		return cookie;
	} else {
		/* We are the child */
		pam_handle_t *pamh=NULL;
		int retval;
		char state;

		conv.appdata_ptr = ud;
		retval = pam_start(service, username, &conv, &pamh);
		/* Is user really user? */
		if (retval == PAM_SUCCESS)
			retval = pam_authenticate(pamh, 0);
		/* permitted access? */
		if (retval == PAM_SUCCESS)
			retval = pam_acct_mgmt(pamh, 0);
		/* This is where we have been authorized or not. */

		/* Be conservative - flag as auth failure if we can't close */
		/*
		 * XXX This is based on example code from Linux-PAM -
		 * but can it really be correct to pam_end if
		 * pam_start failed?
		 */
		if (pam_end(pamh, retval) != PAM_SUCCESS)
			retval = PAM_AUTH_ERR;

		/* Message to parent */
		state = retval == PAM_SUCCESS ? STATE_AUTH_OK : STATE_AUTH_FAIL;
		if (write(ud->statefd[1], &state, 1) != 1) {
			_exit(1);
		}
		/* FDs will be closed, so further communication will stop */
		_exit(0);
	}
}

/*
 * Do second half of PAM authentication - cookie should now be filled
 * in with the response to the challenge.
 */

int
ipam_complete_auth(struct inverted_pam_cookie *cookie) {
    int i;
    char buf[1024];
    struct inverted_pam_userdata *ud = cookie->userdata;
    char state;

    /* Send over our responses */
    for (i = 0; i < cookie->num_msg; i++) {
	if (cookie->msg[i]->msg_style != PAM_PROMPT_ECHO_ON &&
	    cookie->msg[i]->msg_style != PAM_PROMPT_ECHO_OFF)
	    continue;
	if (write(ud->responsefd[1], buf,
		  sprintf(buf, "%d\n", strlen(cookie->resp[i]->resp))) == -1) {
	    ipam_free_cookie(cookie);
	    return 0;
	}
	if (write(ud->responsefd[1], cookie->resp[i]->resp,
		  strlen(cookie->resp[i]->resp)) == -1) {
	    ipam_free_cookie(cookie);
	    return 0;
	}
    }
    /* Find out what state we are changing to */
    if (read(ud->statefd[0], &state, 1) != 1) {
	ipam_free_cookie(cookie);
	return 0;
    }
    
    return state == STATE_AUTH_OK ? 1 : 0;
}

#endif /* USE_PAM */
