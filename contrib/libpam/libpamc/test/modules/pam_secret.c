/*
 * $Id: pam_secret.c,v 1.2 2001/01/20 22:29:47 agmorgan Exp $
 *
 * Copyright (c) 1999 Andrew G. Morgan <morgan@linux.kernel.org>
 */

/*
 * WARNING: AS WRITTEN THIS CODE IS NOT SECURE. THE MD5 IMPLEMENTATION
 *          NEEDS TO BE INTEGRATED MORE NATIVELY.
 */

/* #define DEBUG */

#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <security/pam_modules.h>
#include <security/pam_client.h>
#include <security/_pam_macros.h>

/*
 * This is a sample module that demonstrates the use of binary prompts
 * and how they can be used to implement sophisticated authentication
 * schemes.
 */

struct ps_state_s {
    int retval;        /* last retval returned by the authentication fn */
    int state;         /* what state the module was in when it
			  returned incomplete */

    char *username;    /* the name of the local user */

    char server_cookie[33]; /* storage for 32 bytes of server cookie */
    char client_cookie[33]; /* storage for 32 bytes of client cookie */

    char *secret_data; /* pointer to <NUL> terminated secret_data */
    int invalid_secret;  /* indication of whether the secret is valid */

    pamc_bp_t current_prompt;    /* place to store the current prompt */
    pamc_bp_t current_reply;     /* place to receive the reply prompt */
};

#define PS_STATE_ID   "PAM_SECRET__STATE"
#define PS_AGENT_ID   "secret@here"
#define PS_STATE_DEAD          0
#define PS_STATE_INIT          1
#define PS_STATE_PROMPT1       2
#define PS_STATE_PROMPT2       3

#define MAX_LEN_HOSTNAME       512
#define MAX_FILE_LINE_LEN      1024

/*
 * Routine for generating 16*8 bits of random data represented in ASCII hex
 */

static int generate_cookie(unsigned char *buffer_33)
{
    static const char hexarray[] = "0123456789abcdef";
    int i, fd;

    /* fill buffer_33 with 32 hex characters (lower case) + '\0' */
    fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
	D(("failed to open /dev/urandom"));
	return 0;
    }
    read(fd, buffer_33 + 16, 16);
    close(fd);

    /* expand top 16 bytes into 32 nibbles */
    for (i=0; i<16; ++i) {
	buffer_33[2*i  ] = hexarray[(buffer_33[16+i] & 0xf0)>>4];
	buffer_33[2*i+1] = hexarray[(buffer_33[16+i] & 0x0f)];
    }

    buffer_33[32] = '\0';

    return 1;
}

/*
 * XXX - This is a hack, and is fundamentally insecure. Its subject to
 * all sorts of attacks not to mention the fact that all our secrets
 * will be displayed on the command line for someone doing 'ps' to
 * see. This is just for programming convenience in this instance, it
 * needs to be replaced with the md5 code. Although I am loath to
 * add yet another instance of md5 code to the Linux-PAM source code.
 * [Need to think of a cleaner way to do this for the distribution as
 * a whole...]
 */

#define COMMAND_FORMAT "/bin/echo -n '%s|%s|%s'|/usr/bin/md5sum -"

int create_digest(const char *d1, const char *d2, const char *d3,
		  char *buffer_33)
{
    int length;
    char *buffer;
    FILE *pipe;

    length = strlen(d1)+strlen(d2)+strlen(d3)+sizeof(COMMAND_FORMAT);
    buffer = malloc(length);
    if (buffer == NULL) {
	D(("out of memory"));
	return 0;
    }

    sprintf(buffer, COMMAND_FORMAT, d1,d2,d3);

    D(("executing pipe [%s]", buffer));
    pipe = popen(buffer, "r");
    memset(buffer, 0, length);
    free(buffer);

    if (pipe == NULL) {
	D(("failed to launch pipe"));
	return 0;
    }

    if (fgets(buffer_33, 33, pipe) == NULL) {
	D(("failed to read digest"));
	return 0;
    }

    if (strlen(buffer_33) != 32) {
	D(("digest was not 32 chars"));
	return 0;
    }

    fclose(pipe);

    D(("done [%s]", buffer_33));

    return 1;
}

/*
 * method to attempt to instruct the application's conversation function
 */

static int converse(pam_handle_t *pamh, struct ps_state_s *new)
{
    int retval;
    struct pam_conv *conv;

    D(("called"));

    retval = pam_get_item(pamh, PAM_CONV, (const void **) &conv);
    if (retval == PAM_SUCCESS) {
	struct pam_message msg;
	struct pam_response *single_reply;
	const struct pam_message *msg_ptr;

	memset(&msg, 0, sizeof(msg));
	msg.msg_style = PAM_BINARY_PROMPT;
	msg.msg = (const char *) new->current_prompt;
	msg_ptr = &msg;

	single_reply = NULL;
	retval = conv->conv(1, &msg_ptr, &single_reply, conv->appdata_ptr);
	if (retval == PAM_SUCCESS) {
	    if ((single_reply == NULL) || (single_reply->resp == NULL)) {
		retval == PAM_CONV_ERR;
	    } else {
		new->current_reply = (pamc_bp_t) single_reply->resp;
		single_reply->resp = NULL;
	    }
	}

	if (single_reply) {
	    free(single_reply);
	}
    }

#ifdef DEBUG
    if (retval == PAM_SUCCESS) {
	D(("reply has length=%d and control=%u",
	   PAM_BP_LENGTH(new->current_reply),
	   PAM_BP_CONTROL(new->current_reply)));
    }
    D(("returning %s", pam_strerror(pamh, retval)));
#endif

    return retval;
}

/*
 * identify the secret in question
 */

#define SECRET_FILE_FORMAT "%s/.secret@here"

char *identify_secret(char *identity, const char *user)
{
    struct passwd *pwd;
    char *temp;
    FILE *secrets;
    int length_id;

    pwd = getpwnam(user);
    if ((pwd == NULL) || (pwd->pw_dir == NULL)) {
	D(("user [%s] is not known", user));
    }

    length_id = strlen(pwd->pw_dir) + sizeof(SECRET_FILE_FORMAT);
    temp = malloc(length_id);
    if (temp == NULL) {
	D(("out of memory"));
	pwd = NULL;
	return NULL;
    }

    sprintf(temp, SECRET_FILE_FORMAT, pwd->pw_dir);
    pwd = NULL;

    D(("opening key file [%s]", temp));
    secrets = fopen(temp, "r");
    memset(temp, 0, length_id);

    if (secrets == NULL) {
	D(("failed to open key file"));
	return NULL;
    }

    length_id = strlen(identity);
    temp = malloc(MAX_FILE_LINE_LEN);

    for (;;) {
	char *secret = NULL;

	if (fgets(temp, MAX_FILE_LINE_LEN, secrets) == NULL) {
	    fclose(secrets);
	    return NULL;
	}

	D(("cf[%s][%s]", identity, temp));
	if (memcmp(temp, identity, length_id)) {
	    continue;
	}

	D(("found entry"));
	fclose(secrets);

	for (secret=temp+length_id; *secret; ++secret) {
	    if (!(*secret == ' ' || *secret == '\n' || *secret == '\t')) {
		break;
	    }
	}

	memmove(temp, secret, MAX_FILE_LINE_LEN-(secret-(temp+length_id)));
	secret = temp;

	for (; *secret; ++secret) {
	    if (*secret == ' ' || *secret == '\n' || *secret == '\t') {
		break;
	    }
	}

	if (*secret) {
	    *secret = '\0';
	}

	D(("secret found [%s]", temp));

	return temp;
    }

    /* NOT REACHED */
}

/*
 * function to perform the two message authentication process
 * (with support for event driven conversation functions)
 */

static int auth_sequence(pam_handle_t *pamh,
			 const struct ps_state_s *old, struct ps_state_s *new)
{
    const char *rhostname;
    const char *rusername;
    int retval;

    retval = pam_get_item(pamh, PAM_RUSER, (const void **) &rusername);
    if ((retval != PAM_SUCCESS) || (rusername == NULL)) {
	D(("failed to obtain an rusername"));
	new->state = PS_STATE_DEAD;
	return PAM_AUTH_ERR;
    }

    retval = pam_get_item(pamh, PAM_RHOST, (const void **) &rhostname);
    if ((retval != PAM_SUCCESS) || (rhostname == NULL)) {
	D(("failed to identify local hostname: ", pam_strerror(pamh, retval)));
	new->state = PS_STATE_DEAD;
	return PAM_AUTH_ERR;
    }

    D(("switch on new->state=%d [%s@%s]", new->state, rusername, rhostname));
    switch (new->state) {

    case PS_STATE_INIT:
    {
	const char *user = NULL;

	retval = pam_get_user(pamh, &user, NULL);

	if ((retval == PAM_SUCCESS) && (user == NULL)) {
	    D(("success but no username?"));
	    new->state = PS_STATE_DEAD;
	    retval = PAM_USER_UNKNOWN;
	}

	if (retval != PAM_SUCCESS) {
	    if (retval == PAM_CONV_AGAIN) {
		retval = PAM_INCOMPLETE;
	    } else {
		new->state = PS_STATE_DEAD;
	    }
	    D(("state init failed: %s", pam_strerror(pamh, retval)));
	    return retval;
	}

	/* nothing else in this 'case' can be retried */

	new->username = strdup(user);
	if (new->username == NULL) {
	    D(("out of memory"));
	    new->state = PS_STATE_DEAD;
	    return PAM_BUF_ERR;
	}

	if (! generate_cookie(new->server_cookie)) {
	    D(("problem generating server cookie"));
	    new->state = PS_STATE_DEAD;
	    return PAM_ABORT;
	}

	new->current_prompt = NULL;
	PAM_BP_RENEW(&new->current_prompt, PAM_BPC_SELECT,
		     sizeof(PS_AGENT_ID) + strlen(rusername) + 1
		     + strlen(rhostname) + 1 + 32);
	sprintf(PAM_BP_WDATA(new->current_prompt),
		PS_AGENT_ID "/%s@%s|%.32s", rusername, rhostname,
		new->server_cookie);

	/* note, the BP is guaranteed by the spec to be <NUL> terminated */
	D(("initialization packet [%s]", PAM_BP_DATA(new->current_prompt)));

	/* fall through */
	new->state = PS_STATE_PROMPT1;

	D(("fall through to state_prompt1"));
    }

    case PS_STATE_PROMPT1:
    {
	int i, length;

	/* send {secret@here/jdoe@client.host|<s_cookie>} */
	retval = converse(pamh, new);
	if (retval != PAM_SUCCESS) {
	    if (retval == PAM_CONV_AGAIN) {
		D(("conversation failed to complete"));
		return PAM_INCOMPLETE;
	    } else {
		new->state = PS_STATE_DEAD;
		return retval;
	    }
	}

	if (retval != PAM_SUCCESS) {
	    D(("failed to read ruser@rhost"));
	    new->state = PS_STATE_DEAD;
	    return PAM_AUTH_ERR;
	}

	/* expect to receive the following {<seqid>|<a_cookie>} */
	if (new->current_reply == NULL) {
	    D(("converstation returned [%s] but gave no reply",
	       pam_strerror(pamh, retval)));
	    new->state = PS_STATE_DEAD;
	    return PAM_CONV_ERR;
	}

	/* find | */
	length = PAM_BP_LENGTH(new->current_reply);
	for (i=0; i<length; ++i) {
	    if (PAM_BP_RDATA(new->current_reply)[i] == '|') {
		break;
	    }
	}
	if (i >= length) {
	    D(("malformed response (no |) of length %d", length));
	    new->state = PS_STATE_DEAD;
	    return PAM_CONV_ERR;
	}
	if ((length - ++i) != 32) {
	    D(("cookie is incorrect length (%d,%d) %d != 32",
	       length, i, length-i));
	    new->state = PS_STATE_DEAD;
	    return PAM_CONV_ERR;
	}

	/* copy client cookie */
	memcpy(new->client_cookie, PAM_BP_RDATA(new->current_reply)+i, 32);

	/* generate a prompt that is length(seqid) + length(|) + 32 long */
	PAM_BP_RENEW(&new->current_prompt, PAM_BPC_OK, i+32);
	/* copy the head of the response prompt */
	memcpy(PAM_BP_WDATA(new->current_prompt),
	       PAM_BP_RDATA(new->current_reply), i);
	PAM_BP_RENEW(&new->current_reply, 0, 0);

	/* look up the secret */
	new->invalid_secret = 0;

	if (new->secret_data == NULL) {
	    char *ruser_rhost;

	    ruser_rhost = malloc(strlen(rusername)+2+strlen(rhostname));
	    if (ruser_rhost == NULL) {
		D(("out of memory"));
		new->state = PS_STATE_DEAD;
		return PAM_BUF_ERR;
	    }
	    sprintf(ruser_rhost, "%s@%s", rusername, rhostname);
	    new->secret_data = identify_secret(ruser_rhost, new->username);

	    memset(ruser_rhost, 0, strlen(ruser_rhost));
	    free(ruser_rhost);
	}

	if (new->secret_data == NULL) {
	    D(("secret not found for user"));
	    new->invalid_secret = 1;

	    /* need to make up a secret */
	    new->secret_data = malloc(32 + 1);
	    if (new->secret_data == NULL) {
		D(("out of memory"));
		new->state = PS_STATE_DEAD;
		return PAM_BUF_ERR;
	    }
	    if (! generate_cookie(new->secret_data)) {
		D(("what's up - no fake cookie generated?"));
		new->state = PS_STATE_DEAD;
		return PAM_ABORT;
	    }
	}

	/* construct md5[<client_cookie>|<server_cookie>|<secret_data>] */
	if (! create_digest(new->client_cookie, new->server_cookie,
			    new->secret_data,
			    PAM_BP_WDATA(new->current_prompt)+i)) {
	    D(("md5 digesting failed"));
	    new->state = PS_STATE_DEAD;
	    return PAM_ABORT;
	}

	/* prompt2 is now constructed - fall through to send it */
    }

    case PS_STATE_PROMPT2:
    {
	/* send {<seqid>|md5[<client_cookie>|<server_cookie>|<secret_data>]} */
	retval = converse(pamh, new);
	if (retval != PAM_SUCCESS) {
	    if (retval == PAM_CONV_AGAIN) {
		D(("conversation failed to complete"));
		return PAM_INCOMPLETE;
	    } else {
		new->state = PS_STATE_DEAD;
		return retval;
	    }
	}

	/* After we complete this section, we should not be able to
	   recall this authentication function. So, we force all
	   future calls into the weeds. */

	new->state = PS_STATE_DEAD;

	/* expect reply:{md5[<secret_data>|<server_cookie>|<client_cookie>]} */

	{
	    int cf;
	    char expectation[33];

	    if (!create_digest(new->secret_data, new->server_cookie,
			       new->client_cookie, expectation)) {
		new->state = PS_STATE_DEAD;
		return PAM_ABORT;
	    }

	    cf = strcmp(expectation, PAM_BP_RDATA(new->current_reply));
	    memset(expectation, 0, sizeof(expectation));
	    if (cf || new->invalid_secret) {
		D(("failed to authenticate"));
		return PAM_AUTH_ERR;
	    }
	}

	D(("correctly authenticated :)"));
	return PAM_SUCCESS;
    }

    default:
	new->state = PS_STATE_DEAD;

    case PS_STATE_DEAD:

	D(("state is currently dead/unknown"));
	return PAM_AUTH_ERR;
    }

    fprintf(stderr, "pam_secret: this should not be reached\n");
    return PAM_ABORT;
}

static void clean_data(pam_handle_t *pamh, void *datum, int error_status)
{
    struct ps_state_s *data = datum;

    D(("liberating datum=%p", datum));

    if (data) {
	D(("renew prompt"));
	PAM_BP_RENEW(&data->current_prompt, 0, 0);
	D(("renew reply"));
	PAM_BP_RENEW(&data->current_reply, 0, 0);
	D(("overwrite datum"));
	memset(data, 0, sizeof(struct ps_state_s));
	D(("liberate datum"));
	free(data);
    }

    D(("done."));
}

/*
 * front end for the authentication function
 */

int pam_sm_authenticate(pam_handle_t *pamh, int flags,
			int argc, const char **argv)
{
    int retval;
    struct ps_state_s *new_data;
    const struct ps_state_s *old_data;

    D(("called"));

    new_data = calloc(1, sizeof(struct ps_state_s));
    if (new_data == NULL) {
	D(("out of memory"));
	return PAM_BUF_ERR;
    }
    new_data->retval = PAM_SUCCESS;

    retval = pam_get_data(pamh, PS_STATE_ID, (const void **) &old_data);
    if (retval == PAM_SUCCESS) {
	new_data->state = old_data->state;
	memcpy(new_data->server_cookie, old_data->server_cookie, 32);
	memcpy(new_data->client_cookie, old_data->client_cookie, 32);
	if (old_data->username) {
	    new_data->username = strdup(old_data->username);
	}
	if (old_data->secret_data) {
	    new_data->secret_data = strdup(old_data->secret_data);
	}
	if (old_data->current_prompt) {
	    int length;

	    length = PAM_BP_LENGTH(old_data->current_prompt);
	    PAM_BP_RENEW(&new_data->current_prompt,
			 PAM_BP_CONTROL(old_data->current_prompt), length);
	    PAM_BP_FILL(new_data->current_prompt, 0, length,
			PAM_BP_RDATA(old_data->current_prompt));
	}
	/* don't need to duplicate current_reply */
    } else {
	old_data = NULL;
	new_data->state = PS_STATE_INIT;
    }

    D(("call auth_sequence"));
    new_data->retval = auth_sequence(pamh, old_data, new_data);
    D(("returned from auth_sequence"));

    retval = pam_set_data(pamh, PS_STATE_ID, new_data, clean_data);
    if (retval != PAM_SUCCESS) {
	D(("unable to store new_data"));
    } else {
	retval = new_data->retval;
    }

    old_data = new_data = NULL;

    D(("done (%d)", retval));
    return retval;
}

/*
 * front end for the credential setting function
 */

#define AUTH_SESSION_TICKET_ENV_FORMAT "AUTH_SESSION_TICKET="

int pam_sm_setcred(pam_handle_t *pamh, int flags,
		   int argc, const char **argv)
{
    int retval;
    const struct ps_state_s *old_data;

    D(("called"));

    /* XXX - need to pay attention to the various flavors of call */

    /* XXX - need provide an option to turn this feature on/off: if
       other modules want to supply an AUTH_SESSION_TICKET, we should
       leave it up to the admin which module dominiates. */

    retval = pam_get_data(pamh, PS_STATE_ID, (const void **) &old_data);
    if (retval != PAM_SUCCESS) {
	D(("no data to base decision on"));
	return PAM_AUTH_ERR;
    }

    /*
     * If ok, export a derived shared secret session ticket to the
     * client's PAM environment - the ticket has the form
     *
     * AUTH_SESSION_TICKET =
     *        md5[<server_cookie>|<secret_data>|<client_cookie>]
     *
     * This is a precursor to supporting a spoof resistant trusted
     * path mechanism. This shared secret ticket can be used to add
     * a hard-to-guess checksum to further authentication data.
     */

    retval = old_data->retval;
    if (retval == PAM_SUCCESS) {
	char envticket[sizeof(AUTH_SESSION_TICKET_ENV_FORMAT)+32];

	memcpy(envticket, AUTH_SESSION_TICKET_ENV_FORMAT,
	       sizeof(AUTH_SESSION_TICKET_ENV_FORMAT));

	if (! create_digest(old_data->server_cookie, old_data->secret_data,
			    old_data->client_cookie,
			    envticket+sizeof(AUTH_SESSION_TICKET_ENV_FORMAT)-1
	    )) {
	    D(("unable to generate a digest for session ticket"));
	    return PAM_ABORT;
	}

	D(("putenv[%s]", envticket));
	retval = pam_putenv(pamh, envticket);
	memset(envticket, 0, sizeof(envticket));
    }

    old_data = NULL;
    D(("done (%d)", retval));
    
    return retval;
}
