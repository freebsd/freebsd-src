#!/usr/local/bin/perl
# (C) Copyright 1998 Ivan S. Bishop (isb@notoryus.genmagic.com)
#
############### START SUBROUTINE DECLARATIONS ###########


sub usage {
    print "\n" x 24;
    print "USAGE: ipfanalyze.pl -h  [-p port# or all] [-g] [-s] [-v] [-o] portnum -t [target ip address] [-f] logfilename\n";
    print "\n arguments to -p -f -o REQUIRED\n";
    print "\n -h show this help\n";
    print "\n -p limit stats/study to this port number.(eg 25 not smtp)\n";
    print " -g make graphs, one per 4 hour interval called outN.gif 1<=N<=5\n";
    print " -s make  security report only (no graphical or full port info generated) \n";
    print " -o  lowest port number incoming traffic can talk to and be regarded as safe\n";
    print " -v verbose report with graphs and textual AND SECURITY REPORTS with -o 1024 set\n";
    print " -t the ip address of the inerface on which you collected data!\n";
    print " -f name ipfilter log file (compatible with V 3.2.9) [ipfilter.log]\n";
    print " \nExample: ./ipfanalyze.pl -p all -g -f log1\n";
    print "Will look at traffic to/from all ports and make graphs from file log1\n";
    print " \nExample2 ./ipfanalyze.pl -p 25 -g -f log2\n";
    print "Will look at SMTP traffic and make graphs from file log2\n";
    print " \nExample3 ./ipfanalyze.pl -p all -g -f log3 -o 1024\n";
    print "Will look at all traffic,make graphs from file log3 and log security info for anthing talking inwards below port 1024\n";
    print " \nExample4 ./ipfanalyze.pl -p all -f log3 -v \n";
    print "Report the works.....when ports below 1024 are contacted highlight (like -s -o 1024)\n";
}




sub makegifs {
local  ($maxin,$maxout,$lookat,$xmax)=@_;
$YMAX=$maxin;
$XMAX=$xmax;

if ($maxout > $maxin)
  { $YMAX=$maxout;}

($dateis,$junk)=split " " ,  @recs[0];
($dayis,$monthis,$yearis)=split "/",$dateis;
$month=$months{$monthis};
$dateis="$dayis " . "$month " . "$yearis ";
# split graphs in to 6 four hour spans for 24 hours 
$numgraphs=int($XMAX/240);

$junk=0;
$junk=$XMAX - 240*($numgraphs);
if($junk gt 0 )
{
$numgraphs++;
}

$cnt1=0;
$end=0;
$loop=0;

while ($cnt1++ < $numgraphs)
{
 $filename1="in$cnt1.dat";
 $filename2="out$cnt1.dat";
 $filename3="graph$cnt1.conf";
 open(OUTDATA,"> $filename2") || die "Couldnt open $filename2 for writing \n";
 open(INDATA,"> $filename1") || die "Couldnt open $filename1 for writing \n";
  
 $loop=$end;
 $end=($end + 240);

# write all files as x time coord from 1 to 240 minutes
# set hour in graph via conf file
 $arraycnt=0;
 while ($loop++ < $end )
  {
   $arraycnt++;
   $val1="";
   $val2="";
   $val1=$inwards[$loop] [1];
   if($val1 eq "")
     {$val1=0};
   $val2=$outwards[$loop] [1];
   if($val2 eq "")
     {$val2=0};
   print INDATA "$arraycnt:$val1\n";
   print OUTDATA "$arraycnt:$val2\n";
  }
  close INDATA;
  close OUTDATA;
  $gnum=($cnt1 - 1);
 open(INCONFIG,"> $filename3") || die "Couldnt open ./graph.conf for writing \n";
   print INCONFIG "NUMBERYCELLGRIDSIZE:5\n";
   print INCONFIG "MAXYVALUE:$YMAX\n";
   print INCONFIG "MINYVALUE:0\n";
   print INCONFIG "XCELLGRIDSIZE:1.3\n";
   print INCONFIG "XMAX: 240\n";
   print INCONFIG "Bar:0\n";
   print INCONFIG "Average:0\n";
   print INCONFIG "Graphnum:$gnum\n";
   print INCONFIG "Title: port $lookat packets/minute to/from gatekeep on $dateis  \n";
   print INCONFIG "Transparent:no\n";
   print INCONFIG "Rbgcolour:0\n";
   print INCONFIG "Gbgcolour:255\n";
   print INCONFIG "Bbgcolour:255\n";
   print INCONFIG "Rfgcolour:0\n";
   print INCONFIG "Gfgcolour:0\n";
   print INCONFIG "Bfgcolour:0\n";
   print INCONFIG "Rcolour:0\n";
   print INCONFIG "Gcolour:0\n";
   print INCONFIG "Bcolour:255\n";
   print INCONFIG "Racolour:255\n";
   print INCONFIG "Gacolour:255\n";
   print INCONFIG "Bacolour:0\n";
   print INCONFIG "Rincolour:100\n";
   print INCONFIG "Gincolour:100\n";
   print INCONFIG "Bincolour:60\n";
   print INCONFIG "Routcolour:60\n";
   print INCONFIG "Goutcolour:100\n";
   print INCONFIG "Boutcolour:100\n";
   close INCONFIG;

}


$cnt1=0;
while ($cnt1++ < $numgraphs)
{
 $filename1="in$cnt1.dat";
 $out="out$cnt1.gif";
 $filename2="out$cnt1.dat";
 $filename3="graph$cnt1.conf";
 system( "cp ./$filename1  ./in.dat;
          cp ./$filename2  ./out.dat;
          cp ./$filename3  ./graph.conf");
 system( "./isbgraph -conf graph.conf;mv graphmaker.gif $out");
 system(" cp $out /isb/local/etc/httpd/htdocs/.");

}

} # end of subroutine make gifs




sub packbytime {
local  ($xmax)=@_;
$XMAX=$xmax;
# pass in the dest port number or get graph for all packets
# at 1 minute intervals 
# @shortrecs has form 209.24.1.217 123 192.216.16.2 123 udp len 20 76
# @recs has form 27/07/1998 00:01:05.216596  le0 @0:2 L 192.216.21.16,2733 -> 192.216.16.2,53 PR udp len 20 62
#
# dont uses hashes to store how many packets per minite as they
# return random x coordinate order
@inwards=();
@outwards=();
$cnt=-1;
$value5=0;
$maxin=0;
$maxout=0;
$xpos=0;
while ($cnt++ <= $#recs )
 {
 ($srcip,$srcport,$destip,$destport,$pro)= split " " ,  @shortrecs[$cnt];
  $bit=substr(@recs[$cnt],11);
  ($bit,$junkit)= split " " , $bit ;
 ($hour,$minute,$sec,$junk) = split ":", $bit;
#
# covert the time to decimal minutes and bucket to nearest minute
#
 $xpos=($hour * 3600) + ($minute * 60) + ($sec) ;
# xpos is number of seconds since 00:00:00 on day......
 $xpos=int($xpos / 60);
# if we just want to see all packet in/out activity
 if("$lookat" eq "all")
   {
    if("$destip" eq "$gatekeep")
      {
#      TO GATEKEEP port lookat
#        print "to gatekeep at $xpos\n"; 
        $value5=$inwards[$xpos] [1];
        $value5++ ; 
#        $maxin = $value5 if $maxin < $value5 ;

         if($value5 > $maxin)
                   {
                        $maxin=$value5;
                        $timemaxin="$hour:$minute";
                   }
        $inwards[$xpos][1]=$value5;
        }
    else
      {
#       FROM GATEKEEP to port lookat
#        print "from gatekeep at $xpos\n"; 
        $value4=$outwards[$xpos] [1];
        $value4++ ; 
#        $maxout = $value4 if $maxout < $value4 ;
	if($value4 > $maxout)
           {
                $maxout=$value4;
                $timemaxout="$hour:$minute";
           }

        $outwards[$xpos][1]=$value4;
      }
    }




 if("$destport" eq "$lookat")
   {
    if("$destip" eq "$gatekeep")
      {
#      TO GATEKEEP port lookat
#        print "to gatekeep at $xpos\n"; 
        $value5=$inwards[$xpos] [1];
        $value5++ ; 
        $maxin = $value5 if $maxin < $value5 ;
        $inwards[$xpos][1]=$value5;
        }
    else
      {
#       FROM GATEKEEP to port lookat
#        print "from gatekeep at $xpos\n"; 
        $value4=$outwards[$xpos] [1];
        $value4++ ; 
        $maxout = $value4 if $maxout < $value4 ;
        $outwards[$xpos][1]=$value4;
      }
   }
 } # end while

# now call gif making stuff
if("$opt_g" eq "1")
{
 print "Making plots of in files outN.gif\n";;
 makegifs($maxin,$maxout,$lookat,$#inwards);
}
if ("$timemaxin" ne "")
{print "\nTime of peak packets/minute in was $timemaxin\n";}
if ("$timemaxout" ne "")
{print "\nTime of peak packets/minute OUT was $timemaxout\n";}

} # end of subroutine packets by time





sub posbadones {

$safenam="";
@dummy=$saferports;
foreach $it (split " ",$saferports) {
if ($it eq "icmp" )
 {
   $safenam = $safenam . " icmp";
 }
else
 {
   $safenam = $safenam . " $services{$it}" ;
 }

}
print "\n\n########################################################################\n";
print "well known ports are 0->1023\n";
print "Registered ports are 1024->49151\n";
print "Dynamic/Private ports are 49152->65535\n\n";
print "Sites that contacted gatekeep on 'less safe' ports  (<$ITRUSTABOVE)\n";

print " 'safe'  ports are $safenam                               \n";
print "\n variables  saferports and safehosts hardwire what/who we trust\n";
print "########################################################################\n";

$loop=-1;
while ($loop++ <= $#recs )
 {
 ($srcip,$srcport,$destip,$destport,$pro)= split " " ,  @shortrecs[$loop];
  if ("$destip" eq "$gatekeep") 
    {
     if ($destport < $ITRUSTABOVE )
     {
#      if index not found (ie < 0) then we have a low port attach to gatekeep
#      that is not to a safer port (see top of this file)
#      ie no ports 25 (smtp), 53 (dns) , 113 (ident), 123 (ntp), icmp
       $where=index($saferports,$destport);
       if ($where < 0)
         {
          $nameis=$services{$destport};
          if ("$nameis" eq "" )
            {
              $nameis=$destport;
            }
              print " Warning: $srcip contacted gatekeep $nameis\n";
         }
      }
    }
  }
print "\n\n";
} # end of subroutine posbadones




sub toobusy_site {
$percsafe=1;
print "\n\n########################################################################\n";
print "# Sites sending > $percsafe % of all packets to gatekeep MAY be attacking/probing\n";
print "Trusted hosts are $safehosts\n";
print "\nTOTAL packets were $#recs \n";
print "########################################################################\n";
while(($ipadd,$numpacketsent)=each %numpacks) 
{
$perc=$numpacketsent/$#recs*100;
if ($perc > $percsafe) 
# dont believe safehosts are attacking!
 {
   $where=index($safehosts,$ipadd);
#  if not found (ie < 0 then the source host IP address
#  isn't in the saferhosts list, a list we trust......
   if ($where < 0 )
   {
     printf "$ipadd	 sent %4.1f (\045) of all packets to gatekeep\n",$perc;
   }
 }
}

print "\n\n";
} # end of subroutine toobusy_site 


############### END SUBROUTINE DECLARATIONS ###########

use Getopt::Std;

getopt('pfot');

if("$opt_t" eq "0")
 {usage;print "\n---->ERROR: You must psecify the IP address of the interface that collected the data!\n";
exit;
}
 
if("$opt_h" eq "1")
 {usage;exit 0};
if("$opt_H" eq "1")
 {usage;exit 0};

if("$opt_v" eq "1")
{
$ITRUSTABOVE=1024;
$opt_s=1;
$opt_o=$ITRUSTABOVE;
print "\n" x 5;
print "NOTE: when the final section of the verbose report is generated\n";
print "      every host IP address that contacted $gatekeep has \n";
print "      a tally of how many times packets from a particular port on that host\n";
print "      reached $gatekeep, and WHICH source port or source portname \n";
print "      these packets originated from.\n";
print "      Many non RFC obeying boxes do not use high ports and respond to requests from\n";
print "      $gatekeep using  reserved low ports... hence you'll see things like\n";
print "      #### with 207.50.191.60 as the the source for packets ####\n";
print "      1 connections from topx to gatekeep\n\n\n\n";

}

if("$opt_o" eq "")
 {usage;print "\n---->ERROR: Must specify lowest safe port name for incoming trafic\n";exit 0}
else
{
$ITRUSTABOVE=$opt_o;$opt_s=1;}

if("$opt_f" eq "")
 {usage;print "\n---->ERROR: Must specify filename with -f \n";exit 0};
$FILENAME=$opt_f;

if("$opt_p" eq "")
 {usage;print "\n---->ERROR: Must specify port number or 'all' with -p \n";exit 0};

# -p arg must be all or AN INTEGER in range 1<=N<=64K
if ("$opt_p" ne "all")
 {
   $_=$opt_p;  
   unless (/^[+-]?\d+$/)
   {
     usage;
     print "\n---->ERROR: Must specify port number (1-64K) or 'all' with -p \n";
     exit 0;
   }
 }


# if we get here then the port option is either 'all' or an integer...
# good enough.....
$lookat=$opt_p;

# -o arg must be all or AN INTEGER in range 1<=N<=64K
   $_=$opt_o;  
   unless (/^[+-]?\d+$/)
   {
     usage;
     print "\n---->ERROR: Must specify port number (1-64K)  with -o \n";
     exit 0;
   }


#---------------------------------------------------------------------


%danger=();
%numpacks=();

$saferports="25 53 113 123 icmp";
$gatekeep="192.216.16.2";
#genmagic is  192.216.25.254
$safehosts="$gatekeep 192.216.25.254";



# load hash with service numbers versus names

# hash  called $services
print "Creating hash of service names / numbers \n";
$SERV="./services";
open (INFILE, $SERV) || die "Cant open $SERV: $!n";
while(<INFILE>)
{
 ($servnum,$servname,$junk)=split(/ /,$_);
# chop off null trailing.....
  $servname =~ s/\n$//;
  $services{$servnum}=$servname;
}
print "Create hash of month numbers as month names\n";
%months=("01","January","02","February","03","March","04","April","05","May","06","June","07","July","08","August","09","September","10","October","11","November","12","December");

print "Reading log file into an array\n";
#$FILENAME="./ipfilter.log";
open (REC, $FILENAME) || die "Cant open $FILENAME: \n";
($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$junk)=stat REC;
print "Log file $FILENAME is $size bytes in size\n";
#each record is an element of array rec[] now
while(<REC>) 
 {
 @recs[$numrec++]=$_;
 }


# get list of UNIQUE source IP addresses now, records look like
# 192.216.25.254,62910 ->  192.216.16.2,113 PR tcp len 20 40 -R
# this is slow on big log files, about 1minute for every 2.5M log file
print "Making list of unique source IP addresses (1minute for every 2M log parsed)\n";
$loop=-1;
$where=-1;
while ($loop++ < $#recs )
 {
# get the LHS = source IP address, need fiddle as icmp rcords are logged oddly
  $bit=substr(@recs[$loop],39);
  $bit =~ s/,/ /g;
  ($sourceip,$junkit)= split " " , $bit ;
  
# NOTE the  . is the string concat command NOT + .......!!!!

  $sourceip =~ split " ", $sourceip;
   $where=index($allips,$sourceip);
#  if not found (ie < 0, add it)
   if ($where < 0 )
   {
     $allips = $allips . "$sourceip " ;
   }
 }
  
print "Put all unique ip addresses into a 1D array\n";
@allips=split " ", $allips;

#set loop back to -1 as first array element in recs is element 0 NOT 1 !!
print "Making compact array of logged entries\n";
$loop=-1;
$icmp=" icmp ";
$ptr=" -> ";
$lenst=" len ";
$numpackets=0;

while ($loop++ < $#recs )
 {
# this prints from 39 char to EOR
 $a=substr(@recs[$loop],39);
 ($srcip,$dummy,$destip,$dummy2,$dummy3,$dummy4,$lenicmp)= split " " ,  $a ;
# need to rewrite icmp ping records.... they dont have service numbers
 $whereicmp=index($a,"PR icmp");
 if($whereicmp > 0 )
 {
  $a = $srcip . $icmp .  $ptr . $destip  . $icmp . $icmp . $lenst . $lenicmp ;
 }
 
# dump the "->"  and commas from logging
 $a =~ s/->//g;
 $a =~ s/PR//g;
 $a =~ s/,/ /g;
# shortrec has records that look like
# 209.24.1.217 123 192.216.16.2 123 udp len 20 76
 @shortrecs[$loop]= "$a";

# count number packets from each IP address into hash
 ($srcip,$junk) = split " ","$a";
 $numpackets=$numpacks{"$srcip"};
 $numpackets++ ;
 $numpacks{"$srcip"}=$numpackets; 

}



# call sub to analyse packets by time
# @shortrecs has form 209.24.1.217 123 192.216.16.2 123 udp len 20 76
# @recs has form 27/07/1998 00:01:05.216596  le0 @0:2 L 192.216.21.16,2733 -> 192.216.16.2,53 PR udp len 20 62
packbytime($XMAX);

if("$opt_s" eq "1")
{
# call subroutine to scan for connections to ports on gatekeep
# other than those listed in saferports, connections to high
# ports are assumed OK.....
posbadones;

# call subroutine to print out which sites had sent more than
# a defined % of packets to gatekeep
toobusy_site;
}


# verbose reporting?
if ("$opt_v" eq "1")
{
$cnt=-1;
# loop over ALL unique IP source destinations
while ($cnt++ < $#allips)
{
  %tally=();
  %unknownsrcports=();
  $uniqip=@allips[$cnt];
  $loop=-1;
  $value=0;
  $value1=0;
  $value2=0;
  $value3=0;
  $set="N";

  while ($loop++ < $#recs )
   {
#    get src IP num,    src port number, 
#    destination IP num, destnation port number,protocol
     ($srcip,$srcport,$destip,$destport,$pro)= split " " ,  @shortrecs[$loop];
# loop over all records for the machine $uniqip
# NOTE THE STRINGS ARE COMPARED WITH eq NOT cmp and NOT = !!!!
   if(  "$uniqip" eq "$srcip")
    {
# look up hash of service names to get key... IF ITS NOT THERE THEN WHAT???
# its more than likely  a request coming back in on a high port
# ....So...
# find out the destination port from the unknown (high) src port
# and tally these as they may be a port attack
  if ("$srcport" eq "icmp")
   { $srcportnam="icmp";}
  else
   {
     $srcportnam=$services{$srcport};
   }
#    try and get dest portname, if not there, leave it as the 
#    dest portnumber
  if ("$destport" eq "icmp")
   { $destportnam="icmp";}
  else
   {
  $destportnam=$services{$destport};
   }

  if ($destportnam eq "")
   {
     $destportnam=$destport;
   }

  if ($srcportnam eq "")
    {
#    increment number of times a (high)/unknown port has gone to destport
     $value1=$unknownsrcports{$destportnam}; 
     $value1++ ; 
     $unknownsrcports{$destportnam}=$value1;
    }
  else
   {
#    want tally(srcport) counter to be increased by 1
     $value3=$tally{$srcportnam};
     $value3++ ; 
     $tally{$srcportnam}=$value3;
    }
   }


   }
#  end of loop over ALL IP's

if ($set eq "N")
{
$set="Y";

print "\n#### with $uniqip as the the source for packets ####\n";
while(($key,$value)=each %tally) 
  {
   if (not "$uniqip" eq "$gatekeep")
    {
      print "$value connections from $key to gatekeep\n";
    }
   else
    {
      print "$value connections from gatekeep to $key\n";
     }
  }



while(($key2,$value2)=each %unknownsrcports) 
  {
   if (not "$uniqip" eq "$gatekeep")
    {
     print "$value2 high port connections to $key2 on gatekeep\n";
    }
   else
    {
      print "$value2 high port connections to $key2 from gatekeep\n";
     }
  }

}
# print if rests for UNIQIP IF flag is set to N then toggle flag

} # end of all IPs loop 
} # end of if verbose option set block



