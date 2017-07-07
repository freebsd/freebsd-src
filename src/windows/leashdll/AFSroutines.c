//* Module name: AFSroutines.c

#include <windows.h>
#include <winsock2.h>
#include <stdio.h>
#include <time.h>

/* Private Include files */
#include <conf.h>
#include <leasherr.h>
#include "leashdll.h"
#include <leashwin.h>

#ifndef NO_AFS
#include <afs/stds.h>
#include <afs/auth.h>
#include <afs/krb.h>
#include <afs/cellconfig.h>
#endif
#include "leash-int.h"

#define MAXCELLCHARS   64
#define MAXHOSTCHARS   64
#define MAXHOSTSPERCELL 8
#define TRANSARCAFSDAEMON "TransarcAFSDaemon"
typedef struct {
    char name[MAXCELLCHARS];
    short numServers;
    short flags;
    struct sockaddr_in hostAddr[MAXHOSTSPERCELL];
    char hostName[MAXHOSTSPERCELL][MAXHOSTCHARS];
    char *linkedCell;
} afsconf_cell;

DWORD   AfsOnLine = 1;
extern  DWORD AfsAvailable;

int not_an_API_LeashAFSGetToken(TICKETINFO * ticketinfo, TicketList** ticketList, char * kprinc);
DWORD GetServiceStatus(LPSTR lpszMachineName, LPSTR lpszServiceName, DWORD *lpdwCurrentState);
BOOL SetAfsStatus(DWORD AfsStatus);
BOOL GetAfsStatus(DWORD *AfsStatus);
void Leash_afs_error(LONG rc, LPCSTR FailedFunctionName);

static char *afs_realm_of_cell(afsconf_cell *);
static long get_cellconfig_callback(void *, struct sockaddr_in *, char *);
static int get_cellconfig(char *, afsconf_cell *, char *);

/**************************************/
/* LeashAFSdestroyToken():            */
/**************************************/
int
Leash_afs_unlog(
    void
    )
{
#ifdef NO_AFS
    return(0);
#else
    long	rc;
    char    HostName[64];
    DWORD   CurrentState;

    if (!AfsAvailable || GetAfsStatus(&AfsOnLine) && !AfsOnLine)
        return(0);

    CurrentState = 0;
    memset(HostName, '\0', sizeof(HostName));
    gethostname(HostName, sizeof(HostName));
    if (GetServiceStatus(HostName, TRANSARCAFSDAEMON, &CurrentState) != NOERROR)
        return(0);
    if (CurrentState != SERVICE_RUNNING)
        return(0);

    rc = ktc_ForgetAllTokens();

    return(0);
#endif
}


int
not_an_API_LeashAFSGetToken(
    TICKETINFO * ticketinfo,
    TicketList** ticketList,
    char * kerberosPrincipal
    )
{
#ifdef NO_AFS
    return(0);
#else
    struct ktc_principal    aserver;
    struct ktc_principal    aclient;
    struct ktc_token        atoken;
    int                     EndMonth;
    int                     EndDay;
    int                     cellNum;
    int                     BreakAtEnd;
    char                    UserName[64];
    char                    CellName[64];
    char                    ServiceName[64];
    char                    InstanceName[64];
    char                    EndTime[16];
    char                    Buffer[256];
    char                    Months[12][4] = {"Jan\0", "Feb\0", "Mar\0", "Apr\0", "May\0", "Jun\0", "Jul\0", "Aug\0", "Sep\0", "Oct\0", "Nov\0", "Dec\0"};
    char                    TokenStatus[16];
    time_t                  CurrentTime;
    struct tm               *newtime;
    DWORD                   CurrentState;
    DWORD                   rc;
    char                    HostName[64];


    TicketList* list = NULL;
    if ( ticketinfo ) {
        ticketinfo->btickets = NO_TICKETS;
        ticketinfo->principal[0] = '\0';
    }
    if ( !kerberosPrincipal )
        kerberosPrincipal = "";

    if (!AfsAvailable || GetAfsStatus(&AfsOnLine) && !AfsOnLine)
        return(0);

    CurrentState = 0;
    memset(HostName, '\0', sizeof(HostName));
    gethostname(HostName, sizeof(HostName));
    if (GetServiceStatus(HostName, TRANSARCAFSDAEMON, &CurrentState) != NOERROR)
        return(0);
    if (CurrentState != SERVICE_RUNNING)
        return(0);

    BreakAtEnd = 0;
    cellNum = 0;
    while (1)
    {
        if (rc = ktc_ListTokens(cellNum, &cellNum, &aserver))
        {
            if (rc != KTC_NOENT)
                return(0);

            if (BreakAtEnd == 1)
                break;
        }
        BreakAtEnd = 1;
        memset(&atoken, '\0', sizeof(atoken));
        if (rc = ktc_GetToken(&aserver, &atoken, sizeof(atoken), &aclient))
        {
            if (rc == KTC_ERROR)
                return(0);

            continue;
        }

        if (!list)
        {
            list = (TicketList*) calloc(1, sizeof(TicketList));
            (*ticketList) = list;
        }
        else
        {
            list->next = (struct TicketList*) calloc(1, sizeof(TicketList));
            list = (TicketList*) list->next;
        }

        CurrentTime = time(NULL);

        newtime = localtime(&atoken.endTime);

        memset(UserName, '\0', sizeof(UserName));
        strcpy(UserName, aclient.name);

        memset(CellName, '\0', sizeof(CellName));
        strcpy(CellName, aclient.cell);

        memset(InstanceName, '\0', sizeof(InstanceName));
        strcpy(InstanceName, aclient.instance);

        memset(ServiceName, '\0', sizeof(ServiceName));
        strcpy(ServiceName, aserver.name);

        memset(TokenStatus, '\0', sizeof(TokenStatus));

        EndDay = newtime->tm_mday;

        EndMonth = newtime->tm_mon + 1;;

        sprintf(EndTime, "%02d:%02d:%02d", newtime->tm_hour, newtime->tm_min, newtime->tm_sec);

        sprintf(Buffer,"                          %s %02d %s      %s%s%s@%s  %s",
                Months[EndMonth - 1], EndDay, EndTime,
                UserName,
                InstanceName[0] ? "." : "",
                InstanceName,
                CellName,
                TokenStatus);

        list->theTicket = (char*) calloc(1, sizeof(Buffer));
        if (!list->theTicket)
        {
#ifdef USE_MESSAGE_BOX
            MessageBox(NULL, "Memory Error", "Error", MB_OK);
#endif /* USE_MESSAGE_BOX */
            return ENOMEM;
        }

        strcpy(list->theTicket, Buffer);
        list->name = strdup(aclient.name);
        list->inst = aclient.instance[0] ? strdup(aclient.instance) : NULL;
        list->realm = strdup(aclient.cell);
        list->encTypes = NULL;
        list->addrCount = 0;
        list->addrList = NULL;

        if ( ticketinfo ) {
            sprintf(Buffer,"%s@%s",UserName,CellName);
            if (!ticketinfo->principal[0] || !stricmp(Buffer,kerberosPrincipal)) {
                strcpy(ticketinfo->principal, Buffer);
                ticketinfo->issue_date = 0;
                ticketinfo->lifetime = atoken.endTime;
                ticketinfo->renew_till = 0;

                _tzset();
                if ( ticketinfo->lifetime - time(0) <= 0L )
                    ticketinfo->btickets = EXPD_TICKETS;
                else
                    ticketinfo->btickets = GOOD_TICKETS;
            }
        }
    }
    return(0);
#endif
}

static char OpenAFSConfigKeyName[] = "SOFTWARE\\OpenAFS\\Client";

static int
use_krb524(void)
{
    HKEY parmKey;
    DWORD code, len;
    DWORD use524 = 0;

    code = RegOpenKeyEx(HKEY_CURRENT_USER, OpenAFSConfigKeyName,
                         0, KEY_QUERY_VALUE, &parmKey);
    if (code == ERROR_SUCCESS) {
        len = sizeof(use524);
        code = RegQueryValueEx(parmKey, "Use524", NULL, NULL,
                                (BYTE *) &use524, &len);
        RegCloseKey(parmKey);
    }
    if (code != ERROR_SUCCESS) {
        code = RegOpenKeyEx(HKEY_LOCAL_MACHINE, OpenAFSConfigKeyName,
                             0, KEY_QUERY_VALUE, &parmKey);
        if (code == ERROR_SUCCESS) {
            len = sizeof(use524);
            code = RegQueryValueEx(parmKey, "Use524", NULL, NULL,
                                    (BYTE *) &use524, &len);
            RegCloseKey (parmKey);
        }
    }
    return use524;
}



int
Leash_afs_klog(
    char *service,
    char *cell,
    char *realm,
    int LifeTime
    )
{
/////#ifdef NO_AFS
#if defined(NO_AFS) || defined(NO_KRB4)
    return(0);
#else
    long	rc;
////This is defined in krb.h:
    CREDENTIALS	creds;
    KTEXT_ST	ticket;
    struct ktc_principal	aserver;
    struct ktc_principal	aclient;
    char	realm_of_user[REALM_SZ]; /* Kerberos realm of user */
    char	realm_of_cell[REALM_SZ]; /* Kerberos realm of cell */
    char	local_cell[MAXCELLCHARS+1];
    char	Dmycell[MAXCELLCHARS+1];
    struct ktc_token	atoken;
    struct ktc_token	btoken;
    afsconf_cell	ak_cellconfig; /* General information about the cell */
    char	RealmName[128];
    char	CellName[128];
    char	ServiceName[128];
    DWORD       CurrentState;
    char        HostName[64];
    BOOL        try_krb5 = 0;
    int         retry = 0;
    int         len;
#ifndef NO_KRB5
    krb5_context  context = 0;
    krb5_ccache  _krb425_ccache = 0;
    krb5_creds increds;
    krb5_creds * k5creds = 0;
    krb5_error_code r;
    krb5_principal client_principal = 0;
    krb5_flags		flags = 0;
#endif /* NO_KRB5 */

    if (!AfsAvailable || GetAfsStatus(&AfsOnLine) && !AfsOnLine)
        return(0);

    if ( !realm ) realm = "";
    if ( !cell )  cell = "";
    if ( !service ) service = "";

    CurrentState = 0;
    memset(HostName, '\0', sizeof(HostName));
    gethostname(HostName, sizeof(HostName));
    if (GetServiceStatus(HostName, TRANSARCAFSDAEMON, &CurrentState) != NOERROR)
        return(0);
    if (CurrentState != SERVICE_RUNNING)
        return(0);

    memset(RealmName, '\0', sizeof(RealmName));
    memset(CellName, '\0', sizeof(CellName));
    memset(ServiceName, '\0', sizeof(ServiceName));
    memset(realm_of_user, '\0', sizeof(realm_of_user));
    memset(realm_of_cell, '\0', sizeof(realm_of_cell));
    memset(Dmycell, '\0', sizeof(Dmycell));

    // NULL or empty cell returns information on local cell
    if (cell && cell[0])
        strcpy(Dmycell, cell);
    rc = get_cellconfig(Dmycell, &ak_cellconfig, local_cell);
    if (rc && cell && cell[0]) {
        memset(Dmycell, '\0', sizeof(Dmycell));
        rc = get_cellconfig(Dmycell, &ak_cellconfig, local_cell);
    }
    if (rc)
        return(rc);

#ifndef NO_KRB5
    if (!(r = Leash_krb5_initialize(&context, &_krb425_ccache))) {
        int i;

        memset((char *)&increds, 0, sizeof(increds));

        (*pkrb5_cc_get_principal)(context, _krb425_ccache, &client_principal);
        i = krb5_princ_realm(context, client_principal)->length;
        if (i > REALM_SZ-1)
            i = REALM_SZ-1;
        strncpy(realm_of_user,krb5_princ_realm(context, client_principal)->data,i);
        realm_of_user[i] = 0;
        try_krb5 = 1;
    }
#endif /* NO_KRB5 */

#ifndef NO_KRB4
    if ( !try_krb5 || !realm_of_user[0] ) {
        if ((rc = (*pkrb_get_tf_realm)((*ptkt_string)(), realm_of_user)) != KSUCCESS)
        {
            return(rc);
        }
    }
#endif
    strcpy(realm_of_cell, afs_realm_of_cell(&ak_cellconfig));

    if (strlen(service) == 0)
        strcpy(ServiceName, "afs");
    else
        strcpy(ServiceName, service);

    if (strlen(cell) == 0)
        strcpy(CellName, local_cell);
    else
        strcpy(CellName, cell);

    if (strlen(realm) == 0)
        strcpy(RealmName, realm_of_cell);
    else
        strcpy(RealmName, realm);

    memset(&creds, '\0', sizeof(creds));

#ifndef NO_KRB5
    if ( try_krb5 ) {
        /* First try Service/Cell@REALM */
        if (r = (*pkrb5_build_principal)(context, &increds.server,
                                      strlen(RealmName),
                                      RealmName,
                                      ServiceName,
                                      CellName,
                                      0))
        {
            try_krb5 = 0;
            goto use_krb4;
        }

        increds.client = client_principal;
        increds.times.endtime = 0;
        /* Ask for DES since that is what V4 understands */
        increds.keyblock.enctype = ENCTYPE_DES_CBC_CRC;

#ifdef KRB5_TC_NOTICKET
        flags = 0;
        r = pkrb5_cc_set_flags(context, _krb425_ccache, flags);
#endif
        if (r == 0)
            r = pkrb5_get_credentials(context, 0, _krb425_ccache, &increds, &k5creds);
        if (r == KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN ||
			r == KRB5KRB_ERR_GENERIC /* Heimdal */) {
            /* Next try Service@REALM */
            pkrb5_free_principal(context, increds.server);
            r = pkrb5_build_principal(context, &increds.server,
                                      strlen(RealmName),
                                      RealmName,
                                      ServiceName,
                                      0);
            if (r == 0)
                r = pkrb5_get_credentials(context, 0, _krb425_ccache, &increds, &k5creds);
        }

        pkrb5_free_principal(context, increds.server);
        pkrb5_free_principal(context, client_principal);
#ifdef KRB5_TC_NOTICKET
        flags = KRB5_TC_NOTICKET;
        pkrb5_cc_set_flags(context, _krb425_ccache, flags);
#endif
        (void) pkrb5_cc_close(context, _krb425_ccache);
        _krb425_ccache = 0;

        if (r || k5creds == 0) {
            pkrb5_free_context(context);
            try_krb5 = 0;
            goto use_krb4;
        }

        /* This code inserts the entire K5 ticket into the token
         * No need to perform a krb524 translation which is
         * commented out in the code below
         */
        if ( use_krb524() || k5creds->ticket.length > MAXKTCTICKETLEN )
            goto try_krb524d;

        memset(&aserver, '\0', sizeof(aserver));
        strncpy(aserver.name, ServiceName, MAXKTCNAMELEN - 1);
        strncpy(aserver.cell, CellName, MAXKTCREALMLEN - 1);

        memset(&atoken, '\0', sizeof(atoken));
        atoken.kvno = RXKAD_TKT_TYPE_KERBEROS_V5;
        atoken.startTime = k5creds->times.starttime;
        atoken.endTime = k5creds->times.endtime;
        memcpy(&atoken.sessionKey, k5creds->keyblock.contents, k5creds->keyblock.length);
        atoken.ticketLen = k5creds->ticket.length;
        memcpy(atoken.ticket, k5creds->ticket.data, atoken.ticketLen);

      retry_gettoken5:
        rc = ktc_GetToken(&aserver, &btoken, sizeof(btoken), &aclient);
        if (rc != 0 && rc != KTC_NOENT && rc != KTC_NOCELL) {
            if ( rc == KTC_NOCM && retry < 20 ) {
                Sleep(500);
                retry++;
                goto retry_gettoken5;
            }
            goto try_krb524d;
        }

        if (atoken.kvno == btoken.kvno &&
             atoken.ticketLen == btoken.ticketLen &&
             !memcmp(&atoken.sessionKey, &btoken.sessionKey, sizeof(atoken.sessionKey)) &&
             !memcmp(atoken.ticket, btoken.ticket, atoken.ticketLen))
        {
            /* Success */
            pkrb5_free_creds(context, k5creds);
            pkrb5_free_context(context);
            return(0);
        }

        // * Reset the "aclient" structure before we call ktc_SetToken.
        // * This structure was first set by the ktc_GetToken call when
        // * we were comparing whether identical tokens already existed.

        len = min(k5creds->client->data[0].length,MAXKTCNAMELEN - 1);
        strncpy(aclient.name, k5creds->client->data[0].data, len);
        aclient.name[len] = '\0';

        if ( k5creds->client->length > 1 ) {
            char * p;
            strcat(aclient.name, ".");
            p = aclient.name + strlen(aclient.name);
            len = min(k5creds->client->data[1].length,MAXKTCNAMELEN - strlen(aclient.name) - 1);
            strncpy(p, k5creds->client->data[1].data, len);
            p[len] = '\0';
        }
        aclient.instance[0] = '\0';

        strcpy(aclient.cell, realm_of_cell);

        len = min(k5creds->client->realm.length,strlen(realm_of_cell));
        if ( strncmp(realm_of_cell, k5creds->client->realm.data, len) ) {
            char * p;
            strcat(aclient.name, "@");
            p = aclient.name + strlen(aclient.name);
            len = min(k5creds->client->realm.length,MAXKTCNAMELEN - strlen(aclient.name) - 1);
            strncpy(p, k5creds->client->realm.data, len);
            p[len] = '\0';
        }

        rc = ktc_SetToken(&aserver, &atoken, &aclient, 0);
        if (!rc) {
            /* Success */
            pkrb5_free_creds(context, k5creds);
            pkrb5_free_context(context);
            return(0);
        }

      try_krb524d:
        /* This requires krb524d to be running with the KDC */
        r = pkrb524_convert_creds_kdc(context, k5creds, &creds);
        pkrb5_free_creds(context, k5creds);
		pkrb5_free_context(context);
        if (r) {
            try_krb5 = 0;
            goto use_krb4;
        }
        rc = KSUCCESS;
    } else
#endif /* NO_KRB5 */
    {
      use_krb4:
	rc = KFAILURE;
    }
    if (rc != KSUCCESS)
    {
            return(rc);
    }

	memset(&aserver, '\0', sizeof(aserver));
    strncpy(aserver.name, ServiceName, MAXKTCNAMELEN - 1);
    strncpy(aserver.cell, CellName, MAXKTCNAMELEN - 1);

    memset(&atoken, '\0', sizeof(atoken));
    atoken.kvno = creds.kvno;
    atoken.startTime = creds.issue_date;
    atoken.endTime = (*pkrb_life_to_time)(creds.issue_date,creds.lifetime);
    memcpy(&atoken.sessionKey, creds.session, 8);
    atoken.ticketLen = creds.ticket_st.length;
    memcpy(atoken.ticket, creds.ticket_st.dat, atoken.ticketLen);

    if (!(rc = ktc_GetToken(&aserver, &btoken, sizeof(btoken), &aclient)) &&
        atoken.kvno == btoken.kvno &&
        atoken.ticketLen == btoken.ticketLen &&
        !memcmp(&atoken.sessionKey, &btoken.sessionKey, sizeof(atoken.sessionKey)) &&
        !memcmp(atoken.ticket, btoken.ticket, atoken.ticketLen))
    {
        return(0);
    }

    // * Reset the "aclient" structure before we call ktc_SetToken.
    // * This structure was first set by the ktc_GetToken call when
    // * we were comparing whether identical tokens already existed.

    strncpy(aclient.name, creds.pname, MAXKTCNAMELEN - 1);
    aclient.name[MAXKTCNAMELEN - 1] = '\0';
    if (creds.pinst[0])
    {
        strncat(aclient.name, ".", MAXKTCNAMELEN - 1 - strlen(aclient.name));
        aclient.name[MAXKTCNAMELEN - 1] = '\0';
        strncat(aclient.name, creds.pinst, MAXKTCNAMELEN - 1 - strlen(aclient.name));
        aclient.name[MAXKTCNAMELEN - 1] = '\0';
    }
    strcpy(aclient.instance, "");

    if ( strcmp(realm_of_cell, creds.realm) )
    {
        strncat(aclient.name, "@", MAXKTCNAMELEN - 1 - strlen(aclient.name));
        aclient.name[MAXKTCNAMELEN - 1] = '\0';
        strncat(aclient.name, creds.realm, MAXKTCNAMELEN - 1 - strlen(aclient.name));
        aclient.name[MAXKTCNAMELEN - 1] = '\0';
    }
    aclient.name[MAXKTCNAMELEN-1] = '\0';

    strcpy(aclient.cell, CellName);

    // * NOTE: On WIN32, the order of SetToken params changed...
    // * to   ktc_SetToken(&aserver, &aclient, &atoken, 0)
    // * from ktc_SetToken(&aserver, &atoken, &aclient, 0) on Unix...
    // * The afscompat ktc_SetToken provides the Unix order

    if (rc = ktc_SetToken(&aserver, &atoken, &aclient, 0))
    {
        Leash_afs_error(rc, "ktc_SetToken()");
        return(rc);
    }

    return(0);
#endif
}

/**************************************/
/* afs_realm_of_cell():               */
/**************************************/
static char *afs_realm_of_cell(afsconf_cell *cellconfig)
{
#ifdef NO_AFS
    return(0);
#else
    char krbhst[MAX_HSTNM]="";
    static char krbrlm[REALM_SZ+1]="";
#ifndef NO_KRB5
    krb5_context  ctx = 0;
    char ** realmlist=NULL;
    krb5_error_code r;
#endif /* NO_KRB5 */

    if (!cellconfig)
        return 0;

#ifndef NO_KRB5
    if ( pkrb5_init_context ) {
        r = pkrb5_init_context(&ctx);
        if ( !r )
            r = pkrb5_get_host_realm(ctx, cellconfig->hostName[0], &realmlist);
        if ( !r && realmlist && realmlist[0] ) {
            strcpy(krbrlm, realmlist[0]);
            pkrb5_free_host_realm(ctx, realmlist);
        }
        if (ctx)
            pkrb5_free_context(ctx);
    }
#endif /* NO_KRB5 */

    if ( !krbrlm[0] )
    {
        char *s = krbrlm;
        char *t = cellconfig->name;
        int c;

        while (c = *t++)
        {
            if (islower(c)) c=toupper(c);
            *s++ = c;
        }
        *s++ = 0;
    }
    return(krbrlm);
#endif
}

/**************************************/
/* get_cellconfig():                  */
/**************************************/
static int get_cellconfig(char *cell, afsconf_cell *cellconfig, char *local_cell)
{
#ifdef NO_AFS
    return(0);
#else
    int	rc;

    local_cell[0] = (char)0;
    memset(cellconfig, 0, sizeof(*cellconfig));

    /* WIN32: cm_GetRootCellName(local_cell) - NOTE: no way to get max chars */
    if (rc = cm_GetRootCellName(local_cell))
    {
        return(rc);
    }

    if (strlen(cell) == 0)
        strcpy(cell, local_cell);

    /* WIN32: cm_SearchCellFile(cell, pcallback, pdata) */
    strcpy(cellconfig->name, cell);

    return cm_SearchCell(cell, get_cellconfig_callback, NULL, (void*)cellconfig);
#endif
}

/**************************************/
/* get_cellconfig_callback():          */
/**************************************/
static long get_cellconfig_callback(void *cellconfig, struct sockaddr_in *addrp, char *namep)
{
#ifdef NO_AFS
    return(0);
#else
    afsconf_cell *cc = (afsconf_cell *)cellconfig;

    cc->hostAddr[cc->numServers] = *addrp;
    strcpy(cc->hostName[cc->numServers], namep);
    cc->numServers++;
    return(0);
#endif
}


/**************************************/
/* Leash_afs_error():           */
/**************************************/
void
Leash_afs_error(LONG rc, LPCSTR FailedFunctionName)
{
#ifdef NO_AFS
    return;
#else
#ifdef USE_MESSAGE_BOX
    char message[256];
    const char *errText;

    // Using AFS defines as error messages for now, until Transarc
    // gets back to me with "string" translations of each of these
    // const. defines.
    if (rc == KTC_ERROR)
      errText = "KTC_ERROR";
    else if (rc == KTC_TOOBIG)
      errText = "KTC_TOOBIG";
    else if (rc == KTC_INVAL)
      errText = "KTC_INVAL";
    else if (rc == KTC_NOENT)
      errText = "KTC_NOENT";
    else if (rc == KTC_PIOCTLFAIL)
      errText = "KTC_PIOCTLFAIL";
    else if (rc == KTC_NOPIOCTL)
      errText = "KTC_NOPIOCTL";
    else if (rc == KTC_NOCELL)
      errText = "KTC_NOCELL";
    else if (rc == KTC_NOCM)
      errText = "KTC_NOCM: The service, Transarc AFS Daemon, most likely is not started!";
    else
      errText = "Unknown error!";

    sprintf(message, "%s\n(%s failed)", errText, FailedFunctionName);
    MessageBox(NULL, message, "AFS", MB_OK | MB_ICONERROR | MB_TASKMODAL | MB_SETFOREGROUND);
#endif /* USE_MESSAGE_BOX */
    return;

#endif
}

DWORD GetServiceStatus(
    LPSTR lpszMachineName,
    LPSTR lpszServiceName,
    DWORD *lpdwCurrentState)
{
#ifdef NO_AFS
    return(NOERROR);
#else
    DWORD           hr               = NOERROR;
    SC_HANDLE       schSCManager     = NULL;
    SC_HANDLE       schService       = NULL;
    DWORD           fdwDesiredAccess = 0;
    SERVICE_STATUS  ssServiceStatus  = {0};
    BOOL            fRet             = FALSE;

    if ((pOpenSCManagerA == NULL) ||
        (pOpenServiceA == NULL) ||
        (pQueryServiceStatus == NULL) ||
        (pCloseServiceHandle == NULL))
        {
        *lpdwCurrentState = SERVICE_RUNNING;
        return(NOERROR);
        }

    *lpdwCurrentState = 0;

    fdwDesiredAccess = GENERIC_READ;

    schSCManager = (*pOpenSCManagerA)(lpszMachineName,
                                 NULL,
                                 fdwDesiredAccess);

    if(schSCManager == NULL)
    {
        hr = GetLastError();
        goto cleanup;
    }

    schService = (*pOpenServiceA)(schSCManager,
                             lpszServiceName,
                             fdwDesiredAccess);

    if(schService == NULL)
    {
        hr = GetLastError();
        goto cleanup;
    }

    fRet = (*pQueryServiceStatus)(schService,
                              &ssServiceStatus);

    if(fRet == FALSE)
    {
        hr = GetLastError();
        goto cleanup;
    }

    *lpdwCurrentState = ssServiceStatus.dwCurrentState;

cleanup:

    (*pCloseServiceHandle)(schService);
    (*pCloseServiceHandle)(schSCManager);

    return(hr);
#endif
}

BOOL
SetAfsStatus(
    DWORD AfsStatus
    )
{
#ifdef NO_AFS
    return(TRUE);
#else
    return write_registry_setting(LEASH_SETTINGS_REGISTRY_VALUE_AFS_STATUS,
                                  REG_DWORD, &AfsStatus,
                                  sizeof(AfsStatus)) ? FALSE : TRUE;
#endif
}

BOOL
GetAfsStatus(
    DWORD *AfsStatus
    )
{
#ifdef NO_AFS
    return(TRUE);
#else
    return read_registry_setting(LEASH_SETTINGS_REGISTRY_VALUE_AFS_STATUS,
                                 AfsStatus, sizeof(DWORD)) ? FALSE : TRUE;
#endif
}
