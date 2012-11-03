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

variable kernel_state
variable root_state

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

: init_safemode ( N -- N )
	s" kern.smp.disabled" getenv -1 <> if
		drop ( n c-addr -- n ) \ unused
		toggle_menuitem ( n -- n )
	then
;

: toggle_safemode ( N -- N TRUE )
	toggle_menuitem

	\ Now we're going to make the change effective

	s" toggle_stateN @"      \ base name of toggle state var
	-rot 2dup 12 + c! rot    \ replace 'N' with ASCII numeral

	evaluate 0= if
		s" kern.smp.disabled" unsetenv
		s" hw.ata.ata_dma" unsetenv
		s" hw.ata.atapi_dma" unsetenv
		s" hw.ata.wc" unsetenv
		s" hw.eisa_slots" unsetenv
		s" kern.eventtimer.periodic" unsetenv
		s" kern.geom.part.check_integrity" unsetenv
	else
		s" set kern.smp.disabled=1" evaluate
		s" set hw.ata.ata_dma=0" evaluate
		s" set hw.ata.atapi_dma=0" evaluate
		s" set hw.ata.wc=0" evaluate
		s" set hw.eisa_slots=0" evaluate
		s" set kern.eventtimer.periodic=1" evaluate
		s" set kern.geom.part.check_integrity=0" evaluate
	then

	menu-redraw

	TRUE \ loop menu again
;

: init_singleuser ( N -- N )
	s" boot_single" getenv -1 <> if
		drop ( n c-addr -- n ) \ unused
		toggle_menuitem ( n -- n )
	then
;

: toggle_singleuser ( N -- N TRUE )
	toggle_menuitem
	menu-redraw

	\ Now we're going to make the change effective

	s" toggle_stateN @"      \ base name of toggle state var
	-rot 2dup 12 + c! rot    \ replace 'N' with ASCII numeral

	evaluate 0= if
		s" boot_single" unsetenv
	else
		s" set boot_single=YES" evaluate
	then

	TRUE \ loop menu again
;

: init_verbose ( N -- N )
	s" boot_verbose" getenv -1 <> if
		drop ( n c-addr -- n ) \ unused
		toggle_menuitem ( n -- n )
	then
;

: toggle_verbose ( N -- N TRUE )
	toggle_menuitem
	menu-redraw

	\ Now we're going to make the change effective

	s" toggle_stateN @"      \ base name of toggle state var
	-rot 2dup 12 + c! rot    \ replace 'N' with ASCII numeral

	evaluate 0= if
		s" boot_verbose" unsetenv
	else
		s" set boot_verbose=YES" evaluate
	then

	TRUE \ loop menu again
;

: goto_prompt ( N -- N FALSE )

	s" set autoboot_delay=NO" evaluate

	cr
	." To get back to the menu, type `menu' and press ENTER" cr
	." or type `boot' and press ENTER to start FreeBSD." cr
	cr

	FALSE \ exit the menu
;

: init_cyclestate ( N K -- N )
	over                   ( n k -- n k n )
	s" cycle_stateN"       ( n k n -- n k n c-addr u )
	-rot tuck 11 + c! swap ( n k n c-addr u -- n k c-addr u )
	evaluate               ( n k c-addr u -- n k addr )
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

: init_kernel ( N -- N )
	kernel_state @  ( n -- n k )
	init_cyclestate ( n k -- n )
;

: cycle_kernel ( N -- N TRUE )
	cycle_menuitem
	menu-redraw

	\ Now we're going to make the change effective

	s" cycle_stateN"         \ base name of array state var
	-rot 2dup 11 + c! rot    \ replace 'N' with ASCII numeral
	evaluate                 \ translate name into address
	@                        \ dereference address into value
	dup kernel_state !       \ save a copy for re-initialization
	48 +                     \ convert to ASCII numeral

	s" set kernel=${kernel_prefix}${kernel[N]}${kernel_suffix}"
	                          \ command to assemble full kernel-path
	-rot tuck 36 + c! swap    \ replace 'N' with array index value
	evaluate                  \ sets $kernel to full kernel-path

	TRUE \ loop menu again
;

: init_root ( N -- N )
	root_state @    ( n -- n k )
	init_cyclestate ( n k -- n )
;

: cycle_root ( N -- N TRUE )
	cycle_menuitem
	menu-redraw

	\ Now we're going to make the change effective

	s" cycle_stateN"         \ base name of array state var
	-rot 2dup 11 + c! rot    \ replace 'N' with ASCII numeral
	evaluate                 \ translate name into address
	@                        \ dereference address into value
	dup root_state !         \ save a copy for re-initialization
	48 +                     \ convert to ASCII numeral

	s" set root=${root_prefix}${root[N]}${root_suffix}"
	                          \ command to assemble root image-path
	-rot tuck 30 + c! swap    \ replace 'N' with array index value
	evaluate                  \ sets $kernel to full kernel-path

	TRUE \ loop menu again
;
