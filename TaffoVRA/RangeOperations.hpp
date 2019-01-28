#ifndef TAFFO_RANGE_OPERATIONS_HPP
#define TAFFO_RANGE_OPERATIONS_HPP

#include <list>

#include "Range.hpp"
#include "llvm/IR/Instruction.h"

namespace taffo {

/** Handle binary instructions */
template<typename num_t>
Range<num_t> handleBinaryInstruction(const Range<num_t> op1,
                                     const Range<num_t> op2,
                                     const unsigned OpCode);

template<typename num_t>
Range<num_t> handleMemoryInstruction(const Range<num_t> op,
                                     const unsigned OpCode);

/** Handle unary instructions */
template<typename num_t>
Range<num_t> handleUnaryInstruction(const Range<num_t> op,
                                    const unsigned OpCode);

/** Handle cast instructions */
template<typename num_t>
Range<num_t> handleCastInstruction(const Range<num_t> op,
                                   const unsigned OpCode);

/** operator+ */
template<typename num_t>
Range<num_t> handleAdd(const Range<num_t> op1, const Range<num_t> op2);

/** operator- */
template<typename num_t>
Range<num_t> handleSub(const Range<num_t> op1, const Range<num_t> op2);

/** operator* */
template<typename num_t>
Range<num_t> handleMul(const Range<num_t> op1, const Range<num_t> op2);

/** operator/ */
template<typename num_t>
Range<num_t> handleDiv(const Range<num_t> op1, const Range<num_t> op2);

/** operator% */
template<typename num_t>
Range<num_t> handleRem(const Range<num_t> op1, const Range<num_t> op2);

/** Cast To Unsigned Integer */
template<typename num_t>
Range<num_t> handleCastToUI(const Range<num_t> op);

/** Cast To Signed Integer */
template<typename num_t>
Range<num_t> handleCastToSI(const Range<num_t> op);

/** deep copy of range */
template<typename num_t>
Range<num_t> copyRange(const Range<num_t> op);

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
