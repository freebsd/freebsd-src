#ifndef __LEASHWIN__
#define __LEASHWIN__

////Is this sufficient?
#include <krb5.h>
#define ANAME_SZ	        40
#define	REALM_SZ	        40
#define	SNAME_SZ	        40
#define	INST_SZ		        40
/* include space for '.' and '@' */
#define	MAX_K_NAME_SZ	    (ANAME_SZ + INST_SZ + REALM_SZ + 2)

#define DLGTYPE_PASSWD   0
#define DLGTYPE_CHPASSWD 1
#define DLGTYPE_MASK 0x0000ffff
#define DLGFLAG_READONLYPRINC 0x10000
typedef struct {
    int dlgtype;
    // Tells whether dialog box is in change pwd more or init ticket mode???
    // (verify this):
    int dlgstatemax; // What is this???
    // The title on the Dialog box - for Renewing or Initializing:
    LPSTR title;
    LPSTR principal;
} LSH_DLGINFO, FAR *LPLSH_DLGINFO;

#define LEASH_USERNAME_SZ        64
#define LEASH_REALM_SZ          192
#define LEASH_TITLE_SZ          128
#define LEASH_CCACHE_NAME_SZ 	264

typedef struct {
    DWORD size;
    int dlgtype;
    // Tells whether dialog box is in change pwd mode or init ticket mode
    LPSTR title;		// in v3, set to in.title
    LPSTR username;		// in v3, set to in.username
    LPSTR realm;		// in v3, set to in.realm
    int   use_defaults;
    int   forwardable;
    int   noaddresses;
    int   lifetime;
    int   renew_till;
    int   proxiable;
    int   publicip;
    // Version 1 of this structure ends here
    struct {
        char username[LEASH_USERNAME_SZ];
        char realm[LEASH_REALM_SZ];
	// Version 2 of this structure ends here
	char ccache[LEASH_CCACHE_NAME_SZ];
    } out;
    struct {
	char title[LEASH_TITLE_SZ];
	char username[LEASH_USERNAME_SZ];
	char realm[LEASH_REALM_SZ];
	char ccache[LEASH_CCACHE_NAME_SZ];
    } in;
} LSH_DLGINFO_EX, *LPLSH_DLGINFO_EX;

#define LSH_DLGINFO_EX_V1_SZ (sizeof(DWORD) + 3 * sizeof(LPSTR) + 8 * sizeof(int))
#define LSH_DLGINFO_EX_V2_SZ (LSH_DLGINFO_EX_V1_SZ + LEASH_USERNAME_SZ + LEASH_REALM_SZ)
#define LSH_DLGINFO_EX_V3_SZ (LSH_DLGINFO_EX_V2_SZ + LEASH_TITLE_SZ + LEASH_USERNAME_SZ + LEASH_REALM_SZ + 2 * LEASH_CCACHE_NAME_SZ)

#ifndef NETIDMGR
#define NETID_USERNAME_SZ       128
#define NETID_REALM_SZ          192
#define NETID_TITLE_SZ          256
#define NETID_CCACHE_NAME_SZ 	264

#define NETID_DLGTYPE_TGT      0
#define NETID_DLGTYPE_CHPASSWD 1
typedef struct {
    DWORD size;
    DWORD dlgtype;
    // Tells whether dialog box is in change pwd mode or init ticket mode
    struct {
	WCHAR title[NETID_TITLE_SZ];
	WCHAR username[NETID_USERNAME_SZ];
	WCHAR realm[NETID_REALM_SZ];
	WCHAR ccache[NETID_CCACHE_NAME_SZ];
	DWORD use_defaults;
	DWORD forwardable;
	DWORD noaddresses;
	DWORD lifetime;
	DWORD renew_till;
	DWORD proxiable;
	DWORD publicip;
	DWORD must_use_specified_principal;
    } in;
    struct {
        WCHAR username[NETID_USERNAME_SZ];
        WCHAR realm[NETID_REALM_SZ];
	WCHAR ccache[NETID_CCACHE_NAME_SZ];
    } out;
    // Version 1 of this structure ends here
} NETID_DLGINFO, *LPNETID_DLGINFO;

#define NETID_DLGINFO_V1_SZ (10 * sizeof(DWORD) \
        + sizeof(WCHAR) * (NETID_TITLE_SZ + \
        2 * NETID_USERNAME_SZ + 2 * NETID_REALM_SZ + \
        2 * NETID_CCACHE_NAME_SZ))
#endif /* NETIDMGR */

typedef struct TicketList TicketList;
struct TicketList {
    TicketList *next;
    char *service;
    char *encTypes;
    time_t issued;
    time_t valid_until;
    time_t renew_until;
    unsigned long flags;
};

typedef struct TICKETINFO TICKETINFO;
struct TICKETINFO {
    TICKETINFO *next;
    char   *principal;                /* Principal name/instance@realm */
    char   *ccache_name;
    TicketList *ticket_list;
    int     btickets;                 /* Do we have tickets? */
    time_t  issued;                   /* The issue time */
    time_t  valid_until;              /* */
    time_t  renew_until;              /* The Renew time (k5 only) */
    unsigned long flags;
};

#ifdef __cplusplus
extern "C" {
#endif

int FAR Leash_kinit_dlg(HWND hParent, LPLSH_DLGINFO lpdlginfo);
int FAR Leash_kinit_dlg_ex(HWND hParent, LPLSH_DLGINFO_EX lpdlginfoex);
int FAR Leash_changepwd_dlg(HWND hParent, LPLSH_DLGINFO lpdlginfo);
int FAR Leash_changepwd_dlg_ex(HWND hParent, LPLSH_DLGINFO_EX lpdlginfo);

long FAR Leash_checkpwd(char *principal, char *password);
long FAR Leash_changepwd(char *principal, char *password, char *newpassword, char** result_string);
long FAR Leash_kinit(char *principal, char *password, int lifetime);
long FAR Leash_kinit_ex(char * principal, char * password, int lifetime,
						int forwardable, int proxiable, int renew_life,
						int addressless, unsigned long publicIP);

long FAR Leash_klist(HWND hlist, TICKETINFO FAR *ticketinfo);
long FAR Leash_kdestroy(void);
long FAR Leash_get_lsh_errno( LONG FAR *err_val);

long FAR Leash_renew(void);
long FAR Leash_importable(void);
long FAR Leash_import(void);

BOOL Leash_set_help_file( char FAR *szHelpFile );
LPSTR Leash_get_help_file(void);

void Leash_reset_defaults(void);

#define NO_TICKETS 0
#define EXPD_TICKETS 2
#define GOOD_TICKETS 1

/* Leash Configuration functions - alters Current User Registry */
DWORD Leash_get_default_lifetime();
DWORD Leash_set_default_lifetime(DWORD minutes);
DWORD Leash_reset_default_lifetime();
DWORD Leash_get_default_renew_till();
DWORD Leash_set_default_renew_till(DWORD minutes);
DWORD Leash_reset_default_renew_till();
DWORD Leash_get_default_renewable();
DWORD Leash_set_default_renewable(DWORD onoff);
DWORD Leash_reset_default_renewable();
DWORD Leash_get_default_forwardable();
DWORD Leash_set_default_forwardable(DWORD onoff);
DWORD Leash_reset_default_forwardable();
DWORD Leash_get_default_noaddresses();
DWORD Leash_set_default_noaddresses(DWORD onoff);
DWORD Leash_reset_default_noaddresses();
DWORD Leash_get_default_proxiable();
DWORD Leash_set_default_proxiable(DWORD onoff);
DWORD Leash_reset_default_proxiable();
DWORD Leash_get_default_publicip();
DWORD Leash_set_default_publicip(DWORD ipv4addr);
DWORD Leash_reset_default_publicip();
DWORD Leash_get_hide_kinit_options();
DWORD Leash_set_hide_kinit_options(DWORD onoff);
DWORD Leash_reset_hide_kinit_options();
DWORD Leash_get_default_life_min();
DWORD Leash_set_default_life_min(DWORD minutes);
DWORD Leash_reset_default_life_min();
DWORD Leash_get_default_life_max();
DWORD Leash_set_default_life_max(DWORD minutes);
DWORD Leash_reset_default_life_max();
DWORD Leash_get_default_renew_min();
DWORD Leash_set_default_renew_min(DWORD minutes);
DWORD Leash_reset_default_renew_min();
DWORD Leash_get_default_renew_max();
DWORD Leash_set_default_renew_max(DWORD minutes);
DWORD Leash_reset_default_renew_max();
DWORD Leash_get_default_uppercaserealm();
DWORD Leash_set_default_uppercaserealm(DWORD onoff);
DWORD Leash_reset_default_uppercaserealm();
DWORD Leash_get_default_mslsa_import();
DWORD Leash_set_default_mslsa_import(DWORD onoffmatch);
DWORD Leash_reset_default_mslsa_import();
DWORD Leash_get_default_preserve_kinit_settings();
DWORD Leash_set_default_preserve_kinit_settings(DWORD onoff);
DWORD Leash_reset_default_preserve_kinit_settings();
#ifdef __cplusplus
}
#endif

#endif /* LEASHWIN */
