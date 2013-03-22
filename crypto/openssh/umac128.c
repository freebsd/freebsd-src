/* $FreeBSD$ */
#define UMAC_OUTPUT_LEN		16
#define umac_new		ssh_umac128_new
#define umac_update		ssh_umac128_update
#define umac_final		ssh_umac128_final
#define umac_delete		ssh_umac128_delete
#include "umac.c"
