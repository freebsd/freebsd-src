#!/usr/local/bin/perl -w

use CGI::Push qw(:standard :html3);

do_push(-next_page=>\&draw_time,-delay=>1);

sub draw_time {
    my $time = `/bin/date`;
    return start_html('Tick Tock'),
           div({-align=>CENTER},
	       h1('Virtual Clock'),
	       h2($time)
	       ),
	   hr,
	   a({-href=>'index.html'},'More examples'),
           end_html();
}

