#include "bfcluster.h"

bool BFCluster::popCountFilled = false;

u32 BFCluster::popCount8[0x100] = {};

const int BFCluster::BITVECTOR_FILE_HEADER_BYTES = 8;


BFCluster::BFCluster(const std::string & _hashfunction_filename, 
					const std::string & _bf_filelist, 
					const std::string & _bf_file_path, 
					const char _compressed,
					bool _debug)
//-----
// BFCluster
// 	constructor
// 	read bf_filelist
// 	initialize bitvector matrix
//-----
// Parameters:

//	string	hashfunction_filname:		hash function file name
//	string	bf_filelist:				leaf bloom filter file list
// 	string 	bf_file_path:				bloom filter file path
//	char	compressed:					bloom filter file compressed format					
//				'u' == uncompressed, default, bf file suffix set to ".bf.bv"
//				'r' == rrr compressed, bf file suffix set to "rrr.bf.bv"
//				'a' == roaring compressed, bf file suffix set to "roar.bf.bv"
//-----
// Bash script sample to generate leaf bloom filter file list:
	// ls -al /gpfs/cyberstar/pzm11/backup/sbt/uncompressedleafSBT/* \
		| grep -v union \
		| sed "s/.*\///" \
		| sed "s/\.bf\.bv//" \
		> temp.names
//----
// code reviewed
{

	/*initialize subset_size*/
	SUBSET_SIZE = 500000;

	enable_debug = _debug;
	hashfunction_filename = _hashfunction_filename;

	// get_hash_function defined in BloomTree.h as an API
	this->hashes = get_hash_function(hashfunction_filename, this->hash_number);

	// fill popCount
	if (!popCountFilled)
	{
		u32 ix;

		popCount8[0] = 0;
		for (ix=1 ; ix<0x100 ; ix++){
			popCount8[ix] = popCount8[ix>>1] + (ix&1);
			//std::cout << ix << "," << popCount8[ix] << std::endl;
		}

		popCountFilled = true;
	}

	std::string bf_file_suffix = ".bf.bv";
	if (_compressed == 'r'){
		bf_file_suffix = ".rrr.bf.bv";
	}else if(_compressed == 'a'){
		bf_file_suffix = ".roar.bf.bv";
	}

	if(SUBSET_SIZE%8 != 0){
		std::cerr << "Error: [BFCluster] subset bit size can not convert to bytes." << std::endl;	
	}

	std::string bf_file_path = _bf_file_path;
	if(bf_file_path[bf_file_path.size()-1] != '/') bf_file_path.push_back('/');

	// read bloom filter filenames into bloom_filter_name_list
	ReadBFFilelist(_bf_filelist, 
		bf_file_path, 
		bf_file_suffix);
	
	// initialize bitvectors before reading
	bloom_filter_number = bloom_filter_name_list.size();

	if(bloom_filter_number <= 0){
		std::cerr << "Error: [BFCluster] not enough bloom filter to generate bloom tree" << std::endl;
	}

	/*check subset size*/
	u8* tmp_bv = (u8*)malloc(SUBSET_SIZE / 8); // free memory in destructor
	SUBSET_SIZE = CheckSubsetSize(bloom_filter_name_list[0], tmp_bv, 0, SUBSET_SIZE);
	free(tmp_bv);
	/* --- */

	bitvectors = (u8**)malloc (bloom_filter_number * sizeof(u8*)); // free memory in destructor
	int subset_bytes_needed = SUBSET_SIZE / 8;

	//std::cout << "subset bytes needed: " << subset_bytes_needed << std::endl;

	if(_compressed == 'u'){
		// read bitvector from bloom filter into bitvectors
		for(u32 bv_ix = 0; bv_ix < bloom_filter_name_list.size(); bv_ix++){
			bitvectors[bv_ix] = (u8*)malloc (subset_bytes_needed); // free memory in destructor
			ReadBitSubsetFromBloomFilter(bloom_filter_name_list[bv_ix], bitvectors[bv_ix], 0, SUBSET_SIZE);
		}
	}else if(_compressed == 'r'){
		if(enable_debug){
			std::cerr << "reading from rrr compressed bv file, not implemented" << std::endl;
		}
	}else if(_compressed == 'a'){
		if(enable_debug){
			std::cerr << "reading from roaring compressed bv file, not implemented" << std::endl;
		}
	}

	// calculate distance matrix
	CalculateDistanceMatrix();
	//[todo] read bitvector subset from compressed file
	
}


// destructor
// 
BFCluster::~BFCluster(){
	// free u8** bitvectors
	if (bitvectors != NULL)
	{
		for (int bv_ix = 0 ; bv_ix < bloom_filter_number; bv_ix++)
			{ if (bitvectors[bv_ix] != NULL) free (bitvectors[bv_ix]); }
		free (bitvectors);
		bitvectors = NULL;
	}

	// free double** distance_matrix
	if (distance_matrix != NULL)
	{
		for (int i = 1; i < bloom_filter_number; i++)
			{ if (distance_matrix[i] != NULL) free(distance_matrix[i]); }
		free(distance_matrix);
		distance_matrix = NULL;
	}

	// free Node* node_array (c style)
	if(node_array != NULL){
		free(node_array);
		node_array = NULL;
	}

	// delete TopoNode** toponode_array (c++ style)
	for(int i = 0; i < bloom_filter_number*2-1; i++){
		delete toponode_array[i];
	}
	delete[] toponode_array;
}


bool BFCluster::BuildUncompressedFilters()
// generate union bloom filter files
{
	int array_size = bloom_filter_number - 1;
	for(int i = 0; i < array_size; i++){
		
		auto cur_node = toponode_array[i];
		
		std::string union_filename = cur_node->filename;
		if(union_filename.find_last_of("/") == std::string::npos){
			union_filename = output_dir + union_filename;
		}

		std::string left_child_filename = cur_node->children[0]->filename;
		if(left_child_filename.find_last_of("/") == std::string::npos){
			left_child_filename = output_dir + left_child_filename;
		}
		
		std::string right_child_filename = cur_node->children[1]->filename;
		if(right_child_filename.find_last_of("/") == std::string::npos){
			right_child_filename = output_dir + right_child_filename;
		}
		
		if(! UnionUncompressedFilters(left_child_filename, 
									right_child_filename, 
									union_filename))
		{
			return false;
		}
	}
	return true;
}


bool BFCluster::BuildUncompressedFiltersParallel()
// parallel strategy: parallel in each level of the tree
{
	return true;
}


bool BFCluster::CalculateDistanceMatrix()
//----
// CalculateDistanceMatrix
// 	generate distance_matrix from bitvector_matrix
// 	this function is only called in HierarchicalClustering
// 	this function can be parallel
//----
// code reviewed
// unit tested
{

	int i,j;
	int n = bloom_filter_number;
	if (n < 2) return false;

	/* Set up the ragged array */
	distance_matrix = (double**)malloc(n*sizeof(double*)); // free memory in destructor
	if(distance_matrix==NULL) return NULL; /* Not enough memory available */
	distance_matrix[0] = NULL;
	/* The zeroth row has zero columns. We allocate it anyway for convenience.*/
	for (i = 1; i < n; i++)
	{	
		distance_matrix[i] = (double*)malloc(i*sizeof(double)); // free memory in destructor
		if (distance_matrix[i]==NULL) break; /* Not enough memory available */
	}

	if (i < n) /* break condition encountered */
	{
		j = i;
		for (int i = 1; i < j; i++) free(distance_matrix[i]);
		free(distance_matrix);
		distance_matrix = NULL;
		std::cerr << "Error: [BFCluster] error to allocate memory for distance matrix" << std::endl;
		return false;
	}

	for (int bv1Ix=1 ; bv1Ix < bloom_filter_number; bv1Ix++)
	{
		u8*   bv1   = bitvectors[bv1Ix];
		for (int bv2Ix=0 ; bv2Ix < bv1Ix ; bv2Ix++)
		{
			u8*   bv2   = bitvectors[bv2Ix];
			distance_matrix[bv1Ix][bv2Ix] = PairwiseDistance (bv1, bv2);
		}
	}

	if(enable_debug){
		OutputDistanceMatrix();
	}

	// free space for distance_matrix in destructor
	return true;
}

int BFCluster::CheckSubsetSize(const std::string & bf_filename,
								u8* bv,
								int start_bit,
								int end_bit) 
/*
	check if bloom filter size is large enough
	return possible bytes
*/
{
	const char * filename = bf_filename.c_str();
	FILE * f = fopen(filename, "rb");
	if (f == NULL) {
		std::cerr << "Error: [BFCluster] error opening '" << bf_filename << "'. " << std::endl;
		return false;
	}

	int bytes_to_read = (end_bit - start_bit) / 8;
	int err = fseek(f, BITVECTOR_FILE_HEADER_BYTES + start_bit / 8, SEEK_SET);
	if (err != 0) {
		std::cerr << "Error: [BFCluster] can not seek required start bit." << std::endl;
		return false;
	}

	int bytes_read = fread(bv, 1, bytes_to_read, f);
	fclose(f);

	if (bytes_read != bytes_to_read) {
		std::cerr << "[BFCluster] adjust sampling size to " << bytes_read << " bytes. " << std::endl;
	}
	return bytes_read * 8;
}

TopoNode* BFCluster::ConstructTreeTopologyFromArray(Node * node_array)
//----
// ConstructTreeTopologyFromArray
//	convert tree stored in Node* array into TopoNode* linked list
//----
// Parameters:
// 	Node* 	root	A pointer to a newly allocated array of Node structs, describing the
// 					hierarchical clustering solution consisting of nelements-1 nodes. Depending on
// 					whether genes (rows) or microarrays (columns) were clustered, nelements is
// 					equal to nrows or ncolumns. See src/cluster.h for a description of the Node
// 					structure.
//					Node struct is defined in "cluster.h" from Cluster 3.0
//					array_size = nelements-1 = bloom_filter_number-1, the property of complete binary tree
//----
// Return:
// 	TopoNode* 		A pointer to the root of tree topology structure
//					TopoNode class defined in head file
//----
// Partial Effect:
// 	call GenerateUnionBloomFilterFile
//----
// [todo] compatible with compressed bf
// [todo] build internal node bloom filter
// code reviewed
// unit tested
{
	int array_size = bloom_filter_number - 1;
	int total_toponode_number = array_size + bloom_filter_number; // 2*bloom_filter_number-1
	if(total_toponode_number == 0) return NULL;
	toponode_array = new TopoNode* [total_toponode_number];
	if(toponode_array == NULL) return NULL;

	int i = 0;

	// create(new) internal toponode
	// first N-1 nodes are internal nodes
	for(; i < array_size; i++){
		int internal_name = i;
		//std::string filename = output_dir + std::to_string(internal_name+1) + ".union.bf.bv";
		std::string filename = std::to_string(internal_name+1) + ".union.bf.bv";
		toponode_array[i] = new TopoNode(filename);
		if(toponode_array[i] == NULL) break; // not enough memory
	}

	if(i < array_size){ // if not enough memory
		for(int j = 0; j < i; j++){
			if(toponode_array[j] != NULL) delete toponode_array[j];
		}
		return NULL;
	}

	// create(new) leaf toponode
	// next N nodes are leaf nodes
	for(i = array_size; i < total_toponode_number; i++){
		toponode_array[i] = new TopoNode(bloom_filter_name_list[i-array_size]);
		if(toponode_array[i] == NULL) break; // not enough memory
	}

	if(i < total_toponode_number){ // if not enough memeory
		for(int j = array_size; j < i; j++){
			if(toponode_array[j] != NULL) delete toponode_array[j];
		}
		return NULL;
	}

	// delete toponodes in destructor

	// create tree topology
	for(int i = 0; i < array_size; i++){

		int left_child_ix = node_array[i].left;
		if(left_child_ix >= 0){
			toponode_array[i]->children[0] = toponode_array[array_size+left_child_ix];	
		}else{
			toponode_array[i]->children[0] = toponode_array[left_child_ix*-1 - 1];
		}

		int right_child_ix = node_array[i].right;
		if(right_child_ix >= 0){
			toponode_array[i]->children[1] = toponode_array[array_size+right_child_ix];
		}else{
			toponode_array[i]->children[1] = toponode_array[right_child_ix*-1 - 1];
		}
	}

	// the last internal node is root node
	return toponode_array[array_size-1];
}


Node* BFCluster::gcluster (int nelements, double** distmatrix)

//----
// The gcluster routine performs clustering using pairwise distance.
// distance between two clusters is the hamming distance between the union of bf in each cluster.
//----
//----
// Parameters
// int 		nelements 	The number of elements to be clustered.
// double 	distmatrix
// 						The distance matrix, with nelements rows, each row being filled up to the
// 						diagonal. The elements on the diagonal are not used, as they are assumed to be
// 						zero. The distance matrix will be modified by this routine.
// ----
// Return value

// A pointer to a newly allocated array of Node structs, describing the
// hierarchical clustering solution consisting of nelements-1 nodes. Depending on
// whether genes (rows) or microarrays (columns) were clustered, nelements is
// equal to nrows or ncolumns. See src/cluster.h for a description of the Node
// structure.
// If a memory error occurs, palcluster returns NULL.
//----
//----
// Warning/Partial Effect
//	This function changes original data: bitvectors, i.e. the subset bit vector data is polluted here
// ----

{ int j;
  int n;
  int* clusterid;
  Node* result;

  clusterid = (int*)malloc(nelements*sizeof(int));
  if(!clusterid) return NULL;

  result = (Node*)malloc((nelements-1)*sizeof(Node));
  if (!result)
  { free(clusterid);
    return NULL;
  }

  // Setup a list specifying to which cluster a gene belongs, and keep track 
  // 	of the number of elements in each cluster (needed to calculate the 
  // 	average).
  // leaf bf are assigned the cluster id as its id in bf list [0, N-1] 
  //	N = bloom_filter_number 
  for (j = 0; j < nelements; j++)
  {
    clusterid[j] = j;
  }

  for (n = nelements; n > 1; n--)
  {
    int is = 1;
    int js = 0;
    result[nelements-n].distance = find_closest_pair(n, distmatrix, &is, &js);

    // make sure if data[n-1]/clusterid[n-1] is clustered, is == n-1
    if(js == n-1){
    	int tmp = is;
    	is = js;
    	js = tmp;
    }

    // Save result
    result[nelements-n].left = clusterid[is];
    result[nelements-n].right = clusterid[js];

    // data[js] changed to merged data, i.e. new internal node are stored in data[js]
    // data[is] now point to data[n-1], i.e. move data[n-1] to data[is], and then decrease n-1
    // logic: since cluster[is] and cluster[js] and now merged, original two cluster will be discarded
    //		and a new cluster is created, original data[is] and data[js] will never be used.
    //		To save space, use data[js] to store data of merged node; use data[is] to store the last data points
    //		so that next loop, we only need to iterate 1 to n-1 data points, keep loop neat
    //		To do this, we also need to update the clusterid of data[is] and data[js] to its corresponding cluster id
    //
    // Reference:
    // 1. Müllner, Daniel. "Modern hierarchical, agglomerative clustering algorithms." arXiv preprint arXiv:1109.2378 (2011).
    // 2. de Hoon, Michiel JL, et al. "Open source clustering software." Bioinformatics 20.9 (2004): 1453-1454.

    if(is != n-1){
    	UnionAB2A_CopyC2B(bitvectors[js], bitvectors[is], bitvectors[n-1]);
	}else{
		// if is == n-1, no need to copy
		UnionAB2A(bitvectors[js], bitvectors[is]);
	}
    
    // Fix the distances
    for (j = 0; j < js; j++)
    { 
    	distmatrix[js][j]  = PairwiseDistance(bitvectors[js], bitvectors[j]);
    }
    for (j = js+1; j < is; j++)
    { 
    	distmatrix[j][js] = PairwiseDistance(bitvectors[js], bitvectors[j]);
    }
    for (j = is+1; j < n; j++)
    { 
    	distmatrix[j][js] = PairwiseDistance(bitvectors[js], bitvectors[j]);
    }

    // because data[n-1] is moved to data[is], we also need to move corresponding distance
    if(is != n-1){
    	for (j = 0; j < is; j++) distmatrix[is][j] = distmatrix[n-1][j];
    	for (j = is+1; j < n-1; j++) distmatrix[j][is] = distmatrix[n-1][j];
	}

    // Update clusterids
    // assign newly generated id from -1 to -(N-1) to new internal nodes
    // Node result[x] corresponds to cluster id (x+1)*-1, this property will be used to generate tree topology
    clusterid[js] = n-nelements-1;
    clusterid[is] = clusterid[n-1];
  }
  free(clusterid);

  return result;
}


//	string	tree_topology_filename:		output filename
bool BFCluster::GreedyClustering(const std::string& _output_dir, const std::string & tree_topology_filename){

	output_dir = _output_dir;
	// warning: original data polluted in the following function
	node_array = gcluster (bloom_filter_number, distance_matrix);

	if(enable_debug){
		OutputNodeArray();
	}

	root = ConstructTreeTopologyFromArray(node_array);

	if(enable_debug){
		OutputTreeTopology();
	}

	// output result
	WriteBloomTree(
	    tree_topology_filename, 
	    root, 
	    hashfunction_filename);

	return true;
}


bool BFCluster::HierarchicalClustering(const std::string& _output_dir, const std::string & tree_topology_filename)
//----
// HierarchicalClustering
//----
// Parameters:
//	string	tree_topology_filename:		output filename
//----
// code reviewed
{

	// call clustering function in Cluster 3.0 library
	// palcluster()
	output_dir = _output_dir;

	node_array = pmlcluster (bloom_filter_number, distance_matrix);

	if(enable_debug){
		OutputNodeArray();
	}

	root = ConstructTreeTopologyFromArray(node_array);

	if(enable_debug){
		OutputTreeTopology();
	}

	// output result
	WriteBloomTree(
	    tree_topology_filename, 
	    root, 
	    hashfunction_filename);

	return true;
}


double BFCluster::PairwiseDistance(u8* a, u8* b)
// compute hamming distance between two bitvector
// code reviewed
// unit tested
{
	
	u8* scan1 = a;
	u8* scan2 = b;
	double distance = 0;
	// this loop can be vector parallelized
	for(int i = 0; i < SUBSET_SIZE/8; i++, scan1++, scan2++){
		distance += popCount8[*scan1 ^ *scan2];
		//if(enable_debug){
			//std::cout << (int)*scan1 << "," << (int)*scan2 << "," << (*scan1 ^ *scan2) << "," << popCount8[*scan1 ^ *scan2] << std::endl;
		//}
	}

	if(enable_debug){
		std::cout << "----distance: " << distance << std::endl;
	}

	return distance;
}


bool BFCluster::ReadBitSubsetFromBloomFilter(const std::string & bf_filename, 
											u8* bv,
											int start_bit, 
											int end_bit)
// ReadBitSubsetFromBloomFilter
// extract first SUBSET_SIZE from bloom filter
// code reviewed
// unit tested
{
	//std::string file = "rb";
	const char * filename = bf_filename.c_str();
	FILE * f = fopen (filename, "rb");
	if (f == NULL){
		std::cerr << "Error: [BFCluster] error opening '" << bf_filename << "'. " << std::endl;
		return false;
	}

	int bytes_to_read = (end_bit - start_bit) / 8;
	int err = fseek (f, BITVECTOR_FILE_HEADER_BYTES + start_bit/8, SEEK_SET);
	if (err != 0){
		std::cerr << "Error: [BFCluster] can not seek required start bit." << std::endl;
		return false;
	}

	int bytes_read = fread (bv, 1, bytes_to_read, f);
	if (bytes_read != bytes_to_read){
		std::cerr << "Error: [BFCluster] only " << bytes_read << "bytes read. " << std::endl;
		return false;
	}

	fclose (f);

	if(enable_debug){
		std::cout << "read from file " << bf_filename << std::endl;
		
		//u8* scan;
		//u32 scanIx;

		//std::cout << SUBSET_SIZE << ", " << end_bit << std::endl;

		//std::cout << "read from bit " << start_bit << " to bit " << end_bit << ":" << std::endl;
		//scan = bv;
		//for (scanIx=0 ; scanIx<bytes_to_read ; scanIx++)
		//{
		//	if      (scanIx      == 0) ;
		//	else if (scanIx % 16 == 0) std::cout << std::endl;
		//	else if (scanIx %  4 == 0) std::cout << "  ";
		//	else                       std::cout << " ";
			//std::cout << std::hex << scan[scanIx];
		//}
	}

	return true;
}


// code reviewed
// unit tested
bool BFCluster::ReadBFFilelist(const std::string & bf_filelist,
								const std::string & bf_file_path,
								const std::string & bf_file_suffix){
    std::ifstream input(bf_filelist);
    if(!input.good()){
        std::cerr << "Error: [BFCluster] error opening '"<<bf_filelist<<"'. Bailing out." << std::endl;
        return false;
    }

    if(enable_debug){
    	std::cout << "----reading bf list from file " << bf_filelist << std::endl;
    }

    std::string line;
    while(std::getline(input, line).good()){
    	if(!line.empty()){
    		std::string bloom_filter_name = bf_file_path + line + bf_file_suffix;
    		bloom_filter_name_list.push_back(bloom_filter_name);

    		if(enable_debug){
    			std::cout << line << std::endl;
    		}
    	}
    }

    if(enable_debug){
    	std::cout << "----finish reading bf list" << std::endl;
    }

    if(bloom_filter_name_list.size() == 0){
    	std::cerr << "Error: [BFCluster] empty valid bloom filter " << std::endl;
    	return false;
    }

    return true;
}


void BFCluster::UnionAB2A_CopyC2B(u8* a, u8* b, u8* c)
//----
// UnionAB2A_CopyC2B
// 	1. union(or operation) bitvector A and B into A
// 	2. copy C into B
//----
// Parameters:
// 	u8*		a 		bitvector A
// 	u8*		b 		bitvector B
// 	u8*		c 		bitvector C
//----
// Partial Effect:
// 	a |= b
// 	b = c
//----
{
	u8* scan1 = a;
	u8* scan2 = b;
	u8* scan3 = c;

	for(int i = 0; i < SUBSET_SIZE/8; i++, scan1++, scan2++, scan3++){
		//if(enable_debug){
		//	std::cout << "by scan" << (int)*scan1 << "," << (int)*scan2 << "," << (int)*scan3<< "," << (int)(*scan1 | *scan2) << std::endl;
		//	std::cout << "by ix" << (int)a[i] << "," << (int)b[i] << "," << (int)c[i]<< "," << (int)(a[i] | b[i]) << std::endl;
		//}
		a[i] = *scan1 | *scan2;
		b[i] = *scan3;
		//if(enable_debug){
		//	std::cout << "by scan" << (int)*scan1 << "," << (int)*scan2 << "," << (int)*scan3<< "," << (int)(*scan1 | *scan2) << std::endl;
		//	std::cout << "by ix" << (int)a[i] << "," << (int)b[i] << "," << (int)c[i]<< "," << (int)(a[i] | b[i]) << std::endl;
		//}
	}
}


void BFCluster::UnionAB2A(u8* a, u8* b)
//----
// UnionAB2A
// 	union("or" operation) bitvector A and B into A
//----
// Parameters:
// 	u8*		a 		bitvector A
// 	u8*		b 		bitvector B
//----
{
	for(int i = 0; i < SUBSET_SIZE/8; i++){
		a[i] |= b[i];
	}
}


bool BFCluster::UnionUncompressedFilters(const std::string& left_child_filename, 
										const std::string& right_child_filename, 
										const std::string& union_filename)
//----
// GenerateUnionBloomFilterFile
//	union two children bit vector into a parent bit vector, store it in disk
//----
// Parameters:
// 	string 		left_child_filename		left child bit vector filename
// 	string 		right_child_filename 	right child bit vector filename
// 	string 		union_filename			parent bit vector filename
//----
// Return:
//	return true if success, false otherwise
//----
// Partial Effect:
//	generate a new file union_filename, storing the union bit vector
//----
{
	UncompressedBF* left_child_bf = new UncompressedBF(left_child_filename, *hashes, this->hash_number);
	if(left_child_bf == NULL) return false;
	left_child_bf->load();

	UncompressedBF* right_child_bf = new UncompressedBF(right_child_filename, *hashes, this->hash_number);
	if(right_child_bf == NULL){
		delete left_child_bf;
		return false;
	}
	right_child_bf->load();

	UncompressedBF* union_bf = dynamic_cast<UncompressedBF*> (left_child_bf->union_with(union_filename, right_child_bf));
	if(union_bf == NULL){
		delete left_child_bf;
		delete right_child_bf;
		return false;
	}
	union_bf->save();

	delete left_child_bf;
	delete right_child_bf;
	delete union_bf;
	return true;
}


void BFCluster::WriteBloomTreeHelper(std::ostream & out, TopoNode* root, int level)
//----
// WriteBloomTreeHelper
// 	designed based on BloomFilter::write_bloom_tree_helper()
//----
// Parameters:
//----
// Partial Effect:
// 	output tree topology to ostream & out
//	output file format:	
// 		root, hash_file
// 		*Child1
// 		**Child2
// 	first line contains root bloom filter file name + "," + hash function file name
// 	following lines contains "*"*L + bloom filter file name, 
// 	L is the level of current node in tree topology, with root level = 0
//----
// code reviewed
{
    std::string lstr(level, '*');

    for (int i = 0; i < 2; i++) {
        if (root->children[i] != nullptr) {
            out << lstr << root->children[i]->filename << std::endl;
            WriteBloomTreeHelper(out, root->children[i], level+1);
        }
    }
}



void BFCluster::WriteBloomTree(
    const std::string & outfile, 
    TopoNode* root, 
    const std::string & matrix_file) 
//----
// WriteBloomTree
// 	designed based on BloomFilter::write_bloom_tree()
// 	output tree topology can be directly read by following function given all inter bloom filter created
//		BF* load_bf_from_file(const std::string & fn, HashPair hp, int nh)
//		defined in BF.cc
//----
// Parameters:
//----
// code reviewed
{
    std::cerr << "Writing to " << outfile << std::endl;
    std::ofstream out(outfile.c_str());
    out << root->filename << "," << matrix_file << std::endl;
    WriteBloomTreeHelper(out, root);
    std::cerr << "Done." << std::endl;
}

//---------------------unit test functions------------------------------------------------

bool BFCluster::Test(){
	return true;
}

void BFCluster::OutputDistanceMatrix(){
	std::cout << "----distance matrix----" << std::endl;
	std::cout << "row0: null" << std::endl;
	for (int bv1Ix=1 ; bv1Ix < bloom_filter_number; bv1Ix++)
	{
		std::cout << "row" << bv1Ix << ": ";
		for (int bv2Ix=0 ; bv2Ix < bv1Ix ; bv2Ix++)
		{
			std::cout << distance_matrix[bv1Ix][bv2Ix] << " ";
		}
		std::cout << std::endl;
	}
	std::cout << "-----------------------" << std::endl;
}

void BFCluster::OutputNodeArray(){
	std::cout << "----clustering result----" << std::endl;
	for(int i = 0; i < bloom_filter_number-1; i++){
		std::cout << "l:" << node_array[i].left << ",\tr:" << node_array[i].right << std::endl;
	}
	std::cout << "------------------------- " << std::endl;
}

// help function, pre-order traverse
void BFCluster::buildVector(TopoNode *r, int depth, std::vector<std::vector<std::string> > & ret)
{
    if(r == NULL || r == nullptr) return;
    if((int)(ret.size()) == depth)
        ret.push_back(std::vector<std::string>());
    
    ret[depth].push_back(r->filename);
    buildVector(r->children[0], depth + 1, ret);
    buildVector(r->children[1], depth + 1, ret);
}

// help function
std::vector<std::vector<std::string> > BFCluster::levelOrder(TopoNode *r) {
	std::vector<std::vector<std::string> > ret;
    buildVector(r, 0, ret);
    return ret;
}

// output tree topology by level
void BFCluster::OutputTreeTopology()
{
	auto file_list = levelOrder(this->root);
	std::cout << "----tree topology----" << std::endl;
	for(u32 i = 0; i < file_list.size(); i++){
		std::cout << "level " << i << std::endl;
		for(u32 j = 0; j < file_list[i].size(); j++){
			std::cout << "\t" << file_list[i][j] << std::endl;
		}
	}
	std::cout << "----end tree topology----" << std::endl;
}