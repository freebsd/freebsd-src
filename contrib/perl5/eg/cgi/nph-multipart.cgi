#!/usr/local/bin/perl
use CGI qw/:push -nph/;
$| = 1;
print multipart_init(-boundary=>'----------------here we go!');
while (1) {
    print multipart_start(-type=>'text/plain'),
    "The current time is ",scalar(localtime),"\n",
    multipart_end;
    sleep 1;
}
