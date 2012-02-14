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

marker task-menu-commands.4th

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

: toggle_safemode ( N -- N TRUE )
	toggle_menuitem

	\ Now we're going to make the change effective

	s" toggle_stateN @"      \ base name of toggle state var
	-rot 2dup 12 + c! rot    \ replace 'N' with ASCII numeral

	evaluate 0= if
		s" hint.apic.0.disabled" unsetenv
		s" hw.ata.ata_dma" unsetenv
		s" hw.ata.atapi_dma" unsetenv
		s" hw.ata.wc" unsetenv
		s" hw.eisa_slots" unsetenv
		s" hint.kbdmux.0.disabled" unsetenv
	else
		\ 
		\ Toggle ACPI elements if necessary
		\ 
		acpipresent? if acpienabled? if
			menuacpi @ dup 0<> if
				toggle_menuitem ( N -- N )
			then
			drop
			acpi_disable
		then then

		s" set hint.apic.0.disabled=1" evaluate
		s" set hw.ata.ata_dma=0" evaluate
		s" set hw.ata.atapi_dma=0" evaluate
		s" set hw.ata.wc=0" evaluate
		s" set hw.eisa_slots=0" evaluate
		s" set hint.kbdmux.0.disabled=1" evaluate
	then

	menu-redraw

	TRUE \ loop menu again
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

: cycle_kernel ( N -- N TRUE )
	cycle_menuitem
	menu-redraw

	\ Now we're going to make the change effective

	s" cycle_stateN"         \ base name of array state var
	-rot 2dup 11 + c! rot    \ replace 'N' with ASCII numeral
	evaluate                 \ translate name into address
	@                        \ dereference address into value
	48 +                     \ convert to ASCII numeral

	\ Since we are [in this file] going to override the standard `boot'
	\ routine with a custom one, you should know that we use $kernel
	\ when referencing the desired kernel. Set $kernel below.

	s" set kernel=${kernel_prefix}${kernel[N]}${kernel_suffix}"
	                          \ command to assemble full kernel-path
	-rot tuck 36 + c! swap    \ replace 'N' with array index value
	evaluate                  \ sets $kernel to full kernel-path

	TRUE \ loop menu again
;

: cycle_root ( N -- N TRUE )
	cycle_menuitem
	menu-redraw

	\ Now we're going to make the change effective

	s" cycle_stateN"         \ base name of array state var
	-rot 2dup 11 + c! rot    \ replace 'N' with ASCII numeral
	evaluate                 \ translate name into address
	@                        \ dereference address into value
	48 +                     \ convert to ASCII numeral

	\ Since we are [in this file] going to override the standard `boot'
	\ routine with a custom one, you should know that we use $root when
	\ booting. Set $root below.

	s" set root=${root_prefix}${root[N]}${root_prefix}"
	                          \ command to assemble full kernel-path
	-rot tuck 30 + c! swap    \ replace 'N' with array index value
	evaluate                  \ sets $kernel to full kernel-path

	TRUE \ loop menu again
;
