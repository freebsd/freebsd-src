BEGIN {
  CGI_setup("GET",
  "http://www.gnu.org/cgi-bin/foo?p1=stuff&p2=stuff%26junk" \
       "&percent=a %25 sign",
  "1.0")
  for (i in MENU)
      printf "MENU[\"%s\"] = %s\n", i, MENU[i]
  for (i in PARAM)
      printf "PARAM[\"%s\"] = %s\n", i, PARAM[i]
  for (i in GETARG)
      printf "GETARG[\"%s\"] = %s\n", i, GETARG[i]
}
