#pragma once

#include "genesis/genesis.hpp"

#include "TreeInformation.hpp"
#include "easylogging++.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined( _WIN32 ) || defined(  _WIN64  )
#include <windows.h>
#else
#include <unistd.h>
#endif

using namespace genesis;
using namespace tree;
using namespace utils;

/**
 * Hash a std::pair.
 */
struct pairhash {
public:
	template<typename T, typename U>
	std::size_t operator()(const std::pair<T, U> &x) const {
		return std::hash<T>()(x.first) ^ std::hash<U>()(x.second);
	}
};
/**
 * Compute LQ-, QP-, and EQP-IC support scores for quartets.
 */
template<typename CINT>
class QuartetScoreComputer {
public:
	QuartetScoreComputer(Tree const &refTree, const std::string &evalTreesPath, size_t m, bool verboseOutput,
			bool enforceSmallMem, int num_threads, int internalMemory, std::vector<size_t>& refIdToLookupID);

	using QuartetTuple = std::array<uint16_t, 4>;
	using QuartetCountTuple = std::array<CINT, 3>;

	std::vector<double> getLQICScores();
	std::vector<double> getQPICScores();
	std::vector<double> getEQPICScores();
	void calculateQPICScores();
	void computeQuartetScoresBifurcatingQuartets(size_t uIdx, size_t vIdx, size_t wIdx, size_t zIdx, QuartetCountTuple quartetOccurrences);
	void initScores();
	size_t num_taxa() const;
	std::vector<uint16_t> get_leaves(uint64_t q);
	std::vector<size_t> refIdToLookupId;
	uint64_t get_index(size_t a, size_t b, size_t c, size_t d) const;
	
private:
	double log_score(size_t q1, size_t q2, size_t q3);

	void computeQuartetScoresBifurcating();
	std::unordered_map<std::pair<size_t, size_t>, std::tuple<size_t, size_t, size_t>, pairhash> countBuffer;

	void computeQuartetScoresMultifurcating();
	std::pair<size_t, size_t> nodePairForQuartet(size_t aIdx, size_t bIdx, size_t cIdx, size_t dIdx);
	void processNodePair(size_t uIdx, size_t vIdx);
	std::tuple<CINT, CINT, CINT> countQuartetOccurrences(size_t aIdx, size_t bIdx, size_t cIdx, size_t dIdx);
	std::pair<size_t, size_t> subtreeLeafIndices(size_t linkIdx);
	uint64_t bit_shifting_index_(size_t a, size_t b, size_t c, size_t d, size_t tupleIndex) const;
	short tuple_index_(size_t a, size_t b, size_t c, size_t d) const;
	uint64_t lookup_index_(size_t a, size_t b, size_t c, size_t d) const;
	size_t checkExistenceInSubtree(size_t startLeafIndex, size_t endLeafIndex, size_t a, size_t b, size_t c, size_t d);
	std::tuple<size_t, size_t, size_t, size_t> getReferenceOrder(size_t uIdx, size_t vIdx, size_t aIdx, size_t bIdx, size_t cIdx, size_t dIdx);

	Tree referenceTree; /**< the reference tree */
	size_t rootIdx; /**< ID of the genesis root node in the reference tree */

	size_t num_taxa_;
	bool verbose;

	TreeInformation informationReferenceTree;
	std::vector<double> LQICScores;
	std::vector<double> QPICScores;
	std::vector<double> EQPICScores;

	std::vector<size_t> eulerTourLeaves;
	std::vector<size_t> linkToEulerLeafIndex;

};

template<typename CINT>
void QuartetScoreComputer<CINT>::initScores() {

	if (!is_bifurcating(referenceTree)) {
		std::cout << "The reference tree is multifurcating.\n";
		LQICScores.resize(referenceTree.edge_count());
		std::fill(LQICScores.begin(), LQICScores.end(), std::numeric_limits<double>::infinity());
		// compute only LQ-IC scores
	} else {
		std::cout << "The reference tree is bifurcating.\n";
		LQICScores.resize(referenceTree.edge_count());
		QPICScores.resize(referenceTree.edge_count());
		EQPICScores.resize(referenceTree.edge_count());
		// initialize the LQ-IC, QP-IC and EQP-IC scores of all internodes (edges) to INFINITY
		std::fill(LQICScores.begin(), LQICScores.end(), std::numeric_limits<double>::infinity());
		std::fill(QPICScores.begin(), QPICScores.end(), std::numeric_limits<double>::infinity());
		std::fill(EQPICScores.begin(), EQPICScores.end(), std::numeric_limits<double>::infinity());
	}
}

template<typename CINT>
size_t QuartetScoreComputer<CINT>::num_taxa() const {
	return num_taxa_;
}

template<typename CINT>
std::vector<uint16_t> QuartetScoreComputer<CINT>::get_leaves(uint64_t q){
	std::vector<uint16_t> quartet;
	uint64_t mask = 32767;
	//get all possible inner nodes u
	quartet.push_back(static_cast<uint16_t>((q & (mask << 49)) >> 49));
	quartet.push_back(static_cast<uint16_t>((q & (mask << 34)) >> 34));
	//get all possible inner nodes v
	quartet.push_back(static_cast<uint16_t>((q & (mask << 19)) >> 19));
	quartet.push_back(static_cast<uint16_t>((q & (mask << 4)) >> 4));
	
	return quartet;
}

template<typename CINT>
uint64_t QuartetScoreComputer<CINT>::get_index(size_t a, size_t b, size_t c, size_t d) const {
	uint64_t tmp = lookup_index_(a,b,c,d);
	return tmp;
}

template<typename CINT>
uint64_t QuartetScoreComputer<CINT>::bit_shifting_index_(size_t a, size_t b, size_t c, size_t d, size_t tupleIndex) const {
	uint64_t res = 0;
	uint64_t tmp = 0;
	size_t quartet_id [4] = {a,b,c,d};
	for (short i = 1; i < 5; i++){
		tmp = 0;
		tmp = quartet_id[i-1] << (64-(i*15));
		res += tmp;
	}
	res += tupleIndex;

	return res;
}

template<typename CINT>
short QuartetScoreComputer<CINT>::tuple_index_(size_t a, size_t b, size_t c, size_t d) const {
	// Get all comparisons that we need.
	bool const ac = (a<c);
	bool const ad = (a<d);
	bool const bc = (b<c);
	bool const bd = (b<d);

	// Check first and third case. Second one is implied.
	bool const x = ((ac) & (ad) & (bc) & (bd)) | ((!ac) & (!bc) & (!ad) & (!bd));
	bool const ab_in_cd = ((!ac) & (ad) & (!bc) & (bd)) | ((!ad) & (ac) & (!bd) & (bc));
	bool const cd_in_ab = ((ac) & (!bc) & (ad) & (!bd)) | ((bc) & (!ac) & (bd) & (!ad));
	bool const z = ab_in_cd | cd_in_ab;
	bool const y = !x & !z;

	// Only one can be set.
	assert(!(x & y & z));
	assert(x ^ y ^ z);
	assert(x | y | z);
	short const r = static_cast<short>(y) + 2 * static_cast<short>(z);

	// Result has to be fitting.
	assert(r < 3);
	assert((x && !y && !z && r == 0) || (!x && y && !z && r == 1) || (!x && !y && z && r == 2));
	return r;
}

template<typename CINT>
uint64_t QuartetScoreComputer<CINT>::lookup_index_(size_t a, size_t b, size_t c, size_t d) const {
	size_t ta, tb, tc, td; // from largest to smallest
	size_t low1, high1, low2, high2, middle1, middle2;

	if (a < b) {
		low1 = a;
		high1 = b;
	} else {
		low1 = b;
		high1 = a;
	}
	if (c < d) {
		low2 = c;
		high2 = d;
	} else {
		low2 = d;
		high2 = c;
	}

	if (low1 < low2) {
		td = low1;
		middle1 = low2;
	} else {
		td = low2;
		middle1 = low1;
	}
	if (high1 > high2) {
		ta = high1;
		middle2 = high2;
	} else {
		ta = high2;
		middle2 = high1;
	}
	if (middle1 < middle2) {
		tc = middle1;
		tb = middle2;
	} else {
		tc = middle2;
		tb = middle1;
	}
	size_t tupleIndex = tuple_index_(a,b,c,d);
	return bit_shifting_index_(ta, tb, tc, td, tupleIndex);
}

/**
 * Retrieve the IDs of the node pair {u,v} that induces the metaquartet which induces the quartet {a,b,c,d}.
 * @param aIdx ID of the taxon a in the reference tree
 * @param bIdx ID of the taxon b in the reference tree
 * @param cIdx ID of the taxon c in the reference tree
 * @param dIdx ID of the taxon d in the reference tree
 */
template<typename CINT>
std::pair<size_t, size_t> QuartetScoreComputer<CINT>::nodePairForQuartet(size_t aIdx, size_t bIdx, size_t cIdx,
		size_t dIdx) {
	size_t uIdx = informationReferenceTree.lowestCommonAncestorIdx(aIdx, bIdx, rootIdx);
	size_t vIdx = informationReferenceTree.lowestCommonAncestorIdx(cIdx, dIdx, rootIdx);

	if (uIdx == informationReferenceTree.lowestCommonAncestorIdx(aIdx, bIdx, vIdx)) {
		vIdx = informationReferenceTree.lowestCommonAncestorIdx(cIdx, dIdx, uIdx);
	} else {
		uIdx = informationReferenceTree.lowestCommonAncestorIdx(aIdx, bIdx, vIdx);
	}
	return std::pair<size_t, size_t>(uIdx, vIdx);
}

/**
 * Return the computed LQ-IC support scores.
 */
template<typename CINT>
std::vector<double> QuartetScoreComputer<CINT>::getLQICScores() {
	return LQICScores;
}

/**
 * Return the computed QP-IC support scores.
 */
template<typename CINT>
std::vector<double> QuartetScoreComputer<CINT>::getQPICScores() {
	return QPICScores;
}

/**
 * Return the computed EQP-IC support scores.
 */
template<typename CINT>
std::vector<double> QuartetScoreComputer<CINT>::getEQPICScores() {
	return EQPICScores;
}

/**
 * Compute qic = 1 + p_q1 * log(p_q1) + p_q2 * log(p_q2) + p_q3 * log(p_q3)
 * Take into account corner cases when one or more values are zero.
 * Let ab|cd be the topology of the quartet {a,b,c,d} in the reference tree.
 * @param q1 count of the quartet topology ab|cd
 * @param q2 count of the quartet topology ac|bd
 * @param q3 count of the quartet topology ad|bc
 */
template<typename CINT>
double QuartetScoreComputer<CINT>::log_score(size_t q1, size_t q2, size_t q3) {
	if (q1 == 0 && q2 == 0 && q3 == 0)
		return 0;

	size_t sum = q1 + q2 + q3;
	double p_q1 = (double) q1 / sum;
	double p_q2 = (double) q2 / sum;
	double p_q3 = (double) q3 / sum;

	// qic = 1 + p_q1 * log(p_q1) + p_q2 * log(p_q2) + p_q3 * log(p_q3);
	double qic = 1;
	if (p_q1 != 0)
		qic += p_q1 * log(p_q1) / log(3);
	if (p_q2 != 0)
		qic += p_q2 * log(p_q2) / log(3);
	if (p_q3 != 0)
		qic += p_q3 * log(p_q3) / log(3);

	if (q1 < q2 || q1 < q3) {
		return qic * -1;
	} else {
		return qic;
	}
}

/**
 * @brief For a given pair of nodes (and their LCA), return the indices of the two links of those
 * nodes that point towards the respective other node.
 *
 * For example, given node A and node B, the first index in the returned std::pair is the index
 * of the link of node A that points in direction of node B, and the second index in the std::pair
 * is the index of the link of node B that points towards node A.
 */
std::pair<size_t, size_t> get_path_inner_links(TreeNode const& u, TreeNode const& v, TreeNode const& lca) {
	if (&u == &v) {
		throw std::runtime_error("No inner links on a path between a node and itself.");
	}
	if (&u == &lca) {
		TreeLink const* u_link = nullptr;
		for (auto it : path_set(v, u, lca)) {
			if (it.is_lca())
				continue;
			u_link = &it.link().outer();
		}
		assert(u_link != nullptr);
		return {u_link->index(), v.primary_link().index()};
	}
	if (&v == &lca) {
		TreeLink const* v_link = nullptr;
		for (auto it : path_set(u, v, lca)) {
			if (it.is_lca())
				continue;
			v_link = &it.link().outer();
		}
		assert(v_link != nullptr);
		return {u.primary_link().index(), v_link->index()};
	}
	return {u.primary_link().index(), v.primary_link().index()};
}

template<typename CINT>
void QuartetScoreComputer<CINT>::calculateQPICScores(){
	 // ***** Code for QP-IC and EQP-IC scores, finalizing, start
		for (auto kv : countBuffer) {
			std::pair<size_t, size_t> nodePair = kv.first;
			size_t uIdx = nodePair.first;
			size_t vIdx = nodePair.second;
			std::tuple<size_t, size_t, size_t> counts = kv.second;


			// compute the QP-IC score of the current metaquartet
			double qpic = log_score(std::get<0>(counts), std::get<1>(counts), std::get<2>(counts));

			// check if uIdx and vIdx are neighbors; if so, set QP-IC score of the edge connecting u and v
			auto const& u_link = referenceTree.node_at(uIdx).link();
			auto const& v_link = referenceTree.node_at(vIdx).link();
			if (u_link.outer().node().index() == vIdx) {
				QPICScores[u_link.edge().index()] = qpic;
			} else if (v_link.outer().node().index() == uIdx) {
				QPICScores[v_link.edge().index()] = qpic;
			}

			size_t lcaIdx = informationReferenceTree.lowestCommonAncestorIdx(uIdx, vIdx, rootIdx);
			// update the EQP-IC scores of the edges from uIdx to vIdx
			for (auto it : path_set(referenceTree.node_at(uIdx), referenceTree.node_at(vIdx),
					referenceTree.node_at(lcaIdx))) {
				if (it.is_lca())
					continue;
#pragma omp critical
				EQPICScores[it.edge().index()] = std::min(EQPICScores[it.edge().index()], qpic);
			}
		}
	 // ***** Code for QP-IC and EQP-IC scores, finalizing, end
}

template<typename CINT>
size_t QuartetScoreComputer<CINT>::checkExistenceInSubtree(size_t startLeafIndex, size_t endLeafIndex, size_t a, size_t b, size_t c, size_t d){
	size_t leafIndex = startLeafIndex;
	while(leafIndex != endLeafIndex){
		if(eulerTourLeaves[leafIndex] == a || eulerTourLeaves[leafIndex] == b || eulerTourLeaves[leafIndex] == c || eulerTourLeaves[leafIndex] == d) return eulerTourLeaves[leafIndex];
		leafIndex = (leafIndex + 1) % eulerTourLeaves.size();
	}
	throw std::runtime_error("Leaf not found in subtree");
}

template<typename CINT>
std::tuple<size_t, size_t, size_t, size_t> QuartetScoreComputer<CINT>::getReferenceOrder(size_t uIdx, size_t vIdx, size_t aIdx, size_t bIdx, size_t cIdx, size_t dIdx){
	size_t a,b,c,d = 0;
	size_t lcaIdx = informationReferenceTree.lowestCommonAncestorIdx(uIdx, vIdx, rootIdx);

	std::pair<size_t, size_t> innerLinks = get_path_inner_links(referenceTree.node_at(uIdx),
			referenceTree.node_at(vIdx), referenceTree.node_at(lcaIdx));

	size_t linkSubtree1 = referenceTree.link_at(innerLinks.first).next().index();
	size_t linkSubtree2 = referenceTree.link_at(innerLinks.first).next().next().index();
	size_t linkSubtree3 = referenceTree.link_at(innerLinks.second).next().index();
	size_t linkSubtree4 = referenceTree.link_at(innerLinks.second).next().next().index();
	// iterate over all quartets from {subtree1} x {subtree2} x {subtree3} x {subtree4}.
	// These are the quartets relevant to the current metaquartet.
	size_t startLeafIndexS1 = linkToEulerLeafIndex[linkSubtree1] % eulerTourLeaves.size();
	size_t endLeafIndexS1 = linkToEulerLeafIndex[referenceTree.link_at(linkSubtree1).outer().index()]
			% eulerTourLeaves.size();
	size_t startLeafIndexS2 = linkToEulerLeafIndex[linkSubtree2] % eulerTourLeaves.size();
	size_t endLeafIndexS2 = linkToEulerLeafIndex[referenceTree.link_at(linkSubtree2).outer().index()]
			% eulerTourLeaves.size();
	size_t startLeafIndexS3 = linkToEulerLeafIndex[linkSubtree3] % eulerTourLeaves.size();
	size_t endLeafIndexS3 = linkToEulerLeafIndex[referenceTree.link_at(linkSubtree3).outer().index()]
			% eulerTourLeaves.size();
	size_t startLeafIndexS4 = linkToEulerLeafIndex[linkSubtree4] % eulerTourLeaves.size();
	size_t endLeafIndexS4 = linkToEulerLeafIndex[referenceTree.link_at(linkSubtree4).outer().index()]
			% eulerTourLeaves.size();

	a = checkExistenceInSubtree(startLeafIndexS1, endLeafIndexS1, aIdx, bIdx, cIdx, dIdx);
	b = checkExistenceInSubtree(startLeafIndexS2, endLeafIndexS2, aIdx, bIdx, cIdx, dIdx);
	c = checkExistenceInSubtree(startLeafIndexS3, endLeafIndexS3, aIdx, bIdx, cIdx, dIdx);
	d = checkExistenceInSubtree(startLeafIndexS4, endLeafIndexS4, aIdx, bIdx, cIdx, dIdx);

	return std::tuple<size_t,size_t,size_t,size_t>(a,b,c,d);
}


/**
 * Compute the LQ-IC, QP-IC, and EQP-IC scores for a bifurcating reference tree, iterating over quartets instead of node pairs.
 */
template<typename CINT>
void QuartetScoreComputer<CINT>::computeQuartetScoresBifurcatingQuartets(size_t uIdx, size_t vIdx, size_t wIdx, size_t zIdx, QuartetCountTuple quartetOccurrences) {
	// Process all quartets
	//#pragma omp parallel for schedule(dynamic)
					// find topology ab|cd of {u,v,w,z}

					size_t lca_uv = informationReferenceTree.lowestCommonAncestorIdx(uIdx, vIdx, rootIdx);
					size_t lca_uw = informationReferenceTree.lowestCommonAncestorIdx(uIdx, wIdx, rootIdx);
					size_t lca_uz = informationReferenceTree.lowestCommonAncestorIdx(uIdx, zIdx, rootIdx);
					size_t lca_vw = informationReferenceTree.lowestCommonAncestorIdx(vIdx, wIdx, rootIdx);
					size_t lca_vz = informationReferenceTree.lowestCommonAncestorIdx(vIdx, zIdx, rootIdx);
					size_t lca_wz = informationReferenceTree.lowestCommonAncestorIdx(wIdx, zIdx, rootIdx);

					size_t aIdx, bIdx, cIdx, dIdx;

					if (informationReferenceTree.distanceInEdges(lca_uv, lca_wz)
							> informationReferenceTree.distanceInEdges(lca_uw, lca_vz)
							&& informationReferenceTree.distanceInEdges(lca_uv, lca_wz)
									> informationReferenceTree.distanceInEdges(lca_uz, lca_vw)) {
						aIdx = uIdx;
						bIdx = vIdx;
						cIdx = wIdx;
						dIdx = zIdx; // ab|cd = uv|wz
					} else if (informationReferenceTree.distanceInEdges(lca_uw, lca_vz)
							> informationReferenceTree.distanceInEdges(lca_uv, lca_wz)
							&& informationReferenceTree.distanceInEdges(lca_uw, lca_vz)
									> informationReferenceTree.distanceInEdges(lca_uz, lca_vw)) {
						aIdx = uIdx;
						bIdx = wIdx;
						cIdx = vIdx;
						dIdx = zIdx; // ab|cd = uw|vz
					} else if (informationReferenceTree.distanceInEdges(lca_uz, lca_vw)
							> informationReferenceTree.distanceInEdges(lca_uv, lca_wz)
							&& informationReferenceTree.distanceInEdges(lca_uz, lca_vw)
									> informationReferenceTree.distanceInEdges(lca_uw, lca_vz)) {
						aIdx = uIdx;
						bIdx = zIdx;
						cIdx = vIdx;
						dIdx = wIdx; // ab|cd = uz|vw
					} else {
						// else, we have a multifurcation and the quartet has none of these three topologies. In this case, ignore the quartet.
						//continue;
					}

					 //***** Code for QP-IC and EQP-IC scores start
					std::pair<size_t, size_t> nodePair = nodePairForQuartet(aIdx, bIdx, cIdx, dIdx);
					std::pair<size_t, size_t> nodePairSorted = std::pair<size_t, size_t>(
							std::min(nodePair.first, nodePair.second), std::max(nodePair.first, nodePair.second));

					std::tuple<size_t,size_t,size_t,size_t> quartet = getReferenceOrder(nodePairSorted.first, nodePairSorted.second, aIdx, bIdx, cIdx, dIdx); 
					aIdx = std::get<0>(quartet);
					bIdx = std::get<1>(quartet);
					cIdx = std::get<2>(quartet);
					dIdx = std::get<3>(quartet);
					//aIdx = refIdToLookupId[std::get<0>(quartet)];
					//bIdx = refIdToLookupId[std::get<1>(quartet)];
					//cIdx = refIdToLookupId[std::get<2>(quartet)];
					//dIdx = refIdToLookupId[std::get<3>(quartet)];

					short tupleA = tuple_index_(aIdx, bIdx, cIdx, dIdx);
					short tupleB = tuple_index_(aIdx, cIdx, bIdx, dIdx);
					short tupleC = tuple_index_(aIdx, dIdx, bIdx, cIdx);
			
					double qic = log_score(quartetOccurrences[tupleA],quartetOccurrences[tupleB],quartetOccurrences[tupleC]);

					//std::ofstream output;
					//output.open("countBuffer.csv", std::ios_base::app);
					//output << nodePairSorted.first << "," << nodePairSorted.second << "," << std::get<0>(quartet) << "," << std::get<1>(quartet) << "," << std::get<2>(quartet) 
					//<< "," << std::get<3>(quartet) << "," << quartetOccurrences[tupleA]
					//<< "," << quartetOccurrences[tupleB] << "," << quartetOccurrences[tupleC] 
					//<< "," << qic << std::endl;
					//output.close();

					// find path ends
					//size_t lca_ab = informationReferenceTree.lowestCommonAncestorIdx(aIdx, bIdx, rootIdx);
					//size_t lca_cd = informationReferenceTree.lowestCommonAncestorIdx(cIdx, dIdx, rootIdx);
					size_t fromIdx, toIdx;
					//if (lca_cd == informationReferenceTree.lowestCommonAncestorIdx(cIdx, dIdx, lca_ab)) {
					//	fromIdx = informationReferenceTree.lowestCommonAncestorIdx(aIdx, bIdx, lca_cd);
					//	toIdx = lca_cd;
					//} else {
					//	fromIdx = lca_ab;
					//	toIdx = informationReferenceTree.lowestCommonAncestorIdx(cIdx, dIdx, lca_ab);
					//}
					fromIdx = nodePair.second;
					toIdx = nodePair.first;
					// update the LQ-IC scores of the edges from fromIdx to toIdx
					size_t lcaFromToIdx = informationReferenceTree.lowestCommonAncestorIdx(fromIdx, toIdx, rootIdx);
					for (auto it : path_set(referenceTree.node_at(fromIdx), referenceTree.node_at(toIdx),
							referenceTree.node_at(lcaFromToIdx))) {
						if (it.is_lca())
							continue;

						size_t edge = it.edge().index();
#pragma omp critical
						LQICScores[edge] = std::min(LQICScores[it.edge().index()], qic);
					}
					
#pragma omp critical
{
					if (countBuffer.find(nodePairSorted) == countBuffer.end()) {
						std::tuple<size_t, size_t, size_t> emptyTuple(0, 0, 0);
						countBuffer[nodePairSorted] = emptyTuple;
					}
					std::get<0>(countBuffer[nodePairSorted]) += quartetOccurrences[tupleA];
					std::get<1>(countBuffer[nodePairSorted]) += quartetOccurrences[tupleB];
					std::get<2>(countBuffer[nodePairSorted]) += quartetOccurrences[tupleC];
					 //***** Code for QP-IC and EQP-IC scores end
}

}

/**
 * Update LQ-IC, QP-IC, and EQP-IC scores of all internodes influenced by the pair of inner nodes {u,v}.
 * As each quartet contributes to only one metaquartet induced by a node pair, our code visits each quartet exactly once.
 * @param uIdx the ID of the inner node u
 * @param vIdx the ID of the inner node v
 */
template<typename CINT>
void QuartetScoreComputer<CINT>::processNodePair(size_t uIdx, size_t vIdx) {
	// occurrences of topologies of the the metaquartet induced by {u,v} in the evaluation trees
	unsigned p1, p2, p3;
	p1 = 0;
	p2 = 0;
	p3 = 0;
	// find metaquartet indices by {u,v}

	size_t lcaIdx = informationReferenceTree.lowestCommonAncestorIdx(uIdx, vIdx, rootIdx);

	std::pair<size_t, size_t> innerLinks = get_path_inner_links(referenceTree.node_at(uIdx),
			referenceTree.node_at(vIdx), referenceTree.node_at(lcaIdx));

	size_t linkSubtree1 = referenceTree.link_at(innerLinks.first).next().index();
	size_t linkSubtree2 = referenceTree.link_at(innerLinks.first).next().next().index();
	size_t linkSubtree3 = referenceTree.link_at(innerLinks.second).next().index();
	size_t linkSubtree4 = referenceTree.link_at(innerLinks.second).next().next().index();
	// iterate over all quartets from {subtree1} x {subtree2} x {subtree3} x {subtree4}.
	// These are the quartets relevant to the current metaquartet.
	size_t startLeafIndexS1 = linkToEulerLeafIndex[linkSubtree1] % eulerTourLeaves.size();
	size_t endLeafIndexS1 = linkToEulerLeafIndex[referenceTree.link_at(linkSubtree1).outer().index()]
			% eulerTourLeaves.size();
	size_t startLeafIndexS2 = linkToEulerLeafIndex[linkSubtree2] % eulerTourLeaves.size();
	size_t endLeafIndexS2 = linkToEulerLeafIndex[referenceTree.link_at(linkSubtree2).outer().index()]
			% eulerTourLeaves.size();
	size_t startLeafIndexS3 = linkToEulerLeafIndex[linkSubtree3] % eulerTourLeaves.size();
	size_t endLeafIndexS3 = linkToEulerLeafIndex[referenceTree.link_at(linkSubtree3).outer().index()]
			% eulerTourLeaves.size();
	size_t startLeafIndexS4 = linkToEulerLeafIndex[linkSubtree4] % eulerTourLeaves.size();
	size_t endLeafIndexS4 = linkToEulerLeafIndex[referenceTree.link_at(linkSubtree4).outer().index()]
			% eulerTourLeaves.size();

	size_t aLeafIndex = startLeafIndexS1;
	size_t bLeafIndex = startLeafIndexS2;
	size_t cLeafIndex = startLeafIndexS3;
	size_t dLeafIndex = startLeafIndexS4;

	while (aLeafIndex != endLeafIndexS1) {
		while (bLeafIndex != endLeafIndexS2) {
			while (cLeafIndex != endLeafIndexS3) {
				while (dLeafIndex != endLeafIndexS4) {
					size_t aIdx = eulerTourLeaves[aLeafIndex];
					size_t bIdx = eulerTourLeaves[bLeafIndex];
					size_t cIdx = eulerTourLeaves[cLeafIndex];
					size_t dIdx = eulerTourLeaves[dLeafIndex];

					// process the quartet (a,b,c,d)
					// We already know by the way we defined S1,S2,S3,S4 that the reference tree has the quartet topology ab|cd
					std::tuple<CINT, CINT, CINT> quartetOccurrences = countQuartetOccurrences(aIdx, bIdx, cIdx, dIdx);
					p1 += std::get<0>(quartetOccurrences);
					p2 += std::get<1>(quartetOccurrences);
					p3 += std::get<2>(quartetOccurrences);
					double qic = log_score(std::get<0>(quartetOccurrences), std::get<1>(quartetOccurrences),
							std::get<2>(quartetOccurrences));

					// find path ends
					size_t lca_ab = informationReferenceTree.lowestCommonAncestorIdx(aIdx, bIdx, rootIdx);
					size_t lca_cd = informationReferenceTree.lowestCommonAncestorIdx(cIdx, dIdx, rootIdx);
					size_t fromIdx, toIdx;
					if (lca_cd == informationReferenceTree.lowestCommonAncestorIdx(cIdx, dIdx, lca_ab)) {
						fromIdx = informationReferenceTree.lowestCommonAncestorIdx(aIdx, bIdx, lca_cd);
						toIdx = lca_cd;
					} else {
						fromIdx = lca_ab;
						toIdx = informationReferenceTree.lowestCommonAncestorIdx(cIdx, dIdx, lca_ab);
					}
					// update the LQ-IC scores of the edges from fromIdx to toIdx
					size_t lcaFromToIdx = informationReferenceTree.lowestCommonAncestorIdx(fromIdx, toIdx, rootIdx);
					for (auto it : path_set(referenceTree.node_at(fromIdx), referenceTree.node_at(toIdx),
							referenceTree.node_at(lcaFromToIdx))) {
						if (it.is_lca())
							continue;
#pragma omp critical
						LQICScores[it.edge().index()] = std::min(LQICScores[it.edge().index()], qic);
					}

					dLeafIndex = (dLeafIndex + 1) % eulerTourLeaves.size();
				}
				cLeafIndex = (cLeafIndex + 1) % eulerTourLeaves.size();
				dLeafIndex = startLeafIndexS4;
			}
			bLeafIndex = (bLeafIndex + 1) % eulerTourLeaves.size();
			cLeafIndex = startLeafIndexS3;
			dLeafIndex = startLeafIndexS4;
		}
		aLeafIndex = (aLeafIndex + 1) % eulerTourLeaves.size();
		bLeafIndex = startLeafIndexS2;
		cLeafIndex = startLeafIndexS3;
		dLeafIndex = startLeafIndexS4;
	}

	// compute the QP-IC score of the current metaquartet
	double qpic = log_score(p1, p2, p3);

	// check if uIdx and vIdx are neighbors; if so, set QP-IC score of the edge connecting u and v
	auto const& u_link = referenceTree.node_at(uIdx).link();
	auto const& v_link = referenceTree.node_at(vIdx).link();
	if (u_link.outer().node().index() == vIdx) {
		QPICScores[u_link.edge().index()] = qpic;
	} else if (v_link.outer().node().index() == uIdx) {
		QPICScores[v_link.edge().index()] = qpic;
	}

	// update the EQP-IC scores of the edges from uIdx to vIdx
	for (auto it : path_set(referenceTree.node_at(uIdx), referenceTree.node_at(vIdx), referenceTree.node_at(lcaIdx))) {
		if (it.is_lca())
			continue;
#pragma omp critical
		EQPICScores[it.edge().index()] = std::min(EQPICScores[it.edge().index()], qpic);
	}
}

/**
 * Compute the LQ-IC, QP-IC, and EQP-IC support scores, iterating over node pairs.
 */
template<typename CINT>
void QuartetScoreComputer<CINT>::computeQuartetScoresBifurcating() {
	// Process all pairs of inner nodes
#pragma omp parallel for schedule(dynamic)
	for (size_t i = 0; i < referenceTree.node_count(); ++i) {
		if (!referenceTree.node_at(i).is_inner())
			continue;
		for (size_t j = i + 1; j < referenceTree.node_count(); ++j) {
			if (!referenceTree.node_at(j).is_inner())
				continue;
			processNodePair(i, j);
		}
	}
}

/**
 * Compute LQ-IC scores for a (possibly multifurcating) reference tree. Iterate over quartets.
 */
template<typename CINT>
void QuartetScoreComputer<CINT>::computeQuartetScoresMultifurcating() {
	// Process all quartets
#pragma omp parallel for schedule(dynamic)
	for (size_t uLeafIdx = 0; uLeafIdx < eulerTourLeaves.size(); uLeafIdx++) {
		for (size_t vLeafIdx = uLeafIdx + 1; vLeafIdx < eulerTourLeaves.size(); vLeafIdx++) {
			for (size_t wLeafIdx = vLeafIdx + 1; wLeafIdx < eulerTourLeaves.size(); wLeafIdx++) {
				for (size_t zLeafIdx = wLeafIdx + 1; zLeafIdx < eulerTourLeaves.size(); zLeafIdx++) {
					size_t uIdx = eulerTourLeaves[uLeafIdx];
					size_t vIdx = eulerTourLeaves[vLeafIdx];
					size_t wIdx = eulerTourLeaves[wLeafIdx];
					size_t zIdx = eulerTourLeaves[zLeafIdx];
					// find topology ab|cd of {u,v,w,z}
					size_t lca_uv = informationReferenceTree.lowestCommonAncestorIdx(uIdx, vIdx, rootIdx);
					size_t lca_uw = informationReferenceTree.lowestCommonAncestorIdx(uIdx, wIdx, rootIdx);
					size_t lca_uz = informationReferenceTree.lowestCommonAncestorIdx(uIdx, zIdx, rootIdx);
					size_t lca_vw = informationReferenceTree.lowestCommonAncestorIdx(vIdx, wIdx, rootIdx);
					size_t lca_vz = informationReferenceTree.lowestCommonAncestorIdx(vIdx, zIdx, rootIdx);
					size_t lca_wz = informationReferenceTree.lowestCommonAncestorIdx(wIdx, zIdx, rootIdx);

					size_t aIdx, bIdx, cIdx, dIdx;

					if (informationReferenceTree.distanceInEdges(lca_uv, lca_wz)
							> informationReferenceTree.distanceInEdges(lca_uw, lca_vz)
							&& informationReferenceTree.distanceInEdges(lca_uv, lca_wz)
									> informationReferenceTree.distanceInEdges(lca_uz, lca_vw)) {
						aIdx = uIdx;
						bIdx = vIdx;
						cIdx = wIdx;
						dIdx = zIdx; // ab|cd = uv|wz
					} else if (informationReferenceTree.distanceInEdges(lca_uw, lca_vz)
							> informationReferenceTree.distanceInEdges(lca_uv, lca_wz)
							&& informationReferenceTree.distanceInEdges(lca_uw, lca_vz)
									> informationReferenceTree.distanceInEdges(lca_uz, lca_vw)) {
						aIdx = uIdx;
						bIdx = wIdx;
						cIdx = vIdx;
						dIdx = zIdx; // ab|cd = uw|vz
					} else if (informationReferenceTree.distanceInEdges(lca_uz, lca_vw)
							> informationReferenceTree.distanceInEdges(lca_uv, lca_wz)
							&& informationReferenceTree.distanceInEdges(lca_uz, lca_vw)
									> informationReferenceTree.distanceInEdges(lca_uw, lca_vz)) {
						aIdx = uIdx;
						bIdx = zIdx;
						cIdx = vIdx;
						dIdx = wIdx; // ab|cd = uz|vw
					} else {
						// else, we have a multifurcation and the quartet has none of these three topologies. In this case, ignore the quartet.
						continue;
					}

					std::tuple<CINT, CINT, CINT> quartetOccurrences = countQuartetOccurrences(aIdx, bIdx, cIdx, dIdx);

					double qic = log_score(std::get<0>(quartetOccurrences), std::get<1>(quartetOccurrences),
							std::get<2>(quartetOccurrences));

					// find path ends
					size_t lca_ab = informationReferenceTree.lowestCommonAncestorIdx(aIdx, bIdx, rootIdx);
					size_t lca_cd = informationReferenceTree.lowestCommonAncestorIdx(cIdx, dIdx, rootIdx);
					size_t fromIdx, toIdx;
					if (lca_cd == informationReferenceTree.lowestCommonAncestorIdx(cIdx, dIdx, lca_ab)) {
						fromIdx = informationReferenceTree.lowestCommonAncestorIdx(aIdx, bIdx, lca_cd);
						toIdx = lca_cd;
					} else {
						fromIdx = lca_ab;
						toIdx = informationReferenceTree.lowestCommonAncestorIdx(cIdx, dIdx, lca_ab);
					}
					// update the LQ-IC scores of the edges from fromIdx to toIdx
					size_t lcaFromToIdx = informationReferenceTree.lowestCommonAncestorIdx(fromIdx, toIdx, rootIdx);
					for (auto it : path_set(referenceTree.node_at(fromIdx), referenceTree.node_at(toIdx),
							referenceTree.node_at(lcaFromToIdx))) {
						if (it.is_lca())
							continue;
#pragma omp critical
						LQICScores[it.edge().index()] = std::min(LQICScores[it.edge().index()], qic);
					}
				}
			}
		}
	}
}

/**
 * Get the start and stop indices of the leaves belonging to the subtree induced by the given link.
 * The leaf indices are between [start,stop).
 * @param linkIdx the genesis ID of the link inducing a subtree
 */
template<typename CINT>
std::pair<size_t, size_t> QuartetScoreComputer<CINT>::subtreeLeafIndices(size_t linkIdx) {
	size_t outerLinkIdx = referenceTree.link_at(linkIdx).outer().index();
	return {linkToEulerLeafIndex[linkIdx] % linkToEulerLeafIndex.size(), linkToEulerLeafIndex[outerLinkIdx] % linkToEulerLeafIndex.size()};
}

/**
 * Get the amount of total system RAM memory available.
 */
// Code adapted from http://stackoverflow.com/questions/2513505/how-to-get-available-memory-c-g
inline size_t getTotalSystemMemory() {
#if defined( _WIN32 ) || defined(  _WIN64  )
	MEMORYSTATUSEX status;
	status.dwLength = sizeof(status);
	GlobalMemoryStatusEx(&status);
	return status.ullTotalPhys;
#else
	size_t pages = sysconf(_SC_PHYS_PAGES);
	size_t page_size = sysconf(_SC_PAGE_SIZE);
	return pages * page_size;
#endif
}

/**
 * @param refTree the reference tree
 * @param evalTrees path to the file containing the set of evaluation trees
 * @param m number of evaluation trees
 * @param verboseOutput print some additional (debug) information
 */
template<typename CINT>
QuartetScoreComputer<CINT>::QuartetScoreComputer(Tree const &refTree, const std::string &evalTreesPath, size_t m,
		bool verboseOutput, bool enforeSmallMem, int num_threads, int internalMemory, std::vector<size_t>& refIdToLookupID) {
	referenceTree = refTree;
	rootIdx = referenceTree.root_node().index();

	verbose = verboseOutput;

	std::cout << "There are " << m << " evaluation trees.\n";

	std::cout << "Building subtree informations for reference tree..." << std::endl;
	// precompute subtree informations
	informationReferenceTree.init(refTree);
	linkToEulerLeafIndex.resize(referenceTree.link_count());
	for (auto it : eulertour(referenceTree)) {
		if (it.node().is_leaf()) {
			eulerTourLeaves.push_back(it.node().index());
		}
		linkToEulerLeafIndex[it.link().index()] = eulerTourLeaves.size();
	}
	num_taxa_ = eulerTourLeaves.size();
	refIdToLookupId = refIdToLookupID;

	std::cout << "Finished precomputing subtree informations in reference tree.\n";
	std::cout << "The reference tree has " << num_taxa_ << " taxa.\n";

	//estimate memory requirements
	size_t memoryLookupFast = num_taxa_ * num_taxa_ * num_taxa_ * num_taxa_ * sizeof(CINT);
	size_t memoryLookup = (num_taxa_ * (num_taxa_ - 1) * (num_taxa_ - 2) * (num_taxa_ - 3) / 24) * 3 * sizeof(CINT) + sizeof(size_t);
	size_t estimatedMemory = getTotalSystemMemory();

	std::cout << "Estimated memory usages (in bytes):" << std::endl;
	std::cout << "  Runtime-efficient Lookup table: " << memoryLookupFast << std::endl;
	std::cout << "  Memory-efficient Lookup table: " << memoryLookup << std::endl;
	std::cout << "  Estimated available memory: " << estimatedMemory << std::endl;

	if (memoryLookup > estimatedMemory) {
		throw std::runtime_error("Insufficient memory!");
	}

}
