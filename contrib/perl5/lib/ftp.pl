#-*-perl-*-
#
# This library is no longer being maintained, and is included for backward
# compatibility with Perl 4 programs which may require it.
#
# In particular, this should not be used as an example of modern Perl
# programming techniques.
#
# Suggested alternative: Net::FTP
#
# This is a wrapper to the chat2.pl routines that make life easier
# to do ftp type work.
# Mostly by Lee McLoughlin <lmjm@doc.ic.ac.uk>
# based on original version by Alan R. Martello <al@ee.pitt.edu>
# And by A.Macpherson@bnr.co.uk for multi-homed hosts
#
# $Header: /a/swan/home/swan/staff/csg/lmjm/src/perl/mirror/RCS/ftp.pl,v 1.17 1993/04/21 10:06:54 lmjm Exp lmjm $
# $Log: ftp.pl,v $
# Revision 1.17  1993/04/21  10:06:54  lmjm
# Send all status reports to STDERR not to STDOUT (to allow use by ftpcat).
# Allow target file to be '-' meaning STDOUT
# Added ftp'quote
#
# Revision 1.16  1993/01/28  18:59:05  lmjm
# Allow socket arguemtns to come from main.
# Minor cleanups - removed old comments.
#
# Revision 1.15  1992/11/25  21:09:30  lmjm
# Added another REST return code.
#
# Revision 1.14  1992/08/12  14:33:42  lmjm
# Fail ftp'write if out of space.
#
# Revision 1.13  1992/03/20  21:01:03  lmjm
# Added in the proxy ftp code from Edwards Reed <err@cinops.xerox.com>
# Added  ftp'delete from Aaron Wohl <aw0g+@andrew.cmu.edu>
#
# Revision 1.12  1992/02/06  23:25:56  lmjm
# Moved code around so can use this as a lib for both mirror and ftpmail.
# Time out opens.  In case Unix doesn't bother to.
#
# Revision 1.11  1991/11/27  22:05:57  lmjm
# Match the response code number at the start of a line allowing
# for any leading junk.
#
# Revision 1.10  1991/10/23  22:42:20  lmjm
# Added better timeout code.
# Tried to optimise file transfer
# Moved open/close code to not leak file handles.
# Cleaned up the alarm code.
# Added $fatalerror to show wether the ftp link is really dead.
#
# Revision 1.9  1991/10/07  18:30:35  lmjm
# Made the timeout-read code work.
# Added restarting file gets.
# Be more verbose if ever have to call die.
#
# Revision 1.8  1991/09/17  22:53:16  lmjm
# Spot when open_data_socket fails and return a failure rather than dying.
#
# Revision 1.7  1991/09/12  22:40:25  lmjm
# Added Andrew Macpherson's patches for hosts without ip forwarding.
#
# Revision 1.6  1991/09/06  19:53:52  lmjm
# Relaid out the code the way I like it!
# Changed the debuggin to produce more "appropriate" messages
# Fixed bugs in the ordering of put and dir listing.
# Allow for hash printing when getting files (a la ftp).
# Added the new commands from Al.
# Don't print passwords in debugging.
#
# Revision 1.5  1991/08/29  16:23:49  lmjm
# Timeout reads from the remote ftp server.
# No longer call die expect on fatal errors.  Just return fail codes.
# Changed returns so higher up routines can tell whats happening.
# Get expect/accept in correct order for dir listing.
# When ftp_show is set then print hashes every 1k transferred (like ftp).
# Allow for stripping returns out of incoming data.
# Save last error in a global string.
#
# Revision 1.4  1991/08/14  21:04:58  lmjm
# ftp'get now copes with ungetable files.
# ftp'expect code changed such that the string_to_print is
# ignored and the string sent back from the remote system is printed
# instead.
# Implemented patches from al.  Removed spuiours tracing statements.
#
# Revision 1.3  1991/08/09  21:32:18  lmjm
# Allow for another ok code on cwd's
# Rejigger the log levels
# Send \r\n for some odd ftp daemons
#
# Revision 1.2  1991/08/09  18:07:37  lmjm
# Don't print messages unless ftp_show says to.
#
# Revision 1.1  1991/08/08  20:31:00  lmjm
# Initial revision
#

require 'chat2.pl';	# into main
eval "require 'socket.ph'" || eval "require 'sys/socket.ph'"
	|| die "socket.ph missing: $!\n";


package ftp;

if( defined( &main'PF_INET ) ){
	$pf_inet = &main'PF_INET;
	$sock_stream = &main'SOCK_STREAM;
	local($name, $aliases, $proto) = getprotobyname( 'tcp' );
	$tcp_proto = $proto;
}
else {
	# XXX hardwired $PF_INET, $SOCK_STREAM, 'tcp'
	# but who the heck would change these anyway? (:-)
	$pf_inet = 2;
	$sock_stream = 1;
	$tcp_proto = 6;
}

# If the remote ftp daemon doesn't respond within this time presume its dead
# or something.
$timeout = 30;

# Timeout a read if I don't get data back within this many seconds
$timeout_read = 20 * $timeout;

# Timeout an open
$timeout_open = $timeout;

# This is a "global" it contains the last response from the remote ftp server
# for use in error messages
$ftp'response = "";
# Also ftp'NS is the socket containing the data coming in from the remote ls
# command.

# The size of block to be read or written when talking to the remote
# ftp server
$ftp'ftpbufsize = 4096;

# How often to print a hash out, when debugging
$ftp'hashevery = 1024;
# Output a newline after this many hashes to prevent outputing very long lines
$ftp'hashnl = 70;

# If a proxy connection then who am I really talking to?
$real_site = "";

# This is just a tracing aid.
$ftp_show = 0;
sub ftp'debug
{
	$ftp_show = $_[0];
#	if( $ftp_show ){
#		print STDERR "ftp debugging on\n";
#	}
}

sub ftp'set_timeout
{
	$timeout = $_[0];
	$timeout_open = $timeout;
	$timeout_read = 20 * $timeout;
	if( $ftp_show ){
		print STDERR "ftp timeout set to $timeout\n";
	}
}


sub ftp'open_alarm
{
	die "timeout: open";
}

sub ftp'timed_open
{
	local( $site, $ftp_port, $retry_call, $attempts ) = @_;
	local( $connect_site, $connect_port );
	local( $res );

	alarm( $timeout_open );

	while( $attempts-- ){
		if( $ftp_show ){
			print STDERR "proxy connecting via $proxy_gateway [$proxy_ftp_port]\n" if $proxy;
			print STDERR "Connecting to $site";
			if( $ftp_port != 21 ){
				print STDERR " [port $ftp_port]";
			}
			print STDERR "\n";
		}
		
		if( $proxy ) {
			if( ! $proxy_gateway ) {
				# if not otherwise set
				$proxy_gateway = "internet-gateway";
			}
			if( $debug ) {
				print STDERR "using proxy services of $proxy_gateway, ";
				print STDERR "at $proxy_ftp_port\n";
			}
			$connect_site = $proxy_gateway;
			$connect_port = $proxy_ftp_port;
			$real_site = $site;
		}
		else {
			$connect_site = $site;
			$connect_port = $ftp_port;
		}
		if( ! &chat'open_port( $connect_site, $connect_port ) ){
			if( $retry_call ){
				print STDERR "Failed to connect\n" if $ftp_show;
				next;
			}
			else {
				print STDERR "proxy connection failed " if $proxy;
				print STDERR "Cannot open ftp to $connect_site\n" if $ftp_show;
				return 0;
			}
		}
		$res = &ftp'expect( $timeout,
				    120, "service unavailable to $site", 0, 
	                            220, "ready for login to $site", 1,
				    421, "service unavailable to $site, closing connection", 0);
		if( ! $res ){
			&chat'close();
			next;
		}
		return 1;
	}
	continue {
		print STDERR "Pausing between retries\n";
		sleep( $retry_pause );
	}
	return 0;
}

sub ftp'open
{
	local( $site, $ftp_port, $retry_call, $attempts ) = @_;

	$SIG{ 'ALRM' } = "ftp\'open_alarm";

	local( $ret ) = eval "&timed_open( '$site', $ftp_port, $retry_call, $attempts )";
	alarm( 0 );

	if( $@ =~ /^timeout/ ){
		return -1;
	}
	return $ret;
}

sub ftp'login
{
	local( $remote_user, $remote_password ) = @_;

	if( $proxy ){
		&ftp'send( "USER $remote_user\@$site" );
	}
	else {
		&ftp'send( "USER $remote_user" );
	}
        local( $val ) =
               &ftp'expect($timeout,
	           230, "$remote_user logged in", 1,
		   331, "send password for $remote_user", 2,

		   500, "syntax error", 0,
		   501, "syntax error", 0,
		   530, "not logged in", 0,
		   332, "account for login not supported", 0,

		   421, "service unavailable, closing connection", 0);
	if( $val == 1 ){
		return 1;
	}
	if( $val == 2 ){
		# A password is needed
		&ftp'send( "PASS $remote_password" );

		$val = &ftp'expect( $timeout,
		   230, "$remote_user logged in", 1,

		   202, "command not implemented", 0,
		   332, "account for login not supported", 0,

		   530, "not logged in", 0,
		   500, "syntax error", 0,
		   501, "syntax error", 0,
		   503, "bad sequence of commands", 0, 

		   421, "service unavailable, closing connection", 0);
		if( $val == 1){
			# Logged in
			return 1;
		}
	}
	# If I got here I failed to login
	return 0;
}

sub ftp'close
{
	&ftp'quit();
	&chat'close();
}

# Change directory
# return 1 if successful
# 0 on a failure
sub ftp'cwd
{
	local( $dir ) = @_;

	&ftp'send( "CWD $dir" );

	return &ftp'expect( $timeout,
		200, "working directory = $dir", 1,
		250, "working directory = $dir", 1,

		500, "syntax error", 0,
		501, "syntax error", 0,
                502, "command not implemented", 0,
		530, "not logged in", 0,
                550, "cannot change directory", 0,
		421, "service unavailable, closing connection", 0 );
}

# Get a full directory listing:
# &ftp'dir( remote LIST options )
# Start a list goin with the given options.
# Presuming that the remote deamon uses the ls command to generate the
# data to send back then then you can send it some extra options (eg: -lRa)
# return 1 if sucessful and 0 on a failure
sub ftp'dir_open
{
	local( $options ) = @_;
	local( $ret );
	
	if( ! &ftp'open_data_socket() ){
		return 0;
	}
	
	if( $options ){
		&ftp'send( "LIST $options" );
	}
	else {
		&ftp'send( "LIST" );
	}
	
	$ret = &ftp'expect( $timeout,
		150, "reading directory", 1,
	
		125, "data connection already open?", 0,
	
		450, "file unavailable", 0,
		500, "syntax error", 0,
		501, "syntax error", 0,
		502, "command not implemented", 0,
		530, "not logged in", 0,
	
		   421, "service unavailable, closing connection", 0 );
	if( ! $ret ){
		&ftp'close_data_socket;
		return 0;
	}
	
	# 
	# the data should be coming at us now
	#
	
	# now accept
	accept(NS,S) || die "accept failed $!";
	
	return 1;
}


# Close down reading the result of a remote ls command
# return 1 if successful and 0 on failure
sub ftp'dir_close
{
	local( $ret );

	# read the close
	#
	$ret = &ftp'expect($timeout,
        	226, "", 1,     # transfer complete, closing connection
        	250, "", 1,     # action completed

	        425, "can't open data connection", 0,
        	426, "connection closed, transfer aborted", 0,
	        451, "action aborted, local error", 0,
	        421, "service unavailable, closing connection", 0);

	# shut down our end of the socket
	&ftp'close_data_socket;

	if( ! $ret ){
		return 0;
	}

	return 1;
}

# Quit from the remote ftp server
# return 1 if successful and 0 on failure
sub ftp'quit
{
	$site_command_check = 0;
	@site_command_list = ();

	&ftp'send("QUIT");

	return &ftp'expect($timeout, 
		221, "Goodbye", 1,     # transfer complete, closing connection
	
		500, "error quitting??", 0);
}

sub ftp'read_alarm
{
	die "timeout: read";
}

sub ftp'timed_read
{
	alarm( $timeout_read );
	return sysread( NS, $buf, $ftpbufsize );
}

sub ftp'read
{
	$SIG{ 'ALRM' } = "ftp\'read_alarm";

	local( $ret ) = eval '&timed_read()';
	alarm( 0 );

	if( $@ =~ /^timeout/ ){
		return -1;
	}
	return $ret;
}

# Get a remote file back into a local file.
# If no loc_fname passed then uses rem_fname.
# returns 1 on success and 0 on failure
sub ftp'get
{
	local($rem_fname, $loc_fname, $restart ) = @_;
	
	if ($loc_fname eq "") {
		$loc_fname = $rem_fname;
	}
	
	if( ! &ftp'open_data_socket() ){
		print STDERR "Cannot open data socket\n";
		return 0;
	}

	if( $loc_fname ne '-' ){
		# Find the size of the target file
		local( $restart_at ) = &ftp'filesize( $loc_fname );
		if( $restart && $restart_at > 0 && &ftp'restart( $restart_at ) ){
			$restart = 1;
			# Make sure the file can be updated
			chmod( 0644, $loc_fname );
		}
		else {
			$restart = 0;
			unlink( $loc_fname );
		}
	}

	&ftp'send( "RETR $rem_fname" );
	
	local( $ret ) =
		&ftp'expect($timeout, 
                   150, "receiving $rem_fname", 1,

                   125, "data connection already open?", 0,

                   450, "file unavailable", 2,
                   550, "file unavailable", 2,

		   500, "syntax error", 0,
		   501, "syntax error", 0,
		   530, "not logged in", 0,

		   421, "service unavailable, closing connection", 0);
	if( $ret != 1 ){
		print STDERR "Failure on RETR command\n";

		# shut down our end of the socket
		&ftp'close_data_socket;

		return 0;
	}

	# 
	# the data should be coming at us now
	#

	# now accept
	accept(NS,S) || die "accept failed: $!";

	#
	#  open the local fname
	#  concatenate on the end if restarting, else just overwrite
	if( !open(FH, ($restart ? '>>' : '>') . $loc_fname) ){
		print STDERR "Cannot create local file $loc_fname\n";

		# shut down our end of the socket
		&ftp'close_data_socket;

		return 0;
	}

#    while (<NS>) {
#        print FH ;
#    }

	local( $start_time ) = time;
	local( $bytes, $lasthash, $hashes ) = (0, 0, 0);
	while( ($len = &ftp'read()) > 0 ){
		$bytes += $len;
		if( $strip_cr ){
			$ftp'buf =~ s/\r//g;
		}
		if( $ftp_show ){
			while( $bytes > ($lasthash + $ftp'hashevery) ){
				print STDERR '#';
				$lasthash += $ftp'hashevery;
				$hashes++;
				if( ($hashes % $ftp'hashnl) == 0 ){
					print STDERR "\n";
				}
			}
		}
		if( ! print FH $ftp'buf ){
			print STDERR "\nfailed to write data";
			return 0;
		}
	}
	close( FH );

	# shut down our end of the socket
	&ftp'close_data_socket;

	if( $len < 0 ){
		print STDERR "\ntimed out reading data!\n";

		return 0;
	}
		
	if( $ftp_show ){
		if( $hashes && ($hashes % $ftp'hashnl) != 0 ){
			print STDERR "\n";
		}
		local( $secs ) = (time - $start_time);
		if( $secs <= 0 ){
			$secs = 1; # To avoid a divide by zero;
		}

		local( $rate ) = int( $bytes / $secs );
		print STDERR "Got $bytes bytes ($rate bytes/sec)\n";
	}

	#
	# read the close
	#

	$ret = &ftp'expect($timeout, 
		226, "Got file", 1,     # transfer complete, closing connection
	        250, "Got file", 1,     # action completed
	
	        110, "restart not supported", 0,
	        425, "can't open data connection", 0,
	        426, "connection closed, transfer aborted", 0,
	        451, "action aborted, local error", 0,
		421, "service unavailable, closing connection", 0);

	return $ret;
}

sub ftp'delete
{
	local( $rem_fname, $val ) = @_;

	&ftp'send("DELE $rem_fname" );
	$val = &ftp'expect( $timeout, 
			   250,"Deleted $rem_fname", 1,
			   550,"Permission denied",0
			   );
	return $val == 1;
}

sub ftp'deldir
{
    local( $fname ) = @_;

    # not yet implemented
    # RMD
}

# UPDATE ME!!!!!!
# Add in the hash printing and newline conversion
sub ftp'put
{
	local( $loc_fname, $rem_fname ) = @_;
	local( $strip_cr );
	
	if ($loc_fname eq "") {
		$loc_fname = $rem_fname;
	}
	
	if( ! &ftp'open_data_socket() ){
		return 0;
	}
	
	&ftp'send("STOR $rem_fname");
	
	# 
	# the data should be coming at us now
	#
	
	local( $ret ) =
	&ftp'expect($timeout, 
		150, "sending $loc_fname", 1,

		125, "data connection already open?", 0,
		450, "file unavailable", 0,

		532, "need account for storing files", 0,
		452, "insufficient storage on system", 0,
		553, "file name not allowed", 0,

		500, "syntax error", 0,
		501, "syntax error", 0,
		530, "not logged in", 0,

		421, "service unavailable, closing connection", 0);

	if( $ret != 1 ){
		# shut down our end of the socket
		&ftp'close_data_socket;

		return 0;
	}


	# 
	# the data should be coming at us now
	#
	
	# now accept
	accept(NS,S) || die "accept failed: $!";
	
	#
	#  open the local fname
	#
	if( !open(FH, "<$loc_fname") ){
		print STDERR "Cannot open local file $loc_fname\n";

		# shut down our end of the socket
		&ftp'close_data_socket;

		return 0;
	}
	
	while (<FH>) {
		print NS ;
	}
	close(FH);
	
	# shut down our end of the socket to signal EOF
	&ftp'close_data_socket;
	
	#
	# read the close
	#
	
	$ret = &ftp'expect($timeout, 
		226, "file put", 1,     # transfer complete, closing connection
		250, "file put", 1,     # action completed
	
		110, "restart not supported", 0,
		425, "can't open data connection", 0,
		426, "connection closed, transfer aborted", 0,
		451, "action aborted, local error", 0,
		551, "page type unknown", 0,
		552, "storage allocation exceeded", 0,
	
		421, "service unavailable, closing connection", 0);
	if( ! $ret ){
		print STDERR "error putting $loc_fname\n";
	}
	return $ret;
}

sub ftp'restart
{
	local( $restart_point, $ret ) = @_;

	&ftp'send("REST $restart_point");

	# 
	# see what they say

	$ret = &ftp'expect($timeout, 
			   350, "restarting at $restart_point", 1,
			   
			   500, "syntax error", 0,
			   501, "syntax error", 0,
			   502, "REST not implemented", 2,
			   530, "not logged in", 0,
			   554, "REST not implemented", 2,
			   
			   421, "service unavailable, closing connection", 0);
	return $ret;
}

# Set the file transfer type
sub ftp'type
{
	local( $type ) = @_;

	&ftp'send("TYPE $type");

	# 
	# see what they say

	$ret = &ftp'expect($timeout, 
			   200, "file type set to $type", 1,
			   
			   500, "syntax error", 0,
			   501, "syntax error", 0,
			   504, "Invalid form or byte size for type $type", 0,
			   
			   421, "service unavailable, closing connection", 0);
	return $ret;
}

$site_command_check = 0;
@site_command_list = ();

# routine to query the remote server for 'SITE' commands supported
sub ftp'site_commands
{
	local( $ret );
	
	# if we havent sent a 'HELP SITE', send it now
	if( !$site_command_check ){
	
		$site_command_check = 1;
	
		&ftp'send( "HELP SITE" );
	
		# assume the line in the HELP SITE response with the 'HELP'
		# command is the one for us
		$ret = &ftp'expect( $timeout,
			".*HELP.*", "", "\$1",
			214, "", "0",
			202, "", "0" );
	
		if( $ret eq "0" ){
			print STDERR "No response from HELP SITE\n" if( $ftp_show );
		}
	
		@site_command_list = split(/\s+/, $ret);
	}
	
	return @site_command_list;
}

# return the pwd, or null if we can't get the pwd
sub ftp'pwd
{
	local( $ret, $cwd );

	&ftp'send( "PWD" );

	# 
	# see what they say

	$ret = &ftp'expect( $timeout, 
			   257, "working dir is", 1,
			   500, "syntax error", 0,
			   501, "syntax error", 0,
			   502, "PWD not implemented", 0,
	                   550, "file unavailable", 0,

			   421, "service unavailable, closing connection", 0 );
	if( $ret ){
		if( $ftp'response =~ /^257\s"(.*)"\s.*$/ ){
			$cwd = $1;
		}
	}
	return $cwd;
}

# return 1 for success, 0 for failure
sub ftp'mkdir
{
	local( $path ) = @_;
	local( $ret );

	&ftp'send( "MKD $path" );

	# 
	# see what they say

	$ret = &ftp'expect( $timeout, 
			   257, "made directory $path", 1,
			   
			   500, "syntax error", 0,
			   501, "syntax error", 0,
			   502, "MKD not implemented", 0,
			   530, "not logged in", 0,
	                   550, "file unavailable", 0,

			   421, "service unavailable, closing connection", 0 );
	return $ret;
}

# return 1 for success, 0 for failure
sub ftp'chmod
{
	local( $path, $mode ) = @_;
	local( $ret );

	&ftp'send( sprintf( "SITE CHMOD %o $path", $mode ) );

	# 
	# see what they say

	$ret = &ftp'expect( $timeout, 
			   200, "chmod $mode $path succeeded", 1,
			   
			   500, "syntax error", 0,
			   501, "syntax error", 0,
			   502, "CHMOD not implemented", 0,
			   530, "not logged in", 0,
	                   550, "file unavailable", 0,

			   421, "service unavailable, closing connection", 0 );
	return $ret;
}

# rename a file
sub ftp'rename
{
	local( $old_name, $new_name ) = @_;
	local( $ret );

	&ftp'send( "RNFR $old_name" );

	# 
	# see what they say

	$ret = &ftp'expect( $timeout, 
			   350, "", 1,
			   
			   500, "syntax error", 0,
			   501, "syntax error", 0,
			   502, "RNFR not implemented", 0,
			   530, "not logged in", 0,
	                   550, "file unavailable", 0,
	                   450, "file unavailable", 0,
			   
			   421, "service unavailable, closing connection", 0);


	# check if the "rename from" occurred ok
	if( $ret ) {
		&ftp'send( "RNTO $new_name" );
	
		# 
		# see what they say
	
		$ret = &ftp'expect( $timeout, 
			           250, "rename $old_name to $new_name", 1, 

				   500, "syntax error", 0,
				   501, "syntax error", 0,
				   502, "RNTO not implemented", 0,
				   503, "bad sequence of commands", 0,
				   530, "not logged in", 0,
		                   532, "need account for storing files", 0,
		                   553, "file name not allowed", 0,
				   
				   421, "service unavailable, closing connection", 0);
	}

	return $ret;
}


sub ftp'quote
{
      local( $cmd ) = @_;

      &ftp'send( $cmd );

      return &ftp'expect( $timeout, 
              200, "Remote '$cmd' OK", 1,
              500, "error in remote '$cmd'", 0 );
}

# ------------------------------------------------------------------------------
# These are the lower level support routines

sub ftp'expectgot
{
	($ftp'response, $ftp'fatalerror) = @_;
	if( $ftp_show ){
		print STDERR "$ftp'response\n";
	}
}

#
#  create the list of parameters for chat'expect
#
#  ftp'expect(time_out, {value, string_to_print, return value});
#     if the string_to_print is "" then nothing is printed
#  the last response is stored in $ftp'response
#
# NOTE: lmjm has changed this code such that the string_to_print is
# ignored and the string sent back from the remote system is printed
# instead.
#
sub ftp'expect {
	local( $ret );
	local( $time_out );
	local( $expect_args );
	
	$ftp'response = '';
	$ftp'fatalerror = 0;

	@expect_args = ();
	
	$time_out = shift(@_);
	
	while( @_ ){
		local( $code ) = shift( @_ );
		local( $pre ) = '^';
		if( $code =~ /^\d/ ){
			$pre =~ "[.|\n]*^";
		}
		push( @expect_args, "$pre(" . $code . " .*)\\015\\n" );
		shift( @_ );
		push( @expect_args, 
			"&ftp'expectgot( \$1, 0 ); " . shift( @_ ) );
	}
	
	# Treat all unrecognised lines as continuations
	push( @expect_args, "^(.*)\\015\\n" );
	push( @expect_args, "&ftp'expectgot( \$1, 0 ); 100" );
	
	# add patterns TIMEOUT and EOF
	
	push( @expect_args, 'TIMEOUT' );
	push( @expect_args, "&ftp'expectgot( \"timed out\", 1 ); 0" );
	
	push( @expect_args, 'EOF' );
	push( @expect_args, "&ftp'expectgot( \"remote server gone away\", 1 ); 0" );
	
	if( $ftp_show > 9 ){
		&printargs( $time_out, @expect_args );
	}
	
	$ret = &chat'expect( $time_out, @expect_args );
	if( $ret == 100 ){
		# we saw a continuation line, wait for the end
		push( @expect_args, "^.*\n" );
		push( @expect_args, "100" );
	
		while( $ret == 100 ){
			$ret = &chat'expect( $time_out, @expect_args );
		}
	}
	
	return $ret;
}

#
#  opens NS for io
#
sub ftp'open_data_socket
{
	local( $ret );
	local( $hostname );
	local( $sockaddr, $name, $aliases, $proto, $port );
	local( $type, $len, $thisaddr, $myaddr, $a, $b, $c, $d );
	local( $mysockaddr, $family, $hi, $lo );
	
	
	$sockaddr = 'S n a4 x8';
	chop( $hostname = `hostname` );
	
	$port = "ftp";
	
	($name, $aliases, $proto) = getprotobyname( 'tcp' );
	($name, $aliases, $port) = getservbyname( $port, 'tcp' );
	
#	($name, $aliases, $type, $len, $thisaddr) =
#	gethostbyname( $hostname );
	($a,$b,$c,$d) = unpack( 'C4', $chat'thisaddr );
	
#	$this = pack( $sockaddr, &main'AF_INET, 0, $thisaddr );
	$this = $chat'thisproc;
	
	socket(S, $pf_inet, $sock_stream, $proto ) || die "socket: $!";
	bind(S, $this) || die "bind: $!";
	
	# get the port number
	$mysockaddr = getsockname(S);
	($family, $port, $myaddr) = unpack( $sockaddr, $mysockaddr );
	
	$hi = ($port >> 8) & 0x00ff;
	$lo = $port & 0x00ff;
	
	#
	# we MUST do a listen before sending the port otherwise
	# the PORT may fail
	#
	listen( S, 5 ) || die "listen";
	
	&ftp'send( "PORT $a,$b,$c,$d,$hi,$lo" );
	
	return &ftp'expect($timeout,
		200, "PORT command successful", 1,
		250, "PORT command successful", 1 ,

		500, "syntax error", 0,
		501, "syntax error", 0,
		530, "not logged in", 0,

		421, "service unavailable, closing connection", 0);
}
	
sub ftp'close_data_socket
{
	close(NS);
}

sub ftp'send
{
	local($send_cmd) = @_;
	if( $send_cmd =~ /\n/ ){
		print STDERR "ERROR, \\n in send string for $send_cmd\n";
	}
	
	if( $ftp_show ){
		local( $sc ) = $send_cmd;

		if( $send_cmd =~ /^PASS/){
			$sc = "PASS <somestring>";
		}
		print STDERR "---> $sc\n";
	}
	
	&chat'print( "$send_cmd\r\n" );
}

sub ftp'printargs
{
	while( @_ ){
		print STDERR shift( @_ ) . "\n";
	}
}

sub ftp'filesize
{
	local( $fname ) = @_;

	if( ! -f $fname ){
		return -1;
	}

	return (stat( _ ))[ 7 ];
	
}

# make this package return true
1;
