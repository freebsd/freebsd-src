/*
 * Part one of the mined editor.
 */

/*
 * Ported to FreeBSD by Andrzej Bialecki <abial@freebsd.org>, Oct 1998
 *
 * Added a help screen, and remapped some of the wildest keybindings...
 */

/*
 * Author: Michiel Huisjes.
 * 
 * 1. General remarks.
 * 
 *   Mined is a screen editor designed for the MINIX operating system.
 *   It is meant to be used on files not larger than 50K and to be fast.
 *   When mined starts up, it reads the file into its memory to minimize
 *   disk access. The only time that disk access is needed is when certain
 *   save, write or copy commands are given.
 * 
 *   Mined has the style of Emacs or Jove, that means that there are no modes.
 *   Each character has its own entry in an 256 pointer to function array,
 *   which is called when that character is typed. Only ASCII characters are
 *   connected with a function that inserts that character at the current
 *   location in the file. Two execptions are <linefeed> and <tab> which are
 *   inserted as well. Note that the mapping between commands and functions
 *   called is implicit in the table. Changing the mapping just implies
 *   changing the pointers in this table.
 * 
 *   The display consists of SCREENMAX + 1 lines and XMAX + 1 characters. When
 *   a line is larger (or gets larger during editing) than XBREAK characters,
 *   the line is either shifted SHIFT_SIZE characters to the left (which means
 *   that the first SHIFT_SIZE characters are not printed) or the end of the
 *   line is marked with the SHIFT_MARK character and the rest of the line is
 *   not printed.  A line can never exceed MAX_CHARS characters. Mined will
 *   always try to keep the cursor on the same line and same (relative)
 *   x-coordinate if nothing changed. So if you scroll one line up, the cursor
 *   stays on the same line, or when you move one line down, the cursor will
 *   move to the same place on the line as it was on the previous.
 *   Every character on the line is available for editing including the
 *   linefeed at the the of the line. When the linefeed is deleted, the current
 *   line and the next line are joined. The last character of the file (which
 *   is always a linefeed) can never be deleted.
 *   The bottomline (as indicated by YMAX + 1) is used as a status line during
 *   editing. This line is usually blank or contains information mined needs
 *   during editing. This information (or rather questions) is displayed in
 *   reverse video.
 * 
 *   The terminal modes are changed completely. All signals like start/stop,
 *   interrupt etc. are unset. The only signal that remains is the quit signal.
 *   The quit signal (^\) is the general abort signal for mined. Typing a ^\
 *   during searching or when mined is asking for filenames, etc. will abort
 *   the function and mined will return to the main loop.  Sending a quit
 *   signal during the main loop will abort the session (after confirmation)
 *   and the file is not (!) saved.
 *   The session will also be aborted when an unrecoverable error occurs. E.g
 *   when there is no more memory available. If the file has been modified,
 *   mined will ask if the file has to be saved or not.
 *   If there is no more space left on the disk, mined will just give an error 
 *   message and continue.
 * 
 *   The number of system calls are minized. This is done to keep the editor
 *   as fast as possible. I/O is done in SCREEN_SIZE reads/writes. Accumulated
 *   output is also flushed at the end of each character typed.
 * 
 * 2. Regular expressions
 *   
 *   Mined has a build in regular expression matcher, which is used for
 *   searching and replace routines. A regular expression consists of a
 *   sequence of:
 * 
 *      1. A normal character matching that character.
 *      2. A . matching any character.
 *      3. A ^ matching the begin of a line.
 *      4. A $ (as last character of the pattern) mathing the end of a line.
 *      5. A \<character> matching <character>.
 *      6. A number of characters enclosed in [] pairs matching any of these
 *        characters. A list of characters can be indicated by a '-'. So
 *        [a-z] matches any letter of the alphabet. If the first character
 *        after the '[' is a '^' then the set is negated (matching none of
 *        the characters). 
 *        A ']', '^' or '-' can be escaped by putting a '\' in front of it.
 *        Of course this means that a \ must be represented by \\.
 *      7. If one of the expressions as described in 1-6 is followed by a
 *        '*' than that expressions matches a sequence of 0 or more of
 *        that expression.
 * 
 *   Parsing of regular expression is done in two phases. In the first phase
 *   the expression is compiled into a more comprehensible form. In the second
 *   phase the actual matching is done. For more details see 3.6.
 * 
 * 
 * 3. Implementation of mined.
 * 
 *   3.1 Data structures.
 * 
 *      The main data structures are as follows. The whole file is kept in a
 *      double linked list of lines. The LINE structure looks like this:
 * 
 *         typedef struct Line {
 *              struct Line *next;
 *              struct Line *prev;
 *              char *text;
 *              unsigned char shift_count;
 *         } LINE;
 * 
 *      Each line entry contains a pointer to the next line, a pointer to the
 *      previous line and a pointer to the text of that line. A special field
 *      shift_count contains the number of shifts (in units of SHIFT_SIZE)
 *      that is performed on that line. The total size of the structure is 7
 *      bytes so a file consisting of 1000 empty lines will waste a lot of
 *      memory. A LINE structure is allocated for each line in the file. After
 *      that the number of characters of the line is counted and sufficient
 *      space is allocated to store them (including a linefeed and a '\0').
 *      The resulting address is assigned to the text field in the structure.
 * 
 *      A special structure is allocated and its address is assigned to the
 *      variable header as well as the variable tail. The text field of this
 *      structure is set to NIL_PTR. The tail->prev of this structure points
 *      to the last LINE of the file and the header->next to the first LINE.
 *      Other LINE *variables are top_line and bot_line which point to the
 *      first line resp. the last line on the screen.
 *      Two other variables are important as well. First the LINE *cur_line,
 *      which points to the LINE currently in use and the char *cur_text,
 *      which points to the character at which the cursor stands.
 *      Whenever an ASCII character is typed, a new line is build with this
 *      character inserted. Then the old data space (pointed to by
 *      cur_line->text) is freed, data space for the new line is allocated and
 *      assigned to cur_line->text.
 * 
 *      Two global variables called x and y represent the x and y coordinates
 *      from the cursor. The global variable nlines contains the number of
 *      lines in the file. Last_y indicates the maximum y coordinate of the
 *      screen (which is usually SCREENMAX).
 * 
 *      A few strings must be initialized by hand before compiling mined.
 *      These string are enter_string, which is printed upon entering mined,
 *      rev_video (turn on reverse video), normal_video, rev_scroll (perform a
 *      reverse scroll) and pos_string. The last string should hold the
 *      absolute position string to be printed for cursor motion. The #define
 *      X_PLUS and Y_PLUS should contain the characters to be added to the
 *      coordinates x and y (both starting at 0) to finish cursor positioning.
 * 
 *   3.2 Starting up.
 *      
 *      Mined can be called with or without argument and the function
 *      load_file () is called with these arguments. load_file () checks
 *      if the file exists if it can be read and if it is writable and
 *      sets the writable flag accordingly. If the file can be read, 
 *      load_file () reads a line from the file and stores this line into
 *      a structure by calling install_line () and line_insert () which
 *      installs the line into the double linked list, until the end of the
 *      file is reached.
 *      Lines are read by the function get_line (), which buffers the
 *      reading in blocks of SCREEN_SIZE. Load_file () also initializes the
 *      LINE *variables described above.
 * 
 *   3.3 Moving around.
 * 
 *      Several commands are implemented for moving through the file.
 *      Moving up (UP), down (DN) left (LF) and right (RT) are done by the
 *      arrow keys. Moving one line below the screen scrolls the screen one
 *      line up. Moving one line above the screen scrolls the screen one line
 *      down. The functions forward_scroll () and reverse_scroll () take care
 *      of that.
 *      Several other move functions exist: move to begin of line (BL), end of
 *      line (EL) top of screen (HIGH), bottom of screen (LOW), top of file
 *      (HO), end of file (EF), scroll one page down (PD), scroll one page up
 *      (PU), scroll one line down (SD), scroll one line up (SU) and move to a
 *      certain line number (GOTO).
 *      Two functions called MN () and MP () each move one word further or 
 *      backwards. A word is a number of non-blanks seperated by a space, a
 *      tab or a linefeed.
 * 
 *   3.4 Modifying text.
 * 
 *      The modifying commands can be separated into two modes. The first
 *      being inserting text, and the other deleting text. Two functions are
 *      created for these purposes: insert () and delete (). Both are capable
 *      of deleting or inserting large amounts of text as well as one
 *      character. Insert () must be given the line and location at which
 *      the text must be inserted. Is doesn't make any difference whether this
 *      text contains linefeeds or not. Delete () must be given a pointer to
 *      the start line, a pointer from where deleting should start on that
 *      line and the same information about the end position. The last
 *      character of the file will never be deleted. Delete () will make the
 *      necessary changes to the screen after deleting, but insert () won't.
 *      The functions for modifying text are: insert one char (S), insert a
 *      file (file_insert (fd)), insert a linefeed and put cursor back to
 *      end of line (LIB), delete character under the cursor (DCC), delete
 *      before cursor (even linefeed) (DPC), delete next word (DNW), delete
 *      previous word (DPC) and delete to end of line (if the cursor is at
 *      a linefeed delete line) (DLN).
 * 
 *   3.5 Yanking.
 * 
 *      A few utilities are provided for yanking pieces of text. The function
 *      MA () marks the current position in the file. This is done by setting 
 *      LINE *mark_line and char *mark_text to the current position. Yanking
 *      of text can be done in two modes. The first mode just copies the text
 *      from the mark to the current position (or visa versa) into a buffer
 *      (YA) and the second also deletes the text (DT). Both functions call
 *      the function set_up () with the delete flag on or off. Set_up ()
 *      checks if the marked position is still a valid one (by using
 *      check_mark () and legal ()), and then calls the function yank () with
 *      a start and end position in the file. This function copies the text
 *      into a scratch_file as indicated by the variable yank_file. This
 *      scratch_file is made uniq by the function scratch_file (). At the end
 *      of copying yank will (if necessary) delete the text. A global flag
 *      called yank_status keeps track of the buffer (or file) status. It is
 *      initialized on NOT_VALID and set to EMPTY (by set_up ()) or VALID (by
 *      yank ()). Several things can be done with the buffer. It can be
 *      inserted somewhere else in the file (PT) or it can be copied into
 *      another file (WB), which will be prompted for.
 * 
 *   3.6 Search and replace routines.
 * 
 *      Searching for strings and replacing strings are done by regular
 *      expressions. For any expression the function compile () is called
 *      with as argument the expression to compile. Compile () returns a
 *      pointer to a structure which looks like this:
 * 
 *         typedef struct regex {
 *              union {
 *                    char *err_mess;
 *                    int *expression;
 *              } result;
 *              char status;
 *              char *start_ptr;
 *              char *end_ptr;
 *         } REGEX;
 *      
 *    If something went wrong during compiling (e.g. an illegal expression
 *    was given), the function reg_error () is called, which sets the status
 *    field to REG_ERROR and the err_mess field to the error message. If the
 *    match must be anchored at the beginning of the line (end of line), the
 *    status field is set to BEGIN_LINE (END_LINE). If none of these special
 *    cases are true, the field is set to 0 and the function finished () is
 *    called.  Finished () allocates space to hold the compiled expression
 *    and copies this expression into the expression field of the union
 *    (bcopy ()). Matching is done by the routines match() and line_check().
 *    Match () takes as argument the REGEX *program, a pointer to the
 *    startposition on the current line, and a flag indicating FORWARD or
 *    REVERSE search.  Match () checks out the whole file until a match is
 *    found. If match is found it returns a pointer to the line in which the
 *    match was found else it returns a NIL_LINE. Line_check () takes the
 *    same arguments, but return either MATCH or NO_MATCH.
 *    During checking, the start_ptr and end_ptr fields of the REGEX
 *    structure are assigned to the start and end of the match. 
 *    Both functions try to find a match by walking through the line
 *    character by character. For each possibility, the function
 *    check_string () is called with as arguments the REGEX *program and the
 *    string to search in. It starts walking through the expression until
 *    the end of the expression or the end of the string is reached.
 *    Whenever a * is encountered, this position of the string is marked,
 *    the maximum number of matches are performed and the function star ()
 *    is called in order to try to find the longest match possible. Star ()
 *    takes as arguments the REGEX program, the current position of the
 *    string, the marked position and the current position of the expression
 *    Star () walks from the current position of the string back to the
 *    marked position, and calls string_check () in order to find a match.
 *    It returns MATCH or NO_MATCH, just as string_check () does.
 *    Searching is now easy. Both search routines (forward (SF) and
 *    backwards search (SR)) call search () with an apropiate message and a
 *    flag indicating FORWARD or REVERSE search. Search () will get an
 *    expression from the user by calling get_expression(). Get_expression()
 *    returns a pointer to a REGEX structure or NIL_REG upon errors and
 *    prompts for the expression. If no expression if given, the previous is
 *    used instead. After that search will call match (), and if a match is
 *    found, we can move to that place in the file by the functions find_x()
 *    and find_y () which will find display the match on the screen.
 *    Replacing can be done in two ways. A global replace (GR) or a line
 *    replace (LR). Both functions call change () with a message an a flag
 *    indicating global or line replacement. Change () will prompt for the
 *    expression and for the replacement. Every & in the replacement pattern
 *    means substitute the match instead. An & can be escaped by a \. When
 *    a match is found, the function substitute () will perform the
 *    substitution.
 * 
 *  3.6 Miscellaneous commands.
 * 
 *    A few commands haven't be discussed yet. These are redraw the screen
 *    (RD) fork a shell (SH), print file status (FS), write file to disc
 *    (WT), insert a file at current position (IF), leave editor (XT) and
 *    visit another file (VI). The last two functions will check if the file
 *    has been modified. If it has, they will ask if you want to save the
 *    file by calling ask_save ().
 *    The function ESC () will repeat a command n times. It will prompt for
 *    the number. Aborting the loop can be done by sending the ^\ signal.
 * 
 *  3.7 Utility functions.
 * 
 *    Several functions exists for internal use. First allocation routines:
 *    alloc (bytes) and newline () will return a pointer to free data space
 *    if the given size. If there is no more memory available, the function
 *    panic () is called.
 *    Signal handling: The only signal that can be send to mined is the 
 *    SIGQUIT signal. This signal, functions as a general abort command.
 *    Mined will abort if the signal is given during the main loop. The 
 *    function abort_mined () takes care of that.
 *    Panic () is a function with as argument a error message. It will print
 *    the message and the error number set by the kernel (errno) and will
 *    ask if the file must be saved or not. It resets the terminal
 *    (raw_mode ()) and exits.
 *    String handling routines like copy_string(to, from), length_of(string)
 *    and build_string (buffer, format, arg1, arg2, ...). The latter takes
 *    a description of the string out out the format field and puts the
 *    result in the buffer. (It works like printf (3), but then into a
 *    string). The functions status_line (string1, string2), error (string1,
 *    string2), clear_status () and bottom_line () all print information on
 *    the status line.
 *    Get_string (message, buffer) reads a string and getchar () reads one
 *    character from the terminal.
 *    Num_out ((long) number) prints the number into a 11 digit field
 *    without leading zero's. It returns a pointer to the resulting string.
 *    File_status () prints all file information on the status line.
 *    Set_cursor (x, y) prints the string to put the cursor at coordinates
 *    x and y.
 *    Output is done by four functions: writeline(fd,string), clear_buffer()
 *    write_char (fd, c) and flush_buffer (fd). Three defines are provided
 *    to write on filedescriptor STD_OUT (terminal) which is used normally:
 *    string_print (string), putchar (c) and flush (). All these functions
 *    use the global I/O buffer screen and the global index for this array
 *    called out_count. In this way I/O can be buffered, so that reads or
 *    writes can be done in blocks of SCREEN_SIZE size.
 *    The following functions all handle internal line maintenance. The
 *    function proceed (start_line, count) returns the count'th line after
 *    start_line.  If count is negative, the count'th line before the
 *    start_line is returned. If header or tail is encountered then that
 *    will be returned. Display (x, y, start_line, count) displays count
 *    lines starting at coordinates [x, y] and beginning at start_line. If
 *    the header or tail is encountered, empty lines are displayed instead.
 *    The function reset (head_line, ny) reset top_line, last_y, bot_line,
 *    cur_line and y-coordinate. This is not a neat way to do the
 *    maintenance, but it sure saves a lot of code. It is usually used in
 *    combination with display ().
 *    Put_line(line, offset, clear_line), prints a line (skipping characters
 *    according to the line->shift_size field) until XBREAK - offset
 *    characters are printed or a '\n' is encountered. If clear_line is
 *	  TRUE, spaces are printed until XBREAK - offset characters.
 *	  Line_print (line) is a #define from put_line (line, 0, TRUE).
 *    Moving is done by the functions move_to (x, y), move_addres (address)
 *    and move (x, adress, y). This function is the most important one in
 *    mined. New_y must be between 0 and last_y, new_x can be about
 *    anything, address must be a pointer to an character on the current
 *    line (or y). Move_to () first adjust the y coordinate together with
 *    cur_line. If an address is given, it finds the corresponding
 *    x-coordinate. If an new x-coordinate was given, it will try to locate
 *    the corresponding character. After that it sets the shift_count field
 *    of cur_line to an apropiate number according to new_x. The only thing
 *    left to do now is to assign the new values to cur_line, cur_text, x
 *    and y.
 * 
 * 4. Summary of commands.
 *  
 *  CURSOR MOTION
 *    up-arrow  Move cursor 1 line up.  At top of screen, reverse scroll
 *    down-arrow  Move cursor 1 line down.  At bottom, scroll forward.
 *    left-arrow  Move cursor 1 character left or to end of previous line
 *    right-arrow Move cursor 1 character right or to start of next line
 *    CTRL-A   Move cursor to start of current line
 *    CTRL-Z   Move cursor to end of current line
 *    CTRL-^   Move cursor to top of screen
 *    CTRL-_   Move cursor to bottom of screen
 *    CTRL-F   Forward to start of next word (even to next line)
 *    CTRL-B   Backward to first character of previous word
 *   
 *  SCREEN MOTION
 *    Home key  Move cursor to first character of file
 *    End key   Move cursor to last character of file
 *    PgUp    Scroll backward 1 page. Bottom line becomes top line
 *    PgD    Scroll backward 1 page. Top line becomes bottom line
 *    CTRL-D   Scroll screen down one line (reverse scroll)
 *    CTRL-U   Scroll screen up one line (forward scroll)
 *   
 *  MODIFYING TEXT
 *    ASCII char  Self insert character at cursor
 *    tab    Insert tab at cursor
 *    backspace  Delete the previous char (left of cursor), even line feed
 *    Del    Delete the character under the cursor
 *    CTRL-N   Delete next word
 *    CTRL-P   Delete previous word
 *    CTRL-O   Insert line feed at cursor and back up 1 character
 *    CTRL-T   Delete tail of line (cursor to end); if empty, delete line
 *    CTRL-@   Set the mark (remember the current location)
 *    CTRL-K   Delete text from the mark to current position save on file
 *    CTRL-C   Save the text from the mark to the current position
 *    CTRL-Y   Insert the contents of the save file at current position
 *    CTRL-Q   Insert the contents of the save file into a new file
 *    CTRL-G   Insert a file at the current position
 *   
 *  MISCELLANEOUS
 *    CTRL-E   Erase and redraw the screen
 *    CTRL-V   Visit file (read a new file); complain if old one changed
 *    CTRL-W   Write the current file back to the disk
 *    numeric +  Search forward (prompt for regular expression)
 *    numeric -  Search backward (prompt for regular expression)
 *    numeric 5  Print the current status of the file
 *    CTRL-R   (Global) Replace str1 by str2 (prompts for each string)
 *    CTRL-L   (Line) Replace string1 by string2
 *    CTRL-S   Fork off a shell and wait for it to finish
 *    CTRL-X   EXIT (prompt if file modified)
 *    CTRL-]   Go to a line. Prompts for linenumber
 *    CTRL-\   Abort whatever editor was doing and start again
 *    escape key  Repeat a command count times; (prompts for count)
 */

/*  ========================================================================  *
 *				Utilities				      *	
 *  ========================================================================  */

#include "mined.h"
#include <signal.h>
#include <termios.h>
#include <limits.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

extern int errno;
int ymax = YMAX;
int screenmax = SCREENMAX;


/*
 * Print file status.
 */
void FS()
{
  fstatus(file_name[0] ? "" : "[buffer]", -1L);
}

/*
 * Visit (edit) another file. If the file has been modified, ask the user if
 * he wants to save it.
 */
void VI()
{
  char new_file[LINE_LEN];	/* Buffer to hold new file name */

  if (modified == TRUE && ask_save() == ERRORS)
  	return;
  
/* Get new file name */
  if (get_file("Visit file:", new_file) == ERRORS)
  	return;

/* Free old linked list, initialize global variables and load new file */
  initialize();
#ifdef UNIX
  tputs(CL, 0, _putchar);
#else
  string_print (enter_string);
#endif /* UNIX */
  load_file(new_file[0] == '\0' ? NIL_PTR : new_file);
}

/*
 * Write file in core to disc.
 */
int WT()
{
  register LINE *line;
  register long count = 0L;	/* Nr of chars written */
  char file[LINE_LEN];		/* Buffer for new file name */
  int fd;				/* Filedescriptor of file */

  if (modified == FALSE) {
	error ("Write not necessary.", NIL_PTR);
	return FINE;
  }

/* Check if file_name is valid and if file can be written */
  if (file_name[0] == '\0' || writable == FALSE) {
  	if (get_file("Enter file name:", file) != FINE)
  		return ERRORS;
  	copy_string(file_name, file);		/* Save file name */
  }
  if ((fd = creat(file_name, 0644)) < 0) {	/* Empty file */
  	error("Cannot create ", file_name);
  	writable = FALSE;
  	return ERRORS;
  }
  else
  	writable = TRUE;

  clear_buffer();

  status_line("Writing ", file_name);
  for (line = header->next; line != tail; line = line->next) {
	if (line->shift_count & DUMMY) {
		if (line->next == tail && line->text[0] == '\n')
			continue;
	}
  	if (writeline(fd, line->text) == ERRORS) {
  		count = -1L;
  		break;
  	}
  	count += (long) length_of(line->text);
  }

  if (count > 0L && flush_buffer(fd) == ERRORS)
  	count = -1L;

  (void) close(fd);

  if (count == -1L)
  	return ERRORS;

  modified = FALSE;
  rpipe = FALSE;		/* File name is now assigned */

/* Display how many chars (and lines) were written */
  fstatus("Wrote", count);
  return FINE;
}

/* Call WT and discard value returned. */
void XWT()
{
  (void) WT();
}



/*
 * Call an interactive shell.
 */
void SH()
{
  register int w;
  int pid, status;
  char *shell;

  if ((shell = getenv("SHELL")) == NIL_PTR) shell = "/bin/sh";

  switch (pid = fork()) {
  	case -1:			/* Error */
  		error("Cannot fork.", NIL_PTR);
  		return;
  	case 0:				/* This is the child */
  		set_cursor(0, ymax);
  		putchar('\n');
  		flush();
  		raw_mode(OFF);
		if (rpipe) {			/* Fix stdin */
			close (0);
			if (open("/dev/tty", 0) < 0)
				exit (126);
		}
  		execl(shell, shell, (char *) 0);
  		exit(127);			/* Exit with 127 */
  	default :				/* This is the parent */
  		signal(SIGINT, SIG_IGN);
  		signal(SIGQUIT, SIG_IGN);
  		do {
  			w = wait(&status);
  		} while (w != -1 && w != pid);
  }

  raw_mode(ON);
  RD();

  if ((status >> 8) == 127)		/* Child died with 127 */
  	error("Cannot exec ", shell);
  else if ((status >> 8) == 126)
  	error("Cannot open /dev/tty as fd #0", NIL_PTR);
}

/*
 * Proceed returns the count'th line after `line'. When count is negative
 * it returns the count'th line before `line'. When the next (previous)
 * line is the tail (header) indicating EOF (tof) it stops.
 */
LINE *proceed(line, count)
register LINE *line;
register int count;
{
  if (count < 0)
  	while (count++ < 0 && line != header)
  		line = line->prev;
  else
  	while (count-- > 0 && line != tail)
  		line = line->next;
  return line;
}

/*
 * Show concatenation of s1 and s2 on the status line (bottom of screen)
 * If revfl is TRUE, turn on reverse video on both strings. Set stat_visible
 * only if bottom_line is visible.
 */
int bottom_line(revfl, s1, s2, inbuf, statfl)
FLAG revfl;
char *s1, *s2;
char *inbuf;
FLAG statfl;
{
  int ret = FINE;
  char buf[LINE_LEN];
  register char *p = buf;

  *p++ = ' ';
  if (s1 != NIL_PTR)
	while (*p = *s1++)
		p++;
  if (s2 != NIL_PTR)
	while (*p = *s2++)
		p++;
  *p++ = ' ';
  *p++ = 0;

  if (revfl == ON && stat_visible == TRUE)
	clear_status ();
  set_cursor(0, ymax);
  if (revfl == ON) {		/* Print rev. start sequence */
#ifdef UNIX
  	tputs(SO, 0, _putchar);
#else
  	string_print(rev_video);
#endif /* UNIX */
  	stat_visible = TRUE;
  }
  else				/* Used as clear_status() */
  	stat_visible = FALSE;

  string_print(buf);
  
  if (inbuf != NIL_PTR)
  	ret = input(inbuf, statfl);

  /* Print normal video */
#ifdef UNIX
  tputs(SE, 0, _putchar);
  tputs(CE, 0, _putchar);
#else
  string_print(normal_video);
  string_print(blank_line);	/* Clear the rest of the line */
#endif /* UNIX */
  if (inbuf != NIL_PTR)
  	set_cursor(0, ymax);
  else
  	set_cursor(x, y);	/* Set cursor back to old position */
  flush();			/* Perform the actual write */
  if (ret != FINE)
  	clear_status();
  return ret;
}

/*
 * Count_chars() count the number of chars that the line would occupy on the
 * screen. Counting starts at the real x-coordinate of the line.
 */
int count_chars(line)
LINE *line;
{
  register int cnt = get_shift(line->shift_count) * -SHIFT_SIZE;
  register char *textp = line->text;

/* Find begin of line on screen */
  while (cnt < 0) {
  	if (is_tab(*textp++))
  		cnt = tab(cnt);
  	else
  		cnt++;
  }

/* Count number of chars left */
  cnt = 0;
  while (*textp != '\n') {
  	if (is_tab(*textp++))
  		 cnt = tab(cnt);
  	else
  		cnt++;
  }
  return cnt;
}

/*
 * Move to coordinates nx, ny at screen.  The caller must check that scrolling
 * is not needed.
 * If new_x is lower than 0 or higher than XBREAK, move_to() will check if
 * the line can be shifted. If it can it sets(or resets) the shift_count field
 * of the current line accordingly.
 * Move also sets cur_text to the right char.
 * If we're moving to the same x coordinate, try to move the the x-coordinate
 * used on the other previous call.
 */
void move(new_x, new_address, new_y)
register int new_x;
int new_y;
char *new_address;
{
  register LINE *line = cur_line;	/* For building new cur_line */
  int shift = 0;			/* How many shifts to make */
  static int rel_x = 0;		/* Remember relative x position */
  int tx = x;

/* Check for illegal values */
  if (new_y < 0 || new_y > last_y)
  	return;

/* Adjust y-coordinate and cur_line */
  if (new_y < y)
  	while (y != new_y) {
		if(line->shift_count>0) {
			line->shift_count=0;
			move_to(0,y);
			string_print(blank_line);
			line_print(line);
		}
  		y--;
  		line = line->prev;
  	}
  else
  	while (y != new_y) {
		if(line->shift_count>0) {
			line->shift_count=0;
			move_to(0,y);
			string_print(blank_line);
			line_print(line);
		}
  		y++;
  		line = line->next;
  	}

/* Set or unset relative x-coordinate */
  if (new_address == NIL_PTR) {
  	new_address = find_address(line, (new_x == x) ? rel_x : new_x , &tx);
	if (new_x != x)
		rel_x = tx;
  	new_x = tx;
  }
  else {
  	rel_x = new_x = find_x(line, new_address);
  }

/* Adjust shift_count if new_x lower than 0 or higher than XBREAK */
  if (new_x < 0 || new_x >= XBREAK) {
  	if (new_x > XBREAK || (new_x == XBREAK && *new_address != '\n'))
  		shift = (new_x - XBREAK) / SHIFT_SIZE + 1;
  	else {
  		shift = new_x / SHIFT_SIZE;
		if (new_x % SHIFT_SIZE)
			shift--;
  	}

  	if (shift != 0) {
  		line->shift_count += shift;
  		new_x = find_x(line, new_address);
  		set_cursor(0, y);
  		line_print(line);
  		rel_x = new_x;
  	}
  }

/* Assign and position cursor */
  x = new_x;
  cur_text = new_address;
  cur_line = line;
  set_cursor(x, y);
}

/*
 * Find_x() returns the x coordinate belonging to address.
 * (Tabs are expanded).
 */
int find_x(line, address)
LINE *line;
char *address;
{
  register char *textp = line->text;
  register int nx = get_shift(line->shift_count) * -SHIFT_SIZE;

  while (textp != address && *textp != '\0') {
  	if (is_tab(*textp++)) 	/* Expand tabs */
  		nx = tab(nx);
  	else
  		nx++;
  }
  return nx;
}

/*
 * Find_address() returns the pointer in the line with offset x_coord.
 * (Tabs are expanded).
 */
char *find_address(line, x_coord, old_x)
LINE *line;
int x_coord;
int *old_x;
{
  register char *textp = line->text;
  register int tx = get_shift(line->shift_count) * -SHIFT_SIZE;

  while (tx < x_coord && *textp != '\n') {
  	if (is_tab(*textp)) {
  		if (*old_x - x_coord == 1 && tab(tx) > x_coord)
  			break;		/* Moving left over tab */
  		else
  			tx = tab(tx);
  	}
  	else
  		tx++;
  	textp++;
  }
  
  *old_x = tx;
  return textp;
}

/*
 * Length_of() returns the number of characters int the string `string'
 * excluding the '\0'.
 */
int length_of(string)
register char *string;
{
  register int count = 0;

  if (string != NIL_PTR) {
  	while (*string++ != '\0')
  		count++;
  }
  return count;
}

/*
 * Copy_string() copies the string `from' into the string `to'. `To' must be
 * long enough to hold `from'.
 */
void copy_string(to, from)
register char *to;
register char *from;
{
  while (*to++ = *from++)
  	;
}

/*
 * Reset assigns bot_line, top_line and cur_line according to `head_line'
 * which must be the first line of the screen, and an y-coordinate,
 * which will be the current y-coordinate (if it isn't larger than last_y)
 */
void reset(head_line, screen_y)
LINE *head_line;
int screen_y;
{
  register LINE *line;

  top_line = line = head_line;

/* Search for bot_line (might be last line in file) */
  for (last_y = 0; last_y < nlines - 1 && last_y < screenmax
						&& line->next != tail; last_y++)
  	line = line->next;

  bot_line = line;
  y = (screen_y > last_y) ? last_y : screen_y;

/* Set cur_line according to the new y value */
  cur_line = proceed(top_line, y);
}

/*
 * Set cursor at coordinates x, y.
 */
void set_cursor(nx, ny)
int nx, ny;
{
#ifdef UNIX
  extern char *tgoto();

  tputs(tgoto(CM, nx, ny), 0, _putchar);
#else
  char text_buffer[10];

  build_string(text_buffer, pos_string, ny+1, nx+1);
  string_print(text_buffer);
#endif /* UNIX */
}

/*
 * Routine to open terminal when mined is used in a pipeline.
 */
void open_device()
{
  if ((input_fd = open("/dev/tty", 0)) < 0)
	panic("Cannot open /dev/tty for read");
}

/*
 * Getchar() reads one character from the terminal. The character must be
 * masked with 0377 to avoid sign extension.
 */
int getchar()
{
#ifdef UNIX
  return (_getchar() & 0377);
#else
  char c;

  if (read(input_fd, &c, 1) != 1 && quit == FALSE)
  	panic("Can't read one char from fd #0");

  return c & 0377;
#endif /* UNIX */
}

/*
 * Display() shows count lines on the terminal starting at the given
 * coordinates. When the tail of the list is encountered it will fill the
 * rest of the screen with blank_line's.
 * When count is negative, a backwards print from `line' will be done.
 */
void display(x_coord, y_coord, line, count)
int x_coord, y_coord;
register LINE *line;
register int count;
{
  set_cursor(x_coord, y_coord);

/* Find new startline if count is negative */
  if (count < 0) {
  	line = proceed(line, count);
  	count = -count;
  }

/* Print the lines */
  while (line != tail && count-- >= 0) {
	line->shift_count=0;
  	line_print(line);
  	line = line->next;
  }

/* Print the blank lines (if any) */
  if (loading == FALSE) {
	while (count-- >= 0) {
#ifdef UNIX
		tputs(CE, 0, _putchar);
#else
		string_print(blank_line);
#endif /* UNIX */
		putchar('\n');
	}
  }
}

/*
 * Write_char does a buffered output. 
 */
int write_char(fd, c)
int fd;
char c;
{
  screen [out_count++] = c;
  if (out_count == SCREEN_SIZE)		/* Flush on SCREEN_SIZE chars */
  	return flush_buffer(fd);
  return FINE;
}

/*
 * Writeline writes the given string on the given filedescriptor.
 */
int writeline(fd, text)
register int fd;
register char *text;
{
  while(*text)
  	 if (write_char(fd, *text++) == ERRORS)
  		return ERRORS;
  return FINE;
}

/*
 * Put_line print the given line on the standard output. If offset is not zero
 * printing will start at that x-coordinate. If the FLAG clear_line is TRUE,
 * then (screen) line will be cleared when the end of the line has been
 * reached.
 */
void put_line(line, offset, clear_line)
LINE *line;				/* Line to print */
int offset;				/* Offset to start */
FLAG clear_line;			/* Clear to eoln if TRUE */
{
  register char *textp = line->text;
  register int count = get_shift(line->shift_count) * -SHIFT_SIZE;
  int tab_count;			/* Used in tab expansion */

/* Skip all chars as indicated by the offset and the shift_count field */
  while (count < offset) {
  	if (is_tab(*textp++))
  		count = tab(count);
  	else
  		count++;
  }

  while (*textp != '\n' && count < XBREAK) {
  	if (is_tab(*textp)) {		/* Expand tabs to spaces */
  		tab_count = tab(count);
  		while (count < XBREAK && count < tab_count) {
  			count++;
  			putchar(' ');
  		}
  		textp++;
  	}
  	else {
		if (*textp >= '\01' && *textp <= '\037') {
#ifdef UNIX
			tputs(SO, 0, _putchar);
#else
			string_print (rev_video);
#endif /* UNIX */
  			putchar(*textp++ + '\100');
#ifdef UNIX
			tputs(SE, 0, _putchar);
#else
			string_print (normal_video);
#endif /* UNIX */
		}
		else
  			putchar(*textp++);
  		count++;
  	}
  }

/* If line is longer than XBREAK chars, print the shift_mark */
  if (count == XBREAK && *textp != '\n')
  	putchar(textp[1]=='\n' ? *textp : SHIFT_MARK);

/* Clear the rest of the line is clear_line is TRUE */
  if (clear_line == TRUE) {
#ifdef	UNIX
  	tputs(CE, 0, _putchar);
#else
	string_print(blank_line);
#endif /* UNIX */
  	putchar('\n');
  }
}

/*
 * Flush the I/O buffer on filedescriptor fd.
 */
int flush_buffer(fd)
int fd;
{
  if (out_count <= 0)		/* There is nothing to flush */
  	return FINE;
#ifdef UNIX
  if (fd == STD_OUT) {
  	printf("%.*s", out_count, screen);
  	_flush();
  }
  else
#endif /* UNIX */
  if (write(fd, screen, out_count) != out_count) {
  	bad_write(fd);
  	return ERRORS;
  }
  clear_buffer();		/* Empty buffer */
  return FINE;
}

/*
 * Bad_write() is called when a write failed. Notify the user.
 */
void bad_write(fd)
int fd;
{
  if (fd == STD_OUT)		/* Cannot write to terminal? */
  	exit(1);
  
  clear_buffer();
  build_string(text_buffer, "Command aborted: %s (File incomplete)",
  		            (errno == ENOSPC || errno == -ENOSPC) ?
  			    "No space on device" : "Write error");
  error(text_buffer, NIL_PTR);
}

/*
 * Catch the SIGQUIT signal (^\) send to mined. It turns on the quitflag.
 */
void catch(sig)
int sig;
{
/* Reset the signal */
  signal(SIGQUIT, catch);
  quit = TRUE;
}

/*
 * Abort_mined() will leave mined. Confirmation is asked first.
 */
void abort_mined()
{
  quit = FALSE;

/* Ask for confirmation */
  status_line("Really abort? ", NIL_PTR);
  if (getchar() != 'y') {
  	clear_status();
  	return;
  }

/* Reset terminal */
  raw_mode(OFF);
  set_cursor(0, ymax);
  putchar('\n');
  flush();
#ifdef UNIX
  abort();
#else
  exit(1);
#endif /* UNIX */
}

#define UNDEF	_POSIX_VDISABLE

/*
 * Set and reset tty into CBREAK or old mode according to argument `state'. It
 * also sets all signal characters (except for ^\) to UNDEF. ^\ is caught.
 */
void raw_mode(state)
FLAG state;
{
  static struct termios old_tty;
  static struct termios new_tty;

  if (state == OFF) {
  	tcsetattr(input_fd, TCSANOW, &old_tty);
  	return;
  }

/* Save old tty settings */
  tcgetattr(input_fd, &old_tty);

/* Set tty to CBREAK mode */
  tcgetattr(input_fd, &new_tty);
  new_tty.c_lflag &= ~(ICANON|ECHO|ECHONL);
  new_tty.c_iflag &= ~(IXON|IXOFF);

/* Unset signal chars, leave only SIGQUIT set to ^\ */
  new_tty.c_cc[VINTR] = new_tty.c_cc[VSUSP] = UNDEF;
  new_tty.c_cc[VQUIT] = '\\' & 037;
  signal(SIGQUIT, catch);		/* Which is caught */

  tcsetattr(input_fd, TCSANOW, &new_tty);
}

/*
 * Panic() is called with an error number and a message. It is called when
 * something unrecoverable has happened.
 * It writes the message to the terminal, resets the tty and exits.
 * Ask the user if he wants to save his file.
 */
void panic(message)
register char *message;
{
  extern char yank_file[];

#ifdef UNIX
  tputs(CL, 0, _putchar);
  build_string(text_buffer, "%s\nError code %d\n", message, errno);
#else
  build_string(text_buffer, "%s%s\nError code %d\n", enter_string, message, errno);
#endif /* UNIX */
  (void) write(STD_OUT, text_buffer, length_of(text_buffer));

  if (loading == FALSE)
  	XT();			/* Check if file can be saved */
  else
  	(void) unlink(yank_file);
  raw_mode(OFF);

#ifdef UNIX
  abort();
#else
  exit(1);
#endif /* UNIX */
}

char *alloc(bytes)
int bytes;
{
  char *p;

  p = malloc((unsigned) bytes);
  if (p == NIL_PTR) {
	if (loading == TRUE)
		panic("File too big.");
	panic("Out of memory.");
  }
  return(p);
}

void free_space(p)
char *p;
{
  free(p);
}

/*  ========================================================================  *
 *				Main loops				      *
 *  ========================================================================  */

/* The mapping between input codes and functions. */

void (*key_map[256])() = {       /* map ASCII characters to functions */
   /* 000-017 */ MA, BL, MP, YA, SD, RD, MN, IF, DPC, S, S, DT, LR, S, DNW,LIB,
   /* 020-037 */ DPW, WB, GR, SH, DLN, SU, VI, XWT, XT, PT, EL, ESC, I, GOTO,
		 HIGH, LOW,
   /* 040-057 */ S, S, S, S, S, S, S, S, S, S, S, S, S, S, S, S,
   /* 060-077 */ S, S, S, S, S, S, S, S, S, S, S, S, S, S, S, S,
   /* 100-117 */ S, S, S, S, S, S, S, S, S, S, S, S, S, S, S, S,
   /* 120-137 */ S, S, S, S, S, S, S, S, S, S, S, S, S, S, S, S,
   /* 140-157 */ S, S, S, S, S, S, S, S, S, S, S, S, S, S, S, S,
   /* 160-177 */ S, S, S, S, S, S, S, S, S, S, S, S, S, S, S, DCC,
   /* 200-217 */ S, S, S, S, S, S, S, S, S, S, S, S, S, S, S, S,
   /* 220-237 */ S, S, S, S, S, S, S, S, S, S, S, S, S, S, S, S,
   /* 240-257 */ S, S, S, S, S, S, S, S, S, S, S, S, S, S, S, S,
   /* 260-277 */ S, S, S, S, S, S, S, S, S, S, S, S, S, S, S, S,
   /* 300-317 */ S, S, S, S, S, S, S, S, S, S, S, S, S, S, S, S,
   /* 320-337 */ S, S, S, S, S, S, S, S, S, S, S, S, S, S, S, S,
   /* 340-357 */ S, S, S, S, S, S, S, S, S, S, S, S, S, S, S, S,
   /* 360-377 */ S, S, S, S, S, S, S, S, S, S, S, S, S, S, S, S,
};

int nlines;			/* Number of lines in file */
LINE *header;			/* Head of line list */
LINE *tail;			/* Last line in line list */
LINE *cur_line;			/* Current line in use */
LINE *top_line;			/* First line of screen */
LINE *bot_line;			/* Last line of screen */
char *cur_text;			/* Current char on current line in use */
int last_y;			/* Last y of screen. Usually SCREENMAX */
char screen[SCREEN_SIZE];	/* Output buffer for "writes" and "reads" */

int x, y;			/* x, y coordinates on screen */
FLAG modified = FALSE;		/* Set when file is modified */
FLAG stat_visible;		/* Set if status_line is visible */
FLAG writable;			/* Set if file cannot be written */
FLAG loading;			/* Set if we are loading a file. */
FLAG quit = FALSE;		/* Set when quit character is typed */
FLAG rpipe = FALSE;		/* Set if file should be read from stdin */
int input_fd = 0;		/* Fd for command input */
int out_count;			/* Index in output buffer */
char file_name[LINE_LEN];	/* Name of file in use */
char text_buffer[MAX_CHARS];	/* Buffer for modifying text */

/* Escape sequences. */
#ifdef UNIX
char *CE, *VS, *SO, *SE, *CL, *AL, *CM;
#else
char   *enter_string = "\033[H\033[J";	/* String printed on entering mined */
char   *pos_string = "\033[%d;%dH";	/* Absolute cursor position */
char   *rev_scroll = "\033M";		/* String for reverse scrolling */
char   *rev_video = "\033[7m";		/* String for starting reverse video */
char   *normal_video = "\033[m";	/* String for leaving reverse video */
char   *blank_line = "\033[K";		/* Clear line to end */
#endif /* UNIX */

/* 
 * Yank variables.
 */
FLAG yank_status = NOT_VALID;		/* Status of yank_file */
char yank_file[] = "/tmp/mined.XXXXXX";
long chars_saved;			/* Nr of chars in buffer */

/*
 * Initialize is called when a another file is edited. It free's the allocated
 * space and sets modified back to FALSE and fixes the header/tail pointer.
 */
void initialize()
{
  register LINE *line, *next_line;

/* Delete the whole list */
  for (line = header->next; line != tail; line = next_line) {
  	next_line = line->next;
  	free_space(line->text);
  	free_space((char*)line);
  }

/* header and tail should point to itself */
  line->next = line->prev = line;
  x = y = 0;
  rpipe = modified = FALSE;
}

/*
 * Basename() finds the absolute name of the file out of a given path_name.
 */
char *basename(path)
char *path;
{
  register char *ptr = path;
  register char *last = NIL_PTR;

  while (*ptr != '\0') {
  	if (*ptr == '/')
  		last = ptr;
  	ptr++;
  }
  if (last == NIL_PTR)
  	return path;
  if (*(last + 1) == '\0') {	/* E.g. /usr/tmp/pipo/ */
  	*last = '\0';
  	return basename(path);/* Try again */
  }
  return last + 1;
}

/*
 * Load_file loads the file `file' into core. If file is a NIL_PTR or the file
 * couldn't be opened, just some initializations are done, and a line consisting
 * of a `\n' is installed.
 */
void load_file(file)
char *file;
{
  register LINE *line = header;
  register int len;
  long nr_of_chars = 0L;
  int fd = -1;			/* Filedescriptor for file */

  nlines = 0;			/* Zero lines to start with */

/* Open file */
  writable = TRUE;		/* Benefit of the doubt */
  if (file == NIL_PTR) {
	if (rpipe == FALSE)
  		status_line("No file.", NIL_PTR);
	else {
		fd = 0;
		file = "standard input";
	}
	file_name[0] = '\0';
  }
  else {
  	copy_string(file_name, file);	/* Save file name */
  	if (access(file, 0) < 0)	/* Cannot access file. */
  		status_line("New file ", file);
  	else if ((fd = open(file, 0)) < 0)
  		status_line("Cannot open ", file);
  	else if (access(file, 2) != 0)	/* Set write flag */
  		writable = FALSE;
  }

/* Read file */
  loading = TRUE;				/* Loading file, so set flag */

  if (fd >= 0) {
  	status_line("Reading ", file);
  	while ((len = get_line(fd, text_buffer)) != ERRORS) {
  		line = line_insert(line, text_buffer, len);
  		nr_of_chars += (long) len;
  	}
  	if (nlines == 0)		/* The file was empty! */
  		line = line_insert(line, "\n", 1);
  	clear_buffer();		/* Clear output buffer */
  	cur_line = header->next;
  	fstatus("Read", nr_of_chars);
  	(void) close(fd);		/* Close file */
  }
  else					/* Just install a "\n" */
  	(void) line_insert(line, "\n", 1);

  reset(header->next, 0);		/* Initialize pointers */

/* Print screen */
  display (0, 0, header->next, last_y);
  move_to (0, 0);
  flush();				/* Flush buffer */
  loading = FALSE;			/* Stop loading, reset flag */
}


/*
 * Get_line reads one line from filedescriptor fd. If EOF is reached on fd,
 * get_line() returns ERRORS, else it returns the length of the string.
 */
int get_line(fd, buffer)
int fd;
register char *buffer;
{
  static char *last = NIL_PTR;
  static char *current = NIL_PTR;
  static int read_chars;
  register char *cur_pos = current;
  char *begin = buffer;

  do {
  	if (cur_pos == last) {
  		if ((read_chars = read(fd, screen, SCREEN_SIZE)) <= 0)
  			break;
  		last = &screen[read_chars];
  		cur_pos = screen;
  	}
	if (*cur_pos == '\0')
		*cur_pos = ' ';
  } while ((*buffer++ = *cur_pos++) != '\n');

  current = cur_pos;
  if (read_chars <= 0) {
  	if (buffer == begin)
  		return ERRORS;
  	if (*(buffer - 1) != '\n')
  		if (loading == TRUE) /* Add '\n' to last line of file */
  			*buffer++ = '\n';
  		else {
  			*buffer = '\0';
  			return NO_LINE;
  		}
  }

  *buffer = '\0';
  return buffer - begin;
}

/*
 * Install_line installs the buffer into a LINE structure It returns a pointer
 * to the allocated structure.
 */
LINE *install_line(buffer, length)
char *buffer;
int length;
{
  register LINE *new_line = (LINE *) alloc(sizeof(LINE));

  new_line->text = alloc(length + 1);
  new_line->shift_count = 0;
  copy_string(new_line->text, buffer);

  return new_line;
}

void main(argc, argv)
int argc;
char *argv[];
{
/* mined is the Minix editor. */

  register int index;		/* Index in key table */
  struct winsize winsize;

#ifdef UNIX
  get_term();
  tputs(VS, 0, _putchar);
  tputs(CL, 0, _putchar);
#else
  string_print(enter_string);			/* Hello world */
#endif /* UNIX */
  if (ioctl(STD_OUT, TIOCGWINSZ, &winsize) == 0 && winsize.ws_row != 0) {
	ymax = winsize.ws_row - 1;
	screenmax = ymax - 1;
  }

  if (!isatty(0)) {		/* Reading from pipe */
	if (argc != 1) {
		write(2, "Cannot find terminal.\n", 22);
		exit (1);
	}
	rpipe = TRUE;
	modified = TRUE;	/* Set modified so he can write */
	open_device();
  }

  raw_mode(ON);			/* Set tty to appropriate mode */

  header = tail = (LINE *) alloc(sizeof(LINE));	/* Make header of list*/
  header->text = NIL_PTR;
  header->next = tail->prev = header;

/* Load the file (if any) */
  if (argc < 2)
  	load_file(NIL_PTR);
  else {
  	(void) get_file(NIL_PTR, argv[1]);	/* Truncate filename */
  	load_file(argv[1]);
  }

 /* Main loop of the editor. */
  for (;;) {
  	index = getchar();
  	if (stat_visible == TRUE)
  		clear_status();
  	if (quit == TRUE)
  		abort_mined();
  	else {			/* Call the function for this key */
  		(*key_map[index])(index);
  		flush();       /* Flush output (if any) */
  		if (quit == TRUE)
  			quit = FALSE;
  	}
  }
  /* NOTREACHED */
}

/*  ========================================================================  *
 *				Miscellaneous				      *
 *  ========================================================================  */

/*
 * Redraw the screen
 */
void RD()
{
/* Clear screen */
#ifdef UNIX
  tputs(VS, 0, _putchar);
  tputs(CL, 0, _putchar);
#else
  string_print(enter_string);
#endif /* UNIX */

/* Print first page */
  display(0, 0, top_line, last_y);

/* Clear last line */
  set_cursor(0, ymax);
#ifdef UNIX
  tputs(CE, 0, _putchar);
#else
  string_print(blank_line);
#endif /* UNIX */
  move_to(x, y);
}

/*
 * Ignore this keystroke.
 */
void I()
{
}

/*
 * Leave editor. If the file has changed, ask if the user wants to save it.
 */
void XT()
{
  if (modified == TRUE && ask_save() == ERRORS)
  	return;

  raw_mode(OFF);
  set_cursor(0, ymax);
  putchar('\n');
  flush();
  (void) unlink(yank_file);		/* Might not be necessary */
  exit(0);
}

void (*escfunc(c))()
int c;
{
#if (CHIP == M68000)
#ifndef COMPAT
  int ch;
#endif
#endif
  if (c == '[') {
	/* Start of ASCII escape sequence. */
	c = getchar();
#if (CHIP == M68000)
#ifndef COMPAT
	if ((c >= '0') && (c <= '9')) ch = getchar();
	/* ch is either a tilde or a second digit */
#endif
#endif
	switch (c) {
	case 'H': return(HO);
	case 'A': return(UP);
	case 'B': return(DN);
	case 'C': return(RT);
	case 'D': return(LF);
#if (CHIP == M68000)
#ifndef COMPAT
	/* F1 = ESC [ 1 ~ */
	/* F2 = ESC [ 2 ~ */
	/* F3 = ESC [ 3 ~ */
	/* F4 = ESC [ 4 ~ */
	/* F5 = ESC [ 5 ~ */
	/* F6 = ESC [ 6 ~ */
	/* F7 = ESC [ 17 ~ */
	/* F8 = ESC [ 18 ~ */
	case '1': 
	 	  switch (ch) {
		  case '~': return(SF);
		  case '7': (void) getchar(); return(MA);
		  case '8': (void) getchar(); return(CTL);
                  }
	case '2': return(SR);
	case '3': return(PD);
	case '4': return(PU);
	case '5': return(FS);
	case '6': return(EF);
#endif
#endif
#if (CHIP == INTEL)
#ifdef ASSUME_CONS25
	case 'G': return(PD);
	case 'I': return(PU);
	case 'F': return(EF);
	/* F1 - help */
	case 'M': return(HLP);
	/* F2 - file status */
	case 'N': return(FS);
	/* F3 - search fwd */
	case 'O': return(SF);
	/* Shift-F3 - search back */
	case 'a':return(SR);
	/* F4 - global replace */
	case 'P': return(GR);
	/* Shift-F4 - line replace */
	case 'b': return(LR);
#else
	case 'G': return(FS);
	case 'S': return(SR);
	case 'T': return(SF);
	case 'U': return(PD);
	case 'V': return(PU);
	case 'Y': return(EF);
#endif
#endif
	}
	return(I);
  }
#if (CHIP == M68000)
#ifdef COMPAT
  if (c == 'O') {
	/* Start of ASCII function key escape sequence. */
	switch (getchar()) {
	case 'P': return(SF);
	case 'Q': return(SR);
	case 'R': return(PD);
	case 'S': return(PU);
	case 'T': return(FS);
	case 'U': return(EF);
	case 'V': return(MA);
	case 'W': return(CTL);
	}
    }
#endif
#endif
  return(I);
}

/*
 * ESC() wants a count and a command after that. It repeats the 
 * command count times. If a ^\ is given during repeating, stop looping and
 * return to main loop.
 */
void ESC()
{
  register int count = 0;
  register void (*func)();
  int index;

  index = getchar();
  while (index >= '0' && index <= '9' && quit == FALSE) {
  	count *= 10;
  	count += index - '0';
  	index = getchar();
  }
  if (count == 0) {
	count = 1;
	func = escfunc(index);
  } else {
	func = key_map[index];
	if (func == ESC)
		func = escfunc(getchar());
  }

  if (func == I) {	/* Function assigned? */
  	clear_status();
  	return;
  }

  while (count-- > 0 && quit == FALSE) {
  	if (stat_visible == TRUE)
  		clear_status();
  	(*func)(index);
  	flush();
  }

  if (quit == TRUE)		/* Abort has been given */
  	error("Aborted", NIL_PTR);
}

/*
 * Ask the user if he wants to save his file or not.
 */
int ask_save()
{
  register int c;

  status_line(file_name[0] ? basename(file_name) : "[buffer]" ,
					     " has been modified. Save? (y/n)");

  while((c = getchar()) != 'y' && c != 'n' && quit == FALSE) {
  	ring_bell();
  	flush();
  }

  clear_status();

  if (c == 'y')
  	return WT();

  if (c == 'n')
  	return FINE;

  quit = FALSE;	/* Abort character has been given */
  return ERRORS;
}

/*
 * Line_number() finds the line number we're on.
 */
int line_number()
{
  register LINE *line = header->next;
  register int count = 1;

  while (line != cur_line) {
  	count++;
  	line = line->next;
  }
  
  return count;
}
  
/*
 * Display a line telling how many chars and lines the file contains. Also tell
 * whether the file is readonly and/or modified.
 */
void file_status(message, count, file, lines, writefl, changed)
char *message;
register long count;		/* Contains number of characters in file */
char *file;
int lines;
FLAG writefl, changed;
{
  register LINE *line;
  char msg[LINE_LEN + 40];/* Buffer to hold line */
  char yank_msg[LINE_LEN];/* Buffer for msg of yank_file */

  if (count < 0)		/* Not valid. Count chars in file */
  	for (line = header->next; line != tail; line = line->next)
  		count += length_of(line->text);

  if (yank_status != NOT_VALID)	/* Append buffer info */
  	build_string(yank_msg, " Buffer: %D char%s.", chars_saved,
						(chars_saved == 1L) ? "" : "s");
  else
  	yank_msg[0] = '\0';

  build_string(msg, "%s %s%s%s %d line%s %D char%s.%s Line %d", message,
  		    (rpipe == TRUE && *message != '[') ? "standard input" : basename(file),
  		    (changed == TRUE) ? "*" : "",
  		    (writefl == FALSE) ? " (Readonly)" : "",
  		    lines, (lines == 1) ? "" : "s", 
		    count, (count == 1L) ? "" : "s",
		    yank_msg, line_number());

  if (length_of(msg) + 1 > LINE_LEN - 4) {
  	msg[LINE_LEN - 4] = SHIFT_MARK;	/* Overflow on status line */
  	msg[LINE_LEN - 3] = '\0';
  }
  status_line(msg, NIL_PTR);		/* Print the information */
}

/*
 * Build_string() prints the arguments as described in fmt, into the buffer.
 * %s indicates an argument string, %d indicated an argument number.
 */
#if __STDC__
void build_string(char *buf, char *fmt, ...)
{
#else
void build_string(buf, fmt, va_alist)
char *buf, *fmt;
va_dcl
{
#endif
  va_list argptr;
  char *scanp;

#if __STDC__
  va_start(argptr, fmt);
#else
  va_start(argptr);
#endif

  while (*fmt) {
  	if (*fmt == '%') {
  		fmt++;
  		switch (*fmt++) {
  		case 's' :
  			scanp = va_arg(argptr, char *);
  			break;
  		case 'd' :
  			scanp = num_out((long) va_arg(argptr, int));
  			break;
  		case 'D' :
  			scanp = num_out((long) va_arg(argptr, long));
  			break;
  		default :
  			scanp = "";
  		}
  		while (*buf++ = *scanp++)
  			;
  		buf--;
  	}
  	else
  		*buf++ = *fmt++;
  }
  va_end(argptr);
  *buf = '\0';
}

/*
 * Output an (unsigned) long in a 10 digit field without leading zeros.
 * It returns a pointer to the first digit in the buffer.
 */
char *num_out(number)
long number;
{
  static char num_buf[11];		/* Buffer to build number */
  register long digit;			/* Next digit of number */
  register long pow = 1000000000L;	/* Highest ten power of long */
  FLAG digit_seen = FALSE;
  int i;

  for (i = 0; i < 10; i++) {
  	digit = number / pow;		/* Get next digit */
  	if (digit == 0L && digit_seen == FALSE && i != 9)
  		num_buf[i] = ' ';
  	else {
  		num_buf[i] = '0' + (char) digit;
  		number -= digit * pow;	/* Erase digit */
  		digit_seen = TRUE;
  	}
  	pow /= 10L;			/* Get next digit */
  }
  for (i = 0; num_buf[i] == ' '; i++)	/* Skip leading spaces */
  	;
  return (&num_buf[i]);
}

/*
 * Get_number() read a number from the terminal. The last character typed in is
 * returned.  ERRORS is returned on a bad number. The resulting number is put
 * into the integer the arguments points to.
 */
int get_number(message, result)
char *message;
int *result;
{
  register int index;
  register int count = 0;

  status_line(message, NIL_PTR);

  index = getchar();
  if (quit == FALSE && (index < '0' || index > '9')) {
  	error("Bad count", NIL_PTR);
  	return ERRORS;
  }

/* Convert input to a decimal number */
  while (index >= '0' && index <= '9' && quit == FALSE) {
  	count *= 10;
  	count += index - '0';
  	index = getchar();
  }

  if (quit == TRUE) {
  	clear_status();
  	return ERRORS;
  }

  *result = count;
  return index;
}

/*
 * Input() reads a string from the terminal.  When the KILL character is typed,
 * it returns ERRORS.
 */
int input(inbuf, clearfl)
char *inbuf;
FLAG clearfl;
{
  register char *ptr;
  register char c;			/* Character read */

  ptr = inbuf;

  *ptr = '\0';
  while (quit == FALSE) {
  	flush();
  	switch (c = getchar()) {
  		case '\b' :		/* Erase previous char */
  			if (ptr > inbuf) {
  				ptr--;
#ifdef UNIX
  				tputs(SE, 0, _putchar);
#else
  				string_print(normal_video);
#endif /* UNIX */
  				if (is_tab(*ptr))
  					string_print(" \b\b\b  \b\b");
  				else
  					string_print(" \b\b \b");
#ifdef UNIX
  				tputs(SO, 0, _putchar);
#else
  				string_print(rev_video);
#endif /* UNIX */
  				string_print(" \b");
  				*ptr = '\0';
  			}
  			else
  				ring_bell();
  			break;
  		case '\n' :		/* End of input */
  			/* If inbuf is empty clear status_line */
  			return (ptr == inbuf && clearfl == TRUE) ? NO_INPUT :FINE;
  		default :		/* Only read ASCII chars */
  			if ((c >= ' ' && c <= '~') || c == '\t') {
  				*ptr++ = c;
  				*ptr = '\0';
  				if (c == '\t')
  					string_print("^I");
  				else
  					putchar(c);
  				string_print(" \b");
  			}
  			else
  				ring_bell();
  	}
  }
  quit = FALSE;
  return ERRORS;
}

/*
 * Get_file() reads a filename from the terminal. Filenames longer than 
 * FILE_LENGHT chars are truncated.
 */
int get_file(message, file)
char *message, *file;
{
  char *ptr;
  int ret;

  if (message == NIL_PTR || (ret = get_string(message, file, TRUE)) == FINE) {
  	if (length_of((ptr = basename(file))) > NAME_MAX)
  		ptr[NAME_MAX] = '\0';
  }
  return ret;
}

/*  ========================================================================  *
 *				UNIX I/O Routines			      *
 *  ========================================================================  */

#ifdef UNIX
#undef putchar

int _getchar()
{
  char c;

  if (read(input_fd, &c, 1) != 1 && quit == FALSE)
	panic ("Cannot read 1 byte from input");
  return c & 0377;
}

void _flush()
{
  (void) fflush(stdout);
}

void _putchar(c)
char c;
{
  (void) write_char(STD_OUT, c);
}

void get_term()
{
  static char termbuf[50];
  extern char *tgetstr(), *getenv();
  char *loc = termbuf;
  char entry[1024];

  if (tgetent(entry, getenv("TERM")) <= 0) {
  	printf("Unknown terminal.\n");
  	exit(1);
  }

  AL = tgetstr("al", &loc);
  CE = tgetstr("ce", &loc);
  VS = tgetstr("vs", &loc);
  CL = tgetstr("cl", &loc);
  SO = tgetstr("so", &loc);
  SE = tgetstr("se", &loc);
  CM = tgetstr("cm", &loc);
  ymax = tgetnum("li") - 1;
  screenmax = ymax - 1;

  if (!CE || !SO || !SE || !CL || !AL || !CM) {
  	printf("Sorry, no mined on this type of terminal\n");
  	exit(1);
  }
}
#endif /* UNIX */
