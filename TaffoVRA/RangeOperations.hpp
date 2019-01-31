#ifndef TAFFO_RANGE_OPERATIONS_HPP
#define TAFFO_RANGE_OPERATIONS_HPP

#include <list>

#include "Range.hpp"
#include "llvm/IR/Instruction.h"

namespace taffo {

/** Handle binary instructions */
range_ptr_t handleBinaryInstruction(const range_ptr_t &op1,
                                     const range_ptr_t &op2,
                                     const unsigned OpCode);

range_ptr_t handleMemoryInstruction(const range_ptr_t &op,
                                     const unsigned OpCode);

#if LLVM_VERSION > 7
/** Handle unary instructions */
range_ptr_t handleUnaryInstruction(const range_ptr_t &op,
                                    const unsigned OpCode);
#endif

/** Handle cast instructions */
range_ptr_t handleCastInstruction(const range_ptr_t &op,
                                   const unsigned OpCode);

/** operator+ */
range_ptr_t handleAdd(const range_ptr_t &op1, const range_ptr_t &op2);

/** operator- */
range_ptr_t handleSub(const range_ptr_t &op1, const range_ptr_t &op2);

/** operator* */
range_ptr_t handleMul(const range_ptr_t &op1, const range_ptr_t &op2);

/** operator/ */
range_ptr_t handleDiv(const range_ptr_t &op1, const range_ptr_t &op2);

/** operator% */
range_ptr_t handleRem(const range_ptr_t &op1, const range_ptr_t &op2);

/** Cast To Unsigned Integer */
range_ptr_t handleCastToUI(const range_ptr_t &op);

/** Cast To Signed Integer */
range_ptr_t handleCastToSI(const range_ptr_t &op);

/** boolean Xor instruction */
range_ptr_t handleBooleanXor(const range_ptr_t &op1, const range_ptr_t &op2);

/** boolean And instruction */
range_ptr_t handleBooleanAnd(const range_ptr_t &op1, const range_ptr_t &op2);

/** boolean Or instruction */
range_ptr_t handleBooleanOr(const range_ptr_t &op1, const range_ptr_t &op2);

/** deep copy of range */
range_ptr_t copyRange(const range_ptr_t &op);

/** create a generic boolean range */
range_ptr_t getGenericBoolRange();

/** create a always false boolean range */
range_ptr_t getAlwaysFalse();

/** create a always false boolean range */
range_ptr_t getAlwaysTrue();

}


#endif /* end of include guard: TAFFO_RANGE_OPERATIONS_HPP */
