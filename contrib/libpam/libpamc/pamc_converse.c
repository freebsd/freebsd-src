/*
 * $Id: pamc_converse.c,v 1.2 2001/01/20 22:29:47 agmorgan Exp $
 *
 * Copyright (c) Andrew G. Morgan <morgan@ftp.kernel.org>
 *
 * pamc_converse
 */

#include "libpamc.h"

/*
 * select agent
 */

static int __pamc_select_agent(pamc_handle_t pch, char *agent_id)
{
    pamc_agent_t *agent;

    for (agent = pch->chain; agent; agent = agent->next) {
	if (!strcmp(agent->id, agent_id)) {
	    pch->current = agent;
	    return PAM_BPC_TRUE;
	}
    }

    D(("failed to locate agent"));
    pch->current = NULL;
    return PAM_BPC_FALSE;
}

/*
 * pass a binary prompt to the active agent and wait for a reply prompt
 */

int pamc_converse(pamc_handle_t pch, pamc_bp_t *prompt_p)
{
    __u32 size, offset=0;
    __u8 control, raw[PAM_BP_MIN_SIZE];

    D(("called"));

    if (pch == NULL) {
	D(("null pch"));
	goto pamc_converse_failure;
    }

    if (prompt_p == NULL) {
	D(("null prompt_p"));
	goto pamc_converse_failure;
    }

    if (*prompt_p == NULL) {
	D(("null *prompt_p"));
	goto pamc_converse_failure;
    }

    /* from here on, failures are interoperability problems.. */

    size = PAM_BP_SIZE(*prompt_p);
    if (size < PAM_BP_MIN_SIZE) {
	D(("problem with size being too short (%u)", size));
	goto pamc_unknown_prompt;
    }

    if (PAM_BPC_FOR_CLIENT(*prompt_p) != PAM_BPC_TRUE) {
	D(("*prompt_p is not legal for the client to use"));
	goto pamc_unknown_prompt;
    }
    
    /* do we need to select the agent? */
    if ((*prompt_p)->control == PAM_BPC_SELECT) {
	char *rawh;
	int i, retval;

	D(("selecting a specified agent"));

	rawh = (char *) *prompt_p;
	for (i = PAM_BP_MIN_SIZE; i<size; ++i) {
	    if (rawh[i] == '/') {
		break;
	    }
	}

	if ( (i >= size)
	     || !__pamc_valid_agent_id(i-PAM_BP_MIN_SIZE,
				       rawh + PAM_BP_MIN_SIZE) ) {
	    goto pamc_unknown_prompt;
	}

	rawh[i] = '\0';
	retval = pamc_load(pch, PAM_BP_MIN_SIZE + rawh);
	if (retval == PAM_BPC_TRUE) {
	    retval = __pamc_select_agent(pch, PAM_BP_MIN_SIZE + rawh);
	}
	rawh[i] = '/';

	if (retval != PAM_BPC_TRUE) {
	    goto pamc_unknown_prompt;
	}

	D(("agent is loaded"));
    }

    if (pch->current == NULL) {
	D(("unable to address agent"));
	goto pamc_unknown_prompt;
    }

    /* pump all of the prompt into the agent */
    do {
	int rval = write(pch->current->writer,
			 offset + (const __u8 *) (*prompt_p),
			 size - offset);
	if (rval == -1) {
	    switch (errno) {
	    case EINTR:
		break;
	    default:
		D(("problem writing to agent: %s", strerror(errno)));
		goto pamc_unknown_prompt;
	    }
	} else {
	    offset += rval;
	}
    } while (offset < size);

    D(("whole prompt sent to agent"));

    /* read size and control for response prompt */

    offset = 0;
    memset(raw, 0, sizeof(raw));
    do {
	int rval;

	rval = read(pch->current->reader, raw + offset,
		    PAM_BP_MIN_SIZE - offset);

	if (rval == -1) {
	    switch (errno) {
	    case EINTR:
		break;
	    default:
		D(("problem reading from agent: %s", strerror(errno)));
		goto pamc_unknown_prompt;
	    }
	} else if (rval) {
	    offset += rval;
	} else {
	    D(("agent has closed its output pipe - nothing more to read"));
	    goto pamc_converse_failure;
	}
    } while (offset < PAM_BP_MIN_SIZE);

    /* construct the whole reply prompt */

    size = PAM_BP_SIZE(raw);
    control = PAM_BP_RCONTROL(raw);
    memset(raw, 0, sizeof(raw));

    D(("agent replied with prompt of size %d and control %u",
       size, control));

    PAM_BP_RENEW(prompt_p, control, size - PAM_BP_MIN_SIZE);
    if (*prompt_p == NULL) {
	D(("problem making a new prompt for reply"));
	goto pamc_unknown_prompt;
    }

    /* read the rest of the reply prompt -- note offset has the correct
       value from the previous loop */

    while (offset < size) {
	int rval = read(pch->current->reader, offset + (__u8 *) *prompt_p,
			size-offset);

	if (rval == -1) {
	    switch (errno) {
	    case EINTR:
		break;
	    default:
		D(("problem reading from agent: %s", strerror(errno)));
		goto pamc_unknown_prompt;
	    }
	} else if (rval) {
	    offset += rval;
	} else {
	    D(("problem reading prompt (%d) with %d to go",
	       size, size-offset));
	    goto pamc_converse_failure;
	}
    }

    D(("returning success"));

    return PAM_BPC_TRUE;

pamc_converse_failure:

    D(("conversation failure"));
    PAM_BP_RENEW(prompt_p, 0, 0);
    return PAM_BPC_FALSE;

pamc_unknown_prompt:

    /* the server is trying something that the client does not support */
    D(("unknown prompt"));
    PAM_BP_RENEW(prompt_p, PAM_BPC_FAIL, 0);
    return PAM_BPC_TRUE;
}

