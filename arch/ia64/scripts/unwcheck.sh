#!/bin/sh
#   Usage: unwcheck.sh <executable_file_name>
#   Pre-requisite: readelf [from Gnu binutils package]
#   Purpose: Check the following invariant
#       For each code range in the input binary:
#          Sum[ lengths of unwind regions] = Number of slots in code range.
#   Author : Harish Patil
#   First version: January 2002
#   Modified : 2/13/2002
#   Modified : 3/15/2002: duplicate detection
readelf -u $1 | gawk '\
 function todec(hexstr){
    dec = 0;
    l = length(hexstr);
    for (i = 1; i <= l; i++)
    {
        c = substr(hexstr, i, 1);
        if (c == "A")
            dec = dec*16 + 10;
        else if (c == "B")
            dec = dec*16 + 11;
        else if (c == "C")
            dec = dec*16 + 12;
        else if (c == "D")
            dec = dec*16 + 13;
        else if (c == "E")
            dec = dec*16 + 14;
        else if (c == "F")
            dec = dec*16 + 15;
        else
            dec = dec*16 + c;
    }
    return dec;
 }
 BEGIN { first = 1; sum_rlen = 0; no_slots = 0; errors=0; no_code_ranges=0; }
 {
   if (NF==5 && $3=="info")
   {
      no_code_ranges += 1;
      if (first == 0)
      {
         if (sum_rlen != no_slots)
         {
            print full_code_range;
            print "       ", "lo = ", lo, " hi =", hi;
            print "       ", "sum_rlen = ", sum_rlen, "no_slots = " no_slots;
            print "       ","   ", "*******ERROR ***********";
            print "       ","   ", "sum_rlen:", sum_rlen, " != no_slots:" no_slots;
            errors += 1;
         }
         sum_rlen = 0;
      }
      full_code_range =  $0;
      code_range =  $2;
      gsub("..$", "", code_range);
      gsub("^.", "", code_range);
      split(code_range, addr, "-");
      lo = toupper(addr[1]);

      code_range_lo[no_code_ranges] = addr[1];
      occurs[addr[1]] += 1;
      full_range[addr[1]] = $0;

      gsub("0X.[0]*", "", lo);
      hi = toupper(addr[2]);
      gsub("0X.[0]*", "", hi);
      no_slots = (todec(hi) - todec(lo))/ 16*3
      first = 0;
   }
   if (index($0,"rlen") > 0 )
   {
    rlen_str =  substr($0, index($0,"rlen"));
    rlen = rlen_str;
    gsub("rlen=", "", rlen);
    gsub(")", "", rlen);
    sum_rlen = sum_rlen +  rlen;
   }
  }
  END {
      if (first == 0)
      {
         if (sum_rlen != no_slots)
         {
            print "code_range=", code_range;
            print "       ", "lo = ", lo, " hi =", hi;
            print "       ", "sum_rlen = ", sum_rlen, "no_slots = " no_slots;
            print "       ","   ", "*******ERROR ***********";
            print "       ","   ", "sum_rlen:", sum_rlen, " != no_slots:" no_slots;
            errors += 1;
         }
      }
    no_duplicates = 0;
    for (i=1; i<=no_code_ranges; i++)
    {
        cr = code_range_lo[i];
        if (reported_cr[cr]==1) continue;
        if ( occurs[cr] > 1)
        {
            reported_cr[cr] = 1;
            print "Code range low ", code_range_lo[i], ":", full_range[cr], " occurs: ", occurs[cr], " times.";
            print " ";
            no_duplicates++;
        }
    }
    print "======================================"
    print "Total errors:", errors, "/", no_code_ranges, " duplicates:", no_duplicates;
    print "======================================"
  }
  '
