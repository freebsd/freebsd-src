# Copyright (C) 2010 by the Massachusetts Institute of Technology.
# All rights reserved.

# Export of this software from the United States of America may
#   require a specific license from the United States Government.
#   It is the responsibility of any person or organization contemplating
#   export to obtain such a license before exporting.
#
# WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
# distribute this software and its documentation for any purpose and
# without fee is hereby granted, provided that the above copyright
# notice appear in all copies and that both that copyright notice and
# this permission notice appear in supporting documentation, and that
# the name of M.I.T. not be used in advertising or publicity pertaining
# to distribution of the software without specific, written prior
# permission.  Furthermore if you modify this software you must label
# your software as modified software and not distribute it in such a
# fashion that it might be confused with the original M.I.T. software.
# M.I.T. makes no representations about the suitability of
# this software for any purpose.  It is provided "as is" without express
# or implied warranty.

"""A module for krb5 test scripts

To run test scripts during "make check" (if Python 2.5 or later is
available), add rules like the following to Makefile.in:

    check-pytests::
	$(RUNPYTEST) $(srcdir)/t_testname.py $(PYTESTFLAGS)

A sample test script:

    from k5test import *

    # Run a test program under a variety of configurations:
    for realm in multipass_realms():
        realm.run(['./testprog', 'arg'])

    # Run a test server and client under just the default configuration:
    realm = K5Realm()
    realm.start_server(['./serverprog'], 'starting...')
    realm.run(['./clientprog', realm.host_princ])

    # Inform framework that tests completed successfully.
    success('World peace and cure for cancer')

By default, the realm will have:

* The name KRBTEST.COM
* Listener ports starting at 61000
* krb5.conf and kdc.conf files
* A fresh DB2 KDB
* Running krb5kdc (but not kadmind)
* Principals named realm.user_princ and realm.admin_princ; call
  password('user') and password('admin') to get the password
* Credentials for realm.user_princ in realm.ccache
* Admin rights for realm.admin_princ in the kadmind acl file
* A host principal named realm.host_princ with a random key
* A keytab for the host principal in realm.keytab

The realm's behaviour can be modified with the following constructor
keyword arguments:

* realm='realmname': Override the realm name

* portbase=NNN: Override the listener port base; currently three ports are
  used

* testdir='dirname': Override the storage area for the realm's files
  (path may be specified relative to the current working dir)

* krb5_conf={ ... }: krb5.conf options, expressed as a nested
  dictionary, to be merged with the default krb5.conf settings.  A key
  may be mapped to None to delete a setting from the defaults.  A key
  may be mapped to a list in order to create multiple settings for the
  same variable name.  Keys and values undergo the following template
  substitutions:

    - $realm:    The realm name
    - $testdir:  The realm storage directory (absolute path)
    - $buildtop: The root of the build directory
    - $srctop:   The root of the source directory
    - $plugins:  The plugin directory in the build tree
    - $certs:    The PKINIT certificate directory in the source tree
    - $hostname: The FQDN of the host
    - $port0:    The first listener port (portbase)
    - ...
    - $port9:    The tenth listener port (portbase + 9)

  When choosing ports, note the following:

    - port0 is used in the default krb5.conf for the KDC
    - port1 is used in the default krb5.conf for kadmind
    - port2 is used in the default krb5.conf for kpasswd
    - port3 is used in the default krb5.conf for kpropd
    - port4 is used in the default krb5.conf for iprop (in kadmind)
    - port5 is the return value of realm.server_port()

* kdc_conf={...}: kdc.conf options, expressed as a nested dictionary,
  to be merged with the default kdc.conf settings.  The same
  conventions and substitutions for krb5_conf apply.

* create_kdb=False: Don't create a KDB.  Implicitly disables all of
  the other options since they all require a KDB.

* krbtgt_keysalt='enctype:salttype': After creating the KDB,
  regenerate the krbtgt key using the specified key/salt combination,
  using a kadmin.local cpw query.

* create_user=False: Don't create the user principal.  Implies
  get_creds=False.

* create_host=False: Don't create the host principal or the associated
  keytab.

* start_kdc=False: Don't start the KDC.  Implies get_creds=False.

* start_kadmind=True: Start kadmind.

* get_creds=False: Don't get user credentials.

* bdb_only=True: Use the DB2 KDB module even if K5TEST_LMDB is set in
  the environment.

* pkinit=True: Configure a PKINIT anchor and KDC certificate.

Scripts may use the following functions and variables:

* fail(message): Display message (plus leading marker and trailing
  newline) and explanatory messages about debugging.

* success(message): Indicate that the test script has completed
  successfully.  Suppresses the display of explanatory debugging
  messages in the on-exit handler.  message should briefly summarize
  the operations tested; it will only be displayed (with leading
  marker and trailing newline) if the script is running verbosely.

* skipped(whatmsg, whymsg): Indicate that some tests were skipped.
  whatmsg should concisely say what was skipped (e.g. "LDAP KDB
  tests") and whymsg should give the reason (e.g. "because LDAP module
  not built").

* skip_rest(message): Indicate that some tests were skipped, then exit
  the current script.

* output(message, force_verbose=False): Place message (without any
  added newline) in testlog, and write it to stdout if running
  verbosely.

* mark(message): Place a divider message in the test output, to make
  it easier to determine what part of the test script a command
  invocation belongs to.  The last mark message will also be displayed
  if a command invocation fails.  Do not include a newline in message.

* which(progname): Return the location of progname in the executable
  path, or None if it is not found.

* password(name): Return a weakly random password based on name.  The
  password will be consistent across calls with the same name.

* canonicalize_hostname(name, rdns=True): Return the DNS
  canonicalization of name, optionally using reverse DNS.  On error,
  return name converted to lowercase.

* stop_daemon(proc): Stop a daemon process started with
  realm.start_server() or realm.start_in_inetd().  Only necessary if
  the port needs to be reused; daemon processes will be stopped
  automatically when the script exits.

* multipass_realms(**keywords): This is an iterator function.  Yields
  a realm for each of the standard test passes, each of which alters
  the default configuration in some way to exercise different parts of
  the krb5 code base.  keywords may contain any K5Realm initializer
  keyword with the exception of krbtgt_keysalt, which will not be
  honored.  If keywords contains krb5_conf and/or kdc_conf fragments,
  they will be merged with the default and per-pass specifications.

* multidb_realms(**keywords): Yields a realm for multiple DB modules.
  Currently DB2 and LMDB are included.  Ideally LDAP would be
  included, but setting up a test LDAP server currently requires a
  one-second delay, so all LDAP tests are currently confined to
  t_kdb.py.  keywords may contain any K5Realm initializer.

* cross_realms(num, xtgts=None, args=None, **keywords): This function
  returns a list of num realms, where each realm's configuration knows
  how to contact all of the realms.  By default, each realm will
  contain cross TGTs in both directions for all other realms; this
  default may be overridden by specifying a collection of tuples in
  the xtgts parameter, where each tuple is a pair of zero-based realm
  indexes, indicating that the first realm can authenticate to the
  second (i.e. krbtgt/secondrealm@firstrealm exists in both realm's
  databases).  If args is given, it should be a list of keyword
  arguments specific to each realm; these will be merged with the
  global keyword arguments passed to cross_realms, with specific
  arguments taking priority.

* buildtop: The top of the build directory (absolute path).

* srctop: The top of the source directory (absolute path).

* plugins: The plugin directory in the build tree (absolute path).

* pkinit_enabled: True if the PKINIT plugin module is present in the
  build directory.

* pkinit_certs: The directory containing test PKINIT certificates.

* hostname: The local hostname as it will initially appear in
  krb5_sname_to_principal() results.  (Shortname qualification is
  turned off in the test environment to make this value easy to
  discover from Python.)

* null_input: A file opened to read /dev/null.

* args: Positional arguments left over after flags are processed.

* runenv: The contents of $srctop/runenv.py, containing a dictionary
  'env' which specifies additional variables to be added to the realm
  environment, and a variable 'tls_impl', which indicates which TLS
  implementation (if any) is being used by libkrb5's support for
  contacting KDCs and kpasswd servers over HTTPS.

* verbose: Whether the script is running verbosely.

* testpass: The command-line test pass argument.  The script does not
  need to examine this argument in most cases; it will be honored in
  multipass_realms().

* Pathname variables for programs within the build directory:
  - krb5kdc
  - kadmind
  - kadmin
  - kadminl (kadmin.local)
  - kdb5_ldap_util
  - kdb5_util
  - ktutil
  - kinit
  - klist
  - kswitch
  - kvno
  - kdestroy
  - kpasswd
  - t_inetd
  - kproplog
  - kpropd
  - kprop

Scripts may use the following realm methods and attributes:

* realm.run(args, env=None, **keywords): Run a command in a specified
  environment (or the realm's environment by default), obeying the
  command-line debugging options.  Fail if the command does not return
  0.  Log the command output appropriately, and return it as a single
  multi-line string.  Keyword arguments can contain input='string' to
  send an input string to the command, expected_code=N to expect a
  return code other than 0, expected_msg=MSG to expect a substring in
  the command output, and expected_trace=('a', 'b', ...) to expect an
  ordered series of line substrings in the command's KRB5_TRACE
  output, or return_trace=True to return a tuple of the command output
  and the trace output.

* realm.kprop_port(): Returns a port number based on realm.portbase
  intended for use by kprop and kpropd.

* realm.server_port(): Returns a port number based on realm.portbase
  intended for use by server processes.

* realm.start_server(args, sentinel, env=None): Start a daemon
  process.  Wait until sentinel appears as a substring of a line in
  the server process's stdout or stderr (which are folded together).
  Returns a subprocess.Popen object which can be passed to
  stop_daemon() to stop the server, or used to read from the server's
  output.

* realm.start_in_inetd(args, port=None, env=None): Begin a t_inetd
  process which will spawn a server process after accepting a client
  connection.  If port is not specified, realm.server_port() will be
  used.  Returns a process object which can be passed to stop_daemon()
  to stop the server.

* realm.create_kdb(): Create a new KDB.

* realm.start_kdc(args=[], env=None): Start a krb5kdc process.  Errors
  if a KDC is already running.  If args is given, it contains a list
  of additional krb5kdc arguments.

* realm.stop_kdc(): Stop the krb5kdc process.  Errors if no KDC is
  running.

* realm.start_kadmind(env=None): Start a kadmind process.  Errors if a
  kadmind is already running.

* realm.stop_kadmind(): Stop the kadmind process.  Errors if no
  kadmind is running.

* realm.stop(): Stop any daemon processes running on behalf of the
  realm.

* realm.addprinc(princname, password=None): Using kadmin.local, create
  a principal in the KDB named princname, with either a random or
  specified key.

* realm.extract_keytab(princname, keytab): Using kadmin.local, create
  a keytab for princname in the filename keytab.  Uses the -norandkey
  option to avoid re-randomizing princname's key.

* realm.kinit(princname, password=None, flags=[]): Acquire credentials
  for princname using kinit, with additional flags [].  If password is
  specified, it will be used as input to the kinit process; otherwise
  flags must cause kinit not to need a password (e.g. by specifying a
  keytab).

* realm.pkinit(princ, **keywords): Acquire credentials for princ,
  supplying a PKINIT identity of the basic user test certificate
  (matching user@KRBTEST.COM).

* realm.klist(client_princ, service_princ=None, ccache=None): Using
  klist, list the credentials cache ccache (must be a filename;
  self.ccache if not specified) and verify that the output shows
  credentials for client_princ and service_princ (self.krbtgt_princ if
  not specified).

* realm.klist_keytab(princ, keytab=None): Using klist, list keytab
  (must be a filename; self.keytab if not specified) and verify that
  the output shows the keytab name and principal name.

* realm.prep_kadmin(princname=None, password=None, flags=[]): Populate
  realm.kadmin_ccache with a ticket which can be used to run kadmin.
  If princname is not specified, realm.admin_princ and its default
  password will be used.

* realm.run_kadmin(args, **keywords): Run the specified query in
  kadmin, using realm.kadmin_ccache to authenticate.  Accepts the same
  keyword arguments as run.

* realm.special_env(name, has_kdc_conf, krb5_conf=None,
  kdc_conf=None): Create an environment with a modified krb5.conf
  and/or kdc.conf.  The specified krb5_conf and kdc_conf fragments, if
  any, will be merged with the realm's existing configuration.  If
  has_kdc_conf is false, the new environment will have no kdc.conf.
  The environment returned by this method can be used with realm.run()
  or similar methods.

* realm.start_kpropd(env, args=[]): Start a kpropd process.  Pass an
  environment created with realm.special_env() for the replica.  If
  args is given, it contains a list of additional kpropd arguments.
  Returns a handle to the kpropd process.

* realm.run_kpropd_once(env, args=[]): Run kpropd once, using the -t
  flag.  Pass an environment created with realm.special_env() for the
  replica.  If args is given, it contains a list of additional kpropd
  arguments.  Returns the kpropd output.

* realm.realm: The realm's name.

* realm.testdir: The realm's storage directory (absolute path).

* realm.portbase: The realm's first listener port.

* realm.user_princ: The principal name user@<realmname>.

* realm.admin_princ: The principal name user/admin@<realmname>.

* realm.host_princ: The name of the host principal for this machine,
  with realm.

* realm.nfs_princ: The name of the nfs principal for this machine,
  with realm.

* realm.krbtgt_princ: The name of the krbtgt principal for the realm.

* realm.keytab: A keytab file in realm.testdir.  Initially contains a
  host keytab unless disabled by the realm construction options.

* realm.client_keytab: A keytab file in realm.testdir.  Initially
  nonexistent.

* realm.ccache: A ccache file in realm.testdir.  Initially contains
  credentials for user unless disabled by the realm construction
  options.

* realm.kadmin_ccache: The ccache file initialized by prep_kadmin and
  used by run_kadmin.

* env: The realm's environment, extended from os.environ to point at
  the realm's config files and the build tree's shared libraries.

When the test script is run, its behavior can be modified with
command-line flags.  These are documented in the --help output.

"""

import atexit
import fcntl
import optparse
import os
import shlex
import shutil
import signal
import socket
import string
import subprocess
import sys

# Used when most things go wrong (other than programming errors) so
# that the user sees an error message rather than a Python traceback,
# without help from the test script.  The on-exit handler will display
# additional explanatory text.
def fail(msg):
    """Print a message and exit with failure."""
    global _current_pass
    print("*** Failure:", msg)
    if _last_mark:
        print("*** Last mark: %s" % _last_mark)
    if _last_cmd:
        print("*** Last command (#%d): %s" % (_cmd_index - 1, _last_cmd))
    if _failed_daemon_output:
        print('*** Output of failed daemon:')
        sys.stdout.write(_failed_daemon_output)
    elif _last_cmd_output:
        print("*** Output of last command:")
        sys.stdout.write(_last_cmd_output)
    if _current_pass:
        print("*** Failed in test pass:", _current_pass)
    if _current_db:
        print("*** Failed with db:", _current_db)
    sys.exit(1)


def success(msg):
    global _success
    _stop_daemons()
    output('*** Success: %s\n' % msg)
    _success = True


def mark(msg):
    global _last_mark
    output('\n====== %s ======\n' % msg)
    _last_mark = msg


def skipped(whatmsg, whymsg):
    output('*** Skipping: %s: %s\n' % (whatmsg, whymsg), force_verbose=True)
    f = open(os.path.join(buildtop, 'skiptests'), 'a')
    f.write('Skipped %s: %s\n' % (whatmsg, whymsg))
    f.close()


def skip_rest(whatmsg, whymsg):
    global _success
    skipped(whatmsg, whymsg)
    _stop_daemons()
    _success = True
    sys.exit(0)


def output(msg, force_verbose=False):
    """Output a message to testlog, and to stdout if running verbosely."""
    _outfile.write(msg)
    if verbose or force_verbose:
        sys.stdout.write(msg)


# Return the location of progname in the executable path, or None if
# it is not found.
def which(progname):
    for dir in os.environ["PATH"].split(os.pathsep):
        path = os.path.join(dir, progname)
        if os.access(path, os.X_OK):
            return path
    return None


def password(name):
    """Choose a weakly random password from name, consistent across calls."""
    return name + str(os.getpid())


def canonicalize_hostname(name, rdns=True):
    """Canonicalize name using DNS, optionally with reverse DNS."""
    try:
        ai = socket.getaddrinfo(name, None, 0, 0, 0, socket.AI_CANONNAME)
    except socket.gaierror as e:
        return name.lower()
    (family, socktype, proto, canonname, sockaddr) = ai[0]

    if not rdns:
        return canonname.lower()

    try:
        rname = socket.getnameinfo(sockaddr, socket.NI_NAMEREQD)
    except socket.gaierror:
        return canonname.lower()
    return rname[0].lower()


# Exit handler which ensures processes are cleaned up and, on failure,
# prints messages to help developers debug the problem.
def _onexit():
    global _daemons, _success, srctop, verbose
    global _debug, _stop_before, _stop_after, _shell_before, _shell_after
    if _debug or _stop_before or _stop_after or _shell_before or _shell_after:
        # Wait before killing daemons in case one is being debugged.
        sys.stdout.write('*** Press return to kill daemons and exit script: ')
        sys.stdout.flush()
        sys.stdin.readline()
    for proc in _daemons:
        os.kill(proc.pid, signal.SIGTERM)
        _check_daemon(proc)
    if not _success:
        print
        if not verbose:
            testlogfile = os.path.join(os.getcwd(), 'testlog')
            utildir = os.path.join(srctop, 'util')
            print('For details, see: %s' % testlogfile)
            print('Or re-run this test script with the -v flag:')
            print('    cd %s' % os.getcwd())
            print('    PYTHONPATH=%s %s %s -v' %
                  (utildir, sys.executable, sys.argv[0]))
            print()
        print('Use --debug=NUM to run a command under a debugger.  Use')
        print('--stop-after=NUM to stop after a daemon is started in order to')
        print('attach to it with a debugger.  Use --help to see other')
        print('options.')


def _onsigint(signum, frame):
    # Exit without displaying a stack trace.  Suppress messages from _onexit.
    global _success
    _success = True
    sys.exit(1)


# Find the parent of dir which is at the root of a build or source directory.
def _find_root(dir):
    while True:
        if os.path.exists(os.path.join(dir, 'lib', 'krb5', 'krb')):
            break
        parent = os.path.dirname(dir)
        if (parent == dir):
            return None
        dir = parent
    return dir


def _find_buildtop():
    root = _find_root(os.getcwd())
    if root is None:
        fail('Cannot find root of krb5 build directory.')
    if not os.path.exists(os.path.join(root, 'config.status')):
        # Looks like an unbuilt source directory.
        fail('This script must be run inside a krb5 build directory.')
    return root


def _find_srctop():
    scriptdir = os.path.abspath(os.path.dirname(sys.argv[0]))
    if not scriptdir:
        scriptdir = os.getcwd()
    root = _find_root(scriptdir)
    if root is None:
        fail('Cannot find root of krb5 source directory.')
    return os.path.abspath(root)


# Parse command line arguments, setting global option variables.  Also
# sets the global variable args to the positional arguments, which may
# be used by the test script.
def _parse_args():
    global args, verbose, testpass, _debug, _debugger_command
    global _stop_before, _stop_after, _shell_before, _shell_after
    parser = optparse.OptionParser()
    parser.add_option('-v', '--verbose', action='store_true', dest='verbose',
                      default=False, help='Display verbose output')
    parser.add_option('-p', '--pass', dest='testpass', metavar='PASS',
                      help='If a multi-pass test, run only PASS')
    parser.add_option('--debug', dest='debug', metavar='NUM',
                      help='Debug numbered command (or "all")')
    parser.add_option('--debugger', dest='debugger', metavar='COMMAND',
                      help='Debugger command (default is gdb --args)')
    parser.add_option('--stop-before', dest='stopb', metavar='NUM',
                      help='Stop before numbered command (or "all")')
    parser.add_option('--stop-after', dest='stopa', metavar='NUM',
                      help='Stop after numbered command (or "all")')
    parser.add_option('--shell-before', dest='shellb', metavar='NUM',
                      help='Spawn shell before numbered command (or "all")')
    parser.add_option('--shell-after', dest='shella', metavar='NUM',
                      help='Spawn shell after numbered command (or "all")')
    (options, args) = parser.parse_args()
    verbose = options.verbose
    testpass = options.testpass
    _debug = _parse_cmdnum('--debug', options.debug)
    _stop_before = _parse_cmdnum('--stop-before', options.stopb)
    _stop_after = _parse_cmdnum('--stop-after', options.stopa)
    _shell_before = _parse_cmdnum('--shell-before', options.shellb)
    _shell_after = _parse_cmdnum('--shell-after', options.shella)

    if options.debugger is not None:
        _debugger_command = shlex.split(options.debugger)
    elif which('gdb') is not None:
        _debugger_command = ['gdb', '--args']
    elif which('lldb') is not None:
        _debugger_command = ['lldb', '--']
    elif options.debug is not None:
        print('Cannot find a debugger; use --debugger=COMMAND')
        sys.exit(1)


# Translate a command number spec.  -1 means all, None means none.
def _parse_cmdnum(optname, str):
    if not str:
        return None
    if str == 'all':
        return -1
    try:
        return int(str)
    except ValueError:
        fail('%s value must be "all" or a number' % optname)


# Test if a command index matches a translated command number spec.
def _match_cmdnum(cmdnum, ind):
    if cmdnum is None:
        return False
    elif cmdnum == -1:
        return True
    else:
        return cmdnum == ind


# Return an environment suitable for running programs in the build
# tree.  It is safe to modify the result.
def _build_env():
    global buildtop, runenv
    env = os.environ.copy()
    for (k, v) in runenv.env.items():
        if v.find('./') == 0:
            env[k] = os.path.join(buildtop, v)
        else:
            env[k] = v
    # Make sure we don't get confused by translated messages
    # or localized times.
    env['LC_ALL'] = 'C'
    return env


# Merge the nested dictionaries cfg1 and cfg2 into a new dictionary.
# cfg1 or cfg2 may be None, in which case the other is returned.  If
# cfg2 contains keys mapped to None, the corresponding keys will be
# mapped to None in the result.  The result may contain references to
# parts of cfg1 or cfg2, so is not safe to modify.
def _cfg_merge(cfg1, cfg2):
    if not cfg2:
        return cfg1
    if not cfg1:
        return cfg2
    result = cfg1.copy()
    for key, value2 in cfg2.items():
        if value2 is None:
            result.pop(key, None)
        elif key not in result:
            result[key] = value2
        else:
            value1 = result[key]
            if isinstance(value1, dict):
                if not isinstance(value2, dict):
                    raise TypeError()
                result[key] = _cfg_merge(value1, value2)
            else:
                result[key] = value2
    return result


# Python gives us shlex.split() to turn a shell command into a list of
# arguments, but oddly enough, not the easier reverse operation.  For
# now, do a bad job of faking it.
def _shell_equiv(args):
    return " ".join(args)


# Add a valgrind prefix to the front of args if specified in the
# environment.  Under normal circumstances this just returns args.
def _valgrind(args):
    valgrind = os.getenv('VALGRIND')
    if valgrind:
        args = shlex.split(valgrind) + args
    return args


def _stop_or_shell(stop, shell, env, ind):
    if (_match_cmdnum(stop, ind)):
        sys.stdout.write('*** [%d] Waiting for return: ' % ind)
        sys.stdout.flush()
        sys.stdin.readline()
    if (_match_cmdnum(shell, ind)):
        output('*** [%d] Spawning shell\n' % ind, True)
        subprocess.call(os.getenv('SHELL'), env=env)


# Look for the expected strings in successive lines of trace.
def _check_trace(trace, expected):
    i = 0
    for line in trace.splitlines():
        if i < len(expected) and expected[i] in line:
            i += 1
    if i < len(expected):
        fail('Expected string not found in trace output: ' + expected[i])


def _run_cmd(args, env, input=None, expected_code=0, expected_msg=None,
             expected_trace=None, return_trace=False):
    global null_input, _cmd_index, _last_cmd, _last_cmd_output, _debug
    global _stop_before, _stop_after, _shell_before, _shell_after

    tracefile = None
    if expected_trace is not None or return_trace:
        tracefile = 'testtrace'
        if os.path.exists(tracefile):
            os.remove(tracefile)
        env = env.copy()
        env['KRB5_TRACE'] = tracefile

    if (_match_cmdnum(_debug, _cmd_index)):
        return _debug_cmd(args, env, input)

    args = _valgrind(args)
    _last_cmd = _shell_equiv(args)

    output('*** [%d] Executing: %s\n' % (_cmd_index, _last_cmd))
    _stop_or_shell(_stop_before, _shell_before, env, _cmd_index)

    if input:
        infile = subprocess.PIPE
    else:
        infile = null_input

    # Run the command and log the result, folding stderr into stdout.
    proc = subprocess.Popen(args, stdin=infile, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, env=env,
                            universal_newlines=True)
    (outdata, dummy_errdata) = proc.communicate(input)
    _last_cmd_output = outdata
    code = proc.returncode
    output(outdata)
    output('*** [%d] Completed with return code %d\n' % (_cmd_index, code))
    _stop_or_shell(_stop_after, _shell_after, env, _cmd_index)
    _cmd_index += 1

    # Check the return code and return the output.
    if code != expected_code:
        fail('%s failed with code %d.' % (args[0], code))

    if expected_msg is not None and expected_msg not in outdata:
        fail('Expected string not found in command output: ' + expected_msg)

    if tracefile is not None:
        with open(tracefile, 'r') as f:
            trace = f.read()
        output('*** Trace output for previous command:\n')
        output(trace)
        if expected_trace is not None:
            _check_trace(trace, expected_trace)

    return (outdata, trace) if return_trace else outdata


def _debug_cmd(args, env, input):
    global _cmd_index, _debugger_command

    args = _debugger_command + list(args)
    output('*** [%d] Executing in debugger: %s\n' %
           (_cmd_index, _shell_equiv(args)), True)
    if input:
        print
        print('*** Enter the following input when appropriate:')
        print()
        print(input)
        print()
    code = subprocess.call(args, env=env)
    output('*** [%d] Completed in debugger with return code %d\n' %
           (_cmd_index, code))
    _cmd_index += 1


# Start a daemon process with the specified args and env.  Wait until
# we see sentinel as a substring of a line on either stdout or stderr.
# Clean up the daemon process on exit.
def _start_daemon(args, env, sentinel):
    global null_input, _cmd_index, _last_cmd, _last_cmd_output, _debug
    global _stop_before, _stop_after, _shell_before, _shell_after

    if (_match_cmdnum(_debug, _cmd_index)):
        output('*** [%d] Warning: ' % _cmd_index, True)
        output( 'test script cannot proceed after debugging a daemon\n', True)
        _debug_cmd(args, env, None)
        output('*** Exiting after debugging daemon\n', True)
        sys.exit(1)

    args = _valgrind(args)
    _last_cmd = _shell_equiv(args)
    output('*** [%d] Starting: %s\n' % (_cmd_index, _last_cmd))
    _stop_or_shell(_stop_before, _shell_before, env, _cmd_index)

    # Start the daemon and look for the sentinel in stdout or stderr.
    proc = subprocess.Popen(args, stdin=null_input, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, env=env,
                            universal_newlines=True)
    _last_cmd_output = ''
    while True:
        line = proc.stdout.readline()
        _last_cmd_output += line
        if line == "":
            code = proc.wait()
            fail('%s failed to start with code %d.' % (args[0], code))
        output(line)
        if sentinel in line:
            break
    output('*** [%d] Started with pid %d\n' % (_cmd_index, proc.pid))
    _stop_or_shell(_stop_after, _shell_after, env, _cmd_index)
    _cmd_index += 1

    # Save the daemon in a list for cleanup.  Note that we won't read
    # any more of the daemon's output after the sentinel, which will
    # cause the daemon to block if it generates enough.  For now we
    # assume all daemon processes are quiet enough to avoid this
    # problem.  If it causes an issue, some alternatives are:
    #   - Output to a file and poll the file for the sentinel
    #     (undesirable because it slows down the test suite by the
    #     polling interval times the number of daemons started)
    #   - Create an intermediate subprocess which discards output
    #     after the sentinel.
    _daemons.append(proc)

    # Return the process; the caller can stop it with stop_daemon.
    return proc


# Await a daemon process's exit status and display it if it isn't
# successful.  Display any output it generated after the sentinel.
# Return the daemon's exit status (0 if it terminated with SIGTERM).
def _check_daemon(proc):
    global _failed_daemon_output
    code = proc.wait()
    # If a daemon doesn't catch SIGTERM (like gss-server), treat it as
    # a normal exit.
    if code == -signal.SIGTERM:
        code = 0
    if code != 0:
        output('*** Daemon pid %d exited with code %d\n' % (proc.pid, code))

    out, err = proc.communicate()
    if code != 0:
        _failed_daemon_output = out
    output('*** Daemon pid %d output:\n' % proc.pid)
    output(out)

    return code


# Terminate all active daemon processes.  Fail out if any of them
# exited unsuccessfully.
def _stop_daemons():
    global _daemons
    daemon_error = False
    for proc in _daemons:
        os.kill(proc.pid, signal.SIGTERM)
        code = _check_daemon(proc)
        if code != 0:
            daemon_error = True
    _daemons = []
    if daemon_error:
        fail('One or more daemon processes exited with an error')


# Wait for a daemon process to exit.  Fail out if it exits
# unsuccessfully.
def await_daemon_exit(proc):
    code = _check_daemon(proc)
    _daemons.remove(proc)
    if code != 0:
        fail('Daemon exited unsuccessfully')


# Terminate one daemon process.  Fail out if it exits unsuccessfully.
def stop_daemon(proc):
    os.kill(proc.pid, signal.SIGTERM)
    return await_daemon_exit(proc)


class K5Realm(object):
    """An object representing a functional krb5 test realm."""

    def __init__(self, realm='KRBTEST.COM', portbase=61000, testdir='testdir',
                 krb5_conf=None, kdc_conf=None, create_kdb=True,
                 krbtgt_keysalt=None, create_user=True, get_creds=True,
                 create_host=True, start_kdc=True, start_kadmind=False,
                 start_kpropd=False, bdb_only=False, pkinit=False):
        global hostname, _default_krb5_conf, _default_kdc_conf
        global _lmdb_kdc_conf, _current_db

        self.realm = realm
        self.testdir = os.path.join(os.getcwd(), testdir)
        self.portbase = portbase
        self.user_princ = 'user@' + self.realm
        self.admin_princ = 'user/admin@' + self.realm
        self.host_princ = 'host/%s@%s' % (hostname, self.realm)
        self.nfs_princ = 'nfs/%s@%s' % (hostname, self.realm)
        self.krbtgt_princ = 'krbtgt/%s@%s' % (self.realm, self.realm)
        self.keytab = os.path.join(self.testdir, 'keytab')
        self.client_keytab = os.path.join(self.testdir, 'client_keytab')
        self.ccache = os.path.join(self.testdir, 'ccache')
        self.gss_mech_config = os.path.join(self.testdir, 'mech.conf')
        self.kadmin_ccache = os.path.join(self.testdir, 'kadmin_ccache')
        base_krb5_conf = _default_krb5_conf
        base_kdc_conf = _default_kdc_conf
        if (os.getenv('K5TEST_LMDB') is not None and
            not bdb_only and not _current_db):
            base_kdc_conf = _cfg_merge(base_kdc_conf, _lmdb_kdc_conf)
        if pkinit:
            base_krb5_conf = _cfg_merge(base_krb5_conf, _pkinit_krb5_conf)
            base_kdc_conf = _cfg_merge(base_kdc_conf, _pkinit_kdc_conf)
        self._krb5_conf = _cfg_merge(base_krb5_conf, krb5_conf)
        self._kdc_conf = _cfg_merge(base_kdc_conf, kdc_conf)
        self._kdc_proc = None
        self._kadmind_proc = None
        self._kpropd_procs = []
        krb5_conf_path = os.path.join(self.testdir, 'krb5.conf')
        kdc_conf_path = os.path.join(self.testdir, 'kdc.conf')
        self.env = self._make_env(krb5_conf_path, kdc_conf_path)

        self._create_empty_dir()
        self._create_conf(self._krb5_conf, krb5_conf_path)
        self._create_conf(self._kdc_conf, kdc_conf_path)
        self._create_acl()
        self._create_dictfile()

        if create_kdb:
            self.create_kdb()
        if krbtgt_keysalt and create_kdb:
            self.run([kadminl, 'cpw', '-randkey', '-e', krbtgt_keysalt,
                      self.krbtgt_princ])
        if create_user and create_kdb:
            self.addprinc(self.user_princ, password('user'))
            self.addprinc(self.admin_princ, password('admin'))
        if create_host and create_kdb:
            self.addprinc(self.host_princ)
            self.extract_keytab(self.host_princ, self.keytab)
        if start_kdc and create_kdb:
            self.start_kdc()
        if start_kadmind and create_kdb:
            self.start_kadmind()
        if get_creds and create_kdb and create_user and start_kdc:
            self.kinit(self.user_princ, password('user'))
            self.klist(self.user_princ)

    def _create_empty_dir(self):
        dir = self.testdir
        shutil.rmtree(dir, True)
        if (os.path.exists(dir)):
            fail('Cannot remove %s to create test realm.' % dir)
        os.mkdir(dir)

    def _create_conf(self, profile, filename):
        file = open(filename, 'w')
        for section, contents in profile.items():
            file.write('[%s]\n' % section)
            self._write_cfg_section(file, contents, 1)
        file.close()

    def _write_cfg_section(self, file, contents, indent_level):
        indent = '\t' * indent_level
        for name, value in contents.items():
            name = self._subst_cfg_value(name)
            if isinstance(value, dict):
                # A dictionary value yields a list subsection.
                file.write('%s%s = {\n' % (indent, name))
                self._write_cfg_section(file, value, indent_level + 1)
                file.write('%s}\n' % indent)
            elif isinstance(value, list):
                # A list value yields multiple values for the same name.
                for item in value:
                    item = self._subst_cfg_value(item)
                    file.write('%s%s = %s\n' % (indent, name, item))
            elif isinstance(value, str):
                # A string value yields a straightforward variable setting.
                value = self._subst_cfg_value(value)
                file.write('%s%s = %s\n' % (indent, name, value))
            else:
                raise TypeError()

    def _subst_cfg_value(self, value):
        global buildtop, srctop, hostname
        template = string.Template(value)
        subst = template.substitute(realm=self.realm,
                                    testdir=self.testdir,
                                    buildtop=buildtop,
                                    srctop=srctop,
                                    plugins=plugins,
                                    certs=pkinit_certs,
                                    hostname=hostname,
                                    port0=self.portbase,
                                    port1=self.portbase + 1,
                                    port2=self.portbase + 2,
                                    port3=self.portbase + 3,
                                    port4=self.portbase + 4,
                                    port5=self.portbase + 5,
                                    port6=self.portbase + 6,
                                    port7=self.portbase + 7,
                                    port8=self.portbase + 8,
                                    port9=self.portbase + 9)
        # Empty values must be quoted to avoid a syntax error.
        return subst if subst else '""'

    def _create_acl(self):
        global hostname
        filename = os.path.join(self.testdir, 'acl')
        file = open(filename, 'w')
        file.write('%s *e\n' % self.admin_princ)
        file.write('kiprop/%s@%s p\n' % (hostname, self.realm))
        file.close()

    def _create_dictfile(self):
        filename = os.path.join(self.testdir, 'dictfile')
        file = open(filename, 'w')
        file.write('weak_password\n')
        file.close()

    def _make_env(self, krb5_conf_path, kdc_conf_path):
        env = _build_env()
        env['KRB5_CONFIG'] = krb5_conf_path
        env['KRB5_KDC_PROFILE'] = kdc_conf_path or os.devnull
        env['KRB5CCNAME'] = self.ccache
        env['KRB5_KTNAME'] = self.keytab
        env['KRB5_CLIENT_KTNAME'] = self.client_keytab
        env['KRB5RCACHEDIR'] = self.testdir
        env['KPROPD_PORT'] = str(self.kprop_port())
        env['KPROP_PORT'] = str(self.kprop_port())
        env['GSS_MECH_CONFIG'] = self.gss_mech_config
        return env

    def run(self, args, env=None, **keywords):
        if env is None:
            env = self.env
        return _run_cmd(args, env, **keywords)

    def kprop_port(self):
        return self.portbase + 3

    def server_port(self):
        return self.portbase + 5

    def start_server(self, args, sentinel, env=None):
        if env is None:
            env = self.env
        return _start_daemon(args, env, sentinel)

    def start_in_inetd(self, args, port=None, env=None):
        if not port:
            port = self.server_port()
        if env is None:
            env = self.env
        inetd_args = [t_inetd, str(port), args[0]] + args
        return _start_daemon(inetd_args, env, 'Ready!')

    def create_kdb(self):
        global kdb5_util
        self.run([kdb5_util, 'create', '-s', '-P', 'master'])

    def start_kdc(self, args=[], env=None):
        global krb5kdc
        if env is None:
            env = self.env
        assert(self._kdc_proc is None)
        self._kdc_proc = _start_daemon([krb5kdc, '-n'] + args, env,
                                       'starting...')

    def stop_kdc(self):
        assert(self._kdc_proc is not None)
        stop_daemon(self._kdc_proc)
        self._kdc_proc = None

    def start_kadmind(self, env=None):
        global krb5kdc
        if env is None:
            env = self.env
        assert(self._kadmind_proc is None)
        dump_path = os.path.join(self.testdir, 'dump')
        self._kadmind_proc = _start_daemon([kadmind, '-nofork',
                                            '-p', kdb5_util, '-K', kprop,
                                            '-F', dump_path], env,
                                           'starting...')

    def stop_kadmind(self):
        assert(self._kadmind_proc is not None)
        stop_daemon(self._kadmind_proc)
        self._kadmind_proc = None

    def _kpropd_args(self):
        datatrans_path = os.path.join(self.testdir, 'incoming-datatrans')
        kpropdacl_path = os.path.join(self.testdir, 'kpropd-acl')
        return [kpropd, '-D', '-P', str(self.kprop_port()),
                '-f', datatrans_path, '-p', kdb5_util, '-a', kpropdacl_path]

    def start_kpropd(self, env, args=[]):
        proc = _start_daemon(self._kpropd_args() + args, env, 'ready')
        self._kpropd_procs.append(proc)
        return proc

    def stop_kpropd(self, proc):
        stop_daemon(proc)
        self._kpropd_procs.remove(proc)

    def run_kpropd_once(self, env, args=[]):
        return self.run(self._kpropd_args() + ['-t'] + args, env=env)

    def stop(self):
        if self._kdc_proc:
            self.stop_kdc()
        if self._kadmind_proc:
            self.stop_kadmind()
        for p in self._kpropd_procs:
            stop_daemon(p)
        self._kpropd_procs = []

    def addprinc(self, princname, password=None):
        if password:
            self.run([kadminl, 'addprinc', '-pw', password, princname])
        else:
            self.run([kadminl, 'addprinc', '-randkey', princname])

    def extract_keytab(self, princname, keytab):
        self.run([kadminl, 'ktadd', '-k', keytab, '-norandkey', princname])

    def kinit(self, princname, password=None, flags=[], **keywords):
        if password:
            input = password + "\n"
        else:
            input = None
        return self.run([kinit] + flags + [princname], input=input, **keywords)

    def pkinit(self, princ, flags=[], **kw):
        id = 'FILE:%s,%s' % (os.path.join(pkinit_certs, 'user.pem'),
                             os.path.join(pkinit_certs, 'privkey.pem'))
        flags = flags + ['-X', 'X509_user_identity=%s' % id]
        self.kinit(princ, flags=flags, **kw)

    def klist(self, client_princ, service_princ=None, ccache=None, **keywords):
        if service_princ is None:
            service_princ = self.krbtgt_princ
        if ccache is None:
            ccache = self.ccache
        ccachestr = ccache
        if len(ccachestr) < 2 or ':' not in ccachestr[2:]:
            ccachestr = 'FILE:' + ccachestr
        output = self.run([klist, ccache], **keywords)
        if (('Ticket cache: %s\n' % ccachestr) not in output or
            ('Default principal: %s\n' % client_princ) not in output or
            service_princ not in output):
            fail('Unexpected klist output.')

    def klist_keytab(self, princ, keytab=None, **keywords):
        if keytab is None:
            keytab = self.keytab
        output = self.run([klist, '-k', keytab], **keywords)
        if (('Keytab name: FILE:%s\n' % keytab) not in output or
            'KVNO Principal\n----' not in output or
            princ not in output):
            fail('Unexpected klist output.')

    def prep_kadmin(self, princname=None, pw=None, flags=[]):
        if princname is None:
            princname = self.admin_princ
            pw = password('admin')
        return self.kinit(princname, pw,
                          flags=['-S', 'kadmin/admin',
                                 '-c', self.kadmin_ccache] + flags)

    def run_kadmin(self, args, **keywords):
        return self.run([kadmin, '-c', self.kadmin_ccache] + args, **keywords)

    def special_env(self, name, has_kdc_conf, krb5_conf=None, kdc_conf=None):
        krb5_conf_path = os.path.join(self.testdir, 'krb5.conf.%s' % name)
        krb5_conf = _cfg_merge(self._krb5_conf, krb5_conf)
        self._create_conf(krb5_conf, krb5_conf_path)
        if has_kdc_conf:
            kdc_conf_path = os.path.join(self.testdir, 'kdc.conf.%s' % name)
            kdc_conf = _cfg_merge(self._kdc_conf, kdc_conf)
            self._create_conf(kdc_conf, kdc_conf_path)
        else:
            kdc_conf_path = None
        return self._make_env(krb5_conf_path, kdc_conf_path)


def multipass_realms(**keywords):
    global _current_pass, _passes, testpass
    caller_krb5_conf = keywords.get('krb5_conf')
    caller_kdc_conf = keywords.get('kdc_conf')
    for p in _passes:
        (name, krbtgt_keysalt, krb5_conf, kdc_conf) = p
        if testpass and name != testpass:
            continue
        output('*** Beginning pass %s\n' % name)
        keywords['krb5_conf'] = _cfg_merge(krb5_conf, caller_krb5_conf)
        keywords['kdc_conf'] = _cfg_merge(kdc_conf, caller_kdc_conf)
        keywords['krbtgt_keysalt'] = krbtgt_keysalt
        _current_pass = name
        realm = K5Realm(**keywords)
        yield realm
        realm.stop()
        _current_pass = None


def multidb_realms(**keywords):
    global _current_db, _dbpasses
    caller_kdc_conf = keywords.get('kdc_conf')
    for p in _dbpasses:
        (name, kdc_conf) = p
        output('*** Using DB type %s\n' % name)
        keywords['kdc_conf'] = _cfg_merge(kdc_conf, caller_kdc_conf)
        _current_db = name
        realm = K5Realm(**keywords)
        yield realm
        realm.stop()
        _current_db = None


def cross_realms(num, xtgts=None, args=None, **keywords):
    # Build keyword args for each realm.
    realm_args = []
    for i in range(num):
        realmnumber = i + 1
        # Start with any global keyword arguments to this function.
        a = keywords.copy()
        if args and args[i]:
            # Merge in specific arguments for this realm.  Use
            # _cfg_merge for config fragments.
            a.update(args[i])
            for cf in ('krb5_conf', 'kdc_conf'):
                if cf in keywords and cf in args[i]:
                    a[cf] = _cfg_merge(keywords[cf], args[i][cf])
        # Set defaults for the realm name, testdir, and portbase.
        if not 'realm' in a:
            a['realm'] = 'KRBTEST%d.COM' % realmnumber
        if not 'testdir' in a:
            a['testdir'] = os.path.join('testdir', str(realmnumber))
        if not 'portbase' in a:
            a['portbase'] = 61000 + 10 * realmnumber
        realm_args.append(a)
        
    # Build a [realms] config fragment containing all of the realms.
    realmsection = { '$realm' : None }
    for a in realm_args:
        name = a['realm']
        portbase = a['portbase']
        realmsection[name] = {
            'kdc' : '$hostname:%d' % portbase,
            'admin_server' : '$hostname:%d' % (portbase + 1),
            'kpasswd_server' : '$hostname:%d' % (portbase + 2)
            }
    realmscfg = {'realms': realmsection}

    # Set realmsection in each realm's krb5_conf keyword argument.
    for a in realm_args:
        a['krb5_conf'] = _cfg_merge(realmscfg, a.get('krb5_conf'))

    if xtgts is None:
        # Default to cross tgts for every pair of realms.
        # (itertools.permutations would work here but is new in 2.6.)
        xtgts = [(x,y) for x in range(num) for y in range(num) if x != y]

    # Create the realms.
    realms = []
    for i in range(num):
        r = K5Realm(**realm_args[i])
        # Create specified cross TGTs in this realm's db.
        for j in range(num):
            if j == i:
                continue
            iname = r.realm
            jname = realm_args[j]['realm']
            if (i, j) in xtgts:
                # This realm can authenticate to realm j.
                r.addprinc('krbtgt/%s' % jname, password('cr-%d-%d-' % (i, j)))
            if (j, i) in xtgts:
                # Realm j can authenticate to this realm.
                r.addprinc('krbtgt/%s@%s' % (iname, jname),
                           password('cr-%d-%d-' % (j, i)))
        realms.append(r)
    return realms


_default_krb5_conf = {
    'libdefaults': {
        'default_realm': '$realm',
        'dns_lookup_kdc': 'false',
        'dns_canonicalize_hostname': 'fallback',
        'qualify_shortname': '',
        'plugin_base_dir': '$plugins'},
    'realms': {'$realm': {
            'kdc': '$hostname:$port0',
            'admin_server': '$hostname:$port1',
            'kpasswd_server': '$hostname:$port2'}}}


_default_kdc_conf = {
    'realms': {'$realm': {
            'database_module': 'db',
            'iprop_port': '$port4',
            'key_stash_file': '$testdir/stash',
            'acl_file': '$testdir/acl',
            'dict_file': '$testdir/dictfile',
            'kadmind_port': '$port1',
            'kpasswd_port': '$port2',
            'kdc_listen': '$port0',
            'kdc_tcp_listen': '$port0'}},
    'dbmodules': {
        'db_module_dir': '$plugins/kdb',
        'db': {'db_library': 'db2', 'database_name' : '$testdir/db'}},
    'logging': {
        'admin_server': 'FILE:$testdir/kadmind5.log',
        'kdc': 'FILE:$testdir/kdc.log',
        'default': 'FILE:$testdir/others.log'}}


_lmdb_kdc_conf = {'dbmodules': {'db': {'db_library': 'klmdb',
                                       'nosync': 'true'}}}


_pkinit_krb5_conf = {'realms': {'$realm': {
    'pkinit_anchors': 'FILE:$certs/ca.pem'}}}
_pkinit_kdc_conf = {'realms': {'$realm': {
    'pkinit_identity': 'FILE:$certs/kdc.pem,$certs/privkey.pem'}}}


# A pass is a tuple of: name, krbtgt_keysalt, krb5_conf, kdc_conf.
_passes = [
    # No special settings; exercises AES256.
    ('default', None, None, None),

    # Exercise the DES3 enctype.
    ('des3', None,
     {'libdefaults': {'permitted_enctypes': 'des3 aes256-sha1'}},
     {'realms': {'$realm': {
                    'supported_enctypes': 'des3-cbc-sha1:normal',
                    'master_key_type': 'des3-cbc-sha1'}}}),

    # Exercise the arcfour enctype.
    ('arcfour', None,
     {'libdefaults': {'permitted_enctypes': 'rc4 aes256-sha1'}},
     {'realms': {'$realm': {
                    'supported_enctypes': 'arcfour-hmac:normal',
                    'master_key_type': 'arcfour-hmac'}}}),

    # Exercise the AES128 enctype.
    ('aes128', None,
      {'libdefaults': {'permitted_enctypes': 'aes128-cts'}},
      {'realms': {'$realm': {
                    'supported_enctypes': 'aes128-cts:normal',
                    'master_key_type': 'aes128-cts'}}}),

    # Exercise the camellia256-cts enctype.
    ('camellia256', None,
      {'libdefaults': {'permitted_enctypes': 'camellia256-cts'}},
      {'realms': {'$realm': {
                    'supported_enctypes': 'camellia256-cts:normal',
                    'master_key_type': 'camellia256-cts'}}}),

    # Exercise the aes128-sha2 enctype.
    ('aes128-sha2', None,
      {'libdefaults': {'permitted_enctypes': 'aes128-sha2'}},
      {'realms': {'$realm': {
                    'supported_enctypes': 'aes128-sha2:normal',
                    'master_key_type': 'aes128-sha2'}}}),

    # Exercise the aes256-sha2 enctype.
    ('aes256-sha2', None,
      {'libdefaults': {'permitted_enctypes': 'aes256-sha2'}},
      {'realms': {'$realm': {
                    'supported_enctypes': 'aes256-sha2:normal',
                    'master_key_type': 'aes256-sha2'}}}),

    # Test a setup with modern principal keys but an old TGT key.
    ('aes256.destgt', 'arcfour-hmac:normal',
     {'libdefaults': {'allow_weak_crypto': 'true'}},
     None)
]

_success = False
_current_pass = None
_current_db = None
_daemons = []
_parse_args()
atexit.register(_onexit)
signal.signal(signal.SIGINT, _onsigint)
_outfile = open('testlog', 'w')
_cmd_index = 1
_last_mark = None
_last_cmd = None
_last_cmd_output = None
_failed_daemon_output = None
buildtop = _find_buildtop()
srctop = _find_srctop()
plugins = os.path.join(buildtop, 'plugins')
pkinit_enabled = os.path.exists(os.path.join(plugins, 'preauth', 'pkinit.so'))
pkinit_certs = os.path.join(srctop, 'tests', 'pkinit-certs')
hostname = socket.gethostname().lower()
null_input = open(os.devnull, 'r')

if not os.path.exists(os.path.join(buildtop, 'runenv.py')):
    fail('You must run "make runenv.py" in %s first.' % buildtop)
sys.path = [buildtop] + sys.path
import runenv

# A DB pass is a tuple of: name, kdc_conf.
_dbpasses = [('db2', None)]
if runenv.have_lmdb == 'yes':
    _dbpasses.append(('lmdb', _lmdb_kdc_conf))

krb5kdc = os.path.join(buildtop, 'kdc', 'krb5kdc')
kadmind = os.path.join(buildtop, 'kadmin', 'server', 'kadmind')
kadmin = os.path.join(buildtop, 'kadmin', 'cli', 'kadmin')
kadminl = os.path.join(buildtop, 'kadmin', 'cli', 'kadmin.local')
kdb5_ldap_util = os.path.join(buildtop, 'plugins', 'kdb', 'ldap', 'ldap_util',
                              'kdb5_ldap_util')
kdb5_util = os.path.join(buildtop, 'kadmin', 'dbutil', 'kdb5_util')
ktutil = os.path.join(buildtop, 'kadmin', 'ktutil', 'ktutil')
kinit = os.path.join(buildtop, 'clients', 'kinit', 'kinit')
klist = os.path.join(buildtop, 'clients', 'klist', 'klist')
kswitch = os.path.join(buildtop, 'clients', 'kswitch', 'kswitch')
kvno = os.path.join(buildtop, 'clients', 'kvno', 'kvno')
kdestroy = os.path.join(buildtop, 'clients', 'kdestroy', 'kdestroy')
kpasswd = os.path.join(buildtop, 'clients', 'kpasswd', 'kpasswd')
t_inetd = os.path.join(buildtop, 'tests', 't_inetd')
kproplog = os.path.join(buildtop, 'kprop', 'kproplog')
kpropd = os.path.join(buildtop, 'kprop', 'kpropd')
kprop = os.path.join(buildtop, 'kprop', 'kprop')
