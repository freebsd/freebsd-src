\ Copyright (c) 1999 Daniel C. Sobral <dcs@freebsd.org>
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

s" arch-alpha" environment? [if] [if]
	s" loader_version" environment?  [if]
		3 < [if]
			.( Loader version 0.3+ required) cr
			abort
		[then]
	[else]
		.( Could not get loader version!) cr
		abort
	[then]
[then] [then]

s" arch-i386" environment? [if] [if]
	s" loader_version" environment?  [if]
		8 < [if]
			.( Loader version 0.8+ required) cr
			abort
		[then]
	[else]
		.( Could not get loader version!) cr
		abort
	[then]
[then] [then]

include /boot/support.4th

only forth definitions also support-functions

\ ***** boot-conf
\
\	Prepares to boot as specified by loaded configuration files.

also support-functions definitions

: bootpath s" /boot/" ;
: modulepath s" module_path" ;
: saveenv ( addr len | 0 -1 -- addr' len | 0 -1 )
  dup -1 = if exit then
  dup allocate abort" Out of memory"
  swap 2dup 2>r
  move
  2r>
;
: freeenv ( addr len | 0 -1 )
  -1 = if drop else free abort" Freeing error" then
;
: restoreenv  ( addr len | 0 -1 -- )
  dup -1 = if ( it wasn't set )
    2drop
    modulepath unsetenv
  else
    over >r
    modulepath setenv
    r> free abort" Freeing error"
  then
;

only forth also support-functions also builtins definitions

: boot-conf  ( args 1 | 0 "args" -- flag )
  0 1 unload drop

  0= if ( interpreted )
    \ Get next word on the command line
    bl word count
    ?dup 0= if ( there wasn't anything )
      drop 0
    else ( put in the number of strings )
      1
    then
  then ( interpreted )

  if ( there are arguments )
    \ Try to load the kernel
    s" kernel_options" getenv dup -1 = if drop 2dup 1 else 2over 2 then

    1 load if ( load command failed )
      \ Remove garbage from the stack

      \ Set the environment variable module_path, and try loading
      \ the kernel again.

      \ First, save module_path value
      modulepath getenv saveenv dup -1 = if 0 swap then 2>r

      \ Sets the new value
      2dup modulepath setenv

      \ Try to load the kernel
      s" load ${kernel} ${kernel_options}" ['] evaluate catch
      if ( load failed yet again )
	\ Remove garbage from the stack
	2drop

	\ Try prepending /boot/
	bootpath 2over nip over + allocate
	if ( out of memory )
	  2drop 2drop
	  2r> restoreenv
	  100 exit
	then

	0 2swap strcat 2swap strcat
	2dup modulepath setenv

	drop free if ( freeing memory error )
	  2drop
	  2r> restoreenv
	  100 exit
	then
 
	\ Now, once more, try to load the kernel
	s" load ${kernel} ${kernel_options}" ['] evaluate catch
	if ( failed once more )
	  2drop
	  2r> restoreenv
	  100 exit
	then

      else ( we found the kernel on the path passed )

	2drop ( discard command line arguments )

      then ( could not load kernel from directory passed )

      \ Load the remaining modules, if the kernel was loaded at all
      ['] load_modules catch if 2r> restoreenv 100 exit then

      \ Call autoboot to perform the booting
      0 1 autoboot

      \ Keep new module_path
      2r> freeenv

      exit
    then ( could not load kernel with name passed )

    2drop ( discard command line arguments )

  else ( try just a straight-forward kernel load )
    s" load ${kernel} ${kernel_options}" ['] evaluate catch
    if ( kernel load failed ) 2drop 100 exit then

  then ( there are command line arguments )

  \ Load the remaining modules, if the kernel was loaded at all
  ['] load_modules catch if 100 exit then

  \ Call autoboot to perform the booting
  0 1 autoboot
;

also forth definitions
builtin: boot-conf
only forth definitions also support-functions

\ ***** check-password
\
\	If a password was defined, execute autoboot and ask for
\	password if autoboot returns.

: check-password
  password .addr @ if
    0 autoboot
    false >r
    begin
      bell emit bell emit
      ." Password: "
      password .len @ read-password
      dup password .len @ = if
        2dup password .addr @ password .len @
        compare 0= if r> drop true >r then
      then
      drop free drop
      r@
    until
    r> drop
  then
;

\ ***** start
\
\       Initializes support.4th global variables, sets loader_conf_files,
\       process conf files, and, if any one such file was succesfully
\       read to the end, load kernel and modules.

: start  ( -- ) ( throws: abort & user-defined )
  s" /boot/defaults/loader.conf" initialize
  include_conf_files
  \ Will *NOT* try to load kernel and modules if no configuration file
  \ was succesfully loaded!
  any_conf_read? if
    load_kernel
    load_modules
  then
;

\ ***** initialize
\
\	Overrides support.4th initialization word with one that does
\	everything start one does, short of loading the kernel and
\	modules. Returns a flag

: initialize ( -- flag )
  s" /boot/defaults/loader.conf" initialize
  include_conf_files
  any_conf_read?
;

\ ***** read-conf
\
\	Read a configuration file, whose name was specified on the command
\	line, if interpreted, or given on the stack, if compiled in.

: (read-conf)  ( addr len -- )
  conf_files .addr @ ?dup if free abort" Fatal error freeing memory" then
  strdup conf_files .len ! conf_files .addr !
  include_conf_files \ Will recurse on new loader_conf_files definitions
;

: read-conf  ( <filename> | addr len -- ) ( throws: abort & user-defined )
  state @ if
    \ Compiling
    postpone (read-conf)
  else
    \ Interpreting
    bl parse (read-conf)
  then
; immediate

\ ***** enable-module
\
\       Turn a module loading on.

: enable-module ( <module> -- )
  bl parse module_options @ >r
  begin
    r@
  while
    2dup
    r@ module.name dup .addr @ swap .len @
    compare 0= if
      2drop
      r@ module.name dup .addr @ swap .len @ type
      true r> module.flag !
      ."  will be loaded." cr
      exit
    then
    r> module.next @ >r
  repeat
  r> drop
  type ."  wasn't found." cr
;

\ ***** disable-module
\
\       Turn a module loading off.

: disable-module ( <module> -- )
  bl parse module_options @ >r
  begin
    r@
  while
    2dup
    r@ module.name dup .addr @ swap .len @
    compare 0= if
      2drop
      r@ module.name dup .addr @ swap .len @ type
      false r> module.flag !
      ."  will not be loaded." cr
      exit
    then
    r> module.next @ >r
  repeat
  r> drop
  type ."  wasn't found." cr
;

\ ***** toggle-module
\
\       Turn a module loading on/off.

: toggle-module ( <module> -- )
  bl parse module_options @ >r
  begin
    r@
  while
    2dup
    r@ module.name dup .addr @ swap .len @
    compare 0= if
      2drop
      r@ module.name dup .addr @ swap .len @ type
      r@ module.flag @ 0= dup r> module.flag !
      if
        ."  will be loaded." cr
      else
        ."  will not be loaded." cr
      then
      exit
    then
    r> module.next @ >r
  repeat
  r> drop
  type ."  wasn't found." cr
;

\ ***** show-module
\
\	Show loading information about a module.

: show-module ( <module> -- )
  bl parse module_options @ >r
  begin
    r@
  while
    2dup
    r@ module.name dup .addr @ swap .len @
    compare 0= if
      2drop
      ." Name: " r@ module.name dup .addr @ swap .len @ type cr
      ." Path: " r@ module.loadname dup .addr @ swap .len @ type cr
      ." Type: " r@ module.type dup .addr @ swap .len @ type cr
      ." Flags: " r@ module.args dup .addr @ swap .len @ type cr
      ." Before load: " r@ module.beforeload dup .addr @ swap .len @ type cr
      ." After load: " r@ module.afterload dup .addr @ swap .len @ type cr
      ." Error: " r@ module.loaderror dup .addr @ swap .len @ type cr
      ." Status: " r> module.flag @ if ." Load" else ." Don't load" then cr
      exit
    then
    r> module.next @ >r
  repeat
  r> drop
  type ."  wasn't found." cr
;

\ Words to be used inside configuration files

: retry false ;         \ For use in load error commands
: ignore true ;         \ For use in load error commands

\ Return to strict forth vocabulary

only forth also

