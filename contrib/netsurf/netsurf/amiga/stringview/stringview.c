/*
 * Copyright 2009 Rene W. Olsen <ac@rebels.com>
 * Copyright 2009 Stephen Fellner <sf.amiga@gmail.com>
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

/// Include

#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/layout.h>
#include <proto/listbrowser.h>
#include <proto/utility.h>
#include <proto/string.h>
#include <proto/window.h>

#include <classes/window.h>
#include <gadgets/layout.h>
#include <gadgets/listbrowser.h>

#include "amiga/os3support.h"

#include "stringview.h"
#include "urlhistory.h"

#include <string.h>
#include <stdio.h>

#define End TAG_END)

///

/// Proto

static void myStringOpenListview( Class *cl, Object *obj, struct gpInput *msg );
static void myStringCloseListview( Class *cl, Object *obj );
static uint32 myStringSearch( Class *cl, Object *obj );
static void myStringArrowUp( Class *cl, Object *obj );
static void myStringArrowDown( Class *cl, Object *obj );
static void myStringHandleListview( Class *cl, Object *obj );

///

/* -- Internal -- */

/// myStringOpenListview

static void myStringOpenListview( Class *cl, Object *obj, struct gpInput *msg )
{
struct myStringClassData *data;
struct Gadget *gad;

	data = INST_DATA( cl, obj );

	gad = (struct Gadget *)obj;

	SetAttrs( data->WindowObject,
		WA_CustomScreen, msg->gpi_GInfo->gi_Window->WScreen,
		WA_Left,	data->WinXPos,
		WA_Top,		data->WinYPos,
		WA_Width,	data->WinWidth,
		WA_Height,	data->WinHeight,
		TAG_END
	);

//	  IDoMethod( data->WindowObject, WM_RETHINK );

	data->Window = (struct Window *)IDoMethod( data->WindowObject, WM_OPEN );

	if ( data->Window == NULL )
	{
		goto bailout;
	}

//	  GetAttr( WINDOW_SigMask, MainWindowObject, &MainWindowBits );

bailout:

	return;
}

///
/// myStringCloseListview

static void myStringCloseListview( Class *cl, Object *obj )
{
struct myStringClassData *data;
struct Node *node;

	data = INST_DATA( cl, obj );

	if ( data->Window )
    {
		IDoMethod( data->WindowObject, WM_CLOSE );
		data->Window = NULL;
	}

	while(( node = RemHead( &data->ListviewHeader )))
	{
		FreeListBrowserNode( node );
	}
}

///
/// myStringSearch

static uint32 myStringSearch( Class *cl, Object *obj )
{
	struct myStringClassData *data;
	struct Window *win;
	struct Node *node;
	struct Node *n;
	uint32 found;
	uint32 bufpos;
	STRPTR searchString;
	STRPTR compString;

	found = 0;

	data = INST_DATA( cl, obj );

	win = data->Window;

	// Remove List and Free Nodes

    SetGadgetAttrs( (struct Gadget *)data->ListviewObject, win, NULL,
	    LISTBROWSER_Labels, ~0,
	    TAG_END
    );

	while(( node = RemHead( &data->ListviewHeader )))
	{
		FreeListBrowserNode( node );
	}

	GetAttr( STRINGA_BufferPos, obj, &bufpos );

	if ( bufpos == 0 )
	{
		goto bailout;
	}

//-------------

	searchString = strstr(data->SearchBuffer, "://");
	if(searchString)
	{
		searchString += 3;
		if (bufpos >= searchString - data->SearchBuffer)
			bufpos -= searchString - data->SearchBuffer;
	}
	else
		searchString = data->SearchBuffer;

	node = GetHead( data->SearchHeader );

	while( node )
	{
		uint32 srcpos;
		BOOL possible;

		possible = FALSE;
		srcpos = 0;

		compString = strstr(node->ln_Name, "://");
		if(compString)
			compString += 3;
		else
			compString = node->ln_Name;

		if( 0 == strncasecmp( compString, searchString, bufpos ) )
		{
			// found match after protocol
			possible = TRUE;
		}
		else
		{
			// no match after protocol, see if there's a match after www
			if( 0 == strncasecmp( compString, "www.", 4) ) {
				// got www, compare it!
				if( 0 == strncasecmp( &compString[4], searchString, bufpos ) )
					possible = TRUE;
			}
		}

		if ( possible == TRUE )
		{
			n = AllocListBrowserNode( 1,
				LBNA_Column,    0,
					LBNCA_CopyText, TRUE,
					LBNCA_Text,     node->ln_Name,
				TAG_END
			);

			if ( n )
			{
				AddTail( &data->ListviewHeader, n );
				found++;
			}
		}

		node = GetSucc( node );
	}

//-------------

bailout:

	data->ListviewCount = found;
	data->ListviewSelected = -1;

	// Add List Again

	RefreshSetGadgetAttrs( (struct Gadget *)data->ListviewObject, win, NULL,
	    LISTBROWSER_Labels, &data->ListviewHeader,
		LISTBROWSER_Selected, data->ListviewSelected,
		LISTBROWSER_MakeVisible, 0,
	    TAG_END
    );

	return( found );
}

///
/// myStringArrowUp

static void myStringArrowUp( Class *cl, Object *obj )
{
struct myStringClassData *data;
struct Window *win;
//struct Node *node;
//uint32 cnt;

	data = INST_DATA( cl, obj );

	win = data->Window;

	if ( data->ListviewCount == 0 )
	{
		data->ListviewSelected = -1;
		goto bailout;
	}
	else if (( data->ListviewSelected != -1 ) && ( data->ListviewSelected != 0 ))
	{
		data->ListviewSelected--;
	}

	RefreshSetGadgetAttrs( (struct Gadget *)data->ListviewObject, win, NULL,
		LISTBROWSER_Selected, data->ListviewSelected,
		LISTBROWSER_MakeVisible, data->ListviewSelected,
	    TAG_END
    );

//	  cnt = data->ListviewSelected;
//	  node = GetHead( data->SearchHeader );
//
//	  while( cnt-- > 0 )
//	  {
//		  node = GetSucc( node );
//	  }
//
//	  if ( node )
//	  {
//		  ISetSuperAttrs( obj,
//
//			  TAG_END
//		  );
//	  }

bailout:

	return;
}

///
/// myStringArrowDown

static void myStringArrowDown( Class *cl, Object *obj )
{
struct myStringClassData *data;
struct Window *win;

	data = INST_DATA( cl, obj );

	win = data->Window;

	if ( data->ListviewCount == 0 )
	{
		data->ListviewSelected = -1;
	}
	else if ( data->ListviewSelected == -1 )
	{
		data->ListviewSelected = 0;
	}
	else if ( data->ListviewSelected != data->ListviewCount - 1 )
	{
			data->ListviewSelected++;
	}

	RefreshSetGadgetAttrs( (struct Gadget *)data->ListviewObject, win, NULL,
		LISTBROWSER_Selected, data->ListviewSelected,
		LISTBROWSER_MakeVisible, data->ListviewSelected,
	    TAG_END
    );
}

///
/// myStringHandleListview

static void myStringHandleListview( Class *cl, Object *obj )
{
struct myStringClassData *data;
uint32 result;
uint16 code;

	data = INST_DATA( cl, obj );

	while(( result = IDoMethod( data->WindowObject, WM_HANDLEINPUT, &code )) != WMHI_LASTMSG )
    {
//        switch( result & WMHI_CLASSMASK )
//        {
//            case WMHI_CLOSEWINDOW:
//            {
//				  running = FALSE;
//                break;
//            }
//
//            default:
//            {
//                break;
//            }
//        }
    }
}

///

/* BOOPSI methods */

/// myStringClass_OM_New

uint32 myStringClass_OM_New( Class *cl, Object *obj, struct opSet *msg )
{
	struct myStringClassData *data;
	struct List *header;
	struct TagItem *tag, *tags;
	STRPTR buffer;

	buffer = NULL;
	header = NULL;

	tags = msg->ops_AttrList;

    while(( tag = NextTagItem( &tags )))
    {
        switch ( tag->ti_Tag )
        {
			case STRINGVIEW_Header:
			{
				header = (struct List *)tag->ti_Data;
				break;
			}

			case STRINGA_Buffer:
			{
				buffer = (STRPTR)tag->ti_Data;
				break;
			}

			default:
			{
				break;
			}
		}
	}

	if (( header == NULL ) || ( buffer == NULL ))
	{
		return( 0 );
	}

	obj = (Object *)IDoSuperMethodA( cl, obj, (APTR)msg );

	if ( obj == NULL )
	{
    	goto bailout;
    }

	if ( obj )
	{
		data = INST_DATA( cl, obj );

		data->SearchHeader = header;
		data->SearchBuffer = buffer;

		NewList(	&data->ListviewHeader );

		InitSemaphore( &data->Semaphore );

		data->WindowObject = NewObject( WINDOW_GetClass(), NULL,
			WA_Activate,						FALSE,
			WA_Borderless,						TRUE,
			WINDOW_ParentGroup,					NewObject( LAYOUT_GetClass(), NULL,
				LAYOUT_SpaceInner,				FALSE,
				LAYOUT_SpaceOuter,				FALSE,
				LAYOUT_AddChild,	    		data->ListviewObject = NewObject( LISTBROWSER_GetClass(), NULL,
					LISTBROWSER_Labels,			&data->ListviewHeader,
					LISTBROWSER_MakeVisible,	TRUE,
					LISTBROWSER_ShowSelected,	TRUE,
				End,
			End,
		End;

		if ( data->WindowObject == NULL )
		{
			goto bailout;
		}
	}

	return( (uint32)obj );

bailout:

	if ( obj )
    {
		ICoerceMethod( cl, obj, OM_DISPOSE );
	}

	return( FALSE );
}

///
/// myStringClass_OM_Dispose

uint32 myStringClass_OM_Dispose( Class *cl, Object *obj, struct opSet *msg )
{
struct myStringClassData *data;

	data = INST_DATA( cl, obj );

	if ( data->Window )
    {
		IDoMethod( data->WindowObject, WM_CLOSE );
		data->Window = NULL;
	}

	if ( data->WindowObject )
	{
		DisposeObject( data->WindowObject );
		data->WindowObject = NULL;
	}

	return( IDoSuperMethodA( cl, obj, (APTR)msg ));
}

///
/// myStringClass_OM_Set

//--uint32 myStringClass_OM_Set( Class *cl, Object *obj, struct opSet *msg )
//--{
//--struct TorrentClassData *data;
//--struct TagItem *tag, *tags;
//--tr_stat_t *tor_s;
//--
//--    data = INST_DATA( cl, obj );
//--
//--    tags = msg->ops_AttrList;
//--
//--    while(( tag = NextTagItem( &tags )))
//--    {
//--        switch ( tag->ti_Tag )
//--        {
//--			case RAA_Torrent_Stats:
//--            {
//--				tor_s = (tr_stat_t *)tag->ti_Data;
//--
//--				switch( data->Type )
//--				{
//--					case TorType_BitTorrent5:
//--					{
//--						BitTorrent5_Stat( data, tor_s );
//--						break;
//--					}
//--
//--					case TorType_Transmission:
//--					{
//--						Transmission_Stat( data, tor_s );
//--						break;
//--					}
//--
//--					default:
//--					{
//--						break;
//--					}
//--				}
//--				break;
//--            }
//--
//--			case RAA_Torrent_Activate:
//--			{
//--				switch( data->Type )
//--				{
//--					case TorType_BitTorrent5:
//--					{
//--						if ( tag->ti_Data == TRUE )
//--						{
//--							BitTorrent5_Enable( data );
//--						}
//--						else
//--						{
//--							BitTorrent5_Disable( data );
//--						}
//--						break;
//--					}
//--
//--					case TorType_Transmission:
//--					{
//--						if ( tag->ti_Data == TRUE )
//--						{
//--							Transmission_Enable( data );
//--						}
//--						else
//--						{
//--							Transmission_Disable( data );
//--						}
//--						break;
//--					}
//--
//--					default:
//--					{
//--						break;
//--					}
//--				}
//--				break;
//--			}
//--
//--            default:
//--            {
//--                break;
//--            }
//--        }
//--    }
//--
//--    return( IDoSuperMethodA( cl, obj, (Msg)msg ));
//--}

///

/// myStringClass_GM_HandleInput

static uint32 myStringClass_GM_HandleInput( Class *cl, Object *obj, struct gpInput *msg )
{
	struct myStringClassData *data;
	struct Gadget *gad;
	uint32 retval;

	data = INST_DATA( cl, obj );

	gad = (struct Gadget *)obj;

	//IDoMethod( data->ListviewObject, (APTR)msg );

	if (( gad->Flags & GFLG_SELECTED ) == 0 )
	{
		return( GMR_NOREUSE );
	}

	switch( msg->gpi_IEvent->ie_Class )
	{
		case IECLASS_RAWKEY:
		{
			switch( msg->gpi_IEvent->ie_Code )
			{
				case 0x48:  // Page Up (DownKey)
				case 0x4c:  // Up Arrow (DownKey)
				{
					myStringArrowUp( cl, obj );
				
					retval = GMR_MEACTIVE;

					break;
				}

				case 0x49:  // Page Down (DownKey)
				case 0x4d:  // Down Arrow (DownKey)
				{
					myStringArrowDown( cl, obj );

					retval = GMR_MEACTIVE;

					break;
				}

//				  case 70:	  // Del
//				  case 65:	  // Backspace
//			      {
//					  myStringCloseListview( cl, obj );
//					  
//					  retval = IDoSuperMethodA( cl, obj, (APTR)msg );
//				      break;
//			      }

				case 68:	// Return
				{
					// If listview open, and an item is selected, copy selected node's text to the string gadget
					if( data->Window != NULL && data->ListviewCount > 0 )
					{
						struct Node *selected = NULL;
						STRPTR pText;
						GetAttr( LISTBROWSER_SelectedNode, data->ListviewObject, (uint32 *) ( &selected ) );
						if( selected != NULL )
						{
							GetListBrowserNodeAttrs( selected, LBNA_Column, 0, LBNCA_Text, &pText, TAG_END );
							SetGadgetAttrs( (struct Gadget *)obj, data->Window, NULL, STRINGA_TextVal, pText, TAG_DONE );
						}
					}

					retval = IDoSuperMethodA( cl, obj, (APTR)msg );
					break;
				}

				default:
				{
					uint32 oldpos;
					uint32 newpos;

					GetAttr( STRINGA_BufferPos, obj, &oldpos );

					retval = IDoSuperMethodA( cl, obj, (APTR)msg );

					GetAttr( STRINGA_BufferPos, obj, &newpos );

					if ( oldpos != newpos )
					{
						if ( myStringSearch( cl, obj ))
						{
							// Atleast one entry found, open window if not open
							if ( data->Window == NULL )
							{
								myStringOpenListview( cl, obj, msg );
							}
						}
						else
						{
							// No matches, migth aswell close the window
							myStringCloseListview( cl, obj );
						}
					}
					break;
				}
			}

			myStringHandleListview( cl, obj );
	        break;
	    }

		case IECLASS_MOUSEWHEEL:
		{
			struct InputEvent *ie = msg->gpi_IEvent;

			if ( ie->ie_Y < 0 )
			{
				myStringArrowUp( cl, obj );
			}
			else if ( ie->ie_Y > 0 )
			{
				myStringArrowDown( cl, obj );
			}

			myStringHandleListview( cl, obj );

			retval = GMR_MEACTIVE;
			break;
		}

	    default:
	    {
			retval = IDoSuperMethodA( cl, obj, (APTR)msg );
		    break;
	    }
    }

	return( retval );
}

///
/// myStringClass_GM_GoActive

static uint32 myStringClass_GM_GoActive( Class *cl, Object *obj, struct gpInput *msg )
{
struct myStringClassData *data;
struct Window *win;
struct Gadget *gad;
uint32 retval;

	data = INST_DATA( cl, obj );

	gad = (struct Gadget *)obj;

	if ( gad->Flags & GFLG_DISABLED )
	{
		myStringCloseListview( cl, obj );

		retval = GMR_NOREUSE;
	}
	else
	{
		// If were not Disabled then set Selected flag
		gad->Flags |= GFLG_SELECTED;

		win = msg->gpi_GInfo->gi_Window;

		if ( win )
		{
			data->WinXPos = win->LeftEdge + gad->LeftEdge;
			data->WinYPos = win->TopEdge + gad->TopEdge + gad->Height - 1;
			data->WinWidth = gad->Width;
			data->WinHeight = 150;
		}

		if ( myStringSearch( cl, obj ))
		{
			// Atleast one entry found, open window if not open
			if ( data->Window == NULL )
			{
				myStringOpenListview( cl, obj, msg );
			}
		}
		else
		{
			// No matches, migth aswell close the window
			myStringCloseListview( cl, obj );
		}

		retval = IDoSuperMethodA( cl, obj, (APTR)msg );
	}

	return( retval );
}

///
/// myStringClass_GM_GoInactive

static uint32 myStringClass_GM_GoInactive( Class *cl, Object *obj, struct gpGoInactive *msg )
{
struct myStringClassData *data;

	data = INST_DATA( cl, obj );

	myStringCloseListview( cl, obj );

	return( IDoSuperMethodA(  cl, obj, (APTR)msg ));
}

///

/* Dispatcher */

/// myStringClassDispatcher

uint32 myStringClassDispatcher( Class *cl, Object *obj, Msg msg )
{
struct myStringClassData *data;
uint32 ret;

	if ( msg->MethodID == OM_NEW )
	{
		return( myStringClass_OM_New( cl, obj, (APTR)msg ));
	}
	else if ( msg->MethodID == OM_DISPOSE )
	{
		return( myStringClass_OM_Dispose( cl, obj, (APTR)msg ));
	}
	else
	{
		data = INST_DATA( cl, obj );

		ObtainSemaphore( &data->Semaphore );

		switch( msg->MethodID )
		{
			/* BOOPSI methods */
            case OM_SET:
            {
                struct TagItem *tag, *tags;
                struct opSet *opSet = (struct opSet *)msg;
                tags = opSet->ops_AttrList;
                while ((tag = NextTagItem(&tags)))
                {
                    if (STRINGA_TextVal == tag->ti_Tag)
                    {
                        URLHistory_AddPage((const char *)tag->ti_Data);
                    }
                }
                 
                ret = IDoSuperMethodA(cl, obj, (APTR)msg);
             }
             break;

//			  case OM_SET:        ret = TorrentClass_OM_Set(		  cl, obj, (APTR)msg );   break;

			/* Only used for Gadgets */
//			  case GM_DOMAIN:         		  ret = myStringClass_GM_Domain(	      	  cl, obj, (APTR)msg );   break;
//			  case GM_LAYOUT:				  ret = myStringClass_GM_Layout(			  cl, obj, (APTR)msg );	  break;
//			  case GM_CLIPRECT:	      		  ret = myStringClass_GM_ClipRect(	      	  cl, obj, (APTR)msg );   break;
//			  case GM_EXTENT:				  ret = myStringClass_GM_Extent(			  cl, obj, (APTR)msg );	  break;
//			  case GM_RENDER:     			  ret = myStringClass_GM_Render(			  cl, obj, (APTR)msg );   break;
//			  case GM_HITTEST:				  ret = myStringClass_GM_HitTest(			  cl, obj, (APTR)msg );   break;
			case GM_HANDLEINPUT:	ret = myStringClass_GM_HandleInput(		cl, obj, (APTR)msg );   break;
			case GM_GOACTIVE:		ret = myStringClass_GM_GoActive(		cl, obj, (APTR)msg );   break;
			case GM_GOINACTIVE:		ret = myStringClass_GM_GoInactive(		cl, obj, (APTR)msg );   break;

			/* Unknown method -> delegate to SuperClass */
			default:				ret = IDoSuperMethodA(		cl, obj, (APTR)msg );   break;
		}

		ReleaseSemaphore( &data->Semaphore );

		return( ret );
	}
}

///

/* Create Class */

/// Make String Class

Class *MakeStringClass( void )
{
    Class *cl;
	cl = MakeClass( NULL, NULL, STRING_GetClass(), sizeof(struct myStringClassData), 0 );

	if ( cl )
	{
		cl->cl_Dispatcher.h_Entry = (uint32(*)())myStringClassDispatcher;
	}

    URLHistory_Init();

	return( cl );
}

/// Free String Class

void FreeStringClass(Class *cl)
{
    URLHistory_Free();
    FreeClass(cl);
}

///

/* The End */

