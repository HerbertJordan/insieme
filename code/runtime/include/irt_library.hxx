#pragma once

#ifdef IRT_LIBRARY_MAIN
#include "irt_library.h"
#else
#include "irt_globals.h"
// IRT
struct irt_joinable;
struct irt_work_item;
void irt_merge(irt_joinable*);
irt_work_item* irt_wi_get_current();
uint32 irt_wi_get_wg_num(irt_work_item *wi, uint32 index);
uint32 irt_wi_get_wg_size(irt_work_item *wi, uint32 index);
// IRT lib
void irt_lib_merge_all();
typedef void (*voidfp)(void*);
irt_joinable* irt_lib_parallel(uint32 min, uint32 max, voidfp fun, void* data, size_t data_size);
typedef void (*loopfp)(int64 index, void* data);
void irt_lib_pfor(int64 begin, int64 end, int64 step, loopfp body, void* data, size_t data_size);
#endif

#include <iostream>

namespace irt {

	// Implementation details
	namespace detail {
		template<class Callable>
		void _cpp_par_wrapper(void *callable) {
			(*((Callable*)callable))();
		}

		template<class LoopCallable>
		void _cpp_loop_wrapper(int64 num, void *callable) {
			(*((LoopCallable*)callable))(num);
		}
	}

	inline void merge_all() {
		irt_lib_merge_all();
	}

	inline void merge(irt_joinable* target) {
		irt_merge(target);
	}

	inline uint32 thread_num() {
		return irt_wi_get_wg_num(irt_wi_get_current(), 0);
	}

	inline uint32 group_size() {
		return irt_wi_get_wg_size(irt_wi_get_current(), 0);
	}

	// Executes "num" parallel instances of the callable "fun"
	template<class Callable>
	inline irt_joinable* parallel(int64 num, const Callable& fun) {
		return irt_lib_parallel(num, num, &detail::_cpp_par_wrapper<Callable>, (void*)&fun, sizeof(Callable));
	}

	// Executes "fun" in parallel with the default number of instances
	// (set via environment variables, or lacking that to the number of CPUs in the system)
	template<class Callable>
	inline irt_joinable* parallel(const Callable& fun) {
		return parallel(irt_g_worker_count, fun);
	}

	// Executes "fun" for each loop iteration from "begin" to "end" with step "step",
	// on the current team of parallel threads, or a new one if there isn't any
	template<class LoopCallable>
	inline void pfor(int64 begin, int64 end, int64 step, const LoopCallable& fun) {
		if(irt_wi_get_wg_size(irt_wi_get_current(), 0) < 1) {
			irt::merge( irt::parallel([&](){ irt_lib_pfor(begin, end, step, &detail::_cpp_loop_wrapper<LoopCallable>, (void*)&fun, sizeof(LoopCallable)); } ) );
		} else {
			irt_lib_pfor(begin, end, step, &detail::_cpp_loop_wrapper<LoopCallable>, (void*)&fun, sizeof(LoopCallable));
		}
		return;
	}

	// Executes "fun" for each element of the given container in parallel
	template<class ElemCallable, class Container>
	inline void pfor(Container& container, ElemCallable fun) {
		pfor(0, container.size(), 1, [&](int64 i) { fun(container[i]); });
	}

	// Executes "fun" for each element of the given container in parallel
	template<class Iter, class ElemCallable>
	inline void pfor(const Iter& begin, const Iter& end, ElemCallable fun) {
		pfor(0, std::distance(end,begin), 1, [&](int64 i) { fun(begin + i); });
	}

	// Maps each element of the given container to the result of executing "mapper" on it (in place)
	template<class MapCallable, class Container>
	inline void pmap(Container& container, MapCallable mapper) {
		pfor(0, container.size(), 1, [&](int64 i) { container[i] = mapper(container[i]); });
	}

	// a barrier for the current work group
	inline void barrier() {
		irt_wg_barrier(irt_wi_get_wg(irt_wi_get_current(),0));
	}


	inline void critical_start() {
		irt_lib_critical_start();
	}

	inline void critical_end() {
		irt_lib_critical_end();
	}
};
