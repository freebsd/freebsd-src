#! /usr/bin/env python

"""
Protocol definitions for python based lib9p server/client.

The sub-namespace td has type definitions (qid, stat) and values
that are "#define" constants in C code (e.g., DMDIR, QTFILE, etc).
This also contains the byte values for protocol codes like Tversion,
Rversion, Rerror, and so on.

    >>> td.Tversion
    100
    >>> td.Rlerror
    7

The qid and stat types are PFOD classes and generate instances that
are a cross between namedtuple and OrderedDictionary (see pfod.py
for details):

    >>> td.qid(type=td.QTFILE, path=2, version=1)
    qid(type=0, version=1, path=2)

The td.stat() type output is pretty long, since it has all the
dotu-specific members (used only when packing for dotu/dotl and
set only when unpacking those), so here's just one field:

    >>> td.stat(*(15 * [0])).mode
    0
    >>> import pprint; pprint.pprint(td.stat()._fields)
    ('type',
     'dev',
     'qid',
     'mode',
     'atime',
     'mtime',
     'length',
     'name',
     'uid',
     'gid',
     'muid',
     'extension',
     'n_uid',
     'n_gid',
     'n_muid')

Stat objects sent across the protocol must first be encoded into
wirestat objects, which are basically size-counted pre-sequenced
stat objects.  The pre-sequencing uses:

    >>> td.stat_seq
    Sequencer('stat')

For parsing bytes returned in a Tread on a directory, td.wirestat_seq
is the sequencer.  However, most users should rely on the packers and
unpackers in each protocol (see {pack,unpack}_wirestat below).

    >>> td.wirestat_seq
    Sequencer('wirestat')

There is a dictionary fcall_to_name that maps from byte value
to protocol code.  Names map to themselves as well:

    >>> fcall_names[101]
    'Rversion'
    >>> fcall_names['Tversion']
    'Tversion'

The sub-namespace rrd has request (Tversion, Topen, etc) and
response (Rversion, Ropen, etc) data definitions.  Each of these
is a PFOD class:

    >>> rrd.Tversion(1000, 'hello', tag=0)
    Tversion(tag=0, msize=1000, version='hello')

The function p9_version() looks up the instance of each supported
protocol, or raises a KeyError when given an invalid protocol.
The names may be spelled in any mixture of cases.

The names plain, dotu, and dotl are predefined as the three
supported protocols:

    >>> p9_version('invalid')
    Traceback (most recent call last):
        ...
    KeyError: 'invalid'
    >>> p9_version('9p2000') == plain
    True
    >>> p9_version('9P2000') == plain
    True
    >>> p9_version('9P2000.u') == dotu
    True
    >>> p9_version('9p2000.L') == dotl
    True

Protocol instances have a pack() method that encodes a set of
arguments into a packet.  To know what to encode, pack() must
receive an fcall value and a dictionary containing argument
values, or something equivalent.  The required argument values
depend on the fcall.  For instance, a Tversion fcall needs three
arguments: the version name, the tag, and the msize (these of
course are the pre-filled fields in a Tversion PFOD instance).

    >>> args = {'version': '!', 'tag': 1, 'msize': 1000}
    >>> pkt = dotu.pack(fcall='Tversion', args=args)
    >>> len(pkt)
    14

The length of string '!' is 1, and the packet (or wire) format of
a Tversion request is:

   size[4] fcall[1] tag[2] msize[4] version[s]

which corresponds to a struct's IBHIH (for the fixed size parts)
followed by 1 B (for the string).  The overall packet is 14 bytes
long, so we have size=9, fcall=100, tag=1, msize=1000, and the
version string is length=1, value=33 (ord('!')).

    >>> import struct
    >>> struct.unpack('<IBHIHB', pkt)
    (14, 100, 1, 1000, 1, 33)

Of course, this packed a completely bogus "version" string, but
that's what we told it to do.  Protocol instances remember their
version, so we can get it right by omitting the version from the
arguments:

    >>> dotu.version
    '9P2000.u'
    >>> args = {'tag': 99, 'msize': 1000}
    >>> pkt = dotu.pack(fcall='Tversion', args=args)
    >>> len(pkt)
    21

The fcall can be supplied numerically:

    >>> pkt2 = dotu.pack(fcall=td.Tversion, args=args)
    >>> pkt == pkt2
    True

Instead of providing an fcall you can provide an instance of
the appropriate PFOD.  In this case pack() finds the type from
the PFOD instance.  As usual, the version parameter is filled in
for you:

    >>> pkt2 = dotu.pack(rrd.Tversion(tag=99, msize=1000))
    >>> pkt == pkt2
    True

Note that it's up to you to check the other end's version and
switch to a "lower" protocol as needed.  Each instance does provide
a downgrade_to() method that gets you a possibly-downgraded instance.
This will fail if you are actually trying to upgrade, and also if
you provide a bogus version:

    >>> dotu.downgrade_to('9P2000.L')
    Traceback (most recent call last):
        ...
    KeyError: '9P2000.L'
    >>> dotu.downgrade_to('we never heard of this protocol')
    Traceback (most recent call last):
        ...
    KeyError: 'we never heard of this protocol'

Hence you might use:

    try:
        proto = protocol.dotl.downgrade(vstr)
    except KeyError:
        pkt = protocol.plain.pack(fcall='Rerror',
            args={'tag': tag, 'errstr': 'unknown protocol version '
                    '{0!r}'.format(vstr)})
    else:
        pkt = proto.pack(fcall='Rversion', args={'tag': tag, 'msize': msize})

When using a PFOD instance, it is slightly more efficient to use
pack_from():

    try:
        proto = protocol.dotl.downgrade(vstr)
        reply = protocol.rrd.Rversion(tag=tag, msize=msize)
    except KeyError:
        proto = protocol.plain
        reply = protocol.rrd.Rerror(tag=tag,
            errstr='unknown protocol version {0!r}'.format(vstr))
    pkt = proto.pack_from(reply)

does the equivalent of the try/except/else variant.  Note that
the protocol.rrd.Rversion() instance has version=None.  Like
proto.pack, the pack_from will detect this "missing" value and
fill it in.

Because errors vary (one should use Rlerror for dotl and Rerror
for dotu and plain), and it's convenient to use an Exception
instance for an error, all protocols provide .error().  This
builds the appropriate kind of error response, extracting and
converting errno's and error messages as appropriate.

If <err> is an instance of Exception, err.errno provides the errnum
or ecode value (if used, for dotu and dotl) and err.strerror as the
errstr value (if used, for plain 9p2000).  Otherwise err should be
an integer, and we'll use os.strerror() to get a message.

When using plain 9P2000 this sends error *messages*:

    >>> import errno, os
    >>> utf8 = os.strerror(errno.ENOENT).encode('utf-8')
    >>> pkt = None
    >>> try:
    ...     os.open('presumably this file does not exist here', 0)
    ... except OSError as err:
    ...     pkt = plain.error(1, err)
    ...
    >>> pkt[-len(utf8):] == utf8
    True
    >>> pkt2 = plain.error(1, errno.ENOENT)
    >>> pkt == pkt2
    True

When using 9P2000.u it sends the error code as well, and when
using 9P2000.L it sends only the error code (and more error
codes can pass through):

    >>> len(pkt)
    34
    >>> len(dotu.error(1, errno.ENOENT))
    38
    >>> len(dotl.error(1, errno.ENOENT))
    11

For even more convenience (and another slight speed hack), the
protocol has member functions for each valid pfod, which
effectively do a pack_from of a pfod built from the arguments.  In
the above example this is not very useful (because we want two
different replies), but for Rlink, for instance, which has only
a tag, a server might implement Tlink() as:

    def do_Tlink(proto, data): # data will be a protocol.rrd.Tlink(...)
        tag = data.tag
        dfid = data.dfid
        fid = data.fid
        name = data.name
        ... some code to set up for doing the link link ...
        try:
            os.link(path1, path2)
        except OSError as err:
            return proto.error(tag, err)
        else:
            return proto.Rlink(tag)

    >>> pkt = dotl.Rlink(12345)
    >>> struct.unpack('<IBH', pkt)
    (7, 71, 12345)

Similarly, a client can build a Tversion packet quite trivially:

    >>> vpkt = dotl.Tversion(tag=0, msize=12345)

To see that this is a valid version packet, let's unpack its bytes.
The overall length is 21 bytes: 4 bytes of size, 1 byte of code 100
for Tversion, 2 bytes of tag, 4 bytes of msize, 2 bytes of string
length, and 8 bytes of string '9P2000.L'.

    >>> tup = struct.unpack('<IBHIH8B', vpkt)
    >>> tup[0:5]
    (21, 100, 0, 12345, 8)
    >>> ''.join(chr(i) for i in tup[5:])
    '9P2000.L'

Of course, since you can *pack*, you can also *unpack*.  It's
possible that the incoming packet is malformed.  If so, this
raises various errors (see below).

Unpack is actually a two step process: first we unpack a header
(where the size is already removed and is implied by len(data)),
then we unpack the data within the packet.  You can invoke the
first step separately.  Furthermore, there's a noerror argument
that leaves some fields set to None or empty strings, if the
packet is too short.  (Note that we need a hack for py2k vs py3k
strings here, for doctests.  Also, encoding 12345 into a byte
string produces '90', by ASCII luck!)

    >>> pkt = pkt[4:] # strip generated size
    >>> import sys
    >>> py3k = sys.version_info[0] >= 3
    >>> b2s = lambda x: x.decode('utf-8') if py3k else x
    >>> d = plain.unpack_header(pkt[0:1], noerror=True)
    >>> d.data = b2s(d.data)
    >>> d
    Header(size=5, dsize=0, fcall=71, data='')
    >>> d = plain.unpack_header(pkt[0:2], noerror=True)
    >>> d.data = b2s(d.data)
    >>> d
    Header(size=6, dsize=1, fcall=71, data='9')

Without noerror=True a short packet raises a SequenceError:

    >>> plain.unpack_header(pkt[0:0])   # doctest: +IGNORE_EXCEPTION_DETAIL
    Traceback (most recent call last):
        ...
    SequenceError: out of data while unpacking 'fcall'

Of course, a normal packet decodes fine:

    >>> d = plain.unpack_header(pkt)
    >>> d.data = b2s(d.data)
    >>> d
    Header(size=7, dsize=2, fcall=71, data='90')

but one that is too *long* potentially raises a SequencError.
(This is impossible for a header, though, since the size and
data size are both implied: either there is an fcall code, and
the rest of the bytes are "data", or there isn't and the packet
is too short.  So we can only demonstrate this for regular
unpack; see below.)

Note that all along, this has been decoding Rlink (fcall=71),
which is not valid for plain 9P2000 protocol.  It's up to the
caller to check:

    >>> plain.supports(71)
    False

    >>> plain.unpack(pkt)           # doctest: +IGNORE_EXCEPTION_DETAIL
    Traceback (most recent call last):
        ...
    SequenceError: invalid fcall 'Rlink' for 9P2000
    >>> dotl.unpack(pkt)
    Rlink(tag=12345)

However, the unpack() method DOES check that the fcall type is
valid, even if you supply noerror=True.  This is because we can
only really decode the header, not the data, if the fcall is
invalid:

    >>> plain.unpack(pkt, noerror=True)     # doctest: +IGNORE_EXCEPTION_DETAIL
    Traceback (most recent call last):
        ...
    SequenceError: invalid fcall 'Rlink' for 9P2000

The same applies to much-too-short packets even if noerror is set.
Specifically, if the (post-"size") header shortens down to the empty
string, the fcall will be None:

    >>> dotl.unpack(b'', noerror=True)      # doctest: +IGNORE_EXCEPTION_DETAIL
    Traceback (most recent call last):
        ...
    SequenceError: invalid fcall None for 9P2000.L

If there is at least a full header, though, noerror will do the obvious:

    >>> dotl.unpack(pkt[0:1], noerror=True)
    Rlink(tag=None)
    >>> dotl.unpack(pkt[0:2], noerror=True)
    Rlink(tag=None)

If the packet is too long, noerror suppresses the SequenceError:

    >>> dotl.unpack(pkt + b'x')             # doctest: +IGNORE_EXCEPTION_DETAIL
    Traceback (most recent call last):
        ...
    SequenceError: 1 byte(s) unconsumed
    >>> dotl.unpack(pkt + b'x', noerror=True)
    Rlink(tag=12345)

To pack a stat object when producing data for reading a directory,
use pack_wirestat.  This puts a size in front of the packed stat
data (they're represented this way in read()-of-directory data,
but not elsewhere).

To unpack the result of a Tstat or a read() on a directory, use
unpack_wirestat.  The stat values are variable length so this
works with offsets.  If the packet is truncated, you'll get a
SequenceError, but just as for header unpacking, you can use
noerror to suppress this.

(First, we'll need to build some valid packet data.)

    >>> statobj = td.stat(type=0,dev=0,qid=td.qid(0,0,0),mode=0,
    ... atime=0,mtime=0,length=0,name=b'foo',uid=b'0',gid=b'0',muid=b'0')
    >>> data = plain.pack_wirestat(statobj)
    >>> len(data)
    55

Now we can unpack it:

    >>> newobj, offset = plain.unpack_wirestat(data, 0)
    >>> newobj == statobj
    True
    >>> offset
    55

Since the packed data do not include the dotu extensions, we get
a SequenceError if we try to unpack with dotu or dotl:

    >>> dotu.unpack_wirestat(data, 0)       # doctest: +IGNORE_EXCEPTION_DETAIL
    Traceback (most recent call last):
        ...
    SequenceError: out of data while unpacking 'extension'

When using noerror, the returned new offset will be greater
than the length of the packet, after a failed unpack, and some
elements may be None:

    >>> newobj, offset = plain.unpack_wirestat(data[0:10], 0, noerror=True)
    >>> offset
    55
    >>> newobj.length is None
    True

Similarly, use unpack_dirent to unpack the result of a dot-L
readdir(), using offsets.  (Build them with pack_dirent.)

    >>> dirent = td.dirent(qid=td.qid(1,2,3),offset=0,
    ... type=td.DT_REG,name=b'foo')
    >>> pkt = dotl.pack_dirent(dirent)
    >>> len(pkt)
    27

and then:

    >>> newde, offset = dotl.unpack_dirent(pkt, 0)
    >>> newde == dirent
    True
    >>> offset
    27

"""

from __future__ import print_function

import collections
import os
import re
import sys

import p9err
import pfod
import sequencer

SequenceError = sequencer.SequenceError

fcall_names = {}

# begin ???
# to interfere with (eg) the size part of the packet:
#   pkt = proto.pack(fcall=protocol.td.Tversion,
#       size=123, # wrong
#       args={ 'tag': 1, msize: 1000, version: '9p2000.u' })
# a standard Twrite:
#   pkt = proto.pack(fcall=protocol.td.Twrite,
#       args={ 'tag': 1, 'fid': 2, 'offset': 0, 'data': b'rawdata' })
# or:
#   pkt = proto.pack(fcall=protocol.td.Twrite,
#       data=proto.Twrite(tag=1, fid=2, offset=0, data=b'rawdata' })
# a broken Twrite:
#   pkt = proto.pack(fcall=protocol.td.Twrite,
#       args={ 'tag': 1, 'fid': 2, 'offset': 0, 'count': 99,
#           'data': b'rawdata' })  -- XXX won't work (yet?)
#
# build a QID: (td => typedefs and defines)
#    qid = protocol.td.qid(type=protocol.td.QTFILE, version=1, path=2)
# build the Twrite data as a data structure:
#    wrdata = protocol.td.Twrite(tag=1, fid=2, offset=0, data=b'rawdata')
#
# turn incoming byte stream data into a Header and remaining data:
#    foo = proto.pack(data)

class _PackInfo(object):
    """
    Essentially just a Sequencer, except that we remember
    if there are any :auto annotations on any of the coders,
    and we check for coders that are string coders ('data[size]').

    This could in theory be a recursive check, but in practice
    all the automatics are at the top level, and we have no mechanism
    to pass down inner automatics.
    """
    def __init__(self, seq):
        self.seq = seq
        self.autos = None
        for pair in seq:        # (cond, code) pair
            sub = pair[1]
            if sub.aux is None:
                continue
            assert sub.aux == 'auto' or sub.aux == 'len'
            if self.autos is None:
                self.autos = []
            self.autos.append(pair)

    def __repr__(self):
        return '{0}({1!r})'.format(self.__class__.__name__, self.seq)

    def pack(self, auto_vars, conditions, data, rodata):
        """
        Pack data.  Insert automatic and/or counted variables
        automatically, if they are not already set in the data.

        If rodata ("read-only data") is True we make sure not
        to modify the caller's data.  Since data is a PFOD rather
        than a normal ordered dictionary, we use _copy().
        """
        if self.autos:
            for cond, sub in self.autos:
                # False conditionals don't need to be filled-in.
                if cond is not None and not conditions[cond]:
                    continue
                if sub.aux == 'auto':
                    # Automatic variable, e.g., version.  The
                    # sub-coder's name ('version') is the test item.
                    if data.get(sub.name) is None:
                        if rodata:
                            data = data._copy()
                            rodata = False
                        data[sub.name] = auto_vars[sub.name]
                else:
                    # Automatic length, e.g., data[count].  The
                    # sub-coders's repeat item ('count') is the
                    # test item.  Of course, it's possible that
                    # the counted item is missing as well.  If so
                    # we just leave both None and take the
                    # encoding error.
                    assert sub.aux == 'len'
                    if data.get(sub.repeat) is not None:
                        continue
                    item = data.get(sub.name)
                    if item is not None:
                        if rodata:
                            data = data._copy()
                            rodata = False
                        data[sub.repeat] = len(item)
        return self.seq.pack(data, conditions)

class _P9Proto(object):
    def __init__(self, auto_vars, conditions, p9_data, pfods, index):
        self.auto_vars = auto_vars      # currently, just version
        self.conditions = conditions    # '.u'
        self.pfods = pfods # dictionary, maps pfod to packinfo
        self.index = index # for comparison: plain < dotu < dotl

        self.use_rlerror = rrd.Rlerror in pfods

        for dtype in pfods:
            name = dtype.__name__
            # For each Txxx/Rxxx, define a self.<name>() to
            # call self.pack_from().
            #
            # The packinfo is from _Packinfo(seq); the fcall and
            # seq come from p9_data.protocol[<name>].
            proto_tuple = p9_data.protocol[name]
            assert dtype == proto_tuple[0]
            packinfo = pfods[dtype]
            # in theory we can do this with no names using nested
            # lambdas, but that's just too confusing, so let's
            # do it with nested functions instead.
            def builder(constructor=dtype, packinfo=packinfo):
                "return function that calls _pack_from with built PFOD"
                def invoker(self, *args, **kwargs):
                    "build PFOD and call _pack_from"
                    return self._pack_from(constructor(*args, **kwargs),
                                           rodata=False, caller=None,
                                           packinfo=packinfo)
                return invoker
            func = builder()
            func.__name__ = name
            func.__doc__ = 'pack from {0}'.format(name)
            setattr(self.__class__, name, func)

    def __repr__(self):
        return '{0}({1!r})'.format(self.__class__.__name__, self.version)

    def __str__(self):
        return self.version

    # define rich-comparison operators, so we can, e.g., test vers > plain
    def __lt__(self, other):
        return self.index < other.index
    def __le__(self, other):
        return self.index <= other.index
    def __eq__(self, other):
        return self.index == other.index
    def __ne__(self, other):
        return self.index != other.index
    def __gt__(self, other):
        return self.index > other.index
    def __ge__(self, other):
        return self.index >= other.index

    def downgrade_to(self, other_name):
        """
        Downgrade from this protocol to a not-greater one.

        Raises KeyError if other_name is not a valid protocol,
        or this is not a downgrade (with setting back to self
        considered a valid "downgrade", i.e., we're doing subseteq
        rather than subset).
        """
        if not isinstance(other_name, str) and isinstance(other_name, bytes):
            other_name = other_name.decode('utf-8', 'surrogateescape')
        other = p9_version(other_name)
        if other > self:
            raise KeyError(other_name)
        return other

    def error(self, tag, err):
        "produce Rerror or Rlerror, whichever is appropriate"
        if isinstance(err, Exception):
            errnum = err.errno
            errmsg = err.strerror
        else:
            errnum = err
            errmsg = os.strerror(errnum)
        if self.use_rlerror:
            return self.Rlerror(tag=tag, ecode=p9err.to_dotl(errnum))
        return self.Rerror(tag=tag, errstr=errmsg,
                           errnum=p9err.to_dotu(errnum))

    def pack(self, *args, **kwargs):
        "pack up a pfod or fcall-and-arguments"
        fcall = kwargs.pop('fcall', None)
        if fcall is None:
            # Called without fcall=...
            # This requires that args have one argument that
            # is the PFOD; kwargs should be empty (but we'll take
            # data=pfod as well).  The size is implied, and
            # fcall comes from the pfod.
            data = kwargs.pop('data', None)
            if data is None:
                if len(args) != 1:
                    raise TypeError('pack() with no fcall requires 1 argument')
                data = args[0]
            if len(kwargs):
                raise TypeError('pack() got an unexpected keyword argument '
                                '{0}'.format(kwargs.popitem()[0]))
            return self._pack_from(data, True, 'pack', None)

        # Called as pack(fcall=whatever, data={...}).
        # The data argument must be a dictionary since we're going to
        # apply ** to it in the call to build the PFOD.  Note that
        # it could already be a PFOD, which is OK, but we're going to
        # copy it to a new one regardless (callers that have a PFOD
        # should use pack_from instead).
        if len(args):
            raise TypeError('pack() got unexpected arguments '
                            '{0!r}'.format(args))
        data = kwargs.pop('args', None)
        if len(kwargs):
            raise TypeError('pack() got an unexpected keyword argument '
                            '{0}'.format(kwargs.popitem()[0]))
        if not isinstance(data, dict):
            raise TypeError('pack() with fcall and data '
                            'requires data to be a dictionary')
        try:
            name = fcall_names[fcall]
        except KeyError:
            raise TypeError('pack(): {0} is not a valid '
                            'fcall value'.format(fcall))
        cls = getattr(rrd, name)
        data = cls(**data)
        return self._pack_from(data, False, 'pack', None)

    def pack_from(self, data):
        "pack from pfod data, using its type to determine fcall"
        return self._pack_from(data, True, 'pack_from', None)

    def _pack_from(self, data, rodata, caller, packinfo):
        """
        Internal pack(): called from both invokers (self.Tversion,
        self.Rwalk, etc.) and from pack and pack_from methods.
        "caller" says which.  If rodata is True we're not supposed to
        modify the incoming data, as it may belong to someone
        else.  Some calls to pack() build a PFOD and hence pass in
        False.

        The predefined invokers pass in a preconstructed PFOD,
        *and* set rodata=False, *and* provide a packinfo, so that
        we never have to copy, nor look up the packinfo.
        """
        if caller is not None:
            assert caller in ('pack', 'pack_from') and packinfo is None
            # Indirect call from pack_from(), or from pack() after
            # pack() built a PFOD.  We make sure this kind of PFOD
            # is allowed for this protocol.
            packinfo = self.pfods.get(data.__class__, None)
            if packinfo is None:
                raise TypeError('{0}({1!r}): invalid '
                                'input'.format(caller, data))

        # Pack the data
        pkt = packinfo.pack(self.auto_vars, self.conditions, data, rodata)

        fcall = data.__class__.__name__
        fcall_code = getattr(td, fcall)

        # That's the inner data; now we must add the header,
        # with fcall (translated back to byte code value) and
        # outer data.  The size is implied by len(pkt).  There
        # are no other auto variables, and no conditions.
        #
        # NB: the size includes the size of the header itself
        # and the fcall code byte, plus the size of the data.
        data = _9p_data.header_pfod(size=4 + 1 + len(pkt), dsize=len(pkt),
                                    fcall=fcall_code, data=pkt)
        empty = None # logically should be {}, but not actually used below
        pkt = _9p_data.header_pack_seq.pack(data, empty)
        return pkt

    @staticmethod
    def unpack_header(bstring, noerror=False):
        """
        Unpack header.

        We know that our caller has already stripped off the
        overall size field (4 bytes), leaving us with the fcall
        (1 byte) and data (len(bstring)-1 bytes).  If len(bstring)
        is 0, this is an invalid header: set dsize to 0 and let
        fcall become None, if noerror is set.
        """
        vdict = _9p_data.header_pfod()
        vdict['size'] = len(bstring) + 4
        vdict['dsize'] = max(0, len(bstring) - 1)
        _9p_data.header_unpack_seq.unpack(vdict, None, bstring, noerror)
        return vdict

    def unpack(self, bstring, noerror=False):
        "produce filled PFOD from fcall in packet"
        vdict = self.unpack_header(bstring, noerror)
        # NB: vdict['dsize'] is used internally during unpack, to
        # find out how many bytes to copy to vdict['data'], but by
        # the time unpack is done, we no longer need it.
        #
        # size = vdict['size']
        # dsize = vdict['dsize']
        fcall = vdict['fcall']
        data = vdict['data']
        # Note: it's possible for size and/or fcall to be None,
        # when noerror is true.  However, if we support fcall, then
        # clearly fcall is not None; and since fcall follows size,
        # we can always proceed if we support fcall.
        if self.supports(fcall):
            fcall = fcall_names[fcall]
            cls = getattr(rrd, fcall)
            seq = self.pfods[cls].seq
        elif fcall == td.Rlerror:
            # As a special case for diod, we accept Rlerror even
            # if it's not formally part of the protocol.
            cls = rrd.Rlerror
            seq = dotl.pfods[rrd.Rlerror].seq
        else:
            fcall = fcall_names.get(fcall, fcall)
            raise SequenceError('invalid fcall {0!r} for '
                                '{1}'.format(fcall, self))
        vdict = cls()
        seq.unpack(vdict, self.conditions, data, noerror)
        return vdict

    def pack_wirestat(self, statobj):
        """
        Pack a stat object to appear as data returned by read()
        on a directory.  Essentially, we prefix the data with a size.
        """
        data = td.stat_seq.pack(statobj, self.conditions)
        return td.wirestat_seq.pack({'size': len(data), 'data': data}, {})

    def unpack_wirestat(self, bstring, offset, noerror=False):
        """
        Produce the next td.stat object from byte-string,
        returning it and new offset.
        """
        statobj = td.stat()
        d = { 'size': None }
        newoff = td.wirestat_seq.unpack_from(d, self.conditions, bstring,
                                             offset, noerror)
        size = d['size']
        if size is None:        # implies noerror; newoff==offset+2
            return statobj, newoff
        # We now have size and data.  If noerror, data might be
        # too short, in which case we'll unpack a partial statobj.
        # Or (with or without noeror), data might be too long, so
        # that while len(data) == size, not all the data get used.
        # That may be allowed by the protocol: it's not clear.
        data = d['data']
        used = td.stat_seq.unpack_from(statobj, self.conditions, data,
                                       0, noerror)
        # if size != used ... then what?
        return statobj, newoff

    def pack_dirent(self, dirent):
        """
        Dirents (dot-L only) are easy to pack, but we provide
        this function for symmetry.  (Should we raise an error
        if called on plain or dotu?)
        """
        return td.dirent_seq.pack(dirent, self.conditions)

    def unpack_dirent(self, bstring, offset, noerror=False):
        """
        Produces the next td.dirent object from byte-string,
        returning it and new offset.
        """
        deobj = td.dirent()
        offset = td.dirent_seq.unpack_from(deobj, self.conditions, bstring,
                                           offset, noerror)
        return deobj, offset

    def supports(self, fcall):
        """
        Return True if and only if this protocol supports the
        given fcall.

        >>> plain.supports(100)
        True
        >>> plain.supports('Tversion')
        True
        >>> plain.supports('Rlink')
        False
        """
        fcall = fcall_names.get(fcall, None)
        if fcall is None:
            return False
        cls = getattr(rrd, fcall)
        return cls in self.pfods

    def get_version(self, as_bytes=True):
        "get Plan 9 protocol version, as string or (default) as bytes"
        ret = self.auto_vars['version']
        if as_bytes and not isinstance(ret, bytes):
            ret = ret.encode('utf-8')
        return ret

    @property
    def version(self):
        "Plan 9 protocol version"
        return self.get_version(as_bytes=False)

DEBUG = False

# This defines a special en/decoder named "s" using a magic
# builtin.  This and stat are the only variable-length
# decoders, and this is the only recursively-variable-length
# one (i.e., stat decoding is effectively fixed size once we
# handle strings).  So this magic avoids the need for recursion.
#
# Note that _string_ is, in effect, size[2] orig_var[size].
_STRING_MAGIC = '_string_'
SDesc = "typedef s: " + _STRING_MAGIC

# This defines an en/decoder for type "qid",
# which en/decodes 1 byte called type, 4 called version, and
# 8 called path (for a total of 13 bytes).
#
# It also defines QTDIR, QTAPPEND, etc.  (These are not used
# for en/decode, or at least not yet.)
QIDDesc = """\
typedef qid: type[1] version[4] path[8]

    #define QTDIR       0x80
    #define QTAPPEND    0x40
    #define QTEXCL      0x20
    #define QTMOUNT     0x10
    #define QTAUTH      0x08
    #define QTTMP       0x04
    #define QTSYMLINK   0x02
    #define QTFILE      0x00
"""

# This defines a stat decoder, which has a 9p2000 standard front,
# followed by an optional additional portion.
#
# The constants are named DMDIR etc.
STATDesc = """
typedef stat: type[2] dev[4] qid[qid] mode[4] atime[4] mtime[4] \
length[8] name[s] uid[s] gid[s] muid[s] \
{.u: extension[s] n_uid[4] n_gid[4] n_muid[4] }

    #define DMDIR           0x80000000
    #define DMAPPEND        0x40000000
    #define DMMOUNT         0x10000000
    #define DMAUTH          0x08000000
    #define DMTMP           0x04000000
    #define DMSYMLINK       0x02000000
            /* 9P2000.u extensions */
    #define DMDEVICE        0x00800000
    #define DMNAMEDPIPE     0x00200000
    #define DMSOCKET        0x00100000
    #define DMSETUID        0x00080000
    #define DMSETGID        0x00040000
"""

# This defines a wirestat decoder.  A wirestat is a size and then
# a (previously encoded, or future-decoded) stat.
WirestatDesc = """
typedef wirestat: size[2] data[size]
"""

# This defines a dirent decoder, which has a dot-L specific format.
#
# The dirent type fields are defined as DT_* (same as BSD and Linux).
DirentDesc = """
typedef dirent: qid[qid] offset[8] type[1] name[s]

    #define DT_UNKNOWN       0
    #define DT_FIFO          1
    #define DT_CHR           2
    #define DT_DIR           4
    #define DT_BLK           6
    #define DT_REG           8
    #define DT_LNK          10
    #define DT_SOCK         12
    #define DT_WHT          14
"""

# N.B.: this is largely a slightly more rigidly formatted variant of
# the contents of:
# https://github.com/chaos/diod/blob/master/protocol.md
#
# Note that <name> = <value>: ... assigns names for the fcall
# fcall (function call) table.  Names without "= value" are
# assumed to be the previous value +1 (and the two names are
# also checked to make sure they are Tfoo,Rfoo).
ProtocolDesc = """\
Rlerror.L = 7: tag[2] ecode[4]
    ecode is a numerical Linux errno

Tstatfs.L = 8: tag[2] fid[4]
Rstatfs.L: tag[2] type[4] bsize[4] blocks[8] bfree[8] bavail[8] \
         files[8] ffree[8] fsid[8] namelen[4]
    Rstatfs corresponds to Linux statfs structure:
    struct statfs {
        long    f_type;     /* type of file system */
        long    f_bsize;    /* optimal transfer block size */
        long    f_blocks;   /* total data blocks in file system */
        long    f_bfree;    /* free blocks in fs */
        long    f_bavail;   /* free blocks avail to non-superuser */
        long    f_files;    /* total file nodes in file system */
        long    f_ffree;    /* free file nodes in fs */
        fsid_t  f_fsid;     /* file system id */
        long    f_namelen;  /* maximum length of filenames */
    };

    This comes from nowhere obvious...
        #define FSTYPE      0x01021997

Tlopen.L = 12: tag[2] fid[4] flags[4]
Rlopen.L: tag[2] qid[qid] iounit[4]
    lopen prepares fid for file (or directory) I/O.

    flags contains Linux open(2) flag bits, e.g., O_RDONLY, O_RDWR, O_WRONLY.

        #define L_O_CREAT       000000100
        #define L_O_EXCL        000000200
        #define L_O_NOCTTY      000000400
        #define L_O_TRUNC       000001000
        #define L_O_APPEND      000002000
        #define L_O_NONBLOCK    000004000
        #define L_O_DSYNC       000010000
        #define L_O_FASYNC      000020000
        #define L_O_DIRECT      000040000
        #define L_O_LARGEFILE   000100000
        #define L_O_DIRECTORY   000200000
        #define L_O_NOFOLLOW    000400000
        #define L_O_NOATIME     001000000
        #define L_O_CLOEXEC     002000000
        #define L_O_SYNC        004000000
        #define L_O_PATH        010000000
        #define L_O_TMPFILE     020000000

Tlcreate.L = 14: tag[2] fid[4] name[s] flags[4] mode[4] gid[4]
Rlcreate.L: tag[2] qid[qid] iounit[4]
    lcreate creates a regular file name in directory fid and prepares
    it for I/O.

    fid initially represents the parent directory of the new file.
    After the call it represents the new file.

    flags contains Linux open(2) flag bits (including O_CREAT).

    mode contains Linux creat(2) mode (permissions) bits.

    gid is the effective gid of the caller.

Tsymlink.L = 16: tag[2] dfid[4] name[s] symtgt[s] gid[4]
Rsymlink.L: tag[2] qid[qid]
    symlink creates a symbolic link name in directory dfid.  The
    link will point to symtgt.

    gid is the effective group id of the caller.

    The qid for the new symbolic link is returned in the reply.

Tmknod.L = 18: tag[2] dfid[4] name[s] mode[4] major[4] minor[4] gid[4]
Rmknod.L: tag[2] qid[qid]
    mknod creates a device node name in directory dfid with major
    and minor numbers.

    mode contains Linux mknod(2) mode bits.  (Note that these
    include the S_IFMT bits which may be S_IFBLK, S_IFCHR, or
    S_IFSOCK.)

    gid is the effective group id of the caller.

    The qid for the new device node is returned in the reply.

Trename.L = 20: tag[2] fid[4] dfid[4] name[s]
Rrename.L: tag[2]
    rename renames a file system object referenced by fid, to name
    in the directory referenced by dfid.

    This operation will eventually be replaced by renameat.

Treadlink.L = 22: tag[2] fid[4]
Rreadlink.L: tag[2] target[s]
    readlink returns the contents of teh symbolic link referenced by fid.

Tgetattr.L = 24: tag[2] fid[4] request_mask[8]
Rgetattr.L: tag[2] valid[8] qid[qid] mode[4] uid[4] gid[4] nlink[8] \
          rdev[8] size[8] blksize[8] blocks[8] \
          atime_sec[8] atime_nsec[8] mtime_sec[8] mtime_nsec[8] \
          ctime_sec[8] ctime_nsec[8] btime_sec[8] btime_nsec[8] \
          gen[8] data_version[8]

    getattr gets attributes of a file system object referenced by fid.
    The response is intended to follow pretty closely the fields
    returned by the stat(2) system call:

    struct stat {
        dev_t     st_dev;     /* ID of device containing file */
        ino_t     st_ino;     /* inode number */
        mode_t    st_mode;    /* protection */
        nlink_t   st_nlink;   /* number of hard links */
        uid_t     st_uid;     /* user ID of owner */
        gid_t     st_gid;     /* group ID of owner */
        dev_t     st_rdev;    /* device ID (if special file) */
        off_t     st_size;    /* total size, in bytes */
        blksize_t st_blksize; /* blocksize for file system I/O */
        blkcnt_t  st_blocks;  /* number of 512B blocks allocated */
        time_t    st_atime;   /* time of last access */
        time_t    st_mtime;   /* time of last modification */
        time_t    st_ctime;   /* time of last status change */
    };

    The differences are:

     * st_dev is omitted
     * st_ino is contained in the path component of qid
     * times are nanosecond resolution
     * btime, gen and data_version fields are reserved for future use

    Not all fields are valid in every call. request_mask is a bitmask
    indicating which fields are requested. valid is a bitmask
    indicating which fields are valid in the response. The mask
    values are as follows:

    #define GETATTR_MODE        0x00000001
    #define GETATTR_NLINK       0x00000002
    #define GETATTR_UID         0x00000004
    #define GETATTR_GID         0x00000008
    #define GETATTR_RDEV        0x00000010
    #define GETATTR_ATIME       0x00000020
    #define GETATTR_MTIME       0x00000040
    #define GETATTR_CTIME       0x00000080
    #define GETATTR_INO         0x00000100
    #define GETATTR_SIZE        0x00000200
    #define GETATTR_BLOCKS      0x00000400

    #define GETATTR_BTIME       0x00000800
    #define GETATTR_GEN         0x00001000
    #define GETATTR_DATA_VERSION 0x00002000

    #define GETATTR_BASIC       0x000007ff  /* Mask for fields up to BLOCKS */
    #define GETATTR_ALL         0x00003fff  /* Mask for All fields above */

Tsetattr.L = 26: tag[2] fid[4] valid[4] mode[4] uid[4] gid[4] size[8] \
               atime_sec[8] atime_nsec[8] mtime_sec[8] mtime_nsec[8]
Rsetattr.L: tag[2]
    setattr sets attributes of a file system object referenced by
    fid.  As with getattr, valid is a bitmask selecting which
    fields to set, which can be any combination of:

    mode - Linux chmod(2) mode bits.

    uid, gid - New owner, group of the file as described in Linux chown(2).

    size - New file size as handled by Linux truncate(2).

    atime_sec, atime_nsec - Time of last file access.

    mtime_sec, mtime_nsec - Time of last file modification.

    The valid bits are defined as follows:

    #define SETATTR_MODE        0x00000001
    #define SETATTR_UID         0x00000002
    #define SETATTR_GID         0x00000004
    #define SETATTR_SIZE        0x00000008
    #define SETATTR_ATIME       0x00000010
    #define SETATTR_MTIME       0x00000020
    #define SETATTR_CTIME       0x00000040
    #define SETATTR_ATIME_SET   0x00000080
    #define SETATTR_MTIME_SET   0x00000100

    If a time bit is set without the corresponding SET bit, the
    current system time on the server is used instead of the value
    sent in the request.

Txattrwalk.L = 30: tag[2] fid[4] newfid[4] name[s]
Rxattrwalk.L: tag[2] size[8]
    xattrwalk gets a newfid pointing to xattr name.  This fid can
    later be used to read the xattr value.  If name is NULL newfid
    can be used to get the list of extended attributes associated
    with the file system object.

Txattrcreate.L = 32: tag[2] fid[4] name[s] attr_size[8] flags[4]
Rxattrcreate.L: tag[2]
    xattrcreate gets a fid pointing to the xattr name.  This fid
    can later be used to set the xattr value.

    flag is derived from set Linux setxattr. The manpage says

        The flags parameter can be used to refine the semantics of
        the operation.  XATTR_CREATE specifies a pure create,
        which fails if the named attribute exists already.
        XATTR_REPLACE specifies a pure replace operation, which
        fails if the named attribute does not already exist.  By
        default (no flags), the extended attribute will be created
        if need be, or will simply replace the value if the
        attribute exists.

    The actual setxattr operation happens when the fid is clunked.
    At that point the written byte count and the attr_size
    specified in TXATTRCREATE should be same otherwise an error
    will be returned.

Treaddir.L = 40: tag[2] fid[4] offset[8] count[4]
Rreaddir.L: tag[2] count[4] data[count]
    readdir requests that the server return directory entries from
    the directory represented by fid, previously opened with
    lopen.  offset is zero on the first call.

    Directory entries are represented as variable-length records:
        qid[qid] offset[8] type[1] name[s]
    At most count bytes will be returned in data.  If count is not
    zero in the response, more data is available.  On subsequent
    calls, offset is the offset returned in the last directory
    entry of the previous call.

Tfsync.L = 50: tag[2] fid[4]
Rfsync.L: tag[2]
    fsync tells the server to flush any cached data associated
    with fid, previously opened with lopen.

Tlock.L = 52: tag[2] fid[4] type[1] flags[4] start[8] length[8] \
       proc_id[4] client_id[s]
Rlock.L: tag[2] status[1]
    lock is used to acquire or release a POSIX record lock on fid
    and has semantics similar to Linux fcntl(F_SETLK).

    type has one of the values:

        #define LOCK_TYPE_RDLCK 0
        #define LOCK_TYPE_WRLCK 1
        #define LOCK_TYPE_UNLCK 2

    start, length, and proc_id correspond to the analagous fields
    passed to Linux fcntl(F_SETLK):

    struct flock {
        short l_type;  /* Type of lock: F_RDLCK, F_WRLCK, F_UNLCK */
        short l_whence;/* How to intrprt l_start: SEEK_SET,SEEK_CUR,SEEK_END */
        off_t l_start; /* Starting offset for lock */
        off_t l_len;   /* Number of bytes to lock */
        pid_t l_pid;   /* PID of process blocking our lock (F_GETLK only) */
    };

    flags bits are:

        #define LOCK_SUCCESS    0
        #define LOCK_BLOCKED    1
        #define LOCK_ERROR      2
        #define LOCK_GRACE      3

    The Linux v9fs client implements the fcntl(F_SETLKW)
    (blocking) lock request by calling lock with
    LOCK_FLAGS_BLOCK set.  If the response is LOCK_BLOCKED,
    it retries the lock request in an interruptible loop until
    status is no longer LOCK_BLOCKED.

    The Linux v9fs client translates BSD advisory locks (flock) to
    whole-file POSIX record locks.  v9fs does not implement
    mandatory locks and will return ENOLCK if use is attempted.

    Because of POSIX record lock inheritance and upgrade
    properties, pass-through servers must be implemented
    carefully.

Tgetlock.L = 54: tag[2] fid[4] type[1] start[8] length[8] proc_id[4] \
               client_id[s]
Rgetlock.L: tag[2] type[1] start[8] length[8] proc_id[4] client_id[s]
    getlock tests for the existence of a POSIX record lock and has
    semantics similar to Linux fcntl(F_GETLK).

    As with lock, type has one of the values defined above, and
    start, length, and proc_id correspond to the analagous fields
    in struct flock passed to Linux fcntl(F_GETLK), and client_Id
    is an additional mechanism for uniquely identifying the lock
    requester and is set to the nodename by the Linux v9fs client.

Tlink.L = 70: tag[2] dfid[4] fid[4] name[s]
Rlink.L: tag[2]
    link creates a hard link name in directory dfid.  The link
    target is referenced by fid.

Tmkdir.L = 72: tag[2] dfid[4] name[s] mode[4] gid[4]
Rmkdir.L: tag[2] qid[qid]
    mkdir creates a new directory name in parent directory dfid.

    mode contains Linux mkdir(2) mode bits.

    gid is the effective group ID of the caller.

    The qid of the new directory is returned in the response.

Trenameat.L = 74: tag[2] olddirfid[4] oldname[s] newdirfid[4] newname[s]
Rrenameat.L: tag[2]
    Change the name of a file from oldname to newname, possible
    moving it from old directory represented by olddirfid to new
    directory represented by newdirfid.

    If the server returns ENOTSUPP, the client should fall back to
    the rename operation.

Tunlinkat.L = 76: tag[2] dirfd[4] name[s] flags[4]
Runlinkat.L: tag[2]
    Unlink name from directory represented by dirfd.  If the file
    is represented by a fid, that fid is not clunked.  If the
    server returns ENOTSUPP, the client should fall back to the
    remove operation.

    There seems to be only one defined flag:

        #define AT_REMOVEDIR    0x200

Tversion = 100: tag[2] msize[4] version[s]:auto
Rversion: tag[2] msize[4] version[s]

    negotiate protocol version

    version establishes the msize, which is the maximum message
    size inclusive of the size value that can be handled by both
    client and server.

    It also establishes the protocol version.  For 9P2000.L
    version must be the string 9P2000.L.

Tauth = 102: tag[2] afid[4] uname[s] aname[s] n_uname[4]
Rauth: tag[2] aqid[qid]
    auth initiates an authentication handshake for n_uname.
    Rlerror is returned if authentication is not required.  If
    successful, afid is used to read/write the authentication
    handshake (protocol does not specify what is read/written),
    and afid is presented in the attach.

Tattach = 104: tag[2] fid[4] afid[4] uname[s] aname[s] {.u: n_uname[4] }
Rattach: tag[2] qid[qid]
    attach introduces a new user to the server, and establishes
    fid as the root for that user on the file tree selected by
    aname.

    afid can be NOFID (~0) or the fid from a previous auth
    handshake.  The afid can be clunked immediately after the
    attach.

        #define NOFID       0xffffffff

    n_uname, if not set to NONUNAME (~0), is the uid of the
    user and is used in preference to uname.  Note that it appears
    in both .u and .L (unlike most .u-specific features).

        #define NONUNAME    0xffffffff

    v9fs has several modes of access which determine how it uses
    attach.  In the default access=user, an initial attach is sent
    for the user provided in the uname=name mount option, and for
    each user that accesses the file system thereafter.  For
    access=, only the initial attach is sent for and all other
    users are denied access by the client.

Rerror = 107: tag[2] errstr[s] {.u: errnum[4] }

Tflush = 108: tag[2] oldtag[2]
Rflush: tag[2]
    flush aborts an in-flight request referenced by oldtag, if any.

Twalk = 110: tag[2] fid[4] newfid[4] nwname[2] nwname*(wname[s])
Rwalk: tag[2] nwqid[2] nwqid*(wqid[qid])
    walk is used to descend a directory represented by fid using
    successive path elements provided in the wname array.  If
    succesful, newfid represents the new path.

    fid can be cloned to newfid by calling walk with nwname set to
    zero.

    if nwname==0, fid need not represent a directory.

Topen = 112: tag[2] fid[4] mode[1]
Ropen: tag[2] qid[qid] iounit[4]
    open prepares fid for file (or directory) I/O.

    mode is:
        #define OREAD       0   /* open for read */
        #define OWRITE      1   /* open for write */
        #define ORDWR       2   /* open for read and write */
        #define OEXEC       3   /* open for execute */

        #define OTRUNC      16  /* truncate (illegal if OEXEC) */
        #define OCEXEC      32  /* close on exec (nonsensical) */
        #define ORCLOSE     64  /* remove on close */
        #define ODIRECT     128 /* direct access (.u extension?) */

Tcreate = 114: tag[2] fid[4] name[s] perm[4] mode[1] {.u: extension[s] }
Rcreate: tag[2] qid[qid] iounit[4]
    create is similar to open; however, the incoming fid is the
    diretory in which the file is to be created, and on success,
    return, the fid refers to the then-created file.

Tread = 116: tag[2] fid[4] offset[8] count[4]
Rread: tag[2] count[4] data[count]
    perform a read on the file represented by fid.  Note that in
    v9fs, a read(2) or write(2) system call for a chunk of the
    file that won't fit in a single request is broken up into
    multiple requests.

    Under 9P2000.L, read cannot be used on directories.  See readdir.

Twrite = 118: tag[2] fid[4] offset[8] count[4] data[count]
Rwrite: tag[2] count[4]
    perform a write on the file represented by fid.  Note that in
    v9fs, a read(2) or write(2) system call for a chunk of the
    file that won't fit in a single request is broken up into
    multiple requests.

    write cannot be used on directories.

Tclunk = 120: tag[2] fid[4]
Rclunk: tag[2]
    clunk signifies that fid is no longer needed by the client.

Tremove = 122: tag[2] fid[4]
Rremove: tag[2]
    remove removes the file system object represented by fid.

    The fid is always clunked (even on error).

Tstat = 124: tag[2] fid[4]
Rstat: tag[2] size[2] data[size]

Twstat = 126: tag[2] fid[4] size[2] data[size]
Rwstat: tag[2]
"""

class _Token(object):
    r"""
    A scanned token.

    Tokens have a type (tok.ttype) and value (tok.value).  The value
    is generally the token itself, although sometimes a prefix and/or
    suffix has been removed (for 'label', 'word*', ':aux', and
    '[type]' tokens).  If prefix and/or suffix are removed, the full
    original token is
    in its .orig.

    Tokens are:
     - 'word', 'word*', or 'label':
         '[.\w]+' followed by optional '*' or ':':

     - 'aux': ':' followed by '\w+' (used for :auto annotation)

     - 'type':
       open bracket '[', followed by '\w+' or '\d+' (only one of these),
       followed by close bracket ']'

     - '(', ')', '{', '}': themeselves

    Each token can have arbitrary leading white space (which is
    discarded).

    (Probably should return ':' as a char and handle it in parser,
    but oh well.)
    """
    def __init__(self, ttype, value, orig=None):
        self.ttype = ttype
        self.value = value
        self.orig = value if orig is None else orig
        if self.ttype == 'type' and self.value.isdigit():
            self.ival = int(self.value)
        else:
            self.ival = None
    def __str__(self):
        return self.orig

_Token.tok_expr = re.compile(r'\s*([.\w]+(?:\*|:)?'
                             r'|:\w+'
                             r'|\[(?:\w+|\d+)\]'
                             r'|[(){}])')

def _scan(string):
    """
    Tokenize a string.

    Note: This raises a ValueError with the position of any unmatched
    character in the string.
    """
    tlist = []

    # make sure entire string is tokenized properly
    pos = 0
    for item in _Token.tok_expr.finditer(string):
        span = item.span()
        if span[0] != pos:
            print('error: unmatched character(s) in input\n{0}\n{1}^'.format(
                string, ' ' * pos))
            raise ValueError('unmatched lexeme', pos)
        pos = span[1]
        tlist.append(item.group(1))
    if pos != len(string):
        print('error: unmatched character(s) in input\n{0}\n{1}^'.format(
            string, ' ' * pos))
        raise ValueError('unmatched lexeme', pos)

    # classify each token, stripping decorations
    result = []
    for item in tlist:
        if item in ('(', ')', '{', '}'):
            tok = _Token(item, item)
        elif item[0] == ':':
            tok = _Token('aux', item[1:], item)
        elif item.endswith(':'):
            tok = _Token('label', item[0:-1], item)
        elif item.endswith('*'):
            tok = _Token('word*', item[0:-1], item)
        elif item[0] == '[':
            # integer or named type
            if item[-1] != ']':
                raise ValueError('internal error: "{0}" is not [...]'.format(
                    item))
            tok = _Token('type', item[1:-1], item)
        else:
            tok = _Token('word', item)
        result.append(tok)
    return result

def _debug_print_sequencer(seq):
    """for debugging"""
    print('sequencer is {0!r}'.format(seq), file=sys.stderr)
    for i, enc in enumerate(seq):
        print(' [{0:d}] = {1}'.format(i, enc), file=sys.stderr)

def _parse_expr(seq, string, typedefs):
    """
    Parse "expression-ish" items, which is a list of:
        name[type]
        name*(subexpr)    (a literal asterisk)
        { label ... }

    The "type" may be an integer or a second name.  In the case
    of a second name it must be something from <typedefs>.

    The meaning of name[integer] is that we are going to encode
    or decode a fixed-size field of <integer> bytes, using the
    given name.

    For name[name2], we can look up name2 in our typedefs table.
    The only real typedefs's used here are "stat" and "s"; each
    of these expands to a variable-size encode/decode.  See the
    special case below, though.

    The meaning of name*(...) is: the earlier name will have been
    defined by an earlier _parse_expr for this same line.  That
    earlier name provides a repeat-count.

    Inside the parens we get a name[type] sub-expressino.  This may
    not recurse further, so we can use a pretty cheesy parser.

    As a special case, given name[name2], we first check whether
    name2 is an earlier name a la name*(...).  Here the meaning
    is much like name2*(name[1]), except that the result is a
    simple byte string, rather than an array.

    The meaning of "{ label ... " is that everything following up
    to "}" is optional and used only with 9P2000.u and/or 9P2000.L.
    Inside the {...} pair is the usual set of tokens, but again
    {...} cannot recurse.

    The parse fills in a Sequencer instance, and returns a list
    of the parsed names.
    """
    names = []
    cond = None

    tokens = collections.deque(_scan(string))

    def get_subscripted(tokens):
        """
        Allows name[integer] and name1[name2] only; returns
        tuple after stripping off both tokens, or returns None
        and does not strip tokens.
        """
        if len(tokens) == 0 or tokens[0].ttype != 'word':
            return None
        if len(tokens) > 1 and tokens[1].ttype == 'type':
            word = tokens.popleft()
            return word, tokens.popleft()
        return None

    def lookup(name, typeinfo, aux=None):
        """
        Convert cond (if not None) to its .value, so that instead
        of (x, '.u') we get '.u'.

        Convert typeinfo to an encdec.  Typeinfo may be 1/2/4/8, or
        one of our typedef names.  If it's a typedef name it will
        normally correspond to an EncDecTyped, but we have one special
        case for string types, and another for using an earlier-defined
        variable.
        """
        condval = None if cond is None else cond.value
        if typeinfo.ival is None:
            try:
                cls, sub = typedefs[typeinfo.value]
            except KeyError:
                raise ValueError('unknown type name {0}'.format(typeinfo))
            # the type name is typeinfo.value; the corresponding
            # pfod class is cls; the *variable* name is name;
            # and the sub-sequence is sub.  But if cls is None
            # then it's our string type.
            if cls is None:
                encdec = sequencer.EncDecSimple(name, _STRING_MAGIC, aux)
            else:
                encdec = sequencer.EncDecTyped(cls, name, sub, aux)
        else:
            if typeinfo.ival not in (1, 2, 4, 8):
                raise ValueError('bad integer code in {0}'.format(typeinfo))
            encdec = sequencer.EncDecSimple(name, typeinfo.ival, aux)
        return condval, encdec

    def emit_simple(name, typeinfo, aux=None):
        """
        Emit name[type].  We may be inside a conditional; if so
        cond is not None.
        """
        condval, encdec = lookup(name, typeinfo, aux)
        seq.append_encdec(condval, encdec)
        names.append(name)

    def emit_repeat(name1, name2, typeinfo):
        """
        Emit name1*(name2[type]).

        Note that the conditional is buried in the sub-coder for
        name2.  It must be passed through anyway in case the sub-
        coder is only partly conditional.  If the sub-coder is
        fully conditional, each sub-coding uses or produces no
        bytes and hence the array itself is effectively conditional
        as well (it becomes name1 * [None]).

        We don't (currently) have any auxiliary data for arrays.
        """
        if name1 not in names:
            raise ValueError('{0}*({1}[{2}]): '
                             '{0} undefined'.format(name1, name2,
                                                    typeinfo.value))
        condval, encdec = lookup(name2, typeinfo)
        encdec = sequencer.EncDecA(name1, name2, encdec)
        seq.append_encdec(condval, encdec)
        names.append(name2)

    def emit_bytes_repeat(name1, name2):
        """
        Emit name1[name2], e.g., data[count].
        """
        condval = None if cond is None else cond.value
        # Note that the two names are reversed when compared to
        # count*(data[type]).  The "sub-coder" is handled directly
        # by EncDecA, hence is None.
        #
        # As a peculiar side effect, all bytes-repeats cause the
        # count itself to become automatic (to have an aux of 'len').
        encdec = sequencer.EncDecA(name2, name1, None, 'len')
        seq.append_encdec(condval, encdec)
        names.append(name1)

    supported_conditions = ('.u')
    while tokens:
        token = tokens.popleft()
        if token.ttype == 'label':
            raise ValueError('misplaced label')
        if token.ttype == 'aux':
            raise ValueError('misplaced auxiliary')
        if token.ttype == '{':
            if cond is not None:
                raise ValueError('nested "{"')
            if len(tokens) == 0:
                raise ValueError('unclosed "{"')
            cond = tokens.popleft()
            if cond.ttype != 'label':
                raise ValueError('"{" not followed by cond label')
            if cond.value not in supported_conditions:
                raise ValueError('unsupported condition "{0}"'.format(
                    cond.value))
            continue
        if token.ttype == '}':
            if cond is None:
                raise ValueError('closing "}" w/o opening "{"')
            cond = None
            continue
        if token.ttype == 'word*':
            if len(tokens) == 0 or tokens[0].ttype != '(':
                raise ValueError('{0} not followed by (...)'.format(token))
            tokens.popleft()
            repeat = get_subscripted(tokens)
            if repeat is None:
                raise ValueError('parse error after {0}('.format(token))
            if len(tokens) == 0 or tokens[0].ttype != ')':
                raise ValueError('missing ")" after {0}({1}{2}'.format(
                    token, repeat[0], repeat[1]))
            tokens.popleft()
            # N.B.: a repeat cannot have an auxiliary info (yet?).
            emit_repeat(token.value, repeat[0].value, repeat[1])
            continue
        if token.ttype == 'word':
            # Special case: _STRING_MAGIC turns into a string
            # sequencer.  This should be used with just one
            # typedef (typedef s: _string_).
            if token.value == _STRING_MAGIC:
                names.append(_STRING_MAGIC) # XXX temporary
                continue
            if len(tokens) == 0 or tokens[0].ttype != 'type':
                raise ValueError('parse error after {0}'.format(token))
            type_or_size = tokens.popleft()
            # Check for name[name2] where name2 is a word (not a
            # number) that is in the names[] array.
            if type_or_size.value in names:
                # NB: this cannot have auxiliary info.
                emit_bytes_repeat(token.value, type_or_size.value)
                continue
            if len(tokens) > 0 and tokens[0].ttype == 'aux':
                aux = tokens.popleft()
                if aux.value != 'auto':
                    raise ValueError('{0}{1}: only know "auto", not '
                                     '{2}'.format(token, type_or_size,
                                                  aux.value))
                emit_simple(token.value, type_or_size, aux.value)
            else:
                emit_simple(token.value, type_or_size)
            continue
        raise ValueError('"{0}" not valid here"'.format(token))

    if cond is not None:
        raise ValueError('unclosed "}"')

    return names

class _ProtoDefs(object):
    def __init__(self):
        # Scan our typedefs. This may execute '#define's as well.
        self.typedefs = {}
        self.defines = {}
        typedef_re = re.compile(r'\s*typedef\s+(\w+)\s*:\s*(.*)')
        self.parse_lines('SDesc', SDesc, typedef_re, self.handle_typedef)
        self.parse_lines('QIDDesc', QIDDesc, typedef_re, self.handle_typedef)
        self.parse_lines('STATDesc', STATDesc, typedef_re, self.handle_typedef)
        self.parse_lines('WirestatDesc', WirestatDesc, typedef_re,
                         self.handle_typedef)
        self.parse_lines('DirentDesc', DirentDesc, typedef_re,
                         self.handle_typedef)

        # Scan protocol (the bulk of the work).  This, too, may
        # execute '#define's.
        self.protocol = {}
        proto_re = re.compile(r'(\*?\w+)(\.\w+)?\s*(?:=\s*(\d+))?\s*:\s*(.*)')
        self.prev_proto_value = None
        self.parse_lines('ProtocolDesc', ProtocolDesc,
                         proto_re, self.handle_proto_def)

        self.setup_header()

        # set these up for export()
        self.plain = {}
        self.dotu = {}
        self.dotl = {}

    def parse_lines(self, name, text, regexp, match_handler):
        """
        Parse a sequence of lines.  Match each line using the
        given regexp, or (first) as a #define line.  Note that
        indented lines are either #defines or are commentary!

        If hnadling raises a ValueError, we complain and include
        the appropriate line offset.  Then we sys.exit(1) (!).
        """
        define = re.compile(r'\s*#define\s+(\w+)\s+([^/]*)'
                            r'(\s*/\*.*\*/)?\s*$')
        for lineoff, line in enumerate(text.splitlines()):
            try:
                match = define.match(line)
                if match:
                    self.handle_define(*match.groups())
                    continue
                match = regexp.match(line)
                if match:
                    match_handler(*match.groups())
                    continue
                if len(line) and not line[0].isspace():
                    raise ValueError('unhandled line: {0}'.format(line))
            except ValueError as err:
                print('Internal error while parsing {0}:\n'
                      '    {1}\n'
                      '(at line offset +{2}, discounting \\-newline)\n'
                      'The original line in question reads:\n'
                      '{3}'.format(name, err.args[0], lineoff, line),
                      file=sys.stderr)
                sys.exit(1)

    def handle_define(self, name, value, comment):
        """
        Handle #define match.

        The regexp has three fields, matching the name, value,
        and possibly-empty comment; these are our arguments.
        """
        # Obnoxious: int(,0) requires new 0o syntax in py3k;
        # work around by trying twice, once with base 0, then again
        # with explicit base 8 if the first attempt fails.
        try:
            value = int(value, 0)
        except ValueError:
            value = int(value, 8)
        if DEBUG:
            print('define: defining {0} as {1:x}'.format(name, value),
                  file=sys.stderr)
        if name in self.defines:
            raise ValueError('redefining {0}'.format(name))
        self.defines[name] = (value, comment)

    def handle_typedef(self, name, expr):
        """
        Handle typedef match.

        The regexp has just two fields, the name and the expression
        to parse (note that the expression must fit all on one line,
        using backslach-newline if needed).

        Typedefs may refer back to existing typedefs, so we pass
        self.typedefs to _parse_expr().
        """
        seq = sequencer.Sequencer(name)
        fields = _parse_expr(seq, expr, self.typedefs)
        # Check for special string magic typedef.  (The name
        # probably should be just 's' but we won't check that
        # here.)
        if len(fields) == 1 and fields[0] == _STRING_MAGIC:
            cls = None
        else:
            cls = pfod.pfod(name, fields)
        if DEBUG:
            print('typedef: {0} = {1!r}; '.format(name, fields),
                  end='', file=sys.stderr)
            _debug_print_sequencer(seq)
        if name in self.typedefs:
            raise ValueError('redefining {0}'.format(name))
        self.typedefs[name] = cls, seq

    def handle_proto_def(self, name, proto_version, value, expr):
        """
        Handle protocol definition.

        The regexp matched:
        - The name of the protocol option such as Tversion,
          Rversion, Rlerror, etc.
        - The protocol version, if any (.u or .L).
        - The value, if specified.  If no value is specified
          we use "the next value".
        - The expression to parse.

        As with typedefs, the expression must fit all on one
        line.
        """
        if value:
            value = int(value)
        elif self.prev_proto_value is not None:
            value = self.prev_proto_value + 1
        else:
            raise ValueError('{0}: missing protocol value'.format(name))
        if value < 0 or value > 255:
            raise ValueError('{0}: protocol value {1} out of '
                             'range'.format(name, value))
        self.prev_proto_value = value

        seq = sequencer.Sequencer(name)
        fields = _parse_expr(seq, expr, self.typedefs)
        cls = pfod.pfod(name, fields)
        if DEBUG:
            print('proto: {0} = {1}; '.format(name, value),
                  end='', file=sys.stderr)
            _debug_print_sequencer(seq)
        if name in self.protocol:
            raise ValueError('redefining {0}'.format(name))
        self.protocol[name] = cls, value, proto_version, seq

    def setup_header(self):
        """
        Handle header definition.

        This is a bit gimmicky and uses some special cases,
        because data is sized to dsize which is effectively
        just size - 5.  We can't express this in our mini language,
        so we just hard-code the sequencer and pfod.

        In addition, the unpacker never gets the original packet's
        size field, only the fcall and the data.
        """
        self.header_pfod = pfod.pfod('Header', 'size dsize fcall data')

        seq = sequencer.Sequencer('Header-pack')
        # size: 4 bytes
        seq.append_encdec(None, sequencer.EncDecSimple('size', 4, None))
        # fcall: 1 byte
        seq.append_encdec(None, sequencer.EncDecSimple('fcall', 1, None))
        # data: string of length dsize
        seq.append_encdec(None, sequencer.EncDecA('dsize', 'data', None))
        if DEBUG:
            print('Header-pack:', file=sys.stderr)
            _debug_print_sequencer(seq)
        self.header_pack_seq = seq

        seq = sequencer.Sequencer('Header-unpack')
        seq.append_encdec(None, sequencer.EncDecSimple('fcall', 1, None))
        seq.append_encdec(None, sequencer.EncDecA('dsize', 'data', None))
        if DEBUG:
            print('Header-unpack:', file=sys.stderr)
            _debug_print_sequencer(seq)
        self.header_unpack_seq = seq

    def export(self, mod):
        """
        Dump results of internal parsing process
        into our module namespace.

        Note that we do not export the 's' typedef, which
        did not define a data structure.

        Check for name collisions while we're at it.
        """
        namespace = type('td', (object,), {})

        # Export the typedefs (qid, stat).
        setattr(mod, 'td', namespace)
        for key in self.typedefs:
            cls = self.typedefs[key][0]
            if cls is None:
                continue
            setattr(namespace, key, cls)

        # Export two sequencers for en/decoding stat fields
        # (needed for reading directories and doing Twstat).
        setattr(namespace, 'stat_seq', self.typedefs['stat'][1])
        setattr(namespace, 'wirestat_seq', self.typedefs['wirestat'][1])

        # Export the similar dirent decoder.
        setattr(namespace, 'dirent_seq', self.typedefs['dirent'][1])

        # Export the #define values
        for key, val in self.defines.items():
            if hasattr(namespace, key):
                print('{0!r} is both a #define and a typedef'.format(key))
                raise AssertionError('bad internal names')
            setattr(namespace, key, val[0])

        # Export Tattach, Rattach, Twrite, Rversion, etc values.
        # Set up fcall_names[] table to map from value back to name.
        # We also map fcall names to themselves, so given either a
        # name or a byte code we can find out whether it's a valid
        # fcall.
        for key, val in self.protocol.items():
            if hasattr(namespace, key):
                prev_def = '#define' if key in self.defines else 'typedef'
                print('{0!r} is both a {1} and a protocol '
                      'value'.format(key, prev_def))
                raise AssertionError('bad internal names')
            setattr(namespace, key, val[1])
            fcall_names[key] = key
            fcall_names[val[1]] = key

        # Hook up PFOD's for each protocol object -- for
        # Tversion/Rversion, Twrite/Rwrite, Tlopen/Rlopen, etc.
        # They go in the rrd name-space, and also in dictionaries
        # per-protocol here, with the lookup pointing to a _PackInfo
        # for the corresponding sequencer.
        #
        # Note that each protocol PFOD is optionally annotated with
        # its specific version.  We know that .L > .u > plain; but
        # all the "lesser" PFODs are available to all "greater"
        # protocols at all times.
        #
        # (This is sort-of-wrong for Rerror vs Rlerror, but we
        # don't bother to exclude Rerror from .L.)
        #
        # The PFODs themselves were already created, at parse time.
        namespace = type('rrd', (object,), {})
        setattr(mod, 'rrd', namespace)
        for key, val in self.protocol.items():
            cls = val[0]
            proto_version = val[2]
            seq = val[3]
            packinfo = _PackInfo(seq)
            if proto_version is None:
                # all three protocols have it
                self.plain[cls] = packinfo
                self.dotu[cls] = packinfo
                self.dotl[cls] = packinfo
            elif proto_version == '.u':
                # only .u and .L have it
                self.dotu[cls] = packinfo
                self.dotl[cls] = packinfo
            elif proto_version == '.L':
                # only .L has it
                self.dotl[cls] = packinfo
            else:
                raise AssertionError('unknown protocol {1} for '
                                     '{0}'.format(key, proto_version))
            setattr(namespace, key, cls)

_9p_data = _ProtoDefs()
_9p_data.export(sys.modules[__name__])

# Currently we look up by text-string, in lowercase.
_9p_versions = {
    '9p2000': _P9Proto({'version': '9P2000'},
                       {'.u': False},
                       _9p_data,
                       _9p_data.plain,
                       0),
    '9p2000.u': _P9Proto({'version': '9P2000.u'},
                         {'.u': True},
                         _9p_data,
                         _9p_data.dotu,
                         1),
    '9p2000.l': _P9Proto({'version': '9P2000.L'},
                         {'.u': True},
                         _9p_data,
                         _9p_data.dotl,
                         2),
}
def p9_version(vers_string):
    """
    Return protocol implementation of given version.  Raises
    KeyError if the version is invalid.  Note that the KeyError
    will be on a string-ified, lower-cased version of the vers_string
    argument, even if it comes in as a bytes instance in py3k.
    """
    if not isinstance(vers_string, str) and isinstance(vers_string, bytes):
        vers_string = vers_string.decode('utf-8', 'surrogateescape')
    return _9p_versions[vers_string.lower()]

plain = p9_version('9p2000')
dotu = p9_version('9p2000.u')
dotl = p9_version('9p2000.L')

def qid_type2name(qidtype):
    """
    Convert qid type field to printable string.

    >>> qid_type2name(td.QTDIR)
    'dir'
    >>> qid_type2name(td.QTAPPEND)
    'append-only'
    >>> qid_type2name(0xff)
    'invalid(0xff)'
    """
    try:
        # Is it ever OK to have multiple bits set,
        # e.g., both QTAPPEND and QTEXCL?
        return {
            td.QTDIR: 'dir',
            td.QTAPPEND: 'append-only',
            td.QTEXCL: 'exclusive',
            td.QTMOUNT: 'mount',
            td.QTAUTH: 'auth',
            td.QTTMP: 'tmp',
            td.QTSYMLINK: 'symlink',
            td.QTFILE: 'file',
        }[qidtype]
    except KeyError:
        pass
    return 'invalid({0:#x})'.format(qidtype)

if __name__ == '__main__':
    import doctest
    doctest.testmod()
