# CGI Library and core of a web server
#
# Juergen Kahrs, Juergen.Kahrs@vr-web.de
# with Arnold Robbins, arnold@gnu.org
# September 2000

# Global arrays
#   GETARG --- arguments to CGI GET command
#   MENU   --- menu items (path names)
#   PARAM  --- parameters of form x=y

# Optional variable MyHost contains host address
# Optional variable MyPort contains port number
# Needs TopHeader, TopDoc, TopFooter
# Sets MyPrefix, HttpService, Status, Reason

BEGIN {
  if (MyHost == "") {
     "uname -n" | getline MyHost
     close("uname -n")
  }
  if (MyPort ==  0) MyPort = 8080
  HttpService = "/inet/tcp/" MyPort "/0/0"
  MyPrefix    = "http://" MyHost ":" MyPort
  SetUpServer()
  while ("awk" != "complex") {
    # header lines are terminated this way
    RS = ORS    = "\r\n"
    Status      = 200             # this means OK
    Reason      = "OK"
    Header      = TopHeader
    Document    = TopDoc
    Footer      = TopFooter
    if        (GETARG["Method"] == "GET") {
        HandleGET()
    } else if (GETARG["Method"] == "HEAD") {
        # not yet implemented
    } else if (GETARG["Method"] != "") {
        print "bad method", GETARG["Method"]
    }
    Prompt = Header Document Footer
    print "HTTP/1.0", Status, Reason     |& HttpService
    print "Connection: Close"            |& HttpService
    print "Pragma: no-cache"             |& HttpService
    len = length(Prompt) + length(ORS)
    print "Content-length:", len         |& HttpService
    print ORS Prompt                     |& HttpService
    # ignore all the header lines
    while ((HttpService |& getline) > 0) 
        continue
    # stop talking to this client
    close(HttpService)
    # wait for new client request
    HttpService |& getline
    # do some logging
    print systime(), strftime(), $0
    CGI_setup($1, $2, $3)
  }
}

function CGI_setup(   method, uri, version, i)
{
    delete GETARG
    delete MENU
    delete PARAM
    GETARG["Method"] = method
    GETARG["URI"] = uri
    GETARG["Version"] = version

    i = index(uri, "?")
    if (i > 0) {  # is there a "?" indicating a CGI request?
        split(substr(uri, 1, i-1), MENU, "[/:]")
        split(substr(uri, i+1), PARAM, "&")
        for (i in PARAM) {
            PARAM[i] = _CGI_decode(PARAM[i])
            j = index(PARAM[i], "=")
            GETARG[substr(PARAM[i], 1, j-1)] = \
	                                 substr(PARAM[i], j+1)
        }
    } else { # there is no "?", no need for splitting PARAMs
        split(uri, MENU, "[/:]")
    }
    for (i in MENU)     # decode characters in path
        if (i > 4)      # but not those in host name
            MENU[i] = _CGI_decode(MENU[i])
}
function _CGI_decode(str,   hexdigs, i, pre, code1, code2,
                            val, result)
{
   hexdigs = "123456789abcdef"

   i = index(str, "%")
   if (i == 0) # no work to do
      return str

   do {
      pre = substr(str, 1, i-1)   # part before %xx
      code1 = substr(str, i+1, 1) # first hex digit
      code2 = substr(str, i+2, 1) # second hex digit
      str = substr(str, i+3)      # rest of string

      code1 = tolower(code1)
      code2 = tolower(code2)
      val = index(hexdigs, code1) * 16 \
            + index(hexdigs, code2)

      result = result pre sprintf("%c", val)
      i = index(str, "%")
   } while (i != 0)
   if (length(str) > 0)
      result = result str
   return result
}
