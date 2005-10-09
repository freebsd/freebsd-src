#	@(#)init.tcl	8.10 (Berkeley) 7/19/96
proc screen {} {
	global tk_ssize_row
	global tk_ssize_col

	# Build menubar with File, Options and Help entries.
	frame .menu -relief raised -borderwidth 1
	pack append . .menu {top fillx}

	# File pull-down menu
	menubutton .menu.file -text "File" \
	    -menu .menu.file.fileops -underline 0
	menu .menu.file.fileops
	.menu.file.fileops add command -label "Edit ..." \
	    -command "tk_edit" -underline 0
	.menu.file.fileops add command -label "Save File" \
	    -command "tk_write" -underline 0
	.menu.file.fileops add command -label "Save File as ..." \
	    -command "tk_writeas" -underline 1
	.menu.file.fileops add command -label "Save and Quit" \
	    -command "tk_writequit" -underline 7
	.menu.file.fileops add command -label "Quit" \
	    -command "tk_quit" -underline 0

	# Options pull-down menu
	menubutton .menu.option -text "Options" \
	    -menu .menu.option.optionops -underline 0
	menu .menu.option.optionops
	.menu.option.optionops add command -label "Set all" \
	    -command tk_options -underline 0

	# Help pull-down menu
	menubutton .menu.help -text "Help" \
	    -menu .menu.help.helpops -underline 0
	menu .menu.help.helpops
	.menu.help.helpops add command -label "On Help" -underline 3 \
	    -command tk_help
	.menu.help.helpops add command -label "On Version" -underline 3 \
	    -command tk_version

	pack append .menu \
	    .menu.file {left} .menu.option {left} .menu.help {right}

	# Set up for keyboard-based menu traversal
	tk_bindForTraversal .
	bind . <Any-Enter> {focus .}
	focus .
	tk_menuBar .menu .menu.file .menu.help

	# Create text window
	text .t -relief raised -bd 1 -setgrid true -yscrollcommand ".s set"
	scrollbar .s -relief flat -command ".t yview"
	pack append . .s {right filly} .t {expand fill}

	# Use tags to build a cursor for the text window.
	set bg [lindex [.t config -background] 4]
	set fg [lindex [.t config -foreground] 4]
	.t tag configure tk_cursor -background $fg -foreground $bg
	.t mark set tk_cursor_indx insert
	.t tag add tk_cursor tk_cursor_indx

	# Bind the keys.
	bind .t <Any-KeyPress>	{tk_flash; break}
	bind .t 0		{tk_key_enter "0"; break}
	bind .t 1		{tk_key_enter "1"; break}
	bind .t 2		{tk_key_enter "2"; break}
	bind .t 3		{tk_key_enter "3"; break}
	bind .t 4		{tk_key_enter "4"; break}
	bind .t 5		{tk_key_enter "5"; break}
	bind .t 6		{tk_key_enter "6"; break}
	bind .t 7		{tk_key_enter "7"; break}
	bind .t 8		{tk_key_enter "8"; break}
	bind .t 9		{tk_key_enter "9"; break}
	bind .t <BackSpace>	{tk_key_enter "\010"; break}
	bind .t <Control-a>	{tk_key_enter "\001"; break}
	bind .t <Control-b>	{tk_key_enter "\002"; break}
	bind .t <Control-c>	{tk_key_enter "\003"; break}
	bind .t <Control-d>	{tk_key_enter "\004"; break}
	bind .t <Control-e>	{tk_key_enter "\005"; break}
	bind .t <Control-f>	{tk_key_enter "\006"; break}
	bind .t <Control-g>	{tk_key_enter "\007"; break}
	bind .t <Control-h>	{tk_key_enter "\010"; break}
	bind .t <Control-i>	{tk_key_enter "\011"; break}
	bind .t <Control-j>	{tk_key_enter "\012"; break}
	bind .t <Control-k>	{tk_key_enter "\013"; break}
	bind .t <Control-l>	{tk_key_enter "\014"; break}
	bind .t <Control-m>	{tk_key_enter "\015"; break}
	bind .t <Control-n>	{tk_key_enter "\016"; break}
	bind .t <Control-o>	{tk_key_enter "\017"; break}
	bind .t <Control-p>	{tk_key_enter "\020"; break}
	bind .t <Control-q>	{tk_key_enter "\021"; break}
	bind .t <Control-r>	{tk_key_enter "\022"; break}
	bind .t <Control-s>	{tk_key_enter "\023"; break}
	bind .t <Control-t>	{tk_key_enter "\024"; break}
	bind .t <Control-u>	{tk_key_enter "\025"; break}
	bind .t <Control-v>	{tk_key_enter "\026"; break}
	bind .t <Control-w>	{tk_key_enter "\027"; break}
	bind .t <Control-x>	{tk_key_enter "\030"; break}
	bind .t <Control-y>	{tk_key_enter "\031"; break}
	bind .t <Control-z>	{tk_key_enter "\032"; break}
	bind .t <Control_L>	{tk_noop; break}
	bind .t <Control_R>	{tk_noop; break}
	bind .t <Delete>	{tk_key_enter "x"; break}
	bind .t <Down>		{tk_key_enter "j"; break}
	bind .t <End>		{tk_key_enter "G"; break}
	bind .t <Escape>	{tk_key_enter "\033"; break}
	bind .t <Home>		{tk_key_enter "1G"; break}
	bind .t <Insert>	{tk_key_enter "i"; break}
	bind .t <Left>		{tk_key_enter "h"; break}
	bind .t <Next>		{tk_key_enter "\006"; break}
	bind .t <Prior>		{tk_key_enter "\002"; break}
	bind .t <Return>	{tk_key_enter "\015"; break}
	bind .t <Right>		{tk_key_enter "l"; break}
	bind .t <Shift_L>	{tk_noop; break}
	bind .t <Shift_Lock>	{tk_noop; break}
	bind .t <Shift_R>	{tk_noop; break}
	bind .t <Tab>		{tk_key_enter "\011"; break}
	bind .t <Up>		{tk_key_enter "k"; break}
	bind .t <ampersand>	{tk_key_enter "&"; break}
	bind .t <asciicircum>	{tk_key_enter "^"; break}
	bind .t <asciitilde>	{tk_key_enter "~"; break}
	bind .t <asterisk>	{tk_key_enter "*"; break}
	bind .t <at>		{tk_key_enter "@"; break}
	bind .t <backslash>	{tk_key_enter "\\"; break}
	bind .t <bar>		{tk_key_enter "|"; break}
	bind .t <braceleft>	{tk_key_enter "{"; break}
	bind .t <braceright>	{tk_key_enter "; break}"}
	bind .t <bracketleft>	{tk_key_enter "\["; break}
	bind .t <bracketright>	{tk_key_enter "]"; break}
	bind .t <colon>		{tk_key_enter ":"; break}
	bind .t <comma>		{tk_key_enter ","; break}
	bind .t <dollar>	{tk_key_enter "$"; break}
	bind .t <equal>		{tk_key_enter "="; break}
	bind .t <exclam>	{tk_key_enter "!"; break}
	bind .t <greater>	{tk_key_enter ">"; break}
	bind .t <less>		{tk_key_enter "<"; break}
	bind .t <minus>		{tk_key_enter "-"; break}
	bind .t <numbersign>	{tk_key_enter "#"; break}
	bind .t <parenleft>	{tk_key_enter "("; break}
	bind .t <parenright>	{tk_key_enter ")"; break}
	bind .t <percent>	{tk_key_enter "%"; break}
	bind .t <period>	{tk_key_enter "."; break}
	bind .t <plus>		{tk_key_enter "+"; break}
	bind .t <question>	{tk_key_enter "?"; break}
	bind .t <quotedbl>	{tk_key_enter "\""; break}
	bind .t <quoteright>	{tk_key_enter "'"; break}
	bind .t <semicolon>	{tk_key_enter ";"; break}
	bind .t <slash>		{tk_key_enter "/"; break}
	bind .t <space>		{tk_key_enter " "; break}
	bind .t <underscore>	{tk_key_enter "_"; break}
	bind .t A		{tk_key_enter "A"; break}
	bind .t B		{tk_key_enter "B"; break}
	bind .t C		{tk_key_enter "C"; break}
	bind .t D		{tk_key_enter "D"; break}
	bind .t E		{tk_key_enter "E"; break}
	bind .t F		{tk_key_enter "F"; break}
	bind .t G		{tk_key_enter "G"; break}
	bind .t H		{tk_key_enter "H"; break}
	bind .t I		{tk_key_enter "I"; break}
	bind .t J		{tk_key_enter "J"; break}
	bind .t K		{tk_key_enter "K"; break}
	bind .t L		{tk_key_enter "L"; break}
	bind .t M		{tk_key_enter "M"; break}
	bind .t N		{tk_key_enter "N"; break}
	bind .t O		{tk_key_enter "O"; break}
	bind .t P		{tk_key_enter "P"; break}
	bind .t Q		{tk_key_enter "Q"; break}
	bind .t R		{tk_key_enter "R"; break}
	bind .t S		{tk_key_enter "S"; break}
	bind .t T		{tk_key_enter "T"; break}
	bind .t U		{tk_key_enter "U"; break}
	bind .t V		{tk_key_enter "V"; break}
	bind .t W		{tk_key_enter "W"; break}
	bind .t X		{tk_key_enter "X"; break}
	bind .t Y		{tk_key_enter "Y"; break}
	bind .t Z		{tk_key_enter "Z"; break}
	bind .t a		{tk_key_enter "a"; break}
	bind .t b		{tk_key_enter "b"; break}
	bind .t c		{tk_key_enter "c"; break}
	bind .t d		{tk_key_enter "d"; break}
	bind .t e		{tk_key_enter "e"; break}
	bind .t f		{tk_key_enter "f"; break}
	bind .t g		{tk_key_enter "g"; break}
	bind .t h		{tk_key_enter "h"; break}
	bind .t i		{tk_key_enter "i"; break}
	bind .t j		{tk_key_enter "j"; break}
	bind .t k		{tk_key_enter "k"; break}
	bind .t l		{tk_key_enter "l"; break}
	bind .t m		{tk_key_enter "m"; break}
	bind .t n		{tk_key_enter "n"; break}
	bind .t o		{tk_key_enter "o"; break}
	bind .t p		{tk_key_enter "p"; break}
	bind .t q		{tk_key_enter "q"; break}
	bind .t r		{tk_key_enter "r"; break}
	bind .t s		{tk_key_enter "s"; break}
	bind .t t		{tk_key_enter "t"; break}
	bind .t u		{tk_key_enter "u"; break}
	bind .t v		{tk_key_enter "v"; break}
	bind .t w		{tk_key_enter "w"; break}
	bind .t x		{tk_key_enter "x"; break}
	bind .t y		{tk_key_enter "y"; break}
	bind .t z		{tk_key_enter "z"; break}

	# XXX
	# I haven't been able to make Tcl/Tk write uninitialized portions
	# of the text window.  Fill in the screen.
	tk_ssize
	.t mark set insert 1.0
	for {set i 1} {$i <= $tk_ssize_row} {incr i} {
		for {set j 1} {$j <= $tk_ssize_col} {incr j} {
			.t insert insert " "
		}
		.t insert insert "\n"
	}
}

# tk_noop --
#	Do nothing.
#
# XXX
# I can't figure out how to get a binding that does nothing without
# calling a function, so this stub does it for me.
proc tk_noop {} {
}

# tk_key_enter --
#	Enter a key.
proc tk_key_enter {val} {
	global newkey
	global waiting

	set waiting 0
	tk_key $val
	set newkey 1
}

# tk_key_wait --
#	Wait for a key.
proc tk_key_wait {timeout} {
	global newkey
	global waiting

	if { $timeout != 0 } {
		after $timeout "set newkey 1"
	}
	set waiting 1
	tkwait variable newkey
}

# Callback functions for the File menu.
# tk_edit
#	Edit another file.
proc tk_edit {} {
}

# tk_quit
#	Quit.
proc tk_quit {} {
	global newkey
	global waiting

	tk_op quit
	if { $waiting != 0 } {
		set newkey 1
	}
}

# tk_write
#	Write the edit buffer.
proc tk_write {} {
	global newkey
	global waiting

	tk_op write
	if { $waiting != 0 } {
		set newkey 1
	}
}

# tk_writeas
#	Write the edit buffer to a named file.
proc tk_writeas {} {
}

# tk_writequit
#	Write and quit.
proc tk_writequit {} {
	global newkey
	global waiting

	tk_op writequit
	if { $waiting != 0 } {
		set newkey 1
	}
}

# Callback functions for the Help menu.
#
# tk_help --
#	Present a help screen.
proc tk_help {} {
	tk_dialog .d {} "No help screen currently available." {} 0 Continue
}

# tk_options
# Contains the option selector box. It is divided into three parts, the
# checkbuttons for the boolean options, the entry fields for the string
# numeric options, and a control area containing buttons.  There is only
# one function.
proc tk_options {} {

	# Build option selector box with three subframes for boolean,
	# numeric, and string options.  Make it a toplevel window.
	toplevel .os
	wm title .os options

	# Option variables.
	global tko_altwerase
	global tko_autoindent
	global tko_autoprint
	global tko_autowrite
	global tko_backup
	global tko_beautify
	global tko_cdpath
	global tko_cedit
	global tko_columns
	global tko_comment
	global tko_directory
	global tko_edcompatible
	global tko_escapetime
	global tko_errorbells
	global tko_exrc
	global tko_extended
	global tko_filec
	global tko_flash
	global tko_hardtabs
	global tko_iclower
	global tko_ignorecase
	global tko_keytime
	global tko_leftright
	global tko_lines
	global tko_lisp
	global tko_list
	global tko_lock
	global tko_magic
	global tko_matchtime
	global tko_mesg
	global tko_modeline
	global tko_msgcat
	global tko_noprint
	global tko_number
	global tko_octal
	global tko_open
	global tko_optimize
	global tko_paragraphs
	global tko_print
	global tko_prompt
	global tko_readonly
	global tko_recdir
	global tko_redraw
	global tko_remap
	global tko_report
	global tko_ruler
	global tko_scroll
	global tko_searchincr
	global tko_sections
	global tko_secure
	global tko_shell
	global tko_shellmeta
	global tko_shiftwidth
	global tko_showmatch
	global tko_showmode
	global tko_sidescroll
	global tko_slowopen
	global tko_sourceany
	global tko_tabstop
	global tko_taglength
	global tko_tags
	global tko_term
	global tko_terse
	global tko_tildeop
	global tko_timeout
	global tko_ttywerase
	global tko_verbose
	global tko_warn
	global tko_window
	global tko_windowname
	global tko_wraplen
	global tko_wrapmargin
	global tko_wrapscan
	global tko_writeany

	# Initialize option values.
	tk_opt_init

	# Build subframe for boolean options.
	frame .os.bopts

	# This is the width of the edcompatible button.
	set buttonwidth 13

	# Pack the boolean os, 5 to a frame.
	frame .os.bopts.f1
	pack append .os.bopts .os.bopts.f1 {top}
	checkbutton .os.bopts.f1.b1 \
	    -variable tko_altwerase -text "altwerase" \
	    -command "tk_opt_set altwerase $tko_altwerase" \
	    -width $buttonwidth -anchor w
	checkbutton .os.bopts.f1.b2 \
	    -variable tko_autoindent -text "autoindent" \
	    -command "tk_opt_set autoindent $tko_autoindent" \
	    -width $buttonwidth -anchor w
	checkbutton .os.bopts.f1.b3 \
	    -variable tko_autoprint -text "autoprint" \
	    -command "tk_opt_set autoprint $tko_autoprint" \
	    -width $buttonwidth -anchor w
	checkbutton .os.bopts.f1.b4 \
	    -variable tko_autowrite -text "autowrite" \
	    -command "tk_opt_set autowrite $tko_autowrite" \
	    -width $buttonwidth -anchor w
	checkbutton .os.bopts.f1.b5 \
	    -variable tko_beautify -text "beautify" \
	    -command "tk_opt_set beautify $tko_beautify" \
	    -width $buttonwidth -anchor w
	pack append .os.bopts.f1 \
	    .os.bopts.f1.b1 {left frame w} \
	    .os.bopts.f1.b2 {left frame w} \
	    .os.bopts.f1.b3 {left frame w} \
	    .os.bopts.f1.b4 {left frame w} \
	    .os.bopts.f1.b5 {left frame w}

	frame .os.bopts.f2
	pack append .os.bopts .os.bopts.f2 {top}
	checkbutton .os.bopts.f2.b1 \
	    -variable tko_comment -text "comment" \
	    -command "tk_opt_set comment $tko_comment" \
	    -width $buttonwidth -anchor w
	checkbutton .os.bopts.f2.b2 \
	    -variable tko_edcompatible -text "edcompatible" \
	    -command "tk_opt_set edcompatible $tko_edcompatible" \
	    -width $buttonwidth -anchor w
	checkbutton .os.bopts.f2.b3 \
	    -variable tko_errorbells -text "errorbells" \
	    -command "tk_opt_set errorbells $tko_errorbells" \
	    -width $buttonwidth -anchor w
	checkbutton .os.bopts.f2.b4 \
	    -variable tko_exrc -text "exrc" \
	    -command "tk_opt_set exrc $tko_exrc" \
	    -width $buttonwidth -anchor w
	checkbutton .os.bopts.f2.b5 \
	    -variable tko_extended -text "extended" \
	    -command "tk_opt_set extended $tko_extended" \
	    -width $buttonwidth -anchor w
	pack append .os.bopts.f2 \
	    .os.bopts.f2.b1 {left frame w} \
	    .os.bopts.f2.b2 {left frame w} \
	    .os.bopts.f2.b3 {left frame w} \
	    .os.bopts.f2.b4 {left frame w} \
	    .os.bopts.f2.b5 {left frame w}

	frame .os.bopts.f3
	pack append .os.bopts .os.bopts.f3 {top}
	checkbutton .os.bopts.f3.b1 \
	    -variable tko_flash -text "flash" \
	    -command "tk_opt_set flash $tko_flash" \
	    -width $buttonwidth -anchor w
	checkbutton .os.bopts.f3.b2 \
	    -variable tko_iclower -text "iclower" \
	    -command "tk_opt_set iclower $tko_iclower" \
	    -width $buttonwidth -anchor w
	checkbutton .os.bopts.f3.b3 \
	    -variable tko_ignorecase -text "ignorecase" \
	    -command "tk_opt_set ignorecase $tko_ignorecase" \
	    -width $buttonwidth -anchor w
	checkbutton .os.bopts.f3.b4 \
	    -variable tko_leftright -text "leftright" \
	    -command "tk_opt_set leftright $tko_leftright" \
	    -width $buttonwidth -anchor w
	checkbutton .os.bopts.f3.b5 \
	    -variable tko_lisp -text "lisp" \
	    -command "tk_opt_set lisp $tko_lisp" \
	    -width $buttonwidth -anchor w
	pack append .os.bopts.f3 \
	    .os.bopts.f3.b1 {left frame w} \
	    .os.bopts.f3.b2 {left frame w} \
	    .os.bopts.f3.b3 {left frame w} \
	    .os.bopts.f3.b4 {left frame w} \
	    .os.bopts.f3.b5 {left frame w}

	frame .os.bopts.f4
	pack append .os.bopts .os.bopts.f4 {top}
	checkbutton .os.bopts.f4.b1 \
	    -variable tko_list -text "list" \
	    -command "tk_opt_set list $tko_list" \
	    -width $buttonwidth -anchor w
	checkbutton .os.bopts.f4.b2 \
	    -variable tko_lock -text "lock" \
	    -command "tk_opt_set lock $tko_lock" \
	    -width $buttonwidth -anchor w
	checkbutton .os.bopts.f4.b3 \
	    -variable tko_magic -text "magic" \
	    -command "tk_opt_set magic $tko_magic" \
	    -width $buttonwidth -anchor w
	checkbutton .os.bopts.f4.b4 \
	    -variable tko_mesg -text "mesg" \
	    -command "tk_opt_set mesg $tko_mesg" \
	    -width $buttonwidth -anchor w
	checkbutton .os.bopts.f4.b5\
	    -variable tko_number -text "number" \
	    -command "tk_opt_set number $tko_number" \
	    -width $buttonwidth -anchor w
	pack append .os.bopts.f4 \
	    .os.bopts.f4.b1 {left frame w} \
	    .os.bopts.f4.b2 {left frame w} \
	    .os.bopts.f4.b3 {left frame w} \
	    .os.bopts.f4.b4 {left frame w} \
	    .os.bopts.f4.b5 {left frame w}

	frame .os.bopts.f5
	pack append .os.bopts .os.bopts.f5 {top}
	checkbutton .os.bopts.f5.b1 \
	    -variable tko_octal -text "octal" \
	    -command "tk_opt_set octal $tko_octal" \
	    -width $buttonwidth -anchor w
	checkbutton .os.bopts.f5.b2 \
	    -variable tko_open -text "open" \
	    -command "tk_opt_set open $tko_open" \
	    -width $buttonwidth -anchor w
	checkbutton .os.bopts.f5.b3 \
	    -variable tko_optimize -text "optimize" \
	    -command "tk_opt_set optimize $tko_optimize" \
	    -width $buttonwidth -anchor w
	checkbutton .os.bopts.f5.b4 \
	    -variable tko_prompt -text "prompt" \
	    -command "tk_opt_set prompt $tko_prompt" \
	    -width $buttonwidth -anchor w
	checkbutton .os.bopts.f5.b5 \
	    -variable tko_readonly -text "readonly" \
	    -command "tk_opt_set readonly $tko_readonly" \
	    -width $buttonwidth -anchor w
	pack append .os.bopts.f5 \
	    .os.bopts.f5.b1 {left frame w} \
	    .os.bopts.f5.b2 {left frame w} \
	    .os.bopts.f5.b3 {left frame w} \
	    .os.bopts.f5.b4 {left frame w} \
	    .os.bopts.f5.b5 {left frame w}

	frame .os.bopts.f6
	pack append .os.bopts .os.bopts.f6 {top}
	checkbutton .os.bopts.f6.b1 \
	    -variable tko_remap -text "remap" \
	    -command "tk_opt_set remap $tko_remap" \
	    -width $buttonwidth -anchor w
	checkbutton .os.bopts.f6.b2 \
	    -variable tko_ruler -text "ruler" \
	    -command "tk_opt_set ruler $tko_ruler" \
	    -width $buttonwidth -anchor w
	checkbutton .os.bopts.f6.b3 \
	    -variable tko_searchincr -text "searchincr" \
	    -command "tk_opt_set searchincr $tko_searchincr" \
	    -width $buttonwidth -anchor w
	checkbutton .os.bopts.f6.b4 \
	    -variable tko_secure -text "secure" \
	    -command "tk_opt_set secure $tko_secure" \
	    -width $buttonwidth -anchor w
	checkbutton .os.bopts.f6.b5 \
	    -variable tko_showmatch -text "showmatch" \
	    -command "tk_opt_set showmatch $tko_showmatch" \
	    -width $buttonwidth -anchor w
	pack append .os.bopts.f6 \
	    .os.bopts.f6.b1 {left frame w} \
	    .os.bopts.f6.b2 {left frame w} \
	    .os.bopts.f6.b3 {left frame w} \
	    .os.bopts.f6.b4 {left frame w} \
	    .os.bopts.f6.b5 {left frame w}

	frame .os.bopts.f7
	pack append .os.bopts .os.bopts.f7 {top}
	checkbutton .os.bopts.f7.b1 \
	    -variable tko_showmode -text "showmode" \
	    -command "tk_opt_set showmode $tko_showmode" \
	    -width $buttonwidth -anchor w
	checkbutton .os.bopts.f7.b2 \
	    -variable tko_slowopen -text "slowopen" \
	    -command "tk_opt_set slowopen $tko_slowopen" \
	    -width $buttonwidth -anchor w
	checkbutton .os.bopts.f7.b3 \
	    -variable tko_sourceany -text "sourceany" \
	    -command "tk_opt_set sourceany $tko_sourceany" \
	    -width $buttonwidth -anchor w
	checkbutton .os.bopts.f7.b4 \
	    -variable tko_terse -text "terse" \
	    -command "tk_opt_set terse $tko_terse" \
	    -width $buttonwidth -anchor w
	checkbutton .os.bopts.f7.b5 \
	    -variable tko_tildeop -text "tildeop" \
	    -command "tk_opt_set tildeope $tko_tildeop" \
	    -width $buttonwidth -anchor w
	pack append .os.bopts.f7 \
	    .os.bopts.f7.b1 {left frame w} \
	    .os.bopts.f7.b2 {left frame w} \
	    .os.bopts.f7.b3 {left frame w} \
	    .os.bopts.f7.b4 {left frame w} \
	    .os.bopts.f7.b5 {left frame w}

	frame .os.bopts.f8
	pack append .os.bopts .os.bopts.f8 {top fillx}
	checkbutton .os.bopts.f8.b1 \
	    -variable tko_timeout -text "timeout" \
	    -command "tk_opt_set timeout $tko_timeout" \
	    -width $buttonwidth -anchor w
	checkbutton .os.bopts.f8.b2 \
	    -variable tko_ttywerase -text "ttywerase" \
	    -command "tk_opt_set ttywerase $tko_ttywerase" \
	    -width $buttonwidth -anchor w
	checkbutton .os.bopts.f8.b3 \
	    -variable tko_verbose -text "verbose" \
	    -command "tk_opt_set verbose $tko_verbose" \
	    -width $buttonwidth -anchor w
	checkbutton .os.bopts.f8.b4 \
	    -variable tko_warn -text "warn" \
	    -command "tk_opt_set warn $tko_warn" \
	    -width $buttonwidth -anchor w
	checkbutton .os.bopts.f8.b5 \
	    -variable tko_windowname -text "windowname" \
	    -command "tk_opt_set windowname $tko_windowname" \
	    -width $buttonwidth -anchor w
	pack append .os.bopts.f8 \
	    .os.bopts.f8.b1 {left frame w} \
	    .os.bopts.f8.b2 {left frame w} \
	    .os.bopts.f8.b3 {left frame w} \
	    .os.bopts.f8.b4 {left frame w} \
	    .os.bopts.f8.b5 {left frame w}

	frame .os.bopts.f9
	pack append .os.bopts .os.bopts.f9 {top fillx}
	checkbutton .os.bopts.f9.b1 \
	    -variable tko_wrapscan -text "wrapscan" \
	    -command "tk_opt_set wrapscan $tko_wrapscan" \
	    -width $buttonwidth -anchor w
	checkbutton .os.bopts.f9.b2 \
	    -variable tko_writeany -text "writeany" \
	    -command "tk_opt_set writeany $tko_writeany" \
	    -width $buttonwidth -anchor w
	pack append .os.bopts.f9 \
	    .os.bopts.f9.b1 {left frame w} \
	    .os.bopts.f9.b2 {left frame w}

	# Build frame for number options:
	frame .os.nopts

	# Label and entry widths.
	set lwidth 12
	set ewidth  3

	frame .os.nopts.n1
	label .os.nopts.n1.l -text "column:" -width $lwidth -anchor w
	entry .os.nopts.n1.e -width $ewidth -relief raised \
	    -textvariable tko_columns
	trace variable tko_columns w tk_opt_ew
	pack append .os.nopts.n1 \
	    .os.nopts.n1.l {left} .os.nopts.n1.e {left frame w}

	frame .os.nopts.n2
	label .os.nopts.n2.l -text "escapetime:" -width $lwidth -anchor w
	entry .os.nopts.n2.e -width $ewidth -textvariable tko_escapetime \
	    -relief raised
	trace variable tko_escapetime w tk_opt_ew
	pack append .os.nopts.n2 \
	    .os.nopts.n2.l {left} .os.nopts.n2.e {left frame w}

	frame .os.nopts.n3
	label .os.nopts.n3.l -text "hardtabs:" -width $lwidth -anchor w
	entry .os.nopts.n3.e -width $ewidth -textvariable tko_hardtabs \
	    -relief raised
	trace variable tko_hardtabs w tk_opt_ew
	pack append .os.nopts.n3 \
	    .os.nopts.n3.l {left} .os.nopts.n3.e {left frame w}

	frame .os.nopts.n4
	label .os.nopts.n4.l -text "keytime:" -width $lwidth -anchor w
	entry .os.nopts.n4.e -width $ewidth -textvariable tko_keytime \
	    -relief raised
	trace variable tko_keytime w tk_opt_ew
	pack append .os.nopts.n4 \
	    .os.nopts.n4.l {left} .os.nopts.n4.e {left frame w}

	frame .os.nopts.n5
	label .os.nopts.n5.l -text "lines:" -width $lwidth -anchor w
	entry .os.nopts.n5.e -width $ewidth -textvariable tko_lines \
	    -relief raised
	trace variable tko_lines w tk_opt_ew
	pack append .os.nopts.n5 \
	    .os.nopts.n5.l {left} .os.nopts.n5.e {left frame w}

	frame .os.nopts.n6
	label .os.nopts.n6.l -text "matchtime:" -width $lwidth -anchor w
	entry .os.nopts.n6.e -width $ewidth -textvariable tko_matchtime \
	    -relief raised
	trace variable tko_matchtime w tk_opt_ew
	pack append .os.nopts.n6 \
	    .os.nopts.n6.l {left} .os.nopts.n6.e {left frame w}

	frame .os.nopts.n7
	label .os.nopts.n7.l -text "report:" -width $lwidth -anchor w
	entry .os.nopts.n7.e -width $ewidth -textvariable tko_report \
	    -relief raised
	trace variable tko_report w tk_opt_ew
	pack append .os.nopts.n7 \
	    .os.nopts.n7.l {left} .os.nopts.n7.e {left frame w}

	frame .os.nopts.n8
	label .os.nopts.n8.l -text "scroll:" -width $lwidth -anchor w
	entry .os.nopts.n8.e -width $ewidth -textvariable tko_scroll \
	    -relief raised
	trace variable tko_scroll w tk_opt_ew
	pack append .os.nopts.n8 \
	    .os.nopts.n8.l {left} .os.nopts.n8.e {left frame w}

	frame .os.nopts.n9
	label .os.nopts.n9.l -text "shiftwidth:" -width $lwidth -anchor w
	entry .os.nopts.n9.e -width $ewidth -textvariable tko_shiftwidth \
	    -relief raised
	trace variable tko_shiftwidth w tk_opt_ew
	pack append .os.nopts.n9 \
	    .os.nopts.n9.l {left} .os.nopts.n9.e {left frame w}

	frame .os.nopts.n10
	label .os.nopts.n10.l -text "sidescroll:" -width $lwidth -anchor w
	entry .os.nopts.n10.e -width $ewidth -textvariable tko_sidescroll \
	    -relief raised
	trace variable tko_sidescroll w tk_opt_ew
	pack append .os.nopts.n10 \
	    .os.nopts.n10.l {left} .os.nopts.n10.e {left frame w}

	frame .os.nopts.n11
	label .os.nopts.n11.l -text "tabstop:" -width $lwidth -anchor w
	entry .os.nopts.n11.e -width $ewidth -textvariable tko_tabstop \
	    -relief raised
	trace variable tko_tabstop w tk_opt_ew
	pack append .os.nopts.n11 \
	    .os.nopts.n11.l {left} .os.nopts.n11.e {left frame w}

	frame .os.nopts.n12
	label .os.nopts.n12.l -text "taglength:" -width $lwidth -anchor w
	entry .os.nopts.n12.e -width $ewidth -textvariable tko_taglength \
	    -relief raised
	trace variable tko_taglength w tk_opt_ew
	pack append .os.nopts.n12 \
	    .os.nopts.n12.l {left} .os.nopts.n12.e {left frame w}

	frame .os.nopts.n13
	label .os.nopts.n13.l -text "window:" -width $lwidth -anchor w
	entry .os.nopts.n13.e -width $ewidth -textvariable tko_window \
	    -relief raised
	trace variable tko_window w tk_opt_ew
	pack append .os.nopts.n13 \
	    .os.nopts.n13.l {left} .os.nopts.n13.e {left frame w}

	frame .os.nopts.n14
	label .os.nopts.n14.l -text "wraplen:" -width $lwidth -anchor w
	entry .os.nopts.n14.e -width $ewidth -textvariable tko_wraplen \
	    -relief raised
	trace variable tko_wraplen w tk_opt_ew
	pack append .os.nopts.n14 \
	    .os.nopts.n14.l {left} .os.nopts.n14.e {left frame w}

	frame .os.nopts.n15
	label .os.nopts.n15.l -text "wrapmargin:" -width $lwidth -anchor w
	entry .os.nopts.n15.e -width $ewidth -textvariable tko_wrapmargin \
	    -relief raised
	trace variable tko_wrapmargin w tk_opt_ew
	pack append .os.nopts.n15 \
	    .os.nopts.n15.l {left} .os.nopts.n15.e {left frame w}

	pack append .os.nopts \
	    .os.nopts.n1 {top fillx} \
	    .os.nopts.n3 {top expand fillx} \
	    .os.nopts.n4 {top expand fillx} \
	    .os.nopts.n5 {top expand fillx} \
	    .os.nopts.n6 {top expand fillx} \
	    .os.nopts.n7 {top expand fillx} \
	    .os.nopts.n8 {top expand fillx} \
	    .os.nopts.n9 {top expand fillx} \
	    .os.nopts.n10 {top expand fillx} \
	    .os.nopts.n11 {top expand fillx} \
	    .os.nopts.n12 {top expand fillx} \
	    .os.nopts.n13 {top expand fillx} \
	    .os.nopts.n14 {top expand fillx} \
	    .os.nopts.n15 {top expand fillx}

	# Build frame for string options
	frame .os.sopts

	# Entry width.
	set ewidth  40

	frame .os.sopts.s1
	label .os.sopts.s1.l -text "backup:" -width $lwidth -anchor w
	entry .os.sopts.s1.e -width $ewidth -textvariable tko_backup \
	    -relief raised
	pack append .os.sopts.s1 \
	    .os.sopts.s1.l {left} .os.sopts.s1.e {left frame w}

	frame .os.sopts.s2
	label .os.sopts.s2.l -text "cdpath:" -width $lwidth -anchor w
	entry .os.sopts.s2.e -width $ewidth -textvariable tko_cdpath \
	    -relief raised
	pack append .os.sopts.s2 \
	    .os.sopts.s2.l {left} .os.sopts.s2.e {left frame w}

	frame .os.sopts.s3
	label .os.sopts.s3.l -text "directory:" -width $lwidth -anchor w
	entry .os.sopts.s3.e -width $ewidth -textvariable tko_directory \
	    -relief raised
	pack append .os.sopts.s3 \
	    .os.sopts.s3.l {left} .os.sopts.s3.e {left frame w}

	frame .os.sopts.s4
	label .os.sopts.s4.l -text "cedit:" -width $lwidth -anchor w
	entry .os.sopts.s4.e -width $ewidth -textvariable tko_cedit \
	    -relief raised
	pack append .os.sopts.s4 \
	    .os.sopts.s4.l {left} .os.sopts.s4.e {left frame w}

	frame .os.sopts.s5
	label .os.sopts.s5.l -text "filec:" -width $lwidth -anchor w
	entry .os.sopts.s5.e -width $ewidth -textvariable tko_filec \
	    -relief raised
	pack append .os.sopts.s5 \
	    .os.sopts.s5.l {left} .os.sopts.s5.e {left frame w}

	frame .os.sopts.s6
	label .os.sopts.s6.l -text "msgcat:" -width $lwidth -anchor w
	entry .os.sopts.s6.e -width $ewidth -textvariable tko_msgcat \
	    -relief raised
	pack append .os.sopts.s6 \
	    .os.sopts.s6.l {left} .os.sopts.s6.e {left frame w}

	frame .os.sopts.s7
	label .os.sopts.s7.l -text "noprint:" -width $lwidth -anchor w
	entry .os.sopts.s7.e -width $ewidth -textvariable tko_noprint \
	    -relief raised
	pack append .os.sopts.s7 \
	    .os.sopts.s7.l {left} .os.sopts.s7.e {left frame w}

	frame .os.sopts.s8
	label .os.sopts.s8.l -text "paragraphs:" -width $lwidth -anchor w
	entry .os.sopts.s8.e -width $ewidth -textvariable tko_paragraphs \
	    -relief raised
	pack append .os.sopts.s8 \
	    .os.sopts.s8.l {left} .os.sopts.s8.e {left frame w}

	frame .os.sopts.s9
	label .os.sopts.s9.l -text "print:" -width $lwidth -anchor w
	entry .os.sopts.s9.e -width $ewidth -textvariable tko_print \
	    -relief raised
	pack append .os.sopts.s9 \
	    .os.sopts.s9.l {left} .os.sopts.s9.e {left frame w}

	frame .os.sopts.s10
	label .os.sopts.s10.l -text "recdir:" -width $lwidth -anchor w
	entry .os.sopts.s10.e -width $ewidth -textvariable tko_recdir \
	    -relief raised
	pack append .os.sopts.s10 \
	    .os.sopts.s10.l {left} .os.sopts.s10.e {left frame w}

	frame .os.sopts.s11
	label .os.sopts.s11.l -text "sections:" -width $lwidth -anchor w
	entry .os.sopts.s11.e -width $ewidth -textvariable tko_sections \
	    -relief raised
	pack append .os.sopts.s11 \
	    .os.sopts.s11.l {left} .os.sopts.s11.e {left frame w}

	frame .os.sopts.s12
	label .os.sopts.s12.l -text "shell:" -width $lwidth -anchor w
	entry .os.sopts.s12.e -width $ewidth -textvariable tko_shell \
	    -relief raised
	pack append .os.sopts.s12 \
	    .os.sopts.s12.l {left} .os.sopts.s12.e {left frame w}

	frame .os.sopts.s13
	label .os.sopts.s13.l -text "shellmeta:" -width $lwidth -anchor w
	entry .os.sopts.s13.e -width $ewidth -textvariable tko_shellmeta \
	    -relief raised
	pack append .os.sopts.s13 \
	    .os.sopts.s13.l {left} .os.sopts.s13.e {left frame w}

	frame .os.sopts.s14
	label .os.sopts.s14.l -text "tags:" -width $lwidth -anchor w
	entry .os.sopts.s14.e -width $ewidth -textvariable tko_tags \
	    -relief raised
	pack append .os.sopts.s14 \
	    .os.sopts.s14.l {left} .os.sopts.s14.e {left frame w}

	frame .os.sopts.s15
	label .os.sopts.s15.l -text "term:" -width $lwidth -anchor w
	entry .os.sopts.s15.e -width $ewidth -textvariable tko_term \
	    -relief raised
	pack append .os.sopts.s15 \
	    .os.sopts.s15.l {left} .os.sopts.s15.e {left frame w}

	pack append .os.sopts \
	    .os.sopts.s1 {top expand fillx} \
	    .os.sopts.s2 {top expand fillx} \
	    .os.sopts.s3 {top expand fillx} \
	    .os.sopts.s4 {top expand fillx} \
	    .os.sopts.s5 {top expand fillx} \
	    .os.sopts.s6 {top expand fillx} \
	    .os.sopts.s7 {top expand fillx} \
	    .os.sopts.s8 {top expand fillx} \
	    .os.sopts.s9 {top expand fillx} \
	    .os.sopts.s10 {top expand fillx} \
	    .os.sopts.s11 {top expand fillx} \
	    .os.sopts.s12 {top expand fillx} \
	    .os.sopts.s13 {top expand fillx} \
	    .os.sopts.s14 {top expand fillx} \
	    .os.sopts.s15 {top expand fillx}

	# Build frame for continue button.
	frame .os.control -bd 4
	button .os.control.quit -text "Continue" -command "destroy .os"
	bind .os <Return> ".os.control.quit flash; destroy .os"
	pack append .os.control .os.control.quit {left}

	# Pack everything together.
	pack append .os \
	    .os.bopts {top} \
	    .os.control {bottom fillx} \
	    .os.nopts {left fillx padx 4m pady 4m} \
	    .os.sopts {left fillx pady 4m}

	grab .os
	focus .os
}

# tk_opt_ew --
#	Handle a change to an option entry widget.
proc tk_opt_ew {name element op} {
	upvar $name x
	tk_opt_set "$name=$x"
}

# tk_err --
#	Display a Tcl/Tk error message.
proc tk_err {msg} {
	tk_dialog .d {} "$msg" {} 0 Continue

	#puts "msg: $msg"
}

# tk_addstr --
#	Add a string to the screen.
proc tk_addstr {len str} {
	global tk_cursor_row
	global tk_cursor_col
	
	# Delete the current characters, then insert the new ones.
	.t mark set insert $tk_cursor_row.$tk_cursor_col
	.t delete insert "insert + $len chars"
	.t insert insert "$str"
	incr tk_cursor_col $len

	#puts "tk_addstr: row $tk_cursor_row col $tk_cursor_col: insert $str"
}

# tk_clrtoeol --
#	Clear to the end of the line.
proc tk_clrtoeol {} {
	global tk_cursor_row
	global tk_cursor_col
	global tk_ssize_col

	# Overwrite to the end of the line with spaces.
	.t mark set insert $tk_cursor_row.$tk_cursor_col
	.t delete insert "insert lineend"
	for {set j $tk_cursor_col} {$j < $tk_ssize_col} {incr j} {
		.t insert insert " "
	}

	#puts "tk_clrtoel: row $tk_cursor_row col $tk_cursor_col"
}

# tk_deleteln --
#	Delete the line.
proc tk_deleteln {} {
	global tk_cursor_row
	global tk_cursor_col
	global tk_ssize_col

	# Delete the line.
	.t mark set insert $tk_cursor_row.$tk_cursor_col
	.t delete insert "insert lineend + 1 chars"

	# Append a new, blank line at the end of the screen.
	.t mark set insert end
	for {set j 1} {$j <= $tk_ssize_col} {incr j} {
		.t insert insert " "
	}
	.t insert insert "\n"

	#puts "tk_deleteln: row $tk_cursor_row"
}

# tk_flash --
#	Flash the screen.
proc tk_flash {} {
	set bg [lindex [.t config -background] 4]
	set fg [lindex [.t config -foreground] 4]
	.t configure -background $fg -foreground $bg
	update idletasks
	.t configure -background $bg -foreground $fg
	update idletasks
}

# tk_insertln --
#	Insert the line.
proc tk_insertln {} {
	global tk_cursor_row
	global tk_cursor_col
	global tk_ssize_row
	global tk_ssize_col

	# Delete the last line on the screen.
	.t mark set insert $tk_ssize_row.0
	.t delete insert "insert lineend + 1 chars"

	# Insert a new, blank line.
	.t mark set insert $tk_cursor_row.$tk_cursor_col
	for {set j 1} {$j <= $tk_ssize_col} {incr j} {
		.t insert insert " "
	}
	.t insert insert "\n"

	#puts "tk_insertln: row $tk_cursor_row"
}

# tk_move --
#	Move the cursor.
proc tk_move {row col} {
	global tk_cursor_row
	global tk_cursor_col

	# Convert to Tcl/Tk coordinates, update the insert cursor.
	set tk_cursor_row [ expr $row + 1 ]
	set tk_cursor_col $col
	.t mark set insert $tk_cursor_row.$tk_cursor_col

	# Update the screen cursor.
	.t tag remove tk_cursor tk_cursor_indx
	.t mark set tk_cursor_indx insert
	.t tag add tk_cursor tk_cursor_indx

	#puts "tk_move: row $tk_cursor_row col $tk_cursor_col"
}

# tk_rename --
#	Rename the screen.
proc tk_rename {name} {
	wm title . "$name"
}

# tk_ssize --
#	Return the window size.
proc tk_ssize {} {
	global tk_ssize_col
	global tk_ssize_row

	set s [ .t configure -width ]
	set tk_ssize_col [ lindex $s [ expr [ llength $s ] -1 ] ]
	set s [ .t configure -height ]
	set tk_ssize_row [ lindex $s [ expr [ llength $s ] -1 ] ]

	#puts "tk_ssize: rows $tk_ssize_row, cols $tk_ssize_col"
}

# tk_standout --
#	Change into standout mode.
proc tk_standout {} {
}

# tk_standend --
#	Change out of standout mode.
proc tk_standend {} {
}

# Cursor
set tk_cursor_row	1
set tk_cursor_col	0

# Screen size
set tk_ssize_row	0
set tk_ssize_col	0

screen
#tkwait window .
