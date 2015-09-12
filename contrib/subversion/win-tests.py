#
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#
#
"""
Driver for running the tests on Windows.

For a list of options, run this script with the --help option.
"""

# $HeadURL: http://svn.apache.org/repos/asf/subversion/branches/1.8.x/win-tests.py $
# $LastChangedRevision: 1692801 $

import os, sys, subprocess
import filecmp
import shutil
import traceback
try:
  # Python >=3.0
  import configparser
except ImportError:
  # Python <3.0
  import ConfigParser as configparser
import string
import random

import getopt
try:
    my_getopt = getopt.gnu_getopt
except AttributeError:
    my_getopt = getopt.getopt

def _usage_exit():
  "print usage, exit the script"

  print("Driver for running the tests on Windows.")
  print("Usage: python win-tests.py [option] [test-path]")
  print("")
  print("Valid options:")
  print("  -r, --release          : test the Release configuration")
  print("  -d, --debug            : test the Debug configuration (default)")
  print("  --bin=PATH             : use the svn binaries installed in PATH")
  print("  -u URL, --url=URL      : run ra_dav or ra_svn tests against URL;")
  print("                           will start svnserve for ra_svn tests")
  print("  -v, --verbose          : talk more")
  print("  -q, --quiet            : talk less")
  print("  -f, --fs-type=type     : filesystem type to use (fsfs is default)")
  print("  -c, --cleanup          : cleanup after running a test")
  print("  -t, --test=TEST        : Run the TEST test (all is default); use")
  print("                           TEST#n to run a particular test number,")
  print("                           multiples also accepted e.g. '2,4-7'")
  print("  --log-level=LEVEL      : Set log level to LEVEL (E.g. DEBUG)")
  print("  --log-to-stdout        : Write log results to stdout")

  print("  --svnserve-args=list   : comma-separated list of arguments for")
  print("                           svnserve")
  print("                           default is '-d,-r,<test-path-root>'")
  print("  --asp.net-hack         : use '_svn' instead of '.svn' for the admin")
  print("                           dir name")
  print("  --httpd-dir            : location where Apache HTTPD is installed")
  print("  --httpd-port           : port for Apache HTTPD; random port number")
  print("                           will be used, if not specified")
  print("  --httpd-daemon         : Run Apache httpd as daemon")
  print("  --httpd-service        : Run Apache httpd as Windows service (default)")
  print("  --httpd-no-log         : Disable httpd logging")
  print("  --http-short-circuit   : Use SVNPathAuthz short_circuit on HTTP server")
  print("  --disable-http-v2      : Do not advertise support for HTTPv2 on server")
  print("  --disable-bulk-updates : Disable bulk updates on HTTP server")
  print("  --ssl-cert             : Path to SSL server certificate to trust.")
  print("  --javahl               : Run the javahl tests instead of the normal tests")
  print("  --list                 : print test doc strings only")
  print("  --milestone-filter=RE  : RE is a regular expression pattern that (when")
  print("                           used with --list) limits the tests listed to")
  print("                           those with an associated issue in the tracker")
  print("                           which has a target milestone that matches RE.")
  print("  --mode-filter=TYPE     : limit tests to expected TYPE = XFAIL, SKIP, PASS,")
  print("                           or 'ALL' (default)")
  print("  --enable-sasl          : enable Cyrus SASL authentication for")
  print("                           svnserve")
  print("  -p, --parallel         : run multiple tests in parallel")
  print("  --server-minor-version : the minor version of the server being")
  print("                           tested")
  print("  --config-file          : Configuration file for tests")
  print("  --fsfs-sharding        : Specify shard size (for fsfs)")
  print("  --fsfs-packing         : Run 'svnadmin pack' automatically")

  sys.exit(0)

CMDLINE_TEST_SCRIPT_PATH = 'subversion/tests/cmdline/'
CMDLINE_TEST_SCRIPT_NATIVE_PATH = CMDLINE_TEST_SCRIPT_PATH.replace('/', os.sep)

sys.path.insert(0, os.path.join('build', 'generator'))
sys.path.insert(1, 'build')

import gen_win
version_header = os.path.join('subversion', 'include', 'svn_version.h')
cp = configparser.ConfigParser()
cp.read('gen-make.opts')
gen_obj = gen_win.GeneratorBase('build.conf', version_header,
                                cp.items('options'))
all_tests = gen_obj.test_progs + gen_obj.bdb_test_progs \
          + gen_obj.scripts + gen_obj.bdb_scripts
client_tests = [x for x in all_tests if x.startswith(CMDLINE_TEST_SCRIPT_PATH)]

svn_dlls = []
for section in gen_obj.sections.values():
  if section.options.get("msvc-export"):
    dll_basename = section.name + "-" + str(gen_obj.version) + ".dll"
    svn_dlls.append(os.path.join("subversion", section.name, dll_basename))

opts, args = my_getopt(sys.argv[1:], 'hrdvqct:pu:f:',
                       ['release', 'debug', 'verbose', 'quiet', 'cleanup',
                        'test=', 'url=', 'svnserve-args=', 'fs-type=', 'asp.net-hack',
                        'httpd-dir=', 'httpd-port=', 'httpd-daemon',
                        'httpd-server', 'http-short-circuit', 'httpd-no-log',
                        'disable-http-v2', 'disable-bulk-updates', 'help',
                        'fsfs-packing', 'fsfs-sharding=', 'javahl',
                        'list', 'enable-sasl', 'bin=', 'parallel',
                        'config-file=', 'server-minor-version=', 'log-level=',
                        'log-to-stdout', 'mode-filter=', 'milestone-filter=',
                        'ssl-cert='])
if len(args) > 1:
  print('Warning: non-option arguments after the first one will be ignored')

# Interpret the options and set parameters
base_url, fs_type, verbose, quiet, cleanup = None, None, None, None, None
repo_loc = 'local repository.'
objdir = 'Debug'
log = 'tests.log'
faillog = 'fails.log'
run_svnserve = None
svnserve_args = None
run_httpd = None
httpd_port = None
httpd_service = None
httpd_no_log = None
http_short_circuit = False
advertise_httpv2 = True
http_bulk_updates = True
list_tests = None
milestone_filter = None
test_javahl = None
enable_sasl = None
svn_bin = None
parallel = None
fsfs_sharding = None
fsfs_packing = None
server_minor_version = None
config_file = None
log_to_stdout = None
mode_filter=None
tests_to_run = []
log_level = None
ssl_cert = None

for opt, val in opts:
  if opt in ('-h', '--help'):
    _usage_exit()
  elif opt in ('-u', '--url'):
    base_url = val
  elif opt in ('-f', '--fs-type'):
    fs_type = val
  elif opt in ('-v', '--verbose'):
    verbose = 1
  elif opt in ('-q', '--quiet'):
    quiet = 1
  elif opt in ('-c', '--cleanup'):
    cleanup = 1
  elif opt in ('-t', '--test'):
    tests_to_run.append(val)
  elif opt in ['-r', '--release']:
    objdir = 'Release'
  elif opt in ['-d', '--debug']:
    objdir = 'Debug'
  elif opt == '--svnserve-args':
    svnserve_args = val.split(',')
    run_svnserve = 1
  elif opt == '--asp.net-hack':
    os.environ['SVN_ASP_DOT_NET_HACK'] = opt
  elif opt == '--httpd-dir':
    abs_httpd_dir = os.path.abspath(val)
    run_httpd = 1
  elif opt == '--httpd-port':
    httpd_port = int(val)
  elif opt == '--httpd-daemon':
    httpd_service = 0
  elif opt == '--httpd-service':
    httpd_service = 1
  elif opt == '--httpd-no-log':
    httpd_no_log = 1
  elif opt == '--http-short-circuit':
    http_short_circuit = True
  elif opt == '--disable-http-v2':
    advertise_httpv2 = False
  elif opt == '--disable-bulk-updates':
    http_bulk_updates = False
  elif opt == '--fsfs-sharding':
    fsfs_sharding = int(val)
  elif opt == '--fsfs-packing':
    fsfs_packing = 1
  elif opt == '--javahl':
    test_javahl = 1
  elif opt == '--list':
    list_tests = 1
  elif opt == '--milestone-filter':
    milestone_filter = val
  elif opt == '--mode-filter':
    mode_filter = val
  elif opt == '--enable-sasl':
    enable_sasl = 1
    base_url = "svn://localhost/"
  elif opt == '--server-minor-version':
    server_minor_version = val
  elif opt == '--bin':
    svn_bin = val
  elif opt in ('-p', '--parallel'):
    parallel = 1
  elif opt in ('--config-file'):
    config_file = val
  elif opt == '--log-to-stdout':
    log_to_stdout = 1
  elif opt == '--log-level':
    log_level = val
  elif opt == '--ssl-cert':
    ssl_cert = val

# Calculate the source and test directory names
abs_srcdir = os.path.abspath("")
abs_objdir = os.path.join(abs_srcdir, objdir)
if len(args) == 0:
  abs_builddir = abs_objdir
  create_dirs = 0
else:
  abs_builddir = os.path.abspath(args[0])
  create_dirs = 1

# Default to fsfs explicitly
if not fs_type:
  fs_type = 'fsfs'

# Don't run bdb tests if they want to test fsfs
if fs_type == 'fsfs':
  all_tests = gen_obj.test_progs + gen_obj.scripts

if run_httpd:
  if not httpd_port:
    httpd_port = random.randrange(1024, 30000)
  if not base_url:
    base_url = 'http://localhost:' + str(httpd_port)

if base_url:
  repo_loc = 'remote repository ' + base_url + '.'
  if base_url[:4] == 'http':
    log = 'dav-tests.log'
    faillog = 'dav-fails.log'
  elif base_url[:3] == 'svn':
    log = 'svn-tests.log'
    faillog = 'svn-fails.log'
    run_svnserve = 1
  else:
    # Don't know this scheme, but who're we to judge whether it's
    # correct or not?
    log = 'url-tests.log'
    faillog = 'url-fails.log'

# Have to move the executables where the tests expect them to be
copied_execs = []   # Store copied exec files to avoid the final dir scan

def create_target_dir(dirname):
  tgt_dir = os.path.join(abs_builddir, dirname)
  if not os.path.exists(tgt_dir):
    if verbose:
      print("mkdir: %s" % tgt_dir)
    os.makedirs(tgt_dir)

def copy_changed_file(src, tgt):
  if not os.path.isfile(src):
    print('Could not find ' + src)
    sys.exit(1)
  if os.path.isdir(tgt):
    tgt = os.path.join(tgt, os.path.basename(src))
  if os.path.exists(tgt):
    assert os.path.isfile(tgt)
    if filecmp.cmp(src, tgt):
      if verbose:
        print("same: %s" % src)
        print(" and: %s" % tgt)
      return 0
  if verbose:
    print("copy: %s" % src)
    print("  to: %s" % tgt)
  shutil.copy(src, tgt)
  return 1

def copy_execs(baton, dirname, names):
  copied_execs = baton
  for name in names:
    if not name.endswith('.exe'):
      continue
    src = os.path.join(dirname, name)
    tgt = os.path.join(abs_builddir, dirname, name)
    create_target_dir(dirname)
    if copy_changed_file(src, tgt):
      copied_execs.append(tgt)

def locate_libs():
  "Move DLLs to a known location and set env vars"

  dlls = []

  # look for APR 1.x dll's and use those if found
  apr_test_path = os.path.join(gen_obj.apr_path, objdir, 'libapr-1.dll')
  if os.path.exists(apr_test_path):
    suffix = "-1"
  else:
    suffix = ""

  if cp.has_option('options', '--with-static-apr'):
    dlls.append(os.path.join(gen_obj.apr_path, objdir,
                             'libapr%s.dll' % (suffix)))
    dlls.append(os.path.join(gen_obj.apr_util_path, objdir,
                             'libaprutil%s.dll' % (suffix)))

  if gen_obj.libintl_path is not None:
    dlls.append(os.path.join(gen_obj.libintl_path, 'bin', 'intl3_svn.dll'))

  if gen_obj.bdb_lib is not None:
    partial_path = os.path.join(gen_obj.bdb_path, 'bin', gen_obj.bdb_lib)
    if objdir == 'Debug':
      dlls.append(partial_path + 'd.dll')
    else:
      dlls.append(partial_path + '.dll')

  if gen_obj.sasl_path is not None:
    dlls.append(os.path.join(gen_obj.sasl_path, 'lib', 'libsasl.dll'))

  for dll in dlls:
    copy_changed_file(dll, abs_objdir)

  # Copy the Subversion library DLLs
  if not cp.has_option('options', '--disable-shared'):
    for svn_dll in svn_dlls:
      copy_changed_file(os.path.join(abs_objdir, svn_dll), abs_objdir)

  # Copy the Apache modules
  if run_httpd and cp.has_option('options', '--with-httpd'):
    mod_dav_svn_path = os.path.join(abs_objdir, 'subversion',
                                    'mod_dav_svn', 'mod_dav_svn.so')
    mod_authz_svn_path = os.path.join(abs_objdir, 'subversion',
                                      'mod_authz_svn', 'mod_authz_svn.so')
    mod_dontdothat_path = os.path.join(abs_objdir, 'tools', 'server-side',
                                        'mod_dontdothat', 'mod_dontdothat.so')

    copy_changed_file(mod_dav_svn_path, abs_objdir)
    copy_changed_file(mod_authz_svn_path, abs_objdir)
    copy_changed_file(mod_dontdothat_path, abs_objdir)

  os.environ['PATH'] = abs_objdir + os.pathsep + os.environ['PATH']

def fix_case(path):
    path = os.path.normpath(path)
    parts = path.split(os.path.sep)
    drive = parts[0].upper()
    parts = parts[1:]
    path = drive + os.path.sep
    for part in parts:
        dirs = os.listdir(path)
        for dir in dirs:
            if dir.lower() == part.lower():
                path = os.path.join(path, dir)
                break
    return path

class Svnserve:
  "Run svnserve for ra_svn tests"
  def __init__(self, svnserve_args, objdir, abs_objdir, abs_builddir):
    self.args = svnserve_args
    self.name = 'svnserve.exe'
    self.kind = objdir
    self.path = os.path.join(abs_objdir,
                             'subversion', 'svnserve', self.name)
    self.root = os.path.join(abs_builddir, CMDLINE_TEST_SCRIPT_NATIVE_PATH)
    self.proc_handle = None

  def __del__(self):
    "Stop svnserve when the object is deleted"
    self.stop()

  def _quote(self, arg):
    if ' ' in arg:
      return '"' + arg + '"'
    else:
      return arg

  def start(self):
    if not self.args:
      args = [self.name, '-d', '-r', self.root]
    else:
      args = [self.name] + self.args
    print('Starting %s %s' % (self.kind, self.name))
    try:
      import win32process
      import win32con
      args = ' '.join([self._quote(x) for x in args])
      self.proc_handle = (
        win32process.CreateProcess(self._quote(self.path), args,
                                   None, None, 0,
                                   win32con.CREATE_NEW_CONSOLE,
                                   None, None, win32process.STARTUPINFO()))[0]
    except ImportError:
      os.spawnv(os.P_NOWAIT, self.path, args)

  def stop(self):
    if self.proc_handle is not None:
      try:
        import win32process
        print('Stopping %s' % self.name)
        win32process.TerminateProcess(self.proc_handle, 0)
        return
      except ImportError:
        pass
    print('Svnserve.stop not implemented')

class Httpd:
  "Run httpd for DAV tests"
  def __init__(self, abs_httpd_dir, abs_objdir, abs_builddir, httpd_port,
               service, no_log, httpv2, short_circuit, bulk_updates):
    self.name = 'apache.exe'
    self.httpd_port = httpd_port
    self.httpd_dir = abs_httpd_dir

    if httpv2:
      self.httpv2_option = 'on'
    else:
      self.httpv2_option = 'off'

    if bulk_updates:
      self.bulkupdates_option = 'on'
    else:
      self.bulkupdates_option = 'off'

    self.service = service
    self.proc_handle = None
    self.path = os.path.join(self.httpd_dir, 'bin', self.name)

    if short_circuit:
      self.path_authz_option = 'short_circuit'
    else:
      self.path_authz_option = 'on'

    if not os.path.exists(self.path):
      self.name = 'httpd.exe'
      self.path = os.path.join(self.httpd_dir, 'bin', self.name)
      if not os.path.exists(self.path):
        raise RuntimeError("Could not find a valid httpd binary!")

    self.root_dir = os.path.join(CMDLINE_TEST_SCRIPT_NATIVE_PATH, 'httpd')
    self.root = os.path.join(abs_builddir, self.root_dir)
    self.authz_file = os.path.join(abs_builddir,
                                   CMDLINE_TEST_SCRIPT_NATIVE_PATH,
                                   'svn-test-work', 'authz')
    self.dontdothat_file = os.path.join(abs_builddir,
                                         CMDLINE_TEST_SCRIPT_NATIVE_PATH,
                                         'svn-test-work', 'dontdothat')
    self.httpd_config = os.path.join(self.root, 'httpd.conf')
    self.httpd_users = os.path.join(self.root, 'users')
    self.httpd_mime_types = os.path.join(self.root, 'mime.types')
    self.httpd_groups = os.path.join(self.root, 'groups')
    self.abs_builddir = abs_builddir
    self.abs_objdir = abs_objdir
    self.service_name = 'svn-test-httpd-' + str(httpd_port)

    if self.service:
      self.httpd_args = [self.name, '-n', self._quote(self.service_name),
                         '-f', self._quote(self.httpd_config)]
    else:
      self.httpd_args = [self.name, '-f', self._quote(self.httpd_config)]

    create_target_dir(self.root_dir)

    self._create_users_file()
    self._create_groups_file()
    self._create_mime_types_file()
    self._create_dontdothat_file()

    # Determine version.
    if os.path.exists(os.path.join(self.httpd_dir,
                                   'modules', 'mod_access_compat.so')):
      self.httpd_ver = 2.3
    elif os.path.exists(os.path.join(self.httpd_dir,
                                     'modules', 'mod_auth_basic.so')):
      self.httpd_ver = 2.2
    else:
      self.httpd_ver = 2.0

    # Create httpd config file
    fp = open(self.httpd_config, 'w')

    # Limit the number of threads (default = 64)
    fp.write('<IfModule mpm_winnt.c>\n')
    fp.write('ThreadsPerChild 16\n')
    fp.write('</IfModule>\n')

    # Global Environment
    fp.write('ServerRoot   ' + self._quote(self.root) + '\n')
    fp.write('DocumentRoot ' + self._quote(self.root) + '\n')
    fp.write('ServerName   localhost\n')
    fp.write('PidFile      pid\n')
    fp.write('ErrorLog     log\n')
    fp.write('Listen       ' + str(self.httpd_port) + '\n')

    if not no_log:
      fp.write('LogFormat    "%h %l %u %t \\"%r\\" %>s %b" common\n')
      fp.write('Customlog    log common\n')
      fp.write('LogLevel     Debug\n')
    else:
      fp.write('LogLevel     Crit\n')

    # Write LoadModule for minimal system module
    fp.write(self._sys_module('dav_module', 'mod_dav.so'))
    if self.httpd_ver >= 2.3:
      fp.write(self._sys_module('access_compat_module', 'mod_access_compat.so'))
      fp.write(self._sys_module('authz_core_module', 'mod_authz_core.so'))
      fp.write(self._sys_module('authz_user_module', 'mod_authz_user.so'))
      fp.write(self._sys_module('authn_core_module', 'mod_authn_core.so'))
    if self.httpd_ver >= 2.2:
      fp.write(self._sys_module('auth_basic_module', 'mod_auth_basic.so'))
      fp.write(self._sys_module('authn_file_module', 'mod_authn_file.so'))
      fp.write(self._sys_module('authz_groupfile_module', 'mod_authz_groupfile.so'))
      fp.write(self._sys_module('authz_host_module', 'mod_authz_host.so'))
    else:
      fp.write(self._sys_module('auth_module', 'mod_auth.so'))
    fp.write(self._sys_module('alias_module', 'mod_alias.so'))
    fp.write(self._sys_module('mime_module', 'mod_mime.so'))
    fp.write(self._sys_module('log_config_module', 'mod_log_config.so'))

    # Write LoadModule for Subversion modules
    fp.write(self._svn_module('dav_svn_module', 'mod_dav_svn.so'))
    fp.write(self._svn_module('authz_svn_module', 'mod_authz_svn.so'))

    # And for mod_dontdothat
    fp.write(self._svn_module('dontdothat_module', 'mod_dontdothat.so'))

    # Don't handle .htaccess, symlinks, etc.
    fp.write('<Directory />\n')
    fp.write('AllowOverride None\n')
    fp.write('Options None\n')
    fp.write('</Directory>\n\n')

    # Define two locations for repositories
    fp.write(self._svn_repo('repositories'))
    fp.write(self._svn_repo('local_tmp'))
    fp.write(self._svn_authz_repo())

    # And two redirects for the redirect tests
    fp.write('RedirectMatch permanent ^/svn-test-work/repositories/'
             'REDIRECT-PERM-(.*)$ /svn-test-work/repositories/$1\n')
    fp.write('RedirectMatch           ^/svn-test-work/repositories/'
             'REDIRECT-TEMP-(.*)$ /svn-test-work/repositories/$1\n')

    fp.write('TypesConfig     ' + self._quote(self.httpd_mime_types) + '\n')
    fp.write('HostNameLookups Off\n')

    fp.close()

  def __del__(self):
    "Stop httpd when the object is deleted"
    self.stop()

  def _quote(self, arg):
    if ' ' in arg:
      return '"' + arg + '"'
    else:
      return arg

  def _create_users_file(self):
    "Create users file"
    htpasswd = os.path.join(self.httpd_dir, 'bin', 'htpasswd.exe')
    # Create the cheapest to compare password form for our testsuite
    os.spawnv(os.P_WAIT, htpasswd, ['htpasswd.exe', '-bcp', self.httpd_users,
                                    'jrandom', 'rayjandom'])
    os.spawnv(os.P_WAIT, htpasswd, ['htpasswd.exe', '-bp',  self.httpd_users,
                                    'jconstant', 'rayjandom'])
    os.spawnv(os.P_WAIT, htpasswd, ['htpasswd.exe', '-bp',  self.httpd_users,
                                    'JRANDOM', 'rayjandom'])
    os.spawnv(os.P_WAIT, htpasswd, ['htpasswd.exe', '-bp',  self.httpd_users,
                                    'JCONSTANT', 'rayjandom'])

  def _create_groups_file(self):
    "Create groups for mod_authz_svn tests"
    fp = open(self.httpd_groups, 'w')
    fp.write('random: jrandom\n')
    fp.write('constant: jconstant\n')
    fp.close()

  def _create_mime_types_file(self):
    "Create empty mime.types file"
    fp = open(self.httpd_mime_types, 'w')
    fp.close()

  def _create_dontdothat_file(self):
    "Create empty mime.types file"
    # If the tests have not previously been run or were cleaned
    # up, then 'svn-test-work' does not exist yet.
    parent_dir = os.path.dirname(self.dontdothat_file)
    if not os.path.exists(parent_dir):
      os.makedirs(parent_dir)

    fp = open(self.dontdothat_file, 'w')
    fp.write('[recursive-actions]\n')
    fp.write('/ = deny\n')
    fp.close()

  def _sys_module(self, name, path):
    full_path = os.path.join(self.httpd_dir, 'modules', path)
    return 'LoadModule ' + name + " " + self._quote(full_path) + '\n'

  def _svn_module(self, name, path):
    full_path = os.path.join(self.abs_objdir, path)
    return 'LoadModule ' + name + ' ' + self._quote(full_path) + '\n'

  def _svn_repo(self, name):
    path = os.path.join(self.abs_builddir,
                        CMDLINE_TEST_SCRIPT_NATIVE_PATH,
                        'svn-test-work', name)
    location = '/svn-test-work/' + name
    ddt_location = '/ddt-test-work/' + name
    return \
      '<Location ' + location + '>\n' \
      '  DAV             svn\n' \
      '  SVNParentPath   ' + self._quote(path) + '\n' \
      '  SVNAdvertiseV2Protocol ' + self.httpv2_option + '\n' \
      '  SVNPathAuthz ' + self.path_authz_option + '\n' \
      '  SVNAllowBulkUpdates ' + self.bulkupdates_option + '\n' \
      '  AuthzSVNAccessFile ' + self._quote(self.authz_file) + '\n' \
      '  AuthType        Basic\n' \
      '  AuthName        "Subversion Repository"\n' \
      '  AuthUserFile    ' + self._quote(self.httpd_users) + '\n' \
      '  Require         valid-user\n' \
      '</Location>\n' \
      '<Location ' + ddt_location + '>\n' \
      '  DAV             svn\n' \
      '  SVNParentPath   ' + self._quote(path) + '\n' \
      '  SVNAdvertiseV2Protocol ' + self.httpv2_option + '\n' \
      '  SVNPathAuthz ' + self.path_authz_option + '\n' \
      '  SVNAllowBulkUpdates ' + self.bulkupdates_option + '\n' \
      '  AuthzSVNAccessFile ' + self._quote(self.authz_file) + '\n' \
      '  AuthType        Basic\n' \
      '  AuthName        "Subversion Repository"\n' \
      '  AuthUserFile    ' + self._quote(self.httpd_users) + '\n' \
      '  Require         valid-user\n' \
      '  DontDoThatConfigFile ' + self._quote(self.dontdothat_file) + '\n' \
      '</Location>\n'

  def _svn_authz_repo(self):
    local_tmp = os.path.join(self.abs_builddir,
                             CMDLINE_TEST_SCRIPT_NATIVE_PATH,
                             'svn-test-work', 'local_tmp')
    return \
      '<Location /authz-test-work/anon>' + '\n' \
      '  DAV               svn' + '\n' \
      '  SVNParentPath     ' + local_tmp + '\n' \
      '  AuthzSVNAccessFile ' + self._quote(self.authz_file) + '\n' \
      '  SVNAdvertiseV2Protocol ' + self.httpv2_option + '\n' \
      '  SVNListParentPath On' + '\n' \
      '  <IfModule mod_authz_core.c>' + '\n' \
      '    Require all granted' + '\n' \
      '  </IfModule>' + '\n' \
      '  <IfModule !mod_authz_core.c>' + '\n' \
      '    Allow from all' + '\n' \
      '  </IfModule>' + '\n' \
      '  SVNPathAuthz ' + self.path_authz_option + '\n' \
      '</Location>' + '\n' \
      '<Location /authz-test-work/mixed>' + '\n' \
      '  DAV               svn' + '\n' \
      '  SVNParentPath     ' + local_tmp + '\n' \
      '  AuthzSVNAccessFile ' + self._quote(self.authz_file) + '\n' \
      '  SVNAdvertiseV2Protocol ' + self.httpv2_option + '\n' \
      '  SVNListParentPath On' + '\n' \
      '  AuthType          Basic' + '\n' \
      '  AuthName          "Subversion Repository"' + '\n' \
      '  AuthUserFile    ' + self._quote(self.httpd_users) + '\n' \
      '  Require           valid-user' + '\n' \
      '  Satisfy Any' + '\n' \
      '  SVNPathAuthz ' + self.path_authz_option + '\n' \
      '</Location>' + '\n' \
      '<Location /authz-test-work/mixed-noauthwhenanon>' + '\n' \
      '  DAV               svn' + '\n' \
      '  SVNParentPath     ' + local_tmp + '\n' \
      '  AuthzSVNAccessFile ' + self._quote(self.authz_file) + '\n' \
      '  SVNAdvertiseV2Protocol ' + self.httpv2_option + '\n' \
      '  SVNListParentPath On' + '\n' \
      '  AuthType          Basic' + '\n' \
      '  AuthName          "Subversion Repository"' + '\n' \
      '  AuthUserFile    ' + self._quote(self.httpd_users) + '\n' \
      '  Require           valid-user' + '\n' \
      '  AuthzSVNNoAuthWhenAnonymousAllowed On' + '\n' \
      '  SVNPathAuthz On' + '\n' \
      '</Location>' + '\n' \
      '<Location /authz-test-work/authn>' + '\n' \
      '  DAV               svn' + '\n' \
      '  SVNParentPath     ' + local_tmp + '\n' \
      '  AuthzSVNAccessFile ' + self._quote(self.authz_file) + '\n' \
      '  SVNAdvertiseV2Protocol ' + self.httpv2_option + '\n' \
      '  SVNListParentPath On' + '\n' \
      '  AuthType          Basic' + '\n' \
      '  AuthName          "Subversion Repository"' + '\n' \
      '  AuthUserFile    ' + self._quote(self.httpd_users) + '\n' \
      '  Require           valid-user' + '\n' \
      '  SVNPathAuthz ' + self.path_authz_option + '\n' \
      '</Location>' + '\n' \
      '<Location /authz-test-work/authn-anonoff>' + '\n' \
      '  DAV               svn' + '\n' \
      '  SVNParentPath     ' + local_tmp + '\n' \
      '  AuthzSVNAccessFile ' + self._quote(self.authz_file) + '\n' \
      '  SVNAdvertiseV2Protocol ' + self.httpv2_option + '\n' \
      '  SVNListParentPath On' + '\n' \
      '  AuthType          Basic' + '\n' \
      '  AuthName          "Subversion Repository"' + '\n' \
      '  AuthUserFile    ' + self._quote(self.httpd_users) + '\n' \
      '  Require           valid-user' + '\n' \
      '  AuthzSVNAnonymous Off' + '\n' \
      '  SVNPathAuthz On' + '\n' \
      '</Location>' + '\n' \
      '<Location /authz-test-work/authn-lcuser>' + '\n' \
      '  DAV               svn' + '\n' \
      '  SVNParentPath     ' + local_tmp + '\n' \
      '  AuthzSVNAccessFile ' + self._quote(self.authz_file) + '\n' \
      '  SVNAdvertiseV2Protocol ' + self.httpv2_option + '\n' \
      '  SVNListParentPath On' + '\n' \
      '  AuthType          Basic' + '\n' \
      '  AuthName          "Subversion Repository"' + '\n' \
      '  AuthUserFile    ' + self._quote(self.httpd_users) + '\n' \
      '  Require           valid-user' + '\n' \
      '  AuthzForceUsernameCase Lower' + '\n' \
      '  SVNPathAuthz ' + self.path_authz_option + '\n' \
      '</Location>' + '\n' \
      '<Location /authz-test-work/authn-lcuser>' + '\n' \
      '  DAV               svn' + '\n' \
      '  SVNParentPath     ' + local_tmp + '\n' \
      '  AuthzSVNAccessFile ' + self._quote(self.authz_file) + '\n' \
      '  SVNAdvertiseV2Protocol ' + self.httpv2_option + '\n' \
      '  SVNListParentPath On' + '\n' \
      '  AuthType          Basic' + '\n' \
      '  AuthName          "Subversion Repository"' + '\n' \
      '  AuthUserFile    ' + self._quote(self.httpd_users) + '\n' \
      '  Require           valid-user' + '\n' \
      '  AuthzForceUsernameCase Lower' + '\n' \
      '  SVNPathAuthz ' + self.path_authz_option + '\n' \
      '</Location>' + '\n' \
      '<Location /authz-test-work/authn-group>' + '\n' \
      '  DAV               svn' + '\n' \
      '  SVNParentPath     ' + local_tmp + '\n' \
      '  AuthzSVNAccessFile ' + self._quote(self.authz_file) + '\n' \
      '  SVNAdvertiseV2Protocol ' + self.httpv2_option + '\n' \
      '  SVNListParentPath On' + '\n' \
      '  AuthType          Basic' + '\n' \
      '  AuthName          "Subversion Repository"' + '\n' \
      '  AuthUserFile    ' + self._quote(self.httpd_users) + '\n' \
      '  AuthGroupFile    ' + self._quote(self.httpd_groups) + '\n' \
      '  Require           group random' + '\n' \
      '  AuthzSVNAuthoritative Off' + '\n' \
      '  SVNPathAuthz On' + '\n' \
      '</Location>' + '\n' \
      '<IfModule mod_authz_core.c>' + '\n' \
      '<Location /authz-test-work/sallrany>' + '\n' \
      '  DAV               svn' + '\n' \
      '  SVNParentPath     ' + local_tmp + '\n' \
      '  AuthzSVNAccessFile ' + self._quote(self.authz_file) + '\n' \
      '  SVNAdvertiseV2Protocol ' + self.httpv2_option + '\n' \
      '  SVNListParentPath On' + '\n' \
      '  AuthType          Basic' + '\n' \
      '  AuthName          "Subversion Repository"' + '\n' \
      '  AuthUserFile    ' + self._quote(self.httpd_users) + '\n' \
      '  AuthzSendForbiddenOnFailure On' + '\n' \
      '  Satisfy All' + '\n' \
      '  <RequireAny>' + '\n' \
      '    Require valid-user' + '\n' \
      '    Require expr req(\'ALLOW\') == \'1\'' + '\n' \
      '  </RequireAny>' + '\n' \
      '  SVNPathAuthz ' + self.path_authz_option + '\n' \
      '</Location>' + '\n' \
      '<Location /authz-test-work/sallrall>'+ '\n' \
      '  DAV               svn' + '\n' \
      '  SVNParentPath     ' + local_tmp + '\n' \
      '  AuthzSVNAccessFile ' + self._quote(self.authz_file) + '\n' \
      '  SVNAdvertiseV2Protocol ' + self.httpv2_option + '\n' \
      '  SVNListParentPath On' + '\n' \
      '  AuthType          Basic' + '\n' \
      '  AuthName          "Subversion Repository"' + '\n' \
      '  AuthUserFile    ' + self._quote(self.httpd_users) + '\n' \
      '  AuthzSendForbiddenOnFailure On' + '\n' \
      '  Satisfy All' + '\n' \
      '  <RequireAll>' + '\n' \
      '    Require valid-user' + '\n' \
      '    Require expr req(\'ALLOW\') == \'1\'' + '\n' \
      '  </RequireAll>' + '\n' \
      '  SVNPathAuthz ' + self.path_authz_option + '\n' \
      '</Location>' + '\n' \
      '</IfModule>' + '\n' \

  def start(self):
    if self.service:
      self._start_service()
    else:
      self._start_daemon()

  def stop(self):
    if self.service:
      self._stop_service()
    else:
      self._stop_daemon()

  def _start_service(self):
    "Install and start HTTPD service"
    print('Installing service %s' % self.service_name)
    os.spawnv(os.P_WAIT, self.path, self.httpd_args + ['-k', 'install'])
    print('Starting service %s' % self.service_name)
    os.spawnv(os.P_WAIT, self.path, self.httpd_args + ['-k', 'start'])

  def _stop_service(self):
    "Stop and uninstall HTTPD service"
    os.spawnv(os.P_WAIT, self.path, self.httpd_args + ['-k', 'stop'])
    os.spawnv(os.P_WAIT, self.path, self.httpd_args + ['-k', 'uninstall'])

  def _start_daemon(self):
    "Start HTTPD as daemon"
    print('Starting httpd as daemon')
    print(self.httpd_args)
    try:
      import win32process
      import win32con
      args = ' '.join([self._quote(x) for x in self.httpd_args])
      self.proc_handle = (
        win32process.CreateProcess(self._quote(self.path), args,
                                   None, None, 0,
                                   win32con.CREATE_NEW_CONSOLE,
                                   None, None, win32process.STARTUPINFO()))[0]
    except ImportError:
      os.spawnv(os.P_NOWAIT, self.path, self.httpd_args)

  def _stop_daemon(self):
    "Stop the HTTPD daemon"
    if self.proc_handle is not None:
      try:
        import win32process
        print('Stopping %s' % self.name)
        win32process.TerminateProcess(self.proc_handle, 0)
        return
      except ImportError:
        pass
    print('Httpd.stop_daemon not implemented')

# Move the binaries to the test directory
locate_libs()
if create_dirs:
  old_cwd = os.getcwd()
  try:
    os.chdir(abs_objdir)
    baton = copied_execs
    for dirpath, dirs, files in os.walk('subversion'):
      copy_execs(baton, dirpath, files)
    for dirpath, dirs, files in os.walk('tools/server-side'):
      copy_execs(baton, dirpath, files)
  except:
    os.chdir(old_cwd)
    raise
  else:
    os.chdir(old_cwd)

# Create the base directory for Python tests
create_target_dir(CMDLINE_TEST_SCRIPT_NATIVE_PATH)

# Ensure the tests directory is correctly cased
abs_builddir = fix_case(abs_builddir)

daemon = None
# Run the tests

# No need to start any servers if we are only listing the tests.
if not list_tests:
  if run_svnserve:
    daemon = Svnserve(svnserve_args, objdir, abs_objdir, abs_builddir)

  if run_httpd:
    daemon = Httpd(abs_httpd_dir, abs_objdir, abs_builddir, httpd_port,
                   httpd_service, httpd_no_log,
                   advertise_httpv2, http_short_circuit,
                   http_bulk_updates)

  # Start service daemon, if any
  if daemon:
    daemon.start()

# Find the full path and filename of any test that is specified just by
# its base name.
if len(tests_to_run) != 0:
  tests = []
  for t in tests_to_run:
    tns = None
    if '#' in t:
      t, tns = t.split('#')

    test = [x for x in all_tests if x.split('/')[-1] == t]
    if not test and not (t.endswith('-test.exe') or t.endswith('_tests.py')):
      # The lengths of '-test.exe' and of '_tests.py' are both 9.
      test = [x for x in all_tests if x.split('/')[-1][:-9] == t]

    if not test:
      print("Skipping test '%s', test not found." % t)
    elif tns:
      tests.append('%s#%s' % (test[0], tns))
    else:
      tests.extend(test)

  tests_to_run = tests
else:
  tests_to_run = all_tests


if list_tests:
  print('Listing %s configuration on %s' % (objdir, repo_loc))
else:
  print('Testing %s configuration on %s' % (objdir, repo_loc))
sys.path.insert(0, os.path.join(abs_srcdir, 'build'))

if not test_javahl:
  import run_tests
  if log_to_stdout:
    log_file = None
    fail_log_file = None
  else:
    log_file = os.path.join(abs_builddir, log)
    fail_log_file = os.path.join(abs_builddir, faillog)

  if run_httpd:
    httpd_version = "%.1f" % daemon.httpd_ver
  else:
    httpd_version = None
  th = run_tests.TestHarness(abs_srcdir, abs_builddir,
                             log_file,
                             fail_log_file,
                             base_url, fs_type, 'serf',
                             server_minor_version, not quiet,
                             cleanup, enable_sasl, parallel, config_file,
                             fsfs_sharding, fsfs_packing,
                             list_tests, svn_bin, mode_filter,
                             milestone_filter,
                             httpd_version=httpd_version,
                             set_log_level=log_level, ssl_cert=ssl_cert)
  old_cwd = os.getcwd()
  try:
    os.chdir(abs_builddir)
    failed = th.run(tests_to_run)
  except:
    os.chdir(old_cwd)
    raise
  else:
    os.chdir(old_cwd)
else:
  failed = False
  args = (
          'java.exe',
          '-Dtest.rootdir=' + os.path.join(abs_builddir, 'javahl'),
          '-Dtest.srcdir=' + os.path.join(abs_srcdir,
                                          'subversion/bindings/javahl'),
          '-Dtest.rooturl=',
          '-Dtest.fstype=' + fs_type ,
          '-Dtest.tests=',

          '-Djava.library.path='
                    + os.path.join(abs_objdir,
                                   'subversion/bindings/javahl/native'),
          '-classpath',
          os.path.join(abs_srcdir, 'subversion/bindings/javahl/classes') +';' +
            gen_obj.junit_path
         )

  sys.stderr.flush()
  print('Running org.apache.subversion tests:')
  sys.stdout.flush()

  r = subprocess.call(args + tuple(['org.apache.subversion.javahl.RunTests']))
  sys.stdout.flush()
  sys.stderr.flush()
  if (r != 0):
    print('[Test runner reported failure]')
    failed = True

  print('Running org.tigris.subversion tests:')
  sys.stdout.flush()
  r = subprocess.call(args + tuple(['org.tigris.subversion.javahl.RunTests']))
  sys.stdout.flush()
  sys.stderr.flush()
  if (r != 0):
    print('[Test runner reported failure]')
    failed = True

# Stop service daemon, if any
if daemon:
  del daemon

# Remove the execs again
for tgt in copied_execs:
  try:
    if os.path.isfile(tgt):
      if verbose:
        print("kill: %s" % tgt)
      os.unlink(tgt)
  except:
    traceback.print_exc(file=sys.stdout)
    pass


if failed:
  sys.exit(1)
