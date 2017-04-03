// contact: chensun@cse.psu.edu

#ifndef bfcluster_H		// (prevent multiple inclusion)
#define bfcluster_H

#include "util.h"

//#include <iostream>
//#include <fstream>
//#include <string>
//#include <cstring>
#include <vector>
//#include <cassert>

#include "BF.h"
#include "BloomTree.h"
//#include "Build.h"

#ifdef __cplusplus
extern "C" {
#endif

	#include "utilities.h"
	#include "cluster.h" // Cluster 3.0

#ifdef __cplusplus
}
#endif


// simplified BloomTree class, store tree topology
// two parameters missed are HashPair and hash_number
// BloomTree class can also be used instead with those two parameters arbitrarily set
class TopoNode{
public:
	TopoNode* children[2];
	//TopoNode* parent;
	std::string filename; // bloom filter file name
	TopoNode(std::string _filename):filename(_filename)//, 
		//parent(0)
	{
		children[0] = nullptr;
		children[1] = nullptr;
	}
};

// class member variables and functions are sorted by name

// BFCluster class: build Bloom Tree by clustering Bloom Filters

// two clustering algorithms used:
// 1. agglomerative hierarchical clustering
//		distance function: hamming distance of two bit vectors 
//		linkage criteria: average, min, max
//
// 2. heuristic/greedy clustering
//		greedily merging closest two nodes

class BFCluster{

public:

//----constructor and destructor

	BFCluster(const std::string & _hashfunction_filename, 
		const std::string & _bf_filelist, 
		const std::string & _bf_file_path, 
		const char _compressed = 'u',
		bool _debug = false);

	~BFCluster();

//----static and constants
	int SUBSET_SIZE; //500kb

	static const int BITVECTOR_FILE_HEADER_BYTES;

	static u32 popCount8[0x100];

	static bool popCountFilled;

//----public member functions:

	bool BuildUncompressedFilters();

	bool BuildUncompressedFiltersParallel();

	bool GreedyClustering(const std::string& _output_dir, const std::string & tree_topology_filename);

	bool HierarchicalClustering(const std::string& _output_dir, const std::string & tree_topology_filename);

//----unit test member functions
//	all definitions are at the end of .cc file 

	bool Test();
	void OutputDistanceMatrix();
	void OutputNodeArray();
	void buildVector(TopoNode *r, int depth, std::vector<std::vector<std::string> > & ret);
	std::vector<std::vector<std::string> > levelOrder(TopoNode *r);
	void OutputTreeTopology(); 

protected:


//----private member variables

	// input data, designed according to bitvector_subset.h
	u8** bitvectors;

	// store info in bf_filelist
	std::vector<std::string> bloom_filter_name_list;

	// number of bloom filters, equal to the size of bloom_filter_name_list
	int bloom_filter_number;

	// hamming distance matrix, all unsigned integers, values <= subset size
	// set as double** to compatible with other distance functions in future
	double** distance_matrix; 

	bool enable_debug;
	// the first line of tree topology file(after the first comma)
	std::string hashfunction_filename; 

	HashPair* hashes;

	int hash_number;

	// the directory to output results
	std::string output_dir;

	// tree topology root node
	TopoNode* root; 
	
	TopoNode** toponode_array;

	// tree topology array pointer
	// Node struct defined in cluster.h
	/*
	 * typedef struct {int left; int right; double distance;} Node;
	 * 
	 * A Node struct describes a single node in a tree created by hierarchical
	 * clustering. The tree can be represented by an array of n Node structs,
	 * where n is the number of elements minus one. The integers left and right
	 * in each Node struct refer to the two elements or subnodes that are joined
	 * in this node. The original elements are numbered 0..nelements-1, and the
	 * nodes -1..-(nelements-1). For each node, distance contains the distance
	 * between the two subnodes that were joined.
	 */
	Node* node_array;

	// 8*8 xor search table
	u8** distance_search_table;

	// 8*8 or search table 
	u8** union_search_table;
//----private member functions
	
	bool CalculateDistanceMatrix();

	int CheckSubsetSize(const std::string & bf_filename,
						u8* bv,
						int start_bit,
						int end_bit);

	TopoNode* ConstructTreeTopologyFromArray(Node * node_array);

	// clustering with linkage union
	Node* gcluster (int nelements, double** distmatrix);



	// inline function
	double PairwiseDistance(u8* a, u8* b);

	// call by constructor
	bool ReadBitSubsetFromBloomFilter(const std::string & bf_filename,
									u8* bv, 
									int start_bit, 
									int end_bit);

	// call by constructor
	bool ReadBFFilelist(const std::string & bf_filelist,
		const std::string & bf_file_path,
		const std::string & bf_file_suffix);

	void UnionAB2A(u8* a, u8* b);

	void UnionAB2A_CopyC2B(u8* a, u8* b, u8* c);

	bool UnionUncompressedFilters(const std::string& left_child_filename, 
								const std::string& right_child_filename, 
								const std::string& union_filename);

	// call by HierarchicalClustering/GreedyClustering
	void WriteBloomTree(
	    const std::string & outfile, 
	    TopoNode* root, 
	    const std::string & matrix_file);

	void WriteBloomTreeHelper(std::ostream & out, TopoNode* root, int level=1);

};

#endif