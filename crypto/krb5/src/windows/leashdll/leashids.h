// *** Child Windows
#define ID_MAINLIST			100
#define ID_COUNTDOWN		101

#define ID_DESTROY				111
#define ID_CHANGEPASSWORD		112
#define ID_INITTICKETS			113
#define ID_SYNCTIME				114

#define ID_UPDATESTATE			120
#define ID_UPDATEDISPLAY		121
#define ID_KRBDLL_DEBUG 		122


// *** Menus

#define ID_EXIT					200

#define ID_HELP_LEASH			210
#define ID_HELP_KERBEROS		211
#define ID_ABOUT				212

#define ID_CHECKV				299

// *** Password Dialog
#define ID_PRINCIPAL			301
#define ID_OLDPASSWORD			302
#define ID_CONFIRMPASSWORD1		303
#define ID_CONFIRMPASSWORD2		304
#define ID_PICFRAME				305

#define ID_PRINCCAPTION			311
#define ID_OLDPCAPTION			312
#define ID_CONFIRMCAPTION1		313
#define ID_CONFIRMCAPTION2		314
#define CAPTION_OFFSET			 10

#define ID_DURATION			320

#define ID_RESTART			351

#define ID_CLOSEME			380

// *** About dialog stuff
#define ID_LEASH_CPYRT		400
#define ID_LEASH_AUTHOR		401
#define ID_KERB_CPYRT		402
#define ID_KERB_AUTHOR		403
#define ID_LEGALESE			404
#define ID_ASSISTANCE		405

// *** Keyboard accelerator crud
#define ID_HELP					1000
#define ID_CONTEXTSENSITIVEHELP	1001
#define ID_ENDCSHELP			1002
#define ID_HELPFIRST			1000
#define ID_HELPLAST				1002

// *** window messages
#define WM_STARTUP			(WM_USER + 1)
#define WM_F1DOWN			(WM_USER + 2)
#define WM_DOHELP			(WM_USER + 3)
#define WM_PAINTICON		0x0026

// *** command messages
#define ID_NEXTSTATE		      10000

#define LSH_TIME_HOST                    1970
#define LSH_DEFAULT_TICKET_LIFE          1971
#define LSH_DEFAULT_TICKET_RENEW_TILL    1972
#define LSH_DEFAULT_TICKET_FORWARD       1973
#define LSH_DEFAULT_TICKET_NOADDRESS     1974
#define LSH_DEFAULT_TICKET_PROXIABLE     1975
#define LSH_DEFAULT_TICKET_PUBLICIP      1976
#define LSH_DEFAULT_DIALOG_KINIT_OPT     1978
#define LSH_DEFAULT_DIALOG_LIFE_MIN      1979
#define LSH_DEFAULT_DIALOG_LIFE_MAX      1980
#define LSH_DEFAULT_DIALOG_RENEW_MIN     1981
#define LSH_DEFAULT_DIALOG_RENEW_MAX     1982
#define LSH_DEFAULT_TICKET_RENEW         1983
#define LSH_DEFAULT_DIALOG_LOCK_LOCATION 1984
#define LSH_DEFAULT_UPPERCASEREALM       1985
#define LSH_DEFAULT_MSLSA_IMPORT         1986
#define LSH_DEFAULT_PRESERVE_KINIT       1987

// Authenticate Dialog
#define IDD_AUTHENTICATE                1162
#define IDC_STATIC_IPADDR               1163
#define IDC_STATIC_NAME                 1164
#define IDC_STATIC_PWD                  1165
#define IDC_EDIT_PRINCIPAL              1166
#define IDC_COMBO_REALM                 1167
#define IDC_EDIT_PASSWORD               1168
#define IDC_STATIC_LIFETIME             1169
#define IDC_SLIDER_LIFETIME             1170
#define IDC_STATIC_KRB5                 1171
#define IDC_CHECK_FORWARDABLE           1172
#define IDC_CHECK_NOADDRESS             1173
#define IDC_CHECK_RENEWABLE             1174
#define IDC_SLIDER_RENEWLIFE            1175
#define IDC_STATIC_LIFETIME_VALUE       1176
#define IDC_STATIC_RENEW_TILL_VALUE     1177
#define IDC_PICTURE_LEASH               1179
#define IDC_BUTTON_OPTIONS              1086
#define IDC_STATIC_REALM                1087
#define IDC_STATIC_COPYRIGHT            1088
#define IDC_STATIC_NOTICE               1089
#define IDC_STATIC_RENEW                1090
#define IDD_PASSWORD                    1091
#define IDC_BUTTON_CLEAR_HISTORY        1092
#define IDC_CHECK_REMEMBER_PRINCIPAL    1093
#define IDC_EDIT_PASSWORD2              1192
#define IDC_STATIC_PWD2                 1193
#define IDC_EDIT_PASSWORD3              1194
#define IDC_STATIC_PWD3                 1195
#define IDC_STATIC_VERSION              1196
