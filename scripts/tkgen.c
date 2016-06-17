/* Generate tk script based upon config.in
 *
 * Version 1.0
 * Eric Youngdale
 * 10/95
 *
 * 1996 01 04
 * Avery Pennarun - Aesthetic improvements.
 *
 * 1996 01 24
 * Avery Pennarun - Bugfixes and more aesthetics.
 *
 * 1996 03 08
 * Avery Pennarun - The int and hex config.in commands work right.
 *                - Choice buttons are more user-friendly.
 *                - Disabling a text entry line greys it out properly.
 *                - dep_tristate now works like in Configure. (not pretty)
 *                - No warnings in gcc -Wall. (Fixed some "interesting" bugs.)
 *                - Faster/prettier "Help" lookups.
 *
 * 1996 03 15
 * Avery Pennarun - Added new sed script from Axel Boldt to make help even
 *                  faster. (Actually awk is downright slow on some machines.)
 *                - Fixed a bug I introduced into Choice dependencies.  Thanks
 *                  to Robert Krawitz for pointing this out.
 *
 * 1996 03 16
 * Avery Pennarun - basic "do_make" support added to let sound config work.
 *
 * 1996 03 25
 *     Axel Boldt - Help now works on "choice" buttons.
 *
 * 1996 04 06
 * Avery Pennarun - Improved sound config stuff. (I think it actually works
 *                  now!)
 *                - Window-resize-limits don't use ugly /usr/lib/tk4.0 hack.
 *                - int/hex work with tk3 again. (The "cget" error.)
 *                - Next/Prev buttons switch between menus.  I can't take
 *                  much credit for this; the code was already there, but
 *                  ifdef'd out for some reason.  It flickers a lot, but
 *                  I suspect there's no "easy" fix for that.
 *                - Labels no longer highlight as you move the mouse over
 *                  them (although you can still press them... oh well.)
 *                - Got rid of the last of the literal color settings, to
 *                  help out people with mono X-Windows systems. 
 *                  (Apparently there still are some out there!)
 *                - Tabstops seem sensible now.
 *
 * 1996 04 14
 * Avery Pennarun - Reduced flicker when creating windows, even with "update
 *                  idletasks" hack.
 *
 * 1997 12 08
 * Michael Chastain - Remove sound driver special cases.
 *
 * 1997 11 15
 * Michael Chastain - For choice buttons, write values for all options,
 *                    not just the single chosen one.  This is compatible
 *                    with 'make config' and 'make oldconfig', and is
 *                    needed so smart-config dependencies work if the
 *                    user switches from one configuration method to
 *                    another.
 *
 * 1998 03 09
 * Axel Boldt - Smaller layout of main menu - it's still too big for 800x600.
 *            - Display help in text window to allow for cut and paste.
 *            - Allow for empty lines in help texts.
 *            - update_define should not set all variables unconditionally to
 *              0: they may have been set to 1 elsewhere. CONFIG_NETLINK is
 *              an example.
 *
 * 1999 01 04
 * Michael Elizabeth Chastain <mec@shout.net>
 * - Call clear_globalflags when writing out update_mainmenu.
 *   This fixes the missing global/vfix lines for ARCH=alpha on 2.2.0-pre4.
 *
 * 8 January 1999, Michael Elizabeth Chastain <mec@shout.net>
 * - Emit menus_per_column
 *
 * 14 January 1999, Michael Elizabeth Chastain <mec@shout.net>
 * - Steam-clean this file.  I tested this by generating kconfig.tk for every
 *   architecture and comparing it character-for-character against the output
 *   of the old tkparse.
 * - Fix flattening of nested menus.  The old code simply assigned items to
 *   the most recent token_mainmenu_option, without paying attention to scope.
 *   For example: "menu-1 bool-a menu-2 bool-b endmenu bool-c bool-d endmenu".
 *   The old code would put bool-a in menu-1, bool-b in menu-2, and bool-c
 *   and bool-d in *menu-2*.  This hosed the nested submenus in
 *   drives/net/Config.in and other places.
 * - Fix menu line wraparound at 128 menus (some fool used a 'char' for
 *   a counter).
 *
 * 23 January 1999, Michael Elizabeth Chastain <mec@shout.net>
 * - Remove bug-compatible code.
 *
 * 07 July 1999, Andrzej M. Krzysztofowicz <ankry@mif.pg.gda.pl>
 * Some bugfixes, including
 * - disabling "m" options when CONFIG_MODULES is set to "n" as well as "y"
 *   option in dep_tristate when dependency is set to "m",
 * - deactivating choices which should not be available,
 * - basic validation for int and hex introduced if the entered one is not 
 *   valid,
 * - updates of all opened menus instead of the active only. I was afraid
 *   that it would slow down updates, but I don't even see any speed difference
 *   on my machine. If it slows you can still work with only a single menu
 *   opened,
 * - fixed error when focussing non-existent window (especially Help windows),
 * Higher level submenus implemented.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "tkparse.h"


/*
 * Total number of menus.
 */
static int tot_menu_num = 0;

/*
 * Pointers to mainmenu_option and endmenu of each menu.
 */
struct kconfig * menu_first [100];
struct kconfig * menu_last  [100];

/*
 * Generate portion of wish script for the beginning of a submenu.
 * The guts get filled in with the various options.
 */
static void start_proc( char * label, int menu_num, int toplevel )
{
    if ( toplevel )
	printf( "menu_option menu%d %d \"%s\"\n", menu_num, menu_num, label );
    printf( "proc menu%d {w title} {\n", menu_num );
    printf( "\tset oldFocus [focus]\n" );
    if ( menu_first[menu_num]->menu_number != 0 )
	printf( "\tcatch {focus .menu%d}\n",
		menu_first[menu_num]->menu_number );
    printf( "\tcatch {destroy $w; unregister_active %d}\n", menu_num );
    printf( "\ttoplevel $w -class Dialog\n" );
    printf( "\twm withdraw $w\n" );
    printf( "\tglobal active_menus\n" );
    printf( "\tset active_menus [lsort -integer [linsert $active_menus end %d]]\n", menu_num );
    printf( "\tmessage $w.m -width 400 -aspect 300 -text \\\n" );
    printf( "\t\t\"%s\"  -relief raised\n", label );
    printf( "\tpack $w.m -pady 10 -side top -padx 10\n" );
    printf( "\twm title $w \"%s\" \n\n", label );

    printf( "\tbind $w <Escape> \"catch {focus $oldFocus}; destroy $w; unregister_active %d; break\"\n", menu_num);

    printf("\tset nextscript ");
    printf("\"catch {focus $oldFocus}; " );
    /* 
     * We are checking which windows should be destroyed and which are 
     * common parents with the next one. Remember that menu_num field
     * in mainmenu_option record reports number of its *parent* menu.
     */
    if ( menu_num < tot_menu_num
    && menu_first[menu_num + 1]->menu_number != menu_num )
    {
	int to_destr;

	printf( "destroy $w; unregister_active %d; ", menu_num );
	to_destr = menu_first[menu_num]->menu_number;
	while ( to_destr > 0 && menu_first[menu_num + 1]->menu_number != to_destr )
	{
	    printf( "catch {destroy .menu%d}; unregister_active %d; ",
		to_destr, to_destr );
	    to_destr = menu_first[to_destr]->menu_number;
	}
    }
    printf( "menu%d .menu%d \\\"$title\\\"\"\n",
	menu_num+1, menu_num+1 );

    /*
     * Attach the "Prev", "Next" and "OK" buttons at the end of the window.
     */
    printf( "\tframe $w.f\n" );
    if ( toplevel )
	printf( "\tbutton $w.f.back -text \"Main Menu\" \\\n" );
    else
	printf( "\tbutton $w.f.back -text \"OK\" \\\n" );
    printf( "\t\t-width 15 -command \"catch {focus $oldFocus}; destroy $w; unregister_active %d\"\n",
	menu_num );
    printf( "\tbutton $w.f.next -text \"Next\" -underline 0\\\n" );
    printf( "\t\t-width 15 -command $nextscript\n");

    if ( menu_num == tot_menu_num ) {
      printf( "\t$w.f.next configure -state disabled\n" );
        /* 
         *  this is a bit hackish but Alt-n must be rebound
         *  otherwise if the user press Alt-n on the last menu
         *  it will give him/her the next menu of one of the 
         *  previous options
         */
        printf( "\tbind all <Alt-n> \"puts \\\"no more menus\\\" \"\n");
    }
    else
    {
        /*
         * I should be binding to $w not all - but if I do nehat I get the error "unknown path"
         */
        printf( "\tbind all <Alt-n> $nextscript\n");
    }
    printf( "\tbutton $w.f.prev -text \"Prev\" -underline 0\\\n" );
    printf( "\t\t-width 15 -command \"catch {focus $oldFocus}; destroy $w; unregister_active %d; menu%d .menu%d \\\"$title\\\"\"\n",
	menu_num, menu_num-1, menu_num-1 );
    if ( menu_num == 1 ) {
	printf( "\t$w.f.prev configure -state disabled\n" );
    }
    else
    {
        printf( "\tbind $w <Alt-p> \"catch {focus $oldFocus}; destroy $w; unregister_active %d; menu%d .menu%d \\\"$title\\\";break\"\n",
            menu_num, menu_num-1, menu_num-1 );
    }  
    printf( "\tpack $w.f.back $w.f.next $w.f.prev -side left -expand on\n" );
    printf( "\tpack $w.f -pady 10 -side bottom -anchor w -fill x\n" );

    /*
     * Lines between canvas and other areas of the window.
     */
    printf( "\tframe $w.topline -relief ridge -borderwidth 2 -height 2\n" );
    printf( "\tpack $w.topline -side top -fill x\n\n" );
    printf( "\tframe $w.botline -relief ridge -borderwidth 2 -height 2\n" );
    printf( "\tpack $w.botline -side bottom -fill x\n\n" );

    /*
     * The "config" frame contains the canvas and a scrollbar.
     */
    printf( "\tframe $w.config\n" );
    printf( "\tpack $w.config -fill y -expand on\n\n" );
    printf( "\tscrollbar $w.config.vscroll -command \"$w.config.canvas yview\"\n" );
    printf( "\tpack $w.config.vscroll -side right -fill y\n\n" );

    /*
     * The scrollable canvas itself, where the real work (and mess) gets done.
     */
    printf( "\tcanvas $w.config.canvas -height 1\\\n" );
    printf( "\t\t-relief flat -borderwidth 0 -yscrollcommand \"$w.config.vscroll set\" \\\n" );
    printf( "\t\t-width [expr [winfo screenwidth .] * 1 / 2] \n" );
    printf( "\tframe $w.config.f\n" );
    printf( "\tbind $w <Key-Down> \"$w.config.canvas yview scroll  1 unit;break;\"\n");
    printf( "\tbind $w <Key-Up> \"$w.config.canvas yview scroll  -1 unit;break;\"\n");
    printf( "\tbind $w <Key-Next> \"$w.config.canvas yview scroll  1 page;break;\"\n");
    printf( "\tbind $w <Key-Prior> \"$w.config.canvas yview scroll  -1 page;break;\"\n");
    printf( "\tbind $w <Key-Home> \"$w.config.canvas yview moveto 0;break;\"\n");
    printf( "\tbind $w <Key-End> \"$w.config.canvas yview moveto 1 ;break;\"\n");
    printf( "\tpack $w.config.canvas -side right -fill y\n" );
    printf("\n\n");
}



/*
 * Each proc we create needs a global declaration for any global variables we
 * use.  To minimize the size of the file, we set a flag each time we output
 * a global declaration so we know whether we need to insert one for a
 * given function or not.
 */
static void clear_globalflags(void)
{
    int i;
    for ( i = 1; i <= max_varnum; i++ )
	vartable[i].global_written = 0;
}



/*
 * Output a "global" line for a given variable.  Also include the
 * call to "vfix".  (If vfix is not needed, then it's fine to just printf
 * a "global" line).
 */
void global( const char *var )
{
    printf( "\tglobal %s\n", var );
}



/*
 * This function walks the chain of conditions that we got from cond.c
 * and creates a TCL conditional to enable/disable a given widget.
 */
void generate_if( struct kconfig * cfg, struct condition * ocond,
    int menu_num, int line_num )
{
    struct condition * cond;
    struct dependency * tmp;
    struct kconfig * cfg1;

    if ( line_num >= -1 )
    {
	if ( cfg->token == token_define_bool || cfg->token == token_define_hex
	||   cfg->token == token_define_int || cfg->token == token_define_string
	||   cfg->token == token_define_tristate || cfg->token == token_unset )
	    return;
	if ( cfg->token == token_comment && line_num == -1 )
	    return;
    }
    else
    {
	if ( cfg->token == token_string || cfg->token == token_mainmenu_option )
	    return;
    }

    /*
     * First write any global declarations we need for this conditional.
     */
    for ( cond = ocond; cond != NULL; cond = cond->next )
    {
	switch ( cond->op )
	{
	default:
	    break;

	case op_variable:
	    if ( ! vartable[cond->nameindex].global_written )
	    {
		vartable[cond->nameindex].global_written = 1;
		global( vartable[cond->nameindex].name );
	    }
	    break;
	}
    }

    /*
     * Now write this option.
     */
    if ( cfg->nameindex > 0 && ! vartable[cfg->nameindex].global_written )
    {
	vartable[cfg->nameindex].global_written = 1;
	global( vartable[cfg->nameindex].name );
    }

    /*
     * Generate the body of the conditional.
     */
    printf( "\tif {" );
    for ( cond = ocond; cond != NULL; cond = cond->next )
    {
	switch ( cond->op )
	{
	default:
	    break;

	case op_bang:   printf( " ! "  ); break;
	case op_eq:     printf( " == " ); break;
	case op_neq:    printf( " != " ); break;
	case op_and:    printf( " && " ); break;
	case op_and1:   printf( " && " ); break;
	case op_or:     printf( " || " ); break;
	case op_lparen: printf( "("    ); break;
	case op_rparen: printf( ")"    ); break;

	case op_variable:
	    printf( "$%s", vartable[cond->nameindex].name );
	    break;

	case op_constant:
	    if      ( strcmp( cond->str, "y" ) == 0 ) printf( "1" );
	    else if ( strcmp( cond->str, "n" ) == 0 ) printf( "0" );
	    else if ( strcmp( cond->str, "m" ) == 0 ) printf( "2" );
	    else if ( strcmp( cond->str, "" ) == 0 )  printf( "4" );
	    else
		printf( "\"%s\"", cond->str );
	    break;
	}
    }
    printf( "} then {" );

    /*
     * Generate a procedure call to write the value.
     * This code depends on procedures in header.tk.
     */
    if ( line_num >= -1 )
    {
	int modtoyes = 0;

	switch ( cfg->token )
	{
	default:
	    printf( " }\n" );
	    break;

	case token_dep_mbool:
	    modtoyes = 1;
	case token_dep_bool:
	    printf( "\n" );
	    for ( tmp = cfg->depend; tmp; tmp = tmp->next )
		if ( ! vartable[get_varnum( tmp->name )].global_written )
		{
		    global( tmp->name );
		}
	    printf( "\tset tmpvar_dep [effective_dep [list" );
	    for ( tmp = cfg->depend; tmp; tmp = tmp->next )
		printf( " $%s", tmp->name );
	    printf( "]];set %s [sync_bool $%s $tmpvar_dep %d];",
		vartable[cfg->nameindex].name, vartable[cfg->nameindex].name,
		modtoyes );
	    printf( "if {$tmpvar_dep != 1" );
	    if (modtoyes)
		printf( " && $tmpvar_dep != 2" );
	    printf( "} then {configure_entry .menu%d.config.f.x%d disabled {y};",
		menu_num, line_num );
	    printf( "} else {" );
	    printf( "configure_entry .menu%d.config.f.x%d normal {y};",
		menu_num, line_num );
	    printf( "}; " );
	case token_bool:
	    if ( cfg->token == token_bool )
		printf( "\n\t" );
	    printf( "configure_entry .menu%d.config.f.x%d normal {n l",
		menu_num, line_num );
	    if ( cfg->token == token_bool )
		printf( " y" );
	    printf( "}" );
	    printf( "} else {");
	    printf( "configure_entry .menu%d.config.f.x%d disabled {y n l}}\n",
		menu_num, line_num );
	    break;

	case token_choice_header:
	    printf( "configure_entry .menu%d.config.f.x%d normal {x l}",
		menu_num, line_num );
	    printf( "} else {" );
	    printf( "configure_entry .menu%d.config.f.x%d disabled {x l}",
		menu_num, line_num );
	    printf( "}\n" );
	    break;

	case token_choice_item:
	    fprintf( stderr, "Internal error on token_choice_item\n" );
	    exit( 1 );

	case token_dep_tristate:
	    printf( "\n" );
	    for ( tmp = cfg->depend; tmp; tmp = tmp->next )
		if ( ! vartable[get_varnum( tmp->name )].global_written )
		{
		    global( tmp->name );
		}
	    printf( "\tset tmpvar_dep [effective_dep [list" );
	    for ( tmp = cfg->depend; tmp; tmp = tmp->next )
		printf( " $%s", tmp->name );
	    printf( "]];set %s [sync_tristate $%s $tmpvar_dep];",
		vartable[cfg->nameindex].name, vartable[cfg->nameindex].name );
	    printf( "\tif {$tmpvar_dep != 1} then {" );
	    printf( "configure_entry .menu%d.config.f.x%d disabled {y}",
		menu_num, line_num );
	    printf( "} else {" );
	    printf( "configure_entry .menu%d.config.f.x%d normal {y}",
		menu_num, line_num );
	    printf( "}; " );
	    printf( "if {$tmpvar_dep == 0} then {" );
	    printf( "configure_entry .menu%d.config.f.x%d disabled {m}",
		menu_num, line_num );
	    printf( "} else {" );
	    printf( "configure_entry .menu%d.config.f.x%d normal {m}",
		menu_num, line_num );
	    printf( "}; " );
	case token_tristate:
	    if ( cfg->token == token_tristate )
	    {
		printf( "\n\tconfigure_entry .menu%d.config.f.x%d normal {y}; ",
		    menu_num, line_num );
	    }
	    printf( "if {($CONFIG_MODULES == 1)} then {" );
	    printf( "configure_entry .menu%d.config.f.x%d normal {m}} else {",
		menu_num, line_num );
	    printf( "configure_entry .menu%d.config.f.x%d disabled {m}}; ",
		menu_num, line_num );
	    printf( "configure_entry .menu%d.config.f.x%d normal {n l}",
		menu_num, line_num );

	/*
	 * Or in a bit to the variable - this causes all of the radiobuttons
	 * to be deselected (i.e. not be red).
	 */
	    printf( "} else {" );
	    printf( "configure_entry .menu%d.config.f.x%d disabled {y n m l}}\n",
		menu_num, line_num );
	    break;

	case token_hex:
	case token_int:
	case token_string:
	    printf( ".menu%d.config.f.x%d.x configure -state normal -foreground [ cget .ref -foreground ]; ",
		menu_num, line_num );
	    printf( ".menu%d.config.f.x%d.l configure -state normal; ",
		menu_num, line_num );
	    printf( "} else {" );
	    printf( ".menu%d.config.f.x%d.x configure -state disabled -foreground [ cget .ref -disabledforeground ]; ",
		menu_num, line_num );
	    printf( ".menu%d.config.f.x%d.l configure -state disabled}\n",
		menu_num, line_num );
	    break;

	case token_comment:
	case token_mainmenu_option:
	    if ( line_num >= 0 )
	    {
		printf( "configure_entry .menu%d.config.f.x%d normal {m}",
		    menu_num, line_num );
		printf( "} else {" );
		printf( "configure_entry .menu%d.config.f.x%d disabled {m}}\n",
		    menu_num, line_num );
	    }
	    else
		printf( ".f0.x%d configure -state normal } else { .f0.x%d configure -state disabled }\n",
		    menu_num, menu_num );
	    break;
	}
    }
    else
    {
	int modtoyes = 0;

	switch ( cfg->token )
	{
	default:
	    printf( " }\n" );
	    break;

	case token_dep_mbool:
	    modtoyes = 1;
	case token_dep_bool:
	    printf( "\n" );
	    for ( tmp = cfg->depend; tmp; tmp = tmp->next )
		if ( ! vartable[get_varnum( tmp->name )].global_written )
		{
		    global( tmp->name );
		}
	    printf( "\tset tmpvar_dep [effective_dep [list" );
	    for ( tmp = cfg->depend; tmp; tmp = tmp->next )
		printf( " $%s", tmp->name );
	    printf( "]];set %s [sync_bool $%s $tmpvar_dep %d];",
		vartable[cfg->nameindex].name, vartable[cfg->nameindex].name,
		modtoyes );
	case token_bool:
	    if ( cfg->token == token_bool )
		printf( "\n\t" );
	    printf( "set %s [expr $%s&15]",
		vartable[cfg->nameindex].name, vartable[cfg->nameindex].name );
	    printf( "} else {");
	    printf( "set %s [expr $%s|16]}\n",
		vartable[cfg->nameindex].name, vartable[cfg->nameindex].name );
	    break;

	case token_choice_header:
	    printf( "} else {" );
	    for ( cfg1  = cfg->next;
		  cfg1 != NULL && cfg1->token == token_choice_item;
		  cfg1  = cfg1->next )
		printf( "set %s 4;", vartable[cfg1->nameindex].name );
	    printf( "}\n" );
	    break;

	case token_choice_item:
	    fprintf( stderr, "Internal error on token_choice_item\n" );
	    exit( 1 );

	case token_define_bool:
	case token_define_tristate:
	    if ( ! vartable[get_varnum( cfg->value )].global_written )
	    {
		global( cfg->value );
	    }
	    printf( "set %s $%s }\n",
		vartable[cfg->nameindex].name, cfg->value );
	    break;

	case token_define_hex:
	case token_define_int:
	    printf( "set %s %s }\n",
		vartable[cfg->nameindex].name, cfg->value );
	    break;

	case token_define_string:
	    printf( "set %s \"%s\" }\n",
		vartable[cfg->nameindex].name, cfg->value );
	    break;

	case token_dep_tristate:
	    printf( "\n" );
	    for ( tmp = cfg->depend; tmp; tmp = tmp->next )
		if ( ! vartable[get_varnum( tmp->name )].global_written )
		{
		    global( tmp->name );
		}
	    printf( "\tset tmpvar_dep [effective_dep [list" );
	    for ( tmp = cfg->depend; tmp; tmp = tmp->next )
		printf( " $%s", tmp->name );
	    printf( "]]; set %s [sync_tristate $%s $tmpvar_dep]; ",
		vartable[cfg->nameindex].name, vartable[cfg->nameindex].name );
	case token_tristate:
	    if ( cfg->token == token_tristate )
		printf( "if {($CONFIG_MODULES == 0) && ($%s == 2)} then {set %s 1}; ",
		    vartable[cfg->nameindex].name,
		    vartable[cfg->nameindex].name );
	/*
	 * Or in a bit to the variable - this causes all of the radiobuttons
	 * to be deselected (i.e. not be red).
	 */
	    printf( "set %s [expr $%s&15]",
		vartable[cfg->nameindex].name, vartable[cfg->nameindex].name );
	    printf( "} else {" );

	/*
	 * Clear the disable bit to enable the correct radiobutton.
	 */
	    printf( "set %s [expr $%s|16]}\n",
		vartable[cfg->nameindex].name, vartable[cfg->nameindex].name );
	    break;

	case token_hex:
	case token_int:
	    if ( cfg->value && *cfg->value == '$' )
	    {
		int i = get_varnum( cfg->value+1 );
		printf( "\n" );
		if ( ! vartable[i].global_written )
		{
		    global( vartable[i].name );
		}
		printf( "\t" );
	    }
	    if ( cfg->token == token_hex )
		printf( "validate_hex " );
	    else if ( cfg->token == token_int )
		printf( "validate_int " );
	    printf( "%s \"$%s\" %s}\n",
		vartable[cfg->nameindex].name, vartable[cfg->nameindex].name,
		cfg->value );
	    break;

	case token_unset:
	    printf( "set %s 4}\n", vartable[cfg->nameindex].name );
	    break;
	}
    }
}


/*
 * Generate a line that writes a variable to the output file.
 */
void generate_writeconfig( struct kconfig * cfg )
{
    struct condition * cond;
    struct dependency * tmp;
    int depmod = 2;
    
    /*
     * Generate global declaration for this symbol.
     */
    if ( cfg->token != token_comment )
    {
	if ( cfg->nameindex > 0 && ! vartable[cfg->nameindex].global_written )
	{
	    vartable[cfg->nameindex].global_written = 1;
	    global( vartable[cfg->nameindex].name );
	}
	if ( cfg->token == token_define_tristate || cfg->token == token_define_bool )
	{
	    if ( ! vartable[get_varnum( cfg->value )].global_written )
	    {
		vartable[get_varnum( cfg->value )].global_written = 1;
		global( cfg->value );
	    }
	}
	else if ( cfg->nameindex <= 0 && cfg->token == token_choice_header )
	{
	    printf( "\tglobal tmpvar_%d\n", -(cfg->nameindex) );
	}
    }

    /*
     * Generate global declarations for the condition chain.
     */
    for ( cond = cfg->cond; cond != NULL; cond = cond->next )
    {
	switch( cond->op )
	{
	default:
	    break;

	case op_variable:
	    if ( ! vartable[cond->nameindex].global_written )
	    {
		vartable[cond->nameindex].global_written = 1;
		global( vartable[cond->nameindex].name );
	    }
	    break;
	}
    }

    /*
     * Generate indentation.
     */
	printf( "\t" );

    /*
     * Generate the conditional.
     */
    if ( cfg->cond != NULL )
    {
	printf( "if {" );
	for ( cond = cfg->cond; cond != NULL; cond = cond->next )
	{
	    switch ( cond->op )
	    {
	    default:           break;
	    case op_bang:      printf( " ! "  ); break;
	    case op_eq:        printf( " == " ); break;
	    case op_neq:       printf( " != " ); break;
	    case op_and:       printf( " && " ); break;
	    case op_and1:      printf( " && " ); break;
	    case op_or:        printf( " || " ); break;
	    case op_lparen:    printf( "("    ); break;
	    case op_rparen:    printf( ")"    ); break;

	    case op_variable:
		printf( "$%s", vartable[cond->nameindex].name );
		break;

	    case op_constant:
		if      ( strcmp( cond->str, "n" ) == 0 ) printf( "0" );
		else if ( strcmp( cond->str, "y" ) == 0 ) printf( "1" );
		else if ( strcmp( cond->str, "m" ) == 0 ) printf( "2" );
		else if ( strcmp( cond->str, "" ) == 0 )  printf( "4" );
		else
		    printf( "\"%s\"", cond->str );
		break;
	    }
	}
	printf( "} then {" );
    }

    /*
     * Generate a procedure call to write the value.
     * This code depends on the write_* procedures in header.tk.
     */
    switch ( cfg->token )
    {
    default:
	if ( cfg->cond != NULL )
	    printf( " }" );
	printf( "\n" );
	break;

    case token_bool:
    case token_tristate:
	printf( "write_tristate $cfg $autocfg %s $%s [list $notmod] 2", 
	    vartable[cfg->nameindex].name, vartable[cfg->nameindex].name );
	if ( cfg->cond != NULL )
	    printf( " }" );
	printf( "\n" );
	break;

    case token_choice_header:
	/*
	 * This is funky code -- it fails if there were any conditionals.
	 * Fortunately all the conditionals got stripped off somewhere
	 * else.
	 */
	{
	    struct kconfig * cfg1;
	    for ( cfg1  = cfg->next;
		  cfg1 != NULL && cfg1->token == token_choice_item;
		  cfg1  = cfg1->next )
	    {
		printf("\n\tif { $tmpvar_%d == \"%s\" } then { write_tristate $cfg $autocfg %s 1 [list $notmod] 2 } else { write_tristate $cfg $autocfg %s 0 [list $notmod] 2 }",
		    -(cfg->nameindex), cfg1->label,
		    vartable[cfg1->nameindex].name,
		    vartable[cfg1->nameindex].name );
	    }
	}
	if ( cfg->cond != NULL )
	    printf( "}" );
	printf( "\n" );
	break;

    case token_choice_item:
	fprintf( stderr, "Internal error on token_choice_item\n" );
	exit( 1 );

    case token_comment:
	printf( "write_comment $cfg $autocfg \"%s\"",
	    cfg->label );
	if ( cfg->cond != NULL )
	    printf( "}" );
	printf( "\n" );
	break;

    case token_define_bool:
    case token_define_tristate:
	if ( cfg->cond == NULL )
	{
	    printf( "write_tristate $cfg $autocfg %s $%s [list $notmod] 2\n",
		vartable[cfg->nameindex].name, vartable[cfg->nameindex].name );
	}
	else
	{
	    printf( "write_tristate $cfg $autocfg %s $%s [list $notmod] 2 }\n",
		vartable[cfg->nameindex].name, cfg->value );
	}
	break;

    case token_dep_mbool:
	depmod = 1;
    case token_dep_bool:
    case token_dep_tristate:
	printf( "write_tristate $cfg $autocfg %s $%s [list",
	    vartable[cfg->nameindex].name, vartable[cfg->nameindex].name );
	for ( tmp = cfg->depend; tmp; tmp = tmp->next )
	    printf( " $%s", tmp->name );
	printf( "] %d", depmod );
	if ( cfg->cond != NULL )
	    printf( " }" );
	printf( "\n" );
	break;

    case token_define_hex:
	printf( "write_hex $cfg $autocfg %s %s $notmod",
	    vartable[cfg->nameindex].name, cfg->value );
	if ( cfg->cond != NULL )
	    printf( " }" );
	printf( "\n" );
	break;

    case token_define_int:
	printf( "write_int $cfg $autocfg %s %s $notmod",
	    vartable[cfg->nameindex].name, cfg->value );
	if ( cfg->cond != NULL )
	    printf( " }" );
	printf( "\n" );
	break;

    case token_define_string:
	printf( "write_string $cfg $autocfg %s \"%s\" $notmod",
	    vartable[cfg->nameindex].name, cfg->value );
	if ( cfg->cond != NULL )
	    printf( " }" );
	printf( "\n" );
	break;

    case token_hex:
	printf( "write_hex $cfg $autocfg %s $%s $notmod",
	    vartable[cfg->nameindex].name, vartable[cfg->nameindex].name );
	if ( cfg->cond != NULL )
	    printf( " }" );
	printf( "\n" );
	break;

    case token_int:
	printf( "write_int $cfg $autocfg %s $%s $notmod",
	    vartable[cfg->nameindex].name, vartable[cfg->nameindex].name );
	if ( cfg->cond != NULL )
	    printf( " }" );
	printf( "\n" );
	break;

    case token_string:
	printf( "write_string $cfg $autocfg %s \"$%s\" $notmod",
	    vartable[cfg->nameindex].name, vartable[cfg->nameindex].name );
	if ( cfg->cond != NULL )
	    printf( " }" );
	printf( "\n" );
	break;
    }
}

static void generate_update_var( struct kconfig * scfg, int menu_num )
{
    struct kconfig * cfg;

    if ( menu_num>0 )
    {
	printf( "proc update_define_menu%d {} {\n", menu_num );
	printf( "\tupdate_define_mainmenu\n" );
    }
    else
	printf( "proc update_define_mainmenu {} {\n" );
    clear_globalflags();
    global( "CONFIG_MODULES" );
    vartable[ get_varnum( "CONFIG_MODULES" ) ].global_written = 1;
    for ( cfg = scfg; cfg != NULL; cfg = cfg->next )
    {
	if ( cfg->menu_number == menu_num && (cfg->token == token_define_bool || cfg->token == token_define_tristate
	||   cfg->token == token_define_hex || cfg->token == token_define_int
	||   cfg->token == token_define_string || cfg->token == token_unset 
	||   cfg->token == token_tristate) )
	{
	    if ( ! vartable[cfg->nameindex].global_written )
	    {
		vartable[cfg->nameindex].global_written = 1;
		global( vartable[cfg->nameindex].name );
	    }
	}
    }

    for ( cfg = scfg; cfg != NULL; cfg = cfg->next )
    {
	char tmp[20];
	struct kconfig * cfg1;

	if ( cfg->menu_number == menu_num )
	{
	    switch ( cfg->token )
	    {
	    default:
	    case token_choice_item:
		break;
	    case token_choice_header:
		sprintf( tmp, "tmpvar_%d", -(cfg->nameindex) );
		global( tmp );
		for ( cfg1  = cfg->next;
		      cfg1 != NULL && cfg1->token == token_choice_item;
		      cfg1  = cfg1->next )
		{
		    vartable[cfg1->nameindex].global_written = 1;
		    global( vartable[cfg1->nameindex].name );
		    printf( "\tif {$tmpvar_%d == \"%s\"} then {set %s 1} else {set %s 0}\n",
			-(cfg->nameindex), cfg1->label,
			vartable[cfg1->nameindex].name,
			vartable[cfg1->nameindex].name );
		}
		break;
	    case token_bool:
	    case token_define_bool:
	    case token_define_tristate:
	    case token_define_hex:
	    case token_define_int:
	    case token_define_string:
	    case token_dep_bool:
	    case token_dep_tristate:
	    case token_dep_mbool:
	    case token_int:
	    case token_hex:
	    case token_mainmenu_option:
	    case token_tristate:
	    case token_unset:
		if ( cfg->cond != NULL )
		    generate_if( cfg, cfg->cond, menu_num, -2 );
		else switch ( cfg->token )
		{
		case token_tristate:
		    printf( "\n\tif {($CONFIG_MODULES == 0)} then {if {($%s == 2)} then {set %s 1}}\n",
			vartable[cfg->nameindex].name, vartable[cfg->nameindex].name );
		    break;
		case token_define_bool:
		case token_define_tristate:
		    if ( ! vartable[get_varnum( cfg->value )].global_written )
		    {
			vartable[get_varnum( cfg->value )].global_written = 1;
			global( cfg->value );
		    }
		    printf( "\tset %s $%s\n", vartable[cfg->nameindex].name,
			cfg->value );
		    break;
		case token_define_hex:
		case token_define_int:
		    printf( "\tset %s %s\n", vartable[cfg->nameindex].name,
			cfg->value );
		    break;
		case token_define_string:
		    printf( "\tset %s \"%s\"\n", vartable[cfg->nameindex].name,
			cfg->value );
		    break;
		case token_unset:
		    printf( "\tset %s 4\n", vartable[cfg->nameindex].name );
		default:
		    break;
		}
	    }
	}
    }
    printf( "}\n\n\n" );
}


/*
 * Generates the end of a menu procedure.
 */
static void end_proc( struct kconfig * scfg, int menu_num )
{
    struct kconfig * cfg;

    printf( "\n\n\n" );
    printf( "\tfocus $w\n" );
    printf( "\tupdate_active\n" );
    printf( "\tglobal winx; global winy\n" );
    if ( menu_first[menu_num]->menu_number != 0 )
    {
	printf( "\tif {[winfo exists .menu%d] == 0} then ",
		menu_first[menu_num]->menu_number );
	printf( "{menu%d .menu%d \"%s\"}\n",
		menu_first[menu_num]->menu_number, menu_first[menu_num]->menu_number,
		menu_first[menu_first[menu_num]->menu_number]->label );
	printf( "\tset winx [expr [winfo x .menu%d]+30]; set winy [expr [winfo y .menu%d]+30]\n",
		menu_first[menu_num]->menu_number, menu_first[menu_num]->menu_number );
    }
    else
	printf( "\tset winx [expr [winfo x .]+30]; set winy [expr [winfo y .]+30]\n" );
    printf( "\tif {[winfo exists $w]} then {wm geometry $w +$winx+$winy}\n" );

    /*
     * Now that the whole window is in place, we need to wait for an "update"
     * so we can tell the canvas what its virtual size should be.
     *
     * Unfortunately, this causes some ugly screen-flashing because the whole
     * window is drawn, and then it is immediately resized.  It seems
     * unavoidable, though, since "frame" objects won't tell us their size
     * until after an update, and "canvas" objects can't automatically pack
     * around frames.  Sigh.
     */
    printf( "\tupdate idletasks\n" );
    printf( "\tif {[winfo exists $w]} then  {$w.config.canvas create window 0 0 -anchor nw -window $w.config.f\n\n" );
    printf( "\t$w.config.canvas configure \\\n" );
    printf( "\t\t-width [expr [winfo reqwidth $w.config.f] + 1]\\\n" );
    printf( "\t\t-scrollregion \"-1 -1 [expr [winfo reqwidth $w.config.f] + 1] \\\n" );
    printf( "\t\t\t [expr [winfo reqheight $w.config.f] + 1]\"\n\n" );
	 
    /*
     * If the whole canvas will fit in 3/4 of the screen height, do it;
     * otherwise, resize to around 1/2 the screen and let us scroll.
     */
    printf( "\tset winy [expr [winfo reqh $w] - [winfo reqh $w.config.canvas]]\n" );
    printf( "\tset scry [expr [winfo screenh $w] / 2]\n" );
    printf( "\tset maxy [expr [winfo screenh $w] * 3 / 4]\n" );
    printf( "\tset canvtotal [expr [winfo reqh $w.config.f] + 2]\n" );
    printf( "\tif [expr $winy + $canvtotal < $maxy] {\n" );
    printf( "\t\t$w.config.canvas configure -height $canvtotal\n" );
    printf( "\t} else {\n" );
    printf( "\t\t$w.config.canvas configure -height [expr $scry - $winy]\n" );
    printf( "\t\t}\n\t}\n" );

    /*
     * Limit the min/max window size.  Height can vary, but not width,
     * because of the limitations of canvas and our laziness.
     */
    printf( "\tupdate idletasks\n" );
    printf( "\tif {[winfo exists $w]} then {\n\twm maxsize $w [winfo width $w] [winfo screenheight $w]\n" );
    printf( "\twm minsize $w [winfo width $w] 100\n\n" );
    printf( "\twm deiconify $w\n" );
    printf( "}\n}\n\n" );

    /*
     * Now we generate the companion procedure for the menu we just
     * generated.  This procedure contains all of the code to
     * disable/enable widgets based upon the settings of the other
     * widgets, and will be called first when the window is mapped,
     * and each time one of the buttons in the window are clicked.
     */
    printf( "proc update_menu%d {} {\n", menu_num );

    /*
     * Clear all of the booleans that are defined in this menu.
     */
    clear_globalflags();
    for ( cfg = scfg; cfg != NULL; cfg = cfg->next )
    {
	if ( cfg->menu_number == menu_num
	&&   cfg->token != token_mainmenu_option
	&&   cfg->token != token_choice_item )
	{
	    if ( cfg->cond != NULL )
	    {
		int i;
		if ( (cfg->token == token_tristate || cfg->token == token_dep_tristate)
		&& ! vartable[i = get_varnum( "CONFIG_MODULES" )].global_written )
		{
		    global( "CONFIG_MODULES" );
		    vartable[i].global_written = 1;
		}
		generate_if( cfg, cfg->cond, cfg->menu_number, cfg->menu_line );
	    }
	    else
	    {
		if ( cfg->token == token_tristate )
		{
		    int i;
		    if ( ! vartable[cfg->nameindex].global_written )
		    {
			vartable[cfg->nameindex].global_written = 1;
			printf( "\tglobal %s\n", vartable[cfg->nameindex].name );
		    }
		    if ( ! vartable[i = get_varnum( "CONFIG_MODULES" )].global_written )
		    {
			global( "CONFIG_MODULES" );
			vartable[i].global_written = 1;
		    }
		    printf( "\n\tif {($CONFIG_MODULES == 1)} then {configure_entry .menu%d.config.f.x%d normal {m}} else {configure_entry .menu%d.config.f.x%d disabled {m}}\n",
			menu_num, cfg->menu_line,
			menu_num, cfg->menu_line );
		}
	    }
	}
	else if ( cfg->token == token_mainmenu_option
	     &&   cfg->menu_number == menu_num
	     &&   cfg->cond != NULL )
	{
	    generate_if( cfg, cfg->cond, menu_num, cfg->menu_line );
	}
    }
    printf("}\n\n\n");

    generate_update_var( scfg, menu_num );
}

/*
 * This is the top level function for generating the tk script.
 */
void dump_tk_script( struct kconfig * scfg )
{
    int menu_depth;
    int menu_num [64];
    int imenu, i;
    int top_level_num = 0;
    struct kconfig * cfg;
    struct kconfig * cfg1 = NULL;
    const char * name = "No Name";

    /*
     * Mark begin and end of each menu so I can omit submenus when walking
     * over a parent menu.
     */
    tot_menu_num = 0;
    menu_depth   = 0;
    menu_num [0] = 0;

    for ( cfg = scfg; cfg != NULL; cfg = cfg->next )
    {
	switch ( cfg->token )
	{
	default:
	    break;

	case token_mainmenu_name:
	    name = cfg->label;
	    break;

	case token_mainmenu_option:
	    if ( ++menu_depth >= 64 )
		{ fprintf( stderr, "menus too deep\n" ); exit( 1 ); }
	    if ( ++tot_menu_num >= 100 )
		{ fprintf( stderr, "too many menus\n" ); exit( 1 ); }
	    menu_num   [menu_depth]   = tot_menu_num;
	    menu_first [tot_menu_num] = cfg;
	    menu_last  [tot_menu_num] = cfg;
	    /*
	     * Note, that menu_number is set to the number of parent 
	     * (upper level) menu.
	     */
	    cfg->menu_number = menu_num[menu_depth - 1];
	    if ( menu_depth == 1 )
		++top_level_num;
	    break;

	case token_endmenu:
	    menu_last [menu_num [menu_depth]] = cfg;
	    /* flatten menus with proper scoping */
	    if ( --menu_depth < 0 )
		{ fprintf( stderr, "unmatched endmenu\n" ); exit( 1 ); }
	    break;

	case token_bool:
	case token_choice_header:
	case token_choice_item:
	case token_comment:
	case token_dep_bool:
	case token_dep_tristate:
	case token_dep_mbool:
	case token_hex:
	case token_int:
	case token_string:
	case token_tristate:
	    cfg->menu_number = menu_num[menu_depth];
	    if ( menu_depth == 0 )
		{ fprintf( stderr, "statement not in menu\n" ); exit( 1 ); }
	    break;

	case token_define_bool:
	case token_define_hex:
	case token_define_int:
	case token_define_string:
	case token_define_tristate:
	case token_unset:
	    cfg->menu_number = menu_num[menu_depth];
	    break;
	}
    }

    /*
     * Generate menus per column setting.
     * There are:
     *   four extra buttons for save/quit/load/store;
     *   one blank button
     *   add two to round up for division
     */
    printf( "set menus_per_column %d\n", (top_level_num + 4 + 1 + 2) / 3 );
    printf( "set total_menus %d\n\n", tot_menu_num );

    printf( "proc toplevel_menu {num} {\n" );
    for ( imenu = 1; imenu <= tot_menu_num; ++imenu )
    {
	int parent = 1;

	if ( menu_first[imenu]->menu_number == 0 )
	    parent = menu_first[imenu]->menu_number;
	else
	    printf( "\tif {$num == %d} then {return %d}\n",
		imenu, menu_first[imenu]->menu_number );
    }
    printf( "\treturn $num\n}\n\n" );

    /*
     * Generate the menus.
     */
    printf( "mainmenu_name \"%s\"\n", name );
    for ( imenu = 1; imenu <= tot_menu_num; ++imenu )
    {
	int menu_line = 0;
	int nr_submenu = imenu;
	int menu_name_omitted = 0;
	int opt_count = 0;

	clear_globalflags();
	start_proc( menu_first[imenu]->label, imenu, 
		!menu_first[imenu]->menu_number );

	for ( cfg = menu_first[imenu]->next; cfg != NULL && cfg != menu_last[imenu]; cfg = cfg->next )
	{
	    switch ( cfg->token )
	    {
	    default:
		break;

	    case token_mainmenu_option:
		while ( menu_first[++nr_submenu]->menu_number > imenu )
		    ;
		cfg->menu_line = menu_line++;
		printf( "\tsubmenu $w.config.f %d %d \"%s\" %d\n",
		    cfg->menu_number, cfg->menu_line, cfg->label, nr_submenu );
		cfg = menu_last[nr_submenu];
		break;

	    case token_comment:
		if ( !cfg->menu_line && !menu_name_omitted )
		{
		    cfg->menu_line = -1;
		    menu_name_omitted = 1;
		}
		else
		{
		    menu_name_omitted = 1;
		    cfg->menu_line = menu_line++;
		    printf( "\tcomment $w.config.f %d %d \"%s\"\n",
			cfg->menu_number, cfg->menu_line, cfg->label );
		}
		break;

	    case token_bool:
		cfg->menu_line = menu_line++;
		printf( "\tbool $w.config.f %d %d \"%s\" %s\n",
		    cfg->menu_number, cfg->menu_line, cfg->label,
		    vartable[cfg->nameindex].name );
		break;

	    case token_choice_header:
		/*
		 * I need the first token_choice_item to pick out the right
		 * help text from Documentation/Configure.help.
		 */
		cfg->menu_line = menu_line++;
		printf( "\tglobal tmpvar_%d\n", -(cfg->nameindex) );
		printf( "\tminimenu $w.config.f %d %d \"%s\" tmpvar_%d %s\n",
		    cfg->menu_number, cfg->menu_line, cfg->label,
		    -(cfg->nameindex), vartable[cfg->next->nameindex].name );
		printf( "\tmenu $w.config.f.x%d.x.menu -tearoffcommand \"menutitle \\\"%s\\\"\"\n",
		    cfg->menu_line, cfg->label );
		cfg1 = cfg;
		opt_count = 0;
		break;

	    case token_choice_item:
		/* note: no menu line; uses choice header menu line */
		printf( "\t$w.config.f.x%d.x.menu add radiobutton -label \"%s\" -variable tmpvar_%d -value \"%s\" -command \"update_active\"\n",
		    cfg1->menu_line, cfg->label, -(cfg1->nameindex),
		    cfg->label );
		opt_count++;
		if ( cfg->next && cfg->next->token != token_choice_item ) {
		    /* last option in the menu */
		    printf( "\tmenusplit $w $w.config.f.x%d.x.menu %d\n",
			cfg1->menu_line, opt_count );
		}
		break;

	    case token_dep_bool:
	    case token_dep_mbool:
		cfg->menu_line = menu_line++;
		printf( "\tdep_bool $w.config.f %d %d \"%s\" %s\n",
		    cfg->menu_number, cfg->menu_line, cfg->label,
		    vartable[cfg->nameindex].name );
		break;

	    case token_dep_tristate:
		cfg->menu_line = menu_line++;
		printf( "\tdep_tristate $w.config.f %d %d \"%s\" %s\n",
		    cfg->menu_number, cfg->menu_line, cfg->label,
		    vartable[cfg->nameindex].name );
		break;

	    case token_hex:
		cfg->menu_line = menu_line++;
		printf( "\thex $w.config.f %d %d \"%s\" %s\n",
		    cfg->menu_number, cfg->menu_line, cfg->label,
		    vartable[cfg->nameindex].name );
		break;

	    case token_int:
		cfg->menu_line = menu_line++;
		printf( "\tint $w.config.f %d %d \"%s\" %s\n",
		    cfg->menu_number, cfg->menu_line, cfg->label,
		    vartable[cfg->nameindex].name );
		break;

	    case token_string:
		cfg->menu_line = menu_line++;
		printf( "\tistring $w.config.f %d %d \"%s\" %s\n",
		    cfg->menu_number, cfg->menu_line, cfg->label,
		    vartable[cfg->nameindex].name );
		break;

	    case token_tristate:
		cfg->menu_line = menu_line++;
		printf( "\ttristate $w.config.f %d %d \"%s\" %s\n",
		    cfg->menu_number, cfg->menu_line, cfg->label,
		    vartable[cfg->nameindex].name );
		break;
	    }
	}

	end_proc( scfg, imenu );
    }

    /*
     * The top level menu also needs an update function.  When we update a
     * submenu, we may need to disable one or more of the submenus on
     * the top level menu, and this procedure will ensure that things are
     * correct.
     */
    clear_globalflags();
    printf( "proc update_mainmenu {}  {\n" );
    for ( imenu = 1; imenu <= tot_menu_num; imenu++ )
    {
	if ( menu_first[imenu]->cond != NULL && menu_first[imenu]->menu_number == 0 )
	    generate_if( menu_first[imenu], menu_first[imenu]->cond, imenu, -1 );
    }
    printf( "}\n\n\n" );

    clear_globalflags();
    /*
     * Generate code to load the default settings into the variables.
     * The script in tail.tk will attempt to load .config,
     * which may override these settings, but that's OK.
     */
    for ( cfg = scfg; cfg != NULL; cfg = cfg->next )
    {
	switch ( cfg->token )
	{
	default:
	    break;

	case token_bool:
	case token_choice_item:
	case token_dep_bool:
	case token_dep_tristate:
	case token_dep_mbool:
	case token_tristate:
	    if ( ! vartable[cfg->nameindex].global_written )
	    {
		printf( "set %s 0\n", vartable[cfg->nameindex].name );
		vartable[cfg->nameindex].global_written = 1;
	    }
	    break;

	case token_choice_header:
	    printf( "set tmpvar_%d \"(not set)\"\n", -(cfg->nameindex) );
	    break;

	case token_hex:
	case token_int:
	    if ( ! vartable[cfg->nameindex].global_written )
	    {
		printf( "set %s %s\n", vartable[cfg->nameindex].name, cfg->value ? cfg->value : "0" );
		vartable[cfg->nameindex].global_written = 1;
	    }
	    break;

	case token_string:
	    if ( ! vartable[cfg->nameindex].global_written )
	    {
		printf( "set %s \"%s\"\n", vartable[cfg->nameindex].name, cfg->value );
		vartable[cfg->nameindex].global_written = 1;
	    }
	    break;
	}
    }

    /*
     * Define to an empty value all other variables (which are never defined)
     */
    for ( i = 1; i <= max_varnum; i++ )
    {
	if ( ! vartable[i].global_written
	&&   strncmp( vartable[i].name, "CONSTANT_", 9 ) )
	    printf( "set %s 4\n", vartable[i].name );
    }

    /*
     * Generate a function to write all of the variables to a file.
     */
    printf( "proc writeconfig {file1 file2} {\n" );
    printf( "\tset cfg [open $file1 w]\n" );
    printf( "\tset autocfg [open $file2 w]\n" );
    printf( "\tset notmod 1\n" );
    printf( "\tset notset 0\n" );
    printf( "\tputs $cfg \"#\"\n");
    printf( "\tputs $cfg \"# Automatically generated make config: don't edit\"\n");
    printf( "\tputs $cfg \"#\"\n" );

    printf( "\tputs $autocfg \"/*\"\n" );
    printf( "\tputs $autocfg \" * Automatically generated C config: don't edit\"\n" );
    printf( "\tputs $autocfg \" */\"\n" );
    printf( "\tputs $autocfg \"#define AUTOCONF_INCLUDED\"\n" );

    clear_globalflags();
    for ( cfg = scfg; cfg != NULL; cfg = cfg->next )
    {
	switch ( cfg->token )
	{
	default:
	    break;

	case token_bool:
	case token_choice_header:
	case token_comment:
	case token_define_bool:
	case token_define_hex:
	case token_define_int:
	case token_define_string:
	case token_define_tristate:
	case token_dep_bool:
	case token_dep_tristate:
	case token_dep_mbool:
	case token_hex:
	case token_int:
	case token_string:
	case token_tristate:
	    generate_writeconfig( cfg );
	    break;
	}
    }
    printf( "\tclose $cfg\n" );
    printf( "\tclose $autocfg\n" );
    printf( "}\n\n\n" );

    /*
     * Generate a simple function that updates the master choice
     * variable depending upon what values were loaded from a .config
     * file.  
     */
    printf( "proc clear_choices { } {\n" );
    for ( cfg = scfg; cfg != NULL; cfg = cfg->next )
    {
	if ( cfg->token == token_choice_header )
	{
	    for ( cfg1  = cfg->next; 
		  cfg1 != NULL && cfg1->token == token_choice_item;
		  cfg1  = cfg1->next )
	    {
		printf( "\tglobal %s; set %s 0\n",
		    vartable[cfg1->nameindex].name,
		    vartable[cfg1->nameindex].name );
	    }
	}
    }
    printf( "}\n\n\n" );

    printf( "proc update_choices { } {\n" );
    for ( cfg = scfg; cfg != NULL; cfg = cfg->next )
    {
	if ( cfg->token == token_choice_header )
	{
	    printf( "\tglobal tmpvar_%d\n", -(cfg->nameindex) );
	    printf("\tset tmpvar_%d \"%s\"\n", -(cfg->nameindex), cfg->value);
	    for ( cfg1  = cfg->next; 
		  cfg1 != NULL && cfg1->token == token_choice_item;
		  cfg1  = cfg1->next )
	    {
		printf( "\tglobal %s\n", vartable[cfg1->nameindex].name );
		printf( "\tif { $%s == 1 } then { set tmpvar_%d \"%s\" }\n",
		    vartable[cfg1->nameindex].name,
		    -(cfg->nameindex), cfg1->label );
	    }
	}
    }
    printf( "}\n\n\n" );

    generate_update_var( scfg, 0 );

    /*
     * That's it.  We are done.  The output of this file will have header.tk
     * prepended and tail.tk appended to create an executable wish script.
     */
}
