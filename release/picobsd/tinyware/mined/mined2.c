/*
 * Part 2 of the mined editor.
 */

/*  ========================================================================  *
 *				Move Commands				      *	
 *  ========================================================================  */

#include "mined.h"
#include <string.h>

/*
 * Move one line up.
 */
void UP()
{
  if (y == 0) {		/* Top line of screen. Scroll one line */
  	(void) reverse_scroll();
  	move_to(x, y);
  }
  else			/* Move to previous line */
  	move_to(x, y - 1);
}

static char *help_string=
"			Mined (Minix Editor), FreeBSD version.\n"
"------------------------+-------------------------------+---------------------\n"
"	CURSOR MOTION	|		EDITING		|	MISC\n"
" Up			| ^N	Delete next word	| ^E	Erase & redraw\n"
" Down	cursor keys	| ^P	Delete prev. word	|	screen\n"
" Left			| ^T	Delete to EOL		| ^\\	Abort current\n"
" Right			+-------------------------------+	operation\n"
" ^A	start of line	|		BLOCKS		| Esc	repeat last\n"
" ^Z	end of line	| ^@	Set mark		|	cmd # times\n"
" ^^	screen top	| ^K	Delete mark <--> cursor	| F2	file status\n"
" ^_	screen bottom	| ^C	Save mark <--> cursor	+=====================\n"
" ^F	word fwd.	| ^Y	Insert the contents of	| ^X	EXIT\n"
" ^B	word back	| 	the save file at cursor | ^S	run shell\n"
"------------------------+ ^Q	Insert the contents of	+=====================\n"
"	SCREEN MOTION	|	the save file into new	|   SEARCH & REPLACE\n"
"  Home	file top	|	file			| F3	fwd. search\n"
"  End	file bottom	+-------------------------------+ SF3	bck. search\n"
"  PgUp	page up		|		FILES		| F4	Global replace\n"
"  PgD	page down	| ^G	Insert a file at cursor | SF4	Line replace\n"
"  ^D	rev. scroll	| ^V	Visit another file	+---------------------\n"
"  ^U	fwd. scroll	| ^W	Write current file	| F1	HELP\n"
"  ^]	goto line #	|				|\n"
"------------------------+-------------------------------+---------------------\n"
"Press any key to continue...";
/*
 * Help
 */
void HLP()
{
	char c;

	string_print(enter_string);
	string_print(help_string);
	flush();
	c=getchar();
	RD();
	return;
}

/*
 * Move one line down.
 */
void DN()
{
  if (y == last_y) {	/* Last line of screen. Scroll one line */
	if (bot_line->next == tail && bot_line->text[0] != '\n') {
		dummy_line();		/* Create new empty line */
		DN();
		return;
	}
	else {
		(void) forward_scroll();
		move_to(x, y);
	}
  }
  else			/* Move to next line */
  	move_to(x, y + 1);
}

/*
 * Move left one position.
 */
void LF()
{
  if (x == 0 && get_shift(cur_line->shift_count) == 0) {/* Begin of line */
	if (cur_line->prev != header) {
		UP();					/* Move one line up */
		move_to(LINE_END, y);
	}
  }
  else
  	move_to(x - 1, y);
}

/*
 * Move right one position.
 */
void RT()
{
  if (*cur_text == '\n') {
  	if (cur_line->next != tail) {		/* Last char of file */
		DN();				/* Move one line down */
		move_to(LINE_START, y);
	}
  }
  else
  	move_to(x + 1, y);
}

/*
 * Move to coordinates [0, 0] on screen.
 */
void HIGH()
{
  move_to(0, 0);
}

/*
 * Move to coordinates [0, YMAX] on screen.
 */
void LOW()
{
  move_to(0, last_y);
}

/*
 * Move to begin of line.
 */
void BL()
{
  move_to(LINE_START, y);
}

/*
 * Move to end of line.
 */
void EL()
{
  move_to(LINE_END, y);
}

/*
 * GOTO() prompts for a linenumber and moves to that line.
 */
void GOTO()
{
  int number;
  LINE *line;

  if (get_number("Please enter line number.", &number) == ERRORS)
  	return;

  if (number <= 0 || (line = proceed(header->next, number - 1)) == tail)
  	error("Illegal line number: ", num_out((long) number));
  else
  	move_to(x, find_y(line));
}

/*
 * Scroll forward one page or to eof, whatever comes first. (Bot_line becomes 
 * top_line of display.) Try to leave the cursor on the same line. If this is
 * not possible, leave cursor on the line halfway the page.
 */
void PD()
{
  register int i;

  for (i = 0; i < screenmax; i++)
  	if (forward_scroll() == ERRORS)
  		break;			/* EOF reached */
  if (y - i < 0)				/* Line no longer on screen */
  	move_to(0, screenmax >> 1);
  else
  	move_to(0, y - i);
}


/*
 * Scroll backwards one page or to top of file, whatever comes first. (Top_line
 * becomes bot_line of display).  The very bottom line (YMAX) is always blank.
 * Try to leave the cursor on the same line. If this is not possible, leave
 * cursor on the line halfway the page.
 */
void PU()
{
  register int i;

  for (i = 0; i < screenmax; i++)
  	if (reverse_scroll() == ERRORS)
  		break;			/* Top of file reached */
  set_cursor(0, ymax);			/* Erase very bottom line */
#ifdef UNIX
  tputs(CE, 0, _putchar);
#else
  string_print(blank_line);
#endif /* UNIX */
  if (y + i > screenmax)			/* line no longer on screen */
  	move_to(0, screenmax >> 1);
  else
  	move_to(0, y + i);
}

/*
 * Go to top of file, scrolling if possible, else redrawing screen.
 */
void HO()
{
  if (proceed(top_line, -screenmax) == header)
  	PU();			/* It fits. Let PU do it */
  else {
  	reset(header->next, 0);/* Reset top_line, etc. */
  	RD();			/* Display full page */
  }
  move_to(LINE_START, 0);
}

/*
 * Go to last line of file, scrolling if possible, else redrawing screen
 */
void EF()
{
  if (tail->prev->text[0] != '\n')
	dummy_line();
  if (proceed(bot_line, screenmax) == tail)
  	PD();			/* It fits. Let PD do it */
  else {
  	reset(proceed(tail->prev, -screenmax), screenmax);
  	RD();			/* Display full page */
  }
  move_to(LINE_START, last_y);
}

/*
 * Scroll one line up. Leave the cursor on the same line (if possible).
 */
void SU()
{
  if (top_line->prev == header)	/* Top of file. Can't scroll */
  	return;

  (void) reverse_scroll();
  set_cursor(0, ymax);		/* Erase very bottom line */
#ifdef UNIX
  tputs(CE, 0, _putchar);
#else
  string_print(blank_line);
#endif /* UNIX */
  move_to(x, (y == screenmax) ? screenmax : y + 1);
}

/*
 * Scroll one line down. Leave the cursor on the same line (if possible).
 */
void SD()
{
  if (forward_scroll() != ERRORS) 
  	move_to(x, (y == 0) ? 0 : y - 1);
  else
  	set_cursor(x, y);
}

/*
 * Perform a forward scroll. It returns ERRORS if we're at the last line of the
 * file.
 */
int forward_scroll()
{
  if (bot_line->next == tail)		/* Last line of file. No dice */
  	return ERRORS;
  top_line = top_line->next;
  bot_line = bot_line->next;
  cur_line = cur_line->next;
  set_cursor(0, ymax);
  line_print(bot_line);

  return FINE;
}

/*
 * Perform a backwards scroll. It returns ERRORS if we're at the first line
 * of the file.
 */
int reverse_scroll()
{
  if (top_line->prev == header)
  	return ERRORS;		/* Top of file. Can't scroll */

  if (last_y != screenmax)	/* Reset last_y if necessary */
  	last_y++;
  else
  	bot_line = bot_line->prev;	/* Else adjust bot_line */
  top_line = top_line->prev;
  cur_line = cur_line->prev;

/* Perform the scroll */
  set_cursor(0, 0);
#ifdef UNIX
  tputs(AL, 0, _putchar);
#else
  string_print(rev_scroll);
#endif /* UNIX */
  set_cursor(0, 0);
  line_print(top_line);

  return FINE;
}

/*
 * A word is defined as a number of non-blank characters separated by tabs
 * spaces or linefeeds.
 */

/*
 * MP() moves to the start of the previous word. A word is defined as a
 * number of non-blank characters separated by tabs spaces or linefeeds.
 */
void MP()
{
  move_previous_word(NO_DELETE);
}

void move_previous_word(remove)
FLAG remove;
{
  register char *begin_line;
  register char *textp;
  char start_char = *cur_text;
  char *start_pos = cur_text;

/* Fist check if we're at the beginning of line. */
  if (cur_text == cur_line->text) {
  	if (cur_line->prev == header)
  		return;
  	start_char = '\0';
  }

  LF();

  begin_line = cur_line->text;
  textp = cur_text;

/* Check if we're in the middle of a word. */
  if (!alpha(*textp) || !alpha(start_char)) {
  	while (textp != begin_line && (white_space(*textp) || *textp == '\n'))
  		textp--;
  }

/* Now we're at the end of previous word. Skip non-blanks until a blank comes */
  while (textp != begin_line && alpha(*textp))
  	textp--;

/* Go to the next char if we're not at the beginning of the line */
  if (textp != begin_line && *textp != '\n')
  	textp++;

/* Find the x-coordinate of this address, and move to it */
  move_address(textp);
  if (remove == DELETE)
  	delete(cur_line, textp, cur_line, start_pos);
}

/*
 * MN() moves to the start of the next word. A word is defined as a number of
 * non-blank characters separated by tabs spaces or linefeeds. Always keep in
 * mind that the pointer shouldn't pass the '\n'.
 */
void MN()
{
  move_next_word(NO_DELETE);
}

void move_next_word(remove)
FLAG remove;
{
  register char *textp = cur_text;

/* Move to the end of the current word. */
  while (*textp != '\n' && alpha(*textp))
  	textp++;

/* Skip all white spaces */
  while (*textp != '\n' && white_space(*textp))
  	textp++;
/* If we're deleting. delete the text in between */
  if (remove == DELETE) {
  	delete(cur_line, cur_text, cur_line, textp);
  	return;
  }

/* If we're at end of line. move to the first word on the next line. */
  if (*textp == '\n' && cur_line->next != tail) {
  	DN();
  	move_to(LINE_START, y);
  	textp = cur_text;
  	while (*textp != '\n' && white_space(*textp))
  		textp++;
  }
  move_address(textp);
}

/*  ========================================================================  *
 *				Modify Commands				      *
 *  ========================================================================  */

/*
 * DCC deletes the character under the cursor.  If this character is a '\n' the
 * current line is joined with the next one.
 * If this character is the only character of the line, the current line will
 * be deleted.
 */
void DCC()
{
  if (*cur_text == '\n')
  	delete(cur_line,cur_text, cur_line->next,cur_line->next->text);
  else
  	delete(cur_line, cur_text, cur_line, cur_text + 1);
}

/*
 * DPC deletes the character on the left side of the cursor.  If the cursor is
 * at the beginning of the line, the last character if the previous line is
 * deleted. 
 */
void DPC()
{
  if (x == 0 && cur_line->prev == header)
  	return;			/* Top of file */
  
  LF();				/* Move one left */
  DCC();				/* Delete character under cursor */
}

/*
 * DLN deletes all characters until the end of the line. If the current
 * character is a '\n', then delete that char.
 */
void DLN()
{
  if (*cur_text == '\n')
  	DCC();
  else
  	delete(cur_line, cur_text, cur_line, cur_text + length_of(cur_text) -1);
}

/*
 * DNW() deletes the next word (as described in MN())
 */
void DNW()
{
  if (*cur_text == '\n')
  	DCC();
  else
  	move_next_word(DELETE);
}

/*
 * DPW() deletes the next word (as described in MP())
 */
void DPW()
{
  if (cur_text == cur_line->text)
  	DPC();
  else
  	move_previous_word(DELETE);
}

/*
 * Insert character `character' at current location.
 */
void S(character)
register char character;
{
  static char buffer[2];

  buffer[0] = character;
/* Insert the character */
  if (insert(cur_line, cur_text, buffer) == ERRORS)
  	return;

/* Fix screen */
  if (character == '\n') {
  	set_cursor(0, y);
  	if (y == screenmax) {		/* Can't use display */
  		line_print(cur_line);
  		(void) forward_scroll();
  	}
  	else {
  		reset(top_line, y);	/* Reset pointers */
  		display(0, y, cur_line, last_y - y);
  	}
  	move_to(0, (y == screenmax) ? y : y + 1);
  }
  else if (x + 1 == XBREAK)/* If line must be shifted, just call move_to*/
  	move_to(x + 1, y);
  else {			 /* else display rest of line */
  	put_line(cur_line, x, FALSE);
  	move_to(x + 1, y);
  }
}

/*
 * CTL inserts a control-char at the current location. A message that this
 * function is called is displayed at the status line.
 */
void CTL()
{
  register char ctrl;

  status_line("Enter control character.", NIL_PTR);
  if ((ctrl = getchar()) >= '\01' && ctrl <= '\037') {
  	S(ctrl);		/* Insert the char */
	clear_status();
  }
  else
	error ("Unknown control character", NIL_PTR);
}

/*
 * LIB insert a line at the current position and moves back to the end of
 * the previous line.
 */
void LIB()
{
  S('\n');	  		/* Insert the line */
  UP();				/* Move one line up */
  move_to(LINE_END, y);		/* Move to end of this line */
}

/*
 * Line_insert() inserts a new line with text pointed to by `string'.
 * It returns the address of the new line.
 */
LINE *line_insert(line, string, len)
register LINE *line;
char *string;
int len;
{
  register LINE *new_line;

/* Allocate space for LINE structure and text */
  new_line = install_line(string, len);

/* Install the line into the double linked list */
  new_line->prev = line;
  new_line->next = line->next;
  line->next = new_line;
  new_line->next->prev = new_line;

/* Increment nlines */
  nlines++;

  return new_line;
}

/*
 * Insert() insert the string `string' at the given line and location.
 */
int insert(line, location, string)
register LINE *line;
char *location, *string;
{
  register char *bufp = text_buffer;	/* Buffer for building line */
  register char *textp = line->text;

  if (length_of(textp) + length_of(string) >= MAX_CHARS) {
  	error("Line too long", NIL_PTR);
  	return ERRORS;
  }

  modified = TRUE;			/* File has been modified */

/* Copy part of line until `location' has been reached */
  while (textp != location)
  	*bufp++ = *textp++;
  
/* Insert string at this location */
  while (*string != '\0')
  	*bufp++ = *string++;
  *bufp = '\0';
  
  if (*(string - 1) == '\n')		/* Insert a new line */
  	(void) line_insert(line, location, length_of(location));
  else					/* Append last part of line */
  	copy_string(bufp, location);

/* Install the new text in this line */
  free_space(line->text);
  line->text = alloc(length_of(text_buffer) + 1);
  copy_string(line->text, text_buffer);

  return FINE;
}

/*
 * Line_delete() deletes the argument line out of the line list. The pointer to
 * the next line is returned.
 */
LINE *line_delete(line)
register LINE *line;
{
  register LINE *next_line = line->next;

/* Delete the line */
  line->prev->next = line->next;
  line->next->prev = line->prev;

/* Free allocated space */
  free_space(line->text);
  free_space((char*)line);

/* Decrement nlines */
  nlines--;

  return next_line;
}

/*
 * Delete() deletes all the characters (including newlines) between the
 * startposition and endposition and fixes the screen accordingly. It
 * returns the number of lines deleted.
 */
void delete(start_line, start_textp, end_line, end_textp)
register LINE *start_line;
LINE *end_line;
char *start_textp, *end_textp;
{
  register char *textp = start_line->text;
  register char *bufp = text_buffer;	/* Storage for new line->text */
  LINE *line, *stop;
  int line_cnt = 0;			/* Nr of lines deleted */
  int count = 0;
  int shift = 0;				/* Used in shift calculation */
  int nx = x;

  modified = TRUE;			/* File has been modified */

/* Set up new line. Copy first part of start line until start_position. */
  while (textp < start_textp) {
  	*bufp++ = *textp++;
  	count++;
  }

/* Check if line doesn't exceed MAX_CHARS */
  if (count + length_of(end_textp) >= MAX_CHARS) {
  	error("Line too long", NIL_PTR);
  	return;
  }

/* Copy last part of end_line if end_line is not tail */
  copy_string(bufp, (end_textp != NIL_PTR) ? end_textp : "\n");

/* Delete all lines between start and end_position (including end_line) */
  line = start_line->next;
  stop = end_line->next;
  while (line != stop && line != tail) {
  	line = line_delete(line);
  	line_cnt++;
  }

/* Check if last line of file should be deleted */
  if (end_textp == NIL_PTR && length_of(start_line->text) == 1 && nlines > 1) {
  	start_line = start_line->prev;
  	(void) line_delete(start_line->next);
  	line_cnt++;
  }
  else {	/* Install new text */
  	free_space(start_line->text);
  	start_line->text = alloc(length_of(text_buffer) + 1);
  	copy_string(start_line->text, text_buffer);
  }

/* Fix screen. First check if line is shifted. Perhaps we should shift it back*/
  if (get_shift(start_line->shift_count)) {
  	shift = (XBREAK - count_chars(start_line)) / SHIFT_SIZE;
  	if (shift > 0) {		/* Shift line `shift' back */
  		if (shift >= get_shift(start_line->shift_count))
  			start_line->shift_count = 0;
  		else
  			start_line->shift_count -= shift;
  		nx += shift * SHIFT_SIZE;/* Reset x value */
  	}
  }

  if (line_cnt == 0) {		    /* Check if only one line changed */
  	if (shift > 0) {	    /* Reprint whole line */
  		set_cursor(0, y);
  		line_print(start_line);
  	}
  	else {			    /* Just display last part of line */
  		set_cursor(x, y);
  		put_line(start_line, x, TRUE);
  	}
  	move_to(nx, y);	   /* Reset cur_text */
  	return;
  }

  shift = last_y;	   /* Save value */
  reset(top_line, y);
  display(0, y, start_line, shift - y);
  move_to((line_cnt == 1) ? nx : 0, y);
}

/*  ========================================================================  *
 *				Yank Commands				      *	
 *  ========================================================================  */

LINE *mark_line;			/* For marking position. */
char *mark_text;
int lines_saved;			/* Nr of lines in buffer */

/*
 * PT() inserts the buffer at the current location.
 */
void PT()
{
  register int fd;		/* File descriptor for buffer */

  if ((fd = scratch_file(READ)) == ERRORS)
  	error("Buffer is empty.", NIL_PTR);
  else {
  	file_insert(fd, FALSE);/* Insert the buffer */
  	(void) close(fd);
  }
}

/*
 * IF() prompt for a filename and inserts the file at the current location 
 * in the file.
 */
void IF()
{
  register int fd;		/* File descriptor of file */
  char name[LINE_LEN];		/* Buffer for file name */

/* Get the file name */
  if (get_file("Get and insert file:", name) != FINE)
  	return;
  
  if ((fd = open(name, 0)) < 0)
  	error("Cannot open ", name);
  else {
  	file_insert(fd, TRUE);	/* Insert the file */
  	(void) close(fd);
  }
}

/*
 * File_insert() inserts a an opened file (as given by filedescriptor fd)
 * at the current location.
 */
void file_insert(fd, old_pos)
int fd;
FLAG old_pos;
{
  char line_buffer[MAX_CHARS];		/* Buffer for next line */
  register LINE *line = cur_line;
  register int line_count = nlines;	/* Nr of lines inserted */
  LINE *page = cur_line;
  int ret = ERRORS;
  
/* Get the first piece of text (might be ended with a '\n') from fd */
  if (get_line(fd, line_buffer) == ERRORS)
  	return;				/* Empty file */

/* Insert this text at the current location. */
  if (insert(line, cur_text, line_buffer) == ERRORS)
  	return;

/* Repeat getting lines (and inserting lines) until EOF is reached */
  while ((ret = get_line(fd, line_buffer)) != ERRORS && ret != NO_LINE)
  	line = line_insert(line, line_buffer, ret);
  
  if (ret == NO_LINE) {		/* Last line read not ended by a '\n' */
  	line = line->next;
  	(void) insert(line, line->text, line_buffer);
  }

/* Calculate nr of lines added */
  line_count = nlines - line_count;

/* Fix the screen */
  if (line_count == 0) {		/* Only one line changed */
  	set_cursor(0, y);
  	line_print(line);
  	move_to((old_pos == TRUE) ? x : x + length_of(line_buffer), y);
  }
  else {				/* Several lines changed */
  	reset(top_line, y);	/* Reset pointers */
  	while (page != line && page != bot_line->next)
  		page = page->next;
  	if (page != bot_line->next || old_pos == TRUE)
  		display(0, y, cur_line, screenmax - y);
  	if (old_pos == TRUE)
  		move_to(x, y);
  	else if (ret == NO_LINE)
		move_to(length_of(line_buffer), find_y(line));
	else 
		move_to(0, find_y(line->next));
  }

/* If nr of added line >= REPORT, print the count */
  if (line_count >= REPORT)
  	status_line(num_out((long) line_count), " lines added.");
}

/*
 * WB() writes the buffer (yank_file) into another file, which
 * is prompted for.
 */
void WB()
{
  register int new_fd;		/* Filedescriptor to copy file */
  int yank_fd;			/* Filedescriptor to buffer */
  register int cnt;		/* Count check for read/write */
  int ret = 0;			/* Error check for write */
  char file[LINE_LEN];		/* Output file */
  
/* Checkout the buffer */
  if ((yank_fd = scratch_file(READ)) == ERRORS) {
  	error("Buffer is empty.", NIL_PTR);
  	return;
  }

/* Get file name */
  if (get_file("Write buffer to file:", file) != FINE)
  	return;
  
/* Creat the new file */
  if ((new_fd = creat(file, 0644)) < 0) {
  	error("Cannot create ", file);
  	return;
  }

  status_line("Writing ", file);
  
/* Copy buffer into file */
  while ((cnt = read(yank_fd, text_buffer, sizeof(text_buffer))) > 0)
  	if (write(new_fd, text_buffer, cnt) != cnt) {
  		bad_write(new_fd);
  		ret = ERRORS;
  		break;
  	}

/* Clean up open files and status_line */
  (void) close(new_fd);
  (void) close(yank_fd);

  if (ret != ERRORS)			/* Bad write */
  	file_status("Wrote", chars_saved, file, lines_saved, TRUE, FALSE);
}

/*
 * MA sets mark_line (mark_text) to the current line (text pointer). 
 */
void MA()
{
  mark_line = cur_line;
  mark_text = cur_text;
  status_line("Mark set", NIL_PTR);
}

/*
 * YA() puts the text between the marked position and the current
 * in the buffer.
 */
void YA()
{
  set_up(NO_DELETE);
}

/*
 * DT() is essentially the same as YA(), but in DT() the text is deleted.
 */
void DT()
{
  set_up(DELETE);
}

/*
 * Set_up is an interface to the actual yank. It calls checkmark () to check
 * if the marked position is still valid. If it is, yank is called with the
 * arguments in the right order.
 */
void set_up(remove)
FLAG remove;				/* DELETE if text should be deleted */
{
  switch (checkmark()) {
  	case NOT_VALID :
  		error("Mark not set.", NIL_PTR);
  		return;
  	case SMALLER :
  		yank(mark_line, mark_text, cur_line, cur_text, remove);
  		break;
  	case BIGGER :
  		yank(cur_line, cur_text, mark_line, mark_text, remove);
  		break;
  	case SAME :		/* Ignore stupid behaviour */
  		yank_status = EMPTY;
  		chars_saved = 0L;
  		status_line("0 characters saved in buffer.", NIL_PTR);
  		break;
  }
}

/*
 * Check_mark() checks if mark_line and mark_text are still valid pointers. If
 * they are it returns SMALLER if the marked position is before the current,
 * BIGGER if it isn't or SAME if somebody didn't get the point.
 * NOT_VALID is returned when mark_line and/or mark_text are no longer valid.
 * Legal() checks if mark_text is valid on the mark_line.
 */
FLAG checkmark()
{
  register LINE *line;
  FLAG cur_seen = FALSE;

/* Special case: check is mark_line and cur_line are the same. */
  if (mark_line == cur_line) {
  	if (mark_text == cur_text)	/* Even same place */
  		return SAME;
  	if (legal() == ERRORS)		/* mark_text out of range */
  		return NOT_VALID;
  	return (mark_text < cur_text) ? SMALLER : BIGGER;
  }

/* Start looking for mark_line in the line structure */
  for (line = header->next; line != tail; line = line->next) {
  	if (line == cur_line)
  		cur_seen = TRUE;
  	else if (line == mark_line)
  		break;
  }

/* If we found mark_line (line != tail) check for legality of mark_text */
  if (line == tail || legal() == ERRORS)
  	return NOT_VALID;

/* cur_seen is TRUE if cur_line is before mark_line */
  return (cur_seen == TRUE) ? BIGGER : SMALLER;
}

/*
 * Legal() checks if mark_text is still a valid pointer.
 */
int legal()
{
  register char *textp = mark_line->text;

/* Locate mark_text on mark_line */
  while (textp != mark_text && *textp++ != '\0')
  	;
  return (*textp == '\0') ? ERRORS : FINE;
}

/*
 * Yank puts all the text between start_position and end_position into
 * the buffer.
 * The caller must check that the arguments to yank() are valid. (E.g. in
 * the right order)
 */
void yank(start_line, start_textp, end_line, end_textp, remove)
LINE *start_line, *end_line;
char *start_textp, *end_textp;
FLAG remove;				/* DELETE if text should be deleted */
{
  register LINE *line = start_line;
  register char *textp = start_textp;
  int fd;

/* Creat file to hold buffer */
  if ((fd = scratch_file(WRITE)) == ERRORS)
  	return;
  
  chars_saved = 0L;
  lines_saved = 0;
  status_line("Saving text.", NIL_PTR);

/* Keep writing chars until the end_location is reached. */
  while (textp != end_textp) {
  	if (write_char(fd, *textp) == ERRORS) {
  		(void) close(fd);
  		return;
  	}
  	if (*textp++ == '\n') {	/* Move to the next line */
  		line = line->next;
  		textp = line->text;
  		lines_saved++;
  	}
  	chars_saved++;
  }

/* Flush the I/O buffer and close file */
  if (flush_buffer(fd) == ERRORS) {
  	(void) close(fd);
  	return;
  }
  (void) close(fd);
  yank_status = VALID;

/*
 * Check if the text should be deleted as well. If it should, the following
 * hack is used to save a lot of code. First move back to the start_position.
 * (This might be the location we're on now!) and them delete the text.
 * It might be a bit confusing the first time somebody uses it.
 * Delete() will fix the screen.
 */
  if (remove == DELETE) {
  	move_to(find_x(start_line, start_textp), find_y(start_line));
  	delete(start_line, start_textp, end_line, end_textp);
  }

  status_line(num_out(chars_saved), " characters saved in buffer.");
}

/*
 * Scratch_file() creates a uniq file in /usr/tmp. If the file couldn't
 * be created other combinations of files are tried until a maximum
 * of MAXTRAILS times. After MAXTRAILS times, an error message is given
 * and ERRORS is returned.
 */

#define MAXTRAILS	26

int scratch_file(mode)
FLAG mode;				/* Can be READ or WRITE permission */
{
  static int trials = 0;		/* Keep track of trails */
  register char *y_ptr, *n_ptr;
  int fd;				/* Filedescriptor to buffer */

/* If yank_status == NOT_VALID, scratch_file is called for the first time */
  if (yank_status == NOT_VALID && mode == WRITE) { /* Create new file */
  	/* Generate file name. */
	y_ptr = &yank_file[11];
	n_ptr = num_out((long) getpid());
	while ((*y_ptr = *n_ptr++) != '\0')
		y_ptr++;
	*y_ptr++ = 'a' + trials;
	*y_ptr = '\0';
  	/* Check file existence */
  	if (access(yank_file, 0) == 0 || (fd = creat(yank_file, 0644)) < 0) {
  		if (trials++ >= MAXTRAILS) {
  			error("Unable to creat scratchfile.", NIL_PTR);
  			return ERRORS;
  		}
  		else
  			return scratch_file(mode);/* Have another go */
  	}
  }
  else if ((mode == READ && (fd = open(yank_file, 0)) < 0) ||
			(mode == WRITE && (fd = creat(yank_file, 0644)) < 0)) {
  	yank_status = NOT_VALID;
  	return ERRORS;
  }

  clear_buffer();
  return fd;
}

/*  ========================================================================  *
 *				Search Routines				      *	
 *  ========================================================================  */

/*
 * A regular expression consists of a sequence of:
 * 	1. A normal character matching that character.
 * 	2. A . matching any character.
 * 	3. A ^ matching the begin of a line.
 * 	4. A $ (as last character of the pattern) mathing the end of a line.
 * 	5. A \<character> matching <character>.
 * 	6. A number of characters enclosed in [] pairs matching any of these
 * 	   characters. A list of characters can be indicated by a '-'. So
 * 	   [a-z] matches any letter of the alphabet. If the first character
 * 	   after the '[' is a '^' then the set is negated (matching none of
 * 	   the characters). 
 * 	   A ']', '^' or '-' can be escaped by putting a '\' in front of it.
 * 	7. If one of the expressions as described in 1-6 is followed by a
 * 	   '*' than that expressions matches a sequence of 0 or more of
 * 	   that expression.
 */

char typed_expression[LINE_LEN];	/* Holds previous expr. */

/*
 * SF searches forward for an expression.
 */
void SF()
{
  search("Search forward:", FORWARD);
}

/*
 * SF searches backwards for an expression.
 */
void SR()
{
  search("Search reverse:", REVERSE);
}

/*
 * Get_expression() prompts for an expression. If just a return is typed, the
 * old expression is used. If the expression changed, compile() is called and
 * the returning REGEX structure is returned. It returns NIL_REG upon error.
 * The save flag indicates whether the expression should be appended at the
 * message pointer.
 */
REGEX *get_expression(message)
char *message;
{
  static REGEX program;			/* Program of expression */
  char exp_buf[LINE_LEN];			/* Buffer for new expr. */

  if (get_string(message, exp_buf, FALSE) == ERRORS)
  	return NIL_REG;
  
  if (exp_buf[0] == '\0' && typed_expression[0] == '\0') {
  	error("No previous expression.", NIL_PTR);
  	return NIL_REG;
  }

  if (exp_buf[0] != '\0') {		/* A new expr. is typed */
  	copy_string(typed_expression, exp_buf);/* Save expr. */
  	compile(exp_buf, &program);	/* Compile new expression */
  }

  if (program.status == REG_ERROR) {	/* Error during compiling */
  	error(program.result.err_mess, NIL_PTR);
  	return NIL_REG;
  }
  return &program;
}

/*
 * GR() a replaces all matches from the current position until the end
 * of the file.
 */
void GR()
{
  change("Global replace:", VALID);
}

/*
 * LR() replaces all matches on the current line.
 */
void LR()
{
  change("Line replace:", NOT_VALID);
}

/*
 * Change() prompts for an expression and a substitution pattern and changes
 * all matches of the expression into the substitution. change() start looking
 * for expressions at the current line and continues until the end of the file
 * if the FLAG file is VALID.
 */
void change(message, file)
char *message;				/* Message to prompt for expression */
FLAG file;
{
  char mess_buf[LINE_LEN];	/* Buffer to hold message */
  char replacement[LINE_LEN];	/* Buffer to hold subst. pattern */
  REGEX *program;			/* Program resulting from compilation */
  register LINE *line = cur_line;
  register char *textp;
  long lines = 0L;		/* Nr of lines on which subs occurred */
  long subs = 0L;			/* Nr of subs made */
  int page = y;			/* Index to check if line is on screen*/

/* Save message and get expression */
  copy_string(mess_buf, message);
  if ((program = get_expression(mess_buf)) == NIL_REG)
  	return;
  
/* Get substitution pattern */
  build_string(mess_buf, "%s %s by:", mess_buf, typed_expression);
  if (get_string(mess_buf, replacement, FALSE) == ERRORS)
  	return;
  
  set_cursor(0, ymax);
  flush();
/* Substitute until end of file */
  do {
  	if (line_check(program, line->text, FORWARD)) {
  		lines++;
  		/* Repeat sub. on this line as long as we find a match*/
  		do {
  			subs++;	/* Increment subs */
  			if ((textp = substitute(line, program,replacement))
								     == NIL_PTR)
  				return;	/* Line too long */
  		} while ((program->status & BEGIN_LINE) != BEGIN_LINE &&
			 (program->status & END_LINE) != END_LINE &&
					  line_check(program, textp, FORWARD));
  		/* Check to see if we can print the result */
  		if (page <= screenmax) {
  			set_cursor(0, page);
  			line_print(line);
  		}
  	}
  	if (page <= screenmax)
  		page++;
  	line = line->next;
  } while (line != tail && file == VALID && quit == FALSE);

  copy_string(mess_buf, (quit == TRUE) ? "(Aborted) " : "");
/* Fix the status line */
  if (subs == 0L && quit == FALSE)
  	error("Pattern not found.", NIL_PTR);
  else if (lines >= REPORT || quit == TRUE) {
  	build_string(mess_buf, "%s %D substitutions on %D lines.", mess_buf,
								   subs, lines);
  	status_line(mess_buf, NIL_PTR);
  }
  else if (file == NOT_VALID && subs >= REPORT)
  	status_line(num_out(subs), " substitutions.");
  else
  	clear_status();
  move_to (x, y);
}

/*
 * Substitute() replaces the match on this line by the substitute pattern
 * as indicated by the program. Every '&' in the replacement is replaced by 
 * the original match. A \ in the replacement escapes the next character.
 */
char *substitute(line, program, replacement)
LINE *line;
REGEX *program;
char *replacement;		/* Contains replacement pattern */
{
  register char *textp = text_buffer;
  register char *subp = replacement;
  char *linep = line->text;
  char *amp;

  modified = TRUE;

/* Copy part of line until the beginning of the match */
  while (linep != program->start_ptr)
  	*textp++ = *linep++;
  
/*
 * Replace the match by the substitution pattern. Each occurrence of '&' is
 * replaced by the original match. A \ escapes the next character.
 */
  while (*subp != '\0' && textp < &text_buffer[MAX_CHARS]) {
  	if (*subp == '&') {		/* Replace the original match */
  		amp = program->start_ptr;
  		while (amp < program->end_ptr && textp<&text_buffer[MAX_CHARS])
  			*textp++ = *amp++;
  		subp++;
  	}
  	else {
  		if (*subp == '\\' && *(subp + 1) != '\0')
  			subp++;
  		*textp++ = *subp++;
  	}
  }

/* Check for line length not exceeding MAX_CHARS */
  if (length_of(text_buffer) + length_of(program->end_ptr) >= MAX_CHARS) {
  	error("Substitution result: line too big", NIL_PTR);
  	return NIL_PTR;
  }

/* Append last part of line to the new build line */
  copy_string(textp, program->end_ptr);

/* Free old line and install new one */
  free_space(line->text);
  line->text = alloc(length_of(text_buffer) + 1);
  copy_string(line->text, text_buffer);

  return(line->text + (textp - text_buffer));
}

/*
 * Search() calls get_expression to fetch the expression. If this went well,
 * the function match() is called which returns the line with the next match.
 * If this line is the NIL_LINE, it means that a match could not be found.
 * Find_x() and find_y() display the right page on the screen, and return
 * the right coordinates for x and y. These coordinates are passed to move_to()
 */
void search(message, method)
char *message;
FLAG method;
{
  register REGEX *program;
  register LINE *match_line;

/* Get the expression */
  if ((program = get_expression(message)) == NIL_REG)
  	return;

  set_cursor(0, ymax);
  flush();
/* Find the match */
  if ((match_line = match(program, cur_text, method)) == NIL_LINE) {
  	if (quit == TRUE)
  		status_line("Aborted", NIL_PTR);
  	else
  		status_line("Pattern not found.", NIL_PTR);
  	return;
  }

  move(0, program->start_ptr, find_y(match_line));
  clear_status();
}

/*
 * find_y() checks if the matched line is on the current page.  If it is, it
 * returns the new y coordinate, else it displays the correct page with the
 * matched line in the middle and returns the new y value;
 */
int find_y(match_line)
LINE *match_line;
{
  register LINE *line;
  register int count = 0;

/* Check if match_line is on the same page as currently displayed. */
  for (line = top_line; line != match_line && line != bot_line->next;
  						      line = line->next)
  	count++;
  if (line != bot_line->next)
  	return count;

/* Display new page, with match_line in center. */
  if ((line = proceed(match_line, -(screenmax >> 1))) == header) {
  /* Can't display in the middle. Make first line of file top_line */
  	count = 0;
  	for (line = header->next; line != match_line; line = line->next)
  		count++;
  	line = header->next;
  }
  else	/* New page is displayed. Set cursor to middle of page */
  	count = screenmax >> 1;

/* Reset pointers and redraw the screen */
  reset(line, 0);
  RD();

  return count;
}

/* Opcodes for characters */
#define	NORMAL		0x0200
#define DOT		0x0400
#define EOLN		0x0800
#define STAR		0x1000
#define BRACKET		0x2000
#define NEGATE		0x0100
#define DONE		0x4000

/* Mask for opcodes and characters */
#define LOW_BYTE	0x00FF
#define HIGH_BYTE	0xFF00

/* Previous is the contents of the previous address (ptr) points to */
#define previous(ptr)		(*((ptr) - 1))

/* Buffer to store outcome of compilation */
int exp_buffer[BLOCK_SIZE];

/* Errors often used */
char *too_long = "Regular expression too long";

/*
 * Reg_error() is called by compile() is something went wrong. It set the
 * status of the structure to error, and assigns the error field of the union.
 */
#define reg_error(str)	program->status = REG_ERROR, \
  					program->result.err_mess = (str)
/*
 * Finished() is called when everything went right during compilation. It
 * allocates space for the expression, and copies the expression buffer into
 * this field.
 */
void finished(program, last_exp)
register REGEX *program;
int *last_exp;
{
  register int length = (last_exp - exp_buffer) * sizeof(int);

/* Allocate space */
  program->result.expression = (int *) alloc(length);
/* Copy expression. (expression consists of ints!) */
  bcopy(exp_buffer, program->result.expression, length);
}

/*
 * Compile compiles the pattern into a more comprehensible form and returns a 
 * REGEX structure. If something went wrong, the status field of the structure
 * is set to REG_ERROR and an error message is set into the err_mess field of
 * the union. If all went well the expression is saved and the expression
 * pointer is set to the saved (and compiled) expression.
 */
void compile(pattern, program)
register char *pattern;			/* Pointer to pattern */
REGEX *program;
{
  register int *expression = exp_buffer;
  int *prev_char;			/* Pointer to previous compiled atom */
  int *acct_field;		/* Pointer to last BRACKET start */
  FLAG negate;			/* Negate flag for BRACKET */
  char low_char;			/* Index for chars in BRACKET */
  char c;

/* Check for begin of line */
  if (*pattern == '^') {
  	program->status = BEGIN_LINE;
  	pattern++;
  }
  else {
  	program->status = 0;
/* If the first character is a '*' we have to assign it here. */
  	if (*pattern == '*') {
  		*expression++ = '*' + NORMAL;
  		pattern++;
  	}
  }

  for (; ;) {
  	switch (c = *pattern++) {
  	case '.' :
  		*expression++ = DOT;
  		break;
  	case '$' :
  		/*
  		 * Only means EOLN if it is the last char of the pattern
  		 */
  		if (*pattern == '\0') {
  			*expression++ = EOLN | DONE;
  			program->status |= END_LINE;
  			finished(program, expression);
  			return;
  		}
  		else
  			*expression++ = NORMAL + '$';
  		break;
  	case '\0' :
  		*expression++ = DONE;
  		finished(program, expression);
  		return;
  	case '\\' :
  		/* If last char, it must! mean a normal '\' */
  		if (*pattern == '\0')
  			*expression++ = NORMAL + '\\';
  		else
  			*expression++ = NORMAL + *pattern++;
  		break;
  	case '*' :
  		/*
  		 * If the previous expression was a [] find out the
  		 * begin of the list, and adjust the opcode.
  		 */
  		prev_char = expression - 1;
  		if (*prev_char & BRACKET)
  			*(expression - (*acct_field & LOW_BYTE))|= STAR;
  		else
  			*prev_char |= STAR;
  		break;
  	case '[' :
  		/*
  		 * First field in expression gives information about
  		 * the list.
  		 * The opcode consists of BRACKET and if necessary
  		 * NEGATE to indicate that the list should be negated
  		 * and/or STAR to indicate a number of sequence of this 
  		 * list.
  		 * The lower byte contains the length of the list.
  		 */
  		acct_field = expression++;
  		if (*pattern == '^') {	/* List must be negated */
  			pattern++;
  			negate = TRUE;
  		}
  		else
  			negate = FALSE;
  		while (*pattern != ']') {
  			if (*pattern == '\0') {
  				reg_error("Missing ]");
  				return;
  			}
  			if (*pattern == '\\')
  				pattern++;
  			*expression++ = *pattern++;
  			if (*pattern == '-') {
  						/* Make list of chars */
  				low_char = previous(pattern);
  				pattern++;	/* Skip '-' */
  				if (low_char++ > *pattern) {
  					reg_error("Bad range in [a-z]");
  					return;
  				}
  				/* Build list */
  				while (low_char <= *pattern)
  					*expression++ = low_char++;
  				pattern++;
  			}
  			if (expression >= &exp_buffer[BLOCK_SIZE]) {
  				reg_error(too_long);
  				return;
  			}
  		}
  		pattern++;			/* Skip ']' */
  		/* Assign length of list in acct field */
  		if ((*acct_field = (expression - acct_field)) == 1) {
  			reg_error("Empty []");
  			return;
  		}
  		/* Assign negate and bracket field */
  		*acct_field |= BRACKET;
  		if (negate == TRUE)
  			*acct_field |= NEGATE;
  		/*
  		 * Add BRACKET to opcode of last char in field because
  		 * a '*' may be following the list.
  		 */
  		previous(expression) |= BRACKET;
  		break;
  	default :
  		*expression++ = c + NORMAL;
  	}
  	if (expression == &exp_buffer[BLOCK_SIZE]) {
  		reg_error(too_long);
  		return;
  	}
  }
  /* NOTREACHED */
}

/*
 * Match gets as argument the program, pointer to place in current line to 
 * start from and the method to search for (either FORWARD or REVERSE).
 * Match() will look through the whole file until a match is found.
 * NIL_LINE is returned if no match could be found.
 */
LINE *match(program, string, method)
REGEX *program;
char *string;
register FLAG method;
{
  register LINE *line = cur_line;
  char old_char;				/* For saving chars */

/* Corrupted program */
  if (program->status == REG_ERROR)
  	return NIL_LINE;

/* Check part of text first */
  if (!(program->status & BEGIN_LINE)) {
  	if (method == FORWARD) {
  		if (line_check(program, string + 1, method) == MATCH)
  			return cur_line;	/* Match found */
  	}
  	else if (!(program->status & END_LINE)) {
  		old_char = *string;	/* Save char and */
  		*string = '\n';		/* Assign '\n' for line_check */
  		if (line_check(program, line->text, method) == MATCH) {
  			*string = old_char; /* Restore char */
  			return cur_line;    /* Found match */
  		}
  		*string = old_char;	/* No match, but restore char */
  	}
  }

/* No match in last (or first) part of line. Check out rest of file */
  do {
  	line = (method == FORWARD) ? line->next : line->prev;
  	if (line->text == NIL_PTR)	/* Header/tail */
  		continue;
  	if (line_check(program, line->text, method) == MATCH)
  		return line;
  } while (line != cur_line && quit == FALSE);

/* No match found. */
  return NIL_LINE;
}

/*
 * Line_check() checks the line (or rather string) for a match. Method
 * indicates FORWARD or REVERSE search. It scans through the whole string
 * until a match is found, or the end of the string is reached.
 */
int line_check(program, string, method)
register REGEX *program;
char *string;
FLAG method;
{
  register char *textp = string;

/* Assign start_ptr field. We might find a match right away! */
  program->start_ptr = textp;

/* If the match must be anchored, just check the string. */
  if (program->status & BEGIN_LINE)
  	return check_string(program, string, NIL_INT);
  
  if (method == REVERSE) {
  	/* First move to the end of the string */
  	for (textp = string; *textp != '\n'; textp++)
  		;
  	/* Start checking string until the begin of the string is met */
  	while (textp >= string) {
  		program->start_ptr = textp;
  		if (check_string(program, textp--, NIL_INT))
  			return MATCH;
  	}
  }
  else {
  	/* Move through the string until the end of is found */
	while (quit == FALSE && *textp != '\0') {
  		program->start_ptr = textp;
  		if (check_string(program, textp, NIL_INT))
  			return MATCH;
		if (*textp == '\n')
			break;
		textp++;
  	}
  }

  return NO_MATCH;
}

/*
 * Check() checks of a match can be found in the given string. Whenever a STAR
 * is found during matching, then the begin position of the string is marked
 * and the maximum number of matches is performed. Then the function star()
 * is called which starts to finish the match from this position of the string
 * (and expression). Check() return MATCH for a match, NO_MATCH is the string 
 * couldn't be matched or REG_ERROR for an illegal opcode in expression.
 */
int check_string(program, string, expression)
REGEX *program;
register char *string;
int *expression;
{
  register int opcode;		/* Holds opcode of next expr. atom */
  char c;				/* Char that must be matched */
  char *mark;			/* For marking position */
  int star_fl;			/* A star has been born */

  if (expression == NIL_INT)
  	expression = program->result.expression;

/* Loop until end of string or end of expression */
  while (quit == FALSE && !(*expression & DONE) &&
					   *string != '\0' && *string != '\n') {
  	c = *expression & LOW_BYTE;	  /* Extract match char */
  	opcode = *expression & HIGH_BYTE; /* Extract opcode */
  	if (star_fl = (opcode & STAR)) {  /* Check star occurrence */
  		opcode &= ~STAR;	  /* Strip opcode */
  		mark = string;		  /* Mark current position */
  	}
  	expression++;		/* Increment expr. */
  	switch (opcode) {
  	case NORMAL :
  		if (star_fl)
  			while (*string++ == c)	/* Skip all matches */
  				;
  		else if (*string++ != c)
  			return NO_MATCH;
  		break;
  	case DOT :
  		string++;
  		if (star_fl)			/* Skip to eoln */
  			while (*string != '\0' && *string++ != '\n')
  				;
  		break;
  	case NEGATE | BRACKET:
  	case BRACKET :
  		if (star_fl)
  			while (in_list(expression, *string++, c, opcode)
								       == MATCH)
  				;
  		else if (in_list(expression, *string++, c, opcode) == NO_MATCH)
  			return NO_MATCH;
  		expression += c - 1;	/* Add length of list */
  		break;
  	default :
  		panic("Corrupted program in check_string()");
  	}
  	if (star_fl) 
  		return star(program, mark, string, expression);
  }
  if (*expression & DONE) {
  	program->end_ptr = string;	/* Match ends here */
  	/*
  	 * We might have found a match. The last thing to do is check
  	 * whether a '$' was given at the end of the expression, or
  	 * the match was found on a null string. (E.g. [a-z]* always
  	 * matches) unless a ^ or $ was included in the pattern.
  	 */
  	if ((*expression & EOLN) && *string != '\n' && *string != '\0')
  		return NO_MATCH;
	if (string == program->start_ptr && !(program->status & BEGIN_LINE)
					 && !(*expression & EOLN))
  		return NO_MATCH;
  	return MATCH;
  }
  return NO_MATCH;
}

/*
 * Star() calls check_string() to find out the longest match possible.
 * It searches backwards until the (in check_string()) marked position
 * is reached, or a match is found.
 */
int star(program, end_position, string, expression)
REGEX *program;
register char *end_position;
register char *string;
int *expression;
{
  do {
  	string--;
  	if (check_string(program, string, expression))
  		return MATCH;
  } while (string != end_position);

  return NO_MATCH;
}

/*
 * In_list() checks if the given character is in the list of []. If it is
 * it returns MATCH. if it isn't it returns NO_MATCH. These returns values
 * are reversed when the NEGATE field in the opcode is present.
 */
int in_list(list, c, list_length, opcode)
register int *list;
char c;
register int list_length;
int opcode;
{
  if (c == '\0' || c == '\n')	/* End of string, never matches */
  	return NO_MATCH;
  while (list_length-- > 1) {	/* > 1, don't check acct_field */
  	if ((*list & LOW_BYTE) == c)
  		return (opcode & NEGATE) ? NO_MATCH : MATCH;
  	list++;
  }
  return (opcode & NEGATE) ? MATCH : NO_MATCH;
}

/*
 * Dummy_line() adds an empty line at the end of the file. This is sometimes
 * useful in combination with the EF and DN command in combination with the
 * Yank command set.
 */
void dummy_line()
{
	(void) line_insert(tail->prev, "\n", 1);
	tail->prev->shift_count = DUMMY;
	if (last_y != screenmax) {
		last_y++;
		bot_line = bot_line->next;
	}
}
