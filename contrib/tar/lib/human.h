#ifndef HUMAN_H_
# define HUMAN_H_ 1

# if HAVE_CONFIG_H
#  include <config.h>
# endif

# if HAVE_INTTYPES_H
#  include <inttypes.h>
# endif

/* A conservative bound on the maximum length of a human-readable string.
   The output can be the product of the largest uintmax_t and the largest int,
   so add their sizes before converting to a bound on digits.  */
# define LONGEST_HUMAN_READABLE ((sizeof (uintmax_t) + sizeof (int)) \
				 * CHAR_BIT / 3)

# ifndef PARAMS
#  if defined PROTOTYPES || (defined __STDC__ && __STDC__)
#   define PARAMS(Args) Args
#  else
#   define PARAMS(Args) ()
#  endif
# endif

enum human_inexact_style
{
  human_floor = -1,
  human_round_to_even = 0,
  human_ceiling = 1
};

char *human_readable PARAMS ((uintmax_t, char *, int, int));
char *human_readable_inexact PARAMS ((uintmax_t, char *, int, int,
				      enum human_inexact_style));

void human_block_size PARAMS ((char const *, int, int *));

#endif /* HUMAN_H_ */
