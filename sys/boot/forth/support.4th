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

\ Loader.rc support functions:
\
\ initialize_support ( -- )	initialize global variables
\ initialize ( addr len -- )	as above, plus load_conf_files
\ load_conf ( addr len -- )	load conf file given
\ include_conf_files ( -- )	load all conf files in load_conf_files
\ print_syntax_error ( -- )	print line and marker of where a syntax
\				error was detected
\ print_line ( -- )		print last line processed
\ load_kernel ( -- )		load kernel
\ load_modules ( -- )		load modules flagged
\
\ Exported structures:
\
\ string			counted string structure
\	cell .addr			string address
\	cell .len			string length
\ module			module loading information structure
\	cell module.flag		should we load it?
\	string module.name		module's name
\	string module.loadname		name to be used in loading the module
\	string module.type		module's type
\	string module.args		flags to be passed during load
\	string module.beforeload	command to be executed before load
\	string module.afterload		command to be executed after load
\	string module.loaderror		command to be executed if load fails
\	cell module.next		list chain
\
\ Exported global variables;
\
\ string conf_files		configuration files to be loaded
\ string password		password
\ cell modules_options		pointer to first module information
\ value verbose?		indicates if user wants a verbose loading
\ value any_conf_read?		indicates if a conf file was succesfully read
\
\ Other exported words:
\
\ strdup ( addr len -- addr' len)			similar to strdup(3)
\ strcat ( addr len addr' len' -- addr len+len' )	similar to strcat(3)
\ strlen ( addr -- len )				similar to strlen(3)
\ s' ( | string' -- addr len | )			similar to s"
\ rudimentary structure support

\ Exception values

1 constant syntax_error
2 constant out_of_memory
3 constant free_error
4 constant set_error
5 constant read_error
6 constant open_error
7 constant exec_error
8 constant before_load_error
9 constant after_load_error

\ Crude structure support

: structure:
  create here 0 , ['] drop , 0
  does> create here swap dup @ allot cell+ @ execute
;
: member: create dup , over , + does> cell+ @ + ;
: ;structure swap ! ;
: constructor! >body cell+ ! ;
: constructor: over :noname ;
: ;constructor postpone ; swap cell+ ! ; immediate
: sizeof ' >body @ state @ if postpone literal then ; immediate
: offsetof ' >body cell+ @ state @ if postpone literal then ; immediate
: ptr 1 cells member: ;
: int 1 cells member: ;

\ String structure

structure: string
	ptr .addr
	int .len
	constructor:
	  0 over .addr !
	  0 swap .len !
	;constructor
;structure


\ Module options linked list

structure: module
	int module.flag
	sizeof string member: module.name
	sizeof string member: module.loadname
	sizeof string member: module.type
	sizeof string member: module.args
	sizeof string member: module.beforeload
	sizeof string member: module.afterload
	sizeof string member: module.loaderror
	ptr module.next
;structure

\ Internal loader structures
structure: preloaded_file
	ptr pf.name
	ptr pf.type
	ptr pf.args
	ptr pf.metadata	\ file_metadata
	int pf.loader
	int pf.addr
	int pf.size
	ptr pf.modules	\ kernel_module
	ptr pf.next	\ preloaded_file
;structure

structure: kernel_module
	ptr km.name
	\ ptr km.args
	ptr km.fp	\ preloaded_file
	ptr km.next	\ kernel_module
;structure

structure: file_metadata
	int		md.size
	2 member:	md.type	\ this is not ANS Forth compatible (XXX)
	ptr		md.next	\ file_metadata
	0 member:	md.data	\ variable size
;structure

structure: config_resource
	ptr cf.name
	int cf.type
0 constant RES_INT
1 constant RES_STRING
2 constant RES_LONG
	2 cells member: u
;structure

structure: config_device
	ptr cd.name
	int cd.unit
	int cd.resource_count
	ptr cd.resources	\ config_resource
;structure

structure: STAILQ_HEAD
	ptr stqh_first	\ type*
	ptr stqh_last	\ type**
;structure

structure: STAILQ_ENTRY
	ptr stqe_next	\ type*
;structure

structure: pnphandler
	ptr pnph.name
	ptr pnph.enumerate
;structure

structure: pnpident
	ptr pnpid.ident					\ char*
	sizeof STAILQ_ENTRY cells member: pnpid.link	\ pnpident
;structure

structure: pnpinfo
	ptr pnpi.desc
	int pnpi.revision
	ptr pnpi.module				\ (char*) module args
	int pnpi.argc
	ptr pnpi.argv
	ptr pnpi.handler			\ pnphandler
	sizeof STAILQ_HEAD member: pnpi.ident	\ pnpident
	sizeof STAILQ_ENTRY member: pnpi.link	\ pnpinfo
;structure

\ Global variables

string conf_files
string password
create module_options sizeof module.next allot 0 module_options !
create last_module_option sizeof module.next allot 0 last_module_option !
0 value verbose?

\ Support string functions

: strdup  ( addr len -- addr' len )
  >r r@ allocate if out_of_memory throw then
  tuck r@ move
  r>
;

: strcat  { addr len addr' len' -- addr len+len' }
  addr' addr len + len' move
  addr len len' +
;

: strlen ( addr -- len )
  0 >r
  begin
    dup c@ while
    1+ r> 1+ >r repeat
  drop r>
;

: s' 
  [char] ' parse
  state @ if
    postpone sliteral
  then
; immediate

: 2>r postpone >r postpone >r ; immediate
: 2r> postpone r> postpone r> ; immediate

\ Private definitions

vocabulary support-functions
only forth also support-functions definitions

\ Some control characters constants

7 constant bell
8 constant backspace
9 constant tab
10 constant lf
13 constant <cr>

\ Read buffer size

80 constant read_buffer_size

\ Standard suffixes

: load_module_suffix s" _load" ;
: module_loadname_suffix s" _name" ;
: module_type_suffix s" _type" ;
: module_args_suffix s" _flags" ;
: module_beforeload_suffix s" _before" ;
: module_afterload_suffix s" _after" ;
: module_loaderror_suffix s" _error" ;

\ Support operators

: >= < 0= ;
: <= > 0= ;

\ Assorted support funcitons

: free-memory free if free_error throw then ;

\ Assignment data temporary storage

string name_buffer
string value_buffer

\ Line by line file reading functions
\
\ exported:
\	line_buffer
\	end_of_file?
\	fd
\	read_line
\	reset_line_reading

vocabulary line-reading
also line-reading definitions also

\ File data temporary storage

string read_buffer
0 value read_buffer_ptr

\ File's line reading function

support-functions definitions

string line_buffer
0 value end_of_file?
variable fd

line-reading definitions

: skip_newlines
  begin
    read_buffer .len @ read_buffer_ptr >
  while
    read_buffer .addr @ read_buffer_ptr + c@ lf = if
      read_buffer_ptr char+ to read_buffer_ptr
    else
      exit
    then
  repeat
;

: scan_buffer  ( -- addr len )
  read_buffer_ptr >r
  begin
    read_buffer .len @ r@ >
  while
    read_buffer .addr @ r@ + c@ lf = if
      read_buffer .addr @ read_buffer_ptr +  ( -- addr )
      r@ read_buffer_ptr -                   ( -- len )
      r> to read_buffer_ptr
      exit
    then
    r> char+ >r
  repeat
  read_buffer .addr @ read_buffer_ptr +  ( -- addr )
  r@ read_buffer_ptr -                   ( -- len )
  r> to read_buffer_ptr
;

: line_buffer_resize  ( len -- len )
  >r
  line_buffer .len @ if
    line_buffer .addr @
    line_buffer .len @ r@ +
    resize if out_of_memory throw then
  else
    r@ allocate if out_of_memory throw then
  then
  line_buffer .addr !
  r>
;
    
: append_to_line_buffer  ( addr len -- )
  line_buffer .addr @ line_buffer .len @
  2swap strcat
  line_buffer .len !
  drop
;

: read_from_buffer
  scan_buffer            ( -- addr len )
  line_buffer_resize     ( len -- len )
  append_to_line_buffer  ( addr len -- )
;

: refill_required?
  read_buffer .len @ read_buffer_ptr =
  end_of_file? 0= and
;

: refill_buffer
  0 to read_buffer_ptr
  read_buffer .addr @ 0= if
    read_buffer_size allocate if out_of_memory throw then
    read_buffer .addr !
  then
  fd @ read_buffer .addr @ read_buffer_size fread
  dup -1 = if read_error throw then
  dup 0= if true to end_of_file? then
  read_buffer .len !
;

: reset_line_buffer
  line_buffer .addr @ ?dup if
    free-memory
  then
  0 line_buffer .addr !
  0 line_buffer .len !
;

support-functions definitions

: reset_line_reading
  0 to read_buffer_ptr
;

: read_line
  reset_line_buffer
  skip_newlines
  begin
    read_from_buffer
    refill_required?
  while
    refill_buffer
  repeat
;

only forth also support-functions definitions

\ Conf file line parser:
\ <line> ::= <spaces><name><spaces>'='<spaces><value><spaces>[<comment>] |
\            <spaces>[<comment>]
\ <name> ::= <letter>{<letter>|<digit>|'_'}
\ <value> ::= '"'{<character_set>|'\'<anything>}'"' | <name>
\ <character_set> ::= ASCII 32 to 126, except '\' and '"'
\ <comment> ::= '#'{<anything>}
\
\ exported:
\	line_pointer
\	process_conf

0 value line_pointer

vocabulary file-processing
also file-processing definitions

\ parser functions
\
\ exported:
\	get_assignment

vocabulary parser
also parser definitions also

0 value parsing_function
0 value end_of_line

: end_of_line?
  line_pointer end_of_line =
;

: letter?
  line_pointer c@ >r
  r@ [char] A >=
  r@ [char] Z <= and
  r@ [char] a >=
  r> [char] z <= and
  or
;

: digit?
  line_pointer c@ >r
  r@ [char] 0 >=
  r> [char] 9 <= and
;

: quote?
  line_pointer c@ [char] " =
;

: assignment_sign?
  line_pointer c@ [char] = =
;

: comment?
  line_pointer c@ [char] # =
;

: space?
  line_pointer c@ bl =
  line_pointer c@ tab = or
;

: backslash?
  line_pointer c@ [char] \ =
;

: underscore?
  line_pointer c@ [char] _ =
;

: dot?
  line_pointer c@ [char] . =
;

: skip_character
  line_pointer char+ to line_pointer
;

: skip_to_end_of_line
  end_of_line to line_pointer
;

: eat_space
  begin
    space?
  while
    skip_character
    end_of_line? if exit then
  repeat
;

: parse_name  ( -- addr len )
  line_pointer
  begin
    letter? digit? underscore? dot? or or or
  while
    skip_character
    end_of_line? if 
      line_pointer over -
      strdup
      exit
    then
  repeat
  line_pointer over -
  strdup
;

: remove_backslashes  { addr len | addr' len' -- addr' len' }
  len allocate if out_of_memory throw then
  to addr'
  addr >r
  begin
    addr c@ [char] \ <> if
      addr c@ addr' len' + c!
      len' char+ to len'
    then
    addr char+ to addr
    r@ len + addr =
  until
  r> drop
  addr' len'
;

: parse_quote  ( -- addr len )
  line_pointer
  skip_character
  end_of_line? if syntax_error throw then
  begin
    quote? 0=
  while
    backslash? if
      skip_character
      end_of_line? if syntax_error throw then
    then
    skip_character
    end_of_line? if syntax_error throw then 
  repeat
  skip_character
  line_pointer over -
  remove_backslashes
;

: read_name
  parse_name		( -- addr len )
  name_buffer .len !
  name_buffer .addr !
;

: read_value
  quote? if
    parse_quote		( -- addr len )
  else
    parse_name		( -- addr len )
  then
  value_buffer .len !
  value_buffer .addr !
;

: comment
  skip_to_end_of_line
;

: white_space_4
  eat_space
  comment? if ['] comment to parsing_function exit then
  end_of_line? 0= if syntax_error throw then
;

: variable_value
  read_value
  ['] white_space_4 to parsing_function
;

: white_space_3
  eat_space
  letter? digit? quote? or or if
    ['] variable_value to parsing_function exit
  then
  syntax_error throw
;

: assignment_sign
  skip_character
  ['] white_space_3 to parsing_function
;

: white_space_2
  eat_space
  assignment_sign? if ['] assignment_sign to parsing_function exit then
  syntax_error throw
;

: variable_name
  read_name
  ['] white_space_2 to parsing_function
;

: white_space_1
  eat_space
  letter?  if ['] variable_name to parsing_function exit then
  comment? if ['] comment to parsing_function exit then
  end_of_line? 0= if syntax_error throw then
;

file-processing definitions

: get_assignment
  line_buffer .addr @ line_buffer .len @ + to end_of_line
  line_buffer .addr @ to line_pointer
  ['] white_space_1 to parsing_function
  begin
    end_of_line? 0=
  while
    parsing_function execute
  repeat
  parsing_function ['] comment =
  parsing_function ['] white_space_1 =
  parsing_function ['] white_space_4 =
  or or 0= if syntax_error throw then
;

only forth also support-functions also file-processing definitions also

\ Process line

: assignment_type?  ( addr len -- flag )
  name_buffer .addr @ name_buffer .len @
  compare 0=
;

: suffix_type?  ( addr len -- flag )
  name_buffer .len @ over <= if 2drop false exit then
  name_buffer .len @ over - name_buffer .addr @ +
  over compare 0=
;

: loader_conf_files?
  s" loader_conf_files" assignment_type?
;

: verbose_flag?
  s" verbose_loading" assignment_type?
;

: execute?
  s" exec" assignment_type?
;

: password?
  s" password" assignment_type?
;

: module_load?
  load_module_suffix suffix_type?
;

: module_loadname?
  module_loadname_suffix suffix_type?
;

: module_type?
  module_type_suffix suffix_type?
;

: module_args?
  module_args_suffix suffix_type?
;

: module_beforeload?
  module_beforeload_suffix suffix_type?
;

: module_afterload?
  module_afterload_suffix suffix_type?
;

: module_loaderror?
  module_loaderror_suffix suffix_type?
;

: set_conf_files
  conf_files .addr @ ?dup if
    free-memory
  then
  value_buffer .addr @ c@ [char] " = if
    value_buffer .addr @ char+ value_buffer .len @ 2 chars -
  else
    value_buffer .addr @ value_buffer .len @
  then
  strdup
  conf_files .len ! conf_files .addr !
;

: append_to_module_options_list  ( addr -- )
  module_options @ 0= if
    dup module_options !
    last_module_option !
  else
    dup last_module_option @ module.next !
    last_module_option !
  then
;

: set_module_name  ( addr -- )
  name_buffer .addr @ name_buffer .len @
  strdup
  >r over module.name .addr !
  r> swap module.name .len !
;

: yes_value?
  value_buffer .addr @ value_buffer .len @
  2dup s' "YES"' compare >r
  2dup s' "yes"' compare >r
  2dup s" YES" compare >r
  s" yes" compare r> r> r> and and and 0=
;

: find_module_option  ( -- addr | 0 )
  module_options @
  begin
    dup
  while
    dup module.name dup .addr @ swap .len @
    name_buffer .addr @ name_buffer .len @
    compare 0= if exit then
    module.next @
  repeat
;

: new_module_option  ( -- addr )
  sizeof module allocate if out_of_memory throw then
  dup sizeof module erase
  dup append_to_module_options_list
  dup set_module_name
;

: get_module_option  ( -- addr )
  find_module_option
  ?dup 0= if new_module_option then
;

: set_module_flag
  name_buffer .len @ load_module_suffix nip - name_buffer .len !
  yes_value? get_module_option module.flag !
;

: set_module_args
  name_buffer .len @ module_args_suffix nip - name_buffer .len !
  get_module_option module.args
  dup .addr @ ?dup if free-memory then
  value_buffer .addr @ value_buffer .len @
  over c@ [char] " = if
    2 chars - swap char+ swap
  then
  strdup
  >r over .addr !
  r> swap .len !
;

: set_module_loadname
  name_buffer .len @ module_loadname_suffix nip - name_buffer .len !
  get_module_option module.loadname
  dup .addr @ ?dup if free-memory then
  value_buffer .addr @ value_buffer .len @
  over c@ [char] " = if
    2 chars - swap char+ swap
  then
  strdup
  >r over .addr !
  r> swap .len !
;

: set_module_type
  name_buffer .len @ module_type_suffix nip - name_buffer .len !
  get_module_option module.type
  dup .addr @ ?dup if free-memory then
  value_buffer .addr @ value_buffer .len @
  over c@ [char] " = if
    2 chars - swap char+ swap
  then
  strdup
  >r over .addr !
  r> swap .len !
;

: set_module_beforeload
  name_buffer .len @ module_beforeload_suffix nip - name_buffer .len !
  get_module_option module.beforeload
  dup .addr @ ?dup if free-memory then
  value_buffer .addr @ value_buffer .len @
  over c@ [char] " = if
    2 chars - swap char+ swap
  then
  strdup
  >r over .addr !
  r> swap .len !
;

: set_module_afterload
  name_buffer .len @ module_afterload_suffix nip - name_buffer .len !
  get_module_option module.afterload
  dup .addr @ ?dup if free-memory then
  value_buffer .addr @ value_buffer .len @
  over c@ [char] " = if
    2 chars - swap char+ swap
  then
  strdup
  >r over .addr !
  r> swap .len !
;

: set_module_loaderror
  name_buffer .len @ module_loaderror_suffix nip - name_buffer .len !
  get_module_option module.loaderror
  dup .addr @ ?dup if free-memory then
  value_buffer .addr @ value_buffer .len @
  over c@ [char] " = if
    2 chars - swap char+ swap
  then
  strdup
  >r over .addr !
  r> swap .len !
;

: set_environment_variable
  name_buffer .len @
  value_buffer .len @ +
  5 chars +
  allocate if out_of_memory throw then
  dup 0  ( addr -- addr addr len )
  s" set " strcat
  name_buffer .addr @ name_buffer .len @ strcat
  s" =" strcat
  value_buffer .addr @ value_buffer .len @ strcat
  ['] evaluate catch if
    2drop free drop
    set_error throw
  else
    free-memory
  then
;

: set_verbose
  yes_value? to verbose?
;

: execute_command
  value_buffer .addr @ value_buffer .len @
  over c@ [char] " = if
    2 - swap char+ swap
  then
  ['] evaluate catch if exec_error throw then
;

: set_password
  password .addr @ ?dup if free if free_error throw then then
  value_buffer .addr @ c@ [char] " = if
    value_buffer .addr @ char+ value_buffer .len @ 2 - strdup
    value_buffer .addr @ free if free_error throw then
  else
    value_buffer .addr @ value_buffer .len @
  then
  password .len ! password .addr !
  0 value_buffer .addr !
;

: process_assignment
  name_buffer .len @ 0= if exit then
  loader_conf_files?	if set_conf_files exit then
  verbose_flag?		if set_verbose exit then
  execute?		if execute_command exit then
  password?		if set_password exit then
  module_load?		if set_module_flag exit then
  module_loadname?	if set_module_loadname exit then
  module_type?		if set_module_type exit then
  module_args?		if set_module_args exit then
  module_beforeload?	if set_module_beforeload exit then
  module_afterload?	if set_module_afterload exit then
  module_loaderror?	if set_module_loaderror exit then
  set_environment_variable
;

\ free_buffer  ( -- )
\
\ Free some pointers if needed. The code then tests for errors
\ in freeing, and throws an exception if needed. If a pointer is
\ not allocated, it's value (0) is used as flag.

: free_buffers
  name_buffer .addr @ dup if free then
  value_buffer .addr @ dup if free then
  or if free_error throw then
;

: reset_assignment_buffers
  0 name_buffer .addr !
  0 name_buffer .len !
  0 value_buffer .addr !
  0 value_buffer .len !
;

\ Higher level file processing

support-functions definitions

: process_conf
  begin
    end_of_file? 0=
  while
    reset_assignment_buffers
    read_line
    get_assignment
    ['] process_assignment catch
    ['] free_buffers catch
    swap throw throw
  repeat
;

only forth also support-functions definitions

: create_null_terminated_string  { addr len -- addr' len }
  len char+ allocate if out_of_memory throw then
  >r
  addr r@ len move
  0 r@ len + c!
  r> len
;

\ Interface to loading conf files

: load_conf  ( addr len -- )
  0 to end_of_file?
  reset_line_reading
  create_null_terminated_string
  over >r
  fopen fd !
  r> free-memory
  fd @ -1 = if open_error throw then
  ['] process_conf catch
  fd @ fclose
  throw
;

: print_line
  line_buffer .addr @ line_buffer .len @ type cr
;

: print_syntax_error
  line_buffer .addr @ line_buffer .len @ type cr
  line_buffer .addr @
  begin
    line_pointer over <>
  while
    bl emit
    char+
  repeat
  drop
  ." ^" cr
;

\ Depuration support functions

only forth definitions also support-functions

: test-file 
  ['] load_conf catch dup .
  syntax_error = if cr print_syntax_error then
;

: show-module-options
  module_options @
  begin
    ?dup
  while
    ." Name: " dup module.name dup .addr @ swap .len @ type cr
    ." Path: " dup module.loadname dup .addr @ swap .len @ type cr
    ." Type: " dup module.type dup .addr @ swap .len @ type cr
    ." Flags: " dup module.args dup .addr @ swap .len @ type cr
    ." Before load: " dup module.beforeload dup .addr @ swap .len @ type cr
    ." After load: " dup module.afterload dup .addr @ swap .len @ type cr
    ." Error: " dup module.loaderror dup .addr @ swap .len @ type cr
    ." Status: " dup module.flag @ if ." Load" else ." Don't load" then cr
    module.next @
  repeat
;

only forth also support-functions definitions

\ Variables used for processing multiple conf files

string current_file_name
variable current_conf_files

\ Indicates if any conf file was succesfully read

0 value any_conf_read?

\ loader_conf_files processing support functions

: set_current_conf_files
  conf_files .addr @ current_conf_files !
;

: get_conf_files
  conf_files .addr @ conf_files .len @ strdup
;

: recurse_on_conf_files?
  current_conf_files @ conf_files .addr @ <>
;

: skip_leading_spaces  { addr len pos -- addr len pos' }
  begin
    pos len = if addr len pos exit then
    addr pos + c@ bl =
  while
    pos char+ to pos
  repeat
  addr len pos
;

: get_file_name  { addr len pos -- addr len pos' addr' len' || 0 }
  pos len = if 
    addr free abort" Fatal error freeing memory"
    0 exit
  then
  pos >r
  begin
    addr pos + c@ bl <>
  while
    pos char+ to pos
    pos len = if
      addr len pos addr r@ + pos r> - exit
    then
  repeat
  addr len pos addr r@ + pos r> -
;

: get_next_file  ( addr len ptr -- addr len ptr' addr' len' | 0 )
  skip_leading_spaces
  get_file_name
;

: set_current_file_name
  over current_file_name .addr !
  dup current_file_name .len !
;

: print_current_file
  current_file_name .addr @ current_file_name .len @ type
;

: process_conf_errors
  dup 0= if true to any_conf_read? drop exit then
  >r 2drop r>
  dup syntax_error = if
    ." Warning: syntax error on file " print_current_file cr
    print_syntax_error drop exit
  then
  dup set_error = if
    ." Warning: bad definition on file " print_current_file cr
    print_line drop exit
  then
  dup read_error = if
    ." Warning: error reading file " print_current_file cr drop exit
  then
  dup open_error = if
    verbose? if ." Warning: unable to open file " print_current_file cr then
    drop exit
  then
  dup free_error = abort" Fatal error freeing memory"
  dup out_of_memory = abort" Out of memory"
  throw  \ Unknown error -- pass ahead
;

\ Process loader_conf_files recursively
\ Interface to loader_conf_files processing

: include_conf_files
  set_current_conf_files
  get_conf_files 0
  begin
    get_next_file ?dup
  while
    set_current_file_name
    ['] load_conf catch
    process_conf_errors
    recurse_on_conf_files? if recurse then
  repeat
;

\ Module loading functions

: load_module?
  module.flag @
;

: load_parameters  ( addr -- addr addrN lenN ... addr1 len1 N )
  dup >r
  r@ module.args .addr @ r@ module.args .len @
  r@ module.loadname .len @ if
    r@ module.loadname .addr @ r@ module.loadname .len @
  else
    r@ module.name .addr @ r@ module.name .len @
  then
  r@ module.type .len @ if
    r@ module.type .addr @ r@ module.type .len @
    s" -t "
    4 ( -t type name flags )
  else
    2 ( name flags )
  then
  r> drop
;

: before_load  ( addr -- addr )
  dup module.beforeload .len @ if
    dup module.beforeload .addr @ over module.beforeload .len @
    ['] evaluate catch if before_load_error throw then
  then
;

: after_load  ( addr -- addr )
  dup module.afterload .len @ if
    dup module.afterload .addr @ over module.afterload .len @
    ['] evaluate catch if after_load_error throw then
  then
;

: load_error  ( addr -- addr )
  dup module.loaderror .len @ if
    dup module.loaderror .addr @ over module.loaderror .len @
    evaluate  \ This we do not intercept so it can throw errors
  then
;

: pre_load_message  ( addr -- addr )
  verbose? if
    dup module.name .addr @ over module.name .len @ type
    ." ..."
  then
;

: load_error_message verbose? if ." failed!" cr then ;

: load_succesful_message verbose? if ." ok" cr then ;

: load_module
  load_parameters load
;

: process_module  ( addr -- addr )
  pre_load_message
  before_load
  begin
    ['] load_module catch if
      dup module.loaderror .len @ if
        load_error			\ Command should return a flag!
      else 
        load_error_message true		\ Do not retry
      then
    else
      after_load
      load_succesful_message true	\ Succesful, do not retry
    then
  until
;

: process_module_errors  ( addr ior -- )
  dup before_load_error = if
    drop
    ." Module "
    dup module.name .addr @ over module.name .len @ type
    dup module.loadname .len @ if
      ." (" dup module.loadname .addr @ over module.loadname .len @ type ." )"
    then
    cr
    ." Error executing "
    dup module.beforeload .addr @ over module.afterload .len @ type cr
    abort
  then

  dup after_load_error = if
    drop
    ." Module "
    dup module.name .addr @ over module.name .len @ type
    dup module.loadname .len @ if
      ." (" dup module.loadname .addr @ over module.loadname .len @ type ." )"
    then
    cr
    ." Error executing "
    dup module.afterload .addr @ over module.afterload .len @ type cr
    abort
  then

  throw  \ Don't know what it is all about -- pass ahead
;

\ Module loading interface

: load_modules  ( -- ) ( throws: abort & user-defined )
  module_options @
  begin
    ?dup
  while
    dup load_module? if
      ['] process_module catch
      process_module_errors
    then
    module.next @
  repeat
;

\ h00h00 magic used to try loading either a kernel with a given name,
\ or a kernel with the default name in a directory of a given name
\ (the pain!)

: bootpath s" /boot/" ;
: modulepath s" module_path" ;

\ Functions used to save and restore module_path's value.
: saveenv ( addr len | -1 -- addr' len | 0 -1 )
  dup -1 = if 0 swap exit then
  strdup
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

: clip_args   \ Drop second string if only one argument is passed
  1 = if
    2swap 2drop
    1
  else
    2
  then
;

also builtins

\ Parse filename from a comma-separated list

: parse-; ( addr len -- addr' len-x addr x )
  over 0 2swap
  begin
    dup 0 <>
  while
    over c@ [char] ; <>
  while
    1- swap 1+ swap
    2swap 1+ 2swap
  repeat then
  dup 0 <> if
    1- swap 1+ swap
  then
  2swap
;

\ Try loading one of multiple kernels specified

: try_multiple_kernels ( addr len addr' len' args -- flag )
  >r
  begin
    parse-; 2>r
    2over 2r>
    r@ clip_args 1 load
  while
    dup 0=
  until
    1 >r \ Failure
  else
    0 >r \ Success
  then
  2drop 2drop
  r>
  r> drop
;

\ Try to load a kernel; the kernel name is taken from one of
\ the following lists, as ordered:
\
\   1. The "kernel" environment variable
\   2. The "bootfile" environment variable
\
\ Flags are passed, if available. The parameter args must be 2
\ if flags are being passed, or 1 if they should be ignored.
\ Dummy flags and len must be passed in the latter case.
\
\ The kernel gets loaded from the current module_path.

: load_a_kernel ( flags len args -- flag )
  local args
  2local flags
  0 0 2local kernel
  end-locals

  \ Check if a default kernel name exists at all, exits if not
  s" kernel" getenv dup -1 <> if
    to kernel
    flags kernel args try_multiple_kernels
    dup 0= if exit then
  then
  drop

  s" bootfile" getenv dup -1 <> if
    to kernel
  else
    drop
    1 exit \ Failure
  then

  \ Try all default kernel names
  flags kernel args try_multiple_kernels
;

\ Try to load a kernel; the kernel name is taken from one of
\ the following lists, as ordered:
\
\   1. The "kernel" environment variable
\   2. The "bootfile" environment variable
\
\ Flags are passed, if provided.
\
\ The kernel will be loaded from a directory computed from the
\ path given. Two directories will be tried in the following order:
\
\   1. /boot/path
\   2. path
\
\ The module_path variable is overridden if load is succesful, by
\ prepending the successful path.

: load_from_directory ( path len 1 | flags len' path len 2 -- flag )
  local args
  2local path
  args 1 = if 0 0 then
  2local flags
  0 0 2local oldmodulepath
  0 0 2local newmodulepath
  end-locals

  \ Set the environment variable module_path, and try loading
  \ the kernel again.
  modulepath getenv saveenv to oldmodulepath

  \ Try prepending /boot/ first
  bootpath nip path nip + 
  oldmodulepath nip dup -1 = if
    drop
  else
    1+ +
  then
  allocate
  if ( out of memory )
    1 exit
  then

  0
  bootpath strcat
  path strcat
  2dup to newmodulepath
  modulepath setenv

  \ Try all default kernel names
  flags args load_a_kernel
  0= if ( success )
    oldmodulepath nip -1 <> if
      newmodulepath s" ;" strcat
      oldmodulepath strcat
      modulepath setenv
      newmodulepath drop free-memory
      oldmodulepath drop free-memory
    then
    0 exit
  then

  \ Well, try without the prepended /boot/
  path newmodulepath drop swap move
  path nip
  2dup to newmodulepath
  modulepath setenv

  \ Try all default kernel names
  flags args load_a_kernel
  if ( failed once more )
    oldmodulepath restoreenv
    newmodulepath drop free-memory
    1
  else
    oldmodulepath nip -1 <> if
      newmodulepath s" ;" strcat
      oldmodulepath strcat
      modulepath setenv
      newmodulepath drop free-memory
      oldmodulepath drop free-memory
    then
    0
  then
;

\ Try to load a kernel; the kernel name is taken from one of
\ the following lists, as ordered:
\
\   1. The "kernel" environment variable
\   2. The "bootfile" environment variable
\   3. The "path" argument
\
\ Flags are passed, if provided.
\
\ The kernel will be loaded from a directory computed from the
\ path given. Two directories will be tried in the following order:
\
\   1. /boot/path
\   2. path
\
\ Unless "path" is meant to be kernel name itself. In that case, it
\ will first be tried as a full path, and, next, search on the
\ directories pointed by module_path.
\
\ The module_path variable is overridden if load is succesful, by
\ prepending the successful path.

: load_directory_or_file ( path len 1 | flags len' path len 2 -- flag )
  local args
  2local path
  args 1 = if 0 0 then
  2local flags
  end-locals

  \ First, assume path is an absolute path to a directory
  flags path args clip_args load_from_directory
  dup 0= if exit else drop then

  \ Next, assume path points to the kernel
  flags path args try_multiple_kernels
;

: load_kernel_and_modules ( flags len path len' 2 | path len' 1 -- flag )
  load_directory_or_file
  0= if ['] load_modules catch then
;

: initialize  ( addr len -- )
  strdup conf_files .len ! conf_files .addr !
;

: kernel_options ( -- addr len 2 | 0 0 1 )
  s" kernel_options" getenv
  dup -1 = if 0 0 1 else 2 then
;

: kernel_and_options
  kernel_options
  s" kernel" getenv
  rot
;

: load_kernel  ( -- ) ( throws: abort )
  s" kernel" getenv
  dup -1 = if
    \ If unset, try any kernel
    drop
    kernel_options load_a_kernel
  else
    \ If set, try first directory, next file name
    kernel_options >r 2swap r> clip_args load_from_directory
    dup if
      drop
      kernel_and_options try_multiple_kernels
    then
  then
  abort" Unable to load a kernel!"
;
 
: read-password { size | buf len -- }
  size allocate if out_of_memory throw then
  to buf
  0 to len
  begin
    key
    dup backspace = if
      drop
      len if
        backspace emit bl emit backspace emit
        len 1 - to len
      else
        bell emit
      then
    else
      dup <cr> = if cr drop buf len exit then
      [char] * emit
      len size < if
        buf len chars + c!
      else
        drop
      then
      len 1+ to len
    then
  again
;

\ Go back to straight forth vocabulary

only forth also definitions

