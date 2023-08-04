/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* kprop/kprop.c */
/*
 * Copyright 1990,1991,2008 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#include "k5-int.h"
#include <inttypes.h>
#include <locale.h>
#include <sys/file.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/param.h>
#include <netdb.h>
#include <fcntl.h>

#include "com_err.h"
#include "fake-addrinfo.h"
#include "kprop.h"

#ifndef GETSOCKNAME_ARG3_TYPE
#define GETSOCKNAME_ARG3_TYPE unsigned int
#endif

static char *kprop_version = KPROP_PROT_VERSION;

static char *progname = NULL;
static int debug = 0;
static char *keytab_path = NULL;
static char *replica_host;
static char *realm = NULL;
static char *def_realm = NULL;
static char *file = KPROP_DEFAULT_FILE;

/* The Kerberos principal we'll be sending as, initialized in get_tickets. */
static krb5_principal my_principal;

static krb5_creds creds;
static krb5_address *sender_addr;
static const char *port = KPROP_SERVICE;
static char *dbpathname;

static void parse_args(krb5_context context, int argc, char **argv);
static void get_tickets(krb5_context context);
static void usage(void);
static void open_connection(krb5_context context, char *host, int *fd_out);
static void kerberos_authenticate(krb5_context context,
                                  krb5_auth_context *auth_context, int fd,
                                  krb5_principal me, krb5_creds **new_creds);
static int open_database(krb5_context context, char *data_fn, off_t *size);
static void close_database(krb5_context context, int fd);
static void xmit_database(krb5_context context,
                          krb5_auth_context auth_context, krb5_creds *my_creds,
                          int fd, int database_fd, off_t in_database_size);
static void send_error(krb5_context context, krb5_creds *my_creds, int fd,
                       char *err_text, krb5_error_code err_code);
static void update_last_prop_file(char *hostname, char *file_name);

static void usage()
{
    fprintf(stderr, _("\nUsage: %s [-r realm] [-f file] [-d] [-P port] "
                      "[-s keytab] replica_host\n\n"), progname);
    exit(1);
}

int
main(int argc, char **argv)
{
    int fd, database_fd;
    off_t database_size;
    krb5_error_code retval;
    krb5_context context;
    krb5_creds *my_creds;
    krb5_auth_context auth_context;

    setlocale(LC_ALL, "");
    retval = krb5_init_context(&context);
    if (retval) {
        com_err(argv[0], retval, _("while initializing krb5"));
        exit(1);
    }
    parse_args(context, argc, argv);
    get_tickets(context);

    database_fd = open_database(context, file, &database_size);
    open_connection(context, replica_host, &fd);
    kerberos_authenticate(context, &auth_context, fd, my_principal, &my_creds);
    xmit_database(context, auth_context, my_creds, fd, database_fd,
                  database_size);
    update_last_prop_file(replica_host, file);
    printf(_("Database propagation to %s: SUCCEEDED\n"), replica_host);
    krb5_free_cred_contents(context, my_creds);
    close_database(context, database_fd);
    krb5_free_default_realm(context, def_realm);
    exit(0);
}

static void
parse_args(krb5_context context, int argc, char **argv)
{
    int c;
    krb5_error_code ret;

    progname = argv[0];
    while ((c = getopt(argc, argv, "r:f:dP:s:")) != -1) {
        switch (c) {
        case 'r':
            realm = optarg;
            break;
        case 'f':
            file = optarg;
            break;
        case 'd':
            debug++;
            break;
        case 'P':
            port = optarg;
            break;
        case 's':
            keytab_path = optarg;
            break;
        default:
            usage();
        }
    }
    if (argc - optind != 1)
        usage();
    replica_host = argv[optind];

    if (realm == NULL) {
        ret = krb5_get_default_realm(context, &def_realm);
        if (ret) {
            com_err(progname, errno, _("while getting default realm"));
            exit(1);
        }
        realm = def_realm;
    }
}

static void
get_tickets(krb5_context context)
{
    char *server;
    krb5_error_code retval;
    krb5_keytab keytab = NULL;
    krb5_principal server_princ = NULL;

    /* Figure out what tickets we'll be using to send. */
    retval = sn2princ_realm(context, NULL, KPROP_SERVICE_NAME, realm,
                            &my_principal);
    if (retval) {
        com_err(progname, errno, _("while setting client principal name"));
        exit(1);
    }

    /* Construct the principal name for the replica host. */
    memset(&creds, 0, sizeof(creds));
    retval = sn2princ_realm(context, replica_host, KPROP_SERVICE_NAME, realm,
                            &server_princ);
    if (retval) {
        com_err(progname, errno, _("while setting server principal name"));
        exit(1);
    }
    retval = krb5_unparse_name_flags(context, server_princ,
                                     KRB5_PRINCIPAL_UNPARSE_NO_REALM, &server);
    if (retval) {
        com_err(progname, retval, _("while unparsing server name"));
        exit(1);
    }

    if (keytab_path != NULL) {
        retval = krb5_kt_resolve(context, keytab_path, &keytab);
        if (retval) {
            com_err(progname, retval, _("while resolving keytab"));
            exit(1);
        }
    }

    retval = krb5_get_init_creds_keytab(context, &creds, my_principal, keytab,
                                        0, server, NULL);
    if (retval) {
        com_err(progname, retval, _("while getting initial credentials\n"));
        exit(1);
    }

    if (keytab != NULL)
        krb5_kt_close(context, keytab);
    krb5_free_unparsed_name(context, server);
    krb5_free_principal(context, server_princ);
}

static void
open_connection(krb5_context context, char *host, int *fd_out)
{
    krb5_error_code retval;
    GETSOCKNAME_ARG3_TYPE socket_length;
    struct addrinfo hints, *res, *answers;
    struct sockaddr *sa;
    struct sockaddr_storage my_sin;
    int s, error;

    *fd_out = -1;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_ADDRCONFIG;
    error = getaddrinfo(host, port, &hints, &answers);
    if (error != 0) {
        com_err(progname, 0, "%s: %s", host, gai_strerror(error));
        exit(1);
    }

    s = -1;
    retval = EINVAL;
    for (res = answers; res != NULL; res = res->ai_next) {
        s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (s < 0) {
            com_err(progname, errno, _("while creating socket"));
            exit(1);
        }

        if (connect(s, res->ai_addr, res->ai_addrlen) < 0) {
            retval = errno;
            close(s);
            s = -1;
            continue;
        }

        /* We successfully connect()ed */
        *fd_out = s;

        break;
    }

    freeaddrinfo(answers);

    if (s == -1) {
        com_err(progname, retval, _("while connecting to server"));
        exit(1);
    }

    /* Set sender_addr. */
    socket_length = sizeof(my_sin);
    if (getsockname(s, (struct sockaddr *)&my_sin, &socket_length) < 0) {
        com_err(progname, errno, _("while getting local socket address"));
        exit(1);
    }
    sa = (struct sockaddr *)&my_sin;
    if (sockaddr2krbaddr(context, sa->sa_family, sa, &sender_addr) != 0) {
        com_err(progname, errno, _("while converting local address"));
        exit(1);
    }
}

static void
kerberos_authenticate(krb5_context context, krb5_auth_context *auth_context,
                      int fd, krb5_principal me, krb5_creds **new_creds)
{
    krb5_error_code retval;
    krb5_error *error = NULL;
    krb5_ap_rep_enc_part *rep_result;

    retval = krb5_auth_con_init(context, auth_context);
    if (retval)
        exit(1);

    krb5_auth_con_setflags(context, *auth_context,
                           KRB5_AUTH_CONTEXT_DO_SEQUENCE);

    retval = krb5_auth_con_setaddrs(context, *auth_context, sender_addr, NULL);
    if (retval) {
        com_err(progname, retval, _("in krb5_auth_con_setaddrs"));
        exit(1);
    }

    retval = krb5_sendauth(context, auth_context, &fd, kprop_version,
                           me, creds.server, AP_OPTS_MUTUAL_REQUIRED, NULL,
                           &creds, NULL, &error, &rep_result, new_creds);
    if (retval) {
        com_err(progname, retval, _("while authenticating to server"));
        if (error != NULL) {
            if (error->error == KRB_ERR_GENERIC) {
                if (error->text.data) {
                    fprintf(stderr, _("Generic remote error: %s\n"),
                            error->text.data);
                }
            } else if (error->error) {
                com_err(progname,
                        (krb5_error_code)error->error + ERROR_TABLE_BASE_krb5,
                        _("signalled from server"));
                if (error->text.data) {
                    fprintf(stderr, _("Error text from server: %s\n"),
                            error->text.data);
                }
            }
            krb5_free_error(context, error);
        }
        exit(1);
    }
    krb5_free_ap_rep_enc_part(context, rep_result);
}

/*
 * Open the Kerberos database dump file.  Takes care of locking it
 * and making sure that the .ok file is more recent that the database
 * dump file itself.
 *
 * Returns the file descriptor of the database dump file.  Also fills
 * in the size of the database file.
 */
static int
open_database(krb5_context context, char *data_fn, off_t *size)
{
    struct stat stbuf, stbuf_ok;
    char *data_ok_fn;
    int fd, err;

    dbpathname = strdup(data_fn);
    if (dbpathname == NULL) {
        com_err(progname, ENOMEM, _("allocating database file name '%s'"),
                data_fn);
        exit(1);
    }
    fd = open(dbpathname, O_RDONLY);
    if (fd < 0) {
        com_err(progname, errno, _("while trying to open %s"), dbpathname);
        exit(1);
    }

    err = krb5_lock_file(context, fd,
                         KRB5_LOCKMODE_SHARED | KRB5_LOCKMODE_DONTBLOCK);
    if (err == EAGAIN || err == EWOULDBLOCK || errno == EACCES) {
        com_err(progname, 0, _("database locked"));
        exit(1);
    } else if (err) {
        com_err(progname, err, _("while trying to lock '%s'"), dbpathname);
        exit(1);
    }
    if (fstat(fd, &stbuf)) {
        com_err(progname, errno, _("while trying to stat %s"), data_fn);
        exit(1);
    }
    if (asprintf(&data_ok_fn, "%s.dump_ok", data_fn) < 0) {
        com_err(progname, ENOMEM, _("while trying to malloc data_ok_fn"));
        exit(1);
    }
    if (stat(data_ok_fn, &stbuf_ok)) {
        com_err(progname, errno, _("while trying to stat %s"), data_ok_fn);
        free(data_ok_fn);
        exit(1);
    }
    if (stbuf.st_mtime > stbuf_ok.st_mtime) {
        com_err(progname, 0, _("'%s' more recent than '%s'."), data_fn,
                data_ok_fn);
        exit(1);
    }
    free(data_ok_fn);
    *size = stbuf.st_size;
    return fd;
}

static void
close_database(krb5_context context, int fd)
{
    int err;

    err = krb5_lock_file(context, fd, KRB5_LOCKMODE_UNLOCK);
    if (err)
        com_err(progname, err, _("while unlocking database '%s'"), dbpathname);
    free(dbpathname);
    close(fd);
}

/*
 * Now we send over the database.  We use the following protocol:
 * Send over a KRB_SAFE message with the size.  Then we send over the
 * database in blocks of KPROP_BLKSIZE, encrypted using KRB_PRIV.
 * Then we expect to see a KRB_SAFE message with the size sent back.
 *
 * At any point in the protocol, we may send a KRB_ERROR message; this
 * will abort the entire operation.
 */
static void
xmit_database(krb5_context context, krb5_auth_context auth_context,
              krb5_creds *my_creds, int fd, int database_fd,
              off_t in_database_size)
{
    krb5_int32 n;
    krb5_data inbuf, outbuf;
    char buf[KPROP_BUFSIZ], dbsize_buf[KPROP_DBSIZE_MAX_BUFSIZ];
    krb5_error_code retval;
    krb5_error *error;
    uint64_t database_size = in_database_size, send_size, sent_size;

    /* Send over the size. */
    inbuf = make_data(dbsize_buf, sizeof(dbsize_buf));
    encode_database_size(database_size, &inbuf);
    /* KPROP_CKSUMTYPE */
    retval = krb5_mk_safe(context, auth_context, &inbuf, &outbuf, NULL);
    if (retval) {
        com_err(progname, retval, _("while encoding database size"));
        send_error(context, my_creds, fd, _("while encoding database size"),
                   retval);
        exit(1);
    }

    retval = krb5_write_message(context, &fd, &outbuf);
    if (retval) {
        krb5_free_data_contents(context, &outbuf);
        com_err(progname, retval, _("while sending database size"));
        exit(1);
    }
    krb5_free_data_contents(context, &outbuf);

    /* Initialize the initial vector. */
    retval = krb5_auth_con_initivector(context, auth_context);
    if (retval) {
        send_error(context, my_creds, fd,
                   "failed while initializing i_vector", retval);
        com_err(progname, retval, _("while allocating i_vector"));
        exit(1);
    }

    /* Send over the file, block by block. */
    inbuf.data = buf;
    sent_size = 0;
    while ((n = read(database_fd, buf, sizeof(buf)))) {
        inbuf.length = n;
        retval = krb5_mk_priv(context, auth_context, &inbuf, &outbuf, NULL);
        if (retval) {
            snprintf(buf, sizeof(buf),
                     "while encoding database block starting at %"PRIu64,
                     sent_size);
            com_err(progname, retval, "%s", buf);
            send_error(context, my_creds, fd, buf, retval);
            exit(1);
        }

        retval = krb5_write_message(context, &fd, &outbuf);
        if (retval) {
            krb5_free_data_contents(context, &outbuf);
            com_err(progname, retval,
                    _("while sending database block starting at %"PRIu64),
                    sent_size);
            exit(1);
        }
        krb5_free_data_contents(context, &outbuf);
        sent_size += n;
        if (debug)
            printf("%"PRIu64" bytes sent.\n", sent_size);
    }
    if (sent_size != database_size) {
        com_err(progname, 0, _("Premature EOF found for database file!"));
        send_error(context, my_creds, fd,
                   "Premature EOF found for database file!",
                   KRB5KRB_ERR_GENERIC);
        exit(1);
    }

    /*
     * OK, we've sent the database; now let's wait for a success
     * indication from the remote end.
     */
    retval = krb5_read_message(context, &fd, &inbuf);
    if (retval) {
        com_err(progname, retval, _("while reading response from server"));
        exit(1);
    }
    /*
     * If we got an error response back from the server, display
     * the error message
     */
    if (krb5_is_krb_error(&inbuf)) {
        retval = krb5_rd_error(context, &inbuf, &error);
        if (retval) {
            com_err(progname, retval,
                    _("while decoding error response from server"));
            exit(1);
        }
        if (error->error == KRB_ERR_GENERIC) {
            if (error->text.data) {
                fprintf(stderr, _("Generic remote error: %s\n"),
                        error->text.data);
            }
        } else if (error->error) {
            com_err(progname,
                    (krb5_error_code)error->error + ERROR_TABLE_BASE_krb5,
                    _("signalled from server"));
            if (error->text.data) {
                fprintf(stderr, _("Error text from server: %s\n"),
                        error->text.data);
            }
        }
        krb5_free_error(context, error);
        exit(1);
    }

    retval = krb5_rd_safe(context,auth_context,&inbuf,&outbuf,NULL);
    if (retval) {
        com_err(progname, retval,
                "while decoding final size packet from server");
        exit(1);
    }

    retval = decode_database_size(&outbuf, &send_size);
    if (retval) {
        com_err(progname, retval, _("malformed sent database size message"));
        exit(1);
    }
    if (send_size != database_size) {
        com_err(progname, 0, _("Kpropd sent database size %"PRIu64
                               ", expecting %"PRIu64),
                send_size, database_size);
        exit(1);
    }
    free(inbuf.data);
    free(outbuf.data);
}

static void
send_error(krb5_context context, krb5_creds *my_creds, int fd, char *err_text,
           krb5_error_code err_code)
{
    krb5_error error;
    const char *text;
    krb5_data outbuf;

    memset(&error, 0, sizeof(error));
    krb5_us_timeofday(context, &error.ctime, &error.cusec);
    error.server = my_creds->server;
    error.client = my_principal;
    error.error = err_code - ERROR_TABLE_BASE_krb5;
    if (error.error > 127)
        error.error = KRB_ERR_GENERIC;
    text = (err_text != NULL) ? err_text : error_message(err_code);
    error.text.length = strlen(text) + 1;
    error.text.data = strdup(text);
    if (error.text.data) {
        if (!krb5_mk_error(context, &error, &outbuf)) {
            (void)krb5_write_message(context, &fd, &outbuf);
            krb5_free_data_contents(context, &outbuf);
        }
        free(error.text.data);
    }
}

static void
update_last_prop_file(char *hostname, char *file_name)
{
    char *file_last_prop;
    int fd;
    static char last_prop[] = ".last_prop";

    if (asprintf(&file_last_prop, "%s.%s%s", file_name, hostname,
                 last_prop) < 0) {
        com_err(progname, ENOMEM,
                _("while allocating filename for update_last_prop_file"));
        return;
    }
    fd = THREEPARAMOPEN(file_last_prop, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) {
        com_err(progname, errno, _("while creating 'last_prop' file, '%s'"),
                file_last_prop);
        free(file_last_prop);
        return;
    }
    write(fd, "", 1);
    free(file_last_prop);
    close(fd);
}
