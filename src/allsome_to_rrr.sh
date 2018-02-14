#!/bin/bash

uncompressedTree="$1"    # e.g. sbt.txt
allsomeTree="$2"         # e.g. sbt-as.txt
rrrCompressedTree="$3"   # e.g. sbt-rrr-allsome.txt

hashfile="example.hashfile"

# compress the SBT-AS bit vectors, creating the node files for a compressed
# SBT-AS

nodeNum=0
cat ${uncompressedTree} \
  | tr -d "*" \
  | sed "s/,.*//" \
  | sed "s/\.bf\.bv//" \
  | while read node ; do
      nodeNum=$((nodeNum+1))
      echo "=== #${nodeNum} RRR-compressing split ${node} to single-file allsome ==="
      bt compress-rrr-double ${hashfile} \
        ${node}.bf-all.bv \
        ${node}.bf-some.bv \
        ${node}.bf-allsome.bv
      done

# create the topology file for the compressed SBT-AS.

cat ${uncompressedTree} \
  | sed "s/\.bf\.bv/.bf-allsome.bv.rrr/" \
  > ${rrrCompressedTree}
