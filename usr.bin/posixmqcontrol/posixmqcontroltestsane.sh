#!/bin/sh
# test for 'insane' queue names.

subject='posixmqcontrol'

# does sanity check enforce leading slash?
${subject} info -q missing.leading.slash 2>/dev/null
code=$?
if [ $code != 64 ]; then
  exit 1
fi

# does sanity check enforce one and only one slash?
${subject} info -q /to/many/slashes 2>/dev/null
code=$?
if [ $code != 64 ]; then
  exit 1
fi

# does sanity check enforce length limit?
${subject} info -q /this.queue.name.is.way.too.long.at.more.than.one.thousand.and.twenty.four.characters.long.because.nobody.needs.to.type.out.something.this.ridiculously.long.than.just.goes.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on.and.on 2>/dev/null
code=$?
if [ $code != 64 ]; then
  exit 1
fi

echo "Pass!"
exit 0
