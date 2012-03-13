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
#include "insieme/utils/func_pipeline.h"

using namespace insieme::utils;

TEST(FunctionPipeline, OneStage) {
	
	auto input = std::make_tuple(10,10);
	auto output = std::make_tuple(1,2,3);

	Function<2,0,1> f(
			input, output, std::function<int (const int&, const int&)>(std::plus<int>())
		);

	EXPECT_EQ(3, std::get<2>(output));
	f();
	EXPECT_EQ(20, std::get<2>(output));
}

TEST(FunctionPipeline, TwoStage) {

	auto s = makeStage<std::tuple<std::string, int, float>, std::tuple<int, int>>();

	s->out_buffer() = std::make_tuple(1, 2); 
	s->in_buffer() = std::make_tuple(std::string("hello world"), 10, 0.4);

	s->add( InOut<1,0>(), std::bind(&std::string::size, std::placeholders::_1) );
	s->add( InOut<1,1,2>(), std::minus<int>() );

	EXPECT_EQ(2, std::get<1>(s->out_buffer()));
	(*s)();
	EXPECT_EQ(10, std::get<1>(s->out_buffer()));
}

TEST(FunctionPipeline, Pipeline) {

	typedef std::tuple<std::string, int, float> InBuffer;
	typedef std::tuple<int,int> InterBuffer;
	typedef std::tuple<int> OutBuffer;

	auto&& s1 = makeStage<InBuffer,InterBuffer>();
	auto&& s2 = makeStage<InterBuffer,OutBuffer>();
	auto&& s3 = makeStage<OutBuffer, OutBuffer>();

	// Filling input buffer

	s1->in_buffer() = std::make_tuple(std::string("hello world"), 10, 0.4);
	s1->add( InOut<0,0>(), std::bind(&std::string::size, std::placeholders::_1) );
	s1->add( InOut<1,1,2>(), std::minus<int>() );
	s2->add( InOut<0,0,1>(), std::plus<int>() );
	s3->add( InOut<0,0>(), id<int>() );

	// Build the pipeline
	auto&& p = s1 >> s2 >> s3;

	typedef Pipeline<InBuffer,OutBuffer> Pipeline1;
	const Pipeline1::out_buff& buff = p();

	// Check the internal buffer state
	EXPECT_EQ(11, std::get<0>( s1->out_buffer() ));
	EXPECT_EQ(10, std::get<1>( s1->out_buffer() ));

	EXPECT_EQ(11, std::get<0>( s2->in_buffer() ));

	EXPECT_EQ(10, std::get<1>( s2->in_buffer() ));

	// Check the outpt buffer state
	EXPECT_EQ(21, std::get<0>(buff));
	EXPECT_EQ(21, std::get<0>(s2->out_buffer()));
	EXPECT_EQ(21, std::get<0>(s2->out_buffer()));
}

TEST(FunctionPipeline, Parallel) {

	typedef std::tuple<std::string, int, float> InBuffer;
	typedef std::tuple<int,int> InterBuffer;
	typedef std::tuple<int> OutBuffer;

	auto&& s1 = makeStage<InBuffer,InterBuffer>();
	auto&& s2 = makeStage<InterBuffer,OutBuffer>();
	auto&& s3 = makeStage<OutBuffer, OutBuffer>();
	s1->add( InOut<0,1>(), id<int>() );
	s1->add( InOut<1,1>(), id<int>() );
	s2->add( InOut<0,0>(), id<int>() );
	s3->add( InOut<0,0>(), id<int>() );

	auto&& s4 = makeStage<InBuffer,InterBuffer>();
	auto&& s5 = makeStage<InterBuffer,OutBuffer>();
	s4->add( InOut<0,1>(), id<int>() );
	s4->add( InOut<1,1>(), id<int>() );
	s5->add( InOut<0,0>(), id<int>() );

	auto&& par = (s1 >> s2 >> s3) | (s4 >> s5);

	par->in_buffer() = std::make_tuple(std::string("hello world"), 10, 0.4, std::string("hello C++11"), 5, 3.4);

	// Trigger the stored action
	par();

	EXPECT_EQ(par->out_buffer(), std::make_tuple(10,5));
}

TEST(FunctionPipeline, Sum) {

	typedef std::tuple<int> Buffer;

	auto&& s1 = makeStage<std::tuple<std::string>,Buffer>();
	auto&& s2 = makeStage<std::tuple<int>,Buffer>();

	s1->add( InOut<0,0>(), std::bind(&std::string::size, std::placeholders::_1) );
	s2->add( InOut<0,0>(), id<int>() );

	auto&& sum = s1 + s2;

	sum->in_buffer() = std::make_tuple(std::string("hello world"), 10);
	// Trigger the stored action
	sum();

	EXPECT_EQ(sum->out_buffer(), std::make_tuple(21));
}



