\ Copyright (c) 2003 Scott Long <scottl@freebsd.org>
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

marker task-beastie.4th

include /boot/screen.4th
include /boot/frames.4th

hide

variable menuidx
variable menubllt
variable menuX
variable menuY
variable promptwidth

variable bootkey
variable bootacpikey
variable bootsafekey
variable bootverbosekey
variable bootsinglekey
variable escapekey
variable rebootkey

46 constant dot

\ The BSD Daemon.  He is 19 rows high and 34 columns wide
: print-beastie ( x y -- )
	2dup at-xy ."              ,        ," 1+
	2dup at-xy ."             /(        )`" 1+
	2dup at-xy ."             \ \___   / |" 1+
	2dup at-xy ."             /- _  `-/  '" 1+
	2dup at-xy ."            (/\/ \ \   /\" 1+
	2dup at-xy ."            / /   | `    \" 1+
	2dup at-xy ."            O O   ) /    |" 1+
	2dup at-xy ."            `-^--'`<     '" 1+
	2dup at-xy ."           (_.)  _  )   /" 1+
	2dup at-xy ."            `.___/`    /" 1+
	2dup at-xy ."              `-----' /" 1+
	2dup at-xy ." <----.     __ / __   \" 1+
	2dup at-xy ." <----|====O)))==) \) /====" 1+
	2dup at-xy ." <----'    `--' `.__,' \" 1+
	2dup at-xy ."              |        |" 1+
	2dup at-xy ."               \       /       /\" 1+
	2dup at-xy ."          ______( (_  / \______/" 1+
	2dup at-xy ."        ,'  ,-----'   |" 1+
	     at-xy ."        `--{__________)"
;

: acpienabled? ( -- flag )
	s" acpi_load" getenv
	dup -1 = if
		drop false exit
	then
	s" YES" compare-insensitive 0<> if
		false exit
	then
	s" hint.acpi.0.disabled" getenv
	dup -1 <> if
		s" 0" compare 0<> if
			false exit
		then
	then
	true
;

: printmenuitem ( -- n )
	menuidx @
	1+ dup
	menuidx !
	menuY @ + dup menuX @ swap at-xy
	menuidx @ .
	menuX @ 1+ swap at-xy
	menubllt @ emit
	menuidx @ 48 +
;

: beastie-menu ( -- )
	0 menuidx !
	dot menubllt !
	8 menuY !
	5 menuX !
	clear
	46 4 print-beastie
	42 20 2 2 box
	13 6 at-xy ." Welcome to FreeBSD!"
	printmenuitem ."  Boot FreeBSD [default]" bootkey !
	s" arch-i386" environment? if
		printmenuitem ."  Boot FreeBSD with ACPI " bootacpikey !
		acpienabled? if
			." disabled"
		else
			." enabled"
		then
	else
		-2 bootacpikey !
	then
	printmenuitem ."  Boot FreeBSD in Safe Mode" bootsafekey !
	printmenuitem ."  Boot FreeBSD in single user mode" bootsinglekey !
	printmenuitem ."  Boot FreeBSD with verbose logging" bootverbosekey !
	printmenuitem ."  Escape to loader prompt" escapekey !
	printmenuitem ."  Reboot" rebootkey !
	menuX @ 20 at-xy
	." Select option, [Enter] for default"
	menuX @ 21 at-xy
	s" or [Space] to pause timer    " dup 2 - promptwidth !
	type
;

: tkey
	dup
	seconds +
	begin 1 while
		over 0<> if
			dup seconds u< if
				drop
				-1
				exit
			then
			menuX @ promptwidth @ + 21 at-xy dup seconds - .
		then
		key? if
			drop
			key
			exit
		then
	50 ms
	repeat
;

set-current

: beastie-start
	beastie-menu
	s" autoboot_delay" getenv
	dup -1 = if
		drop
		10
	else
		0 0 2swap >number drop drop drop
	then
	begin true while
		dup tkey
		0 25 at-xy
		dup 32 = if nip 0 swap then
		dup -1 = if s" boot" evaluate then
		dup 13 = if s" boot" evaluate then
		dup bootkey @ = if s" boot" evaluate then
		dup bootacpikey @ = if
			acpienabled? if
				s" acpi_load" unsetenv
				s" 1" s" hint.acpi.0.disabled" setenv
				s" 1" s" loader.acpi_disabled_by_user" setenv
			else
				s" YES" s" acpi_load" setenv
				s" 0" s" hint.acpi.0.disabled" setenv
			then
			s" boot" evaluate
		then
		dup bootsafekey @ = if
			s" arch-i386" environment? if
				s" acpi_load" unsetenv
				s" 1" s" hint.acpi.0.disabled" setenv
				s" 1" s" loader.acpi_disabled_by_user" setenv
			then
			s" 0" s" hw.ata.ata_dma" setenv
			s" 0" s" hw.ata.atapi_dma" setenv
			s" 0" s" hw.ata.wc" setenv
			s" 0" s" hw.eisa_slots" setenv
			s" boot" evaluate
		then
		dup bootverbosekey @ = if
			s" YES" s" boot_verbose" setenv
			s" boot" evaluate
		then
		dup bootsinglekey @ = if
			s" YES" s" boot_single" setenv
			s" boot" evaluate
		then
		dup escapekey @ = if 2drop exit then
		rebootkey @ = if s" reboot" evaluate then
	repeat
;

previous
