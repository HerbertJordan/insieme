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

#include "insieme/transform/pattern/pattern.h"
#include "insieme/transform/pattern/structure.h"

namespace insieme {
namespace transform {
namespace pattern {

	bool isMatch(const TreePatternPtr& pattern, const TreePtr& tree) {
		return details::match(pattern, tree);
	}

	bool noMatch(const TreePatternPtr& pattern, const TreePtr& tree) {
		return !isMatch(pattern, tree);
	}

	bool isMatchList(const ListPatternPtr& pattern, const vector<TreePtr>& list) {
		return details::match(pattern, list);
	}

	bool noMatchList(const ListPatternPtr& pattern, const vector<TreePtr>& list) {
		return !isMatchList(pattern, list);
	}

	TEST(TreePattern, Basic) {

		TreePtr treeA = makeTree('a');
		TreePtr treeB = makeTree('b');
		EXPECT_EQ("a", toString(treeA));
		EXPECT_EQ("b", toString(treeB));

		TreePatternPtr pattern = atom(treeA);
		EXPECT_EQ("a", toString(pattern));

		EXPECT_PRED2(isMatch, pattern, treeA);
		EXPECT_PRED2(noMatch, pattern, treeB);

		pattern = atom(treeB);
		EXPECT_EQ("b", toString(pattern));
		EXPECT_PRED2(noMatch, pattern, treeA);
		EXPECT_PRED2(isMatch, pattern, treeB);

		pattern = atom(treeA) | atom(treeB);
		EXPECT_EQ("a | b", toString(pattern));
		EXPECT_PRED2(isMatch, pattern, treeA);
		EXPECT_PRED2(isMatch, pattern, treeB);

		pattern = !pattern;
		EXPECT_EQ("!(a | b)", toString(pattern));
		EXPECT_PRED2(noMatch, pattern, treeA);
		EXPECT_PRED2(noMatch, pattern, treeB);

	}
	
	TEST(TreePattern, Variables) {

		TreePtr treeA = makeTree('a');
		TreePtr treeB = makeTree('b');

		TreePtr treeAs = makeTree(treeA, treeA);
		TreePtr treeAB = makeTree(treeA, treeB);
		TreePtr treeBs = makeTree(treeB, treeB);

		EXPECT_EQ("(a,a)", toString(treeAs));
		EXPECT_EQ("(a,b)", toString(treeAB));
		EXPECT_EQ("(b,b)", toString(treeBs));
		
		auto pattern1 = node(var("var") << var("var"));
		auto pattern2 = node(var("var") << !var("var"));
		
		EXPECT_EQ("($var,$var)", toString(pattern1));
		EXPECT_EQ("($var,!($var))", toString(pattern2));
		
		EXPECT_PRED2(isMatch, pattern1, treeAs);
		EXPECT_PRED2(noMatch, pattern1, treeAB);
		EXPECT_PRED2(isMatch, pattern1, treeBs);
		EXPECT_PRED2(noMatch, pattern2, treeAs);
		EXPECT_PRED2(isMatch, pattern2, treeAB);
		EXPECT_PRED2(noMatch, pattern2, treeBs);

		// check variable handling in alternative
		auto pattern3 = pattern1 | pattern2;
		EXPECT_PRED2(isMatch, pattern3, treeAs);
		EXPECT_PRED2(isMatch, pattern3, treeAB);
		EXPECT_PRED2(isMatch, pattern3, treeBs);

		// check "Herbert semantic"
		auto pattern4 = node(*var("var"));
		EXPECT_PRED2(isMatch, pattern4, treeAs);
		EXPECT_PRED2(isMatch, pattern4, treeAB);
		EXPECT_PRED2(isMatch, pattern4, treeBs);

		// check variable handling in aT
		TreePtr treeAsBs = makeTree(treeAs, treeBs);
		TreePtr treeAsAs = makeTree(treeAs, treeAs);
		TreePtr treeAsAB = makeTree(treeAs, treeAB);
		
		EXPECT_EQ("((a,a),(b,b))", toString(treeAsBs));
		EXPECT_EQ("((a,a),(a,a))", toString(treeAsAs));
		EXPECT_EQ("((a,a),(a,b))", toString(treeAsAB));

		auto pattern5 = aT(pattern2);
		EXPECT_PRED2(isMatch, pattern5, treeAsBs);
		EXPECT_PRED2(noMatch, pattern5, treeAsAs);
		EXPECT_PRED2(isMatch, pattern5, treeAsAB);
	}
	
	TEST(TreePattern, VariablesWithSubPattern) {

		TreePtr treeA = makeTree('a');
		TreePtr treeB = makeTree('b');

		TreePatternPtr pattern;

		pattern = var("x");
		EXPECT_EQ("$x", toString(pattern));
		EXPECT_PRED2(isMatch, pattern, treeA);
		EXPECT_PRED2(isMatch, pattern, treeB);

		pattern = var("x", atom(treeA));
		EXPECT_EQ("$x:a", toString(pattern));
		EXPECT_PRED2(isMatch, pattern, treeA);
		EXPECT_PRED2(noMatch, pattern, treeB);

		TreePatternPtr inner = node('a', single(any));
		TreePtr treeC = makeTree('a', makeTree('b'));

		pattern = var("x", inner);
		EXPECT_EQ("$x:(97|_)", toString(pattern));
		EXPECT_PRED2(noMatch, pattern, treeA);
		EXPECT_PRED2(noMatch, pattern, treeB);
		EXPECT_PRED2(isMatch, pattern, treeC);

	}

	TEST(TreePattern, Recursion) {
		
		TreePtr a = makeTree('a');
		TreePtr b = makeTree('b');
		TreePtr c = makeTree('c');
		TreePtr d = makeTree('d');
		
		TreePtr treeA = makeTree(a,makeTree(b,makeTree(c)));
		TreePtr treeB = makeTree(a,makeTree(b,makeTree(b,makeTree(b,makeTree(b,makeTree(c))))));
		TreePtr treeC = makeTree(a,makeTree(b,makeTree(a,makeTree(c))));
		TreePtr treeD = makeTree(a,makeTree(b,makeTree(a,makeTree(b,makeTree(c)))));
		TreePtr treeE = makeTree(a,makeTree(b,makeTree(a,makeTree(d,makeTree(c)))));

		auto pattern = node(atom(a) << rT((atom(b) << recurse) | single(c)));

		EXPECT_PRED2(isMatch, pattern, treeA);
		EXPECT_PRED2(isMatch, pattern, treeB);
		EXPECT_PRED2(noMatch, pattern, treeC);
		EXPECT_PRED2(noMatch, pattern, treeD);
		EXPECT_PRED2(noMatch, pattern, treeE);

		pattern = rT(((single(b) | single(a)) << recurse) | single(c));
		EXPECT_PRED2(isMatch, pattern, treeA);
		EXPECT_PRED2(isMatch, pattern, treeB);
		EXPECT_PRED2(isMatch, pattern, treeC);
		EXPECT_PRED2(isMatch, pattern, treeD);
		EXPECT_PRED2(noMatch, pattern, treeE);
	}

	TEST(ListPattern, Basic) {

		TreePtr treeA = makeTree('a');
		TreePtr treeB = makeTree(treeA, treeA);
		TreePtr treeC = makeTree(treeA, treeA, treeB);

		EXPECT_EQ("a", toString(treeA));
		EXPECT_EQ("(a,a)", toString(treeB));
		EXPECT_EQ("(a,a,(a,a))", toString(treeC));

		ListPatternPtr pattern = single(atom(treeA));
		TreePatternPtr treePattern = node(pattern);
		EXPECT_EQ("a", toString(pattern));
		EXPECT_EQ("(a)", toString(treePattern));

		EXPECT_PRED2(noMatch, node(pattern), treeA);
		EXPECT_PRED2(noMatch, node(pattern), treeB);
		EXPECT_PRED2(noMatch, node(pattern), treeC);

		// test sequence
		pattern = pattern << pattern;
		treePattern = node(pattern);
		EXPECT_EQ("a,a", toString(pattern));
		EXPECT_EQ("(a,a)", toString(treePattern));

		EXPECT_PRED2(noMatch, node(pattern), treeA);
		EXPECT_PRED2(isMatch, node(pattern), treeB);
		EXPECT_PRED2(noMatch, node(pattern), treeC);

		// test repetition
		pattern = *atom(treeA);
		treePattern = node(pattern);
		EXPECT_EQ("[a]*", toString(pattern));
		EXPECT_EQ("([a]*)", toString(treePattern));

		EXPECT_PRED2(isMatch, node(pattern), treeA);
		EXPECT_PRED2(isMatch, node(pattern), treeB);
		EXPECT_PRED2(noMatch, node(pattern), treeC);

		// combination
		pattern = pattern << atom(treeB);
		treePattern = node(pattern);
		EXPECT_EQ("[a]*,(a,a)", toString(pattern));
		EXPECT_EQ("([a]*,(a,a))", toString(treePattern));

		EXPECT_PRED2(noMatch, node(pattern), treeA);
		EXPECT_PRED2(noMatch, node(pattern), treeB);
		EXPECT_PRED2(isMatch, node(pattern), treeC);

	}

	TEST(Wildcard, Basic) {

		TreePtr treeA = makeTree('a');
		TreePtr treeB = makeTree(treeA, treeA);
		TreePtr treeC = makeTree(treeA, treeA, treeB);

		EXPECT_EQ("(a,a,(a,a))", toString(treeC));

		auto pattern = single(treeA) << single(treeA) << any;
		EXPECT_EQ("a,a,_", toString(pattern));

		EXPECT_PRED2(noMatch, node(pattern), treeA);
		EXPECT_PRED2(noMatch, node(pattern), treeB);
		EXPECT_PRED2(isMatch, node(pattern), treeC);


		pattern = single(treeA) << any << single(treeA);
		EXPECT_EQ("a,_,a", toString(pattern));

		EXPECT_PRED2(noMatch, node(pattern), treeA);
		EXPECT_PRED2(noMatch, node(pattern), treeB);
		EXPECT_PRED2(noMatch, node(pattern), treeC);
	}

	TEST(Sequence, Basic) {

		TreePtr treeA = makeTree('a');
		TreePtr treeB = makeTree('b');

		EXPECT_EQ("a", toString(treeA));
		EXPECT_EQ("b", toString(treeB));

		ListPatternPtr pattern = single(treeA);

		EXPECT_PRED2(isMatchList, pattern, toVector(treeA));
		EXPECT_PRED2(noMatchList, pattern, toVector(treeA, treeA));

		pattern = single(treeA) << single(treeA);
		EXPECT_PRED2(isMatchList, pattern, toVector(treeA, treeA));

		pattern = single(treeA) << single(treeB);
		EXPECT_PRED2(noMatchList, pattern, toVector(treeA, treeA));
		EXPECT_PRED2(isMatchList, pattern, toVector(treeA, treeB));

		TreePatternPtr x = var("x");
		pattern = single(x) << single(x);
		EXPECT_PRED2(isMatchList, pattern, toVector(treeA, treeA));
		EXPECT_PRED2(noMatchList, pattern, toVector(treeA, treeB));

		// test wildcard
		pattern = any << single(treeA);
		EXPECT_PRED2(isMatchList, pattern, toVector(treeA, treeA));
		EXPECT_PRED2(isMatchList, pattern, toVector(treeB, treeA));
		EXPECT_PRED2(noMatchList, pattern, toVector(treeA, treeB));

		// test potential empty left side
		pattern = *any << single(treeA);
		EXPECT_PRED2(isMatchList, pattern, toVector(treeA));
		EXPECT_PRED2(isMatchList, pattern, toVector(treeB, treeA));
		EXPECT_PRED2(isMatchList, pattern, toVector(treeB, treeB, treeA));
		EXPECT_PRED2(isMatchList, pattern, toVector(treeB, treeA, treeA));
		EXPECT_PRED2(noMatchList, pattern, toVector(treeA, treeB));

		// test potential empty right side
		pattern = single(treeA) << *any;
		EXPECT_PRED2(isMatchList, pattern, toVector(treeA));
		EXPECT_PRED2(isMatchList, pattern, toVector(treeA, treeB));
		EXPECT_PRED2(isMatchList, pattern, toVector(treeA, treeB, treeB));
		EXPECT_PRED2(isMatchList, pattern, toVector(treeA, treeA, treeB));
		EXPECT_PRED2(noMatchList, pattern, toVector(treeB, treeA));

		// test both sides being of abitrary length
		pattern = (*any << single(treeA)) << *any;
		EXPECT_PRED2(isMatchList, pattern, toVector(treeA));
		EXPECT_PRED2(isMatchList, pattern, toVector(treeA, treeB));
		EXPECT_PRED2(isMatchList, pattern, toVector(treeA, treeB, treeB));
		EXPECT_PRED2(isMatchList, pattern, toVector(treeB, treeA, treeB));
		EXPECT_PRED2(isMatchList, pattern, toVector(treeB, treeB, treeA));
		EXPECT_PRED2(isMatchList, pattern, toVector(treeA, treeA, treeB));
		EXPECT_PRED2(noMatchList, pattern, toVector(treeB, treeB));

		pattern = *any << (single(treeA) << *any);
		EXPECT_PRED2(isMatchList, pattern, toVector(treeA));
		EXPECT_PRED2(isMatchList, pattern, toVector(treeA, treeB));
		EXPECT_PRED2(isMatchList, pattern, toVector(treeA, treeB, treeB));
		EXPECT_PRED2(isMatchList, pattern, toVector(treeB, treeA, treeB));
		EXPECT_PRED2(isMatchList, pattern, toVector(treeB, treeB, treeA));
		EXPECT_PRED2(isMatchList, pattern, toVector(treeA, treeA, treeB));
		EXPECT_PRED2(noMatchList, pattern, toVector(treeB, treeB));

	}

	TEST(Empty, Basic) {

		TreePtr a = makeTree('a');

		EXPECT_EQ("a", toString(a));

		ListPatternPtr pattern = empty;

		EXPECT_PRED2(isMatchList, pattern, toVector<TreePtr>());
		EXPECT_PRED2(noMatchList, pattern, toVector<TreePtr>(a));
	}

	TEST(Optional, Basic) {

		TreePtr a = makeTree('a');

		EXPECT_EQ("a", toString(a));

		ListPatternPtr pattern = opt(a);

		EXPECT_PRED2(isMatchList, pattern, toVector<TreePtr>());
		EXPECT_PRED2(isMatchList, pattern, toVector<TreePtr>(a));
		EXPECT_PRED2(noMatchList, pattern, toVector<TreePtr>(a,a));
	}

	TEST(Repetition, Basic) {

		TreePtr a = makeTree('a');
		TreePtr b = makeTree('b');

		EXPECT_EQ("a", toString(a));
		EXPECT_EQ("b", toString(b));

		ListPatternPtr pattern = single(a);

		EXPECT_PRED2(isMatchList, pattern, toVector(a));

		pattern = *single(a);
		EXPECT_PRED2(isMatchList, pattern, toVector<TreePtr>());
		EXPECT_PRED2(isMatchList, pattern, toVector<TreePtr>(a));
		EXPECT_PRED2(isMatchList, pattern, toVector<TreePtr>(a,a));
		EXPECT_PRED2(noMatchList, pattern, toVector(a,b,a));

		// test variable binding
		TreePatternPtr x = var("x");
		pattern = *single(x);
		EXPECT_PRED2(isMatchList, pattern, toVector<TreePtr>(a));
		EXPECT_PRED2(isMatchList, pattern, toVector<TreePtr>(a,a));
		EXPECT_PRED2(isMatchList, pattern, toVector<TreePtr>(a,b));
		EXPECT_PRED2(isMatchList, pattern, toVector<TreePtr>(a,b,a));

		// test pre-bined variable
		pattern = single(x) << *single(x);
		EXPECT_PRED2(isMatchList, pattern, toVector<TreePtr>(a));
		EXPECT_PRED2(isMatchList, pattern, toVector<TreePtr>(a,a));
		EXPECT_PRED2(isMatchList, pattern, toVector<TreePtr>(b,b));
		EXPECT_PRED2(isMatchList, pattern, toVector<TreePtr>(a,a,a));
		EXPECT_PRED2(noMatchList, pattern, toVector<TreePtr>(a,a,a,b));
		EXPECT_PRED2(noMatchList, pattern, toVector<TreePtr>(a,b,a,b));
	}

	TEST(Repetition, MinRep) {

		TreePtr a = makeTree('a');
		TreePtr b = makeTree('b');

		EXPECT_EQ("a", toString(a));
		EXPECT_EQ("b", toString(b));

		ListPatternPtr pattern = single(a);

		EXPECT_PRED2(isMatchList, pattern, toVector(a));

		pattern = +single(a);
		EXPECT_PRED2(noMatchList, pattern, toVector<TreePtr>());
		EXPECT_PRED2(isMatchList, pattern, toVector<TreePtr>(a));
		EXPECT_PRED2(isMatchList, pattern, toVector<TreePtr>(a,a));
		EXPECT_PRED2(noMatchList, pattern, toVector(a,b,a));

		// test variable binding
		TreePatternPtr x = var("x");
		pattern = +single(x);
		EXPECT_PRED2(isMatchList, pattern, toVector<TreePtr>(a));
		EXPECT_PRED2(isMatchList, pattern, toVector<TreePtr>(a,a));
		EXPECT_PRED2(isMatchList, pattern, toVector<TreePtr>(a,b));
		EXPECT_PRED2(isMatchList, pattern, toVector<TreePtr>(a,b,a));

		// test pre-bined variable
		pattern = single(x) << +single(x);
		EXPECT_PRED2(noMatchList, pattern, toVector<TreePtr>(a));
		EXPECT_PRED2(isMatchList, pattern, toVector<TreePtr>(a,a));
		EXPECT_PRED2(isMatchList, pattern, toVector<TreePtr>(b,b));
		EXPECT_PRED2(isMatchList, pattern, toVector<TreePtr>(a,a,a));
		EXPECT_PRED2(noMatchList, pattern, toVector<TreePtr>(a,a,a,b));
		EXPECT_PRED2(noMatchList, pattern, toVector<TreePtr>(a,b,a,b));
	}

	TEST(ListPattern, Extended) {

		TreePtr a = makeTree('a');
		TreePtr b = makeTree('b');
		TreePtr c = makeTree('c');

		auto pattern = *(single(a) | (single(a) << single(b)) | single(c));

		EXPECT_EQ("[a|a,b|c]*", toString(pattern));
		EXPECT_EQ("([a|a,b|c]*)", toString(node(pattern)));

		// these should all isMatch, since the pattern allows an empty node
		EXPECT_PRED2(isMatch, node(pattern), a);
		EXPECT_PRED2(isMatch, node(pattern), b);
		EXPECT_PRED2(isMatch, node(pattern), c);


		// these should all isMatch, since the pattern allows an empty node
		EXPECT_PRED2(isMatch, node(pattern), makeTree(a));
		EXPECT_PRED2(noMatch, node(pattern), makeTree(b));
		EXPECT_PRED2(isMatch, node(pattern), makeTree(c));
	}

	TEST(Descendant, Basic) {

		// build up test-tree
		TreePtr a = makeTree('a');
		TreePtr b = makeTree('b');
		TreePtr c = makeTree('c');

		TreePtr treeA = makeTree(a,a,a);
		TreePtr treeB = makeTree(a,b,a);
		TreePtr treeC = makeTree(b,a,b);
		TreePtr treeD = makeTree(treeA,treeB,treeC);
		TreePtr treeE = makeTree(b,makeTree(b,makeTree(b,makeTree(a))));

		EXPECT_EQ("a", toString(a));
		EXPECT_EQ("b", toString(b));
		EXPECT_EQ("c", toString(c));

		EXPECT_EQ("(a,a,a)", toString(treeA));
		EXPECT_EQ("(a,b,a)", toString(treeB));
		EXPECT_EQ("(b,a,b)", toString(treeC));
		EXPECT_EQ("((a,a,a),(a,b,a),(b,a,b))", toString(treeD));
		EXPECT_EQ("(b,(b,(b,(a))))", toString(treeE));


		// create a descendant pattern
		auto patternA = aT(atom(a));
		auto patternB = aT(atom(b));
		auto patternC = aT(atom(c));
		auto patternD = aT(atom(a), atom(b));

		EXPECT_PRED2(isMatch, patternA, a);
		EXPECT_PRED2(noMatch, patternA, b);
		EXPECT_PRED2(noMatch, patternA, c);

		EXPECT_PRED2(noMatch, patternB, a);
		EXPECT_PRED2(isMatch, patternB, b);
		EXPECT_PRED2(noMatch, patternB, c);

		EXPECT_PRED2(noMatch, patternC, a);
		EXPECT_PRED2(noMatch, patternC, b);
		EXPECT_PRED2(isMatch, patternC, c);

		EXPECT_PRED2(noMatch, patternD, a);
		EXPECT_PRED2(noMatch, patternD, b);
		EXPECT_PRED2(noMatch, patternD, c);

		EXPECT_PRED2(isMatch, patternA, treeA);
		EXPECT_PRED2(isMatch, patternA, treeB);
		EXPECT_PRED2(isMatch, patternA, treeC);
		EXPECT_PRED2(isMatch, patternA, treeD);
		EXPECT_PRED2(isMatch, patternA, treeE);

		EXPECT_PRED2(noMatch, patternB, treeA);
		EXPECT_PRED2(isMatch, patternB, treeB);
		EXPECT_PRED2(isMatch, patternB, treeC);
		EXPECT_PRED2(isMatch, patternB, treeD);
		EXPECT_PRED2(isMatch, patternB, treeE);

		EXPECT_PRED2(noMatch, patternC, treeA);
		EXPECT_PRED2(noMatch, patternC, treeB);
		EXPECT_PRED2(noMatch, patternC, treeC);
		EXPECT_PRED2(noMatch, patternC, treeD);
		EXPECT_PRED2(noMatch, patternC, treeE);

		EXPECT_PRED2(noMatch, patternD, treeA);
		EXPECT_PRED2(isMatch, patternD, treeB);
		EXPECT_PRED2(isMatch, patternD, treeC);
		EXPECT_PRED2(isMatch, patternD, treeD);
		EXPECT_PRED2(isMatch, patternD, treeE);
	}


	TEST(ListPattern, Variables) {

		TreePtr a = makeTree('a');
		TreePtr b = makeTree('b');

		ListPatternPtr pattern;

		pattern = listVar("x");
		EXPECT_PRED2(isMatchList, pattern, toVector<TreePtr>());
		EXPECT_PRED2(isMatchList, pattern, toVector<TreePtr>(a));
		EXPECT_PRED2(isMatchList, pattern, toVector<TreePtr>(a,a));
		EXPECT_PRED2(isMatchList, pattern, toVector<TreePtr>(a,a,b));

		pattern = listVar("x") << listVar("x");
		EXPECT_PRED2(isMatchList, pattern, toVector<TreePtr>());
		EXPECT_PRED2(noMatchList, pattern, toVector<TreePtr>(a));
		EXPECT_PRED2(isMatchList, pattern, toVector<TreePtr>(a,a));
		EXPECT_PRED2(noMatchList, pattern, toVector<TreePtr>(a,a,b));
		EXPECT_PRED2(noMatchList, pattern, toVector<TreePtr>(a,a,b,b));
		EXPECT_PRED2(noMatchList, pattern, toVector<TreePtr>(a,a,a,b));
		EXPECT_PRED2(isMatchList, pattern, toVector<TreePtr>(a,b,a,b));


		// with limited pattern
		pattern = listVar("x", *single(a));
		pattern = pattern << pattern;  	// requests an even number of as

		EXPECT_PRED2(isMatchList, pattern, toVector<TreePtr>());
		EXPECT_PRED2(noMatchList, pattern, toVector<TreePtr>(a));
		EXPECT_PRED2(isMatchList, pattern, toVector<TreePtr>(a,a));
		EXPECT_PRED2(noMatchList, pattern, toVector<TreePtr>(a,a,a));
		EXPECT_PRED2(noMatchList, pattern, toVector<TreePtr>(a,a,b,b));
		EXPECT_PRED2(noMatchList, pattern, toVector<TreePtr>(a,a,a,b));
		EXPECT_PRED2(isMatchList, pattern, toVector<TreePtr>(a,a,a,a));

	}


	TEST(TreePattern, MatchResult) {

		TreePtr a = makeTree('a');
		TreePtr b = makeTree('b');

		TreePatternPtr pattern;
		boost::optional<Match<tree_target>> res;

		// something easy ...
		pattern = var("x");

		res = details::match(pattern, a);
		EXPECT_TRUE(res);
		EXPECT_EQ("Match({x=a})", toString(*res));

		res = details::match(pattern, b);
		EXPECT_TRUE(res);
		EXPECT_EQ("Match({x=b})", toString(*res));


		// something more challenging
		pattern = node(*var("x"));

		res = details::match(pattern, makeTree(a,b,a));
		EXPECT_TRUE(res);
		EXPECT_EQ("Match({x=[a,b,a]})", toString(*res));

		// even more challenging
		pattern = node(var("x") << *var("x"));

		res = details::match(pattern, makeTree(a,b,a));
		EXPECT_FALSE(res);

		res = details::match(pattern, makeTree(a,a,a));
		EXPECT_TRUE(res);
		EXPECT_EQ("Match({x=a})", toString(*res));


		// and one more
		pattern = node(*pattern);

		res = details::match(pattern, makeTree(makeTree(a,a,a), makeTree(b,b,b,b)));
		EXPECT_TRUE(res);
		EXPECT_EQ("Match({x=[a,b]})", toString(*res));


		// and one more
		pattern = node(*node(*var("x")));

		res = details::match(pattern, makeTree(makeTree(a,b,a), makeTree(b,a,a,b)));
		EXPECT_TRUE(res);
		EXPECT_EQ("Match({x=[[a,b,a],[b,a,a,b]]})", toString(*res));
	}

	TEST(ListPattern, MatchResult) {

		TreePtr a = makeTree('a');
		TreePtr b = makeTree('b');

		TreePatternPtr pattern;
		boost::optional<Match<tree_target>> res;

		// something easy ...
		pattern = node(listVar("x"));

		res = details::match(pattern, a);
		EXPECT_TRUE(res);
		EXPECT_EQ("Match({x=[]})", toString(*res));

		res = details::match(pattern, makeTree(a,a,b));
		EXPECT_TRUE(res);
		EXPECT_EQ("Match({x=[a,a,b]})", toString(*res));


		// something more challenging
		pattern = node(*node(listVar("x")));

		res = details::match(pattern, makeTree(makeTree(a,b),makeTree(b,a,b),a));
		EXPECT_TRUE(res);
		EXPECT_EQ("Match({x=[[a,b],[b,a,b],[]]})", toString(*res));

	}


	TEST(Match, RecursiveVars) {

		TreePtr a = makeTree('a');
		TreePtr b = makeTree('b');
		TreePtr c = makeTree('c');

		TreePatternPtr pattern;

		pattern = atom(a) | rT(var("y", node(var("x") << single(atom(a) | recurse))));
		EXPECT_EQ("a | rT.x($y:($x,a | rec.x))", toString(pattern));


		EXPECT_PRED2(isMatch, pattern, a);
		EXPECT_PRED2(isMatch, pattern, makeTree(b, a));
		EXPECT_PRED2(isMatch, pattern, makeTree(b, makeTree(b, a)));
		EXPECT_PRED2(noMatch, pattern, b);

		auto res = details::match(pattern, a);
		EXPECT_TRUE(res);
		if (res) EXPECT_EQ("Match({})", toString(*res));

		res = details::match(pattern, makeTree(b, a));
		EXPECT_TRUE(res);
		//if (res) EXPECT_EQ("Match({x=[b], y=[(b,a)]})", toString(*res)); // FIXME

		res = details::match(pattern, makeTree(b, makeTree(c, makeTree(b, a))));
		EXPECT_TRUE(res);
		// if (res) EXPECT_EQ("Match({x=[b,c,b], y=[(b,(c,(b,a))),(c,(b,a)),(b,a)]})", toString(*res)); //FIXME
	}


} // end namespace pattern
} // end namespace transform
} // end namespace insieme
