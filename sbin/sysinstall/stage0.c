/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dkuug.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id$
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ncurses.h>
#include <dialog.h>

#include "sysinstall.h"

void
stage0()
{
	if (!access(README_FILE, R_OK)) {
		clear();
		dialog_textbox("READ ME FIRST", README_FILE, 25, 80);
	}
	if (!access(COPYRIGHT_FILE, R_OK)) {
		clear();
		dialog_textbox("COPYRIGHT", COPYRIGHT_FILE, 25, 80);
	}
}
