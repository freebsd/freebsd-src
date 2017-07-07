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
 * KSU was writen by:  Ari Medvinsky, ari@isi.edu
 */

#include "ksu.h"
#include "adm_proto.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <grp.h>

/* globals */
char * prog_name;
int auth_debug =0;
char k5login_path[MAXPATHLEN];
char k5users_path[MAXPATHLEN];
char * gb_err = NULL;
int quiet = 0;
/***********/

#define KS_TEMPORARY_CACHE "MEMORY:_ksu"
#define KS_TEMPORARY_PRINC "_ksu/_ksu@_ksu"
#define _DEF_CSH "/bin/csh"
static int set_env_var (char *, char *);
static void sweep_up (krb5_context, krb5_ccache);
static char * ontty (void);
static krb5_error_code set_ccname_env(krb5_context, krb5_ccache);
static void print_status( const char *fmt, ...)
#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 7)
    __attribute__ ((__format__ (__printf__, 1, 2)))
#endif
    ;
static krb5_error_code resolve_target_cache(krb5_context ksu_context,
                                            krb5_principal princ,
                                            krb5_ccache *ccache_out,
                                            krb5_boolean *ccache_reused);

/* Note -e and -a options are mutually exclusive */
/* insure the proper specification of target user as well as catching
   ill specified arguments to commands */

void usage (){
    fprintf(stderr,
            _("Usage: %s [target user] [-n principal] [-c source cachename] "
              "[-k] [-r time] [-pf] [-l lifetime] [-zZ] [-q] "
              "[-e command [args... ] ] [-a [args... ] ]\n"), prog_name);
}

/* for Ultrix and friends ... */
#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64
#endif

/* These are file static so sweep_up can get to them*/
static uid_t source_uid, target_uid;

int
main (argc, argv)
    int argc;
    char ** argv;
{
    int hp =0;
    int some_rest_copy = 0;
    int all_rest_copy = 0;
    char *localhostname = NULL;
    krb5_get_init_creds_opt *options = NULL;
    int option=0;
    int statusp=0;
    krb5_error_code retval = 0;
    krb5_principal client = NULL, tmp_princ = NULL;
    krb5_ccache cc_tmp = NULL, cc_target = NULL;
    krb5_context ksu_context;
    char * cc_target_tag = NULL;
    char * target_user = NULL;
    char * source_user;

    krb5_ccache cc_source = NULL;
    const char * cc_source_tag = NULL;
    const char * cc_source_tag_tmp = NULL;
    char * cmd = NULL, * exec_cmd = NULL;
    int errflg = 0;
    krb5_boolean auth_val;
    krb5_boolean authorization_val = FALSE;
    int path_passwd = 0;
    int done =0,i,j;
    uid_t ruid = getuid ();
    struct passwd *pwd=NULL,  *target_pwd ;
    char * shell;
    char ** params;
    int keep_target_cache = 0;
    int child_pid, child_pgrp, ret_pid;
    extern char * getpass(), *crypt();
    int pargc;
    char ** pargv;
    krb5_boolean stored = FALSE, cc_reused = FALSE, given_princ = FALSE;
    krb5_boolean zero_password;
    krb5_boolean restrict_creds;
    krb5_deltat lifetime, rlife;

    params = (char **) xcalloc (2, sizeof (char *));
    params[1] = NULL;

    unsetenv ("KRB5_CONFIG");

    retval = krb5_init_secure_context(&ksu_context);
    if (retval) {
        com_err(argv[0], retval, _("while initializing krb5"));
        exit(1);
    }

    retval = krb5_get_init_creds_opt_alloc(ksu_context, &options);
    if (retval) {
        com_err(argv[0], retval, _("while initializing krb5"));
        exit(1);
    }

    if (strrchr(argv[0], '/'))
        argv[0] = strrchr(argv[0], '/')+1;
    prog_name = argv[0];
    if (strlen (prog_name) > 50) {
        /* this many chars *after* last / ?? */
        com_err(prog_name, 0,
                _("program name too long - quitting to avoid triggering "
                  "system logging bugs"));
        exit (1);
    }


#ifndef LOG_NDELAY
#define LOG_NDELAY 0
#endif

#ifndef LOG_AUTH /* 4.2 syslog */
    openlog(prog_name, LOG_PID|LOG_NDELAY);
#else
    openlog(prog_name, LOG_PID | LOG_NDELAY, LOG_AUTH);
#endif /* 4.2 syslog */


    if (( argc == 1) || (argv[1][0] == '-')){
        target_user = xstrdup("root");
        pargc = argc;
        pargv = argv;
    } else {
        target_user = xstrdup(argv[1]);
        pargc = argc -1;

        if ((pargv =(char **) calloc(pargc +1,sizeof(char *)))==NULL){
            com_err(prog_name, errno, _("while allocating memory"));
            exit(1);
        }

        pargv[pargc] = NULL;
        pargv[0] = argv[0];

        for(i =1; i< pargc; i ++){
            pargv[i] = argv[i + 1];
        }
    }

    if (krb5_seteuid (ruid)) {
        com_err (prog_name, errno, _("while setting euid to source user"));
        exit (1);
    }
    while(!done && ((option = getopt(pargc, pargv,"n:c:r:a:zZDfpkql:e:")) != -1)){
        switch (option) {
        case 'r':
            if (strlen (optarg) >= 14)
                optarg = "bad-time";
            retval = krb5_string_to_deltat(optarg, &rlife);
            if (retval != 0 || rlife == 0) {
                fprintf(stderr, _("Bad lifetime value (%s hours?)\n"), optarg);
                errflg++;
            }
            krb5_get_init_creds_opt_set_renew_life(options, rlife);
            break;
        case 'a':
            /* when integrating this remember to pass in pargc, pargv and
               take care of params argument */
            optind --;
            if (auth_debug){printf("Before get_params optind=%d\n", optind);}

            if ((retval = get_params( & optind, pargc, pargv, &params))){
                com_err(prog_name, retval, _("when gathering parameters"));
                errflg++;
            }
            if(auth_debug){ printf("After get_params optind=%d\n", optind);}
            done = 1;
            break;
        case 'p':
            krb5_get_init_creds_opt_set_proxiable(options, 1);
            break;
        case 'f':
            krb5_get_init_creds_opt_set_forwardable(options, 1);
            break;
        case 'k':
            keep_target_cache =1;
            break;
        case 'q':
            quiet =1;
            break;
        case 'l':
            if (strlen (optarg) >= 14)
                optarg = "bad-time";
            retval = krb5_string_to_deltat(optarg, &lifetime);
            if (retval != 0 || lifetime == 0) {
                fprintf(stderr, _("Bad lifetime value (%s hours?)\n"), optarg);
                errflg++;
            }
            krb5_get_init_creds_opt_set_tkt_life(options, lifetime);
            break;
        case 'n':
            if ((retval = krb5_parse_name(ksu_context, optarg, &client))){
                com_err(prog_name, retval, _("when parsing name %s"), optarg);
                errflg++;
            }
            given_princ = TRUE;
            break;
#ifdef DEBUG
        case 'D':
            auth_debug = 1;
            break;
#endif
        case 'z':
            some_rest_copy = 1;
            if(all_rest_copy) {
                fprintf(stderr,
                        _("-z option is mutually exclusive with -Z.\n"));
                errflg++;
            }
            break;
        case 'Z':
            all_rest_copy = 1;
            if(some_rest_copy) {
                fprintf(stderr,
                        _("-Z option is mutually exclusive with -z.\n"));
                errflg++;
            }
            break;
        case 'c':
            if (cc_source_tag == NULL) {
                cc_source_tag = xstrdup(optarg);
                if ( strchr(cc_source_tag, ':')){
                    cc_source_tag_tmp = strchr(cc_source_tag, ':') + 1;

                    if (!ks_ccache_name_is_initialized(ksu_context,
                                                       cc_source_tag)) {
                        com_err(prog_name, errno,
                                _("while looking for credentials cache %s"),
                                cc_source_tag_tmp);
                        exit (1);
                    }
                }
                else {
                    fprintf(stderr, _("malformed credential cache name %s\n"),
                            cc_source_tag);
                    errflg++;
                }

            } else {
                fprintf(stderr, _("Only one -c option allowed\n"));
                errflg++;
            }
            break;
        case 'e':
            cmd = xstrdup(optarg);
            if(auth_debug){printf("Before get_params optind=%d\n", optind);}
            if ((retval = get_params( & optind, pargc, pargv, &params))){
                com_err(prog_name, retval, _("when gathering parameters"));
                errflg++;
            }
            if(auth_debug){printf("After get_params optind=%d\n", optind);}
            done = 1;

            if (auth_debug){
                fprintf(stderr,"Command to be executed: %s\n", cmd);
            }
            break;
        case '?':
        default:
            errflg++;
            break;
        }
    }

    if (errflg) {
        usage();
        exit(2);
    }

    if (optind != pargc ){
        usage();
        exit(2);
    }

    if (auth_debug){
        for(j=1; params[j] != NULL; j++){
            fprintf (stderr,"params[%d]= %s\n", j,params[j]);
        }
    }

    /***********************************/
    source_user = getlogin(); /*checks for the the login name in /etc/utmp*/

    /* verify that that the user exists and get his passwd structure */

    if (source_user == NULL ||(pwd = getpwnam(source_user)) == NULL ||
        pwd->pw_uid != ruid){
        pwd = getpwuid(ruid);
    }

    if (pwd == NULL) {
        fprintf(stderr, _("ksu: who are you?\n"));
        exit(1);
    }
    if (pwd->pw_uid != ruid) {
        fprintf (stderr, _("Your uid doesn't match your passwd entry?!\n"));
        exit (1);
    }
    /* Okay, now we have *some* passwd entry that matches the
       current real uid.  */

    /* allocate space and copy the usernamane there */
    source_user = xstrdup(pwd->pw_name);
    source_uid = pwd->pw_uid;

    if (!strcmp(SOURCE_USER_LOGIN, target_user)){
        target_user = xstrdup (source_user);
    }

    if ((target_pwd = getpwnam(target_user)) == NULL){
        fprintf(stderr, _("ksu: unknown login %s\n"), target_user);
        exit(1);
    }
    target_uid = target_pwd->pw_uid;

    init_auth_names(target_pwd->pw_dir);

    /***********************************/

    if (cc_source_tag == NULL){
        cc_source_tag = krb5_cc_default_name(ksu_context);
        cc_source_tag_tmp = strchr(cc_source_tag, ':');
        if (cc_source_tag_tmp == 0)
            cc_source_tag_tmp = cc_source_tag;
        else
            cc_source_tag_tmp++;
    }

    /* get a handle for the cache */
    if ((retval = krb5_cc_resolve(ksu_context, cc_source_tag, &cc_source))){
        com_err(prog_name, retval, _("while getting source cache"));
        exit(1);
    }

    if ((retval = get_best_princ_for_target(ksu_context, source_uid,
                                            target_uid, source_user,
                                            target_user, cc_source,
                                            options, cmd, localhostname,
                                            &client, &hp))){
        com_err(prog_name,retval, _("while selecting the best principal"));
        exit(1);
    }

    /* We may be running as either source or target, depending on
       what happened; become source.*/
    if ( geteuid() != source_uid) {
        if (krb5_seteuid(0) || krb5_seteuid(source_uid) ) {
            com_err(prog_name, errno, _("while returning to source uid after "
                                        "finding best principal"));
            exit(1);
        }
    }

    if (auth_debug){
        if (hp){
            fprintf(stderr,
                    "GET_best_princ_for_target result: NOT AUTHORIZED\n");
        }else{
            fprintf(stderr,
                    "GET_best_princ_for_target result-best principal ");
            plain_dump_principal (ksu_context, client);
            fprintf(stderr,"\n");
        }
    }

    if (hp){
        if (gb_err) fprintf(stderr, "%s", gb_err);
        fprintf(stderr, _("account %s: authorization failed\n"), target_user);
        exit(1);
    }

    if (auth_debug)
        fprintf(stderr, " source cache =  %s\n", cc_source_tag);

    /*
     * After proper authentication and authorization, populate a cache for the
     * target user.
     */

    /*
     * We read the set of creds we want to copy from the source ccache as the
     * source uid, become root for authentication, and then become the target
     * user to handle authorization and creating the target user's cache.
     */

    /* if root ksu's to a regular user, then
       then only the credentials for that particular user
       should be copied */

    restrict_creds = (source_uid == 0) && (target_uid != 0);
    retval = krb5_parse_name(ksu_context, KS_TEMPORARY_PRINC, &tmp_princ);
    if (retval) {
        com_err(prog_name, retval, _("while parsing temporary name"));
        exit(1);
    }
    retval = krb5_cc_resolve(ksu_context, KS_TEMPORARY_CACHE, &cc_tmp);
    if (retval) {
        com_err(prog_name, retval, _("while creating temporary cache"));
        exit(1);
    }
    retval = krb5_ccache_copy(ksu_context, cc_source, tmp_princ, cc_tmp,
                              restrict_creds, client, &stored);
    if (retval) {
        com_err(prog_name, retval, _("while copying cache %s to %s"),
                krb5_cc_get_name(ksu_context, cc_source), KS_TEMPORARY_CACHE);
        exit(1);
    }
    krb5_cc_close(ksu_context, cc_source);

    krb5_get_init_creds_opt_set_out_ccache(ksu_context, options, cc_tmp);

    /* Become root for authentication*/

    if (krb5_seteuid(0)) {
        com_err(prog_name, errno, _("while reclaiming root uid"));
        exit(1);
    }

    if ((source_uid == 0) || (target_uid == source_uid)){
#ifdef GET_TGT_VIA_PASSWD
        if (!all_rest_copy && given_princ && client != NULL && !stored) {
            fprintf(stderr, _("WARNING: Your password may be exposed if you "
                              "enter it here and are logged\n"));
            fprintf(stderr, _("         in remotely using an unsecure "
                              "(non-encrypted) channel.\n"));
            if (ksu_get_tgt_via_passwd(ksu_context, client, options,
                                       &zero_password, NULL) == FALSE) {

                if (zero_password == FALSE){
                    fprintf(stderr, _("Goodbye\n"));
                    exit(1);
                }

                fprintf(stderr, _("Could not get a tgt for "));
                plain_dump_principal (ksu_context, client);
                fprintf(stderr, "\n");

            }
            stored = TRUE;
        }
#endif /* GET_TGT_VIA_PASSWD */
    }

    /* if the user is root or same uid then authentication is not neccesary,
       root gets in automatically */

    if (source_uid && (source_uid != target_uid)) {
        char * client_name;

        auth_val = krb5_auth_check(ksu_context, client, localhostname,
                                   options, target_user, cc_tmp,
                                   &path_passwd, target_uid);

        /* if Kerberos authentication failed then exit */
        if (auth_val ==FALSE){
            fprintf(stderr, _("Authentication failed.\n"));
            syslog(LOG_WARNING, "'%s %s' authentication failed for %s%s",
                   prog_name,target_user,source_user,ontty());
            exit(1);
        }
        stored = TRUE;

        if ((retval = krb5_unparse_name(ksu_context, client, &client_name))) {
            com_err(prog_name, retval, _("When unparsing name"));
            exit(1);
        }

        print_status(_("Authenticated %s\n"), client_name);
        syslog(LOG_NOTICE,"'%s %s' authenticated %s for %s%s",
               prog_name,target_user,client_name,
               source_user,ontty());

        /* Run authorization as target.*/
        if (krb5_seteuid(target_uid)) {
            com_err(prog_name, errno, _("while switching to target for "
                                        "authorization check"));
            exit(1);
        }

        if ((retval = krb5_authorization(ksu_context, client,target_user,
                                         cmd, &authorization_val, &exec_cmd))){
            com_err(prog_name,retval, _("while checking authorization"));
            krb5_seteuid(0); /*So we have some chance of sweeping up*/
            exit(1);
        }

        if (krb5_seteuid(0)) {
            com_err(prog_name, errno, _("while switching back from target "
                                        "after authorization check"));
            exit(1);
        }
        if (authorization_val == TRUE){

            if (cmd) {
                print_status(_("Account %s: authorization for %s for "
                               "execution of\n"), target_user, client_name);
                print_status(_("               %s successful\n"), exec_cmd);
                syslog(LOG_NOTICE,
                       "Account %s: authorization for %s for execution of %s successful",
                       target_user, client_name, exec_cmd);

            }else{
                print_status(_("Account %s: authorization for %s "
                               "successful\n"), target_user, client_name);
                syslog(LOG_NOTICE,
                       "Account %s: authorization for %s successful",
                       target_user, client_name);
            }
        }else {
            if (cmd){
                if (exec_cmd){ /* was used to pass back the error msg */
                    fprintf(stderr, "%s", exec_cmd );
                    syslog(LOG_WARNING, "%s",exec_cmd);
                }
                fprintf(stderr, _("Account %s: authorization for %s for "
                                  "execution of %s failed\n"),
                        target_user, client_name, cmd );
                syslog(LOG_WARNING,
                       "Account %s: authorization for %s for execution of %s failed",
                       target_user, client_name, cmd );

            }else{
                fprintf(stderr, _("Account %s: authorization of %s failed\n"),
                        target_user, client_name);
                syslog(LOG_WARNING,
                       "Account %s: authorization of %s failed",
                       target_user, client_name);

            }

            exit(1);
        }
    }

    if( some_rest_copy){
        retval = krb5_ccache_filter(ksu_context, cc_tmp, client);
        if (retval) {
            com_err(prog_name,retval, _("while calling cc_filter"));
            exit(1);
        }
    }

    if (all_rest_copy){
        retval = krb5_cc_initialize(ksu_context, cc_tmp, tmp_princ);
        if (retval) {
            com_err(prog_name, retval, _("while erasing target cache"));
            exit(1);
        }
        stored = FALSE;
    }

    /* get the shell of the user, this will be the shell used by su */
    target_pwd = getpwnam(target_user);

    if (target_pwd->pw_shell)
        shell = xstrdup(target_pwd->pw_shell);
    else {
        shell = _DEF_CSH;  /* default is cshell */
    }

#ifdef HAVE_GETUSERSHELL

    /* insist that the target login uses a standard shell (root is omited) */

    if (!standard_shell(target_pwd->pw_shell) && source_uid) {
        fprintf(stderr, _("ksu: permission denied (shell).\n"));
        exit(1);
    }
#endif /* HAVE_GETUSERSHELL */

    if (target_pwd->pw_uid){

        if(set_env_var("USER", target_pwd->pw_name)){
            fprintf(stderr,
                    _("ksu: couldn't set environment variable USER\n"));
            exit(1);
        }
    }

    if(set_env_var( "HOME", target_pwd->pw_dir)){
        fprintf(stderr, _("ksu: couldn't set environment variable HOME\n"));
        exit(1);
    }

    if(set_env_var( "SHELL", shell)){
        fprintf(stderr, _("ksu: couldn't set environment variable SHELL\n"));
        exit(1);
    }

    /* set permissions */
    if (setgid(target_pwd->pw_gid) < 0) {
        perror("ksu: setgid");
        exit(1);
    }

    if (initgroups(target_user, target_pwd->pw_gid)) {
        fprintf(stderr, _("ksu: initgroups failed.\n"));
        exit(1);
    }

    if ( ! strcmp(target_user, source_user)){
        print_status(_("Leaving uid as %s (%ld)\n"),
                     target_user, (long) target_pwd->pw_uid);
    }else{
        print_status(_("Changing uid to %s (%ld)\n"),
                     target_user, (long) target_pwd->pw_uid);
    }

#ifdef  HAVE_SETLUID
    /*
     * If we're on a system which keeps track of login uids, then
     * set the login uid. If this fails this opens up a problem on DEC OSF
     * with C2 enabled.
     */
    if (setluid((uid_t) pwd->pw_uid) < 0) {
        perror("setluid");
        exit(1);
    }
#endif  /* HAVE_SETLUID */

    if (setuid(target_pwd->pw_uid) < 0) {
        perror("ksu: setuid");
        exit(1);
    }

    retval = resolve_target_cache(ksu_context, client, &cc_target, &cc_reused);
    if (retval)
        exit(1);
    retval = krb5_cc_get_full_name(ksu_context, cc_target, &cc_target_tag);
    if (retval) {
        com_err(prog_name, retval, _("while getting name of target ccache"));
        sweep_up(ksu_context, cc_target);
        exit(1);
    }
    if (auth_debug)
        fprintf(stderr, " target cache =  %s\n", cc_target_tag);
    if (cc_reused)
        keep_target_cache = TRUE;

    if (stored) {
        retval = krb5_ccache_copy(ksu_context, cc_tmp, client, cc_target,
                                  FALSE, client, &stored);
        if (retval) {
            com_err(prog_name, retval, _("while copying cache %s to %s"),
                    KS_TEMPORARY_CACHE, cc_target_tag);
            exit(1);
        }

        if (!ks_ccache_is_initialized(ksu_context, cc_target)) {
            com_err(prog_name, errno,
                    _("%s does not have correct permissions for %s, "
                      "%s aborted"), target_user, cc_target_tag, prog_name);
            exit(1);
        }
    }

    krb5_free_string(ksu_context, cc_target_tag);

    /* Set the cc env name to target. */
    retval = set_ccname_env(ksu_context, cc_target);
    if (retval != 0) {
        sweep_up(ksu_context, cc_target);
        exit(1);
    }

    if (cmd){
        if ((source_uid == 0) || (source_uid == target_uid )){
            exec_cmd = cmd;
        }

        if( !exec_cmd){
            fprintf(stderr, _("Internal error: command %s did not get "
                              "resolved\n"), cmd);
            exit(1);
        }

        params[0] = exec_cmd;
    }
    else{
        params[0] = shell;
    }

    if (auth_debug){
        fprintf(stderr, "program to be execed %s\n",params[0]);
    }

    if( keep_target_cache ) {
        execv(params[0], params);
        com_err(prog_name, errno, _("while trying to execv %s"), params[0]);
        sweep_up(ksu_context, cc_target);
        exit(1);
    }else{
        statusp = 1;
        switch ((child_pid = fork())) {
        default:
            if (auth_debug){
                printf(" The child pid is %ld\n", (long) child_pid);
                printf(" The parent pid is %ld\n", (long) getpid());
            }
            while ((ret_pid = waitpid(child_pid, &statusp, WUNTRACED)) != -1) {
                if (WIFSTOPPED(statusp)) {
                    child_pgrp = tcgetpgrp(1);
                    kill(getpid(), SIGSTOP);
                    tcsetpgrp(1, child_pgrp);
                    kill(child_pid, SIGCONT);
                    statusp = 1;
                    continue;
                }
                break;
            }
            if (auth_debug){
                printf("The exit status of the child is %d\n", statusp);
            }
            if (ret_pid == -1) {
                com_err(prog_name, errno, _("while calling waitpid"));
            }
            sweep_up(ksu_context, cc_target);
            exit (statusp);
        case -1:
            com_err(prog_name, errno, _("while trying to fork."));
            sweep_up(ksu_context, cc_target);
            exit (1);
        case 0:
            execv(params[0], params);
            com_err(prog_name, errno, _("while trying to execv %s"),
                    params[0]);
            exit (1);
        }
    }
}

/* Set KRB5CCNAME in the environment to point to ccache.  Print an error
 * message on failure. */
static krb5_error_code
set_ccname_env(krb5_context ksu_context, krb5_ccache ccache)
{
    krb5_error_code retval;
    char *ccname;

    retval = krb5_cc_get_full_name(ksu_context, ccache, &ccname);
    if (retval) {
        com_err(prog_name, retval, _("while reading cache name from ccache"));
        return retval;
    }
    if (set_env_var(KRB5_ENV_CCNAME, ccname)) {
        retval = errno;
        fprintf(stderr,
                _("ksu: couldn't set environment variable %s\n"),
                KRB5_ENV_CCNAME);
    }
    krb5_free_string(ksu_context, ccname);
    return retval;
}

/*
 * Get the configured default ccache name.  Unset KRB5CCNAME and force a
 * recomputation so we don't use values for the source user.  Print an error
 * message on failure.
 */
static krb5_error_code
get_configured_defccname(krb5_context context, char **target_out)
{
    krb5_error_code retval;
    const char *defname;
    char *target = NULL;

    *target_out = NULL;

    unsetenv(KRB5_ENV_CCNAME);

    /* Make sure we don't have a cached value for a different uid. */
    retval = krb5_cc_set_default_name(context, NULL);
    if (retval != 0) {
        com_err(prog_name, retval, _("while resetting target ccache name"));
        return retval;
    }

    defname = krb5_cc_default_name(context);
    if (defname != NULL) {
        if (strchr(defname, ':') != NULL) {
            target = strdup(defname);
        } else {
            if (asprintf(&target, "FILE:%s", defname) < 0)
                target = NULL;
        }
    }
    if (target == NULL) {
        com_err(prog_name, ENOMEM, _("while determining target ccache name"));
        return ENOMEM;
    }
    *target_out = target;
    return 0;
}

/* Determine where the target user's creds should be stored.  Print an error
 * message on failure. */
static krb5_error_code
resolve_target_cache(krb5_context context, krb5_principal princ,
                     krb5_ccache *ccache_out, krb5_boolean *ccache_reused)
{
    krb5_error_code retval;
    krb5_boolean switchable, reused = FALSE;
    krb5_ccache ccache = NULL;
    char *sep, *ccname = NULL, *sym = NULL, *target;

    *ccache_out = NULL;
    *ccache_reused = FALSE;

    retval = get_configured_defccname(context, &target);
    if (retval != 0)
        return retval;

    /* Check if the configured default name uses a switchable type. */
    sep = strchr(target, ':');
    *sep = '\0';
    switchable = krb5_cc_support_switch(context, target);
    *sep = ':';

    if (!switchable) {
        /* Try to avoid destroying an in-use target ccache by coming up with
         * the name of a cache that doesn't exist yet. */
        do {
            free(ccname);
            retval = gen_sym(context, &sym);
            if (retval) {
                com_err(prog_name, retval,
                        _("while generating part of the target ccache name"));
                return retval;
            }
            if (asprintf(&ccname, "%s.%s", target, sym) < 0) {
                retval = ENOMEM;
                free(sym);
                com_err(prog_name, retval, _("while allocating memory for the "
                                             "target ccache name"));
                goto cleanup;
            }
            free(sym);
        } while (ks_ccache_name_is_initialized(context, ccname));
        retval = krb5_cc_resolve(context, ccname, &ccache);
    } else {
        /* Look for a cache in the collection that we can reuse. */
        retval = krb5_cc_cache_match(context, princ, &ccache);
        if (retval == 0) {
            reused = TRUE;
        } else {
            /* There isn't one, so create a new one. */
            *sep = '\0';
            retval = krb5_cc_new_unique(context, target, NULL, &ccache);
            *sep = ':';
            if (retval) {
                com_err(prog_name, retval,
                        _("while creating new target ccache"));
                goto cleanup;
            }
            retval = krb5_cc_initialize(context, ccache, princ);
            if (retval) {
                com_err(prog_name, retval,
                        _("while initializing target cache"));
                goto cleanup;
            }
        }
    }

    *ccache_out = ccache;
    *ccache_reused = reused;

cleanup:
    free(target);
    return retval;
}

#ifdef HAVE_GETUSERSHELL

int standard_shell(sh)
    char *sh;
{
    register char *cp;
    char *getusershell();

    while ((cp = getusershell()) != NULL)
        if (!strcmp(cp, sh))
            return (1);
    return (0);
}

#endif /* HAVE_GETUSERSHELL */

static char * ontty()
{
    char *p, *ttyname();
    static char buf[MAXPATHLEN + 5];
    int result;

    buf[0] = 0;
    if ((p = ttyname(STDERR_FILENO))) {
        result = snprintf(buf, sizeof(buf), " on %s", p);
        if (SNPRINTF_OVERFLOW(result, sizeof(buf))) {
            fprintf(stderr, _("terminal name %s too long\n"), p);
            exit (1);
        }
    }
    return (buf);
}


static int set_env_var(name, value)
    char *name;
    char *value;
{
    char * env_var_buf;

    asprintf(&env_var_buf,"%s=%s",name, value);
    return putenv(env_var_buf);

}

static void sweep_up(context, cc)
    krb5_context context;
    krb5_ccache cc;
{
    krb5_error_code retval;

    krb5_seteuid(0);
    if (krb5_seteuid(target_uid) < 0) {
        com_err(prog_name, errno,
                _("while changing to target uid for destroying ccache"));
        exit(1);
    }

    if (ks_ccache_is_initialized(context, cc)) {
        if ((retval = krb5_cc_destroy(context, cc)))
            com_err(prog_name, retval, _("while destroying cache"));
    }
}

/*****************************************************************
get_params is to be called for the -a option or -e option to
           collect all params passed in for the shell or for
           cmd.  An aray is returned containing all params.
           optindex is incremented accordingly and the first
           element in the returned array is reserved for the
           name of the command to be executed or the name of the
           shell.
*****************************************************************/

krb5_error_code
get_params(optindex, pargc, pargv, params)
    int *optindex;
    int pargc;
    char **pargv;
    char ***params;
{

    int i,j;
    char ** ret_params;
    int size = pargc - *optindex + 2;

    if ((ret_params = (char **) calloc(size, sizeof (char *)))== NULL ){
        return ENOMEM;
    }

    for (i = *optindex, j=1; i < pargc; i++,j++){
        ret_params[j] = pargv[i];
        *optindex = *optindex + 1;
    }

    ret_params[size-1] = NULL;
    *params = ret_params;
    return 0;
}

static
void print_status(const char *fmt, ...)
{
    va_list ap;
    if (! quiet){
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
    }
}

krb5_error_code
ksu_tgtname(context, server, client, tgtprinc)
    krb5_context context;
    const krb5_data *server, *client;
    krb5_principal *tgtprinc;
{
    return krb5_build_principal_ext(context, tgtprinc, client->length, client->data,
                                    KRB5_TGS_NAME_SIZE, KRB5_TGS_NAME,
                                    server->length, server->data,
                                    0);
}
