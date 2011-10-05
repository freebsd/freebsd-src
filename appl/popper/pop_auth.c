/*
 * Copyright (c) 2004 Kungliga Tekniska Högskolan
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

#include <popper.h>
#ifdef SASL
#include <base64.h>
#include <pop_auth.h>
RCSID("$Id$");

/*
 *  auth: RFC1734
 */

static char *
getline(POP *p)
{
    char *buf = NULL;
    size_t size = 1024;
    buf = malloc(size);
    if(buf == NULL)
	return NULL;
    *buf = '\0';
    while(fgets(buf + strlen(buf), size - strlen(buf), p->input) != NULL) {
	char *p;
	if((p = strchr(buf, '\n')) != NULL) {
	    while(p > buf && p[-1] == '\r')
		p--;
	    *p = '\0';
	    return buf;
	}
	/* just assume we ran out of buffer space, we'll catch eof
           next round */
	size += 1024;
	p = realloc(buf, size);
	if(p == NULL)
	    break;
	buf = p;
    }
    free(buf);
    return NULL;
}

static char auth_msg[128];
void
pop_auth_set_error(const char *message)
{
    strlcpy(auth_msg, message, sizeof(auth_msg));
}

static struct auth_mech *methods[] = {
#ifdef KRB5
    &gssapi_mech,
#endif
    NULL
};

static int
auth_execute(POP *p, struct auth_mech *m, void *state, const char *line)
{
    void *input, *output;
    size_t input_length, output_length;
    int status;

    if(line == NULL) {
	input = NULL;
	input_length = 0;
    } else {
	input = strdup(line);
	if(input == NULL) {
	    pop_auth_set_error("out of memory");
	    return POP_AUTH_FAILURE;
	}
	input_length = base64_decode(line, input);
	if(input_length == (size_t)-1) {
	    pop_auth_set_error("base64 decode error");
	    return POP_AUTH_FAILURE;
	}
    }
    output = NULL; output_length = 0;
    status = (*m->loop)(p, state, input, input_length, &output, &output_length);
    if(output_length > 0) {
	char *s;
	base64_encode(output, output_length, &s);
	fprintf(p->output, "+ %s\r\n", s);
	fflush(p->output);
	free(output);
	free(s);
    }
    return status;
}

static int
auth_loop(POP *p, struct auth_mech *m)
{
    int status;
    void *state = NULL;
    char *line;

    status = (*m->init)(p, &state);

    status = auth_execute(p, m, state, p->pop_parm[2]);

    while(status == POP_AUTH_CONTINUE) {
	line = getline(p);
	if(line == NULL) {
	    (*m->cleanup)(p, state);
	    return pop_msg(p, POP_FAILURE, "error reading data");
	}
	if(strcmp(line, "*") == 0) {
	    (*m->cleanup)(p, state);
	    return pop_msg(p, POP_FAILURE, "terminated by client");
	}
	status = auth_execute(p, m, state, line);
	free(line);
    }


    (*m->cleanup)(p, state);
    if(status == POP_AUTH_FAILURE)
	return pop_msg(p, POP_FAILURE, "%s", auth_msg);

    status = login_user(p);
    if(status != POP_SUCCESS)
	return status;
    return pop_msg(p, POP_SUCCESS, "authentication complete");
}

int
pop_auth (POP *p)
{
    int i;

    for (i = 0; methods[i] != NULL; ++i)
	if (strcasecmp(p->pop_parm[1], methods[i]->name) == 0)
	    return auth_loop(p, methods[i]);
    return pop_msg(p, POP_FAILURE,
		   "Authentication method %s unknown", p->pop_parm[1]);
}

void
pop_capa_sasl(POP *p)
{
    int i;

    if(methods[0] == NULL)
	return;

    fprintf(p->output, "SASL");
    for (i = 0; methods[i] != NULL; ++i)
	fprintf(p->output, " %s", methods[i]->name);
    fprintf(p->output, "\r\n");
}
#endif
