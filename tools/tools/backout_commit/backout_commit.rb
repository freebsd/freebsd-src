#!/usr/bin/env ruby -w

# $FreeBSD$

# Please note, that this utility must be kept in sync with
# CVSROOT/log_accum.pl.  If someone has a different output from their
# mail client when saving e-mails as text files, feel free to hack it
# in as an option.
#
# If someone would like to hack in the ability to generate diffs based
# off of this script, by all means, be my guest.

require 'getoptlong'

$basedir        = '/usr'
$backout_script = "backout-#{Time.now.strftime("%Y-%m-%d-%H-%M")}.sh"
$commit_authors = []
$commit_dates   = []
$commit_file    = nil
$commit_message = nil
$cvsbin         = nil
$cvs_path       = '/usr/bin/cvs'
$cvsrc_ignore   = true
$debug          = 0
$echo_path      = '/usr/bin/echo'
$echo_warnings  = true
$force_script_edit = false
$force_remove   = false
$output         = $stdout
$quiet_script   = false
$shell_path     = '/bin/sh'
$shell_args     = '--'

def debug(level, *msgs)
  if level <= $debug
    if $debug > 1
      $output.puts "DEBUG(#{level}): #{msgs.shift}"
    else
      $output.puts msgs.shift
    end

    for msg in msgs
      $output.puts "\t  #{msg}"
    end
  end
end # def debug()


def usage(msg, info = nil)
  out = (msg.nil? ? $stdout : $stderr)
  out.puts "#{File.basename($0)} usage:" << (msg.nil? ? '' : " #{msg}")
  out.puts "#{info}" unless info.nil?
  out.puts ""
  out.puts "  -s, --backout-script=<file>  Specifies the filename of the script"
  out.puts "  -D, --basedir=<dir>          Specifies the base directory [/usr]"
  out.puts "  -a, --commit-author=<uid>    Forces a commit author"
  out.puts "  -d, --commit-date=<date>     Forces a commit date"
  out.puts "  -m, --commit-file=<path>     Specifies a commit message file"
  out.puts "  -M, --commit-message=<msg>   Specifies a commit message"
  out.puts "  -c, --cvs-path=<path>        Specifies the CVS binary to be used [cvs]"
  out.puts "  -C, --cvsrc-ignore=<bool>    If true, will ignore options in ~/.cvsrc"
  out.puts "  -e, --echo-path=<path>       Specifies the path to echo"
  out.puts "  -f, --force-remove=<bool>    If true, removes new files [false]"
  out.puts "  -F, --force-edit=<bool>      If true, add -C to the shell arguments in"
  out.puts "                               the backout script if the shell is sh,"
  out.puts "                               which forces an edit of the script"
  out.puts "  -O, --output=<stdio>         Specifies what fd to direct the output to"
  out.puts "  -A, --shell-args=<string>    Specifies the shell arguments to be used"
  out.puts "  -S, --shell-path=<path>      Specifies the shell to be used [/bin/sh]"
  out.puts "  -W, --warnings=<bool>        Turns on or off warnings [true]"
  exit(msg.nil? ? 0 : 1)
end


OPTION_LIST = [
  ['--backout-script','-s', GetoptLong::REQUIRED_ARGUMENT],
  ['--basedir','-D', GetoptLong::REQUIRED_ARGUMENT],
  ['--commit-author','-a', GetoptLong::REQUIRED_ARGUMENT],
  ['--commit-date','-d', GetoptLong::REQUIRED_ARGUMENT],
  ['--commit-file','-m', GetoptLong::REQUIRED_ARGUMENT],
  ['--commit-message','-M', GetoptLong::REQUIRED_ARGUMENT],
  ['--cvs-path','-c',GetoptLong::REQUIRED_ARGUMENT],
  ['--cvsrc-ignore','-C',GetoptLong::REQUIRED_ARGUMENT],
  ['--echo-path','-e',GetoptLong::REQUIRED_ARGUMENT],
  ['--force-edit','-F',GetoptLong::REQUIRED_ARGUMENT],
  ['--force-remove','-f',GetoptLong::REQUIRED_ARGUMENT],
  ['--output', '-O', GetoptLong::REQUIRED_ARGUMENT],
  ['--quiet-script','-q',GetoptLong::REQUIRED_ARGUMENT],
  ['--shell-args','-A',GetoptLong::REQUIRED_ARGUMENT],
  ['--shell-path','-S',GetoptLong::REQUIRED_ARGUMENT],
  ['--warnings','-w', GetoptLong::REQUIRED_ARGUMENT],
]

opt_parser = GetoptLong.new(*OPTION_LIST)
opt_parser.quiet = true

begin
  opt_parser.each do |opt,arg|
    case opt
    when '--backout-script'
      debug(3, "backout script was #{$backout_script.inspect} : is #{arg.inspect}")
      $backout_script = arg
    when '--basedir'
      debug(3, "base directory was #{$basedir.inspect} : is #{arg.inspect}")
      $basedir = arg
    when '--commit-author'
      debug(3, "commit author #{arg.inspect} added to list")
      $commit_authors.push(arg.dup)
    when '--commit-date'
      debug(3, "commit date #{arg.inspect} added to list")
      $commit_date.push(arg.dup)
    when '--commit-file'
      debug(3, "commit file was #{$commit_file.inspect} : is #{arg.inspect}")
      $commit_file = arg
    when '--commit-message'
      debug(3, "commit message was #{$commit_message.inspect} : is #{arg.inspect}")
      $commit_message = arg
    when '--cvs-path'
      debug(3, "cvs path was #{$cvs_path.inspect} : is #{arg.inspect}")
      $cvs_path = arg
    when '--cvsrc-ignore'
      if arg =~ /true|yes/i
	$cvsrc_ignore = true
      elsif arg =~ /false|no/i
	$cvsrc_ignore = false
      else
	usage("#{opt}: unknown bool format \"#{arg}\"", "Valid options are \"true\", \"false\", \"yes\", or \"no\"")
      end
      debug(3, "ignoring of ~/.cvsrc is set to #{$cvsrc_ignore.inspect}")
    when '--echo-path'
      debug(3, "echo path was #{$echo_path.inspect} : is #{arg.inspect}")
      $echo_path = arg
    when '--force-edit'
      if arg =~ /true|yes/i
	$force_script_edit = true
      elsif arg =~ /false|no/i
	$force_script_edit = false
      else
	usage("#{opt}: unknown bool format \"#{arg}\"", "Valid options are \"true\", \"false\", \"yes\", or \"no\"")
      end
      debug(3, "force edit of backout script is set to #{$force_script_edit.inspect}")
    when '--force-remove'
      if arg =~ /true|yes/i
	$force_remove = true
      elsif arg =~ /false|no/i
	$force_remove = false
      else
	usage("#{opt}: unknown bool format \"#{arg}\"", "Valid options are \"true\", \"false\", \"yes\", or \"no\"")
      end
      debug(3, "force removal of files is set to #{$force_remove.inspect}")
    when '--output'
      case arg
      when 'stdout'
        $output = $stdout
      when 'stderr'
        $output = $stderr
      else
        usage("#{opt}: unknown output format","Valid outputs are \"stdout\" and \"stderr\"")
      end
      debug(3, "output set to #{arg}")
    when '--quiet-script'
      if arg =~ /true|yes/i
	$quiet_script = true
      elsif arg =~ /false|no/i
	$quiet_script = false
      else
	usage("#{opt}: unknown bool format \"#{arg}\"", "Valid options are \"true\", \"false\", \"yes\", or \"no\"")
      end
      debug(3, "quiet script is set to #{$quiet_script.inspect}")
    when '--shell-args'
      debug(3, "shell args were #{$shell_args.inspect} : is #{arg.inspect}")
      $shell_args = arg
    when '--shell-path'
      debug(3, "shell path was #{$shell_path.inspect} : is #{arg.inspect}")
      $shell_path = arg
    when '--warnings'
      if arg =~ /true|yes/i
	$echo_warnings = true
      elsif arg =~ /false|no/i
	$echo_warnings = false
      else
	usage("#{opt}: unknown bool format \"#{arg}\"", "Valid options are \"true\", \"false\", \"yes\", or \"no\"")
      end
      debug(3, "warnings are set to #{$echo_warnings.inspect}")
    end
  end
rescue GetoptLong::InvalidOption
  usage("invalid argument")
rescue GetoptLong::MissingArgument
  usage("missing argument")
rescue GetoptLong::NeedlessArgument => msg
  usage("passed an extra argument: #{msg}")
end

debug(3, "Verbosity set to: #{$debug}")

$cvsbin = $cvs_path
$cvsbin << " -f" if $cvsrc_ignore  

if ARGV.length < 1
  usage("require a commit message to parse")
end

$output.puts("Backout directory:\t#{$basedir}")
$output.puts("Backout script:\t\t#{$backout_script}")
$output.puts("")

# Backout script - to be run by hand
File.open($backout_script, "w+") do |f|
  removals = []
  updates = []
  files = []

  f.puts("#!#{$shell_path}#{($force_script_edit && $shell_path == '/bin/sh') ? ' -C' : ''} #{$shell_args}")
  f.puts()
  f.puts("# Generated at: #{Time.now()}")
  f.puts("# Generated by: #{ENV['USER']}\@#{ENV['HOST']}")
  f.puts()
  f.puts("BASEDIR=#{$basedir}")
  f.puts('if [ $BASEDIR != $PWD ]; then')
  f.puts('  echo "Please change to $BASEDIR before running this shell script"')
  f.puts('  exit 1')
  f.puts('fi')
  f.puts()

  author_regexp  = Regexp.new(/^([^\ ]+)\s+([\d]{4})\/([\d]{2})\/([\d]{2}) ([\d]{2}):([\d]{2}):([\d]{2}) ([A-Z]{3})$/)
  file_regexp    = Regexp.new(/^  ([\d\.]+)\s+\+([\d]+) \-([\d]+)\s+(.*?)$/)
  newdead_regexp = Regexp.new(/^(.*?) \((new|dead)\)$/)
  rev_regexp     = Regexp.new(/^  Revision  Changes    Path$/)

  for email_file in ARGV
    File.open(email_file) do |e|
      $output.print("Scanning through #{email_file}...")
      found_files = false
      for line in e
	line.chomp!
	if found_files == false
	  amd = author_regexp.match(line)
	  if !amd.nil?
	    $commit_authors.push(amd[1].dup)
	    $commit_dates.push(Time.local(*amd[2..7]).dup)
	  elsif rev_regexp.match(line)
	    found_files = true
	  end
	else # if found_files
	  md = file_regexp.match(line)
	  next if md.nil?

	  filename = md[4]
	  ndmd = newdead_regexp.match(filename)
	  if !ndmd.nil?
	    filename = ndmd[1]
	    if ndmd[2] == 'new'
	      removals.push(filename)
	      f.puts("#{$force_remove ? '' : '# '}#{$echo_path} -n \"Removing #{filename}...\"") if !$quiet_script
	      f.puts("#{$force_remove ? '' : '# '}#{$cvsbin} rm -f #{filename}")
	      f.puts("#{$force_remove ? '' : '# '}#{$echo_path} \"done.\"") if !$quiet_script
	      f.puts()
	      files.push(filename)
	      next
	    end
	  end
	  f.puts("#{$echo_path} -n \"Updating #{filename} to #{md[1]}...\"") if !$quiet_script
	  f.puts("#{$cvsbin} up -p -r #{md[1]} #{filename} > #{filename}")
	  f.puts("#{$echo_path} \"done.\"") if !$quiet_script
	  f.puts()
	  files.push(filename)
	end # if found_files
      end # for line in..
      $output.puts("done.")
    end # File.open()
  end # for email_file in ARGV...

  if removals.length > 0 && $force_remove == false
    f.puts("#{$echo_warnings ? '' : '# '}#{$echo_path} \"You may want to remove the following file#{removals.length > 1 ? 's' : ''}:\"")
    for filename in removals
      f.puts("#{$echo_warnings ? '' : '# '}#{$echo_path} \"\t#{filename}\"")
    end
    f.puts()
    f.puts("#{$echo_warnings ? '' : '# '}#{$echo_path} \"There is code in #{$backout_script} to remove #{removals.length > 1 ? 'these files' : 'this file'} for you,\"")
    f.puts("#{$echo_warnings ? '' : '# '}#{$echo_path} \"just uncomment them or pass the option --force-remove=true to #{$0}.\"")
  end

  f.puts()
  f.puts("# # # Uncomment the following line to commit the backout.")
  f.puts("# # #{$echo_path} -n \"Committing backout...\"") if !$quiet_script
  if !$commit_message.nil?
    if $commit_message.empty? or $commit_message =~ /^default|no|yes|true|false/i
      $commit_message = "Backout of commit by #{$commit_authors.join(', ')} done on #{$commit_dates.join(', ')} because\n[___FILL_IN_THE_BLANK___]\n"
    end

    f.puts()
    f.puts("# # # EDIT COMMIT MESSAGE HERE")
    f.puts("CVSCOMMITMSG=<<DONTUSECVSMSG")
    f.puts($commit_message)
    f.puts('DONTUSECVSMSG')
    f.puts()
  elsif !$commit_file.nil?
    f.puts("if [ ! -r #{$commit_file} ]; then")
    f.puts("  #{$echo_path} \"The commit message file #{$commit_file} is not readable,\"")
    f.puts("  #{$echo_path} \"please fix this and re-run the script.\"")
    f.puts("  exit 1")
    f.puts("fi")
    f.puts()
  end

  f.print("# # #{$cvsbin} ci")
  if !$commit_message.nil?
    f.print(" -m \"$CVSCOMMITMSG\"")
  elsif !$commit_file.nil?
    f.print(" -F \"#{$commit_file}\"")
  end
  f.puts(" #{files.join(' ')}")

  if !$quiet_script
    if $commit_message.nil? and $commit_file.nil?
      f.print("# # #{$echo_path} \"Commit complete.  Backout should be complete.  Please check to verify.\"")
    else
      f.puts("# # #{$echo_path} \"done.\"")
    end
  end
end # File.open()

$output.puts()
$output.puts("Change to #{$basedir} and run this script.  Please look through this script and")
$output.puts("make changes as necessary.  There are commented out commands available")
$output.puts("in the script.")
$output.puts()
if !$commit_message.nil?
  $output.puts("If you scroll to the bottom of #{$backout_script} you should be able to")
  $output.puts("find a HERE document with your commit message, if you would like to make")
  $output.puts("any further changes to your message.")
  $output.puts()
end
if !$commit_file.nil?
  begin
    stat = File.stat($commit_file)
  rescue Errno::ENOENT
    $output.puts("The output file specified, \"#{$commit_file}\" DOES NOT EXIST!!!  Please be sure to")
    $output.puts("create/edit the file \"#{$commit_file}\" before you run this script")
    $output.puts()
  end
end
$output.puts("Example script usage:")
$output.puts("\tmv #{$backout_script} #{$basedir}")
$output.puts("\tcd #{$basedir}")
$output.puts("\tless #{$backout_script}")
$output.puts("\t#{$shell_path} #{$backout_script}")
$output.puts("\trm -f #{$backout_script}")
$output.puts()
