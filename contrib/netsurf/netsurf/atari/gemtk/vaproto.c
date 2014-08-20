#include <limits.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <gem.h>

#include "vaproto.h"
#include "gemtk.h"

#ifndef NDEBUG
# define DEBUG_PRINT(x) 	printf x
#else
# define DEBUG_PRINT(x)
#endif


/* Global memory, at least 1024 bytes large: */
static char *va_helpbuf;

/* Desktop's AES ID: */
static short av_shell_id     = -1;

/* What AV commands can desktop do? */
static short av_shell_status = 0;

/* The application name used for AV/VA messages: */
static const char * av_clientname = "gemtk_av_app";


static short get_avserver(void)
{
	short        ret    = -100;
	const char * av_env = getenv("AVSERVER");
	if (av_env) {
		char av_envname[9];
		strncpy(av_envname,av_env, 8);
		av_envname[8] = '\0';
		while (strlen (av_envname) < 8) {
			strcat(av_envname, " ");
		}
		ret = appl_find (av_envname);
	}
	return ret;
}

/**
 * initialitze the AV client API
 * \param appname Name of the application passed to menu_register()
 * \return returns 1 on success, otherwise an negative error code.
 *
 */
int gemtk_av_init(const char *appname)
{
    short mode = 0x0003     /* prefer TT ram    */
                | 0x0020;   /* global accesible */

    if(av_shell_id != -1) {
        /* Already initialized */
        return(1);
    }

    va_helpbuf = (char*)Mxalloc(1024, mode);

    if(va_helpbuf == NULL){
        gemtk_msg_box_show(GEMTK_MSG_BOX_ALERT, "Could not allocate AV memory!");
        return(-1);
    }

    if(appname != NULL){
        av_clientname = appname;
    }

    av_shell_id = get_avserver();
    DEBUG_PRINT(("AV Server ID: %d", av_shell_id));

	gemtk_av_send(AV_PROTOKOLL, NULL, NULL);

    va_helpbuf[0] = '\0';
}

void gemtk_av_exit(void)
{
    if(av_shell_id == -1) {
        /* Nothing to do */
        return;
    }

    if (av_shell_status & AA_EXIT) {
        /* AV server knows AV_EXIT */
		gemtk_av_send(AV_EXIT, NULL, NULL);
	}

    if(va_helpbuf != NULL){
        free(va_helpbuf);
        va_helpbuf = NULL;
    }

    av_shell_id = -1;

}

bool gemtk_av_send (short message, const char * data1, const char * data2)
{
	short msg[8];
	short to_ap_id = av_shell_id;

	/* - 100 to ap id would be no AV server */
	if (to_ap_id == -100){
	    return false;
	}

	msg[0] = message;
	msg[1] = gl_apid;
	msg[7] = msg[6] = msg[5] = msg[4] = msg[3] = msg[2] = 0;

	switch (message)
	{
		case AV_EXIT:
			msg[3] = gl_apid;
			break;
		case AV_PROTOKOLL:
			msg[3] = VV_START | VV_ACC_QUOTING;
			*(char **)(msg+6) = strcpy (va_helpbuf, av_clientname);
			break;
		case AV_STARTPROG:
            DEBUG_PRINT(("AV_STARTPROG: %s (%s)\n", data1, data2));
			*(char **)(msg+3) = strcpy(va_helpbuf, data1);
			*(char **)(msg+5) = strcpy(va_helpbuf, data2);
			break;
		case AV_VIEW:
            DEBUG_PRINT(("AV_VIEW: %s (%d)\n", data1, (short)data2));
			*(char **)(msg+3) = strcpy(va_helpbuf, data1);
			msg[5] = (short)data2;
			break;
        default:
			return false; /* not supported */
	}

	return (appl_write (to_ap_id, 16, msg) > 0);
}

bool gemtk_av_dispatch (short msg[8])
{

    if(av_shell_id == -1)
        return(false);

	switch (msg[0]) {
		case VA_PROTOSTATUS :
            DEBUG_PRINT(("AV STATUS: %d for %d\n", msg[3], msg[1]));
			if (msg[1] == av_shell_id) {
				av_shell_status = msg[3];
				if(av_shell_status & AA_STARTPROG){
                    printf(" AA_STARTPROG\n");
				}
			}
			break;

        default:
            DEBUG_PRINT(("Unknown AV message: %d", msg[0]));
            break;
    }

	return(true);
}


