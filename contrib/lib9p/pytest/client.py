#! /usr/bin/env python

"""
Run various tests, as a client.
"""

from __future__ import print_function

import argparse
try:
    import ConfigParser as configparser
except ImportError:
    import configparser
import functools
import logging
import os
import socket
import struct
import sys
import time
import traceback

import p9conn
import protocol

LocalError = p9conn.LocalError
RemoteError = p9conn.RemoteError
TEError = p9conn.TEError

class TestState(object):
    def __init__(self):
        self.config = None
        self.logger = None
        self.successes = 0
        self.skips = 0
        self.failures = 0
        self.exceptions = 0
        self.clnt_tab = {}
        self.mkclient = None
        self.stop = False
        self.gid = 0

    def ccc(self, cid=None):
        """
        Connect or reconnect as client (ccc = check and connect client).

        If caller provides a cid (client ID) we check that specific
        client.  Otherwise the default ID ('base') is used.
        In any case we return the now-connected client, plus the
        attachment (session info) if any.
        """
        if cid is None:
            cid = 'base'
        pair = self.clnt_tab.get(cid)
        if pair is None:
            clnt = self.mkclient()
            pair = [clnt, None]
            self.clnt_tab[cid] = pair
        else:
            clnt = pair[0]
        if not clnt.is_connected():
            clnt.connect()
        return pair

    def dcc(self, cid=None):
        """
        Disconnect client (disconnect checked client).  If no specific
        client ID is provided, this disconnects ALL checked clients!
        """
        if cid is None:
            for cid in list(self.clnt_tab.keys()):
                self.dcc(cid)
        pair = self.clnt_tab.get(cid)
        if pair is not None:
            clnt = pair[0]
            if clnt.is_connected():
                clnt.shutdown()
            del self.clnt_tab[cid]

    def ccs(self, cid=None):
        """
        Like ccc, but establish a session as well, by setting up
        the uname/n_uname.

        Return the client instance (only).
        """
        pair = self.ccc(cid)
        clnt = pair[0]
        if pair[1] is None:
            # No session yet - establish one.  Note, this may fail.
            section = None if cid is None else ('client-' + cid)
            aname = getconf(self.config, section, 'aname', '')
            uname = getconf(self.config, section, 'uname', '')
            if clnt.proto > protocol.plain:
                n_uname = getint(self.config, section, 'n_uname', 1001)
            else:
                n_uname = None
            clnt.attach(afid=None, aname=aname, uname=uname, n_uname=n_uname)
            pair[1] = (aname, uname, n_uname)
        return clnt

def getconf(conf, section, name, default=None, rtype=str):
    """
    Get configuration item for given section, or for "client" if
    there is no entry for that particular section (or if section
    is None).

    This lets us get specific values for specific tests or
    groups ([foo] name=value), falling back to general values
    ([client] name=value).

    The type of the returned value <rtype> can be str, int, bool,
    or float.  The default is str (and see getconfint, getconfbool,
    getconffloat below).

    A default value may be supplied; if it is, that's the default
    return value (this default should have the right type).  If
    no default is supplied, a missing value is an error.
    """
    try:
        # note: conf.get(None, 'foo') raises NoSectionError
        where = section
        result = conf.get(where, name)
    except (configparser.NoSectionError, configparser.NoOptionError):
        try:
            where = 'client'
            result = conf.get(where, name)
        except configparser.NoSectionError:
            sys.exit('no [{0}] section in configuration!'.format(where))
        except configparser.NoOptionError:
            if default is not None:
                return default
            if section is not None:
                where = '[{0}] or [{1}]'.format(section, where)
            else:
                where = '[{0}]'.format(where)
            raise LocalError('need {0}=value in {1}'.format(name, where))
    where = '[{0}]'.format(where)
    if rtype is str:
        return result
    if rtype is int:
        return int(result)
    if rtype is float:
        return float(result)
    if rtype is bool:
        if result.lower() in ('1', 't', 'true', 'y', 'yes'):
            return True
        if result.lower() in ('0', 'f', 'false', 'n', 'no'):
            return False
        raise ValueError('{0} {1}={2}: invalid boolean'.format(where, name,
                                                              result))
    raise ValueError('{0} {1}={2}: internal error: bad result type '
                     '{3!r}'.format(where, name, result, rtype))

def getint(conf, section, name, default=None):
    "get integer config item"
    return getconf(conf, section, name, default, int)

def getfloat(conf, section, name, default=None):
    "get float config item"
    return getconf(conf, section, name, default, float)

def getbool(conf, section, name, default=None):
    "get boolean config item"
    return getconf(conf, section, name, default, bool)

def pluralize(n, singular, plural):
    "return singular or plural based on value of n"
    return plural if n != 1 else singular

class TCDone(Exception):
    "used in succ/fail/skip - skips rest of testcase with"
    pass

class TestCase(object):
    """
    Start a test case.  Most callers must then do a ccs() to connect.

    A failed test will generally disconnect from the server; a
    new ccs() will reconnect, if the server is still alive.
    """
    def __init__(self, name, tstate):
        self.name = name
        self.status = None
        self.detail = None
        self.tstate = tstate
        self._shutdown = None
        self._autoclunk = None
        self._acconn = None

    def auto_disconnect(self, conn):
        self._shutdown = conn

    def succ(self, detail=None):
        "set success status"
        self.status = 'SUCC'
        self.detail = detail
        raise TCDone()

    def fail(self, detail):
        "set failure status"
        self.status = 'FAIL'
        self.detail = detail
        raise TCDone()

    def skip(self, detail=None):
        "set skip status"
        self.status = 'SKIP'
        self.detail = detail
        raise TCDone()

    def autoclunk(self, fid):
        "mark fid to be closed/clunked on test exit"
        if self._acconn is None:
            raise ValueError('autoclunk: no _acconn')
        self._autoclunk.append(fid)

    def trace(self, msg, *args, **kwargs):
        "add tracing info to log-file output"
        level = kwargs.pop('level', logging.INFO)
        self.tstate.logger.log(level, '      ' + msg, *args, **kwargs)

    def ccs(self):
        "call tstate ccs, turn socket.error connect failure into test fail"
        try:
            self.detail = 'connecting'
            ret = self.tstate.ccs()
            self.detail = None
            self._acconn = ret
            return ret
        except socket.error as err:
            self.fail(str(err))

    def __enter__(self):
        self.tstate.logger.log(logging.DEBUG, 'ENTER: %s', self.name)
        self._autoclunk = []
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        tstate = self.tstate
        eat_exc = False
        tb_detail = None
        if exc_type is TCDone:
            # we exited with succ, fail, or skip
            eat_exc = True
            exc_type = None
        if exc_type is not None:
            if self.status is None:
                self.status = 'EXCP'
            else:
                self.status += ' EXC'
            if exc_type == TEError:
                # timeout/eof - best guess is that we crashed the server!
                eat_exc = True
                tb_detail = ['timeout or EOF']
            elif exc_type in (socket.error, RemoteError, LocalError):
                eat_exc = True
                tb_detail = traceback.format_exception(exc_type, exc_val,
                                                       exc_tb)
            level = logging.ERROR
            tstate.failures += 1
            tstate.exceptions += 1
        else:
            if self.status is None:
                self.status = 'SUCC'
            if self.status == 'SUCC':
                level = logging.INFO
                tstate.successes += 1
            elif self.status == 'SKIP':
                level = logging.INFO
                tstate.skips += 1
            else:
                level = logging.ERROR
                tstate.failures += 1
        tstate.logger.log(level, '%s: %s', self.status, self.name)
        if self.detail:
            tstate.logger.log(level, '      detail: %s', self.detail)
        if tb_detail:
            for line in tb_detail:
                tstate.logger.log(level, '      %s', line.rstrip())
        for fid in self._autoclunk:
            self._acconn.clunk(fid, ignore_error=True)
        if self._shutdown:
            self._shutdown.shutdown()
        return eat_exc

def main():
    "the usual main"
    parser = argparse.ArgumentParser(description='run tests against a server')

    parser.add_argument('-c', '--config',
        action='append',
        help='specify additional file(s) to read (beyond testconf.ini)')

    args = parser.parse_args()
    config = configparser.SafeConfigParser()
    # use case sensitive keys
    config.optionxform = str

    try:
        with open('testconf.ini', 'r') as stream:
            config.readfp(stream)
    except (OSError, IOError) as err:
        sys.exit(str(err))
    if args.config:
        ok = config.read(args.config)
        failed = set(ok) - set(args.config)
        if len(failed):
            nfailed = len(failed)
            word = 'files' if nfailed > 1 else 'file'
            failed = ', '.join(failed)
            print('failed to read {0} {1}: {2}'.format(nfailed, word, failed))
            sys.exit(1)

    logging.basicConfig(level=config.get('client', 'loglevel').upper())
    logger = logging.getLogger(__name__)
    tstate = TestState()
    tstate.logger = logger
    tstate.config = config

    server = config.get('client', 'server')
    port = config.getint('client', 'port')
    proto = config.get('client', 'protocol')
    may_downgrade = config.getboolean('client', 'may_downgrade')
    timeout = config.getfloat('client', 'timeout')

    tstate.stop = True # unless overwritten below
    with TestCase('send bad packet', tstate) as tc:
        tc.detail = 'connecting to {0}:{1}'.format(server, port)
        try:
            conn = p9conn.P9SockIO(logger, server=server, port=port)
        except socket.error as err:
            tc.fail('cannot connect at all (server down?)')
        tc.auto_disconnect(conn)
        tc.detail = None
        pkt = struct.pack('<I', 256);
        conn.write(pkt)
        # ignore reply if any, we're just trying to trip the server
        tstate.stop = False
        tc.succ()

    if not tstate.stop:
        tstate.mkclient = functools.partial(p9conn.P9Client, logger,
                                           timeout, proto, may_downgrade,
                                           server=server, port=port)
        tstate.stop = True
        with TestCase('send bad Tversion', tstate) as tc:
            try:
                clnt = tstate.mkclient()
            except socket.error as err:
                tc.fail('can no longer connect, did bad pkt crash server?')
            tc.auto_disconnect(clnt)
            clnt.set_monkey('version', b'wrongo, fishbreath!')
            tc.detail = 'connecting'
            try:
                clnt.connect()
            except RemoteError as err:
                tstate.stop = False
                tc.succ(err.args[0])
            tc.fail('server accepted a bad Tversion')

    if not tstate.stop:
        # All NUL characters in strings are invalid.
        with TestCase('send illegal NUL in Tversion', tstate) as tc:
            clnt = tstate.mkclient()
            tc.auto_disconnect(clnt)
            clnt.set_monkey('version', b'9P2000\0')
            # Forcibly allow downgrade so that Tversion
            # succeeds if they ignore the \0.
            clnt.may_downgrade = True
            tc.detail = 'connecting'
            try:
                clnt.connect()
            except (TEError, RemoteError) as err:
                tc.succ(err.args[0])
            tc.fail('server accepted NUL in Tversion')

    if not tstate.stop:
        with TestCase('connect normally', tstate) as tc:
            tc.detail = 'connecting'
            try:
                tstate.ccc()
            except RemoteError as err:
                # can't test any further, but this might be success
                tstate.stop = True
                if 'they only support version' in err.args[0]:
                    tc.succ(err.args[0])
                tc.fail(err.args[0])
            tc.succ()

    if not tstate.stop:
        with TestCase('attach with bad afid', tstate) as tc:
            clnt = tstate.ccc()[0]
            section = 'attach-with-bad-afid'
            aname = getconf(tstate.config, section, 'aname', '')
            uname = getconf(tstate.config, section, 'uname', '')
            if clnt.proto > protocol.plain:
                n_uname = getint(tstate.config, section, 'n_uname', 1001)
            else:
                n_uname = None
            try:
                clnt.attach(afid=42, aname=aname, uname=uname, n_uname=n_uname)
            except RemoteError as err:
                tc.succ(err.args[0])
            tc.dcc()
            tc.fail('bad attach afid not rejected')

    try:
        if not tstate.stop:
            # Various Linux tests need gids.  Just get them for everyone.
            tstate.gid = getint(tstate.config, 'client', 'gid', 0)
            more_test_cases(tstate)
    finally:
        tstate.dcc()

    n_tests = tstate.successes + tstate.failures
    print('summary:')
    if tstate.successes:
        print('{0}/{1} tests succeeded'.format(tstate.successes, n_tests))
    if tstate.failures:
        print('{0}/{1} tests failed'.format(tstate.failures, n_tests))
    if tstate.skips:
        print('{0} {1} skipped'.format(tstate.skips,
                                       pluralize(tstate.skips,
                                                 'test', 'tests')))
    if tstate.exceptions:
        print('{0} {1} occurred'.format(tstate.exceptions,
                                       pluralize(tstate.exceptions,
                                                 'exception', 'exceptions')))
    if tstate.stop:
        print('tests stopped early')
    return 1 if tstate.stop or tstate.exceptions or tstate.failures else 0

def more_test_cases(tstate):
    "run cases that can only proceed if connecting works at all"
    with TestCase('attach normally', tstate) as tc:
        tc.ccs()
        tc.succ()
    if tstate.stop:
        return

    # Empty string is not technically illegal.  It's not clear
    # whether it should be accepted or rejected.  However, it
    # used to crash the server entirely, so it's a desirable
    # test case.
    with TestCase('empty string in Twalk request', tstate) as tc:
        clnt = tc.ccs()
        try:
            fid, qid = clnt.lookup(clnt.rootfid, [b''])
        except RemoteError as err:
            tc.succ(err.args[0])
        clnt.clunk(fid)
        tc.succ('note: empty Twalk component name not rejected')

    # Name components may not contain /
    with TestCase('embedded / in lookup component name', tstate) as tc:
        clnt = tc.ccs()
        try:
            fid, qid = clnt.lookup(clnt.rootfid, [b'/'])
            tc.autoclunk(fid)
        except RemoteError as err:
            tc.succ(err.args[0])
        tc.fail('/ in lookup component name not rejected')

    # Proceed from a clean tree.  As a side effect, this also tests
    # either the old style readdir (read() on a directory fid) or
    # the dot-L readdir().
    #
    # The test case will fail if we don't have permission to remove
    # some file(s).
    with TestCase('clean up tree (readdir+remove)', tstate) as tc:
        clnt = tc.ccs()
        fset = clnt.uxreaddir(b'/')
        fset = [i for i in fset if i != '.' and i != '..']
        tc.trace("what's there initially: {0!r}".format(fset))
        try:
            clnt.uxremove(b'/', force=False, recurse=True)
        except RemoteError as err:
            tc.trace('failed to read or clean up tree', level=logging.ERROR)
            tc.trace('this might be a permissions error', level=logging.ERROR)
            tstate.stop = True
            tc.fail(str(err))
        fset = clnt.uxreaddir(b'/')
        fset = [i for i in fset if i != '.' and i != '..']
        tc.trace("what's left after removing everything: {0!r}".format(fset))
        if fset:
            tstate.stop = True
            tc.trace('note: could be a permissions error', level=logging.ERROR)
            tc.fail('/ not empty after removing all: {0!r}'.format(fset))
        tc.succ()
    if tstate.stop:
        return

    # Name supplied to create, mkdir, etc, may not contain /.
    # Note that this test may fail for the wrong reason if /dir
    # itself does not already exist, so first let's make /dir.
    only_dotl = getbool(tstate.config, 'client', 'only_dotl', False)
    with TestCase('mkdir', tstate) as tc:
        clnt = tc.ccs()
        if only_dotl and not clnt.supports(protocol.td.Tmkdir):
            tc.skip('cannot test dot-L mkdir on {0}'.format(clnt.proto))
        try:
            fid, qid = clnt.uxlookup(b'/dir', None)
            tc.autoclunk(fid)
            tstate.stop = True
            tc.fail('found existing /dir after cleaning tree')
        except RemoteError as err:
            # we'll just assume it's "no such file or directory"
            pass
        if only_dotl:
            qid = clnt.mkdir(clnt.rootfid, b'dir', 0o777, tstate.gid)
        else:
            qid, _ = clnt.create(clnt.rootfid, b'dir',
                                 protocol.td.DMDIR | 0o777,
                                 protocol.td.OREAD)
        if qid.type != protocol.td.QTDIR:
            tstate.stop = True
            tc.fail('creating /dir: result is not a directory')
        tc.trace('now attempting to create /dir/sub the wrong way')
        try:
            if only_dotl:
                qid = clnt.mkdir(clnt.rootfid, b'dir/sub', 0o777, tstate.gid)
            else:
                qid, _ = clnt.create(clnt.rootfid, b'dir/sub',
                                     protocol.td.DMDIR | 0o777,
                                     protocol.td.OREAD)
            # it's not clear what happened on the server at this point!
            tc.trace("creating dir/sub (with embedded '/') should have "
                     'failed but did not')
            tstate.stop = True
            fset = clnt.uxreaddir(b'/dir')
            if 'sub' in fset:
                tc.trace('(found our dir/sub detritus)')
                clnt.uxremove(b'dir/sub', force=True)
                fset = clnt.uxreaddir(b'/dir')
                if 'sub' not in fset:
                    tc.trace('(successfully removed our dir/sub detritus)')
                    tstate.stop = False
            tc.fail('created dir/sub as single directory with embedded slash')
        except RemoteError as err:
            # we'll just assume it's the right kind of error
            tc.trace('invalid path dir/sub failed with: %s', str(err))
            tc.succ('embedded slash in mkdir correctly refused')
    if tstate.stop:
        return

    with TestCase('getattr/setattr', tstate) as tc:
        # This test is not really thorough enough, need to test
        # all combinations of settings.  Should also test that
        # old values are restored on failure, although it is not
        # clear how to trigger failures.
        clnt = tc.ccs()
        if not clnt.supports(protocol.td.Tgetattr):
            tc.skip('%s does not support Tgetattr', clnt)
        fid, _, _, _ = clnt.uxopen(b'/dir/file', os.O_CREAT | os.O_RDWR, 0o666,
            gid=tstate.gid)
        tc.autoclunk(fid)
        written = clnt.write(fid, 0, 'bytes\n')
        if written != 6:
            tc.trace('expected to write 6 bytes, actually wrote %d', written,
                     level=logging.WARN)
        attrs = clnt.Tgetattr(fid)
        #tc.trace('getattr: after write, before setattr: got %s', attrs)
        if attrs.size != written:
            tc.fail('getattr: expected size=%d, got size=%d',
                    written, attrs.size)
        # now truncate, set mtime to (3,14), and check result
        set_time_to = p9conn.Timespec(sec=0, nsec=140000000)
        clnt.Tsetattr(fid, size=0, mtime=set_time_to)
        attrs = clnt.Tgetattr(fid)
        #tc.trace('getattr: after setattr: got %s', attrs)
        if attrs.mtime.sec != set_time_to.sec or attrs.size != 0:
            tc.fail('setattr: expected to get back mtime.sec={0}, size=0; '
                    'got mtime.sec={1}, size='
                    '{1}'.format(set_time_to.sec, attrs.mtime.sec, attrs.size))
        # nsec is not as stable but let's check
        if attrs.mtime.nsec != set_time_to.nsec:
            tc.trace('setattr: expected to get back mtime_nsec=%d; '
                     'got %d', set_time_to.nsec, mtime_nsec)
        tc.succ('able to set and see size and mtime')

    # this test should be much later, but we know the current
    # server is broken...
    with TestCase('rename adjusts other fids', tstate) as tc:
        clnt = tc.ccs()
        dirfid, _ = clnt.uxlookup(b'/dir')
        tc.autoclunk(dirfid)
        clnt.uxmkdir(b'd1', 0o777, tstate.gid, startdir=dirfid)
        clnt.uxmkdir(b'd1/sub', 0o777, tstate.gid, startdir=dirfid)
        d1fid, _ = clnt.uxlookup(b'd1', dirfid)
        tc.autoclunk(d1fid)
        subfid, _ = clnt.uxlookup(b'sub', d1fid)
        tc.autoclunk(subfid)
        fid, _, _, _ = clnt.uxopen(b'file', os.O_CREAT | os.O_RDWR,
                                   0o666, startdir=subfid, gid=tstate.gid)
        tc.autoclunk(fid)
        written = clnt.write(fid, 0, 'filedata\n')
        if written != 9:
            tc.trace('expected to write 9 bytes, actually wrote %d', written,
                     level=logging.WARN)
        # Now if we rename /dir/d1 to /dir/d2, the fids for both
        # sub/file and sub itself should still be usable.  This
        # holds for both Trename (Linux only) and Twstat based
        # rename ops.
        #
        # Note that some servers may cache some number of files and/or
        # diretories held open, so we should open many fids to wipe
        # out the cache (XXX notyet).
        if clnt.supports(protocol.td.Trename):
            clnt.rename(d1fid, dirfid, name=b'd2')
        else:
            clnt.wstat(d1fid, name=b'd2')
        try:
            rofid, _, _, _ = clnt.uxopen(b'file', os.O_RDONLY, startdir=subfid)
            clnt.clunk(rofid)
        except RemoteError as err:
            tc.fail('open file in renamed dir/d2/sub: {0}'.format(err))
        tc.succ()

    # Even if xattrwalk is supported by the protocol, it's optional
    # on the server.
    with TestCase('xattrwalk', tstate) as tc:
        clnt = tc.ccs()
        if not clnt.supports(protocol.td.Txattrwalk):
            tc.skip('{0} does not support Txattrwalk'.format(clnt))
        dirfid, _ = clnt.uxlookup(b'/dir')
        tc.autoclunk(dirfid)
        try:
            # need better tests...
            attrfid, size = clnt.xattrwalk(dirfid)
            tc.autoclunk(attrfid)
            data = clnt.read(attrfid, 0, size)
            tc.trace('xattrwalk with no name: data=%r', data)
            tc.succ('xattrwalk size={0} datalen={1}'.format(size, len(data)))
        except RemoteError as err:
            tc.trace('xattrwalk on /dir: {0}'.format(err))
        tc.succ('xattrwalk apparently not implemented')

if __name__ == '__main__':
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit('\nInterrupted')
