/*-
 * Copyright (c) 1998 Kazutaka YOKOTA (yokota@zodiac.mech.utsunomiya-u.ac.jp)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote 
 *    products derived from this software without specific prior written 
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHRO AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHRO OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/release/sysinstall/mouse.c,v 1.7.2.1 2000/07/14 07:51:47 jhb Exp $
 */

#include "sysinstall.h"
#include <string.h>

int
mousedTest(dialogMenuItem *self)
{
	char *type;
	char *port;
	char *flags;
	int ret;

	type = variable_get(VAR_MOUSED_TYPE);
	port = variable_get(VAR_MOUSED_PORT);
	flags = variable_get(VAR_MOUSED_FLAGS);
	if ((type == NULL) || (port == NULL) 
		|| (strlen(type) <= 0) || (strlen(port) <= 0)
		|| (strcmp(type, "NO") == 0)) {
		msgConfirm("Please select a mouse protocol and a port first.");
		return DITEM_FAILURE;
	}

	msgNotify("Trying to start the mouse daemon...");
	if (file_readable("/var/run/moused.pid"))
	    vsystem("kill `cat /var/run/moused.pid`");
	systemExecute("vidcontrol -m on");
	if (flags != NULL)
	    vsystem("moused -t %s -p %s %s", type, port, flags);
	else
	    vsystem("moused -t %s -p %s", type, port);

	ret = msgYesNo("Now move the mouse and see if it works.\n"
	      "(Note that buttons don't have any effect for now.)\n\n"
	      "         Is the mouse cursor moving?\n");
	systemExecute("vidcontrol -m off");
	if (ret) {
		if (file_readable("/var/run/moused.pid"))
		    vsystem("kill `cat /var/run/moused.pid`");
		variable_set2(VAR_MOUSED, "NO", 1);
	} else {
		variable_set2(VAR_MOUSED, "YES", 1);
		vsystem("ln -fs /dev/sysmouse /dev/mouse"); /* backwards compat */
	}

	return DITEM_SUCCESS | DITEM_RESTORE;
}

int
mousedDisable(dialogMenuItem *self)
{
	if (file_readable("/var/run/moused.pid"))
	    vsystem("kill `cat /var/run/moused.pid`");
	variable_set2(VAR_MOUSED, "NO", 1);
	variable_set2(VAR_MOUSED_TYPE, "NO", 1);
	variable_unset(VAR_MOUSED_PORT);
	variable_unset(VAR_MOUSED_FLAGS);
	msgConfirm("The mouse daemon is disabled.");
	return DITEM_SUCCESS;
}

int 
setMouseFlags(dialogMenuItem *self)
{
    int ret;
    ret = variable_get_value(VAR_MOUSED_FLAGS,
			     "Please Specify the mouse daemon flags.  If you would like to\n"
			     "emulate 3 buttons, use -3 here.\n", 1)
	? DITEM_SUCCESS : DITEM_FAILURE;
    if (ret != DITEM_SUCCESS)
	variable_unset(VAR_MOUSED_FLAGS);
    return ret;
}

