------------------------------------------------------------------------------
--                                                                          --
--                           GNAT ncurses Binding                           --
--                                                                          --
--                      Terminal_Interface.Curses.Menus                     --
--                                                                          --
--                                 B O D Y                                  --
--                                                                          --
------------------------------------------------------------------------------
-- Copyright (c) 1998-2008,2009 Free Software Foundation, Inc.              --
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
--  $Revision: 1.27 $
--  $Date: 2009/12/26 17:38:58 $
--  Binding Version 01.00
------------------------------------------------------------------------------
with Ada.Unchecked_Deallocation;
with Terminal_Interface.Curses.Aux; use Terminal_Interface.Curses.Aux;

with Interfaces.C; use Interfaces.C;
with Interfaces.C.Strings; use Interfaces.C.Strings;
with Interfaces.C.Pointers;

with Ada.Unchecked_Conversion;

package body Terminal_Interface.Curses.Menus is

   type C_Item_Array is array (Natural range <>) of aliased Item;
   package I_Array is new
     Interfaces.C.Pointers (Natural, Item, C_Item_Array, Null_Item);

   use type System.Bit_Order;
   subtype chars_ptr is Interfaces.C.Strings.chars_ptr;

   function MOS_2_CInt is new
     Ada.Unchecked_Conversion (Menu_Option_Set,
                               C_Int);

   function CInt_2_MOS is new
     Ada.Unchecked_Conversion (C_Int,
                               Menu_Option_Set);

   function IOS_2_CInt is new
     Ada.Unchecked_Conversion (Item_Option_Set,
                               C_Int);

   function CInt_2_IOS is new
     Ada.Unchecked_Conversion (C_Int,
                               Item_Option_Set);

------------------------------------------------------------------------------
   procedure Request_Name (Key  : Menu_Request_Code;
                           Name : out String)
   is
      function Request_Name (Key : C_Int) return chars_ptr;
      pragma Import (C, Request_Name, "menu_request_name");
   begin
      Fill_String (Request_Name (C_Int (Key)), Name);
   end Request_Name;

   function Request_Name (Key : Menu_Request_Code) return String
   is
      function Request_Name (Key : C_Int) return chars_ptr;
      pragma Import (C, Request_Name, "menu_request_name");
   begin
      return Fill_String (Request_Name (C_Int (Key)));
   end Request_Name;

   function Create (Name        : String;
                    Description : String := "") return Item
   is
      type Char_Ptr is access all Interfaces.C.char;
      function Newitem (Name, Desc : Char_Ptr) return Item;
      pragma Import (C, Newitem, "new_item");

      type Name_String is new char_array (0 .. Name'Length);
      type Name_String_Ptr is access Name_String;
      pragma Controlled (Name_String_Ptr);

      type Desc_String is new char_array (0 .. Description'Length);
      type Desc_String_Ptr is access Desc_String;
      pragma Controlled (Desc_String_Ptr);

      Name_Str : constant Name_String_Ptr := new Name_String;
      Desc_Str : constant Desc_String_Ptr := new Desc_String;
      Name_Len, Desc_Len : size_t;
      Result : Item;
   begin
      To_C (Name, Name_Str.all, Name_Len);
      To_C (Description, Desc_Str.all, Desc_Len);
      Result := Newitem (Name_Str.all (Name_Str.all'First)'Access,
                         Desc_Str.all (Desc_Str.all'First)'Access);
      if Result = Null_Item then
         raise Eti_System_Error;
      end if;
      return Result;
   end Create;

   procedure Delete (Itm : in out Item)
   is
      function Descname (Itm  : Item) return chars_ptr;
      pragma Import (C, Descname, "item_description");
      function Itemname (Itm  : Item) return chars_ptr;
      pragma Import (C, Itemname, "item_name");

      function Freeitem (Itm : Item) return C_Int;
      pragma Import (C, Freeitem, "free_item");

      Res : Eti_Error;
      Ptr : chars_ptr;
   begin
      Ptr := Descname (Itm);
      if Ptr /= Null_Ptr then
         Interfaces.C.Strings.Free (Ptr);
      end if;
      Ptr := Itemname (Itm);
      if Ptr /= Null_Ptr then
         Interfaces.C.Strings.Free (Ptr);
      end if;
      Res := Freeitem (Itm);
      if Res /= E_Ok then
         Eti_Exception (Res);
      end if;
      Itm := Null_Item;
   end Delete;
-------------------------------------------------------------------------------
   procedure Set_Value (Itm   : Item;
                        Value : Boolean := True)
   is
      function Set_Item_Val (Itm : Item;
                             Val : C_Int) return C_Int;
      pragma Import (C, Set_Item_Val, "set_item_value");

      Res : constant Eti_Error := Set_Item_Val (Itm, Boolean'Pos (Value));
   begin
      if  Res /= E_Ok then
         Eti_Exception (Res);
      end if;
   end Set_Value;

   function Value (Itm : Item) return Boolean
   is
      function Item_Val (Itm : Item) return C_Int;
      pragma Import (C, Item_Val, "item_value");
   begin
      if Item_Val (Itm) = Curses_False then
         return False;
      else
         return True;
      end if;
   end Value;

-------------------------------------------------------------------------------
   function Visible (Itm : Item) return Boolean
   is
      function Item_Vis (Itm : Item) return C_Int;
      pragma Import (C, Item_Vis, "item_visible");
   begin
      if Item_Vis (Itm) = Curses_False then
         return False;
      else
         return True;
      end if;
   end Visible;
-------------------------------------------------------------------------------
   procedure Set_Options (Itm     : Item;
                          Options : Item_Option_Set)
   is
      function Set_Item_Opts (Itm : Item;
                              Opt : C_Int) return C_Int;
      pragma Import (C, Set_Item_Opts, "set_item_opts");

      Opt : constant C_Int := IOS_2_CInt (Options);
      Res : Eti_Error;
   begin
      Res := Set_Item_Opts (Itm, Opt);
      if Res /= E_Ok then
         Eti_Exception (Res);
      end if;
   end Set_Options;

   procedure Switch_Options (Itm     : Item;
                             Options : Item_Option_Set;
                             On      : Boolean := True)
   is
      function Item_Opts_On (Itm : Item;
                             Opt : C_Int) return C_Int;
      pragma Import (C, Item_Opts_On, "item_opts_on");
      function Item_Opts_Off (Itm : Item;
                              Opt : C_Int) return C_Int;
      pragma Import (C, Item_Opts_Off, "item_opts_off");

      Opt : constant C_Int := IOS_2_CInt (Options);
      Err : Eti_Error;
   begin
      if On then
         Err := Item_Opts_On (Itm, Opt);
      else
         Err := Item_Opts_Off (Itm, Opt);
      end if;
      if Err /= E_Ok then
         Eti_Exception (Err);
      end if;
   end Switch_Options;

   procedure Get_Options (Itm     : Item;
                          Options : out Item_Option_Set)
   is
      function Item_Opts (Itm : Item) return C_Int;
      pragma Import (C, Item_Opts, "item_opts");

      Res : constant C_Int := Item_Opts (Itm);
   begin
      Options := CInt_2_IOS (Res);
   end Get_Options;

   function Get_Options (Itm : Item := Null_Item) return Item_Option_Set
   is
      Ios : Item_Option_Set;
   begin
      Get_Options (Itm, Ios);
      return Ios;
   end Get_Options;
-------------------------------------------------------------------------------
   procedure Name (Itm  : Item;
                   Name : out String)
   is
      function Itemname (Itm : Item) return chars_ptr;
      pragma Import (C, Itemname, "item_name");
   begin
      Fill_String (Itemname (Itm), Name);
   end Name;

   function Name (Itm : Item) return String
   is
      function Itemname (Itm : Item) return chars_ptr;
      pragma Import (C, Itemname, "item_name");
   begin
      return Fill_String (Itemname (Itm));
   end Name;

   procedure Description (Itm         : Item;
                          Description : out String)
   is
      function Descname (Itm  : Item) return chars_ptr;
      pragma Import (C, Descname, "item_description");
   begin
      Fill_String (Descname (Itm), Description);
   end Description;

   function Description (Itm : Item) return String
   is
      function Descname (Itm  : Item) return chars_ptr;
      pragma Import (C, Descname, "item_description");
   begin
      return Fill_String (Descname (Itm));
   end Description;
-------------------------------------------------------------------------------
   procedure Set_Current (Men : Menu;
                          Itm : Item)
   is
      function Set_Curr_Item (Men : Menu;
                              Itm : Item) return C_Int;
      pragma Import (C, Set_Curr_Item, "set_current_item");

      Res : constant Eti_Error := Set_Curr_Item (Men, Itm);
   begin
      if Res /= E_Ok then
         Eti_Exception (Res);
      end if;
   end Set_Current;

   function Current (Men : Menu) return Item
   is
      function Curr_Item (Men : Menu) return Item;
      pragma Import (C, Curr_Item, "current_item");

      Res : constant Item := Curr_Item (Men);
   begin
      if Res = Null_Item then
         raise Menu_Exception;
      end if;
      return Res;
   end Current;

   procedure Set_Top_Row (Men  : Menu;
                          Line : Line_Position)
   is
      function Set_Toprow (Men  : Menu;
                           Line : C_Int) return C_Int;
      pragma Import (C, Set_Toprow, "set_top_row");

      Res : constant Eti_Error := Set_Toprow (Men, C_Int (Line));
   begin
      if  Res /= E_Ok then
         Eti_Exception (Res);
      end if;
   end Set_Top_Row;

   function Top_Row (Men : Menu) return Line_Position
   is
      function Toprow (Men : Menu) return C_Int;
      pragma Import (C, Toprow, "top_row");

      Res : constant C_Int := Toprow (Men);
   begin
      if Res = Curses_Err then
         raise Menu_Exception;
      end if;
      return Line_Position (Res);
   end Top_Row;

   function Get_Index (Itm : Item) return Positive
   is
      function Get_Itemindex (Itm : Item) return C_Int;
      pragma Import (C, Get_Itemindex, "item_index");

      Res : constant C_Int := Get_Itemindex (Itm);
   begin
      if Res = Curses_Err then
         raise Menu_Exception;
      end if;
      return Positive (Natural (Res) + Positive'First);
   end Get_Index;
-------------------------------------------------------------------------------
   procedure Post (Men  : Menu;
                   Post : Boolean := True)
   is
      function M_Post (Men : Menu) return C_Int;
      pragma Import (C, M_Post, "post_menu");
      function M_Unpost (Men : Menu) return C_Int;
      pragma Import (C, M_Unpost, "unpost_menu");

      Res : Eti_Error;
   begin
      if Post then
         Res := M_Post (Men);
      else
         Res := M_Unpost (Men);
      end if;
      if Res /= E_Ok then
         Eti_Exception (Res);
      end if;
   end Post;
-------------------------------------------------------------------------------
   procedure Set_Options (Men     : Menu;
                          Options : Menu_Option_Set)
   is
      function Set_Menu_Opts (Men : Menu;
                              Opt : C_Int) return C_Int;
      pragma Import (C, Set_Menu_Opts, "set_menu_opts");

      Opt : constant C_Int := MOS_2_CInt (Options);
      Res : Eti_Error;
   begin
      Res := Set_Menu_Opts (Men, Opt);
      if  Res /= E_Ok then
         Eti_Exception (Res);
      end if;
   end Set_Options;

   procedure Switch_Options (Men     : Menu;
                             Options : Menu_Option_Set;
                             On      : Boolean := True)
   is
      function Menu_Opts_On (Men : Menu;
                             Opt : C_Int) return C_Int;
      pragma Import (C, Menu_Opts_On, "menu_opts_on");
      function Menu_Opts_Off (Men : Menu;
                              Opt : C_Int) return C_Int;
      pragma Import (C, Menu_Opts_Off, "menu_opts_off");

      Opt : constant C_Int := MOS_2_CInt (Options);
      Err : Eti_Error;
   begin
      if On then
         Err := Menu_Opts_On  (Men, Opt);
      else
         Err := Menu_Opts_Off (Men, Opt);
      end if;
      if Err /= E_Ok then
         Eti_Exception (Err);
      end if;
   end Switch_Options;

   procedure Get_Options (Men     : Menu;
                          Options : out Menu_Option_Set)
   is
      function Menu_Opts (Men : Menu) return C_Int;
      pragma Import (C, Menu_Opts, "menu_opts");

      Res : constant C_Int := Menu_Opts (Men);
   begin
      Options := CInt_2_MOS (Res);
   end Get_Options;

   function Get_Options (Men : Menu := Null_Menu) return Menu_Option_Set
   is
      Mos : Menu_Option_Set;
   begin
      Get_Options (Men, Mos);
      return Mos;
   end Get_Options;
-------------------------------------------------------------------------------
   procedure Set_Window (Men : Menu;
                         Win : Window)
   is
      function Set_Menu_Win (Men : Menu;
                             Win : Window) return C_Int;
      pragma Import (C, Set_Menu_Win, "set_menu_win");

      Res : constant Eti_Error := Set_Menu_Win (Men, Win);
   begin
      if  Res /= E_Ok then
         Eti_Exception (Res);
      end if;
   end Set_Window;

   function Get_Window (Men : Menu) return Window
   is
      function Menu_Win (Men : Menu) return Window;
      pragma Import (C, Menu_Win, "menu_win");

      W : constant Window := Menu_Win (Men);
   begin
      return W;
   end Get_Window;

   procedure Set_Sub_Window (Men : Menu;
                             Win : Window)
   is
      function Set_Menu_Sub (Men : Menu;
                             Win : Window) return C_Int;
      pragma Import (C, Set_Menu_Sub, "set_menu_sub");

      Res : constant Eti_Error := Set_Menu_Sub (Men, Win);
   begin
      if  Res /= E_Ok then
         Eti_Exception (Res);
      end if;
   end Set_Sub_Window;

   function Get_Sub_Window (Men : Menu) return Window
   is
      function Menu_Sub (Men : Menu) return Window;
      pragma Import (C, Menu_Sub, "menu_sub");

      W : constant Window := Menu_Sub (Men);
   begin
      return W;
   end Get_Sub_Window;

   procedure Scale (Men     : Menu;
                    Lines   : out Line_Count;
                    Columns : out Column_Count)
   is
      type C_Int_Access is access all C_Int;
      function M_Scale (Men    : Menu;
                        Yp, Xp : C_Int_Access) return C_Int;
      pragma Import (C, M_Scale, "scale_menu");

      X, Y : aliased C_Int;
      Res  : constant Eti_Error := M_Scale (Men, Y'Access, X'Access);
   begin
      if Res /= E_Ok then
         Eti_Exception (Res);
      end if;
      Lines := Line_Count (Y);
      Columns := Column_Count (X);
   end Scale;
-------------------------------------------------------------------------------
   procedure Position_Cursor (Men : Menu)
   is
      function Pos_Menu_Cursor (Men : Menu) return C_Int;
      pragma Import (C, Pos_Menu_Cursor, "pos_menu_cursor");

      Res : constant Eti_Error := Pos_Menu_Cursor (Men);
   begin
      if  Res /= E_Ok then
         Eti_Exception (Res);
      end if;
   end Position_Cursor;

-------------------------------------------------------------------------------
   procedure Set_Mark (Men  : Menu;
                       Mark : String)
   is
      type Char_Ptr is access all Interfaces.C.char;
      function Set_Mark (Men  : Menu;
                         Mark : Char_Ptr) return C_Int;
      pragma Import (C, Set_Mark, "set_menu_mark");

      Txt : char_array (0 .. Mark'Length);
      Len : size_t;
      Res : Eti_Error;
   begin
      To_C (Mark, Txt, Len);
      Res := Set_Mark (Men, Txt (Txt'First)'Access);
      if Res /= E_Ok then
         Eti_Exception (Res);
      end if;
   end Set_Mark;

   procedure Mark (Men  : Menu;
                   Mark : out String)
   is
      function Get_Menu_Mark (Men : Menu) return chars_ptr;
      pragma Import (C, Get_Menu_Mark, "menu_mark");
   begin
      Fill_String (Get_Menu_Mark (Men), Mark);
   end Mark;

   function Mark (Men : Menu) return String
   is
      function Get_Menu_Mark (Men : Menu) return chars_ptr;
      pragma Import (C, Get_Menu_Mark, "menu_mark");
   begin
      return Fill_String (Get_Menu_Mark (Men));
   end Mark;

-------------------------------------------------------------------------------
   procedure Set_Foreground
     (Men   : Menu;
      Fore  : Character_Attribute_Set := Normal_Video;
      Color : Color_Pair := Color_Pair'First)
   is
      function Set_Menu_Fore (Men  : Menu;
                              Attr : C_Chtype) return C_Int;
      pragma Import (C, Set_Menu_Fore, "set_menu_fore");

      Ch : constant Attributed_Character := (Ch    => Character'First,
                                             Color => Color,
                                             Attr  => Fore);
      Res : constant Eti_Error := Set_Menu_Fore (Men, AttrChar_To_Chtype (Ch));
   begin
      if  Res /= E_Ok then
         Eti_Exception (Res);
      end if;
   end Set_Foreground;

   procedure Foreground (Men  : Menu;
                         Fore : out Character_Attribute_Set)
   is
      function Menu_Fore (Men : Menu) return C_Chtype;
      pragma Import (C, Menu_Fore, "menu_fore");
   begin
      Fore := Chtype_To_AttrChar (Menu_Fore (Men)).Attr;
   end Foreground;

   procedure Foreground (Men   : Menu;
                         Fore  : out Character_Attribute_Set;
                         Color : out Color_Pair)
   is
      function Menu_Fore (Men : Menu) return C_Chtype;
      pragma Import (C, Menu_Fore, "menu_fore");
   begin
      Fore  := Chtype_To_AttrChar (Menu_Fore (Men)).Attr;
      Color := Chtype_To_AttrChar (Menu_Fore (Men)).Color;
   end Foreground;

   procedure Set_Background
     (Men   : Menu;
      Back  : Character_Attribute_Set := Normal_Video;
      Color : Color_Pair := Color_Pair'First)
   is
      function Set_Menu_Back (Men  : Menu;
                              Attr : C_Chtype) return C_Int;
      pragma Import (C, Set_Menu_Back, "set_menu_back");

      Ch : constant Attributed_Character := (Ch    => Character'First,
                                             Color => Color,
                                             Attr  => Back);
      Res : constant Eti_Error := Set_Menu_Back (Men, AttrChar_To_Chtype (Ch));
   begin
      if  Res /= E_Ok then
         Eti_Exception (Res);
      end if;
   end Set_Background;

   procedure Background (Men  : Menu;
                         Back : out Character_Attribute_Set)
   is
      function Menu_Back (Men : Menu) return C_Chtype;
      pragma Import (C, Menu_Back, "menu_back");
   begin
      Back := Chtype_To_AttrChar (Menu_Back (Men)).Attr;
   end Background;

   procedure Background (Men   : Menu;
                         Back  : out Character_Attribute_Set;
                         Color : out Color_Pair)
   is
      function Menu_Back (Men : Menu) return C_Chtype;
      pragma Import (C, Menu_Back, "menu_back");
   begin
      Back  := Chtype_To_AttrChar (Menu_Back (Men)).Attr;
      Color := Chtype_To_AttrChar (Menu_Back (Men)).Color;
   end Background;

   procedure Set_Grey (Men   : Menu;
                       Grey  : Character_Attribute_Set := Normal_Video;
                       Color : Color_Pair := Color_Pair'First)
   is
      function Set_Menu_Grey (Men  : Menu;
                              Attr : C_Chtype) return C_Int;
      pragma Import (C, Set_Menu_Grey, "set_menu_grey");

      Ch : constant Attributed_Character := (Ch    => Character'First,
                                             Color => Color,
                                             Attr  => Grey);

      Res : constant Eti_Error := Set_Menu_Grey (Men, AttrChar_To_Chtype (Ch));
   begin
      if  Res /= E_Ok then
         Eti_Exception (Res);
      end if;
   end Set_Grey;

   procedure Grey (Men  : Menu;
                   Grey : out Character_Attribute_Set)
   is
      function Menu_Grey (Men : Menu) return C_Chtype;
      pragma Import (C, Menu_Grey, "menu_grey");
   begin
      Grey := Chtype_To_AttrChar (Menu_Grey (Men)).Attr;
   end Grey;

   procedure Grey (Men  : Menu;
                   Grey : out Character_Attribute_Set;
                   Color : out Color_Pair)
   is
      function Menu_Grey (Men : Menu) return C_Chtype;
      pragma Import (C, Menu_Grey, "menu_grey");
   begin
      Grey  := Chtype_To_AttrChar (Menu_Grey (Men)).Attr;
      Color := Chtype_To_AttrChar (Menu_Grey (Men)).Color;
   end Grey;

   procedure Set_Pad_Character (Men : Menu;
                                Pad : Character := Space)
   is
      function Set_Menu_Pad (Men : Menu;
                             Ch  : C_Int) return C_Int;
      pragma Import (C, Set_Menu_Pad, "set_menu_pad");

      Res : constant Eti_Error := Set_Menu_Pad (Men,
                                                C_Int (Character'Pos (Pad)));
   begin
      if Res /= E_Ok then
         Eti_Exception (Res);
      end if;
   end Set_Pad_Character;

   procedure Pad_Character (Men : Menu;
                            Pad : out Character)
   is
      function Menu_Pad (Men : Menu) return C_Int;
      pragma Import (C, Menu_Pad, "menu_pad");
   begin
      Pad := Character'Val (Menu_Pad (Men));
   end Pad_Character;
-------------------------------------------------------------------------------
   procedure Set_Spacing (Men   : Menu;
                          Descr : Column_Position := 0;
                          Row   : Line_Position   := 0;
                          Col   : Column_Position := 0)
   is
      function Set_Spacing (Men     : Menu;
                            D, R, C : C_Int) return C_Int;
      pragma Import (C, Set_Spacing, "set_menu_spacing");

      Res : constant Eti_Error := Set_Spacing (Men,
                                               C_Int (Descr),
                                               C_Int (Row),
                                               C_Int (Col));
   begin
      if Res /= E_Ok then
         Eti_Exception (Res);
      end if;
   end Set_Spacing;

   procedure Spacing (Men   : Menu;
                      Descr : out Column_Position;
                      Row   : out Line_Position;
                      Col   : out Column_Position)
   is
      type C_Int_Access is access all C_Int;
      function Get_Spacing (Men     : Menu;
                            D, R, C : C_Int_Access) return C_Int;
      pragma Import (C, Get_Spacing, "menu_spacing");

      D, R, C : aliased C_Int;
      Res : constant Eti_Error := Get_Spacing (Men,
                                               D'Access,
                                               R'Access,
                                               C'Access);
   begin
      if Res /= E_Ok then
         Eti_Exception (Res);
      else
         Descr := Column_Position (D);
         Row   := Line_Position (R);
         Col   := Column_Position (C);
      end if;
   end Spacing;
-------------------------------------------------------------------------------
   function Set_Pattern (Men  : Menu;
                         Text : String) return Boolean
   is
      type Char_Ptr is access all Interfaces.C.char;
      function Set_Pattern (Men     : Menu;
                            Pattern : Char_Ptr) return C_Int;
      pragma Import (C, Set_Pattern, "set_menu_pattern");

      S   : char_array (0 .. Text'Length);
      L   : size_t;
      Res : Eti_Error;
   begin
      To_C (Text, S, L);
      Res := Set_Pattern (Men, S (S'First)'Access);
      case Res is
         when E_No_Match => return False;
         when E_Ok       => return True;
         when others =>
            Eti_Exception (Res);
            return False;
      end case;
   end Set_Pattern;

   procedure Pattern (Men  : Menu;
                      Text : out String)
   is
      function Get_Pattern (Men : Menu) return chars_ptr;
      pragma Import (C, Get_Pattern, "menu_pattern");
   begin
      Fill_String (Get_Pattern (Men), Text);
   end Pattern;
-------------------------------------------------------------------------------
   procedure Set_Format (Men     : Menu;
                         Lines   : Line_Count;
                         Columns : Column_Count)
   is
      function Set_Menu_Fmt (Men : Menu;
                             Lin : C_Int;
                             Col : C_Int) return C_Int;
      pragma Import (C, Set_Menu_Fmt, "set_menu_format");

      Res : constant Eti_Error := Set_Menu_Fmt (Men,
                                                C_Int (Lines),
                                                C_Int (Columns));
   begin
      if  Res /= E_Ok then
         Eti_Exception (Res);
      end if;
   end Set_Format;

   procedure Format (Men     : Menu;
                     Lines   : out Line_Count;
                     Columns : out Column_Count)
   is
      type C_Int_Access is access all C_Int;
      function Menu_Fmt (Men  : Menu;
                         Y, X : C_Int_Access) return C_Int;
      pragma Import (C, Menu_Fmt, "menu_format");

      L, C : aliased C_Int;
      Res  : constant Eti_Error := Menu_Fmt (Men, L'Access, C'Access);
   begin
      if Res /= E_Ok then
         Eti_Exception (Res);
      else
         Lines   := Line_Count (L);
         Columns := Column_Count (C);
      end if;
   end Format;
-------------------------------------------------------------------------------
   procedure Set_Item_Init_Hook (Men  : Menu;
                                 Proc : Menu_Hook_Function)
   is
      function Set_Item_Init (Men  : Menu;
                              Proc : Menu_Hook_Function) return C_Int;
      pragma Import (C, Set_Item_Init, "set_item_init");

      Res : constant Eti_Error := Set_Item_Init (Men, Proc);
   begin
      if  Res /= E_Ok then
         Eti_Exception (Res);
      end if;
   end Set_Item_Init_Hook;

   procedure Set_Item_Term_Hook (Men  : Menu;
                                 Proc : Menu_Hook_Function)
   is
      function Set_Item_Term (Men  : Menu;
                              Proc : Menu_Hook_Function) return C_Int;
      pragma Import (C, Set_Item_Term, "set_item_term");

      Res : constant Eti_Error := Set_Item_Term (Men, Proc);
   begin
      if Res /= E_Ok then
         Eti_Exception (Res);
      end if;
   end Set_Item_Term_Hook;

   procedure Set_Menu_Init_Hook (Men  : Menu;
                                 Proc : Menu_Hook_Function)
   is
      function Set_Menu_Init (Men  : Menu;
                              Proc : Menu_Hook_Function) return C_Int;
      pragma Import (C, Set_Menu_Init, "set_menu_init");

      Res : constant Eti_Error := Set_Menu_Init (Men, Proc);
   begin
      if  Res /= E_Ok then
         Eti_Exception (Res);
      end if;
   end Set_Menu_Init_Hook;

   procedure Set_Menu_Term_Hook (Men  : Menu;
                                 Proc : Menu_Hook_Function)
   is
      function Set_Menu_Term (Men  : Menu;
                              Proc : Menu_Hook_Function) return C_Int;
      pragma Import (C, Set_Menu_Term, "set_menu_term");

      Res : constant Eti_Error := Set_Menu_Term (Men, Proc);
   begin
      if Res /= E_Ok then
         Eti_Exception (Res);
      end if;
   end Set_Menu_Term_Hook;

   function Get_Item_Init_Hook (Men : Menu) return Menu_Hook_Function
   is
      function Item_Init (Men : Menu) return Menu_Hook_Function;
      pragma Import (C, Item_Init, "item_init");
   begin
      return Item_Init (Men);
   end Get_Item_Init_Hook;

   function Get_Item_Term_Hook (Men : Menu) return Menu_Hook_Function
   is
      function Item_Term (Men : Menu) return Menu_Hook_Function;
      pragma Import (C, Item_Term, "item_term");
   begin
      return Item_Term (Men);
   end Get_Item_Term_Hook;

   function Get_Menu_Init_Hook (Men : Menu) return Menu_Hook_Function
   is
      function Menu_Init (Men : Menu) return Menu_Hook_Function;
      pragma Import (C, Menu_Init, "menu_init");
   begin
      return Menu_Init (Men);
   end Get_Menu_Init_Hook;

   function Get_Menu_Term_Hook (Men : Menu) return Menu_Hook_Function
   is
      function Menu_Term (Men : Menu) return Menu_Hook_Function;
      pragma Import (C, Menu_Term, "menu_term");
   begin
      return Menu_Term (Men);
   end Get_Menu_Term_Hook;
-------------------------------------------------------------------------------
   procedure Redefine (Men   : Menu;
                       Items : Item_Array_Access)
   is
      function Set_Items (Men   : Menu;
                          Items : System.Address) return C_Int;
      pragma Import (C, Set_Items, "set_menu_items");

      Res : Eti_Error;
   begin
      pragma Assert (Items (Items'Last) = Null_Item);
      if Items (Items'Last) /= Null_Item then
         raise Menu_Exception;
      else
         Res := Set_Items (Men, Items.all'Address);
         if  Res /= E_Ok then
            Eti_Exception (Res);
         end if;
      end if;
   end Redefine;

   function Item_Count (Men : Menu) return Natural
   is
      function Count (Men : Menu) return C_Int;
      pragma Import (C, Count, "item_count");
   begin
      return Natural (Count (Men));
   end Item_Count;

   function Items (Men   : Menu;
                   Index : Positive) return Item
   is
      use I_Array;

      function C_Mitems (Men : Menu) return Pointer;
      pragma Import (C, C_Mitems, "menu_items");

      P : Pointer := C_Mitems (Men);
   begin
      if P = null or else Index > Item_Count (Men) then
         raise Menu_Exception;
      else
         P := P + ptrdiff_t (C_Int (Index) - 1);
         return P.all;
      end if;
   end Items;

-------------------------------------------------------------------------------
   function Create (Items : Item_Array_Access) return Menu
   is
      function Newmenu (Items : System.Address) return Menu;
      pragma Import (C, Newmenu, "new_menu");

      M   : Menu;
   begin
      pragma Assert (Items (Items'Last) = Null_Item);
      if Items (Items'Last) /= Null_Item then
         raise Menu_Exception;
      else
         M := Newmenu (Items.all'Address);
         if M = Null_Menu then
            raise Menu_Exception;
         end if;
         return M;
      end if;
   end Create;

   procedure Delete (Men : in out Menu)
   is
      function Free (Men : Menu) return C_Int;
      pragma Import (C, Free, "free_menu");

      Res : constant Eti_Error := Free (Men);
   begin
      if Res /= E_Ok then
         Eti_Exception (Res);
      end if;
      Men := Null_Menu;
   end Delete;

------------------------------------------------------------------------------
   function Driver (Men : Menu;
                    Key : Key_Code) return Driver_Result
   is
      function Driver (Men : Menu;
                       Key : C_Int) return C_Int;
      pragma Import (C, Driver, "menu_driver");

      R : constant Eti_Error := Driver (Men, C_Int (Key));
   begin
      if R /= E_Ok then
         case R is
            when E_Unknown_Command  => return Unknown_Request;
            when E_No_Match         => return No_Match;
            when E_Request_Denied |
                 E_Not_Selectable   => return Request_Denied;
            when others =>
               Eti_Exception (R);
         end case;
      end if;
      return Menu_Ok;
   end Driver;

   procedure Free (IA         : in out Item_Array_Access;
                   Free_Items : Boolean := False)
   is
      procedure Release is new Ada.Unchecked_Deallocation
        (Item_Array, Item_Array_Access);
   begin
      if IA /= null and then Free_Items then
         for I in IA'First .. (IA'Last - 1) loop
            if IA (I) /= Null_Item then
               Delete (IA (I));
            end if;
         end loop;
      end if;
      Release (IA);
   end Free;

-------------------------------------------------------------------------------
   function Default_Menu_Options return Menu_Option_Set
   is
   begin
      return Get_Options (Null_Menu);
   end Default_Menu_Options;

   function Default_Item_Options return Item_Option_Set
   is
   begin
      return Get_Options (Null_Item);
   end Default_Item_Options;
-------------------------------------------------------------------------------

end Terminal_Interface.Curses.Menus;
