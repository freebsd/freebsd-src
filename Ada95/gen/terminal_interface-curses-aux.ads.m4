--  -*- ada -*-
define(`HTMLNAME',`terminal_interface-curses-aux__ads.htm')dnl
include(M4MACRO)dnl
------------------------------------------------------------------------------
--                                                                          --
--                           GNAT ncurses Binding                           --
--                                                                          --
--                       Terminal_Interface.Curses.Aux                      --
--                                                                          --
--                                 S P E C                                  --
--                                                                          --
------------------------------------------------------------------------------
-- Copyright (c) 1998-2007,2009 Free Software Foundation, Inc.              --
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
--  $Revision: 1.17 $
--  Binding Version 01.00
------------------------------------------------------------------------------
include(`Base_Defs')
with System;
with Interfaces.C;
with Interfaces.C.Strings; use Interfaces.C.Strings;
with Unchecked_Conversion;

package Terminal_Interface.Curses.Aux is
   pragma Preelaborate (Terminal_Interface.Curses.Aux);

   use type Interfaces.C.int;

   subtype C_Int      is Interfaces.C.int;
   subtype C_Short    is Interfaces.C.short;
   subtype C_Long_Int is Interfaces.C.long;
   subtype C_Size_T   is Interfaces.C.size_t;
   subtype C_UInt     is Interfaces.C.unsigned;
   subtype C_ULong    is Interfaces.C.unsigned_long;
   subtype C_Char_Ptr is Interfaces.C.Strings.chars_ptr;
   type    C_Void_Ptr is new System.Address;
include(`Chtype_Def')
   --  This is how those constants are defined in ncurses. I see them also
   --  exactly like this in all ETI implementations I ever tested. So it
   --  could be that this is quite general, but please check with your curses.
   --  This is critical, because curses sometime mixes boolean returns with
   --  returning an error status.
   Curses_Ok    : constant C_Int := CF_CURSES_OK;
   Curses_Err   : constant C_Int := CF_CURSES_ERR;

   Curses_True  : constant C_Int := CF_CURSES_TRUE;
   Curses_False : constant C_Int := CF_CURSES_FALSE;

   --  Eti_Error: type for error codes returned by the menu and form subsystem
include(`Eti_Defs')
   procedure Eti_Exception (Code : Eti_Error);
   --  Dispatch the error code and raise the appropriate exception
   --
   --
   --  Some helpers
   function Chtype_To_AttrChar is new
     Unchecked_Conversion (Source => C_Chtype,
                           Target => Attributed_Character);
   function AttrChar_To_Chtype is new
     Unchecked_Conversion (Source => Attributed_Character,
                           Target => C_Chtype);

   function AttrChar_To_AttrType is new
     Unchecked_Conversion (Source => Attributed_Character,
                           Target => C_AttrType);

   function AttrType_To_AttrChar is new
     Unchecked_Conversion (Source => C_AttrType,
                           Target => Attributed_Character);

   procedure Fill_String (Cp  : chars_ptr;
                          Str : out String);
   --  Fill the Str parameter with the string denoted by the chars_ptr
   --  C-Style string.

   function Fill_String (Cp : chars_ptr) return String;
   --  Same but as function.

end Terminal_Interface.Curses.Aux;
