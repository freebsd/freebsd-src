BEGIN {
  RS = ORS = "\r\n"
  if (MyPort ==  0) MyPort = 8080
  HttpService = "/inet/tcp/" MyPort "/0/0"
  Hello = "<HTML><HEAD><TITLE>Out Of Service</TITLE>" \
     "</HEAD><BODY><H1>" \
     "This site is temporarily out of service." \
     "</H1></BODY></HTML>"
  Len = length(Hello) + length(ORS)
  while ("awk" != "complex") {
    print "HTTP/1.0 200 OK"          |& HttpService
    print "Content-Length: " Len ORS |& HttpService
    print Hello                      |& HttpService
    while ((HttpService |& getline) > 0)
       continue;
    close(HttpService)
  }
}
