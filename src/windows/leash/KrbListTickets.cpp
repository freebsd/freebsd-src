#include "stdafx.h"
#include "lglobals.h"
#include "krb5.h"

static void
FreeTicketList(TicketList** ticketList)
{
    TicketList* tempList = *ticketList, *killList;

    while (tempList) {
        killList = tempList;
        tempList = tempList->next;
        free(killList->service);
        if (killList->encTypes)
            free(killList->encTypes);
        free(killList);
    }

    *ticketList = NULL;
}

void
LeashKRB5FreeTicketInfo(TICKETINFO *ticketinfo)
{
    if (ticketinfo->principal) {
        free(ticketinfo->principal);
        ticketinfo->principal = NULL;
    }
    if (ticketinfo->ccache_name) {
        pkrb5_free_string(NULL, ticketinfo->ccache_name);
        ticketinfo->ccache_name = NULL;
    }
    if (ticketinfo->ticket_list)
        FreeTicketList(&ticketinfo->ticket_list);
}

void
LeashKRB5FreeTickets(TICKETINFO **ticketinfolist)
{
    TICKETINFO *ticketinfo = *ticketinfolist, *next;
    while (ticketinfo) {
        next = ticketinfo->next;
        LeashKRB5FreeTicketInfo(ticketinfo);
        free(ticketinfo);
        ticketinfo = next;
    }
    *ticketinfolist = NULL;
}

/*
 * LeashKRB5Error()
 */
int
LeashKRB5Error(krb5_error_code rc, LPCSTR FailedFunctionName)
{
#ifdef USE_MESSAGE_BOX
    char message[256];
    const char *errText;

    errText = perror_message(rc);
    _snprintf(message, sizeof(message),
              "%s\n(Kerberos error %ld)\n\n%s failed",
              errText,
              rc,
              FailedFunctionName);
    message[sizeof(message)-1] = 0;

    MessageBox(NULL, message, "Kerberos Five", MB_OK | MB_ICONERROR |
               MB_TASKMODAL |
               MB_SETFOREGROUND);
#endif /* USE_MESSAGE_BOX */
    return rc;
}


static char *
etype_string(krb5_enctype enctype)
{
    static char buf[100];

    krb5_error_code retval;

    if ((retval = pkrb5_enctype_to_name(enctype, FALSE, buf, sizeof(buf)))) {
        /* XXX if there's an error != EINVAL, I should probably report it */
        sprintf_s(buf, "etype %d", enctype);
    }

    return buf;
}


static void
CredToTicketInfo(krb5_creds KRBv5Credentials, TICKETINFO *ticketinfo)
{
    ticketinfo->issued = KRBv5Credentials.times.starttime;
    ticketinfo->valid_until = KRBv5Credentials.times.endtime;
    ticketinfo->renew_until = KRBv5Credentials.ticket_flags & TKT_FLG_RENEWABLE ?
        KRBv5Credentials.times.renew_till : 0;
    _tzset();
    if ( ticketinfo->valid_until - time(0) <= 0L )
        ticketinfo->btickets = EXPD_TICKETS;
    else
        ticketinfo->btickets = GOOD_TICKETS;
}

static int
CredToTicketList(krb5_context ctx, krb5_creds KRBv5Credentials,
                 char *PrincipalName, TicketList ***ticketListTail)
{
    krb5_error_code code = 0;
    krb5_ticket *tkt=NULL;
    char *sServerName = NULL;
    char Buffer[256];
    char *functionName = NULL;
    TicketList *list = NULL;

    functionName = "krb5_unparse_name()";
    code = (*pkrb5_unparse_name)(ctx, KRBv5Credentials.server, &sServerName);
    if (code)
        goto cleanup;

    if (!KRBv5Credentials.times.starttime)
        KRBv5Credentials.times.starttime = KRBv5Credentials.times.authtime;

    memset(Buffer, '\0', sizeof(Buffer));

    // @fixme: calloc for ptr init
    list = (TicketList *)calloc(1, sizeof(TicketList));
    if (!list) {
        code = ENOMEM;
        functionName = "calloc()";
        goto cleanup;
    }
    list->service = strdup(sServerName);
    if (!list->service) {
        code = ENOMEM;
        functionName = "calloc()";
        goto cleanup;
    }
    list->issued = KRBv5Credentials.times.starttime;
    list->valid_until = KRBv5Credentials.times.endtime;
    if (KRBv5Credentials.ticket_flags & TKT_FLG_RENEWABLE)
        list->renew_until = KRBv5Credentials.times.renew_till;
    else
        list->renew_until = 0;

    if (!pkrb5_decode_ticket(&KRBv5Credentials.ticket, &tkt)) {
        wsprintf(Buffer, "Session Key: %s  Ticket: %s",
            etype_string(KRBv5Credentials.keyblock.enctype),
            etype_string(tkt->enc_part.enctype));
        pkrb5_free_ticket(ctx, tkt);
        tkt = NULL;
    } else {
        wsprintf(Buffer, "Session Key: %s",
            etype_string(KRBv5Credentials.keyblock.enctype));
    }

    list->encTypes = (char *)calloc(1, strlen(Buffer)+1);
    if (!list->encTypes) {
        functionName = "calloc()";
        code = ENOMEM;
        goto cleanup;
    }
    strcpy(list->encTypes, Buffer);

    list->flags = KRBv5Credentials.ticket_flags;
cleanup:
    if (code) {
        LeashKRB5Error(code, functionName);
        if (list)
            FreeTicketList(&list);
    } else {
        **ticketListTail = list;
        *ticketListTail = &list->next;
    }

    if (sServerName != NULL)
        (*pkrb5_free_unparsed_name)(ctx, sServerName);

    return code;
}

// return 0 if ticketinfo was successfully appended to list, 1 otherwise
int
do_ccache(krb5_context ctx,
          krb5_ccache cache,
          TICKETINFO **ticketInfoTail)
{
    krb5_cc_cursor cur;
    krb5_creds creds;
    krb5_principal princ = NULL;
    krb5_flags flags;
    krb5_error_code code;
    char *defname = NULL;
    char *functionName = NULL;
    TicketList **ticketListTail;
    TICKETINFO *ticketinfo = NULL;
    int retval = 1;

    // Don't need the actual ticket.
    flags = KRB5_TC_NOTICKET;
    code = pkrb5_cc_set_flags(ctx, cache, flags);
    if (code) {
        if (code == KRB5_FCC_NOFILE || code == KRB5_CC_NOTFOUND) {
            // Normal behavior; skip cache but suppress error message box
            code = 0;
        } else {
            functionName = "krb5_cc_set_flags";
        }
        goto cleanup;
    }
    code = pkrb5_cc_get_principal(ctx, cache, &princ);
    if (code) {
        // Normal behavior; skip cache but suppress error message box
        code = 0;
        goto cleanup;
    }
    code = pkrb5_unparse_name(ctx, princ, &defname);
    if (code) {
        functionName = "krb5_unparse_name";
        goto cleanup;
    }
    code = pkrb5_cc_start_seq_get(ctx, cache, &cur);
    if (code) {
        functionName = "krb5_cc_start_seq_get";
        goto cleanup;
    }
    if (*ticketInfoTail)
        ticketinfo = *ticketInfoTail;
    else
        // @fixme: calloc to init pointers
        ticketinfo = (TICKETINFO *)calloc(1, sizeof(TICKETINFO));

    if (ticketinfo == NULL) {
        functionName = "calloc";
        code = ENOMEM;
        goto cleanup;
    }
    ticketinfo->next = NULL;
    ticketinfo->ticket_list = NULL;
    ticketinfo->principal = strdup(defname);
    if (ticketinfo->principal == NULL) {
        functionName = "strdup";
        code = ENOMEM;
        goto cleanup;
    }
    code = pkrb5_cc_get_full_name(ctx, cache, &ticketinfo->ccache_name);
    if (code) {
        functionName = "krb5_cc_get_full_name";
        goto cleanup;
    }
    *ticketInfoTail = ticketinfo;
    ticketListTail = &ticketinfo->ticket_list;
    while (!(code = pkrb5_cc_next_cred(ctx, cache, &cur, &creds))) {
        if (!pkrb5_is_config_principal(ctx, creds.server)) {
            CredToTicketList(ctx, creds, defname, &ticketListTail);
            CredToTicketInfo(creds, ticketinfo);
        }
        pkrb5_free_cred_contents(ctx, &creds);
    }
    if (code == KRB5_CC_END) {
        code = pkrb5_cc_end_seq_get(ctx, cache, &cur);
        if (code) {
            functionName = "krb5_cc_end_seq_get";
            goto cleanup;
        }
        flags = 0;
        code = pkrb5_cc_set_flags(ctx, cache, flags);
        if (code) {
            functionName = "krb5_cc_set_flags";
            goto cleanup;
        }
    } else {
        functionName = "krb5_cc_next_cred";
        goto cleanup;
    }
cleanup:
    if (code)
        LeashKRB5Error(code, functionName);
    if (ticketinfo) {
        if (ticketinfo == *ticketInfoTail)
            retval = 0;
        else
            LeashKRB5FreeTickets(&ticketinfo);
    }
    if (defname)
        pkrb5_free_unparsed_name(ctx, defname);
    if (princ)
        pkrb5_free_principal(ctx, princ);
    return retval;
}


//
// Returns 0 for success, 1 for failure
//
int
do_all_ccaches(krb5_context ctx, TICKETINFO **ticketinfotail)
{
    krb5_error_code code;
    krb5_ccache cache;
    krb5_cccol_cursor cursor;
    int retval = 1;
    char *functionName = NULL;

    code = pkrb5_cccol_cursor_new(ctx, &cursor);
    if (code) {
        functionName = "krb5_cccol_cursor_new";
        goto cleanup;
    }
    retval = 0;
    while (!(code = pkrb5_cccol_cursor_next(ctx, cursor, &cache)) &&
           cache != NULL) {
        // Note that ticketinfotail will be updated here to point to the tail
        // of the list but the caller of this function will remain with a
        // pointer to the head.
        if (do_ccache(ctx, cache, ticketinfotail) == 0)
            ticketinfotail = &((*ticketinfotail)->next);
        pkrb5_cc_close(ctx, cache);
    }
    if (code)
         functionName = "krb5_cccol_cursor_next";
    pkrb5_cccol_cursor_free(ctx, &cursor);
cleanup:
    if (code)
        LeashKRB5Error(code, functionName);
    return retval;
}

void
LeashKRB5ListDefaultTickets(TICKETINFO *ticketinfo)
{
    krb5_error_code	code;
    krb5_context ctx = 0;
    krb5_ccache cache = 0;
    char *functionName = NULL;

    ticketinfo->btickets = NO_TICKETS;
    ticketinfo->principal = NULL;
    ticketinfo->ccache_name = NULL;
    ticketinfo->next = NULL;
    ticketinfo->ticket_list = NULL;
    ticketinfo->renew_until = 0;
    ticketinfo->valid_until = 0;
    ticketinfo->issued = 0;

    code = pkrb5_init_context(&ctx);
    if (code) {
        functionName = "krb5_init_context";
        goto cleanup;
    }

    code = pkrb5_cc_default(ctx, &cache);
    if (code) {
        functionName = "krb5_cc_default";
        goto cleanup;
    }
    if (cache != NULL)
        do_ccache(ctx, cache, &ticketinfo);
cleanup:
    if (code)
        LeashKRB5Error(code, functionName);
    if (cache)
        pkrb5_cc_close(ctx, cache);
    if (ctx)
        pkrb5_free_context(ctx);
}


/*
 * LeashKRB5ListAllTickets()
 */

void
LeashKRB5ListAllTickets(TICKETINFO **ticketinfo)
{
    krb5_error_code	code;
    krb5_context ctx = 0;
    char *functionName = NULL;

    code = pkrb5_init_context(&ctx);
    if (code) {
        functionName = "krb5_init_context";
        goto cleanup;
    }

    do_all_ccaches(ctx, ticketinfo);
cleanup:
    if (code)
        LeashKRB5Error(code, functionName);
    if (ctx)
        pkrb5_free_context(ctx);
}
