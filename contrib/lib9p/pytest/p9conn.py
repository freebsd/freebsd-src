#! /usr/bin/env python

"""
handle plan9 server <-> client connections

(We can act as either server or client.)

This code needs some doctests or other unit tests...
"""

import collections
import errno
import logging
import math
import os
import socket
import stat
import struct
import sys
import threading
import time

import lerrno
import numalloc
import p9err
import pfod
import protocol

# Timespec based timestamps, if present, have
# both seconds and nanoseconds.
Timespec = collections.namedtuple('Timespec', 'sec nsec')

# File attributes from Tgetattr, or given to Tsetattr.
# (move to protocol.py?)  We use pfod here instead of
# namedtuple so that we can create instances with all-None
# fields easily.
Fileattrs = pfod.pfod('Fileattrs',
    'ino mode uid gid nlink rdev size blksize blocks '
    'atime mtime ctime btime gen data_version')

qt2n = protocol.qid_type2name

STD_P9_PORT=564

class P9Error(Exception):
    pass

class RemoteError(P9Error):
    """
    Used when the remote returns an error.  We track the client
    (connection instance), the operation being attempted, the
    message, and an error number and type.  The message may be
    from the Rerror reply, or from converting the errno in a dot-L
    or dot-u Rerror reply.  The error number may be None if the
    type is 'Rerror' rather than 'Rlerror'.  The message may be
    None or empty string if a non-None errno supplies the error
    instead.
    """
    def __init__(self, client, op, msg, etype, errno):
        self.client = str(client)
        self.op = op
        self.msg = msg
        self.etype = etype # 'Rerror' or 'Rlerror'
        self.errno = errno # may be None
        self.message = self._get_message()
        super(RemoteError, self).__init__(self, self.message)

    def __repr__(self):
        return ('{0!r}({1}, {2}, {3}, {4}, '
                '{5})'.format(self.__class__.__name__, self.client, self.op,
                              self.msg, self.errno, self.etype))
    def __str__(self):
        prefix = '{0}: {1}: '.format(self.client, self.op)
        if self.errno: # check for "is not None", or just non-false-y?
            name = {'Rerror': '.u', 'Rlerror': 'Linux'}[self.etype]
            middle = '[{0} error {1}] '.format(name, self.errno)
        else:
            middle = ''
        return '{0}{1}{2}'.format(prefix, middle, self.message)

    def is_ENOTSUP(self):
        if self.etype == 'Rlerror':
            return self.errno == lerrno.EOPNOTSUPP
        return self.errno == errno.EOPNOTSUPP

    def _get_message(self):
        "get message based on self.msg or self.errno"
        if self.errno is not None:
            return {
                'Rlerror': p9err.dotl_strerror,
                'Rerror' : p9err.dotu_strerror,
            }[self.etype](self.errno)
        return self.msg

class LocalError(P9Error):
    pass

class TEError(LocalError):
    pass

class P9SockIO(object):
    """
    Common base for server and client, handle send and
    receive to communications channel.  Note that this
    need not set up the channel initially, only the logger.
    The channel is typically connected later.  However, you
    can provide one initially.
    """
    def __init__(self, logger, name=None, server=None, port=STD_P9_PORT):
        self.logger = logger
        self.channel = None
        self.name = name
        self.maxio = None
        self.size_coder = struct.Struct('<I')
        if server is not None:
            self.connect(server, port)
        self.max_payload = 2**32 - self.size_coder.size

    def __str__(self):
        if self.name:
            return self.name
        return repr(self)

    def get_recommended_maxio(self):
        "suggest a max I/O size, for when self.maxio is 0 / unset"
        return 16 * 4096

    def min_maxio(self):
        "return a minimum size below which we refuse to work"
        return self.size_coder.size + 100

    def connect(self, server, port=STD_P9_PORT):
        """
        Connect to given server name / IP address.

        If self.name was none, sets self.name to ip:port on success.
        """
        if self.is_connected():
            raise LocalError('already connected')
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
        sock.connect((server, port))
        if self.name is None:
            if port == STD_P9_PORT:
                name = server
            else:
                name = '{0}:{1}'.format(server, port)
        else:
            name = None
        self.declare_connected(sock, name, None)

    def is_connected(self):
        "predicate: are we connected?"
        return self.channel != None

    def declare_connected(self, chan, name, maxio):
        """
        Now available for normal protocol (size-prefixed) I/O.
        
        Replaces chan and name and adjusts maxio, if those
        parameters are not None.
        """
        if maxio:
            minio = self.min_maxio()
            if maxio < minio:
                raise LocalError('maxio={0} < minimum {1}'.format(maxio, minio))
        if chan is not None:
            self.channel = chan
        if name is not None:
            self.name = name
        if maxio is not None:
            self.maxio = maxio
            self.max_payload = maxio - self.size_coder.size

    def reduce_maxio(self, maxio):
        "Reduce maximum I/O size per other-side request"
        minio = self.min_maxio()
        if maxio < minio:
            raise LocalError('new maxio={0} < minimum {1}'.format(maxio, minio))
        if maxio > self.maxio:
            raise LocalError('new maxio={0} > current {1}'.format(maxio,
                                                                  self.maxio))
        self.maxio = maxio
        self.max_payload = maxio - self.size_coder.size

    def declare_disconnected(self):
        "Declare comm channel dead (note: leaves self.name set!)"
        self.channel = None
        self.maxio = None

    def shutwrite(self):
        "Do a SHUT_WR on the outbound channel - can't send more"
        chan = self.channel
        # we're racing other threads here
        try:
            chan.shutdown(socket.SHUT_WR)
        except (OSError, AttributeError):
            pass

    def shutdown(self):
        "Shut down comm channel"
        if self.channel:
            try:
                self.channel.shutdown(socket.SHUT_RDWR)
            except socket.error:
                pass
            self.channel.close()
            self.declare_disconnected()

    def read(self):
        """
        Try to read a complete packet.

        Returns '' for EOF, as read() usually does.

        If we can't even get the size, this still returns ''.
        If we get a sensible size but are missing some data,
        we can return a short packet.  Since we know if we did
        this, we also return a boolean: True means "really got a
        complete packet."

        Note that '' EOF always returns False: EOF is never a
        complete packet.
        """
        if self.channel is None:
            return b'', False
        size_field = self.xread(self.size_coder.size)
        if len(size_field) < self.size_coder.size:
            if len(size_field) == 0:
                self.logger.log(logging.INFO, '%s: normal EOF', self)
            else:
                self.logger.log(logging.ERROR,
                               '%s: EOF while reading size (got %d bytes)',
                               self, len(size_field))
                # should we raise an error here?
            return b'', False

        size = self.size_coder.unpack(size_field)[0] - self.size_coder.size
        if size <= 0 or size > self.max_payload:
            self.logger.log(logging.ERROR,
                            '%s: incoming size %d is insane '
                            '(max payload is %d)',
                            self, size, self.max_payload)
            # indicate EOF - should we raise an error instead, here?
            return b'', False
        data = self.xread(size)
        return data, len(data) == size

    def xread(self, nbytes):
        """
        Read nbytes bytes, looping if necessary.  Return '' for
        EOF; may return a short count if we get some data, then
        EOF.
        """
        assert nbytes > 0
        # Try to get everything at once (should usually succeed).
        # Return immediately for EOF or got-all-data.
        data = self.channel.recv(nbytes)
        if data == b'' or len(data) == nbytes:
            return data

        # Gather data fragments into an array, then join it all at
        # the end.
        count = len(data)
        data = [data]
        while count < nbytes:
            more = self.channel.recv(nbytes - count)
            if more == b'':
                break
            count += len(more)
            data.append(more)
        return b''.join(data)

    def write(self, data):
        """
        Write all the data, in the usual encoding.  Note that
        the length of the data, including the length of the length
        itself, is already encoded in the first 4 bytes of the
        data.

        Raises IOError if we can't write everything.

        Raises LocalError if len(data) exceeds max_payload.
        """
        size = len(data)
        assert size >= 4
        if size > self.max_payload:
            raise LocalError('data length {0} exceeds '
                             'maximum {1}'.format(size, self.max_payload))
        self.channel.sendall(data)

def _pathcat(prefix, suffix):
    """
    Concatenate paths we are using on the server side.  This is
    basically just prefix + / + suffix, with two complications:

    It's possible we don't have a prefix path, in which case
    we want the suffix without a leading slash.

    It's possible that the prefix is just b'/', in which case we
    want prefix + suffix.
    """
    if prefix:
        if prefix == b'/':  # or prefix.endswith(b'/')?
            return prefix + suffix
        return prefix + b'/' + suffix
    return suffix

class P9Client(P9SockIO):
    """
    Act as client.

    We need the a logger (see logging), a timeout, and a protocol
    version to request.  By default, we will downgrade to a lower
    version if asked.

    If server and port are supplied, they are remembered and become
    the default for .connect() (which is still deferred).

    Note that we keep a table of fid-to-path in self.live_fids,
    but at any time (except while holding the lock) a fid can
    be deleted entirely, and the table entry may just be True
    if we have no path name.  In general, we update the name
    when we can.
    """
    def __init__(self, logger, timeout, version, may_downgrade=True,
                 server=None, port=None):
        super(P9Client, self).__init__(logger)
        self.timeout = timeout
        self.iproto = protocol.p9_version(version)
        self.may_downgrade = may_downgrade
        self.tagalloc = numalloc.NumAlloc(0, 65534)
        self.tagstate = {}
        # The next bit is slighlty dirty: perhaps we should just
        # allocate NOFID out of the 2**32-1 range, so as to avoid
        # "knowing" that it's 2**32-1.
        self.fidalloc = numalloc.NumAlloc(0, protocol.td.NOFID - 1)
        self.live_fids = {}
        self.rootfid = None
        self.rootqid = None
        self.rthread = None
        self.lock = threading.Lock()
        self.new_replies = threading.Condition(self.lock)
        self._monkeywrench = {}
        self._server = server
        self._port = port
        self._unsup = {}

    def get_monkey(self, what):
        "check for a monkey-wrench"
        with self.lock:
            wrench = self._monkeywrench.get(what)
            if wrench is None:
                return None
            if isinstance(wrench, list):
                # repeats wrench[0] times, or forever if that's 0
                ret = wrench[1]
                if wrench[0] > 0:
                    wrench[0] -= 1
                    if wrench[0] == 0:
                        del self._monkeywrench[what]
            else:
                ret = wrench
                del self._monkeywrench[what]
        return ret

    def set_monkey(self, what, how, repeat=None):
        """
        Set a monkey-wrench.  If repeat is not None it is the number of
        times the wrench is applied (0 means forever, or until you call
        set again with how=None).  What is what to monkey-wrench, which
        depends on the op.  How is generally a replacement value.
        """
        if how is None:
            with self.lock:
                try:
                    del self._monkeywrench[what]
                except KeyError:
                    pass
            return
        if repeat is not None:
            how = [repeat, how]
        with self.lock:
            self._monkeywrench[what] = how

    def get_tag(self, for_Tversion=False):
        "get next available tag ID"
        with self.lock:
            if for_Tversion:
                tag = 65535
            else:
                tag = self.tagalloc.alloc()
            if tag is None:
                raise LocalError('all tags in use')
            self.tagstate[tag] = True # ie, in use, still waiting
        return tag

    def set_tag(self, tag, reply):
        "set the reply info for the given tag"
        assert tag >= 0 and tag < 65536
        with self.lock:
            # check whether we're still waiting for the tag
            state = self.tagstate.get(tag)
            if state is True:
                self.tagstate[tag] = reply # i.e., here's the answer
                self.new_replies.notify_all()
                return
            # state must be one of these...
            if state is False:
                # We gave up on this tag.  Reply came anyway.
                self.logger.log(logging.INFO,
                                '%s: got tag %d = %r after timing out on it',
                                self, tag, reply)
                self.retire_tag_locked(tag)
                return
            if state is None:
                # We got a tag back from the server that was not
                # outstanding!
                self.logger.log(logging.WARNING,
                                '%s: got tag %d = %r when tag %d not in use!',
                                self, tag, reply, tag)
                return
            # We got a second reply before handling the first reply!
            self.logger.log(logging.WARNING,
                            '%s: got tag %d = %r when tag %d = %r!',
                            self, tag, reply, tag, state)
            return

    def retire_tag(self, tag):
        "retire the given tag - only used by the thread that handled the result"
        if tag == 65535:
            return
        assert tag >= 0 and tag < 65535
        with self.lock:
            self.retire_tag_locked(tag)

    def retire_tag_locked(self, tag):
        "retire the given tag while holding self.lock"
        # must check "in tagstate" because we can race
        # with retire_all_tags.
        if tag in self.tagstate:
            del self.tagstate[tag]
            self.tagalloc.free(tag)

    def retire_all_tags(self):
        "retire all tags, after connection drop"
        with self.lock:
            # release all tags in any state (waiting, answered, timedout)
            self.tagalloc.free_multi(self.tagstate.keys())
            self.tagstate = {}
            self.new_replies.notify_all()

    def alloc_fid(self):
        "allocate new fid"
        with self.lock:
            fid = self.fidalloc.alloc()
            self.live_fids[fid] = True
        return fid

    def getpath(self, fid):
        "get path from fid, or return None if no path known, or not valid"
        with self.lock:
            path = self.live_fids.get(fid)
        if path is True:
            path = None
        return path

    def getpathX(self, fid):
        """
        Much like getpath, but return <fid N, unknown path> if necessary.
        If we do have a path, return its repr().
        """
        path = self.getpath(fid)
        if path is None:
            return '<fid {0}, unknown path>'.format(fid)
        return repr(path)

    def setpath(self, fid, path):
        "associate fid with new path (possibly from another fid)"
        with self.lock:
            if isinstance(path, int):
                path = self.live_fids.get(path)
            # path might now be None (not a live fid after all), or
            # True (we have no path name), or potentially even the
            # empty string (invalid for our purposes).  Treat all of
            # those as True, meaning "no known path".
            if not path:
                path = True
            if self.live_fids.get(fid):
                # Existing fid maps to either True or its old path.
                # Set the new path (which may be just a placeholder).
                self.live_fids[fid] = path

    def did_rename(self, fid, ncomp, newdir=None):
        """
        Announce that we renamed using a fid - we'll try to update
        other fids based on this (we can't really do it perfectly).

        NOTE: caller must provide a final-component.
        The caller can supply the new path (and should
        do so if the rename is not based on the retained path
        for the supplied fid, i.e., for rename ops where fid
        can move across directories).  The rules:

         - If newdir is None (default), we use stored path.
         - Otherwise, newdir provides the best approximation
           we have to the path that needs ncomp appended.

        (This is based on the fact that renames happen via Twstat
        or Trename, or Trenameat, which change just one tail component,
        but the path names vary.)
        """
        if ncomp is None:
            return
        opath = self.getpath(fid)
        if newdir is None:
            if opath is None:
                return
            ocomps = opath.split(b'/')
            ncomps = ocomps[0:-1]
        else:
            ocomps = None           # well, none yet anyway
            ncomps = newdir.split(b'/')
        ncomps.append(ncomp)
        if opath is None or opath[0] != '/':
            # We don't have enough information to fix anything else.
            # Just store the new path and return.  We have at least
            # a partial path now, which is no worse than before.
            npath = b'/'.join(ncomps)
            with self.lock:
                if fid in self.live_fids:
                    self.live_fids[fid] = npath
            return
        if ocomps is None:
            ocomps = opath.split(b'/')
        olen = len(ocomps)
        ofinal = ocomps[olen - 1]
        # Old paths is full path.  Find any other fids that start
        # with some or all the components in ocomps.  Note that if
        # we renamed /one/two/three to /four/five this winds up
        # renaming files /one/a to /four/a, /one/two/b to /four/five/b,
        # and so on.
        with self.lock:
            for fid2, path2 in self.live_fids.iteritems():
                # Skip fids without byte-string paths
                if not isinstance(path2, bytes):
                    continue
                # Before splitting (which is a bit expensive), try
                # a straightforward prefix match.  This might give
                # some false hits, e.g., prefix /one/two/threepenny
                # starts with /one/two/three, but it quickly eliminates
                # /raz/baz/mataz and the like.
                if not path2.startswith(opath):
                    continue
                # Split up the path, and use that to make sure that
                # the final component is a full match.
                parts2 = path2.split(b'/')
                if parts2[olen - 1] != ofinal:
                    continue
                # OK, path2 starts with the old (renamed) sequence.
                # Replace the old components with the new ones.
                # This updates the renamed fid when we come across
                # it!  It also handles a change in the number of
                # components, thanks to Python's slice assignment.
                parts2[0:olen] = ncomps
                self.live_fids[fid2] = b'/'.join(parts2)

    def retire_fid(self, fid):
        "retire one fid"
        with self.lock:
            self.fidalloc.free(fid)
            del self.live_fids[fid]

    def retire_all_fids(self):
        "return live fids to pool"
        # this is useful for debugging fid leaks:
        #for fid in self.live_fids:
        #    print 'retiring', fid, self.getpathX(fid)
        with self.lock:
            self.fidalloc.free_multi(self.live_fids.keys())
            self.live_fids = {}

    def read_responses(self):
        "Read responses.  This gets spun off as a thread."
        while self.is_connected():
            pkt, is_full = super(P9Client, self).read()
            if pkt == b'':
                self.shutwrite()
                self.retire_all_tags()
                return
            if not is_full:
                self.logger.log(logging.WARNING, '%s: got short packet', self)
            try:
                # We have one special case: if we're not yet connected
                # with a version, we must unpack *as if* it's a plain
                # 9P2000 response.
                if self.have_version:
                    resp = self.proto.unpack(pkt)
                else:
                    resp = protocol.plain.unpack(pkt)
            except protocol.SequenceError as err:
                self.logger.log(logging.ERROR, '%s: bad response: %s',
                                self, err)
                try:
                    resp = self.proto.unpack(pkt, noerror=True)
                except protocol.SequenceError:
                    header = self.proto.unpack_header(pkt, noerror=True)
                    self.logger.log(logging.ERROR,
                                    '%s: (not even raw-decodable)', self)
                    self.logger.log(logging.ERROR,
                                    '%s: header decode produced %r',
                                    self, header)
                else:
                    self.logger.log(logging.ERROR,
                                    '%s: raw decode produced %r',
                                    self, resp)
                # after this kind of problem, probably need to
                # shut down, but let's leave that out for a bit
            else:
                # NB: all protocol responses have a "tag",
                # so resp['tag'] always exists.
                self.logger.log(logging.DEBUG, "read_resp: tag %d resp %r", resp.tag, resp)
                self.set_tag(resp.tag, resp)

    def wait_for(self, tag):
        """
        Wait for a response to the given tag.  Return the response,
        releasing the tag.  If self.timeout is not None, wait at most
        that long (and release the tag even if there's no reply), else
        wait forever.

        If this returns None, either the tag was bad initially, or
        a timeout occurred, or the connection got shut down.
        """
        self.logger.log(logging.DEBUG, "wait_for: tag %d", tag)
        if self.timeout is None:
            deadline = None
        else:
            deadline = time.time() + self.timeout
        with self.lock:
            while True:
                # tagstate is True (waiting) or False (timedout) or
                # a valid response, or None if we've reset the tag
                # states (retire_all_tags, after connection drop).
                resp = self.tagstate.get(tag, None)
                if resp is None:
                    # out of sync, exit loop
                    break
                if resp is True:
                    # still waiting for a response - wait some more
                    self.new_replies.wait(self.timeout)
                    if deadline and time.time() > deadline:
                        # Halt the waiting, but go around once more.
                        # Note we may have killed the tag by now though.
                        if tag in self.tagstate:
                            self.tagstate[tag] = False
                    continue
                # resp is either False (timeout) or a reply.
                # If resp is False, change it to None; the tag
                # is now dead until we get a reply (then we
                # just toss the reply).
                # Otherwise, we're done with the tag: free it.
                # In either case, stop now.
                if resp is False:
                    resp = None
                else:
                    self.tagalloc.free(tag)
                    del self.tagstate[tag]
                break
        return resp

    def badresp(self, req, resp):
        """
        Complain that a response was not something expected.
        """
        if resp is None:
            self.shutdown()
            raise TEError('{0}: {1}: timeout or EOF'.format(self, req))
        if isinstance(resp, protocol.rrd.Rlerror):
            raise RemoteError(self, req, None, 'Rlerror', resp.ecode)
        if isinstance(resp, protocol.rrd.Rerror):
            if resp.errnum is None:
                raise RemoteError(self, req, resp.errstr, 'Rerror', None)
            raise RemoteError(self, req, None, 'Rerror', resp.errnum)
        raise LocalError('{0}: {1} got response {2!r}'.format(self, req, resp))

    def supports(self, req_code):
        """
        Test self.proto.support(req_code) unless we've recorded that
        while the protocol supports it, the client does not.
        """
        return req_code not in self._unsup and self.proto.supports(req_code)

    def supports_all(self, *req_codes):
        "basically just all(supports(...))"
        return all(self.supports(code) for code in req_codes)

    def unsupported(self, req_code):
        """
        Record an ENOTSUP (RemoteError was ENOTSUP) for a request.
        Must be called from the op, this does not happen automatically.
        (It's just an optimization.)
        """
        self._unsup[req_code] = True

    def connect(self, server=None, port=None):
        """
        Connect to given server/port pair.

        The server and port are remembered.  If given as None,
        the last remembered values are used.  The initial
        remembered values are from the creation of this client
        instance.

        New values are only remembered here on a *successful*
        connect, however.
        """
        if server is None:
            server = self._server
            if server is None:
                raise LocalError('connect: no server specified and no default')
        if port is None:
            port = self._port
            if port is None:
                port = STD_P9_PORT
        self.name = None            # wipe out previous name, if any
        super(P9Client, self).connect(server, port)
        maxio = self.get_recommended_maxio()
        self.declare_connected(None, None, maxio)
        self.proto = self.iproto    # revert to initial protocol
        self.have_version = False
        self.rthread = threading.Thread(target=self.read_responses)
        self.rthread.start()
        tag = self.get_tag(for_Tversion=True)
        req = protocol.rrd.Tversion(tag=tag, msize=maxio,
                                    version=self.get_monkey('version'))
        super(P9Client, self).write(self.proto.pack_from(req))
        resp = self.wait_for(tag)
        if not isinstance(resp, protocol.rrd.Rversion):
            self.shutdown()
            if isinstance(resp, protocol.rrd.Rerror):
                version = req.version or self.proto.get_version()
                # for python3, we need to convert version to string
                if not isinstance(version, str):
                    version = version.decode('utf-8', 'surrogateescape')
                raise RemoteError(self, 'version ' + version,
                                  resp.errstr, 'Rerror', None)
            self.badresp('version', resp)
        their_maxio = resp.msize
        try:
            self.reduce_maxio(their_maxio)
        except LocalError as err:
            raise LocalError('{0}: sent maxio={1}, they tried {2}: '
                             '{3}'.format(self, maxio, their_maxio,
                                          err.args[0]))
        if resp.version != self.proto.get_version():
            if not self.may_downgrade:
                self.shutdown()
                raise LocalError('{0}: they only support '
                                 'version {1!r}'.format(self, resp.version))
            # raises LocalError if the version is bad
            # (should we wrap it with a connect-to-{0} msg?)
            self.proto = self.proto.downgrade_to(resp.version)
        self._server = server
        self._port = port
        self.have_version = True

    def attach(self, afid, uname, aname, n_uname):
        """
        Attach.

        Currently we don't know how to do authentication,
        but we'll pass any provided afid through.
        """
        if afid is None:
            afid = protocol.td.NOFID
        if uname is None:
            uname = ''
        if aname is None:
            aname = ''
        if n_uname is None:
            n_uname = protocol.td.NONUNAME
        tag = self.get_tag()
        fid = self.alloc_fid()
        pkt = self.proto.Tattach(tag=tag, fid=fid, afid=afid,
                                 uname=uname, aname=aname,
                                 n_uname=n_uname)
        super(P9Client, self).write(pkt)
        resp = self.wait_for(tag)
        if not isinstance(resp, protocol.rrd.Rattach):
            self.retire_fid(fid)
            self.badresp('attach', resp)
        # probably should check resp.qid
        self.rootfid = fid
        self.rootqid = resp.qid
        self.setpath(fid, b'/')

    def shutdown(self):
        "disconnect from server"
        if self.rootfid is not None:
            self.clunk(self.rootfid, ignore_error=True)
        self.retire_all_tags()
        self.retire_all_fids()
        self.rootfid = None
        self.rootqid = None
        super(P9Client, self).shutdown()
        if self.rthread:
            self.rthread.join()
            self.rthread = None

    def dupfid(self, fid):
        """
        Copy existing fid to a new fid.
        """
        tag = self.get_tag()
        newfid = self.alloc_fid()
        pkt = self.proto.Twalk(tag=tag, fid=fid, newfid=newfid, nwname=0,
                               wname=[])
        super(P9Client, self).write(pkt)
        resp = self.wait_for(tag)
        if not isinstance(resp, protocol.rrd.Rwalk):
            self.retire_fid(newfid)
            self.badresp('walk {0}'.format(self.getpathX(fid)), resp)
        # Copy path too
        self.setpath(newfid, fid)
        return newfid

    def lookup(self, fid, components):
        """
        Do Twalk.  Caller must provide a starting fid, which should
        be rootfid to look up from '/' - we do not do / vs . here.
        Caller must also provide a component-ized path (on purpose,
        so that caller can provide invalid components like '' or '/').
        The components must be byte-strings as well, for the same
        reason.

        We do allocate the new fid ourselves here, though.

        There's no logic here to split up long walks (yet?).
        """
        # these are too easy to screw up, so check
        if self.rootfid is None:
            raise LocalError('{0}: not attached'.format(self))
        if (isinstance(components, (str, bytes) or
            not all(isinstance(i, bytes) for i in components))):
            raise LocalError('{0}: lookup: invalid '
                             'components {1!r}'.format(self, components))
        tag = self.get_tag()
        newfid = self.alloc_fid()
        startpath = self.getpath(fid)
        pkt = self.proto.Twalk(tag=tag, fid=fid, newfid=newfid,
                               nwname=len(components), wname=components)
        super(P9Client, self).write(pkt)
        resp = self.wait_for(tag)
        if not isinstance(resp, protocol.rrd.Rwalk):
            self.retire_fid(newfid)
            self.badresp('walk {0} in '
                         '{1}'.format(components, self.getpathX(fid)),
                         resp)
        # Just because we got Rwalk does not mean we got ALL the
        # way down the path.  Raise OSError(ENOENT) if we're short.
        if resp.nwqid > len(components):
            # ??? this should be impossible. Local error?  Remote error?
            # OS Error?
            self.clunk(newfid, ignore_error=True)
            raise LocalError('{0}: walk {1} in {2} returned {3} '
                             'items'.format(self, components,
                                            self.getpathX(fid), resp.nwqid))
        if resp.nwqid < len(components):
            self.clunk(newfid, ignore_error=True)
            # Looking up a/b/c and got just a/b, c is what's missing.
            # Looking up a/b/c and got just a, b is what's missing.
            missing = components[resp.nwqid]
            within = _pathcat(startpath, b'/'.join(components[:resp.nwqid]))
            raise OSError(errno.ENOENT,
                          '{0}: {1} in {2}'.format(os.strerror(errno.ENOENT),
                                                   missing, within))
        self.setpath(newfid, _pathcat(startpath, b'/'.join(components)))
        return newfid, resp.wqid

    def lookup_last(self, fid, components):
        """
        Like lookup, but return only the last component's qid.
        As a special case, if components is an empty list, we
        handle that.
        """
        rfid, wqid = self.lookup(fid, components)
        if len(wqid):
            return rfid, wqid[-1]
        if fid == self.rootfid:         # usually true, if we get here at all
            return rfid, self.rootqid
        tag = self.get_tag()
        pkt = self.proto.Tstat(tag=tag, fid=rfid)
        super(P9Client, self).write(pkt)
        resp = self.wait_for(tag)
        if not isinstance(resp, protocol.rrd.Rstat):
            self.badresp('stat {0}'.format(self.getpathX(fid)), resp)
        statval = self.proto.unpack_wirestat(resp.data)
        return rfid, statval.qid

    def clunk(self, fid, ignore_error=False):
        "issue clunk(fid)"
        tag = self.get_tag()
        pkt = self.proto.Tclunk(tag=tag, fid=fid)
        super(P9Client, self).write(pkt)
        resp = self.wait_for(tag)
        if not isinstance(resp, protocol.rrd.Rclunk):
            if ignore_error:
                return
            self.badresp('clunk {0}'.format(self.getpathX(fid)), resp)
        self.retire_fid(fid)

    def remove(self, fid, ignore_error=False):
        "issue remove (old style), which also clunks fid"
        tag = self.get_tag()
        pkt = self.proto.Tremove(tag=tag, fid=fid)
        super(P9Client, self).write(pkt)
        resp = self.wait_for(tag)
        if not isinstance(resp, protocol.rrd.Rremove):
            if ignore_error:
                # remove failed: still need to clunk the fid
                self.clunk(fid, True)
                return
            self.badresp('remove {0}'.format(self.getpathX(fid)), resp)
        self.retire_fid(fid)

    def create(self, fid, name, perm, mode, filetype=None, extension=b''):
        """
        Issue create op (note that this may be mkdir, symlink, etc).
        fid is the directory in which the create happens, and for
        regular files, it becomes, on success, a fid referring to
        the now-open file.  perm is, e.g., 0644, 0755, etc.,
        optionally with additional high bits.  mode is a mode
        byte (e.g., protocol.td.ORDWR, or OWRONLY|OTRUNC, etc.).

        As a service to callers, we take two optional arguments
        specifying the file type ('dir', 'symlink', 'device',
        'fifo', or 'socket') and additional info if needed.
        The additional info for a symlink is the target of the
        link (a byte string), and the additional info for a device
        is a byte string with "b <major> <minor>" or "c <major> <minor>".

        Otherwise, callers can leave filetype=None and encode the bits
        into the mode (caller must still provide extension if needed).

        We do NOT check whether the extension matches extra DM bits,
        or that there's only one DM bit set, or whatever, since this
        is a testing setup.
        """
        tag = self.get_tag()
        if filetype is not None:
            perm |= {
                'dir': protocol.td.DMDIR,
                'symlink': protocol.td.DMSYMLINK,
                'device': protocol.td.DMDEVICE,
                'fifo': protocol.td.DMNAMEDPIPE,
                'socket': protocol.td.DMSOCKET,
            }[filetype]
        pkt = self.proto.Tcreate(tag=tag, fid=fid, name=name,
            perm=perm, mode=mode, extension=extension)
        super(P9Client, self).write(pkt)
        resp = self.wait_for(tag)
        if not isinstance(resp, protocol.rrd.Rcreate):
            self.badresp('create {0} in {1}'.format(name, self.getpathX(fid)),
                         resp)
        if resp.qid.type == protocol.td.QTFILE:
            # Creating a regular file opens the file,
            # thus changing the fid's path.
            self.setpath(fid, _pathcat(self.getpath(fid), name))
        return resp.qid, resp.iounit

    def open(self, fid, mode):
        "use Topen to open file or directory fid (mode is 1 byte)"
        tag = self.get_tag()
        pkt = self.proto.Topen(tag=tag, fid=fid, mode=mode)
        super(P9Client, self).write(pkt)
        resp = self.wait_for(tag)
        if not isinstance(resp, protocol.rrd.Ropen):
            self.badresp('open {0}'.format(self.getpathX(fid)), resp)
        return resp.qid, resp.iounit

    def lopen(self, fid, flags):
        "use Tlopen to open file or directory fid (flags from L_O_*)"
        tag = self.get_tag()
        pkt = self.proto.Tlopen(tag=tag, fid=fid, flags=flags)
        super(P9Client, self).write(pkt)
        resp = self.wait_for(tag)
        if not isinstance(resp, protocol.rrd.Rlopen):
            self.badresp('lopen {0}'.format(self.getpathX(fid)), resp)
        return resp.qid, resp.iounit

    def read(self, fid, offset, count):
        "read (up to) count bytes from offset, given open fid"
        tag = self.get_tag()
        pkt = self.proto.Tread(tag=tag, fid=fid, offset=offset, count=count)
        super(P9Client, self).write(pkt)
        resp = self.wait_for(tag)
        if not isinstance(resp, protocol.rrd.Rread):
            self.badresp('read {0} bytes at offset {1} in '
                         '{2}'.format(count, offset, self.getpathX(fid)),
                         resp)
        return resp.data

    def write(self, fid, offset, data):
        "write (up to) count bytes to offset, given open fid"
        tag = self.get_tag()
        pkt = self.proto.Twrite(tag=tag, fid=fid, offset=offset,
                                count=len(data), data=data)
        super(P9Client, self).write(pkt)
        resp = self.wait_for(tag)
        if not isinstance(resp, protocol.rrd.Rwrite):
            self.badresp('write {0} bytes at offset {1} in '
                         '{2}'.format(len(data), offset, self.getpathX(fid)),
                         resp)
        return resp.count

    # Caller may
    #  - pass an actual stat object, or
    #  - pass in all the individual to-set items by keyword, or
    #  - mix and match a bit: get an existing stat, then use
    #    keywords to override fields.
    # We convert "None"s to the internal "do not change" values,
    # and for diagnostic purposes, can turn "do not change" back
    # to None at the end, too.
    def wstat(self, fid, statobj=None, **kwargs):
        if statobj is None:
            statobj = protocol.td.stat()
        else:
            statobj = statobj._copy()
        # Fields in stat that you can't send as a wstat: the
        # type and qid are informative.  Similarly, the
        # 'extension' is an input when creating a file but
        # read-only when stat-ing.
        #
        # It's not clear what it means to set dev, but we'll leave
        # it in as an optional parameter here.  fs/backend.c just
        # errors out on an attempt to change it.
        if self.proto == protocol.plain:
            forbid = ('type', 'qid', 'extension',
                      'n_uid', 'n_gid', 'n_muid')
        else:
            forbid = ('type', 'qid', 'extension')
        nochange = {
            'type': 0,
            'qid': protocol.td.qid(0, 0, 0),
            'dev': 2**32 - 1,
            'mode': 2**32 - 1,
            'atime': 2**32 - 1,
            'mtime': 2**32 - 1,
            'length': 2**64 - 1,
            'name': b'',
            'uid': b'',
            'gid': b'',
            'muid': b'',
            'extension': b'',
            'n_uid': 2**32 - 1,
            'n_gid': 2**32 - 1,
            'n_muid': 2**32 - 1,
        }
        for field in statobj._fields:
            if field in kwargs:
                if field in forbid:
                    raise ValueError('cannot wstat a stat.{0}'.format(field))
                statobj[field] = kwargs.pop(field)
            else:
                if field in forbid or statobj[field] is None:
                    statobj[field] = nochange[field]
        if kwargs:
            raise TypeError('wstat() got an unexpected keyword argument '
                            '{0!r}'.format(kwargs.popitem()))

        data = self.proto.pack_wirestat(statobj)
        tag = self.get_tag()
        pkt = self.proto.Twstat(tag=tag, fid=fid, data=data)
        super(P9Client, self).write(pkt)
        resp = self.wait_for(tag)
        if not isinstance(resp, protocol.rrd.Rwstat):
            # For error viewing, switch all the do-not-change
            # and can't-change fields to None.
            statobj.qid = None
            for field in statobj._fields:
                if field in forbid:
                    statobj[field] = None
                elif field in nochange and statobj[field] == nochange[field]:
                    statobj[field] = None
            self.badresp('wstat {0}={1}'.format(self.getpathX(fid), statobj),
                         resp)
        # wstat worked - change path names if needed
        if statobj.name != b'':
            self.did_rename(fid, statobj.name)

    def readdir(self, fid, offset, count):
        "read (up to) count bytes of dir data from offset, given open fid"
        tag = self.get_tag()
        pkt = self.proto.Treaddir(tag=tag, fid=fid, offset=offset, count=count)
        super(P9Client, self).write(pkt)
        resp = self.wait_for(tag)
        if not isinstance(resp, protocol.rrd.Rreaddir):
            self.badresp('readdir {0} bytes at offset {1} in '
                         '{2}'.format(count, offset, self.getpathX(fid)),
                         resp)
        return resp.data

    def rename(self, fid, dfid, name):
        "invoke Trename: rename file <fid> to <dfid>/name"
        tag = self.get_tag()
        pkt = self.proto.Trename(tag=tag, fid=fid, dfid=dfid, name=name)
        super(P9Client, self).write(pkt)
        resp = self.wait_for(tag)
        if not isinstance(resp, protocol.rrd.Rrename):
            self.badresp('rename {0} to {2} in '
                         '{1}'.format(self.getpathX(fid),
                                      self.getpathX(dfid), name),
                         resp)
        self.did_rename(fid, name, self.getpath(dfid))

    def renameat(self, olddirfid, oldname, newdirfid, newname):
        "invoke Trenameat: rename <olddirfid>/oldname to <newdirfid>/newname"
        tag = self.get_tag()
        pkt = self.proto.Trenameat(tag=tag,
                                   olddirfid=olddirfid, oldname=oldname,
                                   newdirfid=newdirfid, newname=newname)
        super(P9Client, self).write(pkt)
        resp = self.wait_for(tag)
        if not isinstance(resp, protocol.rrd.Rrenameat):
            self.badresp('rename {1} in {0} to {3} in '
                         '{2}'.format(oldname, self.getpathX(olddirfid),
                                      newname, self.getpathX(newdirdfid)),
                         resp)
        # There's no renamed *fid*, just a renamed file!  So no
        # call to self.did_rename().

    def unlinkat(self, dirfd, name, flags):
        "invoke Tunlinkat - flags should be 0 or protocol.td.AT_REMOVEDIR"
        tag = self.get_tag()
        pkt = self.proto.Tunlinkat(tag=tag, dirfd=dirfd,
                                   name=name, flags=flags)
        super(P9Client, self).write(pkt)
        resp = self.wait_for(tag)
        if not isinstance(resp, protocol.rrd.Runlinkat):
            self.badresp('unlinkat {0} in '
                         '{1}'.format(name, self.getpathX(dirfd)), resp)

    def decode_stat_objects(self, bstring, noerror=False):
        """
        Read on a directory returns an array of stat objects.
        Note that for .u these encode extra data.

        It's possible for this to produce a SequenceError, if
        the data are incorrect, unless you pass noerror=True.
        """
        objlist = []
        offset = 0
        while offset < len(bstring):
            obj, offset = self.proto.unpack_wirestat(bstring, offset, noerror)
            objlist.append(obj)
        return objlist

    def decode_readdir_dirents(self, bstring, noerror=False):
        """
        Readdir on a directory returns an array of dirent objects.

        It's possible for this to produce a SequenceError, if
        the data are incorrect, unless you pass noerror=True.
        """
        objlist = []
        offset = 0
        while offset < len(bstring):
            obj, offset = self.proto.unpack_dirent(bstring, offset, noerror)
            objlist.append(obj)
        return objlist

    def lcreate(self, fid, name, lflags, mode, gid):
        "issue lcreate (.L)"
        tag = self.get_tag()
        pkt = self.proto.Tlcreate(tag=tag, fid=fid, name=name,
                                  flags=lflags, mode=mode, gid=gid)
        super(P9Client, self).write(pkt)
        resp = self.wait_for(tag)
        if not isinstance(resp, protocol.rrd.Rlcreate):
            self.badresp('create {0} in '
                         '{1}'.format(name, self.getpathX(fid)), resp)
        # Creating a file opens the file,
        # thus changing the fid's path.
        self.setpath(fid, _pathcat(self.getpath(fid), name))
        return resp.qid, resp.iounit

    def mkdir(self, dfid, name, mode, gid):
        "issue mkdir (.L)"
        tag = self.get_tag()
        pkt = self.proto.Tmkdir(tag=tag, dfid=dfid, name=name,
                                mode=mode, gid=gid)
        super(P9Client, self).write(pkt)
        resp = self.wait_for(tag)
        if not isinstance(resp, protocol.rrd.Rmkdir):
            self.badresp('mkdir {0} in '
                         '{1}'.format(name, self.getpathX(dfid)), resp)
        return resp.qid

    # We don't call this getattr(), for the obvious reason.
    def Tgetattr(self, fid, request_mask=protocol.td.GETATTR_ALL):
        "issue Tgetattr.L - get what you ask for, or everything by default"
        tag = self.get_tag()
        pkt = self.proto.Tgetattr(tag=tag, fid=fid, request_mask=request_mask)
        super(P9Client, self).write(pkt)
        resp = self.wait_for(tag)
        if not isinstance(resp, protocol.rrd.Rgetattr):
            self.badresp('Tgetattr {0} of '
                         '{1}'.format(request_mask, self.getpathX(fid)), resp)
        attrs = Fileattrs()
        # Handle the simplest valid-bit tests:
        for name in ('mode', 'nlink', 'uid', 'gid', 'rdev',
                     'size', 'blocks', 'gen', 'data_version'):
            bit = getattr(protocol.td, 'GETATTR_' + name.upper())
            if resp.valid & bit:
                attrs[name] = resp[name]
        # Handle the timestamps, which are timespec pairs
        for name in ('atime', 'mtime', 'ctime', 'btime'):
            bit = getattr(protocol.td, 'GETATTR_' + name.upper())
            if resp.valid & bit:
                attrs[name] = Timespec(sec=resp[name + '_sec'],
                                       nsec=resp[name + '_nsec'])
        # There is no control bit for blksize; qemu and Linux always
        # provide one.
        attrs.blksize = resp.blksize
        # Handle ino, which comes out of qid.path
        if resp.valid & protocol.td.GETATTR_INO:
            attrs.ino = resp.qid.path
        return attrs

    # We don't call this setattr(), for the obvious reason.
    # See wstat for usage.  Note that time fields can be set
    # with either second or nanosecond resolutions, and some
    # can be set without supplying an actual timestamp, so
    # this is all pretty ad-hoc.
    #
    # There's also one keyword-only argument, ctime=<anything>,
    # which means "set SETATTR_CTIME".  This has the same effect
    # as supplying valid=protocol.td.SETATTR_CTIME.
    def Tsetattr(self, fid, valid=0, attrs=None, **kwargs):
        if attrs is None:
            attrs = Fileattrs()
        else:
            attrs = attrs._copy()

        # Start with an empty (all-zero) Tsetattr instance.  We
        # don't really need to zero out tag and fid, but it doesn't
        # hurt.  Note that if caller says, e.g., valid=SETATTR_SIZE
        # but does not supply an incoming size (via "attrs" or a size=
        # argument), we'll ask to set that field to 0.
        attrobj = protocol.rrd.Tsetattr()
        for field in attrobj._fields:
            attrobj[field] = 0

        # In this case, forbid means "only as kwargs": these values
        # in an incoming attrs object are merely ignored.
        forbid = ('ino', 'nlink', 'rdev', 'blksize', 'blocks', 'btime',
                  'gen', 'data_version')
        for field in attrs._fields:
            if field in kwargs:
                if field in forbid:
                    raise ValueError('cannot Tsetattr {0}'.format(field))
                attrs[field] = kwargs.pop(field)
            elif attrs[field] is None:
                continue
            # OK, we're setting this attribute.  Many are just
            # numeric - if that's the case, we're good, set the
            # field and the appropriate bit.
            bitname = 'SETATTR_' + field.upper()
            bit = getattr(protocol.td, bitname)
            if field in ('mode', 'uid', 'gid', 'size'):
                valid |= bit
                attrobj[field] = attrs[field]
                continue
            # Timestamps are special:  The value may be given as
            # an integer (seconds), or as a float (we convert to
            # (we convert to sec+nsec), or as a timespec (sec+nsec).
            # If specified as 0, we mean "we are not providing the
            # actual time, use the server's time."
            #
            # The ctime field's value, if any, is *ignored*.
            if field in ('atime', 'mtime'):
                value = attrs[field]
                if hasattr(value, '__len__'):
                    if len(value) != 2:
                        raise ValueError('invalid {0}={1!r}'.format(field,
                                                                    value))
                    sec = value[0]
                    nsec = value[1]
                else:
                    sec = value
                    if isinstance(sec, float):
                        nsec, sec = math.modf(sec)
                        nsec = int(round(nsec * 1000000000))
                    else:
                        nsec = 0
                valid |= bit
                attrobj[field + '_sec'] = sec
                attrobj[field + '_nsec'] = nsec
                if sec != 0 or nsec != 0:
                    # Add SETATTR_ATIME_SET or SETATTR_MTIME_SET
                    # as appropriate, to tell the server to *this
                    # specific* time, instead of just "server now".
                    bit = getattr(protocol.td, bitname + '_SET')
                    valid |= bit
        if 'ctime' in kwargs:
            kwargs.pop('ctime')
            valid |= protocol.td.SETATTR_CTIME
        if kwargs:
            raise TypeError('Tsetattr() got an unexpected keyword argument '
                            '{0!r}'.format(kwargs.popitem()))

        tag = self.get_tag()
        attrobj.valid = valid
        attrobj.tag = tag
        attrobj.fid = fid
        pkt = self.proto.pack(attrobj)
        super(P9Client, self).write(pkt)
        resp = self.wait_for(tag)
        if not isinstance(resp, protocol.rrd.Rsetattr):
            self.badresp('Tsetattr {0} {1} of '
                         '{2}'.format(valid, attrs, self.getpathX(fid)), resp)

    def xattrwalk(self, fid, name=None):
        "walk one name or all names: caller should read() the returned fid"
        tag = self.get_tag()
        newfid = self.alloc_fid()
        pkt = self.proto.Txattrwalk(tag=tag, fid=fid, newfid=newfid,
                                    name=name or '')
        super(P9Client, self).write(pkt)
        resp = self.wait_for(tag)
        if not isinstance(resp, protocol.rrd.Rxattrwalk):
            self.retire_fid(newfid)
            self.badresp('Txattrwalk {0} of '
                         '{1}'.format(name, self.getpathX(fid)), resp)
        if name:
            self.setpath(newfid, 'xattr:' + name)
        else:
            self.setpath(newfid, 'xattr')
        return newfid, resp.size

    def _pathsplit(self, path, startdir, allow_empty=False):
        "common code for uxlookup and uxopen"
        if self.rootfid is None:
            raise LocalError('{0}: not attached'.format(self))
        if path.startswith(b'/') or startdir is None:
            startdir = self.rootfid
        components = [i for i in path.split(b'/') if i != b'']
        if len(components) == 0 and not allow_empty:
            raise LocalError('{0}: {1!r}: empty path'.format(self, path))
        return components, startdir

    def uxlookup(self, path, startdir=None):
        """
        Unix-style lookup.  That is, lookup('/foo/bar') or
        lookup('foo/bar').  If startdir is not None and the
        path does not start with '/' we look up from there.
        """
        components, startdir = self._pathsplit(path, startdir, allow_empty=True)
        return self.lookup_last(startdir, components)

    def uxopen(self, path, oflags=0, perm=None, gid=None,
               startdir=None, filetype=None):
        """
        Unix-style open()-with-option-to-create, or mkdir().
        oflags is 0/1/2 with optional os.O_CREAT, perm defaults
        to 0o666 (files) or 0o777 (directories).  If we use
        a Linux create or mkdir op, we will need a gid, but it's
        not required if you are opening an existing file.

        Adds a final boolean value for "did we actually create".
        Raises OSError if you ask for a directory but it's a file,
        or vice versa.  (??? reconsider this later)

        Note that this does not handle other file types, only
        directories.
        """
        needtype = {
            'dir': protocol.td.QTDIR,
            None: protocol.td.QTFILE,
        }[filetype]
        omode_byte = oflags & 3 # cheating
        # allow looking up /, but not creating /
        allow_empty = (oflags & os.O_CREAT) == 0
        components, startdir = self._pathsplit(path, startdir,
                                               allow_empty=allow_empty)
        if not (oflags & os.O_CREAT):
            # Not creating, i.e., just look up and open existing file/dir.
            fid, qid = self.lookup_last(startdir, components)
            # If we got this far, use Topen on the fid; we did not
            # create the file.
            return self._uxopen2(path, needtype, fid, qid, omode_byte, False)

        # Only used if using dot-L, but make sure it's always provided
        # since this is generic.
        if gid is None:
            raise ValueError('gid is required when creating file or dir')

        if len(components) > 1:
            # Look up all but last component; this part must succeed.
            fid, _ = self.lookup(startdir, components[:-1])

            # Now proceed with the final component, using fid
            # as the start dir.  Remember to clunk it!
            startdir = fid
            clunk_startdir = True
            components = components[-1:]
        else:
            # Use startdir as the start dir, and get a new fid.
            # Do not clunk startdir!
            clunk_startdir = False
            fid = self.alloc_fid()

        # Now look up the (single) component.  If this fails,
        # assume the file or directory needs to be created.
        tag = self.get_tag()
        pkt = self.proto.Twalk(tag=tag, fid=startdir, newfid=fid,
                               nwname=1, wname=components)
        super(P9Client, self).write(pkt)
        resp = self.wait_for(tag)
        if isinstance(resp, protocol.rrd.Rwalk):
            if clunk_startdir:
                self.clunk(startdir, ignore_error=True)
            # fid successfully walked to refer to final component.
            # Just need to actually open the file.
            self.setpath(fid, _pathcat(self.getpath(startdir), components[0]))
            qid = resp.wqid[0]
            return self._uxopen2(needtype, fid, qid, omode_byte, False)

        # Walk failed.  If we allocated a fid, retire it.  Then set
        # up a fid that points to the parent directory in which to
        # create the file or directory.  Note that if we're creating
        # a file, this fid will get changed so that it points to the
        # file instead of the directory, but if we're creating a
        # directory, it will be unchanged.
        if fid != startdir:
            self.retire_fid(fid)
        fid = self.dupfid(startdir)

        try:
            qid, iounit = self._uxcreate(filetype, fid, components[0],
                                         oflags, omode_byte, perm, gid)

            # Success.  If we created an ordinary file, we have everything
            # now as create alters the incoming (dir) fid to open the file.
            # Otherwise (mkdir), we need to open the file, as with
            # a successful lookup.
            #
            # Note that qid type should match "needtype".
            if filetype != 'dir':
                if qid.type == needtype:
                    return fid, qid, iounit, True
                self.clunk(fid, ignore_error=True)
                raise OSError(_wrong_file_type(qid),
                             '{0}: server told to create {1} but '
                             'created {2} instead'.format(path,
                                                          qt2n(needtype),
                                                          qt2n(qid.type)))

            # Success: created dir; but now need to walk to and open it.
            fid = self.alloc_fid()
            tag = self.get_tag()
            pkt = self.proto.Twalk(tag=tag, fid=startdir, newfid=fid,
                                   nwname=1, wname=components)
            super(P9Client, self).write(pkt)
            resp = self.wait_for(tag)
            if not isinstance(resp, protocol.rrd.Rwalk):
                self.clunk(fid, ignore_error=True)
                raise OSError(errno.ENOENT,
                              '{0}: server made dir but then failed to '
                              'find it again'.format(path))
                self.setpath(fid, _pathcat(self.getpath(fid), components[0]))
            return self._uxopen2(needtype, fid, qid, omode_byte, True)
        finally:
            # Regardless of success/failure/exception, make sure
            # we clunk startdir if needed.
            if clunk_startdir:
                self.clunk(startdir, ignore_error=True)

    def _uxcreate(self, filetype, fid, name, oflags, omode_byte, perm, gid):
        """
        Helper for creating dir-or-file.  The fid argument is the
        parent directory on input, but will point to the file (if
        we're creating a file) on return.  oflags only applies if
        we're creating a file (even then we use omode_byte if we
        are using the plan9 create op).
        """
        # Try to create or mkdir as appropriate.
        if self.supports_all(protocol.td.Tlcreate, protocol.td.Tmkdir):
            # Use Linux style create / mkdir.
            if filetype == 'dir':
                if perm is None:
                    perm = 0o777
                return self.mkdir(startdir, name, perm, gid), None
            if perm is None:
                perm = 0o666
            lflags = flags_to_linux_flags(oflags)
            return self.lcreate(fid, name, lflags, perm, gid)

        if filetype == 'dir':
            if perm is None:
                perm = protocol.td.DMDIR | 0o777
            else:
                perm |= protocol.td.DMDIR
        else:
            if perm is None:
                perm = 0o666
        return self.create(fid, name, perm, omode_byte)

    def _uxopen2(self, needtype, fid, qid, omode_byte, didcreate):
        "common code for finishing up uxopen"
        if qid.type != needtype:
            self.clunk(fid, ignore_error=True)
            raise OSError(_wrong_file_type(qid),
                          '{0}: is {1}, expected '
                          '{2}'.format(path, qt2n(qid.type), qt2n(needtype)))
        qid, iounit = self.open(fid, omode_byte)
        # ? should we re-check qid? it should not have changed
        return fid, qid, iounit, didcreate

    def uxmkdir(self, path, perm, gid, startdir=None):
        """
        Unix-style mkdir.

        The gid is only applied if we are using .L style mkdir.
        """
        components, startdir = self._pathsplit(path, startdir)
        clunkme = None
        if len(components) > 1:
            fid, _ = self.lookup(startdir, components[:-1])
            startdir = fid
            clunkme = fid
            components = components[-1:]
        try:
            if self.supports(protocol.td.Tmkdir):
                qid = self.mkdir(startdir, components[0], perm, gid)
            else:
                qid, _ = self.create(startdir, components[0],
                                     protocol.td.DMDIR | perm,
                                     protocol.td.OREAD)
                # Should we chown/chgrp the dir?
        finally:
            if clunkme:
                self.clunk(clunkme, ignore_error=True)
        return qid

    def uxreaddir(self, path, startdir=None, no_dotl=False):
        """
        Read a directory to get a list of names (which may or may not
        include '.' and '..').

        If no_dotl is True (or anything non-false-y), this uses the
        plain or .u readdir format, otherwise it uses dot-L readdir
        if possible.
        """
        components, startdir = self._pathsplit(path, startdir, allow_empty=True)
        fid, qid = self.lookup_last(startdir, components)
        try:
            if qid.type != protocol.td.QTDIR:
                raise OSError(errno.ENOTDIR,
                              '{0}: {1}'.format(self.getpathX(fid),
                                                os.strerror(errno.ENOTDIR)))
            # We need both Tlopen and Treaddir to use Treaddir.
            if not self.supports_all(protocol.td.Tlopen, protocol.td.Treaddir):
                no_dotl = True
            if no_dotl:
                statvals = self.uxreaddir_stat_fid(fid)
                return [i.name for i in statvals]

            dirents = self.uxreaddir_dotl_fid(fid)
            return [dirent.name for dirent in dirents]
        finally:
            self.clunk(fid, ignore_error=True)

    def uxreaddir_stat(self, path, startdir=None):
        """
        Use directory read to get plan9 style stat data (plain or .u readdir).

        Note that this gets a fid, then opens it, reads, then clunks
        the fid.  If you already have a fid, you may want to use
        uxreaddir_stat_fid (but note that this opens, yet does not
        clunk, the fid).

        We return the qid plus the list of the contents.  If the
        target is not a directory, the qid will not have type QTDIR
        and the contents list will be empty.

        Raises OSError if this is applied to a non-directory.
        """
        components, startdir = self._pathsplit(path, startdir)
        fid, qid = self.lookup_last(startdir, components)
        try:
            if qid.type != protocol.td.QTDIR:
                raise OSError(errno.ENOTDIR,
                              '{0}: {1}'.format(self.getpathX(fid),
                                                os.strerror(errno.ENOTDIR)))
            statvals = self.ux_readdir_stat_fid(fid)
            return qid, statvals
        finally:
            self.clunk(fid, ignore_error=True)

    def uxreaddir_stat_fid(self, fid):
        """
        Implement readdir loop that extracts stat values.
        This opens, but does not clunk, the given fid.

        Unlike uxreaddir_stat(), if this is applied to a file,
        rather than a directory, it just returns no entries.
        """
        statvals = []
        qid, iounit = self.open(fid, protocol.td.OREAD)
        # ?? is a zero iounit allowed? if so, what do we use here?
        if qid.type == protocol.td.QTDIR:
            if iounit <= 0:
                iounit = 512 # probably good enough
            offset = 0
            while True:
                bstring = self.read(fid, offset, iounit)
                if bstring == b'':
                    break
                statvals.extend(self.decode_stat_objects(bstring))
                offset += len(bstring)
        return statvals

    def uxreaddir_dotl_fid(self, fid):
        """
        Implement readdir loop that uses dot-L style dirents.
        This opens, but does not clunk, the given fid.

        If applied to a file, the lopen should fail, because of the
        L_O_DIRECTORY flag.
        """
        dirents = []
        qid, iounit = self.lopen(fid, protocol.td.OREAD |
                                      protocol.td.L_O_DIRECTORY)
        # ?? is a zero iounit allowed? if so, what do we use here?
        # but, we want a minimum of over 256 anyway, let's go for 512
        if iounit < 512:
            iounit = 512
        offset = 0
        while True:
            bstring = self.readdir(fid, offset, iounit)
            if bstring == b'':
                break
            ents = self.decode_readdir_dirents(bstring)
            if len(ents) == 0:
                break               # ???
            dirents.extend(ents)
            offset = ents[-1].offset
        return dirents

    def uxremove(self, path, startdir=None, filetype=None,
                 force=False, recurse=False):
        """
        Implement rm / rmdir, with optional -rf.
        if filetype is None, remove dir or file.  If 'dir' or 'file'
        remove only if it's one of those.  If force is set, ignore
        failures to remove.  If recurse is True, remove contents of
        directories (recursively).

        File type mismatches (when filetype!=None) raise OSError (?).
        """
        components, startdir = self._pathsplit(path, startdir, allow_empty=True)
        # Look up all components. If
        # we get an error we'll just assume the file does not
        # exist (is this good?).
        try:
            fid, qid = self.lookup_last(startdir, components)
        except RemoteError:
            return
        if qid.type == protocol.td.QTDIR:
            # it's a directory, remove only if allowed.
            # Note that we must check for "rm -r /" (len(components)==0).
            if filetype == 'file':
                self.clunk(fid, ignore_error=True)
                raise OSError(_wrong_file_type(qid),
                              '{0}: is dir, expected file'.format(path))
            isroot = len(components) == 0
            closer = self.clunk if isroot else self.remove
            if recurse:
                # NB: _rm_recursive does not clunk fid
                self._rm_recursive(fid, filetype, force)
            # This will fail if the directory is non-empty, unless of
            # course we tell it to ignore error.
            closer(fid, ignore_error=force)
            return
        # Not a directory, call it a file (even if socket or fifo etc).
        if filetype == 'dir':
            self.clunk(fid, ignore_error=True)
            raise OSError(_wrong_file_type(qid),
                          '{0}: is file, expected dir'.format(path))
        self.remove(fid, ignore_error=force)

    def _rm_file_by_dfid(self, dfid, name, force=False):
        """
        Remove a file whose name is <name> (no path, just a component
        name) whose parent directory is <dfid>.  We may assume that the
        file really is a file (or a socket, or fifo, or some such, but
        definitely not a directory).

        If force is set, ignore failures.
        """
        # If we have unlinkat, that's the fast way.  But it may
        # return an ENOTSUP error.  If it does we shouldn't bother
        # doing this again.
        if self.supports(protocol.td.Tunlinkat):
            try:
                self.unlinkat(dfid, name, 0)
                return
            except RemoteError as err:
                if not err.is_ENOTSUP():
                    raise
                self.unsupported(protocol.td.Tunlinkat)
                # fall through to remove() op
        # Fall back to lookup + remove.
        try:
            fid, qid = self.lookup_last(dfid, [name])
        except RemoteError:
            # If this has an errno we could tell ENOENT from EPERM,
            # and actually raise an error for the latter.  Should we?
            return
        self.remove(fid, ignore_error=force)

    def _rm_recursive(self, dfid, filetype, force):
        """
        Recursively remove a directory.  filetype is probably None,
        but if it's 'dir' we fail if the directory contains non-dir
        files.

        If force is set, ignore failures.

        Although we open dfid (via the readdir.*_fid calls) we
        do not clunk it here; that's the caller's job.
        """
        # first, remove contents
        if self.supports_all(protocol.td.Tlopen, protocol.td.Treaddir):
            for entry in self.uxreaddir_dotl_fid(dfid):
                if entry.name in (b'.', b'..'):
                    continue
                fid, qid = self.lookup(dfid, [entry.name])
                try:
                    attrs = self.Tgetattr(fid, protocol.td.GETATTR_MODE)
                    if stat.S_ISDIR(attrs.mode):
                        self.uxremove(entry.name, dfid, filetype, force, True)
                    else:
                        self.remove(fid)
                        fid = None
                finally:
                    if fid is not None:
                        self.clunk(fid, ignore_error=True)
        else:
            for statobj in self.uxreaddir_stat_fid(dfid):
                # skip . and ..
                name = statobj.name
                if name in (b'.', b'..'):
                    continue
                if statobj.qid.type == protocol.td.QTDIR:
                    self.uxremove(name, dfid, filetype, force, True)
                else:
                    self._rm_file_by_dfid(dfid, name, force)

def _wrong_file_type(qid):
    "return EISDIR or ENOTDIR for passing to OSError"
    if qid.type == protocol.td.QTDIR:
        return errno.EISDIR
    return errno.ENOTDIR

def flags_to_linux_flags(flags):
    """
    Convert OS flags (O_CREAT etc) to Linux flags (protocol.td.L_O_CREAT etc).
    """
    flagmap = {
        os.O_CREAT: protocol.td.L_O_CREAT,
        os.O_EXCL: protocol.td.L_O_EXCL,
        os.O_NOCTTY: protocol.td.L_O_NOCTTY,
        os.O_TRUNC: protocol.td.L_O_TRUNC,
        os.O_APPEND: protocol.td.L_O_APPEND,
        os.O_DIRECTORY: protocol.td.L_O_DIRECTORY,
    }

    result = flags & os.O_RDWR
    flags &= ~os.O_RDWR
    for key, value in flagmap.iteritems():
        if flags & key:
            result |= value
            flags &= ~key
    if flags:
        raise ValueError('untranslated bits 0x{0:x} in os flags'.format(flags))
    return result
