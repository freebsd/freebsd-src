#! /usr/bin/awk -f
BEGIN {
  getline
# print ("FILENAME =", FILENAME) > "/dev/stderr"
  #Rewind the file
  if (close(FILENAME)) {
      print "Error " ERRNO " closing input file" > "/dev/stderr";
      exit;   
  }
}
{  print "Analysing ", $0 }

