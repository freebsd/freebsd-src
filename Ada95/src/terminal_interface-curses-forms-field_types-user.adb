------------------------------------------------------------------------------
--                                                                          --
--                           GNAT ncurses Binding                           --
--                                                                          --
--              Terminal_Interface.Curses.Forms.Field_Types.User            --
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
--  $Revision: 1.16 $
--  $Date: 2009/12/26 17:38:58 $
--  Binding Version 01.00
------------------------------------------------------------------------------
with Ada.Unchecked_Conversion;
with Terminal_Interface.Curses.Aux; use Terminal_Interface.Curses.Aux;

package body Terminal_Interface.Curses.Forms.Field_Types.User is

   procedure Set_Field_Type (Fld : Field;
                             Typ : User_Defined_Field_Type)
   is
      function Allocate_Arg (T : User_Defined_Field_Type'Class)
                             return Argument_Access;

      function Set_Fld_Type (F    : Field := Fld;
                             Cft  : C_Field_Type := C_Generic_Type;
                             Arg1 : Argument_Access)
                             return C_Int;
      pragma Import (C, Set_Fld_Type, "set_field_type");

      Res : Eti_Error;

      function Allocate_Arg (T : User_Defined_Field_Type'Class)
                             return Argument_Access
      is
         Ptr : constant Field_Type_Access
             := new User_Defined_Field_Type'Class'(T);
      begin
         return new Argument'(Usr => System.Null_Address,
                              Typ => Ptr,
                              Cft => Null_Field_Type);
      end Allocate_Arg;

   begin
      Res := Set_Fld_Type (Arg1 => Allocate_Arg (Typ));
      if Res /= E_Ok then
         Eti_Exception (Res);
      end if;
   end Set_Field_Type;

   pragma Warnings (Off);
   function To_Argument_Access is new Ada.Unchecked_Conversion
     (System.Address, Argument_Access);
   pragma Warnings (On);

   function Generic_Field_Check (Fld : Field;
                                 Usr : System.Address) return C_Int
   is
      Result : Boolean;
      Udf    : constant User_Defined_Field_Type_Access :=
        User_Defined_Field_Type_Access (To_Argument_Access (Usr).Typ);
   begin
      Result := Field_Check (Fld, Udf.all);
      return C_Int (Boolean'Pos (Result));
   end Generic_Field_Check;

   function Generic_Char_Check (Ch  : C_Int;
                                Usr : System.Address) return C_Int
   is
      Result : Boolean;
      Udf    : constant User_Defined_Field_Type_Access :=
        User_Defined_Field_Type_Access (To_Argument_Access (Usr).Typ);
   begin
      Result := Character_Check (Character'Val (Ch), Udf.all);
      return C_Int (Boolean'Pos (Result));
   end Generic_Char_Check;

   --  -----------------------------------------------------------------------
   --
   function C_Generic_Type return C_Field_Type
   is
      Res : Eti_Error;
      T   : C_Field_Type;
   begin
      if M_Generic_Type = Null_Field_Type then
         T := New_Fieldtype (Generic_Field_Check'Access,
                             Generic_Char_Check'Access);
         if T = Null_Field_Type then
            raise Form_Exception;
         else
            Res := Set_Fieldtype_Arg (T,
                                      Make_Arg'Access,
                                      Copy_Arg'Access,
                                      Free_Arg'Access);
            if Res /= E_Ok then
               Eti_Exception (Res);
            end if;
         end if;
         M_Generic_Type := T;
      end if;
      pragma Assert (M_Generic_Type /= Null_Field_Type);
      return M_Generic_Type;
   end C_Generic_Type;

end Terminal_Interface.Curses.Forms.Field_Types.User;
