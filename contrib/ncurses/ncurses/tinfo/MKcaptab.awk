#!/bin/sh
# $Id: MKcaptab.awk,v 1.11 1999/01/24 02:46:42 Jeffrey.C.Honig Exp $
AWK=${1-awk}
DATA=${2-../include/Caps}

cat <<'EOF'
/*
 *	comp_captab.c -- The names of the capabilities indexed via a hash
 *		         table for the compiler.
 *
 */

#include <ncurses_cfg.h>
#include <tic.h>
#include <term.h>

EOF

./make_hash 1 info <$DATA
./make_hash 3 cap  <$DATA

cat <<'EOF'
const struct alias _nc_capalias_table[] =
{
EOF

$AWK <$DATA '
$1 == "capalias"	{
		    if ($3 == "IGNORE")
			to = "(char *)NULL";
		    else
			to = "\"" $3 "\"";
		    printf "\t{\"%s\", %s, \"%s\"},\t /* %s */\n",
				$2, to, $4, $5
		}
'

cat <<'EOF'
	{(char *)NULL, (char *)NULL, (char *)NULL}
};

const struct alias _nc_infoalias_table[] =
{
EOF

$AWK <$DATA '
$1 == "infoalias"	{
		    if ($3 == "IGNORE")
			to = "(char *)NULL";
		    else
			to = "\"" $3 "\"";
		    printf "\t{\"%s\", %s, \"%s\"},\t /* %s */\n",
				$2, to, $4, $5
		}
'

cat <<'EOF'
	{(char *)NULL, (char *)NULL, (char *)NULL}
};

const struct name_table_entry *_nc_get_table(bool termcap)
{
	return termcap ? _nc_cap_table: _nc_info_table ;
}

const struct name_table_entry * const * _nc_get_hash_table(bool termcap)
{
	return termcap ? _nc_cap_hash_table: _nc_info_hash_table ;
}
EOF
