#Date: Tue, 18 May 1999 12:48:07 -0500 (CDT)
#From: Darrel Hankerson <hankedr@dms.auburn.edu>
#To: arnold@gnu.org
#Subject: [christopher.procter@bt.com: RE: Getline bug in Gawk 3.0.3]
#
#Here's a reply that came directly to me.  --darrel
#
#
#From: christopher.procter@bt.com
#To: hankedr@dms.auburn.edu
#Subject: RE: Getline bug in Gawk 3.0.3
#Date: Tue, 18 May 1999 18:42:28 +0100
#
#Sorry that was me getting carried away and cut and pasting the wrong thing
#into my email
#
#The real problem seems to be that :
#BEGIN {
#for (i=1;i<10;i++){
#	while((getline < "hello.txt")>0){
# 		print $0
#		}
# 	close("hello.txt")
#	}
#}
#works (printing the contents of hello.txt 9 times), where as:-
#
#END{
#for (i=1;i<10;i++){
#	while((getline < "hello.txt")>0){
# 		print $0
#		}
# 	close("hello.txt")
#	}
#}
#
#doesn't, (it prints out hello.txt once followed by the iteration numbers
#from 1 to 9).
#The only difference is that one is in the BEGIN block and one in the END
#block.
#
#Sorry about the first post, I'm not a bad awk programmer, just a tired one
#:)
#
#chris
#
#> -----Original Message-----
#> From:	Darrel Hankerson [SMTP:hankedr@dms.auburn.edu]
#> Sent:	18 May 1999 18:28
#> To:	christopher.procter@bt.com
#> Subject:	Re: Getline bug in Gawk 3.0.3
#> 
#> Could you clarify?  Your first script uses an apparently undefined
#> variable f.
#> 
#> 
#> christopher.procter@bt.com writes:
#> 
#>    BEGIN {
#>    for (i=1;i<10;i++){
#>    while((getline < "hello.txt")>0){
#>      print $0
#>    }
#>      close(f)
#>    }
#>    }
#> 
#>    refuses to close the file and so prints the contents of hello.txt just
#> once.
#>    However:-
#> 
#>    BEGIN {
#>    f="hello.txt"
#>    for (i=1;i<10;i++){
#>    while((getline < f)>0){
#>      print $0
#>    }
#>      close(f)
#>    }
#>    }
#> 
#>    works as advertised (printing the contents of hello.txt 9 times)
#>    It seems like a bug in the close statement.
#> 
#> -- 
#> --Darrel Hankerson hankedr@mail.auburn.edu
#

# srcdir is assigned on command line --- ADR
END {
	f = srcdir "/redfilnm.in"
	for (i = 1; i < 10; i++){
		while((getline < f) > 0){
 			print $0
		}
 		close(f)
	}
}
