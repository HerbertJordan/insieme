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

#include "insieme/backend/c_ast/c_code.h"

#include <fstream>

#include <boost/graph/topological_sort.hpp>
#include <boost/graph/adjacency_list.hpp>

#include "insieme/backend/c_ast/c_ast_printer.h"
#include "insieme/utils/container_utils.h"

#include "insieme/utils/graph_utils.h"

#include "insieme/utils/logging.h"

namespace insieme {
namespace backend {
namespace c_ast {

	// -- C Code ----------------------------------------------------------------

	namespace {

		using namespace boost;

		typedef adjacency_list<vecS, vecS, directedS, CodeFragmentPtr> Graph;
		typedef graph_traits<Graph>::vertex_descriptor Vertex;
		typedef std::map<CodeFragment*, Vertex> CodeVertexMap;

		/**
		 * Adds the given fragment and its entire transitive dependency closure to the given
		 * graph and vertex map.
		 */
		Vertex addFragment(const CodeFragmentPtr& cur, Graph& graph, CodeVertexMap& vertexMap) {

			// insert new fragment into vertex map
			auto insertionRes = vertexMap.insert(std::make_pair(&(*cur), Vertex()));
			if (!insertionRes.second) {
				// was already present => done
				return insertionRes.first->second;
			}

			// not present yet => add to graph
			Vertex v = add_vertex(graph);
			graph[v] = cur;
			insertionRes.first->second = v;

			// add dependencies
			for_each(cur->getDependencies(), [&](const CodeFragmentPtr& dep) {
				Vertex u = addFragment(dep, graph, vertexMap);
				add_edge(v,u,graph);
			});

			// add requirements (no dependency here)
			for_each(cur->getRequirements(), [&](const CodeFragmentPtr& req) {
				addFragment(req, graph, vertexMap);
			});

			// return vertex ID of added node
			return v;
		}

		/**
		 * Obtains the transitive dependency closure of the given node. Within the
		 * resulting list, code fragments required by an element X are located before
		 * this element.
		 *
		 * @param fragments the seed elements for the computation of the transitive closure
		 * @return a list of fragments in a order such that all dependencies are satisfied
		 */
		vector<CodeFragmentPtr> getOrderedFragments(const vector<CodeFragmentPtr>& fragments) {

			// build graph
			Graph graph;
			CodeVertexMap map;

			// complete graph recursively
			for_each(fragments, [&](const CodeFragmentPtr& fragment) {
				addFragment(fragment, graph, map);
			});

			try {
				// obtain topological order
				vector<Vertex> vertexOrder;
				topological_sort(graph, std::back_inserter(vertexOrder));

				// convert vertices list to code fragment pointer list
				vector<CodeFragmentPtr> result;
				::transform(vertexOrder, std::back_inserter(result), [&](const Vertex& curV) {
					return graph[curV];
				});


				// TODO: remove this debug print
//				std::fstream outFile("graph.dot", std::fstream::out | std::fstream::trunc);
//				boost::write_graphviz(outFile, graph, [&](std::ostream& out, const Vertex& v) {
//					string label = toString(*graph[v]);
//					boost::replace_all(label, "\n", " ");
//					const int limit = 40;
//					if (label.size() > limit) {
//						label = label.substr(0,limit) + "...";
//					}
//					out << "[label=\"" << label << "\"]";
//				});
//				boost::write_graphviz(outFile, graph, insieme::utils::graph::label_printer<Graph, print<deref<CodeFragmentPtr>>>(graph, print<deref<CodeFragmentPtr>>()));
//				outFile.close();

				return result;

			} catch(not_a_dag e) {
				// failed => not resolvable cyclic dependencies
				assert(false && "Impossible to resolve cyclic dependencies.");
			}

			assert(false && "Should not be reachable!");

			// return seeds only
			return fragments;
		}

	}


	CCode::CCode(const SharedCodeFragmentManager& manager, const core::NodePtr& source, const CodeFragmentPtr& root)
		: TargetCode(source), fragmentManager(manager), fragments(toVector(root)) { }

	CCode::CCode(const SharedCodeFragmentManager& manager, const core::NodePtr& source, const vector<CodeFragmentPtr>& fragments)
		: TargetCode(source), fragmentManager(manager), fragments(fragments) { }

	std::ostream& CCode::printTo(std::ostream& out) const {

		// print a header
		out << "/**\n";
		out << " * ------------------------ Auto-generated Code ------------------------ \n";
		out << " *           This code was generated by the Insieme Compiler \n";
		out << " *        To be executed using the Insieme Runtime Environment\n";
		out << " * --------------------------------------------------------------------- \n";
		out << " */\n";

		// get list of fragments to be printed
		auto list = getOrderedFragments(fragments);

		// collect and add includes
		std::set<string> includes;
		for_each(list, [&](const CodeFragmentPtr& cur) {
			includes.insert(cur->getIncludes().begin(), cur->getIncludes().end());
		});
		for_each(includes, [&](const string& cur) {
			if (cur[0] == '<' || cur[0] == '"') {
				out << "#include " << cur << "\n";
			} else {
				out << "#include <" << cur << ">\n";
			}
		});

		out << "\n";

		// print topological sorted list of fragments
		for_each(list, [&out](const CodeFragmentPtr& cur) {
			out << *cur;
		});
		return out << "\n";

	}

	// -- Code Fragment Manager -------------------------------------------------

	CodeFragmentManager::~CodeFragmentManager() {
		for_each(fragments, [](const CodeFragment* cur){
			delete cur;
		});
	}


	// -- Code Fragment Definitions ---------------------------------------------

	void CodeFragment::addDependency(const CodeFragmentPtr& fragment) {
		// only add if it is not a null pointer and not self
		if (fragment && this != &*fragment) {
			dependencies.insert(fragment);
		}
	}

	void CodeFragment::addRequirement(const CodeFragmentPtr& fragment) {
		// only add if it is not a null pointer and not self
		if (fragment && this != &*fragment) {
			requirements.insert(fragment);
		}
	}

	std::ostream& CCodeFragment::printTo(std::ostream& out) const {
		// use C-AST printer to generate code
		for_each(code, [&](const c_ast::NodePtr& cur) {
			out << CPrint(cur) << "\n";
		});
		return out;
	}

	CodeFragmentPtr DummyFragment::createNew(const SharedCodeFragmentManager& manager, const FragmentSet& dependencies) {
		return manager->create<DummyFragment>(dependencies);
	}


} // end namespace c_ast
} // end namespace backend
} // end namespace insieme
