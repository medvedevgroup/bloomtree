#!/bin/bash

uncompressedTree="$1"    # e.g. sbt.txt
allsomeTree="$2"         # e.g. sbt-as.txt
rrrCompressedTree="$3"   # e.g. sbt-rrr-allsome.txt

# compress the SBT-AS bit vectors, creating the node files for a compressed
# ABT-AS

cat ${uncompressedTree} \
  | tr -d "*" \
  | sed "s/,.*//" \
  | sed "s/\.bf\.bv//" \
  | while read node ; do
      echo "=== compressing split ${node} to allsome ==="
      bt compress-rrr-double example.hashfile \
        ${node}.bf-all.bv \
        ${node}.bf-some.bv \
        ${node}.bf-allsome.bv
      done

# create the topology file for the compressed ABT-AS.

cat ${uncompressedTree} \
  | sed "s/\.bf\.bv/.bf-allsome.bv.rrr/" \
  > ${rrrCompressedTree}
