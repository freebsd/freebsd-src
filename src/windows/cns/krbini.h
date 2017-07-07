/* Kerberos changed window message */
#define WM_KERBEROS_CHANGED "Kerberos Changed"

/* Kerberos Windows initialization file */
#define KERBEROS_INI    "kerberos.ini"
#ifdef CYGNUS
#define KERBEROS_HLP    "kerbnet.hlp"
#else
#define KERBEROS_HLP    "krb5.hlp"
#endif
#define INI_DEFAULTS    "Defaults"
#define   INI_USER        "User"          /* Default user */
#define   INI_INSTANCE    "Instance"      /* Default instance */
#define   INI_REALM       "Realm"         /* Default realm */
#define   INI_POSITION    "Position"
#define   INI_OPTIONS     "Options"
#define   INI_DURATION    "Duration"   /* Ticket duration in minutes */
#define INI_EXPIRATION  "Expiration" /* Action on expiration (alert or beep) */
#define   INI_ALERT       "Alert"
#define   INI_BEEP        "Beep"
#define   INI_FILES       "Files"
#ifdef KRB4
#define   INI_KRB_CONF    "krb.conf"     /* Location of krb.conf file */
#define   DEF_KRB_CONF    "krb.conf"      /* Default name for krb.conf file */
#endif /* KRB4 */
#ifdef KRB5
#define INI_KRB5_CONF   "krb5.ini"	/* From k5-config.h */
#define INI_KRB_CONF    INI_KRB5_CONF	/* Location of krb.conf file */
#define DEF_KRB_CONF    INI_KRB5_CONF	/* Default name for krb.conf file */
#define INI_TICKETOPTS  "TicketOptions" /* Ticket options */
#define   INI_FORWARDABLE  "Forwardable" /* get forwardable tickets */
#define INI_KRB_CCACHE  "krb5cc"       	/* From k5-config.h */
#endif /* KRB5 */
#define INI_KRB_REALMS  "krb.realms"    /* Location of krb.realms file */
#define DEF_KRB_REALMS  "krb.realms"    /* Default name for krb.realms file */
#define INI_RECENT_LOGINS "Recent Logins"
#define INI_LOGIN       "Login"
