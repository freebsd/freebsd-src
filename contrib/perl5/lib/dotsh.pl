#
#   @(#)dotsh.pl                                               03/19/94
#
#   Author: Charles Collins
#
#   Description:
#      This routine takes a shell script and 'dots' it into the current perl
#      environment. This makes it possible to use existing system scripts
#      to alter environment variables on the fly.
#
#   Usage:
#      &dotsh ('ShellScript', 'DependentVariable(s)');
#
#         where
#
#      'ShellScript' is the full name of the shell script to be dotted
#
#      'DependentVariable(s)' is an optional list of shell variables in the
#         form VARIABLE=VALUE,VARIABLE=VALUE,... that 'ShellScript' is
#         dependent upon. These variables MUST be defined using shell syntax.
#
#   Example:
#      &dotsh ('/tmp/foo', 'arg1');
#      &dotsh ('/tmp/foo');
#      &dotsh ('/tmp/foo arg1 ... argN');
#
sub dotsh {
   local(@sh) = @_;
   local($tmp,$key,$shell,*dotsh,$command,$args,$vars) = '';
   $dotsh = shift(@sh);
   @dotsh = split (/\s/, $dotsh);
   $command = shift (@dotsh);
   $args = join (" ", @dotsh);
   $vars = join ("\n", @sh);
   open (_SH_ENV, "$command") || die "Could not open $dotsh!\n";
   chop($_ = <_SH_ENV>);
   $shell = "$1 -c" if ($_ =~ /^\#\!\s*(\S+(\/sh|\/ksh|\/zsh|\/csh))\s*$/);
   close (_SH_ENV);
   if (!$shell) {
      if ($ENV{'SHELL'} =~ /\/sh$|\/ksh$|\/zsh$|\/csh$/) {
	 $shell = "$ENV{'SHELL'} -c";
      } else {
	 print "SHELL not recognized!\nUsing /bin/sh...\n";
	 $shell = "/bin/sh -c";
      }
   }
   if (length($vars) > 0) {
      system "$shell \"$vars;. $command $args; set > /tmp/_sh_env$$\"";
   } else {
      system "$shell \". $command $args; set > /tmp/_sh_env$$\"";
   }

   open (_SH_ENV, "/tmp/_sh_env$$") || die "Could not open /tmp/_sh_env$$!\n";
   while (<_SH_ENV>) {
       chop;
       m/^([^=]*)=(.*)/s;
       $ENV{$1} = $2;
   }
   close (_SH_ENV);
   system "rm -f /tmp/_sh_env$$";

   foreach $key (keys(%ENV)) {
       $tmp .= "\$$key = \$ENV{'$key'};" if $key =~ /^[A-Za-z]\w*$/;
   }
   eval $tmp;
}
1;
