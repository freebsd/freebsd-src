------------------------------------------------------------------------------
--                                                                          --
--                           GNAT ncurses Binding                           --
--                                                                          --
--               Terminal_Interface.Curses.Menus.Item_User_Data             --
--                                                                          --
--                                 B O D Y                                  --
--                                                                          --
------------------------------------------------------------------------------
-- Copyright (c) 1998-2003,2009 Free Software Foundation, Inc.              --
--                                                                          --
-- Permission is hereby granted, free of charge, to any person obtaining a  --
-- copy of this software and associated documentation files (the            --
-- "Software"), to deal in the Software without restriction, including      --
-- without limitation the rights to use, copy, modify, merge, publish,      --
-- distribute, distribute with modifications, sublicense, and/or sell       --
-- copies of the Software, and to permit persons to whom the Software is    --
-- furnished to do so, subject to the following conditions:                 --
--                                                                          --
-- The above copyright notice and this permission notice shall be included  --
-- in all copies or substantial portions of the Software.                   --
--                                                                          --
-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  --
-- OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               --
-- MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   --
-- IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   --
-- DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    --
-- OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    --
-- THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               --
--                                                                          --
-- Except as contained in this notice, the name(s) of the above copyright   --
-- holders shall not be used in advertising or otherwise to promote the     --
-- sale, use or other dealings in this Software without prior written       --
-- authorization.                                                           --
------------------------------------------------------------------------------
--  Author:  Juergen Pfeifer, 1996
--  Version Control:
--  $Revision: 1.12 $
--  Binding Version 01.00
------------------------------------------------------------------------------
with Interfaces.C;
with Terminal_Interface.Curses.Aux; use Terminal_Interface.Curses.Aux;

package body Terminal_Interface.Curses.Menus.Item_User_Data is

   use type Interfaces.C.int;

   procedure Set_User_Data (Itm  : Item;
                            Data : User_Access)
   is
      function Set_Item_Userptr (Itm  : Item;
                                 Addr : User_Access)  return C_Int;
      pragma Import (C, Set_Item_Userptr, "set_item_userptr");

      Res : constant Eti_Error := Set_Item_Userptr (Itm, Data);
   begin
      if  Res /= E_Ok then
         Eti_Exception (Res);
      end if;
   end Set_User_Data;

   function Get_User_Data (Itm  : Item) return User_Access
   is
      function Item_Userptr (Itm : Item) return User_Access;
      pragma Import (C, Item_Userptr, "item_userptr");
   begin
      return Item_Userptr (Itm);
   end Get_User_Data;

   procedure Get_User_Data (Itm  : Item;
                            Data : out User_Access)
   is
   begin
      Data := Get_User_Data (Itm);
   end Get_User_Data;

end Terminal_Interface.Curses.Menus.Item_User_Data;
