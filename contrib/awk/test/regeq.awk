#Date: Sat, 8 May 1999 17:42:20 +0200
#From: Iva Cabric <ivac@fly.srk.fer.hr>
#To: bug-gnu-utils@gnu.org
#Cc: arnold@gnu.org
#Subject: Problem in gawk with match
#
#Hello,
#
#gawk reports fatal error in match when first character in regexp is "=" :
#
#$ gawk '{ where = match($0, /=a/); print where}' 
#gawk: cmd. line:1: { where = match($0, /=a/); print where}
#gawk: cmd. line:1:                     ^ parse error
#gawk: cmd. line:1: fatal: match() cannot have 0 arguments
#
#Using "\=" instead "=" works without problems :
#
#$ gawk '{ where = match($0, /\=a/); print where}'
#sdgfa
#0
#asdfds=a
#7
#
#Other versions of awk have no problems with "/=/" (except oawk on SunOS).
#
#-- 
#     	 		 	 @
#
{ where = match($0, /=a/); print where}
