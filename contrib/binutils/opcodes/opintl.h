/* opintl.h - opcodes specific header for gettext code.
   Copyright (C) 1998, 1999 Free Software Foundation, Inc.

   Written by Tom Tromey <tromey@cygnus.com>

   This file is part of the opcodes library used by GAS and the GNU binutils.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA. */

#ifdef ENABLE_NLS
# include <libintl.h>
# define _(String) dgettext (PACKAGE, String)
# ifdef gettext_noop
#  define N_(String) gettext_noop (String)
# else
#  define N_(String) (String)
# endif
#else
/* Stubs that do something close enough.  */
# define textdomain(String) (String)
# define gettext(String) (String)
# define dgettext(Domain,Message) (Message)
# define dcgettext(Domain,Message,Type) (Message)
# define bindtextdomain(Domain,Directory) (Domain)
# define _(String) (String)
# define N_(String) (String)
#endif
