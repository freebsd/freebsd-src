/* $FreeBSD$ */
#define UMAC_OUTPUT_LEN		16
#undef umac_new
#define umac_new		ssh_umac128_new
#undef umac_update
#define umac_update		ssh_umac128_update
#undef umac_final
#define umac_final		ssh_umac128_final
#undef umac_delete
#define umac_delete		ssh_umac128_delete
#include "umac.c"
