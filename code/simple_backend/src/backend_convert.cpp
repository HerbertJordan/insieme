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

#include "insieme/simple_backend/backend_convert.h"

#include "insieme/simple_backend/variable_manager.h"
#include "insieme/simple_backend/statement_converter.h"
#include "insieme/simple_backend/code_management.h"
#include "insieme/simple_backend/transform/preprocessor.h"

#include "insieme/core/types.h"
#include "insieme/core/transform/manipulation.h"
#include "insieme/core/analysis/ir_utils.h"
#include "insieme/core/ast_builder.h"

#include "insieme/utils/logging.h"


#include "insieme/core/printer/pretty_printer.h"


namespace insieme {
namespace simple_backend {
	
using namespace core;
using namespace utils::log;

	/**
	 * A map from Entry points to Code sections returned by ConversionContext::convert.
	 * Can be printed to any output stream
	 */
	class ConvertedCode : public TargetCode {

		/**
		 * A special list of headers to be included. This fragment will always be printed
		 * before the fragment representing the actual target code.
		 */
		const std::vector<string> headers;

		/**
		 * The code fragment covering the actual target code.
		 */
		const CodeFragmentPtr code;

	public:

		/**
		 * A constructor for this class.
		 */
		ConvertedCode(const core::ProgramPtr& source, const std::vector<string>& headers, const CodeFragmentPtr& code)
			: TargetCode(source), headers(headers), code(code) { }

		/**
		 * This method allows to print the result to an output stream.
		 */
		virtual std::ostream& printTo(std::ostream& out) const;

	};


	std::ostream& ConvertedCode::printTo(std::ostream& out) const {

		// print some general header information ...
		out << "// --- Generated Inspire Code ---\n";

		// print headers
		for_each(headers, [&](const string& cur) {
			out << cur << std::endl;
		});

		// add the program code
		return ::operator<<(out, this->code);
	}

	TargetCodePtr Converter::convert(const core::ProgramPtr& prog) {

		// obtain headers
		std::vector<string> headers = stmtConverter->getHeaderDefinitions();

		// create a code fragment covering entire program
		CodeFragmentPtr code = CodeFragment::createNewDummy("Full Program");

		// preprocess program
		NodeManager manager;
		core::NodePtr program = transform::preprocess(manager, prog);

		// convert code
		getStmtConverter().convert(program, code);

		// create resulting, converted code
		return std::make_shared<ConvertedCode>(prog, headers, code);
	}


}
}

