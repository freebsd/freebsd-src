{ request = request "\n" $0 }

END {
  BLASTService     = "/inet/tcp/0/www.ncbi.nlm.nih.gov/80"
  printf "POST /cgi-bin/BLAST/nph-blast_report HTTP/1.0\n" |& BLASTService
  printf "Content-Length: " length(request) "\n\n"         |& BLASTService
  printf request                                           |& BLASTService      
  while ((BLASTService |& getline) > 0)
      print $0
  close(BLASTService)
}
