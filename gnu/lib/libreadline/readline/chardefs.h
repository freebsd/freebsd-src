/* chardefs.h -- Character definitions for readline. */
#ifndef _CHARDEFS_
#define _CHARDEFS_

#include <ctype.h>
#include <string.h>

#ifndef savestring
extern char *xmalloc ();
#define savestring(x) strcpy (xmalloc (1 + strlen (x)), (x))
#endif

#ifndef whitespace
#define whitespace(c) (((c) == ' ') || ((c) == '\t'))
#endif

#ifdef CTRL
#undef CTRL
#endif

/* Some character stuff. */
#define control_character_threshold 0x020   /* Smaller than this is control. */
#define meta_character_threshold 0x07f	    /* Larger than this is Meta. */
#define control_character_bit 0x40	    /* 0x000000, must be off. */
#define meta_character_bit 0x080	    /* x0000000, must be on. */
#define largest_char 255		    /* Largest character value. */

#define META_CHAR(c) ((c) > meta_character_threshold && (c) <= largest_char)
#define CTRL(c) ((c) & (~control_character_bit))
#define META(c) ((c) | meta_character_bit)

#define UNMETA(c) ((c) & (~meta_character_bit))
#define UNCTRL(c) to_upper(((c)|control_character_bit))

#define lowercase_p(c) islower(c)
#define uppercase_p(c) isupper(c)

#define pure_alphabetic(c) isalpha(c)

#ifndef to_upper
#define to_upper(c) toupper(c)
#define to_lower(c) tolower(c)
#endif

#define CTRL_P(c) ((c) < control_character_threshold)
#define META_P(c) ((c) > meta_character_threshold)

#ifndef digit_value
#define digit_value(x) ((x) - '0')
#endif

#ifndef NEWLINE
#define NEWLINE '\n'
#endif

#ifndef RETURN
#define RETURN CTRL('M')
#endif

#ifndef RUBOUT
#define RUBOUT 0x7f
#endif

#ifndef TAB
#define TAB '\t'
#endif

#ifdef ABORT_CHAR
#undef ABORT_CHAR
#endif
#define ABORT_CHAR CTRL('G')

#ifdef PAGE
#undef PAGE
#endif
#define PAGE CTRL('L')

#ifdef SPACE
#undef SPACE
#endif
#define SPACE 0x20

#ifdef ESC
#undef ESC
#endif

#define ESC CTRL('[')

#endif  /* _CHARDEFS_ */
