#! /usr/bin/env python

"""
Integer number allocator.

Basically, these keep track of a set of allocatable values in
some range (you provide min and max) and let you allocate out of
the range and return values into the range.

You may pick a value using "next since last time", or "next
available after provided value".  Note that next-after will
wrap around as needed (modular arithmetic style).

The free lists are thread-locked so that this code can be used
with threads.

    >>> a = NumAlloc(5, 10) # note closed interval: 5..10 inclusive
    >>> a
    NumAlloc(5, 10)
    >>> a.avail
    [[5, 10]]
    >>> a.alloc()
    5
    >>> a.avail
    [[6, 10]]
    >>> a.alloc(8)
    8
    >>> a.avail
    [[6, 7], [9, 10]]
    >>> a.free(5)
    >>> a.avail
    [[5, 7], [9, 10]]
    >>> a.free(8)
    >>> a.avail
    [[5, 10]]

Attempting to free a value that is already free is an error:

    >>> a.free(5)
    Traceback (most recent call last):
       ...
    ValueError: free: 5 already available

You can, however, free a value that is outside the min/max
range.  You can also free multiple values at once:

    >>> a.free_multi([0, 1, 2, 4])
    >>> a.avail
    [[0, 2], [4, 10]]
    >>> a.free_multi([3, 12])
    >>> a.avail
    [[0, 10], [12, 12]]

Note that this changes the min/max values:

    >>> a
    NumAlloc(0, 12)

To prevent adding values outside the min/max range, create the
NumArray with autoextend=False, or set .autoextend=False at any
time:

    >>> a.autoextend = False
    >>> a
    NumAlloc(0, 12, autoextend=False)
    >>> a.free(13)
    Traceback (most recent call last):
       ...
    ValueError: free: 13 is outside range limit

You can create an empty range, which is really only useful once
you free values into it:

    >>> r = NumAlloc(0, -1)
    >>> r
    NumAlloc(0, -1)
    >>> r.alloc() is None
    True
    >>> r.free_multi(range(50))
    >>> r
    NumAlloc(0, 49)

Note that r.alloc() starts from where you last left off, even if
you've freed a value:

    >>> r.alloc()
    0
    >>> r.free(0)
    >>> r.alloc()
    1

Of course, in multithreaded code you can't really depend on this
since it will race other threads.  Still, it generally makes for
efficient allocation.  To force allocation to start from the
range's minimum, provide the minimum (e.g., r.min_val) as an
argument to r.alloc():

    >>> r.alloc()
    2
    >>> r.alloc(r.min_val)
    0

Providing a number to alloc() tries to allocate that number,
but wraps around to the next one if needed:

    >>> r.alloc(49)
    49
    >>> r.alloc(49)
    3
    >>> r.alloc(99999)
    4
    >>> r.avail
    [[5, 48]]

There is currently no way to find all allocated values, although
the obvious method (going through r.avail) will work.  Any iterator
would not be thread-safe.
"""

import threading

class NumAlloc(object):
    """
    Number allocator object.
    """
    def __init__(self, min_val, max_val, autoextend=True):
        self.min_val = min_val
        self.max_val = max_val
        if min_val <= max_val:
            self.avail = [[min_val, max_val]]
        else:
            self.avail = []
        self.autoextend = autoextend
        self.last = None
        self.lock = threading.Lock()

    def __repr__(self):
        myname = self.__class__.__name__
        if self.autoextend:
            ae = ''
        else:
            ae = ', autoextend=False'
        return '{0}({1}, {2}{3})'.format(myname, self.min_val, self.max_val, ae)

    def _find_block(self, val):
        """
        Find the block that contains val, or that should contain val.
        Remember that self.avail is a list of avaliable ranges of
        the form [[min1, max1], [min2, max2], ..., [minN, maxN]]
        where max1 < min2, max2 < min3, ..., < minN.

        The input value either falls into one of the available
        blocks, or falls into a gap between two available blocks.
        We want to know which block it goes in, or if it goes
        between two, which block it comes before.

        We can do a binary search to find this block.  When we
        find it, return its index and its values.

        If we find that val is not in a block, return the position
        where the value should go, were it to be put into a new
        block by itself.  E.g., suppose val is 17, and there is a
        block [14,16] and a block [18,20]. We would make this
        [14,16],[17,17],[18,20] by inserting [17,17] between them.
        (Afterward, we will want to fuse all three blocks to make
        [14,18].  However, if we insert as block 0, e.g., if the
        list starts with [18,20] and we insert to get
        [17,17][18,20], we really end up just modifying block 0 to
        [17,20].  Or, if we insert as the new final block, we
        might end up modifying the last block.)
        """
        low = 0
        high = len(self.avail) - 1
        while low <= high:
            mid = low + ((high - low) // 2)
            pair = self.avail[mid]
            if val < pair[0]:
                # must go before block mid
                high = mid - 1
            elif val > pair[1]:
                # must go after block mid
                low = mid + 1
            else:
                # val >= first and val <= last, so we found it
                return mid, pair
        # Low > high: no block actually contains val, or
        # there are no blocks at all.  If there are no blocks,
        # return block #0 and None.  Otherwise return the
        return low, None

    def alloc(self, val=None):
        """
        Get new available value.

        If val is None, we start from the most recently
        allocated value, plus 1.

        If val is a numeric value, we start from that value.
        Hence, since the range is min_val..max_val, you can
        provide min_val to take the first available value.

        This may return None, if no values are still available.
        """
        with self.lock:
            if val is None:
                val = self.last + 1 if self.last is not None else self.min_val
            if val is None or val > self.max_val or val < self.min_val:
                val = self.min_val
            i, pair = self._find_block(val)
            if pair is None:
                # Value is is not available.  The next
                # available value that is greater than val
                # is in the block right after block i.
                # If there is no block after i, the next
                # available value is in block 0.  If there
                # is no block 0, there are no available
                # values.
                nblocks = len(self.avail)
                i += 1
                if i >= nblocks:
                    if nblocks == 0:
                        return None
                    i = 0
                pair = self.avail[i]
                val = pair[0]
            # Value val is available - take it.
            #
            # There are four special cases to handle.
            #
            # 1. pair[0] < val < pair[1]: split the pair.
            # 2. pair[0] == val < pair[1]: increase pair[0].
            # 3. pair[0] == val == pair[1]: delete the pair
            # 4. pair[0] < val == pair[1]: decrease pair[1].
            assert pair[0] <= val <= pair[1]
            if pair[0] == val:
                # case 2 or 3: Take the left edge or delete the pair.
                if val == pair[1]:
                    del self.avail[i]
                else:
                    pair[0] = val + 1
            else:
                # case 1 or 4: split the pair or take the right edge.
                if val == pair[1]:
                    pair[1] = val - 1
                else:
                    newpair = [val + 1, pair[1]]
                    pair[1] = val - 1
                    self.avail.insert(i + 1, newpair)
            self.last = val
            return val

    def free(self, val):
        "Free one value"
        self._free_multi('free', [val])

    def free_multi(self, values):
        "Free many values (provide any iterable)"
        values = list(values)
        values.sort()
        self._free_multi('free_multi', values)

    def _free_multi(self, how, values):
        """
        Free a (sorted) list of values.
        """
        if len(values) == 0:
            return
        with self.lock:
            while values:
                # Take highest value, and any contiguous lower values.
                # Note that it can be significantly faster this way
                # since coalesced ranges make for shorter copies.
                highval = values.pop()
                val = highval
                while len(values) and values[-1] == val - 1:
                    val = values.pop()
                self._free_range(how, val, highval)

    def _maybe_increase_max(self, how, val):
        """
        If needed, widen our range to include new high val -- i.e.,
        possibly increase self.max_val.  Do nothing if this is not a
        new all time high; fail if we have autoextend disabled.
        """
        if val <= self.max_val:
            return
        if self.autoextend:
            self.max_val = val
            return
        raise ValueError('{0}: {1} is outside range limit'.format(how, val))

    def _maybe_decrease_min(self, how, val):
        """
        If needed, widen our range to include new low val -- i.e.,
        possibly decrease self.min_val.  Do nothing if this is not a
        new all time low; fail if we have autoextend disabled.
        """
        if val >= self.min_val:
            return
        if self.autoextend:
            self.min_val = val
            return
        raise ValueError('{0}: {1} is outside range limit'.format(how, val))

    def _free_range(self, how, val, highval):
        """
        Free the range [val..highval].  Note, val==highval it's just
        a one-element range.

        The lock is already held.
        """
        # Find the place to store the lower value.
        # We should never find an actual pair here.
        i, pair = self._find_block(val)
        if pair:
            raise ValueError('{0}: {1} already available'.format(how, val))
        # If we're freeing a range, check that the high val
        # does not span into the *next* range, either.
        if highval > val and i < len(self.avail):
            if self.avail[i][0] <= highval:
                raise ValueError('{0}: {2} (from {{1}..{2}) already '
                                 'available'.format(how, val, highval))

        # We'll need to insert a block and perhaps fuse it
        # with blocks before and/or after.  First, check
        # whether there *is* a before and/or after, and find
        # their corresponding edges and whether we abut them.
        if i > 0:
            abuts_below = self.avail[i - 1][1] + 1 == val
        else:
            abuts_below = False
        if i < len(self.avail):
            abuts_above = self.avail[i][0] - 1 == highval
        else:
            abuts_above = False
        # Now there are these four cases:
        # 1. abuts below and above: fuse the two blocks.
        # 2. abuts below only: adjust previous (i-1'th) block
        # 3. abuts above only: adjust next (i'th) block
        # 4. doesn't abut: insert new block
        if abuts_below:
            if abuts_above:
                # case 1
                self.avail[i - 1][1] = self.avail[i][1]
                del self.avail[i]
            else:
                # case 2
                self._maybe_increase_max(how, highval)
                self.avail[i - 1][1] = highval
        else:
            if abuts_above:
                # case 3
                self._maybe_decrease_min(how, val)
                self.avail[i][0] = val
            else:
                # case 4
                self._maybe_decrease_min(how, val)
                self._maybe_increase_max(how, highval)
                newblock = [val, highval]
                self.avail.insert(i, newblock)

if __name__ == '__main__':
    import doctest
    import sys

    doctest.testmod()
    if sys.version_info[0] >= 3:
        xrange = range
    # run some worst case tests
    # NB: coalesce is terribly slow when done bottom up
    r = NumAlloc(0, 2**16 - 1)
    for i in xrange(r.min_val, r.max_val, 2):
        r.alloc(i)
    print('worst case alloc: len(r.avail) = {0}'.format(len(r.avail)))
    for i in xrange(r.max_val - 1, r.min_val, -2):
        r.free(i)
    print('free again; len(r.avail) should be 1; is {0}'.format(len(r.avail)))
    if len(r.avail) != 1:
        sys.exit('failure')
