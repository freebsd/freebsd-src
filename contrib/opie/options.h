/* options.h: Configuration options the end user might want to tweak.

%%% copyright-cmetz
This software is Copyright 1996 by Craig Metz, All Rights Reserved.
The Inner Net License Version 2 applies to this software.
You should have received a copy of the license with this software. If
you didn't get a copy, you may request one from <license@inner.net>.

       History:

       Created by cmetz for OPIE 2.3 using the old Makefile.source as a
                guide.
*/
/*
  Which hash should the OPIE server software use?

  We strongly recommend that you use MD5. MD4 is faster, but less secure.
If you are migrating from Bellcore S/Key version 1 and wish to use the
existing key database, you must use MD4. In this case, you should consider
ways to re-key your users using MD5.
*/

#define MDX    5 /* Use MD5 */
/* #define MDX    4 /* Use MD4 */

/*
  Ask users to re-type their secret pass phrases?

  Doing so helps catch typing mistakes, but some users find it annoying.
*/

/* #define RETYPE 1 /* Ask users to re-type their secret pass phrases */
#define RETYPE 0 /* Don't ask users to re-type their secret pass phrases */

/*
  Generater lock files to serialize OTP logins?

  There is a potential race attack on OTP when more than one session can
respond to the same challenge at the same time. This locking only allows
one session at a time per principal (user) to attempt to log in using OTP.
The locking, however, creates a denial-of-service attack as a trade-off and
can be annoying if you have a legitimate need for two sessions to attempt
to authenticate as the same principal at the same time.
*/

#define USER_LOCKING 1 /* Serialize OTP challenges for a principal */
/* #define USER_LOCKING 0 /* Don't serialize OTP challenges */

/*
  Should su(8) refuse to switch to disabled accounts?

  Traditionally, su(8) can switch to any account, even if it is disabled.
In most systems, there is no legitimate need for this capability and it can
create security problems.
*/

#define SU_STAR_CHECK 1 /* Refuse to switch to disabled accounts */
/* #define SU_STAR_CHECK 0 /* Allow switching to disabled accounts */

/*
  Should OPIE use more informative prompts?

  The new-style, more informative prompts better indicate to the user what
is being asked for. However, some automated login scripts depend on the
wording of some prompts and will fail if you change them.
*/

#define NEW_PROMPTS 1 /* Use the more informative prompts */
/* #define NEW_PROMPTS 0 /* Use the more compatible prompts */

/*
  Should the user be allowed to override "insecure" terminal checks?

  The "insecure" terminal checks are designed to help make it more clear
to users that they shouldn't disclose their secret over insecure lines
by refusing to accept the secret directly. These checks aren't perfect and
sometimes will cause OPIE to refuse to work when it really should. Allowing
users to override the terminal checks also helps the process of creating
OTP sequences for users. However, allowing users to override the terminal
checks also allows users to shoot themselves in the foot, which isn't usually
what you want.
*/

#define INSECURE_OVERRIDE 0 /* Don't allow users to override the checks */
/* #define INSECURE_OVERRIDE 1 /* Allow users to override the checks */
