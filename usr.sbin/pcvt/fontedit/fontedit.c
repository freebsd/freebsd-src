/*
 * fontedit
 *	Fonteditor for VT220
 *
 *  BUGS:
 *	o Cursor motion is less than optimal (but who cares at 9600),
 *
 *  COMPILE:
 *	cc -O fontedit.c -o fontedit
 *      (use Makefile)
 *
 *	Copyright (c) 1987 by Greg Franks.
 *
 *	Permission is granted to do anything you want with this program
 *	except claim that you wrote it.
 *
 *
 *      REVISION HISTORY:
 *
 *      Nov 21, 1987 - Fixed man page to say "Fontedit" instead of "Top"
 *      Nov 22, 1987 - Added BSD Compatible ioctl, turned cursor on/off
 *                     - eap@bucsf.bu.edu
 */

void clear_screen();
#include <stdio.h>
#ifdef SYSV
#include <sys/termio.h>
#endif SYSV
#ifdef BSD
#include <sys/ioctl.h>
#endif BSD
#if defined (__NetBSD__) || defined (__FreeBSD__)
#include <sys/termios.h>
#include <sys/ioctl.h>
#endif /* __NetBSD__ || __FreeBSD__ */
#include <signal.h>

#ifdef CURFIX
#define CURSORON  "\033[?25h"
#define CURSOROFF "\033[?25l"
#endif CURFIX

#define	MAX_ROWS	10
#define	MAX_COLS	8

typedef enum { false, true } bool;

#define	KEY_FIND 	0x0100
#define	KEY_INSERT 	0x0101
#define	KEY_REMOVE 	0x0102
#define	KEY_SELECT 	0x0103
#define	KEY_PREV 	0x0104
#define	KEY_NEXT 	0x0105
#define	KEY_F6		0X0106
#define	KEY_F7		0x0107
#define	KEY_F8		0x0108
#define	KEY_F9		0x0109
#define	KEY_F10		0x010a
#define	KEY_F11		0x010b
#define	KEY_F12		0x010c
#define	KEY_F13		0x010d
#define	KEY_F14		0x010e
#define	KEY_HELP	0x010f
#define	KEY_DO		0x0110
#define	KEY_F17		0x0111
#define	KEY_F18		0x0112
#define	KEY_F19		0x0113
#define	KEY_F20		0x0114
#define	KEY_UP 		0x0115
#define	KEY_DOWN 	0x0116
#define	KEY_RIGHT 	0x0117
#define	KEY_LEFT 	0x0118

/*
 * Position of main drawing screen.
 */

#define	ROW_OFFSET	3
#define COL_OFFSET	10

/*
 * Position of the DRCS table.
 */

#define	TABLE_ROW	4
#define	TABLE_COL	50

/*
 *
 */

#define	ERROR_ROW	20
#define ERROR_COL	40

bool	display_table[MAX_ROWS][MAX_COLS];

#define	TOTAL_ENTRIES	(128 - 32)
#define	SIXELS_PER_CHAR	16

char	font_table[TOTAL_ENTRIES][SIXELS_PER_CHAR];
unsigned int	current_entry;

#ifdef SYSV
struct termio old_stty, new_stty;
#endif SYSV
#ifdef BSD
struct sgttyb old_stty, new_stty;
#endif BSD
#if defined (__NetBSD__) || defined (__FreeBSD__)
struct termios old_stty, new_stty;
#endif /* __NetBSD__ || __FreeBSD__ */
FILE * font_file = (FILE *)0;


/*
 * Interrupt
 *	Exit gracefully.
 */

interrupt()
{
	void clear_screen();
#ifdef CURFIX
        printf("%s\n",CURSORON);
#endif CURFIX
#ifdef SYSV
	ioctl( 0, TCSETA, &old_stty );
#endif SYSV
#ifdef BSD
        ioctl( 0, TIOCSETP, &old_stty );
#endif BSD
#if defined (__NetBSD__) || defined (__FreeBSD__)
        ioctl( 0, TIOCSETA, &old_stty );
#endif /* __NetBSD__ || __FreeBSD__ */
	clear_screen();
	exit( 0 );
}


/*
 * Main
 *	Grab input/output file and call main command processor.
 */

main( argc, argv )
int argc;
char *argv[];
{
	void command(), init_restore(), clear_screen();
	void save_table(), get_table(), extract_entry();

	if ( argc != 2 ) {
		fprintf( stderr, "usage: fontedit filename\n" );
		exit( 1 );
	}

	printf( "Press HELP for help\n" );
	printf( "\033P1;1;2{ @\033\\" );	/* Clear font buffer	*/
	fflush( stdout );
	sleep( 1 );			/* Let terminal catch up	*/
					/* otherwise we get frogs	*/

	if ( ( font_file = fopen( argv[1], "r" ) ) == (FILE *)0 ) {
		if ( ( font_file = fopen( argv[1], "w" ) ) == (FILE *)0 ) {
			fprintf( stderr, "Cannot create file %s \n", argv[1] );
			exit( 1 );
		}
	}
	fclose( font_file );

	if ( ( font_file = fopen( argv[1], "r" ) ) != (FILE *)0 ) {
		get_table( font_file );
		fclose( font_file );
	}

	if ( ( font_file = fopen( argv[1], "r+" ) ) == (FILE *)0 ) {
		fprintf( stderr, "Cannot open %s for writing\n", argv[1] );
		exit( 1 );
	}
#ifdef CURFIX
        printf("%s\n",CURSOROFF);
#endif CURFIX
#ifdef SYSV
	ioctl( 0, TCGETA, &old_stty );
#endif SYSV
#ifdef BSD
        ioctl( 0, TIOCGETP, &old_stty );
#endif BSD
#if defined (__NetBSD__) || defined (__FreeBSD__)
        ioctl( 0, TIOCGETA, &old_stty );
#endif /* __NetBSD__ || __FreeBSD__ */
	signal( SIGINT, (void *) interrupt );
	new_stty = old_stty;
#ifdef SYSV
	new_stty.c_lflag &= ~ICANON;
	new_stty.c_cc[VMIN] = 1;
	ioctl( 0, TCSETA, &new_stty );
#endif SYSV
#if defined (__NetBSD__) || defined (__FreeBSD__)
	new_stty.c_lflag &= ~ICANON;
        new_stty.c_lflag &= ~ECHO;
	new_stty.c_cc[VMIN] = 1;
	ioctl( 0, TIOCSETA, &new_stty );
#endif /* __NetBSD__ || __FreeBSD__ */
#ifdef BSD
	new_stty.sg_flags |= CBREAK;
        new_stty.sg_flags &= ~ECHO;
	ioctl( 0, TIOCSETP, &new_stty );
#endif BSD
	current_entry = 1;
	extract_entry( current_entry );
	init_restore();
	command();
#ifdef SYSV
	ioctl( 0, TCSETA, &old_stty );
#endif SYSV
#ifdef BSD
	ioctl( 0, TIOCSETP, &old_stty );
#endif BSD
#if defined (__NetBSD__) || defined (__FreeBSD__)
	ioctl( 0, TIOCSETA, &old_stty );
#endif /* __NetBSD__ || __FreeBSD__ */
	clear_screen();

	/* Overwrite the old file. */

	fseek( font_file, 0L, 0 );
	save_table( font_file );
	fclose( font_file );
#ifdef CURFIX
        printf("%s\n",CURSORON);
#endif CURFIX
}



/*
 * Command
 *	Process a function key.
 *
 *	The user cannot fill in slots 0 or 95 (space and del respecitively).
 */

void
command()
{
	register int c;
	register int row, col;
	register int i, j;
	bool change, error, override;

	void build_entry(), extract_entry(), send_entry(), print_entry();
	void highlight(), draw_current(), init_restore(), help();
	void warning();

	change = false;
	error = false;
	override = false;
	row = 0; col = 0;
	highlight( row, col, true );

	for ( ;; ) {
		c = get_key();
		highlight( row, col, false );	/* turn cursor off	*/

		if ( error ) {
			move ( ERROR_ROW, ERROR_COL );
			printf( "\033[K" );		/* Clear error message	*/
			move ( ERROR_ROW+1, ERROR_COL );
			printf( "\033[K" );		/* Clear error message	*/
			error = false;
		} else {
			override = false;
		}

		switch ( c ) {

		case KEY_FIND:		/* update DRCS 	*/
			if ( !change && !override ) {
				warning( "No changes to save" );
				override = true;
				error = true;
			} else {
				build_entry( current_entry );
				send_entry( current_entry );
				print_entry( current_entry, true );
				change = false;
			}
			break;

		case KEY_F6:		/* Turn on pixel	*/
			change = true;
			display_table[row][col] = true;
			highlight( row, col, false );
			col = ( col + 1 ) % MAX_COLS;
			if ( col == 0 )
				row = ( row + 1 ) % MAX_ROWS;
			break;

		case KEY_F7:		/* Turn off pixel	*/
			change = true;
			display_table[row][col] = false;
			highlight( row, col, false );
			col = ( col + 1 ) % MAX_COLS;
			if ( col == 0 )
				row = ( row + 1 ) % MAX_ROWS;
			break;

		case KEY_INSERT:	/* Insert a blank row	*/
			change = true;
			for ( j = 0; j < MAX_COLS; ++j ) {
				for ( i = MAX_ROWS - 1; i > row; --i ) {
					display_table[i][j] = display_table[i-1][j];
				}
				display_table[row][j] = false;
			}
			draw_current();
			break;

		case KEY_REMOVE:	/* Remove a row	*/
			change = true;
			for ( j = 0; j < MAX_COLS; ++j ) {
				for ( i = row; i < MAX_ROWS - 1; ++i ) {
					display_table[i][j] = display_table[i+1][j];
				}
				display_table[MAX_ROWS-1][j] = false;
			}
			draw_current();
			break;

		case KEY_F13:		/* Clear buffer	*/
			if ( change && !override ) {
				warning( "Changes not saved" );
				error = true;
				override = true;
			} else {
				for ( j = 0; j < MAX_COLS; ++j ) {
					for ( i = 0; i < MAX_ROWS; ++i ) {
						display_table[i][j] = false;
					}
				}
				draw_current();
			}
			break;

		case KEY_SELECT:	/* Select font from DRCS	*/
			if ( change && !override ) {
				warning( "Changes not saved" );
				error = true;
				override = true;
			} else {
				extract_entry( current_entry );
				draw_current();
			}
			break;

		case KEY_PREV:		/* Move to prev entry in DRCS	*/
			if ( change && !override ) {
				warning( "Changes not saved" );
				override = true;
				error = true;
			} else {
				print_entry( current_entry, false );
				current_entry = current_entry - 1;
				if ( current_entry == 0 )
					current_entry = TOTAL_ENTRIES - 2;
				print_entry( current_entry, true );
			}
			break;

		case KEY_NEXT:		/* Move to next entry in DRCS	*/
			if ( change && !override ) {
				warning( "Changes not saved" );
				override = true;
				error = true;
			} else {
				print_entry( current_entry, false );
				current_entry = current_entry + 1;
				if ( current_entry  == TOTAL_ENTRIES - 1 )
					current_entry = 1;
				print_entry( current_entry, true );
			}
			break;

		case KEY_UP:		/* UP one row.			*/
			if ( row == 0 )
				row = MAX_ROWS;
			row = row - 1;
			break;

		case KEY_DOWN:		/* Guess.			*/
			row = ( row + 1 ) % MAX_ROWS;
			break;

		case KEY_RIGHT:
			col = ( col + 1 ) % MAX_COLS;
			break;

		case KEY_LEFT:
			if ( col == 0 )
				col = MAX_COLS;
			col = col - 1;
			break;

		case KEY_HELP:		/* Display helpful info		*/
			clear_screen();
			help();
			c = getchar();
			init_restore();
			break;

		case '\004':		/* All done!			*/
			return;

		case '\f':		/* Redraw display		*/
			init_restore();
			break;

		default:		/* user is a klutzy  typist	*/
			move ( ERROR_ROW, ERROR_COL );
			printf( "Unknown key: " );
			if ( c < 0x20 ) {
				printf( "^%c", c );
			} else if ( c < 0x0100 ) {
				printf( "%c", c );
			} else {
				printf( "0x%04x", c );
			}
			fflush( stdout );
			error = true;
		}

		highlight( row, col, true );	/* turn cursor on	*/
	}
}



char *key_table[]	= {
	"\033[1~",		/* Find		*/
	"\033[2~",		/* Insert	*/
	"\033[3~",		/* Remove	*/
	"\033[4~",		/* Select	*/
	"\033[5~",		/* Prev		*/
	"\033[6~",		/* Next		*/
	"\033[17~",
	"\033[18~",
	"\033[19~",
	"\033[20~",
	"\033[21~",
	"\033[23~",
	"\033[24~",
	"\033[25~",
	"\033[26~",
	"\033[28~",
	"\033[29~",
	"\033[31~",
	"\033[32~",
	"\033[33~",
	"\033[34~",
	"\033[A",
	"\033[B",
	"\033[C",
	"\033[D",
	(char *)0 };

/*
 * get_key
 *	Convert VT220 escape sequence into something more reasonable.
 */

int
get_key()
{
	register char	*p;
	char	s[10];
	register int i, j;

	p = s;
	for ( i = 0; i < 10; ++i ) {
		*p = getchar();
		if ( i == 0 && *p != '\033' )
			return( (int)*p );	/* Not an escape sequence */
		if ( *p != '\033' && *p < 0x0020 )
			return( (int)*p );	/* Control character	*/
		*++p = '\0';			/* Null terminate	*/
		for ( j = 0; key_table[j]; ++j ) {
			if ( strcmp( s, key_table[j] ) == 0 ) {
				return( j | 0x0100 );
			    }
		}
	}
	return( -1 );
}



/*
 * pad
 *	Emit nulls so that the terminal can catch up.
 */

pad()
{
	int i;

	for ( i = 0; i < 20; ++i )
		putchar( '\000' );
	fflush( stdout );
}



/*
 * init_restore
 *	refresh the main display table.
 */

void
init_restore()
{
	register int row, col;
	register int i;

	void  draw_current(), clear_screen(), print_entry();

	clear_screen();

	for ( col = 0; col < MAX_COLS; ++col ) {
		move( ROW_OFFSET - 2, col * 3 + COL_OFFSET + 1 );
		printf( "%d", col );
	}
	move( ROW_OFFSET - 1, COL_OFFSET );
	printf( "+--+--+--+--+--+--+--+--+" );
	move( ROW_OFFSET + MAX_ROWS * 2, COL_OFFSET );
	printf( "+--+--+--+--+--+--+--+--+" );

	for ( row = 0; row < MAX_ROWS; ++row ) {
		if ( row != 0 && row != 7 )  {
			move( row * 2 + ROW_OFFSET, COL_OFFSET - 2 );
			printf( "%d|", row );
			move( row * 2 + ROW_OFFSET + 1, COL_OFFSET - 1 );
			printf( "|" );
			move( row * 2 + ROW_OFFSET, COL_OFFSET + MAX_COLS * 3 );
			printf( "|" );
			move( row * 2 + ROW_OFFSET + 1, COL_OFFSET + MAX_COLS * 3 );
			printf( "|" );
		} else {
			move( row * 2 + ROW_OFFSET, COL_OFFSET - 2 );
			printf( "%d*", row );
			move( row * 2 + ROW_OFFSET + 1, COL_OFFSET - 1 );
			printf( "*" );
			move( row * 2 + ROW_OFFSET, COL_OFFSET + MAX_COLS * 3 );
			printf( "*" );
			move( row * 2 + ROW_OFFSET + 1, COL_OFFSET + MAX_COLS * 3 );
			printf( "*" );
		}
	}
	draw_current();

	move( TABLE_ROW - 1, TABLE_COL - 1 );
	printf( "+-+-+-+-+-+-+-+-+-+-+-+-+" );
	move( TABLE_ROW + 8 * 2 - 1, TABLE_COL - 1 );
	printf( "+-+-+-+-+-+-+-+-+-+-+-+-+" );
	for ( i = 0; i < 8; ++i ) {
		move ( TABLE_ROW + i * 2, TABLE_COL - 1 );
		printf( "|" );
		move ( TABLE_ROW + i * 2 + 1, TABLE_COL - 1 );
		printf( "+" );
		move ( TABLE_ROW + i * 2, TABLE_COL + 12 * 2 - 1);
		printf( "|" );
		move ( TABLE_ROW + i * 2 + 1, TABLE_COL +12 * 2 - 1);
		printf( "+" );
	}
	for ( i = 0; i < TOTAL_ENTRIES; ++i )
		print_entry( i, (i == current_entry) ? true : false );
}



/*
 * draw_current
 *	Draw the complete current entry.
 */

void
draw_current()
{
	register int row, col;

	printf( "\033)0" );		/* Special graphics in G1	*/
	printf( "\016" );		/* Lock in G1 (SO)		*/

	for ( row = 0; row < MAX_ROWS; ++row ) {
		for ( col = 0; col < MAX_COLS; ++col ) {
			if ( display_table[row][col] ) {
				move( row * 2 + ROW_OFFSET,     col * 3 + COL_OFFSET );
				printf( "\141\141\141" );
				move( row * 2 + ROW_OFFSET + 1, col * 3 + COL_OFFSET );
				printf( "\141\141\141" );
			} else {
				move( row * 2 + ROW_OFFSET,     col * 3 + COL_OFFSET );
				printf( "   " ); 	/* erase splat	*/
				move( row * 2 + ROW_OFFSET + 1, col * 3 + COL_OFFSET );
				printf( "   " ); 	/* erase splat	*/
			}
		}
		pad();
	}
	printf( "\017" );		/* Lock in G0 (SI)	*/
	fflush( stdout );
}



/*
 * highlight
 *	Draw the cursor in the main display area.
 */

void
highlight( row, col, on )
unsigned int row, col;
bool on;
{

	printf( "\033)0" );		/* Special graphics in G1	*/
	printf( "\016" );		/* Lock in G1 (SO)		*/
	if ( on ) {
		printf( "\033[7m" );	/* Reverse video cursor		*/
	}

	if ( display_table[row][col] ) {
		move( row * 2 + ROW_OFFSET,     col * 3 + COL_OFFSET );
		printf( "\141\141\141" );
		move( row * 2 + ROW_OFFSET + 1, col * 3 + COL_OFFSET );
		printf( "\141\141\141" );
	} else {
		move( row * 2 + ROW_OFFSET,     col * 3 + COL_OFFSET );
		printf( "   " ); 	/* erase splat	*/
		move( row * 2 + ROW_OFFSET + 1, col * 3 + COL_OFFSET );
		printf( "   " ); 	/* erase splat	*/
	}
	pad();
	printf( "\017" );		/* Lock in G0 (SI)	*/
	printf( "\033[0m" );		/* normal video		*/
	printf( "\b" );			/* Back up one spot	*/
	fflush( stdout );
}



/*
 * Clear_screen
 */

void
clear_screen()
{
	printf( "\033[H\033[J" );		/* Clear screen.	*/
	fflush( stdout );
}



/*
 * move
 */

move( y, x )
int y, x;
{
	printf( "\033[%d;%df", y, x );
}



/*
 * Build_entry
 *	Convert the bit pattern used in the main display area into something
 *	that the vt220 can digest - namely sixels...
 */

void
build_entry( entry_no )
unsigned int entry_no;
{
	register int row, col;
	register unsigned int mask;

	for ( col = 0; col < 8; ++col ) {

		/* Top set of sixels	*/

		mask = 0;
		for ( row = 5; row >= 0; --row ) {
			mask = mask << 1;
			if ( display_table[row][col] )
				mask |= 1;
		}
		font_table[entry_no][col] = mask + 077;

		/*  Bottom set of sixels	*/

		mask = 0;
		for ( row = 9; row >= 6; --row ) {
			mask = mask << 1;
			if ( display_table[row][col] )
				mask |= 1;
		}
		font_table[entry_no][col+8] = mask + 077;
	}

}



/*
 * Extract_engry
 *	convert sixel representation into an array of bits.
 */

void
extract_entry( entry_no )
unsigned int entry_no;
{
	register int row, col;
	register unsigned int mask;

	for ( col = 0; col < 8; ++col ) {

		/* Top set of sixels	*/

		mask = font_table[entry_no][col];
		if ( mask >= 077 )
			mask -= 077;
		else
			mask = 0;		/* Bogus entry	*/

		for ( row = 0; row <= 5; ++row ) {
			display_table[row][col] = (bool)(mask & 0x0001);
			mask = mask >> 1;
		}

		/*  Bottom set of sixels	*/

		mask = font_table[entry_no][col+8];
		if ( mask >= 077 )
			mask -= 077;
		else
			mask = 0;

		for ( row = 6; row <= 9; ++row ) {
			display_table[row][col] = (bool)(mask & 0x0001);
			mask = mask >> 1;
		}
	}

}



/*
 * Send_entry
 *	Emit the stuff used by the VT220 to load a character into the
 *	DRCS.  We could, of course, send more than one entry at a time...
 */

void
send_entry( entry_no )
int entry_no;
{
	register char *fp	= font_table[entry_no];

	printf( "\033P1;%d;1;0;0;0{ @%c%c%c%c%c%c%c%c/%c%c%c%c%c%c%c%c\033\\",
		entry_no,
		fp[ 0], fp[ 1], fp[ 2], fp[ 3], fp[ 4], fp[ 5], fp[ 6], fp[ 7],
		fp[ 8], fp[ 9], fp[10], fp[11], fp[12], fp[13], fp[14], fp[15] );
}



/*
 * Print_entry
 *	The terminal normally has G0 in GL.  We don't want to change
 *	this, nor do we want to use GR.  Sooooo send out the necessary
 *	magic for shifting in G2 temporarily for the character that we
 *	want to display.
 */

void
print_entry( entry_no, highlight )
register unsigned int entry_no;
bool highlight;
{

	register int y, x;

	y = entry_no & 0x07;
	x = entry_no >> 3 & 0x1f;
	entry_no += 32;			/* Map up to G set	*/

	move( y * 2 + TABLE_ROW, x * 2 + TABLE_COL );

	if ( highlight )
		printf( "\033[7m" );

	printf( "\033* @" );		/* select DRCS into G2	*/
	printf( "\033N" );		/* select single shift	*/
	printf( "%c", entry_no );	/* Draw the character	*/

	if ( highlight )
		printf( "\033[0m" );
}



/*
 * Save_table
 *	Save a font table
 */

void
save_table( font_file )
FILE *font_file;
{
	register char *fp;
	register int i;

	for ( i = 0; i < TOTAL_ENTRIES; ++i ) {
		fp = font_table[i];
		fprintf( font_file, "\033P1;%d;1;0;0;0{ @%c%c%c%c%c%c%c%c/%c%c%c%c%c%c%c%c\033\\\n",
			i,
			fp[ 0], fp[ 1], fp[ 2], fp[ 3], fp[ 4], fp[ 5], fp[ 6], fp[ 7],
			fp[ 8], fp[ 9], fp[10], fp[11], fp[12], fp[13], fp[14], fp[15] );
	}
}



/*
 * Get_table
 *	Extract font table entries from a file
 */

void
get_table( font_file )
FILE *font_file;
{
	char	s[256];
	register char	*p;
	char	*fp;
	int i;
	register int j;

	while( fgets( s, 255, font_file ) ) {
		if ( strncmp( s, "\033P1;", 4 ) !=  0 )
			continue;	/* Bogus line	*/
		p = &s[4];
		if ( sscanf( p, "%d", &i ) != 1 )
			continue;	/* Illegal entry number	*/

		if ( i <= 0 || TOTAL_ENTRIES <= i )
			continue;	/* Bogues entry	*/

		fp = font_table[i];

		while ( *p && *p != '@' )
			++p;		/* Skip to font definition */
		if ( ! *p++ )
			continue;	/* Skip @	*/

		for ( j = 0; *p && *p != '\033' && j < 16; ++j, ++p ) {
			if ( *p == '/' ) {
				j = 8;
				++p;
			}
			fp[j] = *p;
		}
		send_entry( i );
	}
}



/*
 * Help
 *	Print out help information.
 */

void
help()
{
	printf( "Font editor\n\n" );
	printf( "F6     - Pixel on\n" );
	printf( "F7     - Pixel off\n" );
	printf( "F13    - Clear display area\n" );
	printf( "HELP   - This screen\n" );
	printf( "FIND   - Update font table\n" );
	printf( "INSERT - Insert a blank row\n" );
	printf( "REMOVE - Remove a row\n" );
	printf( "SELECT - Select current font table entry\n" );
	printf( "PREV   - Move to previous font table entry\n" );
	printf( "NEXT   - Move to next font table entry\n" );
	printf( "^D     - Exit\n" );
	printf( "\n\n\n\nPress any key to continue\n" );
}



/*
 * Warning
 *	Issue a warning to the regarding the current status.
 */

void
warning( s )
char *s;
{
	move( ERROR_ROW, ERROR_COL );
	printf( "Warning: %s!\n", s );
	move( ERROR_ROW+1, ERROR_COL );
	printf( "         Reissue command to override\n" );
}
