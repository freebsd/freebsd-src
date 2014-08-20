/*
 * Copyright 2008-2010 Chris Young <chris@unsatisfactorysoftware.co.uk>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "amiga/os3support.h"

#include "amiga/arexx.h"
#include "amiga/download.h"
#include "amiga/gui.h"
#include "amiga/hotlist.h"
#include "amiga/theme.h"
#include "utils/nsoption.h"

#include "desktop/browser_private.h"

#include <string.h>
#include <math.h>

#include <proto/intuition.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/clicktab.h>
#include <gadgets/clicktab.h>
#include <reaction/reaction_macros.h>

extern const char * const verarexx;
extern const char * const netsurf_version;
extern const char * const wt_revid;
extern const int netsurf_version_major;
extern const int netsurf_version_minor;

enum
{
	RX_OPEN=0,
	RX_QUIT,
	RX_TOFRONT,
	RX_GETURL,
	RX_GETTITLE,
	RX_VERSION,
	RX_SAVE,
	RX_PUBSCREEN,
	RX_BACK,
	RX_FORWARD,
	RX_HOME,
	RX_RELOAD,
	RX_WINDOWS,
	RX_ACTIVE,
	RX_CLOSE,
	RX_HOTLIST
};

STATIC char result[100];

STATIC VOID rx_open(struct ARexxCmd *, struct RexxMsg *);
STATIC VOID rx_quit(struct ARexxCmd *, struct RexxMsg *);
STATIC VOID rx_tofront(struct ARexxCmd *, struct RexxMsg *);
STATIC VOID rx_geturl(struct ARexxCmd *, struct RexxMsg *);
STATIC VOID rx_gettitle(struct ARexxCmd *, struct RexxMsg *);
STATIC VOID rx_version(struct ARexxCmd *, struct RexxMsg *);
STATIC VOID rx_save(struct ARexxCmd *, struct RexxMsg *);
STATIC VOID rx_pubscreen(struct ARexxCmd *, struct RexxMsg *);
STATIC VOID rx_back(struct ARexxCmd *, struct RexxMsg *);
STATIC VOID rx_forward(struct ARexxCmd *, struct RexxMsg *);
STATIC VOID rx_home(struct ARexxCmd *, struct RexxMsg *);
STATIC VOID rx_reload(struct ARexxCmd *, struct RexxMsg *);
STATIC VOID rx_windows(struct ARexxCmd *, struct RexxMsg *);
STATIC VOID rx_active(struct ARexxCmd *, struct RexxMsg *);
STATIC VOID rx_close(struct ARexxCmd *, struct RexxMsg *);
STATIC VOID rx_hotlist(struct ARexxCmd *, struct RexxMsg *);

STATIC struct ARexxCmd Commands[] =
{
	{"OPEN",RX_OPEN,rx_open,"URL/A,NEW=NEWWINDOW/S,NEWTAB/S,SAVEAS/K,W=WINDOW/K/N,T=TAB/K/N", 		0, 	NULL, 	0, 	0, 	NULL },
	{"QUIT",RX_QUIT,rx_quit,NULL, 		0, 	NULL, 	0, 	0, 	NULL },
	{"TOFRONT",RX_TOFRONT,rx_tofront,NULL, 		0, 	NULL, 	0, 	0, 	NULL },
	{"GETURL",RX_GETURL,rx_geturl,	"W=WINDOW/K/N,T=TAB/K/N", 		0, 	NULL, 	0, 	0, 	NULL },
	{"GETTITLE",RX_GETTITLE,rx_gettitle,	"W=WINDOW/K/N,T=TAB/K/N", 		0, 	NULL, 	0, 	0, 	NULL },
	{"VERSION",RX_VERSION,rx_version,"VERSION/N,SVN=REVISION/N,RELEASE/S", 		0, 	NULL, 	0, 	0, 	NULL },
	{"SAVE",RX_SAVE,rx_save,"FILENAME/A,W=WINDOW/K/N,T=TAB/K/N", 		0, 	NULL, 	0, 	0, 	NULL },
	{"GETSCREENNAME",RX_PUBSCREEN,rx_pubscreen,NULL, 		0, 	NULL, 	0, 	0, 	NULL },
	{"BACK",	RX_BACK,	rx_back,	"W=WINDOW/K/N,T=TAB/K/N", 		0, 	NULL, 	0, 	0, 	NULL },
	{"FORWARD",	RX_FORWARD,	rx_forward,	"W=WINDOW/K/N,T=TAB/K/N", 		0, 	NULL, 	0, 	0, 	NULL },
	{"HOME",	RX_HOME,	rx_home,	"W=WINDOW/K/N,T=TAB/K/N", 		0, 	NULL, 	0, 	0, 	NULL },
	{"RELOAD",	RX_RELOAD,	rx_reload,	"FORCE/S,W=WINDOW/K/N,T=TAB/K/N", 		0, 	NULL, 	0, 	0, 	NULL },
	{"WINDOWS",	RX_WINDOWS,	rx_windows,	"W=WINDOW/K/N", 		0, 	NULL, 	0, 	0, 	NULL },
	{"ACTIVE",	RX_ACTIVE,	rx_active,	"T=TAB/S", 		0, 	NULL, 	0, 	0, 	NULL },
	{"CLOSE",	RX_CLOSE,	rx_close,	"W=WINDOW/K/N,T=TAB/K/N", 		0, 	NULL, 	0, 	0, 	NULL },
	{"HOTLIST",	RX_HOTLIST,	rx_hotlist,	"A=ACTION/A", 		0, 	NULL, 	0, 	0, 	NULL },
	{ NULL, 		0, 				NULL, 		NULL, 		0, 	NULL, 	0, 	0, 	NULL }
};

BOOL ami_arexx_init(void)
{
	if(arexx_obj = ARexxObject,
			AREXX_HostName,"NETSURF",
			AREXX_Commands,Commands,
			AREXX_NoSlot,TRUE,
			AREXX_ReplyHook,NULL,
			AREXX_DefExtension,"nsrx",
			End)
	{
		GetAttr(AREXX_SigMask, arexx_obj, &rxsig);
		return true;
	}
	else
	{
/* Create a temporary ARexx port so we can send commands to the NetSurf which
 * is already running */
		arexx_obj = ARexxObject,
			AREXX_HostName,"NETSURF",
			AREXX_Commands,Commands,
			AREXX_NoSlot,FALSE,
			AREXX_ReplyHook,NULL,
			AREXX_DefExtension,"nsrx",
			End;
		return false;
	}
}

void ami_arexx_handle(void)
{
	RA_HandleRexx(arexx_obj);
}

void ami_arexx_execute(char *script)
{
	IDoMethod(arexx_obj, AM_EXECUTE, script, NULL, NULL, NULL, NULL, NULL);
}

void ami_arexx_cleanup(void)
{
	if(arexx_obj) DisposeObject(arexx_obj);
}

struct browser_window *ami_find_tab_gwin(struct gui_window_2 *gwin, int tab)
{
	int tabs = 0;
	struct Node *ctab;
	struct Node *ntab;
	struct browser_window *bw;

	if((tab == 0) || (gwin->tabs == 0)) return gwin->bw;

	ctab = GetHead(&gwin->tab_list);

	do
	{
		tabs++;
		ntab=GetSucc(ctab);
		GetClickTabNodeAttrs(ctab,
							TNA_UserData, &bw,
							TAG_DONE);
		if(tabs == tab) return bw;
	} while(ctab=ntab);

	return NULL;
}

int ami_find_tab_bw(struct gui_window_2 *gwin, struct browser_window *bw)
{
	int tabs = 0;
	struct Node *ctab;
	struct Node *ntab;
	struct browser_window *tbw = NULL;

	if((bw == NULL) || (gwin->tabs == 0)) return 1;

	ctab = GetHead(&gwin->tab_list);

	do
	{
		tabs++;
		ntab=GetSucc(ctab);
		GetClickTabNodeAttrs(ctab,
							TNA_UserData, &tbw,
							TAG_DONE);
		if(tbw == bw) return tabs;
	} while(ctab=ntab);

	return 0;
}

struct browser_window *ami_find_tab(int window, int tab)
{
	struct nsObject *node, *nnode;

	if(!IsMinListEmpty(window_list))
	{
		int windows = 0;

		node = (struct nsObject *)GetHead((struct List *)window_list);

		do
		{
			nnode=(struct nsObject *)GetSucc((struct Node *)node);

			if(node->Type == AMINS_WINDOW)
			{
				windows++;
				if(windows == window)
					return ami_find_tab_gwin(node->objstruct, tab);
			}
		} while(node = nnode);
	}
	return NULL;
}

STATIC VOID rx_open(struct ARexxCmd *cmd, struct RexxMsg *rxm __attribute__((unused)))
{
	struct dlnode *dln;
	struct browser_window *bw = curbw;
	nsurl *url;

	cmd->ac_RC = 0;

	if((cmd->ac_ArgList[4]) && (cmd->ac_ArgList[5]))
		bw = ami_find_tab(*(ULONG *)cmd->ac_ArgList[4], *(ULONG *)cmd->ac_ArgList[5]);

	if (nsurl_create((char *)cmd->ac_ArgList[0], &url) != NSERROR_OK) {
		warn_user("NoMemory", 0);
		return;
	}

	if(cmd->ac_ArgList[3])
	{
		if(!bw) return;

		dln = AllocVecTags(sizeof(struct dlnode), AVT_ClearWithValue, 0, TAG_DONE);
		dln->filename = strdup((char *)cmd->ac_ArgList[3]);
		dln->node.ln_Name = strdup((char *)cmd->ac_ArgList[0]);
		dln->node.ln_Type = NT_USER;
		AddTail(&bw->window->dllist, (struct Node *)dln);
		if(!bw->download) {
			browser_window_navigate(curbw,
					url,
					NULL,
					BW_NAVIGATE_DOWNLOAD,
					NULL,
					NULL,
					NULL);
		}
	}
	else if(cmd->ac_ArgList[2])
	{
		browser_window_create(BW_CREATE_HISTORY |
				      BW_CREATE_TAB,
				      url,
				      NULL,
				      bw,
				      NULL);
	}
	else if(cmd->ac_ArgList[1])
	{
		browser_window_create(BW_CREATE_HISTORY,
				      url,
				      NULL,
				      NULL,
				      NULL);
	}
	else
	{
		if(bw)
		{
			browser_window_navigate(bw,
					url,
					NULL,
					BW_NAVIGATE_HISTORY,
					NULL,
					NULL,
					NULL);
		}
		else
		{
			browser_window_create(BW_CREATE_HISTORY,
					      url,
					      NULL,
					      NULL,
					      NULL);
		}
	}
	nsurl_unref(url);
}

STATIC VOID rx_save(struct ARexxCmd *cmd, struct RexxMsg *rxm __attribute__((unused)))
{
	BPTR fh = 0;
	ULONG source_size;
	const char *source_data;
	struct browser_window *bw = curbw;

	cmd->ac_RC = 0;

	if((cmd->ac_ArgList[1]) && (cmd->ac_ArgList[2]))
		bw = ami_find_tab(*(ULONG *)cmd->ac_ArgList[1], *(ULONG *)cmd->ac_ArgList[2]);

	if(!bw) return;

	ami_set_pointer(bw->window->shared, GUI_POINTER_WAIT, false);
					
	if(fh = FOpen((char *)cmd->ac_ArgList[0], MODE_NEWFILE, 0))
	{
		if(source_data = content_get_source_data(bw->current_content, &source_size))
			FWrite(fh, source_data, 1, source_size);

		FClose(fh);
		SetComment((char *)cmd->ac_ArgList[0], nsurl_access(hlcache_handle_get_url(bw->current_content)));
	}

	ami_reset_pointer(bw->window->shared);
}

STATIC VOID rx_quit(struct ARexxCmd *cmd, struct RexxMsg *rxm __attribute__((unused)))
{
	cmd->ac_RC = 0;
	ami_quit_netsurf();
}

STATIC VOID rx_tofront(struct ARexxCmd *cmd, struct RexxMsg *rxm __attribute__((unused)))
{
	cmd->ac_RC = 0;
	ScreenToFront(scrn);
}

STATIC VOID rx_geturl(struct ARexxCmd *cmd, struct RexxMsg *rxm __attribute__((unused)))
{
	struct browser_window *bw = curbw;

	cmd->ac_RC = 0;

	if((cmd->ac_ArgList[0]) && (cmd->ac_ArgList[1]))
		bw = ami_find_tab(*(ULONG *)cmd->ac_ArgList[0], *(ULONG *)cmd->ac_ArgList[1]);

	if(bw && bw->current_content)
	{
		strcpy(result, nsurl_access(hlcache_handle_get_url(bw->current_content)));
	}
	else
	{
		strcpy(result,"");
	}

	cmd->ac_Result = result;
}

STATIC VOID rx_gettitle(struct ARexxCmd *cmd, struct RexxMsg *rxm __attribute__((unused)))
{
	struct browser_window *bw = curbw;

	cmd->ac_RC = 0;

	if((cmd->ac_ArgList[0]) && (cmd->ac_ArgList[1]))
		bw = ami_find_tab(*(ULONG *)cmd->ac_ArgList[0], *(ULONG *)cmd->ac_ArgList[1]);

	if(bw)
	{
		if(bw->window->shared->tabs > 1)
			strcpy(result, bw->window->tabtitle);
		else
			strcpy(result, bw->window->shared->wintitle);
	}
	else
	{
		strcpy(result,"");
	}

	cmd->ac_Result = result;
}

STATIC VOID rx_version(struct ARexxCmd *cmd, struct RexxMsg *rxm __attribute__((unused)))
{
	cmd->ac_RC = 0;

	if(cmd->ac_ArgList[2])
	{
		if(cmd->ac_ArgList[1])
		{
			if((netsurf_version_major > *(ULONG *)cmd->ac_ArgList[0]) || ((netsurf_version_minor >= *(ULONG *)cmd->ac_ArgList[1]) && (netsurf_version_major == *(ULONG *)cmd->ac_ArgList[0])))
			{
				strcpy(result,"1");
			}
			else
			{
				strcpy(result,"0");
			}
		}
		else if(cmd->ac_ArgList[0])
		{
			if((netsurf_version_major >= *(ULONG *)cmd->ac_ArgList[0]))
			{
				strcpy(result,"1");
			}
			else
			{
				strcpy(result,"0");
			}
		}
		else
		{
			strcpy(result,netsurf_version);
		}
	}
	else
	{
		if(cmd->ac_ArgList[1])
		{
			if((netsurf_version_major > *(ULONG *)cmd->ac_ArgList[0]) || ((atoi(wt_revid) >= *(ULONG *)cmd->ac_ArgList[1]) && (netsurf_version_major == *(ULONG *)cmd->ac_ArgList[0])))
			{
				strcpy(result,"1");
			}
			else
			{
				strcpy(result,"0");
			}
		}
		else if(cmd->ac_ArgList[0])
		{
			if((netsurf_version_major >= *(ULONG *)cmd->ac_ArgList[0]))
			{
				strcpy(result,"1");
			}
			else
			{
				strcpy(result,"0");
			}
		}
		else
		{
			strcpy(result,verarexx);
		}
	}

	cmd->ac_Result = result;
}

STATIC VOID rx_pubscreen(struct ARexxCmd *cmd, struct RexxMsg *rxm __attribute__((unused)))
{
	cmd->ac_RC = 0;

	if(nsoption_charp(pubscreen_name) == NULL)
	{
		strcpy(result,"NetSurf");
	}
	else
	{
		strcpy(result, nsoption_charp(pubscreen_name));
	}

	cmd->ac_Result = result;
}

STATIC VOID rx_back(struct ARexxCmd *cmd, struct RexxMsg *rxm __attribute__((unused)))
{
	struct browser_window *bw = curbw;

	cmd->ac_RC = 0;

	if((cmd->ac_ArgList[0]) && (cmd->ac_ArgList[1]))
		bw = ami_find_tab(*(ULONG *)cmd->ac_ArgList[0], *(ULONG *)cmd->ac_ArgList[1]);

	if(bw) ami_gui_history(bw->window->shared, true);
}

STATIC VOID rx_forward(struct ARexxCmd *cmd, struct RexxMsg *rxm __attribute__((unused)))
{
	struct browser_window *bw = curbw;

	cmd->ac_RC = 0;

	if((cmd->ac_ArgList[0]) && (cmd->ac_ArgList[1]))
		bw = ami_find_tab(*(ULONG *)cmd->ac_ArgList[0], *(ULONG *)cmd->ac_ArgList[1]);

	if(bw) ami_gui_history(bw->window->shared, false);

}

STATIC VOID rx_home(struct ARexxCmd *cmd, struct RexxMsg *rxm __attribute__((unused)))
{
	struct browser_window *bw = curbw;
	nsurl *url;

	cmd->ac_RC = 0;

	if((cmd->ac_ArgList[0]) && (cmd->ac_ArgList[1]))
		bw = ami_find_tab(*(ULONG *)cmd->ac_ArgList[0], *(ULONG *)cmd->ac_ArgList[1]);

	if(bw == NULL) return;

	if (nsurl_create(nsoption_charp(homepage_url), &url) != NSERROR_OK) {
		warn_user("NoMemory", 0);
	} else {
		browser_window_navigate(bw,
					url,
					NULL,
					BW_NAVIGATE_HISTORY,
					NULL,
					NULL,
					NULL);
		nsurl_unref(url);
	}
}

STATIC VOID rx_reload(struct ARexxCmd *cmd, struct RexxMsg *rxm __attribute__((unused)))
{
	struct browser_window *bw = curbw;

	cmd->ac_RC = 0;

	if((cmd->ac_ArgList[1]) && (cmd->ac_ArgList[2]))
		bw = ami_find_tab(*(ULONG *)cmd->ac_ArgList[1], *(ULONG *)cmd->ac_ArgList[2]);

	if(bw)
	{
		if(cmd->ac_ArgList[0]) /* FORCE */
		{
			browser_window_reload(bw, true);
		}
		else
		{
			browser_window_reload(bw, false);
		}
	}
}

STATIC VOID rx_windows(struct ARexxCmd *cmd, struct RexxMsg *rxm __attribute__((unused)))
{
	int windows = 0, tabs = 0;
	struct nsObject *node, *nnode;
	struct gui_window_2 *gwin;

	cmd->ac_RC = 0;

	if(!IsMinListEmpty(window_list))
	{
		node = (struct nsObject *)GetHead((struct List *)window_list);

		do
		{
			nnode=(struct nsObject *)GetSucc((struct Node *)node);

			gwin = node->objstruct;

			if(node->Type == AMINS_WINDOW)
			{
				windows++;
				if((cmd->ac_ArgList[0]) && (*(ULONG *)cmd->ac_ArgList[0] == windows))
					tabs = gwin->tabs;
			}
		} while(node = nnode);
	}

	if(cmd->ac_ArgList[0]) sprintf(result, "%ld", tabs);
		else sprintf(result, "%ld", windows);
	cmd->ac_Result = result;
}

STATIC VOID rx_active(struct ARexxCmd *cmd, struct RexxMsg *rxm __attribute__((unused)))
{
	int window = 0, tab = 0;
	struct browser_window *bw = curbw;
	struct nsObject *node, *nnode;
	struct gui_window_2 *gwin = NULL;

	cmd->ac_RC = 0;

	if(!IsMinListEmpty(window_list))
	{
		int windows = 0;

		node = (struct nsObject *)GetHead((struct List *)window_list);

		do
		{
			nnode=(struct nsObject *)GetSucc((struct Node *)node);

			gwin = node->objstruct;

			if(node->Type == AMINS_WINDOW)
			{
				windows++;
				if(gwin->bw == bw)
				{
					window = windows;
					break;
				}
			}
		} while(node = nnode);
	}

	if(cmd->ac_ArgList[0])
	{
		tab = ami_find_tab_bw(gwin, bw);
	}

	if(cmd->ac_ArgList[0]) sprintf(result, "%ld", tab);
		else sprintf(result, "%ld", window);
	cmd->ac_Result = result;
}

STATIC VOID rx_close(struct ARexxCmd *cmd, struct RexxMsg *rxm __attribute__((unused)))
{
	struct browser_window *bw = curbw;

	cmd->ac_RC = 0;

	if((cmd->ac_ArgList[0]) && (cmd->ac_ArgList[1]))
		bw = ami_find_tab(*(ULONG *)cmd->ac_ArgList[0], *(ULONG *)cmd->ac_ArgList[1]);
	else if(cmd->ac_ArgList[0])
	{
		ami_close_all_tabs(bw->window->shared);
		return;
	}

	if(bw) browser_window_destroy(bw);
}

STATIC VOID rx_hotlist(struct ARexxCmd *cmd, struct RexxMsg *rxm __attribute__((unused)))
{
	cmd->ac_RC = 0;

	if(strcasecmp((char *)cmd->ac_ArgList[0], "OPEN") == 0) {
		ami_tree_open(hotlist_window, AMI_TREE_HOTLIST);
	} else if(strcasecmp((char *)cmd->ac_ArgList[0], "CLOSE") == 0) {
		ami_tree_close(hotlist_window);
	}
}
