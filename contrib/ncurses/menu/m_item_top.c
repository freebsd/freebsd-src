/****************************************************************************
 * Copyright (c) 1998 Free Software Foundation, Inc.                        *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/****************************************************************************
 *   Author: Juergen Pfeifer <juergen.pfeifer@gmx.net> 1995,1997            *
 ****************************************************************************/

/***************************************************************************
* Module m_item_top                                                        *
* Set and get top menus item                                               *
***************************************************************************/

#include "menu.priv.h"

MODULE_ID("$Id: m_item_top.c,v 1.4 1999/05/16 17:26:01 juergen Exp $")

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  int set_top_row(MENU *menu, int row)
|   
|   Description   :  Makes the speified row the top row in the menu
|
|   Return Values :  E_OK             - success
|                    E_BAD_ARGUMENT   - not a menu pointer or invalid row
|                    E_NOT_CONNECTED  - there are no items for the menu
+--------------------------------------------------------------------------*/
int set_top_row(MENU * menu, int row)
{
  ITEM *item;
  
  if (menu)
    {
      if ( menu->status & _IN_DRIVER )
	RETURN(E_BAD_STATE);
      if (menu->items == (ITEM **)0)
	RETURN(E_NOT_CONNECTED);
      
      if ((row<0) || (row > (menu->rows - menu->arows)))
	RETURN(E_BAD_ARGUMENT);
    }
  else
    RETURN(E_BAD_ARGUMENT);
  
  if (row != menu->toprow)
    {
      if (menu->status & _LINK_NEEDED) 
	_nc_Link_Items(menu);
      
      item = menu->items[ (menu->opt&O_ROWMAJOR) ? (row*menu->cols) : row ];
      assert(menu->pattern);
      Reset_Pattern(menu);
      _nc_New_TopRow_and_CurrentItem(menu, row, item);
    }
  
    RETURN(E_OK);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  int top_row(const MENU *)
|   
|   Description   :  Return the top row of the menu
|
|   Return Values :  The row number or ERR if there is no row
+--------------------------------------------------------------------------*/
int top_row(const MENU * menu)
{
  if (menu && menu->items && *(menu->items))
    {
      assert( (menu->toprow>=0) && (menu->toprow < menu->rows) );
      return menu->toprow;
    }
  else
    return(ERR);
}

/* m_item_top.c ends here */
