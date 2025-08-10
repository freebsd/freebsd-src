/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright (c) 1994 by the University of Southern California
 *
 * EXPORT OF THIS SOFTWARE from the United States of America may
 *     require a specific license from the United States Government.
 *     It is the responsibility of any person or organization contemplating
 *     export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to copy, modify, and distribute
 *     this software and its documentation in source and binary forms is
 *     hereby granted, provided that any documentation or other materials
 *     related to such distribution or use acknowledge that the software
 *     was developed by the University of Southern California.
 *
 * DISCLAIMER OF WARRANTY.  THIS SOFTWARE IS PROVIDED "AS IS".  The
 *     University of Southern California MAKES NO REPRESENTATIONS OR
 *     WARRANTIES, EXPRESS OR IMPLIED.  By way of example, but not
 *     limitation, the University of Southern California MAKES NO
 *     REPRESENTATIONS OR WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY
 *     PARTICULAR PURPOSE. The University of Southern
 *     California shall not be held liable for any liability nor for any
 *     direct, indirect, or consequential damages with respect to any
 *     claim by the user or distributor of the ksu software.
 *
 * KSU was written by:  Ari Medvinsky, ari@isi.edu
 */

#include "ksu.h"

static void
free_fcmd_list(char **list)
{
    size_t i;

    if (list == NULL)
        return;
    for (i = 0; i < MAX_CMD && list[i] != NULL; i++)
        free(list[i]);
    free(list);
}

krb5_boolean
fowner(FILE *fp, uid_t uid)
{
    struct stat sbuf;

    /*
     * For security reasons, file must be owned either by
     * the user himself, or by root.  Otherwise, don't grant access.
     */
    if (fstat(fileno(fp), &sbuf)) {
        return(FALSE);
    }

    if ((sbuf.st_uid != uid) && sbuf.st_uid) {
        return(FALSE);
    }

    return(TRUE);
}

/*
 * Given a Kerberos principal "principal", and a local username "luser",
 * determine whether user is authorized to login according to the authorization
 * files ~luser/.k5login" and ~luser/.k5users.  Set *ok to TRUE if authorized,
 * FALSE if not authorized.  Return 0 if the authorization check succeeded
 * (regardless of its result), non-zero if it encountered an error.
 */

krb5_error_code
krb5_authorization(krb5_context context, krb5_principal principal,
                   const char *luser, char *cmd, krb5_boolean *ok,
                   char **out_fcmd)
{
    struct passwd *pwd;
    char *princname = NULL;
    int k5login_flag =0;
    int k5users_flag =0;
    krb5_boolean retbool =FALSE;
    FILE * login_fp = 0, * users_fp = 0;
    krb5_error_code retval = 0;
    struct stat st_temp;

    *ok =FALSE;

    /* no account => no access */
    if ((pwd = getpwnam(luser)) == NULL)
        goto cleanup;

    retval = krb5_unparse_name(context, principal, &princname);
    if (retval)
        return retval;

#ifdef DEBUG
    printf("principal to be authorized %s\n", princname);
    printf("login file: %s\n", k5login_path);
    printf("users file: %s\n", k5users_path);
#endif

    k5login_flag = stat(k5login_path, &st_temp);
    k5users_flag = stat(k5users_path, &st_temp);

    /* k5login and k5users must be owned by target user or root */
    if (!k5login_flag){
        login_fp = fopen(k5login_path, "r");
        if (login_fp == NULL)
            goto cleanup;
        if (fowner(login_fp, pwd->pw_uid) == FALSE)
            goto cleanup;
    }

    if (!k5users_flag){
        users_fp = fopen(k5users_path, "r");
        if (users_fp == NULL)
            goto cleanup;
        if (fowner(users_fp, pwd->pw_uid) == FALSE)
            goto cleanup;
    }

    if (auth_debug){
        fprintf(stderr,
                "In krb5_authorization: if auth files exist -> can access\n");
    }

    /* if either file exists,
       first see if the principal is in the login in file,
       if it's not there check the k5users file */

    if (!k5login_flag){
        if (auth_debug)
            fprintf(stderr,
                    "In krb5_authorization: principal to be authorized %s\n",
                    princname);

        retval = k5login_lookup(login_fp,  princname, &retbool);
        if (retval)
            goto cleanup;
        if (retbool) {
            if (cmd)
                *out_fcmd = xstrdup(cmd);
        }
    }

    if ((!k5users_flag) && (retbool == FALSE) ){
        retval = k5users_lookup (users_fp, princname,
                                 cmd, &retbool, out_fcmd);
        if (retval)
            goto cleanup;
    }

    if (k5login_flag && k5users_flag){

        char * kuser =  (char *) xcalloc (strlen(princname), sizeof(char));
        if (!(krb5_aname_to_localname(context, principal,
                                      strlen(princname), kuser))
            && (strcmp(kuser, luser) == 0)) {
            retbool = TRUE;
        }

        free(kuser);
    }

    *ok =retbool;

cleanup:
    if (users_fp != NULL)
        fclose(users_fp);
    if (login_fp != NULL)
        fclose(login_fp);
    free(princname);
    return retval;
}

/***********************************************************
k5login_lookup looks for princname in file fp. Spaces
before the princaname (in the file ) are not ignored
spaces after the princname are ignored. If there are
any tokens after the principal name  FALSE is returned.

***********************************************************/

krb5_error_code
k5login_lookup(FILE *fp, char *princname, krb5_boolean *found)
{

    krb5_error_code retval;
    char * line;
    char * fprinc;
    char * lp;
    krb5_boolean loc_found = FALSE;

    retval = get_line(fp, &line);
    if (retval)
        return retval;

    while (line){
        fprinc = get_first_token (line, &lp);

        if (fprinc && (!strcmp(princname, fprinc))){
            if( get_next_token (&lp) ){
                free (line);
                break;  /* nothing should follow princname*/
            }
            else{
                loc_found = TRUE;
                free (line);
                break;
            }
        }

        free (line);

        retval = get_line(fp, &line);
        if (retval)
            return retval;
    }


    *found = loc_found;
    return 0;

}

/***********************************************************
k5users_lookup looks for princname in file fp. Spaces
before the princaname (in the file ) are not ignored
spaces after the princname are ignored.

authorization alg:

if princname is not found return false.

if princname is found{
         if cmd == NULL then the file entry after principal
                        name must be nothing or *

         if cmd !=NULL  then entry must be matched (* is ok)
}


***********************************************************/
krb5_error_code
k5users_lookup(FILE *fp, char *princname, char *cmd,
               krb5_boolean *found, char **out_fcmd)
{
    krb5_error_code retval;
    char * line;
    char * fprinc, *fcmd;
    char * lp;
    char * loc_fcmd = NULL;
    krb5_boolean loc_found = FALSE;

    retval = get_line(fp, &line);
    if (retval)
        return retval;

    while (line){
        fprinc = get_first_token (line, &lp);

        if (fprinc && (!strcmp(princname, fprinc))){
            fcmd = get_next_token (&lp);

            if ((fcmd) && (!strcmp(fcmd, PERMIT_ALL_COMMANDS))){
                if (get_next_token(&lp) == NULL){
                    loc_fcmd =cmd ? xstrdup(cmd): NULL;
                    loc_found = TRUE;
                }
                free (line);
                break;
            }

            if (cmd == NULL){
                if (fcmd == NULL)
                    loc_found = TRUE;
                free (line);
                break;

            }else{
                if (fcmd != NULL) {
                    char * temp_rfcmd, *err;
                    krb5_boolean match;
                    do {
                        if(match_commands(fcmd,cmd,&match,
                                          &temp_rfcmd, &err)){
                            if (auth_debug){
                                fprintf(stderr,"%s",err);
                            }
                            loc_fcmd = err;
                            break;
                        }else{
                            if (match == TRUE){
                                loc_fcmd = temp_rfcmd;
                                loc_found = TRUE;
                                break;
                            }
                        }

                    }while ((fcmd = get_next_token( &lp)));
                }
                free (line);
                break;
            }
        }

        free (line);

        retval = get_line(fp, &line);
        if (retval) {
            return retval;
        }
    }

    *out_fcmd = loc_fcmd;
    *found = loc_found;
    return 0;

}


/***********************************************
fcmd_resolve -
takes a command specified .k5users file and
resolves it into a full path name.

************************************************/

krb5_boolean
fcmd_resolve(char *fcmd, char ***out_fcmd, char **out_err)
{
    char * err;
    char ** tmp_fcmd = NULL;
    char * path_ptr, *path;
    char * lp, * tc;
    int i=0;
    krb5_boolean ok = FALSE;

    tmp_fcmd = (char **) xcalloc (MAX_CMD, sizeof(char *));

    if (*fcmd == '/'){  /* must be full path */
        tmp_fcmd[0] = xstrdup(fcmd);
        tmp_fcmd[1] = NULL;
        *out_fcmd = tmp_fcmd;
        tmp_fcmd = NULL;
    }else{
        /* must be either full path or just the cmd name */
        if (strchr(fcmd, '/')){
            asprintf(&err, _("Error: bad entry - %s in %s file, must be "
                             "either full path or just the cmd name\n"),
                     fcmd, KRB5_USERS_NAME);
            *out_err = err;
            goto cleanup;
        }

#ifndef CMD_PATH
        asprintf(&err, _("Error: bad entry - %s in %s file, since %s is just "
                         "the cmd name, CMD_PATH must be defined \n"),
                 fcmd, KRB5_USERS_NAME, fcmd);
        *out_err = err;
        goto cleanup;
#else

        path = xstrdup (CMD_PATH);
        path_ptr = path;

        while ((*path_ptr == ' ') || (*path_ptr == '\t')) path_ptr ++;

        tc = get_first_token (path_ptr, &lp);

        if (! tc){
            asprintf(&err, _("Error: bad entry - %s in %s file, CMD_PATH "
                             "contains no paths \n"), fcmd, KRB5_USERS_NAME);
            *out_err = err;
            goto cleanup;
        }

        i=0;
        do{
            if (*tc != '/'){  /* must be full path */
                asprintf(&err, _("Error: bad path %s in CMD_PATH for %s must "
                                 "start with '/' \n"), tc, KRB5_USERS_NAME );
                *out_err = err;
                goto cleanup;
            }

            tmp_fcmd[i] = xasprintf("%s/%s", tc, fcmd);

            i++;

        } while((tc = get_next_token (&lp)));

        tmp_fcmd[i] = NULL;
        *out_fcmd = tmp_fcmd;
        tmp_fcmd = NULL;
#endif /* CMD_PATH */
    }

    ok = TRUE;

cleanup:
    free_fcmd_list(tmp_fcmd);
    return ok;
}

/********************************************
cmd_single - checks if cmd consists of a path
             or a single token

********************************************/

krb5_boolean
cmd_single(char *cmd)
{

    if ( ( strrchr( cmd, '/')) ==  NULL){
        return TRUE;
    }else{
        return FALSE;
    }
}

/********************************************
cmd_arr_cmp_postfix - compares a command with the postfix
         of fcmd
********************************************/

int
cmd_arr_cmp_postfix(char **fcmd_arr, char *cmd)
{
    char  * temp_fcmd;
    char *ptr;
    int result =1;
    int i = 0;

    while(fcmd_arr[i]){
        if ( (ptr = strrchr( fcmd_arr[i], '/')) ==  NULL){
            temp_fcmd = fcmd_arr[i];
        }else {
            temp_fcmd = ptr + 1;
        }

        result = strcmp (temp_fcmd, cmd);
        if (result == 0){
            break;
        }
        i++;
    }

    return result;


}

/**********************************************
cmd_arr_cmp - checks if cmd matches any
              of the fcmd entries.

**********************************************/

int
cmd_arr_cmp(char **fcmd_arr, char *cmd)
{
    int result =1;
    int i = 0;

    while(fcmd_arr[i]){
        result = strcmp (fcmd_arr[i], cmd);
        if (result == 0){
            break;
        }
        i++;
    }
    return result;
}


krb5_boolean
find_first_cmd_that_exists(char **fcmd_arr, char **cmd_out, char **err_out)
{
    struct stat st_temp;
    int i = 0;
    krb5_boolean retbool= FALSE;
    int j =0;
    struct k5buf buf;

    while(fcmd_arr[i]){
        if (!stat (fcmd_arr[i], &st_temp )){
            *cmd_out = xstrdup(fcmd_arr[i]);
            retbool = TRUE;
            break;
        }
        i++;
    }

    if (retbool == FALSE ){
        k5_buf_init_dynamic(&buf);
        k5_buf_add(&buf, _("Error: not found -> "));
        for(j= 0; j < i; j ++)
            k5_buf_add_fmt(&buf, " %s ", fcmd_arr[j]);
        k5_buf_add(&buf, "\n");
        *err_out = k5_buf_cstring(&buf);
        if (*err_out == NULL) {
            perror(prog_name);
            exit(1);
        }
    }


    return retbool;
}

/***************************************************************
returns 1 if there is an error, 0 if no error.

***************************************************************/

int
match_commands(char *fcmd, char *cmd, krb5_boolean *match,
               char **cmd_out, char **err_out)
{
    char ** fcmd_arr = NULL;
    char * err;
    char * cmd_temp;
    int result = 1;

    if(fcmd_resolve(fcmd, &fcmd_arr, &err )== FALSE ){
        *err_out = err;
        goto cleanup;
    }

    if (cmd_single( cmd ) == TRUE){
        if (!cmd_arr_cmp_postfix(fcmd_arr, cmd)){ /* found */
            if (!find_first_cmd_that_exists(fcmd_arr, &cmd_temp, &err)) {
                *err_out = err;
                goto cleanup;
            }

            *match = TRUE;
            *cmd_out = cmd_temp;
        } else {
            *match = FALSE;
        }
    }else{
        if (!cmd_arr_cmp(fcmd_arr, cmd)){  /* found */
            *match = TRUE;
            *cmd_out = xstrdup(cmd);
        } else{
            *match = FALSE;
        }
    }

    result = 0;

cleanup:
    free_fcmd_list(fcmd_arr);
    return result;
}

/*********************************************************
   get_line - returns a line of any length.  out_line
              is set to null if eof.
*********************************************************/

krb5_error_code
get_line(FILE *fp, char **out_line)
{
    char * line, *r, *newline , *line_ptr;
    int chunk_count = 1;

    line = (char *) xcalloc (BUFSIZ, sizeof (char ));
    line_ptr = line;
    line[0] = '\0';

    while (( r = fgets(line_ptr, BUFSIZ , fp)) != NULL){
        newline = strchr(line_ptr, '\n');
        if (newline) {
            *newline = '\0';
            break;
        }
        else {
            chunk_count ++;
            line = xrealloc(line, chunk_count * BUFSIZ);

            line_ptr = line + (BUFSIZ -1) *( chunk_count -1) ;
        }
    }

    if ((r == NULL) && (strlen(line) == 0)) {
        *out_line = NULL;
    }
    else{
        *out_line = line;
    }

    return 0;
}

/*******************************************************
get_first_token -
Expects a '\0' terminated input line .
If there are any spaces before the first token, they
will be returned as part of the first token.

Note: this routine reuses the space pointed to by line
******************************************************/

char *
get_first_token(char *line, char **lnext)
{

    char * lptr, * out_ptr;


    out_ptr = line;
    lptr = line;

    while (( *lptr == ' ') || (*lptr == '\t')) lptr ++;

    if (strlen(lptr) == 0) return NULL;

    while (( *lptr != ' ') && (*lptr != '\t') && (*lptr != '\0')) lptr ++;

    if (*lptr == '\0'){
        *lnext = lptr;
    } else{
        *lptr = '\0';
        *lnext = lptr + 1;
    }

    return out_ptr;
}
/**********************************************************
get_next_token -
returns the next token pointed to by *lnext.
returns NULL if there is no more tokens.
Note: that this function modifies the stream
      pointed to by *lnext and does not allocate
      space for the returned tocken. It also advances
      lnext to the next tocken.
**********************************************************/

char *
get_next_token (char **lnext)
{
    char * lptr, * out_ptr;


    lptr = *lnext;

    while (( *lptr == ' ') || (*lptr == '\t')) lptr ++;

    if (strlen(lptr) == 0) return NULL;

    out_ptr = lptr;

    while (( *lptr != ' ') && (*lptr != '\t') && (*lptr != '\0')) lptr ++;

    if (*lptr == '\0'){
        *lnext = lptr;
    } else{
        *lptr = '\0';
        *lnext = lptr + 1;
    }

    return out_ptr;
}

void
init_auth_names(char *pw_dir)
{
    const char *sep;
    int r1, r2;

    sep = ((strlen(pw_dir) == 1) && (*pw_dir == '/')) ? "" : "/";
    r1 = snprintf(k5login_path, sizeof(k5login_path), "%s%s%s",
                  pw_dir, sep, KRB5_LOGIN_NAME);
    r2 = snprintf(k5users_path, sizeof(k5users_path), "%s%s%s",
                  pw_dir, sep, KRB5_USERS_NAME);
    if (SNPRINTF_OVERFLOW(r1, sizeof(k5login_path)) ||
        SNPRINTF_OVERFLOW(r2, sizeof(k5users_path))) {
        fprintf(stderr, _("home directory name `%s' too long, can't search "
                          "for .k5login\n"), pw_dir);
        exit (1);
    }
}
