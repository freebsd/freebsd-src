BEGIN {
  if (ARGC != 2) {
    print "MOBAG - a simple mobile agent"
    print "CALL:\n    gawk -f mobag.awk mobag.awk"
    print "IN:\n    the name of this script as a command-line parameter"
    print "PARAM:\n    -v MyOrigin=myhost.com"
    print "OUT:\n    the result on stdout"
    print "JK 29.03.1998 01.04.1998"
    exit
  }
  if (MyOrigin == "") {
     "uname -n" | getline MyOrigin
     close("uname -n")
  }
}
#ReadMySelf
/^function /                     { FUNC = $2 }
/^END/ || /^#ReadMySelf/         { FUNC = $1 }
FUNC != ""                       { MOBFUN[FUNC] = MOBFUN[FUNC] RS $0 }
(FUNC != "") && (/^}/ || /^#EndOfMySelf/) \
                                 { FUNC = "" }
#EndOfMySelf
function migrate(Destination, MobCode, Label) {
  MOBVAR["Label"] = Label
  MOBVAR["Destination"] = Destination
  RS = ORS = "\r\n"
  HttpService = "/inet/tcp/0/" Destination
  for (i in MOBFUN)
     MobCode = (MobCode "\n" MOBFUN[i])
  MobCode = MobCode  "\n\nBEGIN {"
  for (i in MOBVAR)
     MobCode = (MobCode "\n  MOBVAR[\"" i "\"] = \"" MOBVAR[i] "\"")
  MobCode = MobCode "\n}\n"
  print "POST /cgi-bin/PostAgent.sh HTTP/1.0"  |& HttpService
  print "Content-length:", length(MobCode) ORS |& HttpService
  printf "%s", MobCode                         |& HttpService
  while ((HttpService |& getline) > 0)
     print $0
  close(HttpService)
}
END {
  if (ARGC != 2) exit    # stop when called with wrong parameters
  if (MyOrigin != "")    # is this the originating host?
    MyInit()             # if so, initialize the application
  else                   # we are on a host with migrated data
    MyJob()              # so we do our job
}
function MyInit() {
  MOBVAR["MyOrigin"] = MyOrigin
  MOBVAR["Machines"] = "localhost/80 max/80 moritz/80 castor/80"
  split(MOBVAR["Machines"], Machines)           # which host is the first?
  migrate(Machines[1], "", "")                  # go to the first host
  while (("/inet/tcp/8080/0/0" |& getline) > 0) # wait for result
    print $0                                    # print result
  close("/inet/tcp/8080/0/0")
}
function MyJob() {
  # forget this host
  sub(MOBVAR["Destination"], "", MOBVAR["Machines"])
  MOBVAR["Result"]=MOBVAR["Result"] SUBSEP SUBSEP MOBVAR["Destination"] ":"
  while (("who" | getline) > 0)               # who is logged in?
    MOBVAR["Result"] = MOBVAR["Result"] SUBSEP $0
  close("who")
  if (index(MOBVAR["Machines"], "/") > 0) {   # any more machines to visit?
    split(MOBVAR["Machines"], Machines)       # which host is next?
    migrate(Machines[1], "", "")              # go there
  } else {                                    # no more machines
    gsub(SUBSEP, "\n", MOBVAR["Result"])      # send result to origin
    print MOBVAR["Result"] |& "/inet/tcp/0/" MOBVAR["MyOrigin"] "/8080"
    close("/inet/tcp/0/" MOBVAR["MyOrigin"] "/8080")
  }
}
