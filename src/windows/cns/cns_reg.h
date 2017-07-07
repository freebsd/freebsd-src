/*
 * Copyright (c) 1997 Cygnus Solutions
 *
 * Author:  Michael Graff
 */

#include <krb5.h>

typedef struct cns_reg {
  DWORD         x;                               /* default dialog size */
  DWORD         y;
  DWORD         cx;
  DWORD         cy;
  DWORD         lifetime;                        /* ticket lifetime */
  DWORD         beep;                            /* beep on expire/warning? */
  DWORD         alert;                           /* alert (deiconify) when tix expired? */
  DWORD         forwardable;                     /* get forwardable tickets? */
  DWORD         conf_override;                   /* allow changing of confname */
  DWORD         cc_override;                     /* allow changing of ccname */
  DWORD         noaddresses;                     /* Don't require address in tickets */
  char          name[MAX_K_NAME_SZ];             /* last user used */
  char          realm[MAX_K_NAME_SZ];            /* last realm used */
  char          confname[FILENAME_MAX];
  char          ccname[FILENAME_MAX];
  char          def_confname[FILENAME_MAX];
  char          def_ccname[FILENAME_MAX];
  char          logins[FILE_MENU_MAX_LOGINS][MAX_K_NAME_SZ];
} cns_reg_t;

extern cns_reg_t cns_res;

void cns_load_registry(void);
void cns_save_registry(void);
