#include <windows.h>

int (*Lcom_err)(LPSTR,long,LPSTR,...);
LPSTR (*Lerror_message)(long);
LPSTR (*Lerror_table_name)(long);

void Leash_load_com_err_callback(FARPROC ce,
                                 FARPROC em,
                                 FARPROC etn)
{
    (FARPROC)Lcom_err=ce;
    (FARPROC)Lerror_message=em;
    (FARPROC)Lerror_table_name=etn;
}
