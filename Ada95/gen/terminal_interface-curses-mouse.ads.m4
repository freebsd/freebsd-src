--  -*- ada -*-
define(`HTMLNAME',`terminal_interface-curses-mouse__ads.htm')dnl
include(M4MACRO)dnl
------------------------------------------------------------------------------
--                                                                          --
--                           GNAT ncurses Binding                           --
--                                                                          --
--                      Terminal_Interface.Curses.Mouse                     --
--                                                                          --
--                                 S P E C                                  --
--                                                                          --
------------------------------------------------------------------------------
-- Copyright (c) 1998-2006,2009 Free Software Foundation, Inc.              --
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
--  $Revision: 1.28 $
--  $Date: 2009/12/26 17:38:58 $
--  Binding Version 01.00
------------------------------------------------------------------------------
include(`Mouse_Base_Defs')
with System;

package Terminal_Interface.Curses.Mouse is
   pragma Preelaborate (Terminal_Interface.Curses.Mouse);

   --  MANPAGE(`curs_mouse.3x')
   --  Please note, that in ncurses-1.9.9e documentation mouse support
   --  is still marked as experimental. So also this binding will change
   --  if the ncurses methods change.
   --
   --  mouse_trafo, wmouse_trafo are implemented as Transform_Coordinates
   --  in the parent package.
   --
   --  Not implemented:
   --  REPORT_MOUSE_POSITION (i.e. as a parameter to Register_Reportable_Event
   --  or Start_Mouse)
   type Event_Mask is private;
   No_Events  : constant Event_Mask;
   All_Events : constant Event_Mask;

   type Mouse_Button is (Left,     -- aka: Button 1
                         Middle,   -- aka: Button 2
                         Right,    -- aka: Button 3
                         Button4,  -- aka: Button 4
                         Control,  -- Control Key
                         Shift,    -- Shift Key
                         Alt);     -- ALT Key

   subtype Real_Buttons  is Mouse_Button range Left .. Button4;
   subtype Modifier_Keys is Mouse_Button range Control .. Alt;

   type Button_State is (Released,
                         Pressed,
                         Clicked,
                         Double_Clicked,
                         Triple_Clicked);

   type Button_States is array (Button_State) of Boolean;
   pragma Pack (Button_States);

   All_Clicks : constant Button_States := (Clicked .. Triple_Clicked => True,
                                           others => False);
   All_States : constant Button_States := (others => True);

   type Mouse_Event is private;

   --  MANPAGE(`curs_mouse.3x')

   function Has_Mouse return Boolean;
   --  Return true if a mouse device is supported, false otherwise.

   procedure Register_Reportable_Event
     (Button : Mouse_Button;
      State  : Button_State;
      Mask   : in out Event_Mask);
   --  Stores the event described by the button and the state in the mask.
   --  Before you call this the first time, you should init the mask
   --  with the Empty_Mask constant
   pragma Inline (Register_Reportable_Event);

   procedure Register_Reportable_Events
     (Button : Mouse_Button;
      State  : Button_States;
      Mask   : in out Event_Mask);
   --  Register all events described by the Button and the State bitmap.
   --  Before you call this the first time, you should init the mask
   --  with the Empty_Mask constant

   --  ANCHOR(`mousemask()',`Start_Mouse')
   --  There is one difference to mousmask(): we return the value of the
   --  old mask, that means the event mask value before this call.
   --  Not Implemented: The library version
   --  returns a Mouse_Mask that tells which events are reported.
   function Start_Mouse (Mask : Event_Mask := All_Events)
                         return Event_Mask;
   --  AKA
   pragma Inline (Start_Mouse);

   procedure End_Mouse (Mask : Event_Mask := No_Events);
   --  Terminates the mouse, restores the specified event mask
   pragma Inline (End_Mouse);

   --  ANCHOR(`getmouse()',`Get_Mouse')
   function Get_Mouse return Mouse_Event;
   --  AKA
   pragma Inline (Get_Mouse);

   procedure Get_Event (Event  : Mouse_Event;
                        Y      : out Line_Position;
                        X      : out Column_Position;
                        Button : out Mouse_Button;
                        State  : out Button_State);
   --  !!! Warning: X and Y are screen coordinates. Due to ripped of lines they
   --  may not be identical to window coordinates.
   --  Not Implemented: Get_Event only reports one event, the C library
   --  version supports multiple events, e.g. {click-1, click-3}
   pragma Inline (Get_Event);

   --  ANCHOR(`ungetmouse()',`Unget_Mouse')
   procedure Unget_Mouse (Event : Mouse_Event);
   --  AKA
   pragma Inline (Unget_Mouse);

   --  ANCHOR(`wenclose()',`Enclosed_In_Window')
   function Enclosed_In_Window (Win    : Window := Standard_Window;
                                Event  : Mouse_Event) return Boolean;
   --  AKA
   --  But : use event instead of screen coordinates.
   pragma Inline (Enclosed_In_Window);

   --  ANCHOR(`mouseinterval()',`Mouse_Interval')
   function Mouse_Interval (Msec : Natural := 200) return Natural;
   --  AKA
   pragma Inline (Mouse_Interval);

private
   type Event_Mask is new Interfaces.C.unsigned_long;

   type Mouse_Event is
      record
         Id      : Integer range Integer (Interfaces.C.short'First) ..
                                 Integer (Interfaces.C.short'Last);
         X, Y, Z : Integer range Integer (Interfaces.C.int'First) ..
                                 Integer (Interfaces.C.int'Last);
         Bstate  : Event_Mask;
      end record;
   pragma Convention (C, Mouse_Event);

include(`Mouse_Event_Rep')
   Generation_Bit_Order : constant System.Bit_Order := System.M4_BIT_ORDER;
   --  This constant may be different on your system.

include(`Mouse_Events')
   No_Events  : constant Event_Mask := 0;
   All_Events : constant Event_Mask := ALL_MOUSE_EVENTS;

end Terminal_Interface.Curses.Mouse;
