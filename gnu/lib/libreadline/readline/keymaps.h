/* keymaps.h -- Manipulation of readline keymaps. */

#ifndef _KEYMAPS_H_
#define _KEYMAPS_H_

#include <readline/chardefs.h>

#ifndef __FUNCTION_DEF
typedef int Function ();
#define __FUNCTION_DEF
#endif

/* A keymap contains one entry for each key in the ASCII set.
   Each entry consists of a type and a pointer.
   POINTER is the address of a function to run, or the
   address of a keymap to indirect through.
   TYPE says which kind of thing POINTER is. */
typedef struct _keymap_entry {
  char type;
  Function *function;
} KEYMAP_ENTRY;

/* I wanted to make the above structure contain a union of:
   union { Function *function; struct _keymap_entry *keymap; } value;
   but this made it impossible for me to create a static array.
   Maybe I need C lessons. */

typedef KEYMAP_ENTRY KEYMAP_ENTRY_ARRAY[128];
typedef KEYMAP_ENTRY *Keymap;

/* The values that TYPE can have in a keymap entry. */
#define ISFUNC 0
#define ISKMAP 1
#define ISMACR 2

extern KEYMAP_ENTRY_ARRAY emacs_standard_keymap, emacs_meta_keymap, emacs_ctlx_keymap;
extern KEYMAP_ENTRY_ARRAY vi_insertion_keymap, vi_movement_keymap;

/* Return a new, empty keymap.
   Free it with free() when you are done. */
Keymap rl_make_bare_keymap ();

/* Return a new keymap which is a copy of MAP. */
Keymap rl_copy_keymap ();

/* Return a new keymap with the printing characters bound to rl_insert,
   the lowercase Meta characters bound to run their equivalents, and
   the Meta digits bound to produce numeric arguments. */
Keymap rl_make_keymap ();

#endif /* _KEYMAPS_H_ */
