/*
 * This is a small test program for testing libpamc against the
 * secret@here agent. It does the same as the test.secret@here perl
 * script in this directory, but via the libpamc API.
 */

#include <stdio.h>
#include <string.h>
#include <security/pam_client.h>
#include <ctype.h>

struct internal_packet {
    int length;
    int at;
    char *buffer;
};


void append_data(struct internal_packet *packet, int extra, const char *data)
{
    if ((extra + packet->at) >= packet->length) {
	if (packet->length == 0) {
	    packet->length = 1000;
	}
	/* make sure we have at least a char extra space available */
	while (packet->length <= (extra + packet->at)) {
	    packet->length <<= 1;
	}
	packet->buffer = realloc(packet->buffer, packet->length);
	if (packet->buffer == NULL) {
	    fprintf(stderr, "out of memory\n");
	    exit(1);
	}
    }

    if (data != NULL) {
	memcpy(packet->at + packet->buffer, data, extra);
    }
    packet->at += extra;

    /* assisting string manipulation */
    packet->buffer[packet->at] = '\0';
}

void append_string(struct internal_packet *packet, const char *string,
		   int with_nul)
{
    append_data(packet, strlen(string) + (with_nul ? 1:0), string);
}

char *identify_secret(char *identity)
{
    struct internal_packet temp_packet;
    FILE *secrets;
    int length_id;

    temp_packet.length = temp_packet.at = 0;
    temp_packet.buffer = NULL;

    append_string(&temp_packet, "/home/", 0);
    append_string(&temp_packet, getlogin(), 0);
    append_string(&temp_packet, "/.secret@here", 1);

    secrets = fopen(temp_packet.buffer, "r");
    if (secrets == NULL) {
	fprintf(stderr, "server: failed to open\n  [%s]\n",
		temp_packet.buffer);
	exit(1);
    }

    length_id = strlen(identity);
    for (;;) {
	char *secret = NULL;
	temp_packet.at = 0;

	if (fgets(temp_packet.buffer, temp_packet.length, secrets) == NULL) {
	    fclose(secrets);
	    return NULL;
	}

	if (memcmp(temp_packet.buffer, identity, length_id)) {
	    continue;
	}

	fclose(secrets);
	for (secret=temp_packet.buffer; *secret; ++secret) {
	    if (*secret == ' ' || *secret == '\n' || *secret == '\t') {
		break;
	    }
	}
	for (; *secret; ++secret) {
	    if (!(*secret == ' ' || *secret == '\n' || *secret == '\t')) {
		break;
	    }
	}

	for (temp_packet.buffer=secret; *temp_packet.buffer;
	     ++temp_packet.buffer) {
	    if (*temp_packet.buffer == ' ' || *temp_packet.buffer == '\n'
		|| *temp_packet.buffer == '\t') {
		break;
	    }
	}
	if (*temp_packet.buffer) {
	    *temp_packet.buffer = '\0';
	}

	return secret;
    }

    /* NOT REACHED */
}

/*
 * This is a hack, and is fundamentally insecure. All our secrets will be
 * displayed on the command line for someone doing 'ps' to see. This
 * is just for programming convenience in this instance, since this
 * program is simply a regression test. The pam_secret module should
 * not do this, but make use of md5 routines directly.
 */

char *create_digest(int length, const char *raw)
{
    struct internal_packet temp_packet;
    FILE *pipe;

    temp_packet.length = temp_packet.at = 0;
    temp_packet.buffer = NULL;

    append_string(&temp_packet, "echo -n '", 0);
    append_string(&temp_packet, raw, 0);
    append_string(&temp_packet, "'|/usr/bin/md5sum -", 1);

    fprintf(stderr, "am attempting to run [%s]\n", temp_packet.buffer);

    pipe = popen(temp_packet.buffer, "r");
    if (pipe == NULL) {
	fprintf(stderr, "server: failed to run\n  [%s]\n", temp_packet.buffer);
	exit(1);
    }

    temp_packet.at = 0;
    append_data(&temp_packet, 32, NULL);

    if (fgets(temp_packet.buffer, 33, pipe) == NULL) {
	fprintf(stderr, "server: failed to read digest\n");
	exit(1);
    }
    if (strlen(temp_packet.buffer) != 32) {
	fprintf(stderr, "server: digest was not 32 chars?? [%s]\n",
		temp_packet.buffer);
	exit(1);
    }

    fclose(pipe);

    return temp_packet.buffer;
}

void packet_to_prompt(pamc_bp_t *prompt_p, __u8 control,
		      struct internal_packet *packet)
{
    PAM_BP_RENEW(prompt_p, control, packet->at);
    PAM_BP_FILL(*prompt_p, 0, packet->at, packet->buffer);
    packet->at = 0;
}

void prompt_to_packet(pamc_bp_t prompt, struct internal_packet *packet)
{
    int data_length;

    data_length = PAM_BP_LENGTH(prompt);
    packet->at = 0;
    append_data(packet, data_length, NULL);
    
    PAM_BP_EXTRACT(prompt, 0, data_length, packet->buffer);

    fprintf(stderr, "server received[%d]: {%d|0x%.2x|%s}\n",
	    data_length,
	    PAM_BP_SIZE(prompt), PAM_BP_RCONTROL(prompt),
	    PAM_BP_RDATA(prompt));
}

int main(int argc, char **argv)
{
    pamc_handle_t pch;
    pamc_bp_t prompt = NULL;
    struct internal_packet packet_data, *packet;
    char *temp_string, *secret, *user, *a_cookie, *seqid, *digest;
    const char *cookie = "123451234512345";
    int retval;

    packet = &packet_data;
    packet->length = 0;
    packet->at = 0;
    packet->buffer = NULL;

    pch = pamc_start();
    if (pch == NULL) {
	fprintf(stderr, "server: unable to get a handle from libpamc\n");
	exit(1);
    }

    temp_string = getlogin();
    if (temp_string == NULL) {
	fprintf(stderr, "server: who are you?\n");
	exit(1);
    }
#define DOMAIN "@local.host"
    user = malloc(1+strlen(temp_string)+strlen(DOMAIN));
    if (user == NULL) {
	fprintf(stderr, "server: out of memory for user id\n");
	exit(1);
    }
    sprintf(user, "%s%s", temp_string, DOMAIN);

    append_string(packet, "secret@here/", 0);
    append_string(packet, user, 0);
    append_string(packet, "|", 0);
    append_string(packet, cookie, 0);
    packet_to_prompt(&prompt, PAM_BPC_SELECT, packet);

    /* get the library to accept the first packet (which should load
       the secret@here agent) */

    retval = pamc_converse(pch, &prompt);
    fprintf(stderr, "server: after conversation\n");
    if (PAM_BP_RCONTROL(prompt) != PAM_BPC_OK) {
	fprintf(stderr, "server: prompt had unexpected control type: %u\n",
		PAM_BP_RCONTROL(prompt));
	exit(1);
    }

    fprintf(stderr, "server: got a prompt back\n");

    prompt_to_packet(prompt, packet);

    temp_string = strtok(packet->buffer, "|");
    if (temp_string == NULL) {
	fprintf(stderr, "server: prompt does not contain anything");
	exit(1);
    }
    seqid = strdup(temp_string);
    if (seqid == NULL) {
	fprintf(stderr, "server: unable to store sequence id\n");
    }

    temp_string = strtok(NULL, "|");
    if (temp_string == NULL) {
	fprintf(stderr, "server: no cookie from agent\n");
	exit(1);
    }
    a_cookie = strdup(temp_string);
    if (a_cookie == NULL) {
	fprintf(stderr, "server: no memory to store agent cookie\n");
	exit(1);
    }

    fprintf(stderr, "server: agent responded with {%s|%s}\n", seqid, a_cookie);
    secret = identify_secret(user);
    fprintf(stderr, "server: secret=%s\n", secret);

    /* now, we construct the response */
    packet->at = 0;
    append_string(packet, a_cookie, 0);
    append_string(packet, "|", 0);
    append_string(packet, cookie, 0);
    append_string(packet, "|", 0);
    append_string(packet, secret, 0);

    fprintf(stderr, "server: get digest of %s\n", packet->buffer);

    digest = create_digest(packet->at, packet->buffer);

    fprintf(stderr, "server: secret=%s, digest=%s\n", secret, digest);

    packet->at = 0;
    append_string(packet, seqid, 0);
    append_string(packet, "|", 0);
    append_string(packet, digest, 0);
    packet_to_prompt(&prompt, PAM_BPC_OK, packet);

    retval = pamc_converse(pch, &prompt);
    fprintf(stderr, "server: after 2nd conversation\n");
    if (PAM_BP_RCONTROL(prompt) != PAM_BPC_DONE) {
	fprintf(stderr, "server: 2nd prompt had unexpected control type: %u\n",
		PAM_BP_RCONTROL(prompt));
	exit(1);
    }

    prompt_to_packet(prompt, packet);
    PAM_BP_RENEW(&prompt, 0, 0);

    temp_string = strtok(packet->buffer, "|");
    if (temp_string == NULL) {
	fprintf(stderr, "no digest from agent\n");
	exit(1);
    }
    temp_string = strdup(temp_string);

    packet->at = 0;
    append_string(packet, secret, 0);
    append_string(packet, "|", 0);
    append_string(packet, cookie, 0);
    append_string(packet, "|", 0);
    append_string(packet, a_cookie, 0);

    fprintf(stderr, "server: get digest of %s\n", packet->buffer);

    digest = create_digest(packet->at, packet->buffer);

    fprintf(stderr, "server: digest=%s\n", digest);

    if (strcmp(digest, temp_string)) {
	fprintf(stderr, "server: agent doesn't know the secret\n");
	fprintf(stderr, "server: agent says:  [%s]\n"
	                "server: server says: [%s]\n", temp_string, digest);
	exit(1);
    } else {
	fprintf(stderr, "server: agent seems to know the secret\n");

	packet->at = 0;
	append_string(packet, cookie, 0);
	append_string(packet, "|", 0);
	append_string(packet, secret, 0);
	append_string(packet, "|", 0);
	append_string(packet, a_cookie, 0);

	digest = create_digest(packet->at, packet->buffer);

	fprintf(stderr, "server: putenv(\"AUTH_SESSION_TICKET=%s\")\n",
		digest);
    }

    
    retval = pamc_end(&pch);

    fprintf(stderr, "server: agent(s) were %shappy to terminate\n",
	    retval == PAM_BPC_TRUE ? "":"un");

    exit(!retval);
}
