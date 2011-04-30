------------------------------------------------------------------------------
--                                                                          --
--                           GNAT ncurses Binding                           --
--                                                                          --
--                 Terminal_Interface.Curses.Forms.Field_Types              --
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
--  $Revision: 1.21 $
--  $Date: 2009/12/26 17:38:58 $
--  Binding Version 01.00
------------------------------------------------------------------------------
with Terminal_Interface.Curses.Aux; use Terminal_Interface.Curses.Aux;
with Ada.Unchecked_Deallocation;
with Ada.Unchecked_Conversion;
--  |
--  |=====================================================================
--  | man page form_fieldtype.3x
--  |=====================================================================
--  |
package body Terminal_Interface.Curses.Forms.Field_Types is

   use type System.Address;

   pragma Warnings (Off);
   function To_Argument_Access is new Ada.Unchecked_Conversion
     (System.Address, Argument_Access);
   pragma Warnings (On);

   function Get_Fieldtype (F : Field) return C_Field_Type;
   pragma Import (C, Get_Fieldtype, "field_type");

   function Get_Arg (F : Field) return System.Address;
   pragma Import (C, Get_Arg, "field_arg");
   --  |
   --  |=====================================================================
   --  | man page form_field_validation.3x
   --  |=====================================================================
   --  |
   --  |
   --  |
   function Get_Type (Fld : Field) return Field_Type_Access
   is
      Low_Level : constant C_Field_Type := Get_Fieldtype (Fld);
      Arg : Argument_Access;
   begin
      if Low_Level = Null_Field_Type then
         return null;
      else
         if Low_Level = M_Builtin_Router or else
           Low_Level = M_Generic_Type or else
           Low_Level = M_Choice_Router or else
           Low_Level = M_Generic_Choice then
            Arg := To_Argument_Access (Get_Arg (Fld));
            if Arg = null then
               raise Form_Exception;
            else
               return Arg.Typ;
            end if;
         else
            raise Form_Exception;
         end if;
      end if;
   end Get_Type;

   function Make_Arg (Args : System.Address) return System.Address
   is
      --  Actually args is a double indirected pointer to the arguments
      --  of a C variable argument list. In theory it is now quite
      --  complicated to write portable routine that reads the arguments,
      --  because one has to know the growth direction of the stack and
      --  the sizes of the individual arguments.
      --  Fortunately we are only interested in the first argument (#0),
      --  we know its size and for the first arg we don't care about
      --  into which stack direction we have to proceed. We simply
      --  resolve the double indirection and thats it.
      type V is access all System.Address;
      function To_Access is new Ada.Unchecked_Conversion (System.Address,
                                                          V);
   begin
      return To_Access (To_Access (Args).all).all;
   end Make_Arg;

   function Copy_Arg (Usr : System.Address) return System.Address
   is
   begin
      return Usr;
   end Copy_Arg;

   procedure Free_Arg (Usr : System.Address)
   is
      procedure Free_Type is new Ada.Unchecked_Deallocation
        (Field_Type'Class, Field_Type_Access);
      procedure Freeargs is new Ada.Unchecked_Deallocation
        (Argument, Argument_Access);

      To_Be_Free : Argument_Access := To_Argument_Access (Usr);
      Low_Level  : C_Field_Type;
   begin
      if To_Be_Free /= null then
         if To_Be_Free.Usr /= System.Null_Address then
            Low_Level := To_Be_Free.Cft;
            if Low_Level.Freearg /= null then
               Low_Level.Freearg (To_Be_Free.Usr);
            end if;
         end if;
         if To_Be_Free.Typ /= null then
            Free_Type (To_Be_Free.Typ);
         end if;
         Freeargs (To_Be_Free);
      end if;
   end Free_Arg;

   procedure Wrap_Builtin (Fld : Field;
                           Typ : Field_Type'Class;
                           Cft : C_Field_Type := C_Builtin_Router)
   is
      Usr_Arg   : constant System.Address := Get_Arg (Fld);
      Low_Level : constant C_Field_Type := Get_Fieldtype (Fld);
      Arg : Argument_Access;
      Res : Eti_Error;
      function Set_Fld_Type (F    : Field := Fld;
                             Cf   : C_Field_Type := Cft;
                             Arg1 : Argument_Access) return C_Int;
      pragma Import (C, Set_Fld_Type, "set_field_type");

   begin
      pragma Assert (Low_Level /= Null_Field_Type);
      if Cft /= C_Builtin_Router and then Cft /= C_Choice_Router then
         raise Form_Exception;
      else
         Arg := new Argument'(Usr => System.Null_Address,
                              Typ => new Field_Type'Class'(Typ),
                              Cft => Get_Fieldtype (Fld));
         if Usr_Arg /= System.Null_Address then
            if Low_Level.Copyarg /= null then
               Arg.Usr := Low_Level.Copyarg (Usr_Arg);
            else
               Arg.Usr := Usr_Arg;
            end if;
         end if;

         Res := Set_Fld_Type (Arg1 => Arg);
         if Res /= E_Ok then
            Eti_Exception (Res);
         end if;
      end if;
   end Wrap_Builtin;

   function Field_Check_Router (Fld : Field;
                                Usr : System.Address) return C_Int
   is
      Arg  : constant Argument_Access := To_Argument_Access (Usr);
   begin
      pragma Assert (Arg /= null and then Arg.Cft /= Null_Field_Type
                     and then Arg.Typ /= null);
      if Arg.Cft.Fcheck /= null then
         return Arg.Cft.Fcheck (Fld, Arg.Usr);
      else
         return 1;
      end if;
   end Field_Check_Router;

   function Char_Check_Router (Ch  : C_Int;
                               Usr : System.Address) return C_Int
   is
      Arg  : constant Argument_Access := To_Argument_Access (Usr);
   begin
      pragma Assert (Arg /= null and then Arg.Cft /= Null_Field_Type
                     and then Arg.Typ /= null);
      if Arg.Cft.Ccheck /= null then
         return Arg.Cft.Ccheck (Ch, Arg.Usr);
      else
         return 1;
      end if;
   end Char_Check_Router;

   function Next_Router (Fld : Field;
                         Usr : System.Address) return C_Int
   is
      Arg  : constant Argument_Access := To_Argument_Access (Usr);
   begin
      pragma Assert (Arg /= null and then Arg.Cft /= Null_Field_Type
                     and then Arg.Typ /= null);
      if Arg.Cft.Next /= null then
         return Arg.Cft.Next (Fld, Arg.Usr);
      else
         return 1;
      end if;
   end Next_Router;

   function Prev_Router (Fld : Field;
                         Usr : System.Address) return C_Int
   is
      Arg  : constant Argument_Access := To_Argument_Access (Usr);
   begin
      pragma Assert (Arg /= null and then Arg.Cft /= Null_Field_Type
                     and then Arg.Typ /= null);
      if Arg.Cft.Prev /= null then
         return Arg.Cft.Prev (Fld, Arg.Usr);
      else
         return 1;
      end if;
   end Prev_Router;

   --  -----------------------------------------------------------------------
   --
   function C_Builtin_Router return C_Field_Type
   is
      Res : Eti_Error;
      T   : C_Field_Type;
   begin
      if M_Builtin_Router = Null_Field_Type then
         T := New_Fieldtype (Field_Check_Router'Access,
                             Char_Check_Router'Access);
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
         M_Builtin_Router := T;
      end if;
      pragma Assert (M_Builtin_Router /= Null_Field_Type);
      return M_Builtin_Router;
   end C_Builtin_Router;

   --  -----------------------------------------------------------------------
   --
   function C_Choice_Router return C_Field_Type
   is
      Res : Eti_Error;
      T   : C_Field_Type;
   begin
      if M_Choice_Router = Null_Field_Type then
         T := New_Fieldtype (Field_Check_Router'Access,
                             Char_Check_Router'Access);
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

            Res := Set_Fieldtype_Choice (T,
                                         Next_Router'Access,
                                         Prev_Router'Access);
            if Res /= E_Ok then
               Eti_Exception (Res);
            end if;
         end if;
         M_Choice_Router := T;
      end if;
      pragma Assert (M_Choice_Router /= Null_Field_Type);
      return M_Choice_Router;
   end C_Choice_Router;

end Terminal_Interface.Curses.Forms.Field_Types;
