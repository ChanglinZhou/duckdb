//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/function/aggregate/algebraic/corr.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/function/aggregate_function.hpp"
#include "duckdb/function/aggregate/algebraic_functions.hpp"
#include "duckdb/function/aggregate/algebraic/covar.hpp"
#include "duckdb/function/aggregate/algebraic/stddev.hpp"

namespace duckdb {

struct CorrState {
	CovarState cov_pop;
	StddevState dev_pop_x;
	StddevState dev_pop_y;
};

// Returns the correlation coefficient for non-null pairs in a group.
// CORR(y, x) = COVAR_POP(y, x) / (STDDEV_POP(x) * STDDEV_POP(y))
struct CorrOperation {
	template <class STATE>
	static void Initialize(STATE *state) {
		CovarOperation::Initialize<CovarState>(&state->cov_pop);
		STDDevBaseOperation::Initialize<StddevState>(&state->dev_pop_x);
		STDDevBaseOperation::Initialize<StddevState>(&state->dev_pop_y);
	}

	template <class A_TYPE, class B_TYPE, class STATE, class OP>
	static void Operation(STATE *state, FunctionData *bind_data, A_TYPE *x_data, B_TYPE *y_data, nullmask_t &anullmask,
	                      nullmask_t &bnullmask, idx_t xidx, idx_t yidx) {
		CovarOperation::Operation<A_TYPE, B_TYPE, CovarState, OP>(&state->cov_pop, bind_data, x_data, y_data, anullmask,
		                                                          bnullmask, xidx, yidx);
		STDDevBaseOperation::Operation<A_TYPE, StddevState, OP>(&state->dev_pop_x, bind_data, x_data, anullmask, xidx);
		STDDevBaseOperation::Operation<B_TYPE, StddevState, OP>(&state->dev_pop_y, bind_data, y_data, bnullmask, yidx);
	}

	template <class STATE, class OP>
	static void Combine(STATE source, STATE *target) {
		CovarOperation::Combine<CovarState, OP>(source.cov_pop, &target->cov_pop);
		STDDevBaseOperation::Combine<StddevState, OP>(source.dev_pop_x, &target->dev_pop_x);
		STDDevBaseOperation::Combine<StddevState, OP>(source.dev_pop_y, &target->dev_pop_y);
	}

	template <class T, class STATE>
	static void Finalize(Vector &result, FunctionData *, STATE *state, T *target, nullmask_t &nullmask, idx_t idx) {
		if (state->cov_pop.count == 0 || state->dev_pop_x.count == 0 || state->dev_pop_y.count == 0) {
			nullmask[idx] = true;
		} else {
			auto cov = state->cov_pop.co_moment / state->cov_pop.count;
			auto std_x = state->dev_pop_x.count > 1 ? sqrt(state->dev_pop_x.dsquared / state->dev_pop_x.count) : 0;
			if (!Value::DoubleIsValid(std_x)) {
				throw OutOfRangeException("STDDEV_POP for X is invalid!");
			}
			auto std_y = state->dev_pop_y.count > 1 ? sqrt(state->dev_pop_y.dsquared / state->dev_pop_y.count) : 0;
			if (!Value::DoubleIsValid(std_y)) {
				throw OutOfRangeException("STDDEV_POP for Y is invalid!");
			}
			if (std_x * std_y == 0) {
				nullmask[idx] = true;
				return;
			}
			target[idx] = cov / (std_x * std_y);
		}
	}

	static bool IgnoreNull() {
		return true;
	}
};

} // namespace duckdb
