#Date: Tue, 21 Dec 1999 16:11:07 +0100
#From: Daniel Schnell <Daniel.Schnell.GP@icn.siemens.de>
#To: bug-gnu-utils@gnu.org
#CC: arnold@gnu.org
#Subject: BUG in gawk (version 3.0.4 linux, windows): Text mangeling in between

# search for "@K@CODE" segment

$0 ~ /@K@CODE/  {
                # get next record
                getline temp
                printf ("@K@CODE\n")
                printf ("%s\n",temp)
                }

$0 !~ /@K@CODE/ {
                printf ("%s\n", $0)
                }
