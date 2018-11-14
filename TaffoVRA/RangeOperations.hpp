#ifndef TAFFO_RANGE_OPERATIONS_HPP
#define TAFFO_RANGE_OPERATIONS_HPP

#include <list>

#include "Range.hpp"
#include "llvm/IR/Instruction.h"

namespace taffo {

template<typename num_t>
Range<num_t> handleInstruction(const Range<num_t> op1, const Range<num_t> op2, const unsigned OpCode);

template<typename num_t>
Range<num_t> handleAdd(const Range<num_t> op1, const Range<num_t> op2);

template<typename num_t>
Range<num_t> handleSub(const Range<num_t> op1, const Range<num_t> op2);

template<typename num_t>
Range<num_t> handleMul(const Range<num_t> op1, const Range<num_t> op2);

template<typename num_t>
Range<num_t> handleDiv(const Range<num_t> op1, const Range<num_t> op2);

template<typename num_t>
Range<num_t> handleRem(const Range<num_t> op1, const Range<num_t> op2);


// template<typename num_t, unsigned OpCode>
// Range<num_t> handleInstruction(const Range<num_t> op)

// template<typename num_t, unsigned OpCode>
// Range<num_t> handleInstruction(const std::list<Range<num_t> > op_list);


// default delete

// template<typename num_t>
// Range<num_t> handleInstruction(const Range<num_t> op) = delete;
//
// template<typename num_t>
// Range<num_t> handleInstruction(const Range<num_t> op1, const Range<num_t> op2) = delete;
//
// template<typename num_t>
// Range<num_t> handleInstruction(const std::list<Range<num_t> > op_list) = delete;

}


#endif /* end of include guard: TAFFO_RANGE_OPERATIONS_HPP */
