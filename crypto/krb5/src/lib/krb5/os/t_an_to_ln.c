/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#include "krb5.h"

#include <stdio.h>

int
main(int argc, char **argv)
{
    krb5_error_code     kret = 0;
    krb5_context        kcontext;
    krb5_principal      principal;
    char                *programname;
    int                 i;
    char                sbuf[1024];

    programname = argv[0];
    krb5_init_context(&kcontext);
    for (i=1; i < argc; i++) {
        if (!(kret = krb5_parse_name(kcontext, argv[i], &principal))) {
            if (!(kret = krb5_aname_to_localname(kcontext,
                                                 principal,
                                                 1024,
                                                 sbuf))) {
                printf("%s: aname_to_lname maps %s -> <%s>\n",
                       programname, argv[i], sbuf);
            }
            else {
                printf("%s: aname to lname returns %s for %s\n", programname,
                       error_message(kret), argv[i]);
            }
            krb5_free_principal(kcontext, principal);
        }
        else {
            printf("%s: parse_name returns %s\n", programname,
                   error_message(kret));
        }
        if (kret)
            break;
    }
    krb5_free_context(kcontext);
    return((kret) ? 1 : 0);
}
