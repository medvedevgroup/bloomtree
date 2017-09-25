# Tutorial, Creating an Allsome Bloomtree

(1) Initialize the hash function.

```bash  
bt hashes --k 20 example.hashfile
```

(2) Estimate the best bloom filter size.  Note that kmerSize 20 is hardwired in
get_bfsize.sh.

```bash  
ls experiment*.fa \
  | while read f ; do
      echo "=== gzipping ${f} ==="
      cat ${f} | gzip > ${f}.gz
      done
ls experiment*.fa.gz > experimentfiles
../src/get_bfsize.sh experimentfiles
```

Output from get_bfsize.sh:  
```bash  
    Unique canonical kmer counts by number of occurences:
    1 4906564
    2 963379
    3 499717
```

So we want to use a bf_size of about 500,000.

(3) Convert the fasta files to bloom filter bit vectors.

```bash  
bf_size=500000
ls experiment*.fa \
  | while read f ; do
      bv=`echo ${f} | sed "s/\.fa/.bf.bv/"`
      echo "=== converting ${f} to ${bv} ==="
      bt count --cutoff 3 example.hashfile ${bf_size} ${f} ${bv}
      done
```

(4) Build an uncompressed SBT of the bit vectors.

```bash  
ls experiment*.bf.bv | sed "s/\.bf\.bv//" > bitvectornames
../bfcluster/sbuild -f example.hashfile -p . -l bitvectornames -o . -b sbt.txt
```

(5) Split the SBT nodes to create an SBT-AS.

```bash  
bt split sbt.txt sbt-as.txt
```

(6) Compress the SBT-AS bit vectors, creating the node files for a compressed ABT-AS. Also create the topology file for the compressed ABT-AS

Each node is represented in the uncompressed SBT-AS as two files, with extensions .bf-all.bv and .bf-some.bv. These are converted to a single file with extension .bf-allsome.bv.rrr.

```bash  
./allsome_to_rrr.sh sbt.txt sbt-as.txt sbt-rrr-allsome.txt
```

(7) Run a batch of queries.

```bash  
bt query -t 0.5 sbt-rrr-allsome.txt queries queryresults
```

# Creating an ROAR-compressed Allsome Bloomtree

Steps 1 through 5 are the same as above.

(6-ROAR) Compress the SBT-AS bit vectors, creating the node files for a compressed ABT-AS. Also create the topology file for the compressed ABT-AS.

Unlike the RRR-compression step above, the two files representing a node in the uncompressed SBT-AS are converted to two files, with extensions .bf-all.bv.roar and .bf-some.roar.

```bash  
./allsome_to_roar.sh sbt.txt sbt-as.txt sbt-roar-allsome.txt
```

(7-ROAR) Run a batch of queries.

```bash  
bt query -t 0.5 sbt-roar-allsome.txt queries queryresults
```
