function SetUpServer() {
  TopHeader = "<HTML><title>Remote Configuration</title>"
  TopDoc = "<BODY>\
    <h2>Please choose one of the following actions:</h2>\
    <UL>\
      <LI><A HREF=" MyPrefix "/AboutServer>About this server</A></LI>\
      <LI><A HREF=" MyPrefix "/ReadConfig>Read Configuration</A></LI>\
      <LI><A HREF=" MyPrefix "/CheckConfig>Check Configuration</A></LI>\
      <LI><A HREF=" MyPrefix "/ChangeConfig>Change Configuration</A></LI>\
      <LI><A HREF=" MyPrefix "/SaveConfig>Save Configuration</A></LI>\
    </UL>"
  TopFooter  = "</BODY></HTML>"
  if (ConfigFile == "") ConfigFile = "config.asc"
}
function HandleGET() {
  if(MENU[2] == "AboutServer") {
    Document  = "This is a GUI for remote configuration of an\
      embedded system. It is is implemented as one GAWK script."
  } else if (MENU[2] == "ReadConfig") {
    RS = "\n"
    while ((getline < ConfigFile) > 0)
       config[$1] = $2;
    close(ConfigFile)
    RS = "\r\n"
    Document = "Configuration has been read."
  } else if (MENU[2] == "CheckConfig") {
    Document = "<TABLE BORDER=1 CELLPADDING=5>"
    for (i in config)
      Document = Document "<TR><TD>" i "</TD>" \
        "<TD>" config[i] "</TD></TR>"
    Document = Document "</TABLE>"
  } else if (MENU[2] == "ChangeConfig") {
    if ("Param" in GETARG) {            # any parameter to set?
      if (GETARG["Param"] in config) {  # is  parameter valid?
        config[GETARG["Param"]] = GETARG["Value"]
        Document = (GETARG["Param"] " = " GETARG["Value"] ".")
      } else {
        Document = "Parameter <b>" GETARG["Param"] "</b> is invalid." 
      }
    } else {
      Document = "<FORM method=GET><h4>Change one parameter</h4>\
        <TABLE BORDER CELLPADDING=5>\
        <TR><TD>Parameter</TD><TD>Value</TD></TR>\
        <TR><TD><input type=text name=Param value=\"\" size=20></TD>\
            <TD><input type=text name=Value value=\"\" size=40></TD>\
        </TR></TABLE><input type=submit value=\"Set\"></FORM>"
    }
  } else if (MENU[2] == "SaveConfig") {
    for (i in config)
      printf("%s %s\n", i, config[i]) > ConfigFile
    close(ConfigFile)
    Document = "Configuration has been saved."
  }
}
