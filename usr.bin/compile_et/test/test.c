#include <stdio.h>
#include <errno.h>
#include "test1.h"
#include "test2.h"
char *error_message();
extern int sys_nerr, errno;

main()
{
	printf("\nBefore initiating error table:\n\n");
	printf("Table name '%s'\n", error_table_name(KRB_MK_AP_TGTEXP));
	printf("UNIX  name '%s'\n", error_table_name(EPERM));
	printf("Msg TGT-expired is '%s'\n", error_message(KRB_MK_AP_TGTEXP));
	printf("Msg EPERM is '%s'\n", error_message(EPERM));
	printf("Msg FOO_ERR is '%s'\n", error_message(FOO_ERR));
	printf("Msg {sys_nerr-1} is '%s'\n", error_message(sys_nerr-1));
	printf("Msg {sys_nerr} is '%s'\n", error_message(sys_nerr));

	init_error_table(0, 0, 0);
	printf("With 0: tgt-expired -> %s\n", error_message(KRB_MK_AP_TGTEXP));

	init_krb_err_tbl();
	printf("KRB error table initialized:  base %d (%s), name %s\n",
	       krb_err_base, error_message(krb_err_base),
	       error_table_name(krb_err_base));
	printf("With krb: tgt-expired -> %s\n",
	       error_message(KRB_MK_AP_TGTEXP));

	init_quux_err_tbl();
	printf("QUUX error table initialized: base %d (%s), name %s\n",
	       quux_err_base, error_message(quux_err_base),
	       error_table_name(quux_err_base));

	printf("Msg for TGT-expired is '%s'\n",
	       error_message(KRB_MK_AP_TGTEXP));
	printf("Msg {sys_nerr-1} is '%s'\n", error_message(sys_nerr-1));
	printf("Msg FOO_ERR is '%s'\n", error_message(FOO_ERR));
	printf("Msg KRB_SKDC_CANT is '%s'\n",
		    error_message(KRB_SKDC_CANT));
	printf("Msg 1e6 is '%s'\n", error_message(1000000));
	errno = FOO_ERR;
	perror("FOO_ERR");
}
