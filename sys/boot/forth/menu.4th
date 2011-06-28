\ Copyright (c) 2003 Scott Long <scottl@freebsd.org>
\ Copyright (c) 2003 Aleksander Fafula <alex@fafula.com>
\ Copyright (c) 2006-2011 Devin Teske <devinteske@hotmail.com>
\ All rights reserved.
\ 
\ Redistribution and use in source and binary forms, with or without
\ modification, are permitted provided that the following conditions
\ are met:
\ 1. Redistributions of source code must retain the above copyright
\    notice, this list of conditions and the following disclaimer.
\ 2. Redistributions in binary form must reproduce the above copyright
\    notice, this list of conditions and the following disclaimer in the
\    documentation and/or other materials provided with the distribution.
\ 
\ THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
\ ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
\ IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
\ ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
\ FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
\ DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
\ OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
\ HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
\ LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
\ OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
\ SUCH DAMAGE.
\ 
\ $FreeBSD$

marker task-menu.4th

\ Frame drawing
include /boot/frames.4th

f_double        \ Set frames to double (see frames.4th). Replace with
                \ f_single if you want single frames.
46 constant dot \ ASCII definition of a period (in decimal)

 4 constant menu_timeout_default_x \ default column position of timeout
23 constant menu_timeout_default_y \ default row position of timeout msg
10 constant menu_timeout_default   \ default timeout (in seconds)

\ Customize the following values with care

  1 constant menu_start \ Numerical prefix of first menu item
dot constant bullet     \ Menu bullet (appears after numerical prefix)
  5 constant menu_x     \ Row position of the menu (from the top)
 10 constant menu_y     \ Column position of the menu (from left side)

\ Menu Appearance
variable menuidx   \ Menu item stack for number prefixes
variable menurow   \ Menu item stack for positioning
variable menubllt  \ Menu item bullet

\ Menu Positioning
variable menuX     \ Menu X offset (columns)
variable menuY     \ Menu Y offset (rows)

\ Menu-item key association/detection
variable menukey1
variable menukey2
variable menukey3
variable menukey4
variable menukey5
variable menukey6
variable menukey7
variable menukey8
variable menureboot
variable menurebootadded
variable menuacpi
variable menuoptions

\ Menu timer [count-down] variables
variable menu_timeout_enabled \ timeout state (internal use only)
variable menu_time            \ variable for tracking the passage of time
variable menu_timeout         \ determined configurable delay duration
variable menu_timeout_x       \ column position of timeout message
variable menu_timeout_y       \ row position of timeout message

\ Boolean option status variables
variable toggle_state1
variable toggle_state2
variable toggle_state3
variable toggle_state4
variable toggle_state5
variable toggle_state6
variable toggle_state7
variable toggle_state8

\ Array option status variables
variable cycle_state1
variable cycle_state2
variable cycle_state3
variable cycle_state4
variable cycle_state5
variable cycle_state6
variable cycle_state7
variable cycle_state8

\ Containers for storing the initial caption text
create init_text1 255 allot
create init_text2 255 allot
create init_text3 255 allot
create init_text4 255 allot
create init_text5 255 allot
create init_text6 255 allot
create init_text7 255 allot
create init_text8 255 allot

: arch-i386? ( -- BOOL ) \ Returns TRUE (-1) on i386, FALSE (0) otherwise.
	s" arch-i386" environment? dup if
		drop
	then
;

\ This function prints a menu item at menuX (row) and menuY (column), returns
\ the incremental decimal ASCII value associated with the menu item, and
\ increments the cursor position to the next row for the creation of the next
\ menu item. This function is called by the menu-create function. You need not
\ call it directly.
\ 
: printmenuitem ( menu_item_str -- ascii_keycode )

	menurow dup @ 1+ swap ! ( increment menurow )
	menuidx dup @ 1+ swap ! ( increment menuidx )

	\ Calculate the menuitem row position
	menurow @ menuY @ +

	\ Position the cursor at the menuitem position
	dup menuX @ swap at-xy

	\ Print the value of menuidx
	loader_color? if
		." [1m"
	then
	menuidx @ .
	loader_color? if
		." [37m"
	then

	\ Move the cursor forward 1 column
	dup menuX @ 1+ swap at-xy

	menubllt @ emit	\ Print the menu bullet using the emit function

	\ Move the cursor to the 3rd column from the current position
	\ to allow for a space between the numerical prefix and the
	\ text caption
	menuX @ 3 + swap at-xy

	\ Print the menu caption (we expect a string to be on the stack
	\ prior to invoking this function)
	type

	\ Here we will add the ASCII decimal of the numerical prefix
	\ to the stack (decimal ASCII for `1' is 49) as a "return value"
	menuidx @ 48 +
;

: toggle_menuitem ( N -- N ) \ toggles caption text and internal menuitem state

	\ ASCII numeral equal to user-selected menu item must be on the stack.
	\ We do not modify the stack, so the ASCII numeral is left on top.

	s" init_textN"          \ base name of buffer
	-rot 2dup 9 + c! rot    \ replace 'N' with ASCII num

	evaluate c@ 0= if
		\ NOTE: no need to check toggle_stateN since the first time we
		\ are called, we will populate init_textN. Further, we don't
		\ need to test whether menu_caption[x] (ansi_caption[x] when
		\ loader_color=1) is available since we would not have been
		\ called if the caption was NULL.

		\ base name of environment variable
		loader_color? if
			s" ansi_caption[x]"
		else
			s" menu_caption[x]"
		then	
		-rot 2dup 13 + c! rot    \ replace 'x' with ASCII numeral

		getenv dup -1 <> if

			s" init_textN"          \ base name of buffer
			4 pick                  \ copy ASCII num to top
			rot tuck 9 + c! swap    \ replace 'N' with ASCII num
			evaluate

			\ now we have the buffer c-addr on top
			\ ( followed by c-addr/u of current caption )

			\ Copy the current caption into our buffer
			2dup c! -rot \ store strlen at first byte
			begin
				rot 1+    \ bring alt addr to top and increment
				-rot -rot \ bring buffer addr to top
				2dup c@ swap c! \ copy current character
				1+     \ increment buffer addr
				rot 1- \ bring buffer len to top and decrement
				dup 0= \ exit loop if buffer len is zero
			until
			2drop \ buffer len/addr
			drop  \ alt addr

		else
			drop
		then
	then

	\ Now we are certain to have init_textN populated with the initial
	\ value of menu_caption[x] (ansi_caption[x] with loader_color enabled).
	\ We can now use init_textN as the untoggled caption and
	\ toggled_text[x] (toggled_ansi[x] with loader_color enabled) as the
	\ toggled caption and store the appropriate value into menu_caption[x]
	\ (again, ansi_caption[x] with loader_color enabled). Last, we'll
	\ negate the toggled state so that we reverse the flow on subsequent
	\ calls.

	s" toggle_stateN @"      \ base name of toggle state var
	-rot 2dup 12 + c! rot    \ replace 'N' with ASCII numeral

	evaluate 0= if
		\ state is OFF, toggle to ON

		\ base name of toggled text var
		loader_color? if
			s" toggled_ansi[x]"
		else
			s" toggled_text[x]"
		then
		-rot 2dup 13 + c! rot    \ replace 'x' with ASCII num

		getenv dup -1 <> if
			\ Assign toggled text to menu caption

			\ base name of caption var
			loader_color? if
				s" ansi_caption[x]"
			else
				s" menu_caption[x]"
			then
			4 pick                   \ copy ASCII num to top
			rot tuck 13 + c! swap    \ replace 'x' with ASCII num

			setenv \ set new caption
		else
			\ No toggled text, keep the same caption

			drop
		then

		true \ new value of toggle state var (to be stored later)
	else
		\ state is ON, toggle to OFF

		s" init_textN"           \ base name of initial text buffer
		-rot 2dup 9 + c! rot     \ replace 'N' with ASCII numeral
		evaluate                 \ convert string to c-addr
		count                    \ convert c-addr to c-addr/u

		\ base name of caption var
		loader_color? if
			s" ansi_caption[x]"
		else
			s" menu_caption[x]"
		then
		4 pick                   \ copy ASCII num to top
		rot tuck 13 + c! swap    \ replace 'x' with ASCII numeral

		setenv    \ set new caption
		false     \ new value of toggle state var (to be stored below)
	then

	\ now we'll store the new toggle state (on top of stack)
	s" toggle_stateN"        \ base name of toggle state var
	3 pick                   \ copy ASCII numeral to top
	rot tuck 12 + c! swap    \ replace 'N' with ASCII numeral
	evaluate                 \ convert string to addr
	!                        \ store new value
;

: cycle_menuitem ( N -- N ) \ cycles through array of choices for a menuitem

	\ ASCII numeral equal to user-selected menu item must be on the stack.
	\ We do not modify the stack, so the ASCII numeral is left on top.

	s" cycle_stateN"         \ base name of array state var
	-rot 2dup 11 + c! rot    \ replace 'N' with ASCII numeral

	evaluate    \ we now have a pointer to the proper variable
	dup @       \ resolve the pointer (but leave it on the stack)
	1+          \ increment the value

	\ Before assigning the (incremented) value back to the pointer,
	\ let's test for the existence of this particular array element.
	\ If the element exists, we'll store index value and move on.
	\ Otherwise, we'll loop around to zero and store that.

	dup 48 + \ duplicate Array index and convert to ASCII numeral

	\ base name of array caption text
	loader_color? if
		s" ansi_caption[x][y]"          
	else
		s" menu_caption[x][y]"          
	then
	-rot tuck 16 + c! swap          \ replace 'y' with Array index
	4 pick rot tuck 13 + c! swap    \ replace 'x' with menu choice

	\ Now test for the existence of our incremented array index in the
	\ form of $menu_caption[x][y] ($ansi_caption[x][y] with loader_color
	\ enabled) as set in loader.rc(5), et. al.

	getenv dup -1 = if
		\ No caption set for this array index. Loop back to zero.

		drop    ( getenv cruft )
		drop    ( incremented array index )
		0       ( new array index that will be stored later )

		\ base name of caption var
		loader_color? if
			s" ansi_caption[x][0]"
		else
			s" menu_caption[x][0]"
		then
		4 pick rot tuck 13 + c! swap    \ replace 'x' with menu choice

		getenv dup -1 = if
			\ This is highly unlikely to occur, but to make
			\ sure that things move along smoothly, allocate
			\ a temporary NULL string

			s" "
		then
	then

	\ At this point, we should have the following on the stack (in order,
	\ from bottom to top):
	\ 
	\    N      - Ascii numeral representing the menu choice (inherited)
	\    Addr   - address of our internal cycle_stateN variable
	\    N      - zero-based number we intend to store to the above
	\    C-Addr - string value we intend to store to menu_caption[x]
	\             (or ansi_caption[x] with loader_color enabled)
	\ 
	\ Let's perform what we need to with the above.

	\ base name of menuitem caption var
	loader_color? if
		s" ansi_caption[x]"
	else
		s" menu_caption[x]"
	then
	6 pick rot tuck 13 + c! swap    \ replace 'x' with menu choice
	setenv                          \ set the new caption

	swap ! \ update array state variable
;

: acpipresent? ( -- flag ) \ Returns TRUE if ACPI is present, FALSE otherwise
	s" hint.acpi.0.rsdp" getenv
	dup -1 = if
		drop false exit
	then
	2drop
	true
;

: acpienabled? ( -- flag ) \ Returns TRUE if ACPI is enabled, FALSE otherwise
	s" hint.acpi.0.disabled" getenv
	dup -1 <> if
		s" 0" compare 0<> if
			false exit
		then
	else
		drop
	then
	true
;

\ This function prints the appropriate menuitem basename to the stack if an
\ ACPI option is to be presented to the user, otherwise returns -1. Used
\ internally by menu-create, you need not (nor should you) call this directly.
\ 
: acpimenuitem ( -- C-Addr | -1 )

	arch-i386? if
		acpipresent? if
			acpienabled? if
				loader_color? if
					s" toggled_ansi[x]"
				else
					s" toggled_text[x]"
				then
			else
				loader_color? if
					s" ansi_caption[x]"
				else
					s" menu_caption[x]"
				then
			then
		else
			menuidx dup @ 1+ swap ! ( increment menuidx )
			-1
		then
	else
		-1
	then
;

\ This function creates the list of menu items. This function is called by the
\ menu-display function. You need not be call it directly.
\ 
: menu-create ( -- )

	\ Print the frame caption at (x,y)
	s" loader_menu_title" getenv dup -1 = if
		drop s" Welcome to FreeBSD"
	then
	24 over 2 / - 9 at-xy type 

	\ Print our menu options with respective key/variable associations.
	\ `printmenuitem' ends by adding the decimal ASCII value for the
	\ numerical prefix to the stack. We store the value left on the stack
	\ to the key binding variable for later testing against a character
	\ captured by the `getkey' function.

	\ Note that any menu item beyond 9 will have a numerical prefix on the
	\ screen consisting of the first digit (ie. 1 for the tenth menu item)
	\ and the key required to activate that menu item will be the decimal
	\ ASCII of 48 plus the menu item (ie. 58 for the tenth item, aka. `:')
	\ which is misleading and not desirable.
	\ 
	\ Thus, we do not allow more than 8 configurable items on the menu
	\ (with "Reboot" as the optional ninth and highest numbered item).

	\ 
	\ Initialize the ACPI option status.
	\ 
	0 menuacpi !
	s" menu_acpi" getenv -1 <> if
		c@ dup 48 > over 57 < and if ( '1' <= c1 <= '8' )
			menuacpi !
			arch-i386? if acpipresent? if
				\ 
				\ Set menu toggle state to active state
				\ (required by generic toggle_menuitem)
				\ 
				menuacpi @
				s" acpienabled? toggle_stateN !"
				-rot tuck 25 + c! swap
				evaluate
			then then
		else
			drop
		then
	then

	\ 
	\ Initialize the menu_options visual separator.
	\ 
	0 menuoptions !
	s" menu_options" getenv -1 <> if
		c@ dup 48 > over 57 < and if ( '1' <= c1 <= '8' )
			menuoptions !
		else
			drop
		then
	then

	\ Initialize "Reboot" menu state variable (prevents double-entry)
	false menurebootadded !

	49 \ Iterator start (loop range 49 to 56; ASCII '1' to '8')
	begin
		\ If the "Options:" separator, print it.
		dup menuoptions @ = if
			\ Optionally add a reboot option to the menu
			s" menu_reboot" getenv -1 <> if
				drop
				s" Reboot" printmenuitem menureboot !
				true menurebootadded !
			then

			menuX @
			menurow @ 2 + menurow !
			menurow @ menuY @ +
			at-xy
			." Options:"
		then

		\ If this is the ACPI menu option, act accordingly.
		dup menuacpi @ = if
			acpimenuitem ( -- C-Addr | -1 )
		else
			loader_color? if
				s" ansi_caption[x]"
			else
				s" menu_caption[x]"
			then
		then

		( C-Addr | -1 )
		dup -1 <> if
			\ replace 'x' with current iteration
			-rot 2dup 13 + c! rot
        
			\ test for environment variable
			getenv dup -1 <> if
				printmenuitem ( C-Addr -- N )
        
				s" menukeyN !" \ generate cmd to store result
				-rot 2dup 7 + c! rot
        
				evaluate
			else
				drop
			then
		else
			drop

			s" menu_command[x]"
			-rot 2dup 13 + c! rot ( replace 'x' )
			unsetenv
		then

		1+ dup 56 > \ add 1 to iterator, continue if less than 57
	until
	drop \ iterator

	\ Optionally add a reboot option to the menu
	menurebootadded @ true <> if
		s" menu_reboot" getenv -1 <> if
			drop       \ no need for the value
			s" Reboot" \ menu caption (required by printmenuitem)

			printmenuitem
			menureboot !
		else
			0 menureboot !
		then
	then
;

\ Takes a single integer on the stack and updates the timeout display. The
\ integer must be between 0 and 9 (we will only update a single digit in the
\ source message).
\ 
: menu-timeout-update ( N -- )

	dup 9 > if ( N N 9 -- N )
		drop ( N -- )
		9 ( maximum: -- N )
	then

	dup 0 < if ( N N 0 -- N )
		drop ( N -- )
		0 ( minimum: -- N )
	then

	48 + ( convert single-digit numeral to ASCII: N 48 -- N )

	s" Autoboot in N seconds. [Space] to pause" ( N -- N Addr C )

	2 pick 48 - 0> if ( N Addr C N 48 -- N Addr C )

		\ Modify 'N' (Addr+12) above to reflect time-left

		-rot	( N Addr C -- C N Addr )
		tuck	( C N Addr -- C Addr N Addr )
		12 +	( C Addr N Addr -- C Addr N Addr2 )
		c!	( C Addr N Addr2 -- C Addr )
		swap	( C Addr -- Addr C )

		menu_timeout_x @
		menu_timeout_y @
		at-xy ( position cursor: Addr C N N -- Addr C )

		type ( print message: Addr C -- )

	else ( N Addr C N -- N Addr C )

		menu_timeout_x @
		menu_timeout_y @
		at-xy ( position cursor: N Addr C N N -- N Addr C )

		spaces ( erase message: N Addr C -- N Addr )
		2drop ( N Addr -- )

	then

	0 25 at-xy ( position cursor back at bottom-left )
;

\ This function blocks program flow (loops forever) until a key is pressed.
\ The key that was pressed is added to the top of the stack in the form of its
\ decimal ASCII representation. This function is called by the menu-display
\ function. You need not call it directly.
\ 
: getkey ( -- ascii_keycode )

	begin \ loop forever

		menu_timeout_enabled @ 1 = if
			( -- )
			seconds ( get current time: -- N )
			dup menu_time @ <> if ( has time elapsed?: N N N -- N )

				\ At least 1 second has elapsed since last loop
				\ so we will decrement our "timeout" (really a
				\ counter, insuring that we do not proceed too
				\ fast) and update our timeout display.

				menu_time ! ( update time record: N -- )
				menu_timeout @ ( "time" remaining: -- N )
				dup 0> if ( greater than 0?: N N 0 -- N )
					1- ( decrement counter: N -- N )
					dup menu_timeout !
						( re-assign: N N Addr -- N )
				then
				( -- N )

				dup 0= swap 0< or if ( N <= 0?: N N -- )
					\ halt the timer
					0 menu_timeout ! ( 0 Addr -- )
					0 menu_timeout_enabled ! ( 0 Addr -- )
				then

				\ update the timer display ( N -- )
				menu_timeout @ menu-timeout-update

				menu_timeout @ 0= if
					\ We've reached the end of the timeout
					\ (user did not cancel by pressing ANY
					\ key)

					s" menu_timeout_command" getenv dup
					-1 = if
						drop \ clean-up
					else
						evaluate
					then
				then

			else ( -- N )
				\ No [detectable] time has elapsed (in seconds)
				drop ( N -- )
			then
			( -- )
		then

		key? if \ Was a key pressed? (see loader(8))

			\ An actual key was pressed (if the timeout is running,
			\ kill it regardless of which key was pressed)
			menu_timeout @ 0<> if
				0 menu_timeout !
				0 menu_timeout_enabled !

				\ clear screen of timeout message
				0 menu-timeout-update
			then

			\ get the key that was pressed and exit (if we
			\ get a non-zero ASCII code)
			key dup 0<> if
				exit
			else
				drop
			then
		then
		50 ms \ sleep for 50 milliseconds (see loader(8))

	again
;

: menu-erase ( -- ) \ Erases menu and resets positioning variable to positon 1.

	\ Clear the screen area associated with the interactive menu
	menuX @ menuY @
	2dup at-xy 38 spaces 1+		2dup at-xy 38 spaces 1+
	2dup at-xy 38 spaces 1+		2dup at-xy 38 spaces 1+
	2dup at-xy 38 spaces 1+		2dup at-xy 38 spaces 1+
	2dup at-xy 38 spaces 1+		2dup at-xy 38 spaces 1+
	2dup at-xy 38 spaces 1+		2dup at-xy 38 spaces 1+
	2dup at-xy 38 spaces 1+		2dup at-xy 38 spaces
	2drop

	\ Reset the starting index and position for the menu
	menu_start 1- menuidx !
	0 menurow !
;

\ Erase and redraw the menu. Useful if you change a caption and want to
\ update the menu to reflect the new value.
\ 
: menu-redraw ( -- )
	menu-erase
	menu-create
;

\ This function initializes the menu. Call this from your `loader.rc' file
\ before calling any other menu-related functions.
\ 
: menu-init ( -- )
	menu_start
	1- menuidx !    \ Initialize the starting index for the menu
	0 menurow !     \ Initialize the starting position for the menu
	42 13 2 9 box   \ Draw frame (w,h,x,y)
	0 25 at-xy      \ Move cursor to the bottom for output
;

\ Main function. Call this from your `loader.rc' file.
\ 
: menu-display ( -- )

	0 menu_timeout_enabled ! \ start with automatic timeout disabled

	\ check indication that automatic execution after delay is requested
	s" menu_timeout_command" getenv -1 <> if ( Addr C -1 -- | Addr )
		drop ( just testing existence right now: Addr -- )

		\ initialize state variables
		seconds menu_time ! ( store the time we started )
		1 menu_timeout_enabled ! ( enable automatic timeout )

		\ read custom time-duration (if set)
		s" autoboot_delay" getenv dup -1 = if
			drop \ no custom duration (remove dup'd bunk -1)
			menu_timeout_default \ use default setting
		else
			2dup ?number 0= if ( if not a number )
				\ disable timeout if "NO", else use default
				s" NO" compare-insensitive 0= if
					0 menu_timeout_enabled !
					0 ( assigned to menu_timeout below )
				else
					menu_timeout_default
				then
			else
				-rot 2drop

				\ disable timeout if less than zero
				dup 0< if
					drop
					0 menu_timeout_enabled !
					0 ( assigned to menu_timeout below )
				then
			then
		then
		menu_timeout ! ( store value on stack from above )

		menu_timeout_enabled @ 1 = if
			\ read custom column position (if set)
			s" loader_menu_timeout_x" getenv dup -1 = if
				drop \ no custom column position
				menu_timeout_default_x \ use default setting
			else
				\ make sure custom position is a number
				?number 0= if
					menu_timeout_default_x \ or use default
				then
			then
			menu_timeout_x ! ( store value on stack from above )
        
			\ read custom row position (if set)
			s" loader_menu_timeout_y" getenv dup -1 = if
				drop \ no custom row position
				menu_timeout_default_y \ use default setting
			else
				\ make sure custom position is a number
				?number 0= if
					menu_timeout_default_y \ or use default
				then
			then
			menu_timeout_y ! ( store value on stack from above )
		then
	then

	menu-create

	begin \ Loop forever

		0 25 at-xy \ Move cursor to the bottom for output
		getkey     \ Block here, waiting for a key to be pressed

		dup -1 = if
			drop exit \ Caught abort (abnormal return)
		then

		\ Boot if the user pressed Enter/Ctrl-M (13) or
		\ Ctrl-Enter/Ctrl-J (10)
		dup over 13 = swap 10 = or if
			drop ( no longer needed )
			s" boot" evaluate
			exit ( pedantic; never reached )
		then

		\ Evaluate the decimal ASCII value against known menu item
		\ key associations and act accordingly

		49 \ Iterator start (loop range 49 to 56; ASCII '1' to '8')
		begin
			s" menukeyN @"

			\ replace 'N' with current iteration
			-rot 2dup 7 + c! rot

			evaluate rot tuck = if

				\ Adjust for missing ACPI menuitem on non-i386
				arch-i386? true <> menuacpi @ 0<> and if
					menuacpi @ over 2dup < -rot = or
					over 58 < and if
					( key >= menuacpi && key < 58: N -- N )
						1+
					then
				then

				\ base env name for the value (x is a number)
				s" menu_command[x]"

				\ Copy ASCII number to string at offset 13
				-rot 2dup 13 + c! rot

				\ Test for the environment variable
				getenv dup -1 <> if
					\ Execute the stored procedure
					evaluate

					\ We expect there to be a non-zero
					\  value left on the stack after
					\ executing the stored procedure.
					\ If so, continue to run, else exit.

					0= if
						drop \ key pressed
						drop \ loop iterator
						exit
					else
						swap \ need iterator on top
					then
				then

				\ Re-adjust for missing ACPI menuitem
				arch-i386? true <> menuacpi @ 0<> and if
					swap
					menuacpi @ 1+ over 2dup < -rot = or
					over 59 < and if
						1-
					then
					swap
				then
			else
				swap \ need iterator on top
			then

			\ 
			\ Check for menu keycode shortcut(s)
			\ 
			s" menu_keycode[x]"
			-rot 2dup 13 + c! rot
			getenv dup -1 = if
				drop
			else
				?number 0<> if
					rot tuck = if
						swap
						s" menu_command[x]"
						-rot 2dup 13 + c! rot
						getenv dup -1 <> if
							evaluate
							0= if
								2drop
								exit
							then
						else
							drop
						then
					else
						swap
					then
				then
			then

			1+ dup 56 > \ increment iterator
			            \ continue if less than 57
		until
		drop \ loop iterator

		menureboot @ = if 0 reboot then

	again	\ Non-operational key was pressed; repeat
;

\ This function unsets all the possible environment variables associated with
\ creating the interactive menu. Call this when you want to clear the menu
\ area in preparation for another menu.
\ 
: menu-clear ( -- )

	49 \ Iterator start (loop range 49 to 56; ASCII '1' to '8')
	begin
		\ basename for caption variable
		loader_color? if
			s" ansi_caption[x]"
		else
			s" menu_caption[x]"
		then
		-rot 2dup 13 + c! rot	\ replace 'x' with current iteration
		unsetenv		\ not erroneous to unset unknown var

		s" 0 menukeyN !"	\ basename for key association var
		-rot 2dup 9 + c! rot	\ replace 'N' with current iteration
		evaluate		\ assign zero (0) to key assoc. var

		1+ dup 56 >	\ increment, continue if less than 57
	until
	drop \ iterator

	\ clear the "Reboot" menu option flag
	s" menu_reboot" unsetenv
	0 menureboot !

	\ clear the ACPI menu option flag
	s" menu_acpi" unsetenv
	0 menuacpi !

	\ clear the "Options" menu separator flag
	s" menu_options" unsetenv
	0 menuoptions !

	menu-erase
;

\ Assign configuration values
bullet menubllt !
10 menuY !
5 menuX !

\ Initialize our boolean state variables
0 toggle_state1 !
0 toggle_state2 !
0 toggle_state3 !
0 toggle_state4 !
0 toggle_state5 !
0 toggle_state6 !
0 toggle_state7 !
0 toggle_state8 !

\ Initialize our array state variables
0 cycle_state1 !
0 cycle_state2 !
0 cycle_state3 !
0 cycle_state4 !
0 cycle_state5 !
0 cycle_state6 !
0 cycle_state7 !
0 cycle_state8 !

\ Initialize string containers
0 init_text1 c!
0 init_text2 c!
0 init_text3 c!
0 init_text4 c!
0 init_text5 c!
0 init_text6 c!
0 init_text7 c!
0 init_text8 c!
