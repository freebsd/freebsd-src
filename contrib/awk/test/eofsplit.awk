# Date: 	Sat, 30 Mar 1996 12:47:17 -0800 (PST)
# From: Charles Howes <chowes@grid.direct.ca>
# To: bug-gnu-utils@prep.ai.mit.edu, arnold@gnu.ai.mit.edu
# Subject: Bug in Gawk 3.0.0, sample code:
# 
#!/usr/local/bin/gawk -f
#
# Hello!  This is a bug report from chowes@direct.ca
#
# uname -a
# SunOS hostname 5.5 Generic sun4m
#
# Gnu Awk (gawk) 3.0, patchlevel 0:
BEGIN{
FS=":"
while ((getline < "/etc/passwd") > 0) {
  r=$3
  z=0
  n[0]=1
  }
FS=" "
}
#gawk: fp.new:16: fatal error: internal error
#Abort

# #!/usr/local/bin/gawk -f
# # Gnu Awk (gawk) 2.15, patchlevel 6
# 
# BEGIN{
# f="/etc/passwd"
# while (getline < f) n[0]=1
# FS=" "
# }
# #gawk: /staff/chowes/bin/fp:7: fatal error: internal error
# #Abort

# These examples are not perfect coding style because I took a real
# piece of code and tried to strip away anything that didn't make the error
# message go away.
# 
# The interesting part of the 'truss' is:
# 
# fstat(3, 0xEFFFF278)				= 0
# lseek(3, 0, SEEK_SET)				= 0
# read(3, " r o o t : x : 0 : 1 : S".., 2291)	= 2291
# brk(0x00050020)					= 0
# brk(0x00052020)					= 0
# read(3, 0x0004F4B8, 2291)			= 0
# close(3)					= 0
#     Incurred fault #6, FLTBOUNDS  %pc = 0x0001B810
#       siginfo: SIGSEGV SEGV_MAPERR addr=0x00053000
#     Received signal #11, SIGSEGV [caught]
#       siginfo: SIGSEGV SEGV_MAPERR addr=0x00053000
# write(2, " g a w k", 4)				= 4
# write(2, " :  ", 2)				= 2
# 
# --
# Charles Howes -- chowes@direct.ca                 Voice: (604) 691-1607
# System Administrator                                Fax: (604) 691-1605
# Internet Direct - 1050 - 555 West Hastings St - Vancouver, BC V6B 4N6
# 
# A sysadmin's life is a sorry one.  The only advantage he has over Emergency
# Room doctors is that malpractice suits are rare.  On the other hand, ER
# doctors never have to deal with patients installing new versions of their
# own innards!   -Michael O'Brien
# 
#  "I think I know what may have gone wrong in the original s/w.
#   It's a bug in the way it was written."  - Vagueness**n
