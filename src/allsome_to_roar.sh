#!/bin/bash

uncompressedTree="$1"    # e.g. sbt.txt
allsomeTree="$2"         # e.g. sbt-as.txt
roarCompressedTree="$3"  # e.g. sbt-roar-allsome.txt

# compress the SBT-AS bit vectors, creating the node files for a compressed
# ABT-AS

cat ${uncompressedTree} \
  | tr -d "*" \
  | sed "s/,.*//" \
  | sed "s/\.bf\.bv//" \
  | while read node ; do
      echo "=== ROAR-compressing split ${node} ==="
      bt compress-roar-single example.hashfile ${node}.bf-all.bv
      bt compress-roar-single example.hashfile ${node}.bf-some.bv
      done

# create the topology file for the compressed ABT-AS.

cat ${allsomeTree} \
  | sed "s/\.bv/.bv.roar/g" \
  > ${roarCompressedTree}
