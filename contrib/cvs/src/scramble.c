/*
 * Trivially encode strings to protect them from innocent eyes (i.e.,
 * inadvertent password compromises, like a network administrator
 * who's watching packets for legitimate reasons and accidentally sees
 * the password protocol go by).
 * 
 * This is NOT secure encryption.
 *
 * It would be tempting to encode the password according to username
 * and repository, so that the same password would encode to a
 * different string when used with different usernames and/or
 * repositories.  However, then users would not be able to cut and
 * paste passwords around.  They're not supposed to anyway, but we all
 * know they will, and there's no reason to make it harder for them if
 * we're not trying to provide real security anyway.
 */

/* Set this to test as a standalone program. */
/* #define DIAGNOSTIC */

#ifndef DIAGNOSTIC
#include "cvs.h"
#else /* ! DIAGNOSTIC */
/* cvs.h won't define this for us */
#define AUTH_CLIENT_SUPPORT
#define xmalloc malloc
/* Use "gcc -fwritable-strings". */
#include <stdio.h>
#include <stdio.h>
#include <string.h>
#endif /* ! DIAGNOSTIC */

#if defined(AUTH_CLIENT_SUPPORT) || defined(AUTH_SERVER_SUPPORT)

/* Map characters to each other randomly and symmetrically, A <--> B.
 *
 * We divide the ASCII character set into 3 domains: control chars (0
 * thru 31), printing chars (32 through 126), and "meta"-chars (127
 * through 255).  The control chars map _to_ themselves, the printing
 * chars map _among_ themselves, and the meta chars map _among_
 * themselves.  Why is this thus?
 *
 * No character in any of these domains maps to a character in another
 * domain, because I'm not sure what characters are legal in
 * passwords, or what tools people are likely to use to cut and paste
 * them.  It seems prudent not to introduce control or meta chars,
 * unless the user introduced them first.  And having the control
 * chars all map to themselves insures that newline and
 * carriage-return are safely handled.
 */

static unsigned char
shifts[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 114, 120,
53, 79, 96, 109, 72, 108, 70, 64, 76, 67, 116, 74, 68, 87, 111, 52,
75, 119, 49, 34, 82, 81, 95, 65, 112, 86, 118, 110, 122, 105, 41, 57,
83, 43, 46, 102, 40, 89, 38, 103, 45, 50, 42, 123, 91, 35, 125, 55,
54, 66, 124, 126, 59, 47, 92, 71, 115, 78, 88, 107, 106, 56, 36, 121,
117, 104, 101, 100, 69, 73, 99, 63, 94, 93, 39, 37, 61, 48, 58, 113,
32, 90, 44, 98, 60, 51, 33, 97, 62, 77, 84, 80, 85, 223, 225, 216,
187, 166, 229, 189, 222, 188, 141, 249, 148, 200, 184, 136, 248, 190,
199, 170, 181, 204, 138, 232, 218, 183, 255, 234, 220, 247, 213, 203,
226, 193, 174, 172, 228, 252, 217, 201, 131, 230, 197, 211, 145, 238,
161, 179, 160, 212, 207, 221, 254, 173, 202, 146, 224, 151, 140, 196,
205, 130, 135, 133, 143, 246, 192, 159, 244, 239, 185, 168, 215, 144,
139, 165, 180, 157, 147, 186, 214, 176, 227, 231, 219, 169, 175, 156,
206, 198, 129, 164, 150, 210, 154, 177, 134, 127, 182, 128, 158, 208,
162, 132, 167, 209, 149, 241, 153, 251, 237, 236, 171, 195, 243, 233,
253, 240, 194, 250, 191, 155, 142, 137, 245, 235, 163, 242, 178, 152 };


/* SCRAMBLE and DESCRAMBLE work like this:
 *
 * scramble(STR) returns SCRM, a scrambled copy of STR.  SCRM[0] is a
 * single letter indicating the scrambling method.  As of this
 * writing, the only legal method is 'A', but check the code for more
 * up-to-date information.  The copy will have been allocated with
 * malloc(). 
 *
 * descramble(SCRM) returns STR, again in its own malloc'd space.
 * descramble() uses SCRM[0] to determine which method of unscrambling
 * to use.  If it does not recognize the method, it dies with error.
 */

/* Return a malloc'd, scrambled version of STR. */
char *
scramble (str)
     char *str;
{
  int i;
  char *s;

  /* +2 to hold the 'A' prefix that indicates which version of
   * scrambling this is (the first, obviously, since we only do one
   * kind of scrambling so far), and then the '\0' of course.
   */
  s = (char *) xmalloc (strlen (str) + 2);

  s[0] = 'A';   /* Scramble (TM) version prefix. */
  strcpy (s + 1, str);

  for (i = 1; s[i]; i++)
    s[i] = shifts[(unsigned char)(s[i])];

  return s;
}

/* Decode the string in place. */
char *
descramble (str)
     char *str;
{
  char *s;
  int i;

  /* For now we can only handle one kind of scrambling.  In the future
   * there may be other kinds, and this `if' will become a `switch'.
   */
  if (str[0] != 'A')
#ifndef DIAGNOSTIC
    error (1, 0, "descramble: unknown scrambling method");
#else  /* DIAGNOSTIC */
  {
    fprintf (stderr, "descramble: unknown scrambling method\n", str);
    fflush (stderr);
    exit (EXIT_FAILURE);
  }
#endif  /* DIAGNOSTIC */

  /* Method `A' is symmetrical, so scramble again to decrypt. */
  s = scramble (str + 1);

  /* Shift the whole string one char to the left, pushing the unwanted
     'A' off the left end.  Safe, because s is null-terminated. */
  for (i = 0; s[i]; i++)
      s[i] = s[i + 1];

  return s;
}

#endif /* (AUTH_CLIENT_SUPPORT || AUTH_SERVER_SUPPORT) from top of file */

#ifdef DIAGNOSTIC
int
main ()
{
  int i;
  char *e, *m, biggie[256];

  char *cleartexts[5];
  cleartexts[0] = "first";
  cleartexts[1] = "the second";
  cleartexts[2] = "this is the third";
  cleartexts[3] = "$#% !!\\3";
  cleartexts[4] = biggie;
  
  /* Set up the most important test string: */
  /* Can't have a real ASCII zero in the string, because we want to
     use printf, so we substitute the character zero. */
  biggie[0] = '0';
  /* The rest of the string gets straight ascending ASCII. */
  for (i = 1; i < 256; i++)
    biggie[i] = i;

  /* Test all the strings. */
  for (i = 0; i < 5; i++)
    {
      printf ("clear%d: %s\n", i, cleartexts[i]);
      e = scramble (cleartexts[i]);
      printf ("scram%d: %s\n", i, e);
      m = descramble (e);
      free (e);
      printf ("clear%d: %s\n\n", i, m);
      free (m);
    }

  fflush (stdout);
  return 0;
}
#endif /* DIAGNOSTIC */

/*
 * ;;; The Emacs Lisp that did the dirty work ;;;
 * (progn
 * 
 *   ;; Helper func.
 *   (defun random-elt (lst)
 *     (let* ((len (length lst))
 *            (rnd (random len)))
 *       (nth rnd lst)))
 * 
 *   ;; A list of all characters under 127, each appearing once.
 *   (setq non-meta-chars
 *         (let ((i 0)
 *               (l nil))
 *           (while (< i 127)
 *             (setq l (cons i l) 
 *                   i (1+ i)))
 *           l))
 * 
 *   ;; A list of all characters 127 and above, each appearing once.
 *   (setq meta-chars
 *         (let ((i 127)
 *               (l nil))
 *           (while (< i 256)
 *             (setq l (cons i l) 
 *                   i (1+ i)))
 *           l))
 * 
 *   ;; A vector that will hold the chars in a random order.
 *   (setq scrambled-chars (make-vector 256 0))
 * 
 *   ;; These characters should map to themselves.
 *   (let ((i 0))
 *     (while (< i 32)
 *       (aset scrambled-chars i i)
 *       (setq non-meta-chars (delete i non-meta-chars) 
 *             i (1+ i))))
 *   
 *   ;; Assign random (but unique) values, within the non-meta chars. 
 *   (let ((i 32))
 *     (while (< i 127)
 *       (let ((ch (random-elt non-meta-chars)))
 *         (if (= 0 (aref scrambled-chars i))
 *             (progn
 *               (aset scrambled-chars i ch)
 *               (aset scrambled-chars ch i)
 *               (setq non-meta-chars (delete ch non-meta-chars)
 *                     non-meta-chars (delete i non-meta-chars))))
 *         (setq i (1+ i)))))
 * 
 *   ;; Assign random (but unique) values, within the non-meta chars. 
 *   (let ((i 127))
 *     (while (< i 256)
 *       (let ((ch (random-elt meta-chars)))
 *         (if (= 0 (aref scrambled-chars i))
 *             (progn
 *               (aset scrambled-chars i ch)
 *               (aset scrambled-chars ch i)
 *               (setq meta-chars (delete ch meta-chars)
 *                     meta-chars (delete i meta-chars))))
 *         (setq i (1+ i)))))
 * 
 *   ;; Now use the `scrambled-chars' vector to get your C array.
 *   )
 */
