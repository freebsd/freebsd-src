BEGIN {
       RS=""; FS="\n";
       ORS=""; OFS="\n";
      }
{
        split ($2,f," ")
        print $0;
}
