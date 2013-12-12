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

#include <gtest/gtest.h>

#include "insieme/analysis/cba/prototype/parallel_flow_2.h"

#include "insieme/core/ir_builder.h"

namespace insieme {
namespace analysis {
namespace cba {
namespace prototype_2 {

	namespace {

		namespace ug = utils::graph;

		void createDotDump(const Graph& graph) {
//			std::cout << "Creating Dot-Dump for " << analysis.getNumSets() << " sets and " << analysis.getNumConstraints() << " constraints ...\n";
			{
				// open file
				std::ofstream out("graph.dot", std::ios::out );

				// write file
				ug::printGraphViz(
						out,
						graph.asBoostGraph(),
						ug::default_label(),
						ug::no_label(),
						ug::default_deco(),
						[](std::ostream& out, const Edge& e) {
							switch(e) {
							case All: out << ", style=\"solid\"";  break;
							case One: out << ", style=\"dashed\""; break;
//							case Com: out << ", style=\"dotted\""; break;
							}
						}
				);
			}

			// create svg
			system("dot -Tsvg graph.dot -o graph.svg");
		}

	}

	TEST(CBA_Parallel, GraphBuilder) {

		Graph graph;

		// built the graph
		Node a = Node::write("x", 1);
		Node b = Node::write("x", 2);
		Node c = Node::read("x");
		Node d = Node::read("x");
		Node e = Node::write("x", 5);

		Node f = Node::write("x", 6);
		Node g = Node::write("x", 7);

		graph.addEdge(a, b, All);
		graph.addEdge(b, c, All);
		graph.addEdge(c, d, All);
		graph.addEdge(d, e, All);

		graph.addEdge(a, f, All);
		graph.addEdge(f, c, All);

		graph.addEdge(b, g, All);
		graph.addEdge(g, d, All);

		// solve the data flow equations
		solve(graph);

		EXPECT_EQ("{x={1}}", toString(graph.getVertex(a).after));
		EXPECT_EQ("{x={5}}", toString(graph.getVertex(e).after));

		createDotDump(graph);
	}


	TEST(CBA_Parallel, DiamondNoAssign) {

		Graph graph;

		Node a = Node::write("x", 0);
		Node b = Node::noop();
		Node c = Node::noop();
		Node d = Node::read("x");

		graph.addEdge(a,b, All);
		graph.addEdge(a,c, All);
		graph.addEdge(b,d, All);
		graph.addEdge(c,d, All);

		solve(graph);

		EXPECT_EQ("{x={0}}", toString(graph.getVertex(d).before));

		createDotDump(graph);

	}

	TEST(CBA_Parallel, DiamondOneAssign) {

		Graph graph;

		Node a = Node::write("x", 0);
		Node b = Node::write("x", 1);
		Node c = Node::noop();
		Node d = Node::read("x");

		graph.addEdge(a,b, All);
		graph.addEdge(a,c, All);
		graph.addEdge(b,d, All);
		graph.addEdge(c,d, All);

		solve(graph);

		EXPECT_EQ("{x={1}}", toString(graph.getVertex(d).before));

		createDotDump(graph);

	}

	TEST(CBA_Parallel, DiamondTwoAssign) {

		Graph graph;

		Node a = Node::write("x", 0);
		Node b = Node::write("x", 1);
		Node c = Node::write("x", 2);
		Node d = Node::read("x");

		graph.addEdge(a,b, All);
		graph.addEdge(a,c, All);
		graph.addEdge(b,d, All);
		graph.addEdge(c,d, All);

		solve(graph);

		EXPECT_EQ("{x={1,2}}", toString(graph.getVertex(d).before));

		createDotDump(graph);

	}


	TEST(CBA_Parallel, ThreeDiamonds) {

		Graph graph;

		{
			Node a = Node::write("x", 0);
			Node b = Node::noop();
			Node c = Node::noop();
			Node d = Node::read("x");

			graph.addEdge(a,b, All);
			graph.addEdge(a,c, All);
			graph.addEdge(b,d, All);
			graph.addEdge(c,d, All);

			solve(graph);

			EXPECT_EQ("{x={0}}", toString(graph.getVertex(d).before));
		}

		{
			Node a = Node::write("x", 0);
			Node b = Node::write("x", 1);
			Node c = Node::noop();
			Node d = Node::read("x");

			graph.addEdge(a,b, All);
			graph.addEdge(a,c, All);
			graph.addEdge(b,d, All);
			graph.addEdge(c,d, All);

			solve(graph);

			EXPECT_EQ("{x={1}}", toString(graph.getVertex(d).before));
		}

		{
			Node a = Node::write("x", 0);
			Node b = Node::write("x", 1);
			Node c = Node::write("x", 2);
			Node d = Node::read("x");

			graph.addEdge(a,b, All);
			graph.addEdge(a,c, All);
			graph.addEdge(b,d, All);
			graph.addEdge(c,d, All);

			solve(graph);

			EXPECT_EQ("{x={1,2}}", toString(graph.getVertex(d).before));
		}

		{
			// built the graph
			Node a = Node::write("x", 1);
			Node b = Node::write("x", 2);
			Node c = Node::read("x");
			Node d = Node::read("x");
			Node e = Node::write("x", 5);

			Node f = Node::write("x", 6);
			Node g = Node::write("x", 7);

			graph.addEdge(a, b, One);
			graph.addEdge(b, c, One);
			graph.addEdge(c, d, One);
			graph.addEdge(d, e, One);

			graph.addEdge(a, f, All);
			graph.addEdge(f, c, All);

			graph.addEdge(b, g, All);
			graph.addEdge(g, d, All);

			// solve the data flow equations
			solve(graph);

			EXPECT_EQ("{x={1}}", toString(graph.getVertex(a).after));
			EXPECT_EQ("{x={6,7}}", toString(graph.getVertex(d).before));
			EXPECT_EQ("{x={5}}", toString(graph.getVertex(e).after));

		}

		createDotDump(graph);

	}

	TEST(CBA_Parallel, TwoInterleavedSideThreads) {

		Graph graph;

		// built the graph
		Node a = Node::write("x", 1);
		Node b = Node::write("x", 2);
		Node c = Node::read("x");
		Node d = Node::read("x");
		Node e = Node::write("x", 5);

		Node f = Node::write("x", 6);
		Node g = Node::write("x", 7);

		graph.addEdge(a, b, One);
		graph.addEdge(b, c, One);
		graph.addEdge(c, d, One);
		graph.addEdge(d, e, One);

		graph.addEdge(a, f, All);
		graph.addEdge(f, c, All);

		graph.addEdge(b, g, All);
		graph.addEdge(g, d, All);

		// solve the data flow equations
		solve(graph);

		EXPECT_EQ("{x={1}}", toString(graph.getVertex(a).after));
		EXPECT_EQ("{x={6,7}}", toString(graph.getVertex(d).before));
		EXPECT_EQ("{x={5}}", toString(graph.getVertex(e).after));

		createDotDump(graph);

	}

	TEST(CBA_Parallel, SeqAndParallel) {

		Graph graph;

		// built the graph
		Node n0 = Node::write("x", 1);

		Node n1 = Node::noop();
		Node n2 = Node::noop();

		Node n3 = Node::write("x", 2);
		Node n4 = Node::noop();
		Node n5 = Node::write("x", 3);
		Node n6 = Node::noop();

		Node n7 = Node::noop();
		Node n8 = Node::noop();

		Node n9 = Node::noop();


		graph.addEdge(n0, n1, All);
		graph.addEdge(n0, n2, All);

		graph.addEdge(n1, n3, One);
		graph.addEdge(n1, n4, One);
		graph.addEdge(n3, n7, One);
		graph.addEdge(n4, n7, One);

		graph.addEdge(n2, n5, One);
		graph.addEdge(n2, n6, One);
		graph.addEdge(n5, n8, One);
		graph.addEdge(n6, n8, One);

		graph.addEdge(n7, n9, All);
		graph.addEdge(n8, n9, All);

		// solve the data flow equations
		solve(graph);

		EXPECT_EQ("{x={1}}", toString(graph.getVertex(n0).after));
		EXPECT_EQ("{x={1}}", toString(graph.getVertex(n1).after));
		EXPECT_EQ("{x={1}}", toString(graph.getVertex(n2).after));
		EXPECT_EQ("{x={2}}", toString(graph.getVertex(n3).after));
		EXPECT_EQ("{x={1}}", toString(graph.getVertex(n4).after));
		EXPECT_EQ("{x={3}}", toString(graph.getVertex(n5).after));
		EXPECT_EQ("{x={1}}", toString(graph.getVertex(n6).after));
		EXPECT_EQ("{x={1,2}}", toString(graph.getVertex(n7).after));
		EXPECT_EQ("{x={1,3}}", toString(graph.getVertex(n8).after));
		EXPECT_EQ("{x={1,2,3}}", toString(graph.getVertex(n9).after));

		createDotDump(graph);

	}

	TEST(CBA_Parallel, FuzzyJobs) {

		Graph graph;

		// built the graph
		Node n0 = Node::write("x", 1);
		Node n1 = Node::noop();
		Node n2 = Node::noop();

		Node n3 = Node::write("x", 2);
		Node n4 = Node::write("x", 3);

		graph.addEdge(n0, n1, All);
		graph.addEdge(n1, n2, All);

		graph.addEdge(n0, n3, One);
		graph.addEdge(n0, n4, One);
		graph.addEdge(n3, n2, One);
		graph.addEdge(n4, n2, One);

		// solve the data flow equations
		solve(graph);

		EXPECT_EQ("{x={1}}", toString(graph.getVertex(n0).after));
		EXPECT_EQ("{x={2,3}}", toString(graph.getVertex(n2).before));

		createDotDump(graph);

	}

} // end namespace prototype
} // end namespace cba
} // end namespace analysis
} // end namespace insieme
