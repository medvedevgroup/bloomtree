# SBT-AS (AllSome Sequence Bloom Tree).


This package implements the SBT-AS data structure presented in [1]. SBT-AS is a variant of the SBT data structure introduced by Solomon and Kingsford [2]. An SBT is a data structure that indexes a set of short-read sequencing experiments and supports fast sequence queries. SBT-AS improves the running time and space usage of the original SBT. The code of SBT-AS is built on top of and uses the original SBT code [1].


# Installation


To install SBT-AS using the source:  
1. Download the latest version of bloomtree-allsome using Github  
```bash  
     git clone https://github.com/medvedevgroup/bloomtree-allsome  
```  
2. Compile:  
```bash  
    cd bloomtree-allsome/src  
    make  
    cd ../bfcluster  
    make  
```


# Prerequisites


* gcc (Version 4.9.1 or later)
* Jellyfish (Version 2.2.0 or later)
* CRoaring (https://github.com/RoaringBitmap/CRoaring)
* SDSL-lite (April 2017 or later). Note that there was a bug in SDSL-lite prior
to April 2017, when compiled with clang. If you are using gcc, version 2.0 or
later should be adequate.


The PREQ_INSTALL file contains some helpful information for installing the prerequisites.


# Usage Overview


First, an SBT-AS must be built using the following pipeline:  
1. Initialize a hash function (`bt hashes`)  
2. Analyze the database’s fasta files to determine the bloom filter size (`get_bfsize.sh`)  
3. Convert the fasta files to bloom filter bit vectors (`bt count`)  
4. Build an uncompressed SBT of the bit vectors ( `sbuild`)  
5. Convert the SBT to SBT-AS (`bt split`)  
6. Compress the SBT-AS bit vectors (`bt compress-rrr-double`)


Then, to query the SBT-AS, either the `bt query` or `squery` is used.

A step-by-step example of the pipeline is shown in example/README.md.  


# Usage Details


To build an SBT-AS, follow these steps:


1.  `bt hashes [-k 20] hashfile`  
Creates a file describing the hash function to be used for bloom filters.


2.  `bt count [--cutoff 3] [--threads 16] hashfile bf_size fasta_in filter_out.bf.bv`  
Given an input file from the database, create an uncompressed bitvector file containing the input’s bloom filter. This command should be run for every fasta file in the database. The size of the bloom filter is given by bf_size. See section “Determining the Bloom Filter size” for how to determine the bf_size. Note that all bloom filters must have the same size.


We note that commands 1 and 2 are inherited from the original Solomon and Kinsgford base implementation [2] and further details on them can be found in user_manual-SK/sbt-manual.pdf.


3. `bfcluster/sbuild -f hashfile -p leaf_bloom_filter_directory -l bloom_filter_name.list  -o output_directory -b bloom_filter_tree_topology_file`  
Builds an SBT from the Bloom filters of the database entries. 
    - `-f` hash file used to generate leaf bloom filters
    - `-p` directory of leaf bloom filters
    - `-l` bloom filter name list file, each line in the file is a leaf bloom filter name, prefix excluded
For instance, if one of your leaf bloom filters is "/example/leaves/SAMPLE.bf.bv", then the corresponding line in list is "SAMPLE"
    - `-o` output directory of sequence bloom tree
    - `-b` filename of tree topology


4. `bt split bloomtreefile outfile`  
Given a bloomtreefile describing the tree topology, and bitvector files for the leaves in the tree, build the all+some version of the tree, with each node occupying two files. Note that the current implementation inadvertently requires existing files for the internal nodes, which are read (but not actually used internally).


5. `bt compress-rrr-double hashfile filter1_in.bf.bv filter2_in.bf.bv filter_out.bf.bv.rrr`  
Given a pair of bitvector files representing the uncompressed all and some parts of a node, make an rrr-compressed version of that node as a single file. This command must be run for every node of the tree.


To query the SBT-AS, you can use one of two functions. The `bt query` function implements the regular query algorithm described in Section 4.1 of [1], while the `squery` function implements the large heuristic and large exact query algorithms described in Section 4.3. For large queries,`squery` is orders of magnitude faster and is recommended. For instance, a sequencing experiment, which is a FASTA file that contains many reads, can be considered as a large query. For queries that contain on the order of thousands kmers, no significant performance improvement is expected. The disadvantage of `squery` is that it requires a ROAR compressed tree and will not work with an RRR compressed tree.


1. `bt query [--query-threshold 0.9] bloomtreefile queryfile outfile`  
Given a bloomtreefile describing the tree topology, bitvector files for every node in the tree, and a batch of queries (one per line), find the leaves that are "hits" for each query.


2. `bfcluster/squery -b tree_topology_file -f hashfile -q query.fasta -o output_directory -t threshold [-E]`  
`squery` by default implements the large heuristic algorithm from [1], meaning that the result may not always include all the hits. To instead use the large exact algorithm (which is slightly slower), use the -E option.
    - `-b` sequence bloom tree topology file, it must represent a ROAR compressed tree (see Section X)
    - `-f` hash file used to generate bloom filters
    - `-q` query sequences in FASTA file format
    - `-o` query output directory
    - `-t` query threshold
    - `-E` enable exact algorithm 




# Determining the Bloom Filter size


For optimal performance, the bf_size parameter in `bt count` should be approximately the count of distinct k-mers to be represented by the bloom filters. To determine this, use: 


```bash
get_bfsize.sh fastalistfile
```
Given a list of gzipped fasta files, report the distribution of kmer abundance. If the default cutoff of _c_ is to be used, the kmer count for abundances _c_ should be used for bf_size.


# Building a ROAR-compressed tree


The default compression scheme for SBT-AS is RRR. Alternatively, ROAR may be used. To use ROAR, you just need to change the pipeline to use `bt compress-roar-single` instead of `bt compress-rrr-double`.

However, there are some subtle differences in node management for ROAR-compressed trees.  These are described in the step-by-step examples in example/README.md.  



# Parallel speedup of tree construction


SBT-AS supports a multi-threaded version of `bt compress-rrr`. When multiple threads are available, this can greatly speedup the compression process. To use it, you must have Intel Threading Building Blocks on your system on your system. Then, you must compile `pcompress`:
```bash
cd bloomtree-allsome/bfcluster
make pcompress
```
Then, in place of `bt compress-rrr`, you can use:
 ```bash
bfcluster/pcompress -l bloom_filter_file_list -c compress_type -t thread
```
- `-l` bloom filter file list, each line in the file is a bloom filter file. A valid sequence bloom tree topology file is also OK.
- `-c` compress_type, should be either "rrr" or "roar", defaultly use "rrr" if not indicated.
- `-t` number of threads used


# Additional commands 
The following commands are not needed for standard usage, but may be useful for developers.


`bt compress-rrr bloomtreefile outfile`  
`bt compress-roar bloomtreefile outfile`  
Given a bloomtreefile describing the tree topology, and bitvector files for every node in the tree, make a compressed copy of each node in the tree, and create a topology file describing the compressed-node tree.


`bt compress-rrr-single hashfile filter_in.bf.bv`  
`bt compress-roar-single hashfile filter_in.bf.bv`  
Given a single node’s bitvector file, make a compressed copy of it.  The resulting file will be named filter_in.bf.bv.rrr or filter_in.bf.bv.roar.


`bt rebuild bloomtreefile`  
Given a bloomtreefile describing the tree topology, and bitvector files for the leaves in the tree, build the internal nodes according to the topology.


# Contact
For any questions regarding usage, please contact Chen Sun <chensun@cse.psu.edu> or Bob Harris <rsharris@bx.psu.edu>. 


# Acknowledgements


If using SBT-AS, please cite [1]. This project has been supported in part by NSF awards DBI-1356529, CCF-1439057, IIS-1453527, and IIS-1421908.




# References


[1] Chen Sun, Robert S. Harris, Rayan Chikhi, and Paul Medvedev. Allsome sequence bloom trees. International Conference on Research in Computational Molecular Biology. Springer, Cham, 2017.

[2] Brad Solomon and Carl Kingsford. Fast search of thousands of short-read sequencing experiments. Nature biotechnology, 2016.

# Errata

In [1], section 5 (Results), an incorrect accession number is given (SRR806782). The correct accession number is SRR805782.
