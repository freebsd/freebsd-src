/* chardefs.h -- Character definitions for readline. */
#ifndef _CHARDEFS_

#ifndef savestring
#define savestring(x) (char *)strcpy (xmalloc (1 + strlen (x)), (x))
#endif

#ifndef whitespace
#define whitespace(c) (((c) == ' ') || ((c) == '\t'))
#endif

#ifdef CTRL
#undef CTRL
#endif

/* Some character stuff. */
#define control_character_threshold 0x020   /* smaller than this is control */
#define meta_character_threshold 0x07f	    /* larger than this is Meta. */
#define control_character_bit 0x40	    /* 0x000000, must be off. */
#define meta_character_bit 0x080	    /* x0000000, must be on. */

#define CTRL(c) ((c) & (~control_character_bit))
#define META(c) ((c) | meta_character_bit)

#define UNMETA(c) ((c) & (~meta_character_bit))
#define UNCTRL(c) to_upper(((c)|control_character_bit))

#define lowercase_p(c) (((c) > ('a' - 1) && (c) < ('z' + 1)))
#define uppercase_p(c) (((c) > ('A' - 1) && (c) < ('Z' + 1)))

#define pure_alphabetic(c) (lowercase_p(c) || uppercase_p(c))

#ifndef to_upper
#define to_upper(c) (lowercase_p(c) ? ((c) - 32) : (c))
#define to_lower(c) (uppercase_p(c) ? ((c) + 32) : (c))
#endif

#define CTRL_P(c) ((c) < control_character_threshold)
#define META_P(c) ((c) > meta_character_threshold)

#define NEWLINE '\n'
#define RETURN CTRL('M')
#define RUBOUT 0x07f
#define TAB '\t'
#define ABORT_CHAR CTRL('G')
#define PAGE CTRL('L')
#define SPACE 0x020
#define ESC CTRL('[')

#endif  /* _CHARDEFS_ */
