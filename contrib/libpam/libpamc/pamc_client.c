/*
 * $Id: pamc_client.c,v 1.1.1.1 2000/06/20 22:11:25 agmorgan Exp $
 *
 * Copyright (c) Andrew G. Morgan <morgan@ftp.kernel.org>
 *
 * pamc_start and pamc_end
 */

#include "libpamc.h"

/*
 * liberate path list
 */

static void __pamc_delete_path_list(pamc_handle_t pch)
{
    int i;

    for (i=0; pch->agent_paths[i]; ++i) {
	free(pch->agent_paths[i]);
	pch->agent_paths[i] = NULL;
    }

    free(pch->agent_paths);
    pch->agent_paths = NULL;
}

/*
 * open the pamc library
 */

pamc_handle_t pamc_start(void)
{
    int i, count, last, this;
    const char *default_path;
    pamc_handle_t pch;

    pch = calloc(1, sizeof(struct pamc_handle_s));
    if (pch == NULL) {
	D(("no memory for *pch"));
	return NULL;
    }

    pch->highest_fd_to_close = _PAMC_DEFAULT_TOP_FD;

    default_path = getenv("PAMC_AGENT_PATH");
    if (default_path == NULL) {
	default_path = PAMC_SYSTEM_AGENT_PATH;
    }

    /* number of individual paths */
    for (count=1, i=0; default_path[i]; ++i) {
	if (default_path[i] == PAMC_SYSTEM_AGENT_SEPARATOR) {
	    ++count;
	}
    }

    pch->agent_paths = calloc(count+1, sizeof(char *));
    if (pch->agent_paths == NULL) {
	D(("no memory for path list"));
	goto drop_pch;
    }

    this = last = i = 0;
    while ( default_path[i] || (i != last) ) {
	if ( default_path[i] == PAMC_SYSTEM_AGENT_SEPARATOR
	     || !default_path[i] ) {
	    int length;
	    
	    pch->agent_paths[this] = malloc(length = 1+i-last);

	    if (pch->agent_paths[this] == NULL) {
		D(("no memory for next path"));
		goto drop_list;
	    }

	    memcpy(pch->agent_paths[this], default_path + last, i-last);
	    pch->agent_paths[this][i-last] = '\0';
	    if (length > pch->max_path) {
		pch->max_path = length;
	    }

	    if (++this == count) {
		break;
	    }

	    last = ++i;
	} else {
	    ++i;
	}
    }

    return pch;

drop_list:
    __pamc_delete_path_list(pch);

drop_pch:
    free(pch);

    return NULL;
}

/*
 * shutdown each of the loaded agents and 
 */

static int __pamc_shutdown_agents(pamc_handle_t pch)
{
    int retval = PAM_BPC_TRUE;

    D(("called"));
    
    while (pch->chain) {
	pid_t pid;
	int status;
	pamc_agent_t *this;

	this = pch->chain;
	D(("cleaning up agent %p", this));
	pch->chain = pch->chain->next;
	this->next = NULL;
	D(("cleaning up agent: %s", this->id));

	/* close off contact with agent and wait for it to shutdown */

	close(this->writer);
	this->writer = -1;
	close(this->reader);
	this->reader = -1;

	pid = waitpid(this->pid, &status, 0);
	if (pid == this->pid) {

	    D(("is exit:%d, exit val:%d",
	       WIFEXITED(status), WEXITSTATUS(status)));

	    if (!(WIFEXITED(status) && (WEXITSTATUS(status) == 0))) {
		retval = PAM_BPC_FALSE;
	    }
	} else {
	    D(("problem shutting down agent (%s): pid(%d) != waitpid(%d)!?",
	       this->id, this->pid, pid));
	    retval = PAM_BPC_FALSE;
	}
	pid = this->pid = 0;

	memset(this->id, 0, this->id_length);
	free(this->id);
	this->id = NULL;
	this->id_length = 0;

	free(this);
	this = NULL;
    }

    return retval;
}

/*
 * close the pamc library
 */

int pamc_end(pamc_handle_t *pch_p)
{
    int retval;

    if (pch_p == NULL) {
	D(("called with no pch_p"));
	return PAM_BPC_FALSE;
    }

    if (*pch_p == NULL) {
	D(("called with no *pch_p"));
	return PAM_BPC_FALSE;
    }

    D(("removing path_list"));
    __pamc_delete_path_list(*pch_p);

    D(("shutting down agents"));
    retval = __pamc_shutdown_agents(*pch_p);

    D(("freeing *pch_p"));
    free(*pch_p);
    *pch_p = NULL;

    return retval;
}
