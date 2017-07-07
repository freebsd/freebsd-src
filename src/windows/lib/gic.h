/*
 * Copyright (C) 1997 Cygnus Solutions
 *
 * Author:  Michael Graff
 */

#ifndef _WINDOWS_LIB_GIC_H
#define _WINDOWS_LIB_GIC_H

#include <windows.h>
#include <windowsx.h>

#include "krb5.h"

typedef struct {
	HINSTANCE    hinstance;     /* application instance */
	HWND         hwnd;          /* parent window */
	WORD         id;            /* starting ID */
	WORD         width;         /* max width of the dialog box */
	const char  *banner;        /* the banner */
	WORD         num_prompts;   /* the number of prompts we were passed */
	krb5_prompt *prompts;       /* the prompts themselves */
} gic_data;

krb5_error_code KRB5_CALLCONV gic_prompter(krb5_context, void *, const char *,
					   const char *, int, krb5_prompt []);

#endif /* _WINDOWS_LIB_GIC_H */
