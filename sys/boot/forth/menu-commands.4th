\ Copyright (c) 2006-2012 Devin Teske <dteske@FreeBSD.org>
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

marker task-menu-commands.4th

include /boot/menusets.4th

variable kernel_state
variable root_state

\ 
\ Boot
\ 

: init_boot ( N -- N )
	dup
	s" boot_single" getenv -1 <> if
		drop ( n n c-addr -- n n ) \ unused
		toggle_menuitem ( n n -- n n )
		s" set menu_keycode[N]=115" \ base command to execute
	else
		s" set menu_keycode[N]=98" \ base command to execute
	then
	17 +c! \ replace 'N' with ASCII numeral
	evaluate
;

\ 
\ Alternate Boot
\ 

: init_altboot ( N -- N )
	dup
	s" boot_single" getenv -1 <> if
		drop ( n c-addr -- n ) \ unused
		toggle_menuitem ( n -- n )
		s" set menu_keycode[N]=109" \ base command to execute
	else
		s" set menu_keycode[N]=115" \ base command to execute
	then
	17 +c! \ replace 'N' with ASCII numeral
	evaluate
;

: altboot ( -- )
	s" boot_single" 2dup getenv -1 <> if
		drop ( c-addr/u c-addr -- c-addr/u ) \ unused
		unsetenv ( c-addr/u -- )
	else
		2drop ( c-addr/u -- ) \ unused
		s" set boot_single=YES" evaluate
	then
	0 boot ( state -- )
;

\ 
\ ACPI
\ 

: acpi_enable ( -- )
	s" set acpi_load=YES" evaluate \ XXX deprecated but harmless
	s" set hint.acpi.0.disabled=0" evaluate
	s" loader.acpi_disabled_by_user" unsetenv
;

: acpi_disable ( -- )
	s" acpi_load" unsetenv \ XXX deprecated but harmless
	s" set hint.acpi.0.disabled=1" evaluate
	s" set loader.acpi_disabled_by_user=1" evaluate
;

: toggle_acpi ( N -- N TRUE )

	\ Make changes effective _before_ calling menu-redraw

	acpienabled? if
		acpi_disable
	else
		acpi_enable
	then

	menu-redraw

	TRUE \ loop menu again
;

\ 
\ Safe Mode
\ 

: safemode_enabled? ( -- flag )
	s" kern.smp.disabled" getenv -1 <> dup if
		swap drop ( c-addr flag -- flag )
	then
;

: safemode_enable ( -- )
	s" set kern.smp.disabled=1" evaluate
	s" set hw.ata.ata_dma=0" evaluate
	s" set hw.ata.atapi_dma=0" evaluate
	s" set hw.ata.wc=0" evaluate
	s" set hw.eisa_slots=0" evaluate
	s" set kern.eventtimer.periodic=1" evaluate
	s" set kern.geom.part.check_integrity=0" evaluate
;

: safemode_disable ( -- )
	s" kern.smp.disabled" unsetenv
	s" hw.ata.ata_dma" unsetenv
	s" hw.ata.atapi_dma" unsetenv
	s" hw.ata.wc" unsetenv
	s" hw.eisa_slots" unsetenv
	s" kern.eventtimer.periodic" unsetenv
	s" kern.geom.part.check_integrity" unsetenv
;

: init_safemode ( N -- N )
	safemode_enabled? if
		toggle_menuitem ( n -- n )
	then
;

: toggle_safemode ( N -- N TRUE )
	toggle_menuitem

	\ Now we're going to make the change effective

	dup toggle_stateN @ 0= if
		safemode_disable
	else
		safemode_enable
	then

	menu-redraw

	TRUE \ loop menu again
;

\ 
\ Single User Mode
\ 

: singleuser_enabled? ( -- flag )
	s" boot_single" getenv -1 <> dup if
		swap drop ( c-addr flag -- flag )
	then
;

: singleuser_enable ( -- )
	s" set boot_single=YES" evaluate
;

: singleuser_disable ( -- )
	s" boot_single" unsetenv
;

: init_singleuser ( N -- N )
	singleuser_enabled? if
		toggle_menuitem ( n -- n )
	then
;

: toggle_singleuser ( N -- N TRUE )
	toggle_menuitem
	menu-redraw

	\ Now we're going to make the change effective

	dup toggle_stateN @ 0= if
		singleuser_disable
	else
		singleuser_enable
	then

	TRUE \ loop menu again
;

\ 
\ Verbose Boot
\ 

: verbose_enabled? ( -- flag )
	s" boot_verbose" getenv -1 <> dup if
		swap drop ( c-addr flag -- flag )
	then
;

: verbose_enable ( -- )
	s" set boot_verbose=YES" evaluate
;

: verbose_disable ( -- )
	s" boot_verbose" unsetenv
;

: init_verbose ( N -- N )
	verbose_enabled? if
		toggle_menuitem ( n -- n )
	then
;

: toggle_verbose ( N -- N TRUE )
	toggle_menuitem
	menu-redraw

	\ Now we're going to make the change effective

	dup toggle_stateN @ 0= if
		verbose_disable
	else
		verbose_enable
	then

	TRUE \ loop menu again
;

\ 
\ Escape to Prompt
\ 

: goto_prompt ( N -- N FALSE )

	s" set autoboot_delay=NO" evaluate

	cr
	." To get back to the menu, type `menu' and press ENTER" cr
	." or type `boot' and press ENTER to start FreeBSD." cr
	cr

	FALSE \ exit the menu
;

\ 
\ Cyclestate (used by kernel/root below)
\ 

: init_cyclestate ( N K -- N )
	over cycle_stateN ( n k -- n k addr )
	begin
		tuck @  ( n k addr -- n addr k c )
		over <> ( n addr k c -- n addr k 0|-1 )
	while
		rot ( n addr k -- addr k n )
		cycle_menuitem
		swap rot ( addr k n -- n k addr )
	repeat
	2drop ( n k addr -- n )
;

\
\ Kernel
\ 

: init_kernel ( N -- N )
	kernel_state @  ( n -- n k )
	init_cyclestate ( n k -- n )
;

: cycle_kernel ( N -- N TRUE )
	cycle_menuitem
	menu-redraw

	\ Now we're going to make the change effective

	dup cycle_stateN @
	dup kernel_state !       \ save a copy for re-initialization
	48 +                     \ convert to ASCII numeral

	s" set kernel=${kernel_prefix}${kernel[N]}${kernel_suffix}"
	36 +c!   \ replace 'N' with ASCII numeral
	evaluate \ sets $kernel to full kernel-path

	TRUE \ loop menu again
;

\ 
\ Root
\ 

: init_root ( N -- N )
	root_state @    ( n -- n k )
	init_cyclestate ( n k -- n )
;

: cycle_root ( N -- N TRUE )
	cycle_menuitem
	menu-redraw

	\ Now we're going to make the change effective

	dup cycle_stateN @
	dup root_state !         \ save a copy for re-initialization
	48 +                     \ convert to ASCII numeral

	s" set root=${root_prefix}${root[N]}${root_suffix}"
	30 +c!   \ replace 'N' with ASCII numeral
	evaluate \ sets $root to full root-path

	TRUE \ loop menu again
;

\ 
\ Menusets
\ 

: goto_menu ( N M -- N TRUE )
	menu-unset
	menuset-loadsetnum ( n m -- n )
	menu-redraw
	TRUE \ Loop menu again
;

\ 
\ Defaults
\ 

: set_default_boot_options ( N -- N TRUE )
	acpi_enable
	safemode_disable
	singleuser_disable
	verbose_disable
	2 goto_menu
;
