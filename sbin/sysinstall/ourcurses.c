/* Stopgap, until Paul does the right thing */
#define ESC 27
#define TAB 9

#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <string.h>

#include <string.h>
#include <dialog.h>
#include "sysinstall.h"

int
AskEm(WINDOW *w,char *prompt, char *answer, int len)
{
    int x,y;
    mvwprintw(w,23,0,prompt);
    getyx(w,y,x);
    wclrtoeol(w);
    return line_edit(w,y,x,len,len+1,item_selected_attr,1,answer);
}

void
ShowFile(char *filename, char *header)
{
    char buf[256];
    if (access(filename, R_OK)) {
	sprintf(buf, "The %s file is not provided on the 1.2MB floppy image.", filename);
	dialog_msgbox("Sorry!", buf, -1, -1, 1);
	dialog_clear_norefresh();
	return;
    }
    dialog_clear_norefresh();
    dialog_textbox(header, filename, LINES, COLS);
    dialog_clear_norefresh();
}

