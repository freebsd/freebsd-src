BEGIN {
  if (ARGC != 2) {
    print "GETURL - retrieve Web page via HTTP 1.0"
    print "IN:\n    the URL as a command-line parameter"
    print "PARAM(S):\n    -v Proxy=MyProxy"
    print "OUT:\n    the page content on stdout"
    print "    the page header on stderr"
    print "JK 16.05.1997"
    print "ADR 13.08.2000"
    exit
  }
  URL = ARGV[1]; ARGV[1] = ""
  if (Proxy     == "")  Proxy     = "127.0.0.1"
  if (ProxyPort ==  0)  ProxyPort = 80
  if (Method    == "")  Method    = "GET"
  HttpService = "/inet/tcp/0/" Proxy "/" ProxyPort
  ORS = RS = "\r\n\r\n"
  print Method " " URL " HTTP/1.0" |& HttpService
  HttpService                      |& getline Header
  print Header > "/dev/stderr"
  while ((HttpService |& getline) > 0)
    printf "%s", $0
  close(HttpService)
}
