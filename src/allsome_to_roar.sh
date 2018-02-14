#!/bin/bash

uncompressedTree="$1"    # e.g. sbt.txt
allsomeTree="$2"         # e.g. sbt-as.txt
roarCompressedTree="$3"  # e.g. sbt-roar-allsome.txt

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
      echo "=== #${nodeNum} ROAR-compressing split ${node} ==="
      bt compress-roar-single ${hashfile} ${node}.bf-all.bv
      bt compress-roar-single ${hashfile} ${node}.bf-some.bv
      done

# create the topology file for the compressed SBT-AS.

cat ${allsomeTree} \
  | sed "s/\.bv/.bv.roar/g" \
  > ${roarCompressedTree}
