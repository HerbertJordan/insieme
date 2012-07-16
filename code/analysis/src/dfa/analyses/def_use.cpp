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

#include "insieme/analysis/dfa/analyses/def_use.h"

#include "insieme/analysis/dfa/analyses/reaching_defs.h"
#include "insieme/analysis/dfa/entity.h"
#include "insieme/analysis/dfa/solver.h"


namespace insieme { namespace analysis { namespace dfa { namespace analyses {

struct DefUse::DefUseImpl {

	CFGPtr cfg;
	std::map<size_t, typename ReachingDefinitions::value_type> analysis;

	DefUseImpl(const core::NodePtr& root) : 
		cfg(CFG::buildCFG(root))
	{
		Solver<ReachingDefinitions> s(*cfg);
		analysis = s.solve();

		std::cout << analysis << std::endl;
	}
	
};


DefUse::DefUse(const core::NodePtr& root) : 
	pimpl( std::make_shared<DefUse::DefUseImpl>(root) ) { }

struct DefUse::defs_iterator_impl {

	typedef typename ReachingDefinitions::value_type::const_iterator iterator;

	core::VariableAddress var;
	iterator it, end;

	defs_iterator_impl(const core::VariableAddress& var, const iterator& begin, const iterator& end) :
		var(var), it(begin), end(end) { }
};

bool DefUse::defs_iterator::operator==(const defs_iterator& other) const {
	return pimpl->var == other.pimpl->var && pimpl->it == other.pimpl->it;
}

DefUse::defs_iterator DefUse::defs_begin(const core::VariableAddress& var) const {
	
	cfg::BlockPtr block = pimpl->cfg->find(var);
	auto& reaching_defs = pimpl->analysis[block->getBlockID()];

	return defs_iterator( 
			std::make_shared<DefUse::defs_iterator_impl>(var, reaching_defs.begin(), reaching_defs.end()) 
		);
}

DefUse::defs_iterator DefUse::defs_end(const core::VariableAddress& var) const {
	
	cfg::BlockPtr block = pimpl->cfg->find(var);
	auto& reaching_defs = pimpl->analysis[block->getBlockID()];

	return defs_iterator( 
			std::make_shared<DefUse::defs_iterator_impl>(var, reaching_defs.end(), reaching_defs.end())
		);
}


core::VariableAddress DefUse::defs_iterator::operator*() const { 
	assert(pimpl->it != pimpl->end);

	auto cur = std::get<0>(*pimpl->it);

	core::NodeAddress block = (*std::get<1>(*pimpl->it))[0].getStatementAddress();
	core::NodeAddress addr = core::Address<const core::Node>::find( pimpl->var, block.getAddressedNode());

	return core::static_address_cast<const core::Variable>(core::concat(block, addr));
}


void DefUse::defs_iterator::inc(bool first) {
	
	if (!first) { ++pimpl->it; }

	while(pimpl->it != pimpl->end && 
		  std::get<0>(*pimpl->it) != pimpl->var.getAddressedNode()) { ++(pimpl->it); }
}






} } } } // end insieme::analysis::dfa::analyses