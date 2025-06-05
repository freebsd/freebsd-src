// ATTENTION: someone in the past edited this file manually
// I am continuing this tradition just to get the release out. 3/6/97
// This needs to be revisited and repaired!!!XXXX
// pbh


 /*
 * leasherr.c
 * This file is the C file for leasherr.et.
 * Please do not edit it as it is automatically generated.
 */

#include <stdlib.h>
#include <windows.h>

static const char* const text[] = {
	"Only one instance of Leash can be run at a time.",
	"Principal invalid.",
	"Realm failed.",
	"Instance invalid.",
	"Realm invalid.",
	"Unexpected end of Kerberos memory storage.",
	"Warning! Your Kerberos tickets expire soon.",
	"You did not type the same new password.",
	"You can only use printable characters in a password.",
	"Fatal error; cannot run this program.",
	"Couldn't initialize WinSock.",
	"Couldn't find the timeserver host entry.",
	"Couldn't open a socket.",
	"Couldn't connect to timeserver.",
	"Couldn't get time from server.",
	"Couldn't get local time of day.",
	"Couldn't set local time of day.",
	"Error while receiving time from server.",
	"Protocol problem with timeserver.",
	"The time was already reset. Try using a different program to synchronize the time.",
    0
};

typedef LPSTR (*err_func)(int, long);
struct error_table {
    char const * const * msgs;
    err_func func;
    long base;
    int n_msgs;
};
struct et_list {
#ifdef WINDOWS
	HANDLE next;
#else
	struct et_list *next;
#endif
	const struct error_table * table;
};

static const struct error_table et = { text, (err_func)0, 40591872L, 20 };

#ifdef WINDOWS
void initialize_lsh_error_table(HANDLE *__et_list) {
//    struct et_list *_link,*_et_list;
    struct et_list *_link;
    HANDLE ghlink;

    ghlink=GlobalAlloc(GHND,sizeof(struct et_list));
    _link=GlobalLock(ghlink);
    _link->next=*__et_list;
    _link->table=&et;
    GlobalUnlock(ghlink);
    *__et_list=ghlink;
}
#else
void initialize_lsh_error_table(struct et_list **__et_list) {
    struct et_list *_link;

    _link=malloc(sizeof(struct et_list));
    _link->next=*__et_list;
    _link->table=&et;
    *__et_list=_link;
}
#endif

#ifdef WINDOWS

void Leash_initialize_krb_error_func(err_func func, HANDLE *__et_list)
{
}

void Leash_initialize_kadm_error_table(HANDLE *__et_list)
{
}
#else
#include <krberr.h>

void Leash_initialize_krb_error_func(err_func func, struct et_list **__et_list)
{
    (*pinitialize_krb_error_func)(func,__et_list);
}

#include <kadm_err.h>

void Leash_initialize_kadm_error_table(struct et_list **__et_list)
{
    initialize_kadm_error_table(__et_list);
}
#endif
