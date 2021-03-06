/**
 * Copyright (c) 2002-2013 Distributed and Parallel Systems Group,
 *                Institute of Computer Science,
 *               University of Innsbruck, Austria
 *
 * This file is part of the INSIEME Compiler and Runtime System.
 *
 * We provide the software of this file (below described as "INSIEME")
 * under GPL Version 3.0 on an AS IS basis, and do not warrant its
 * validity or performance.  We reserve the right to update, modify,
 * or discontinue this software at any time.  We shall have no
 * obligation to supply such updates or modifications or any other
 * form of support to you.
 *
 * If you require different license terms for your intended use of the
 * software, e.g. for proprietary commercial or industrial use, please
 * contact us at:
 *                   insieme@dps.uibk.ac.at
 *
 * We kindly ask you to acknowledge the use of this software in any
 * publication or other disclosure of results by referring to the
 * following citation:
 *
 * H. Jordan, P. Thoman, J. Durillo, S. Pellegrini, P. Gschwandtner,
 * T. Fahringer, H. Moritsch. A Multi-Objective Auto-Tuning Framework
 * for Parallel Codes, in Proc. of the Intl. Conference for High
 * Performance Computing, Networking, Storage and Analysis (SC 2012),
 * IEEE Computer Society Press, Nov. 2012, Salt Lake City, USA.
 *
 * All copyright notices must be kept intact.
 *
 * INSIEME depends on several third party software packages. Please 
 * refer to http://www.dps.uibk.ac.at/insieme/license.html for details 
 * regarding third party software licenses.
 */

#include "insieme/analysis/cba/prototype/parallel_flow.h"

#include <iterator>

namespace insieme {
namespace analysis {
namespace cba {
namespace prototype {


	namespace detail {

		std::size_t numPred(const Node& node, const Graph& graph) {
			auto& g = graph.asBoostGraph();
			typedef boost::graph_traits<Graph::GraphType> GraphTraits;
			typename GraphTraits::in_edge_iterator in_i, in_end;
			std::tie(in_i, in_end) = boost::in_edges(graph.getVertexDescriptor(node), g);
			return std::distance(in_i,in_end);
		}

		template<typename F>
		void forEachPred(const Node& node, const Graph& graph, const F& f) {
			auto& g = graph.asBoostGraph();
			typedef boost::graph_traits<Graph::GraphType> GraphTraits;
			typename GraphTraits::in_edge_iterator in_i, in_end;
			for (std::tie(in_i, in_end) = boost::in_edges(graph.getVertexDescriptor(node), g); in_i != in_end; ++in_i) {
				auto e = *in_i;
				auto src = boost::source(e, g);
				f(g[src]);
			}
		}

		template<typename F>
		void forEachInEdge(const Node& node, const Graph& graph, const F& f) {
			auto& g = graph.asBoostGraph();
			typedef boost::graph_traits<Graph::GraphType> GraphTraits;
			typename GraphTraits::in_edge_iterator in_i, in_end;
			for (std::tie(in_i, in_end) = boost::in_edges(graph.getVertexDescriptor(node), g); in_i != in_end; ++in_i) {
				auto e = *in_i;
				auto src = boost::source(e, g);
				f(g[e], g[src]);
			}
		}

		const Node& getNode(const Graph& graph, const ID& id) {
			static const Node dummy;
			for(auto it = graph.vertexBegin(); it != graph.vertexEnd(); ++it) {
				if (it->getID() == id) return *it;
			}
			assert_fail() << "Invalid node id " << id << " requested!";
			return dummy;
		}

		template<typename K, typename E>
		map<K,set<E>> merge(const map<K,set<E>>& a, const map<K,set<E>>& b) {
			map<K,set<E>> res = a;
			for(auto cur : b) {
				res[cur.first].insert(cur.second.begin(), cur.second.end());
			}
			return res;
		}

		template<typename T>
		set<T> set_intersect(const vector<set<T>>& sets) {
			assert_true(!sets.empty());
			set<T> res = sets[0];
			for(const auto& cur : sets) {
				res = utils::set::intersect(res, cur);
			}
			return res;
		}

		template<typename T, typename ... R>
		set<T> set_intersect(const set<T>& first, const R& ... rest) {
			return set_intersect(toVector(first, rest...));
		}

		template<typename T>
		set<T> set_union(const vector<set<T>>& sets) {
			set<T> res;
			for(const auto& cur : sets) {
				res.insert(cur.begin(), cur.end());
			}
			return res;
		}

		template<typename T, typename ... R>
		set<T> set_union(const set<T>& first, const R& ... rest) {
			return set_union(toVector(first, rest...));
		}


		void updateDominatorSet(const Node& node, const Graph& g) {
			auto& dom = node.dom;

			// ---- node distance -----

			node.distance[node.getID()] = 0;
			forEachPred(node, g, [&](const Node& cur) {
				for(auto e : cur.distance) {
					auto pos = node.distance.find(e.first);
					if (pos == node.distance.end()) {
						node.distance[e.first] = e.second+1;
					} else {
						node.distance[e.first] = std::min(pos->second, e.second+1);
					}
				}
			});


			// ---- dominator list -----

			// local node is always included
			dom.insert(node.getID());

			// intersect dominators of predecessors
			vector<set<ID>> preDom;
			forEachPred(node, g, [&](const Node& pred) {
				preDom.push_back(pred.dom);
			});

			if (preDom.empty()) return;

			auto intersect = preDom[0];
			for(auto cur : preDom) {
				intersect = utils::set::intersect(intersect, cur);
			}

			// add intersection of pre-decessor dominators
			dom.insert(intersect.begin(), intersect.end());

			// update strict dominator set
			node.strict_dom = dom;
			node.strict_dom.erase(node.getID());

			// --- immediate dominator ------

			// update immediate dominator

			if (!node.strict_dom.empty()) {
				ID idom = *node.strict_dom.begin();
				auto dist = node.distance[idom];
				for (auto cur : node.strict_dom) {
					if (node.distance[cur] < dist) {
						idom = cur;
						dist = node.distance[cur];
					}
				}
				node.immediate_dom.insert(idom);
				assert_le(node.immediate_dom.size(), 1u);
			}

		}

		void updateAccessIn(const Node& node, const Graph& graph) {
			forEachPred(node, graph, [&](const Node& pred) {
				node.accessIn = merge(node.accessIn, pred.accessOut);
			});
		}


		void updateInNaive(const Node& node, const Graph& graph) {
			// update set of accessed values
			updateAccessIn(node, graph);

			// just merge all the input sets
			forEachPred(node, graph, [&](const Node& pred) {
				node.before = merge(node.before, pred.after);
			});
		}

		void updateInDom(const Node& node, const Graph& graph) {

			// update set of accessed values
			updateAccessIn(node, graph);

			// skip while idom is not known
			if (node.immediate_dom.empty()) return;
			assert_eq(node.immediate_dom.size(), 1u) << "Node: " << node;

			// get idom id
			ID idom = *node.immediate_dom.begin();

			// get variables written since immediate dominating node
			const set<Var>& accessed = node.accessIn[idom];		// get variables written since acc

			// merge input sets
			forEachPred(node, graph, [&](const Node& pred) {
				for(auto cur : pred.after) {
					// only if written in no branch or in this branch
					if (!contains(accessed, cur.first) || contains(pred.accessOut[idom], cur.first)) {
						node.before[cur.first].insert(cur.second.begin(), cur.second.end());
					}
				}
			});
		}

		void updateInActiveKill(const Node& node, const Graph& graph) {

			// -- update killed-def in -----------------------------------

			// update kill-in set - union of all kill-out sets

			// collect all in-kill sets of parallel and sequential edges
			vector<set<ID>> inSeq;
			vector<set<ID>> inPar;
			forEachInEdge(node, graph, [&](const Edge& e, const Node& pred) {
				if (e == Seq) inSeq.push_back(pred.killedLocationsOut);
				if (e == Par) inPar.push_back(pred.killedLocationsOut);
			});

			if (inSeq.empty() && inPar.empty()) {
				node.killedLocationsIn.clear();
			} else if (inSeq.empty()) {
				node.killedLocationsIn = set_union(inPar);
			} else if (inPar.empty()) {
				node.killedLocationsIn = set_intersect(inSeq);
			} else {
				node.killedLocationsIn = set_union(
						set_intersect(inSeq), set_union(inPar)
				);
			}

//			node.killedLocationsIn.clear();
//			forEachPred(node, graph, [&](const Node& pred) {
//				node.killedLocationsIn.insert(pred.killedLocationsOut.begin(), pred.killedLocationsOut.end());
//			});


			// -- active write set ---------------------------------------

			// update active-in - union of all active-out sets of the in-edges
			node.activeWriteIn.clear();
			forEachPred(node, graph, [&](const Node& pred) {
				for(auto cur : pred.activeWriteOut) {
					if (!contains(node.killedLocationsIn, cur)) {
						node.activeWriteIn.insert(cur);
					}
				}
			});

			// update active-write out
			node.activeWriteOut.clear();
			if (node.getType() == Node::Write) {
				node.activeWriteOut.insert(node.getID());
			} else {
				node.activeWriteOut.insert(node.activeWriteIn.begin(), node.activeWriteIn.end());
			}

			// -- update killed-def out -----------------------------------

			// update kill-out set - in set + active set in case this node is a write node
			node.killedLocationsOut.clear();
			node.killedLocationsOut.insert(node.killedLocationsIn.begin(), node.killedLocationsIn.end());
			if (node.getType() == Node::Write) {
				node.killedLocationsOut.insert(node.activeWriteIn.begin(), node.activeWriteIn.end());
			}

			// -- update value set (before) ------------

			node.before.clear();
			for(auto cur : node.activeWriteIn) {
				// collect after-values of all active nodes
				node.before = merge(node.before, getNode(graph,cur).after);
			}

		}

		bool update(const Node& node, const Graph& g) {

			// create a backup of the old state
			Node old = node;

			// update dominator set
			updateDominatorSet(node, g);

			// update in-set  ... that's the tricky part
//			updateInNaive(node, g);
//			updateInDom(node, g);
			updateInActiveKill(node, g);

			// update out-set accesses


			// update out-set (assignment)
			node.after = node.before;
			node.accessOut = node.accessIn;
			if (node.getType() == Node::Write) {
				node.after[node.getVar()] = utils::set::toSet<std::set<Value>>(node.getValue());

				forEachPred(node, g, [&](const Node& pred) {
					node.accessOut[pred.getID()].insert(node.getVar());
				});

			}

			// check whether there was a change
			return  old.dom != node.dom || old.strict_dom != node.strict_dom ||
					old.immediate_dom != node.immediate_dom || old.distance != node.distance ||
					old.before != node.before || old.after != node.after ||
					old.accessIn != node.accessIn || old.accessOut != node.accessOut ||
					old.killedLocationsIn != node.killedLocationsIn || old.killedLocationsOut != node.killedLocationsOut ||
					old.activeWriteIn != node.activeWriteIn || old.activeWriteOut != node.activeWriteOut;
		}

	}

	using namespace detail;

	void solve(const Graph& graph) {

		bool change = true;
		while(change) {
			change = false;
			for(auto cur = graph.vertexBegin(); cur != graph.vertexEnd(); ++cur) {
				change = update(*cur, graph) || change;
			}
		}

	}

} // end namespace prototype
} // end namespace cba
} // end namespace analysis
} // end namespace insieme
