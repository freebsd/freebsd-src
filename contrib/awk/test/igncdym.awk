#From Jeffrey.B.Woodward@Hitchcock.ORG  Mon Feb 21 09:33:32 2000
#Message-id: <12901034@mailbox2.Hitchcock.ORG>
#Date: 20 Feb 2000 18:14:11 EST
#From: Jeffrey.B.Woodward@Hitchcock.ORG (Jeffrey B. Woodward)
#Subject: gawk 3.0.4 bug
#To: bug-gnu-utils@gnu.org
#Cc: arnold@gnu.org
#
#O/S: Digital UNIX 4.0D
#
#C Compiler: DEC C
#
#gawk version: 3.0.4
#
#Sample Program:
#gawk '
  BEGIN {
    pattern[1] = "bar" ; ignore[1] = 1
    pattern[2] = "foo" ; ignore[2] = 0
  }

  {
    for (i = 1 ; i <= 2 ; i++) {
      IGNORECASE = ignore[i]
      print match($0, pattern[i]) " " pattern[i] ":" $0
    }
  }
#' << -EOF-
#This is foo
#This is bar
#-EOF-
#
#Program Output:
#0 bar:This is foo
#0 foo:This is foo
#9 bar:This is bar
#9 foo:This is bar
#
#
#**Expected** Output:
#0 bar:This is foo
#9 foo:This is foo
#9 bar:This is bar
#0 foo:This is bar
#
#
#This problem appears to be directly related to IGNORECASE. If
#IGNORECASE remains constant, the program behaves as expected;
#however, switching IGNORECASE seems to causes problems - it is
#almost as though the pattern stored in the variable is treated
#as a constant and the regexp() is not recompiled(?) - just a
#guess...
#
#
#Thanks,
#-Jeff Woodward
