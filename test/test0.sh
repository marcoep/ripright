#!/bin/bash
#
# Simply test that the FLAC library is really lossless.
#

FLAC=`which flac 2> /dev/null`

if [ -z "$FLAC" ] ; then
  echo "Could not find 'flac' standalone encoder/decoder.  Skipping test."
  exit 0
fi

for LEVEL in {0..8} ; do

    rm -f rand.raw rand.flac rand.raw.md5

    dd if=/dev/urandom of=rand.raw bs=4096 count=10 &> /dev/null &&
    $FLAC -$LEVEL -s --force-raw-format --endian=little --sign=signed --channels=2 --bps=16 --sample-rate=44100 rand.raw &&
    md5sum rand.raw > rand.raw.md5 &&
    rm rand.raw &&
    $FLAC -s --decode --force-raw-format --endian=little --sign=signed rand.flac &&
    md5sum --status -c rand.raw.md5 ||
    exit 1

done

rm rand.raw.md5 rand.flac rand.raw

# END OF SCRIPT

