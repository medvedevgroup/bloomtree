CXX=g++

# directory of bloomtree-dev/src 
BT_PATH=../src
# directory of roaring library.so
ROAR_PATH=${HOME}/${MACHTYPE}/include/roaring
# directory of sdsl library.so
SDSL_PATH=${HOME}/${MACHTYPE}/lib
# directory of jellyfish library.so
FISH_PATH=${HOME}/${MACHTYPE}/lib
# directory of TCLAP head file
TCLAP_PATH=./include

# this is the path where you install TBB
TBB_INSTALL_DIR=/storage/home/cxs1031/backup/src/tbb2017

TBB_INCLUDE=$(TBB_INSTALL_DIR)/include

TBB_LIBRARY_RELEASE=$(TBB_INSTALL_DIR)/build/linux_intel64_gcc_cc5.2.0_libc2.12_kernel2.6.32_release

# also requires head file of roaring, sdsl, jellyfish, tclap in ~/include

CXXFLAGS=-g -std=c++11 -pthread -Wall -O3
CXXFLAGS+=-I${HOME}/${MACHTYPE}/include -I$(BT_PATH) -I${HOME}/${MACHTYPE}/include/jellyfish-2.2.6/ -I$(TCLAP_PATH)

LD_LIB=-L $(ROAR_PATH) -L $(SDSL_PATH) -L $(FISH_PATH)
LD_FLAG=-lroaring -lsdsl -ldivsufsort -ldivsufsort64 -ljellyfish-2.0 -lz -lpthread

TBB_FLAG=-I$(TBB_INCLUDE)
TBB_LIB=-L $(TBB_LIBRARY_RELEASE)
TBB_LD=-ltbb

VPATH=$(BT_PATH)

all: sbuild squery

sbuild: BF.o bfcluster.o cluster.o utilities.o bfmain.o util.o Kmers.o BloomTree.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LD_LIB) $(LD_FLAG)

squery: BF.o bfquery.o util.o Kmers.o BloomTree.o Query.o Count.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LD_LIB) $(LD_FLAG)

pcompress: BF.o parallel_compress.o util.o BloomTree.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LD_LIB) $(TBB_LIB) $(LD_FLAG) $(TBB_LD)

parallel_compress.o: parallel_compress.cc 
	$(CXX) $(CXXFLAGS) $(TBB_FLAG) -c -o $@ $^

Query.o: Query.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $^

Build.o: Build.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $^

BloomTree.o: BloomTree.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $^

Count.o: Count.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $^

Kmers.o: Kmers.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $^

util.o: util.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $^

BF.o: BF.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $^

bfmain.o: main.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $^

bfcluster.o: bfcluster.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $^

cluster.o: cluster.c
	$(CXX) $(CXXFLAGS) -c -o $@ $^

utilities.o: utilities.c
	$(CXX) $(CXXFLAGS) -c -o $@ $^


clean:
	rm -f *.o
	rm -f sbuild
	rm -f pcompress
	rm -f squery


#install:
#	mv bfcluster ${HOME}/${MACHTYPE}/bin

