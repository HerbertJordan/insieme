/**
 * Copyright (c) 2002-2015 Distributed and Parallel Systems Group,
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

#include "insieme/core/ir_node.h"

namespace insieme {
namespace core {

	class DummyAnnotation : public NodeAnnotation {
	public:
		static const string NAME;
		static const utils::StringKey<DummyAnnotation> DummyKey;
		int value;
		DummyAnnotation(int value) : value(value) { };

		virtual const utils::AnnotationKeyPtr getKey() const {
			return &DummyKey;
		}

		const std::string& getAnnotationName() const {
			 return NAME;
		}
		
		virtual bool migrate(const NodeAnnotationPtr& ptr, const NodePtr& before, const NodePtr& after) const {
			return false;
		}
	};

	class DummyAnnotation2 : public NodeAnnotation {
	public:
		static const string NAME; 
		static const utils::StringKey<DummyAnnotation2> DummyKey;
		int value;
		DummyAnnotation2(int value) : value(value) { };

		virtual const utils::AnnotationKeyPtr getKey() const {
			return &DummyKey;
		}

		const std::string& getAnnotationName() const {
			 return NAME;
		}
		
		virtual bool migrate(const NodeAnnotationPtr& ptr, const NodePtr& before, const NodePtr& after) const {
			assert(&*ptr == this && "Annotation pointer should reference this annotation.");
			after->addAnnotation(ptr);
			return false;
		}
	};

	// initalization of the dummy key
	const string DummyAnnotation::NAME = "DummyAnnotation";
	const utils::StringKey<DummyAnnotation> DummyAnnotation::DummyKey("DummyKey");
	const string DummyAnnotation2::NAME = "DummyAnnotation2";
	const utils::StringKey<DummyAnnotation2> DummyAnnotation2::DummyKey("DummyKey2");

} // end namespace core
} // end namespace insieme
