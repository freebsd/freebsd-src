BEGIN {
  NetService = "/inet/tcp/0/localhost/finger"
  print "var{name}" |& NetService
  while ((NetService |& getline) > 0)
    print $0
  close(NetService)
}
