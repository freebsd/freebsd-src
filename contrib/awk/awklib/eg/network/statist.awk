function SetUpServer() {
  TopHeader = "<HTML><title>Statistics with GAWK</title>"
  TopDoc = "<BODY>\
   <h2>Please choose one of the following actions:</h2>\
   <UL>\
    <LI><A HREF=" MyPrefix "/AboutServer>About this server</A></LI>\
    <LI><A HREF=" MyPrefix "/EnterParameters>Enter Parameters</A></LI>\
   </UL>"
  TopFooter  = "</BODY></HTML>"
  GnuPlot    = "gnuplot 2>&1"
  m1=m2=0;    v1=v2=1;    n1=n2=10
}
function HandleGET() {
  if(MENU[2] == "AboutServer") {
    Document  = "This is a GUI for a statistical computation.\
      It compares means and variances of two distributions.\
      It is implemented as one GAWK script and uses GNUPLOT."
  } else if (MENU[2] == "EnterParameters") {
    Document = ""
    if ("m1" in GETARG) {     # are there parameters to compare?
      Document = Document "<SCRIPT LANGUAGE=\"JavaScript\">\
        setTimeout(\"window.open(\\\"" MyPrefix "/Image" systime()\
         "\\\",\\\"dist\\\", \\\"status=no\\\");\", 1000); </SCRIPT>"
      m1 = GETARG["m1"]; v1 = GETARG["v1"]; n1 = GETARG["n1"]
      m2 = GETARG["m2"]; v2 = GETARG["v2"]; n2 = GETARG["n2"]
      t = (m1-m2)/sqrt(v1/n1+v2/n2)
      df = (v1/n1+v2/n2)*(v1/n1+v2/n2)/((v1/n1)*(v1/n1)/(n1-1) \
           + (v2/n2)*(v2/n2) /(n2-1))
      if (v1>v2) {
          f = v1/v2
          df1 = n1 - 1
          df2 = n2 - 1
      } else {
          f = v2/v1
          df1 = n2 - 1
          df2 = n1 - 1
      }
      print "pt=ibeta(" df/2 ",0.5," df/(df+t*t) ")"  |& GnuPlot
      print "pF=2.0*ibeta(" df2/2 "," df1/2 "," \
            df2/(df2+df1*f) ")"                    |& GnuPlot
      print "print pt, pF"                         |& GnuPlot
      RS="\n"; GnuPlot |& getline; RS="\r\n"    # $1 is pt, $2 is pF
      print "invsqrt2pi=1.0/sqrt(2.0*pi)"          |& GnuPlot
      print "nd(x)=invsqrt2pi/sd*exp(-0.5*((x-mu)/sd)**2)" |& GnuPlot
      print "set term png small color"             |& GnuPlot
      #print "set term postscript color"           |& GnuPlot
      #print "set term gif medium size 320,240"    |& GnuPlot
      print "set yrange[-0.3:]"                    |& GnuPlot
      print "set label 'p(m1=m2) =" $1 "' at 0,-0.1 left"  |& GnuPlot
      print "set label 'p(v1=v2) =" $2 "' at 0,-0.2 left"  |& GnuPlot
      print "plot mu=" m1 ",sd=" sqrt(v1) ", nd(x) title 'sample 1',\
        mu=" m2 ",sd=" sqrt(v2) ", nd(x) title 'sample 2'" |& GnuPlot
      print "quit"                                         |& GnuPlot
      GnuPlot |& getline Image
      while ((GnuPlot |& getline) > 0)
          Image = Image RS $0
      close(GnuPlot)
    }
    Document = Document "\
    <h3>Do these samples have the same Gaussian distribution?</h3>\
    <FORM METHOD=GET> <TABLE BORDER CELLPADDING=5>\
    <TR>\
    <TD>1. Mean    </TD>
    <TD><input type=text name=m1 value=" m1 " size=8></TD>\
    <TD>1. Variance</TD>
    <TD><input type=text name=v1 value=" v1 " size=8></TD>\
    <TD>1. Count   </TD>
    <TD><input type=text name=n1 value=" n1 " size=8></TD>\
    </TR><TR>\
    <TD>2. Mean    </TD>
    <TD><input type=text name=m2 value=" m2 " size=8></TD>\
    <TD>2. Variance</TD>
    <TD><input type=text name=v2 value=" v2 " size=8></TD>\
    <TD>2. Count   </TD>
    <TD><input type=text name=n2 value=" n2 " size=8></TD>\
    </TR>                   <input type=submit value=\"Compute\">\      
    </TABLE></FORM><BR>"
  } else if (MENU[2] ~ "Image") {     
    Reason = "OK" ORS "Content-type: image/png"
    #Reason = "OK" ORS "Content-type: application/x-postscript"
    #Reason = "OK" ORS "Content-type: image/gif"
    Header = Footer = ""
    Document = Image
  }
}
