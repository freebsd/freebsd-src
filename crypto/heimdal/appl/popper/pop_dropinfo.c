/*
 * Copyright (c) 1989 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#include <popper.h>
RCSID("$Id: pop_dropinfo.c,v 1.24 1999/09/16 20:38:49 assar Exp $");

#if defined(UIDL) || defined(XOVER)

/*
 * Copy the string found after after : into a malloced buffer. Stop
 * copying at end of string or end of line. End of line delimiter is
 * not part of the resulting copy.
 */
static
char *
find_value_after_colon(char *p)
{
  char *t, *tmp;

  for (; *p != 0 && *p != ':'; p++) /* Find : */
    ;

  if (*p == 0)
    goto error;

  p++;				/* Skip over : */

  for(; *p == ' ' || *p == '\t'; p++) /* Remove white space */
    ;

  for (t = p; *t != 0 && *t != '\n' && *t != '\r'; t++)	/* Find end of str */
    ;

  tmp = t = malloc(t - p + 1);
  if (tmp == 0)
    goto error;

  for (; *p != 0 && *p != '\n' && *p != '\r'; p++, t++)	/* Copy characters */
    *t = *p;
  *t = 0;			/* Terminate string */
  return tmp;

error:
  return "ErrorUIDL";
}
#endif

void
parse_header(MsgInfoList *mp, char *buffer)
{
#if defined(UIDL) || defined(XOVER)
    if (strncasecmp("Message-Id:",buffer, 11) == 0) {
	if (mp->msg_id == NULL)
	    mp->msg_id = find_value_after_colon(buffer);
    } 
#ifdef UIDL
    else if (strncasecmp(buffer, "X-UIDL:", 7) == 0) {
	/* Courtesy to Qualcomm, there really is no such 
	   thing as X-UIDL */
	mp->msg_id = find_value_after_colon(buffer);
    }
#endif
#endif
#ifdef XOVER
    else if (strncasecmp("Subject:", buffer, 8) == 0) {
	if(mp->subject == NULL){
	    char *p;
	    mp->subject = find_value_after_colon(buffer);
	    for(p = mp->subject; *p; p++)
		if(*p == '\t') *p = ' ';
	}
    }
    else if (strncasecmp("From:", buffer, 5) == 0) {
	if(mp->from == NULL){
	    char *p;
	    mp->from = find_value_after_colon(buffer);
	    for(p = mp->from; *p; p++)
		if(*p == '\t') *p = ' ';
	}
    }
    else if (strncasecmp("Date:", buffer, 5) == 0) {
	if(mp->date == NULL){
	    char *p;
	    mp->date = find_value_after_colon(buffer);
	    for(p = mp->date; *p; p++)
		if(*p == '\t') *p = ' ';
	}
    }
#endif
}

int
add_missing_headers(POP *p, MsgInfoList *mp)
{
#if defined(UIDL) || defined(XOVER)
    if (mp->msg_id == NULL) {
	asprintf(&mp->msg_id, "no-message-id-%d", mp->number);
	if(mp->msg_id == NULL) {
	    fclose (p->drop);
	    p->msg_count = 0;
	    return pop_msg (p,POP_FAILURE,
			    "Can't build message list for '%s': Out of memory",
                            p->user);
	}
    }
#endif	    
#ifdef XOVER
    if (mp->subject == NULL)
	mp->subject = "<none>";
    if (mp->from == NULL)
	mp->from = "<unknown>";
    if (mp->date == NULL)
	mp->date = "<unknown>";
#endif
    return POP_SUCCESS;
}

/* 
 *  dropinfo:   Extract information about the POP maildrop and store 
 *  it for use by the other POP routines.
 */

int
pop_dropinfo(POP *p)
{
    char                    buffer[BUFSIZ];         /*  Read buffer */
    MsgInfoList         *   mp;                     /*  Pointer to message 
                                                        info list */
    int			    msg_num;                /*  Current message 
                                                        counter */
    int                     nchar;                  /*  Bytes written/read */
    int blank_line = 1; /* previous line was blank */
    int in_header = 0; /* if we are in a header block */
    
    /*  Initialize maildrop status variables in the POP parameter block */
    p->msg_count = 0;
    p->msgs_deleted = 0;
    p->last_msg = 0;
    p->bytes_deleted = 0;
    p->drop_size = 0;

    /*  Allocate memory for message information structures */
    p->msg_count = ALLOC_MSGS;
    p->mlp = (MsgInfoList *)calloc((unsigned)p->msg_count,sizeof(MsgInfoList));
    if (p->mlp == NULL){
        fclose (p->drop);
        p->msg_count = 0;
        return pop_msg (p,POP_FAILURE,
            "Can't build message list for '%s': Out of memory", p->user);
    }

    rewind (p->drop);

    /*  Scan the file, loading the message information list with 
        information about each message */

    for (msg_num = p->drop_size = 0, mp = p->mlp - 1;
             fgets(buffer,MAXMSGLINELEN,p->drop);) {

        nchar  = strlen(buffer);

        if (blank_line && strncmp(buffer,"From ",5) == 0) {
	    in_header = 1;
            if (++msg_num > p->msg_count) {
                p->mlp=(MsgInfoList *) realloc(p->mlp,
                    (p->msg_count+=ALLOC_MSGS)*sizeof(MsgInfoList));
                if (p->mlp == NULL){
                    fclose (p->drop);
                    p->msg_count = 0;
                    return pop_msg (p,POP_FAILURE,
                        "Can't build message list for '%s': Out of memory",
                            p->user);
                }
                mp = p->mlp + msg_num - 2;
            }
            ++mp;
            mp->number = msg_num;
            mp->length = 0;
            mp->lines = 0;
            mp->offset = ftell(p->drop) - nchar;
            mp->flags = 0;
#if defined(UIDL) || defined(XOVER)
	    mp->msg_id = 0;
#endif
#ifdef XOVER
	    mp->subject = 0;
	    mp->from = 0;
	    mp->date = 0;
#endif
#ifdef DEBUG
            if(p->debug)
                pop_log(p, POP_DEBUG,
			"Msg %d at offset %ld being added to list",
			mp->number, mp->offset);
#endif /* DEBUG */
        } else if(in_header)
	    parse_header(mp, buffer);
	blank_line = (strncmp(buffer, "\n", nchar) == 0);
	if(blank_line) {
	    int e;
	    in_header = 0;
	    e = add_missing_headers(p, mp);
	    if(e != POP_SUCCESS)
		return e;
	}
        mp->length += nchar;
        p->drop_size += nchar;
        mp->lines++;
    }
    p->msg_count = msg_num;

#ifdef DEBUG
    if(p->debug && msg_num > 0) {
        int i;
        for (i = 0, mp = p->mlp; i < p->msg_count; i++, mp++)
#ifdef UIDL
	    pop_log(p,POP_DEBUG,
		    "Msg %d at offset %ld is %ld octets long and has %u lines and id %s.",
                    mp->number,mp->offset,mp->length,mp->lines, mp->msg_id);
#else	
            pop_log(p,POP_DEBUG,
                "Msg %d at offset %d is %d octets long and has %u lines.",
                    mp->number,mp->offset,mp->length,mp->lines);
#endif
    }
#endif /* DEBUG */

    return(POP_SUCCESS);
}
