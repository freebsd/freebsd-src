#
# Awk script to convert filenames shortened down to 8.3 
# back to their larger size. 
#
# Works by looking at every filename and seeing if it's shortened
# 8.3 version exists, and if so then mv the short name to the long
# name.
#
# Usage: find . -type f -print | gawk -f ren2long.awk | sh -x [ 2> /dev/null ]
#


# Parse_path
#
# Takes the whole path and converts the basename part to 8.3. If it
# changed in the process we emit a sh command to mv it if the shortened
# name exists.
# 
function parse_path(p,P2,N,NEW) {

     P2 = tolower(p)

     NEW = ""
     while(1) {
	  N = index(P2,"/")			# Go until all / are parsed
	  if (N == 0) break

	  NEW = NEW name83(substr(P2,1,N-1)) "/"; # More of the path
	  P2 = substr(P2,N+1)
     }

     if (bad[P2] == 1) {
	  print "echo skipping " p
	  return
     }
     NEW = NEW name83(P2)			# Append path and 8.3 name

     if (bad[P2] == 2) {
	  print "if [ -f " NEW " ]; then echo ::rm " NEW " ; rm " NEW " ; fi"
	  return
     }
     if (NEW != p) 
	  print "if [ -f " NEW " ]; then echo ::mv " NEW " " p " ; mv " NEW " " p " ; fi"
}
#
# Name83
# 
# Converts the a single component part of a file name into 8.3 format
#
function name83(fname,P,B,E) {
     P = index(fname,".");			# Find the extension

     if (P == 0) {				# No extension
	  B = substr(fname,1,8);		# Just truncate at 8 chars
	  return B;
    }

    B = substr(fname, 1, P <= 8 ? P-1 : 8);	# At most 8 chars in name
    E = substr(fname, P+1, 3)			# And 3 in extension
    P = index(E, ".")				# 2 dot problem
    if (P)
	 E = substr(E, 1, P-1)

     B = B "." E				# Put name together
     return B
}
BEGIN {
     bad["krb5-types-aux.h"] = 1
     bad["autoconf.h.in"] = 1
     bad["conv_tkt_skey.c"] = 1
     ##bad["makefile"] = 2 -- windows have legitimate files with this name
}
{
     parse_path($1)				# Do it
}
