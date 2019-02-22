#include "RangeOperations.hpp"
#include "RangeOperationsCallWhitelist.hpp"

#include <assert.h>
#include <limits>
#include <map>
#include <memory>

using namespace taffo;

//-----------------------------------------------------------------------------
// Wrappers
//-----------------------------------------------------------------------------

/** Handle binary instructions */
range_ptr_t taffo::handleBinaryInstruction(const range_ptr_t &op1,
                                           const range_ptr_t &op2,
                                           const unsigned OpCode)
{
	switch (OpCode) {
		case llvm::Instruction::Add:
		case llvm::Instruction::FAdd:
			return handleAdd(op1,op2);
			break;
		case llvm::Instruction::Sub:
		case llvm::Instruction::FSub:
			return handleSub(op1,op2);
			break;
		case llvm::Instruction::Mul:
		case llvm::Instruction::FMul:
			return handleMul(op1,op2);
			break;
		case llvm::Instruction::UDiv:
		case llvm::Instruction::SDiv:
		case llvm::Instruction::FDiv:
			return handleDiv(op1,op2);
			break;
		case llvm::Instruction::URem:
		case llvm::Instruction::SRem:
		case llvm::Instruction::FRem:
			return handleRem(op1,op2);
			break;
		case llvm::Instruction::Shl: // TODO implement
		case llvm::Instruction::LShr: // TODO implement
		case llvm::Instruction::AShr: // TODO implement
		case llvm::Instruction::And: // TODO implement
		case llvm::Instruction::Or: // TODO implement
		case llvm::Instruction::Xor: // TODO implement
			break;
		default:
			assert(false); // unsupported operation
			break;
	}
	return nullptr;
}

#if LLVM_VERSION > 7
range_ptr_t taffo::handleUnaryInstruction(const range_ptr_t &op,
                                          const unsigned OpCode)
{
	switch (OpCode) {
		case llvm::Instruction::FNeg:
			return make_range(-op->max(), -op->min());
			break;
		default:
			assert(false); // unsupported operation
			break;
	}
	return nullptr;
}
#endif

/** Cast instructions */
generic_range_ptr_t taffo::handleCastInstruction(const generic_range_ptr_t &op,
                                                 const unsigned OpCode)
{
  switch (OpCode) {
		case llvm::Instruction::Trunc: // TODO implement
			break;
		case llvm::Instruction::ZExt:
		case llvm::Instruction::SExt:
			return copyRange(op);
			break;
		case llvm::Instruction::FPToUI:
			return handleCastToUI(op);
			break;
		case llvm::Instruction::FPToSI:
			return handleCastToSI(op);
			break;
		case llvm::Instruction::UIToFP:
		case llvm::Instruction::SIToFP:
			return copyRange(op);
			break;
		case llvm::Instruction::FPTrunc: // TODO implement
			break;
		case llvm::Instruction::FPExt: // TODO implement
			return copyRange(op);
			break;
		case llvm::Instruction::PtrToInt:
		case llvm::Instruction::IntToPtr:
			return handleCastToSI(op);
			break;
		case llvm::Instruction::BitCast: // TODO check
			return copyRange(op);
			break;
		case llvm::Instruction::AddrSpaceCast:
			return copyRange(op);
			break;
		default:
			assert(false); // unsupported operation
			break;
	}
	return nullptr;
}

/** Handle call to known math functions. Return nullptr if unknown */
range_ptr_t taffo::handleMathCallInstruction(const std::list<range_ptr_t>& ops,
                                             const std::string &function)
{
	const auto it = functionWhiteList.find(function);
	if (it != functionWhiteList.end()) {
		return it->second(ops);
	}
	return nullptr;
}

/** Handle call to known math functions. Return nullptr if unknown */
generic_range_ptr_t taffo::handleCompare(const std::list<generic_range_ptr_t>& ops,
                                         const llvm::CmpInst::Predicate pred)
{
	switch (pred) {
		case llvm::CmpInst::Predicate::FCMP_FALSE:
			return getAlwaysFalse();
		case llvm::CmpInst::Predicate::FCMP_TRUE:
			return getAlwaysTrue();
		default:
			break;
	}

	// from now on only 2 operators compare
	assert(ops.size() > 1 && "too few operators in compare instruction");
	assert(ops.size() <= 2 && "too many operators in compare instruction");

	// extract values for easy access
	range_ptr_t lhs = std::reinterpret_pointer_cast<range_t>(ops.front());
	range_ptr_t rhs = std::reinterpret_pointer_cast<range_t>(ops.back());
	// if unavailable data, nothing can be said
	if (!lhs || !rhs) {
		return getGenericBoolRange();
	}

	// NOTE: not dealing with Ordered / Unordered variants
	switch (pred) {
		case llvm::CmpInst::Predicate::FCMP_OEQ:
		case llvm::CmpInst::Predicate::FCMP_UEQ:
		case llvm::CmpInst::Predicate::ICMP_EQ:
			if (lhs->min() == lhs->max()
			    && rhs->min() == rhs->max()
			    && lhs->min() == rhs->min())
			{
				return getAlwaysTrue();
			} else if (lhs->max() < rhs->min() || rhs->max() < lhs->min()) {
				return getAlwaysFalse();
			} else {
				return getGenericBoolRange();
			}
			break;
		case llvm::CmpInst::Predicate::FCMP_OGT:
		case llvm::CmpInst::Predicate::FCMP_UGT:
		case llvm::CmpInst::Predicate::ICMP_UGT:
		case llvm::CmpInst::Predicate::ICMP_SGT:
			if (lhs->min() > rhs->max()) {
				return getAlwaysTrue();
			} else if (lhs->max() <= rhs->min()) {
				return getAlwaysFalse();
			} else {
				return getGenericBoolRange();
			}
			break;
		case llvm::CmpInst::Predicate::FCMP_OGE:
		case llvm::CmpInst::Predicate::FCMP_UGE:
		case llvm::CmpInst::Predicate::ICMP_UGE:
		case llvm::CmpInst::Predicate::ICMP_SGE:
			if (lhs->min() >= rhs->max()) {
				return getAlwaysTrue();
			} else if (lhs->max() < rhs->min()) {
				return getAlwaysFalse();
			} else {
				return getGenericBoolRange();
			}
			break;
		case llvm::CmpInst::Predicate::FCMP_OLT:
		case llvm::CmpInst::Predicate::FCMP_ULT:
		case llvm::CmpInst::Predicate::ICMP_ULT:
		case llvm::CmpInst::Predicate::ICMP_SLT:
			if (lhs->max() < rhs->min()) {
				return getAlwaysTrue();
			} else if (lhs->min() >= rhs->max()) {
				return getAlwaysFalse();
			} else {
				return getGenericBoolRange();
			}
			break;
		case llvm::CmpInst::Predicate::FCMP_OLE:
		case llvm::CmpInst::Predicate::FCMP_ULE:
		case llvm::CmpInst::Predicate::ICMP_ULE:
		case llvm::CmpInst::Predicate::ICMP_SLE:
			if (lhs->max() <= rhs->min()) {
				return getAlwaysTrue();
			} else if (lhs->min() > rhs->max()) {
				return getAlwaysFalse();
			} else {
				return getGenericBoolRange();
			}
			break;
		case llvm::CmpInst::Predicate::FCMP_ONE:
		case llvm::CmpInst::Predicate::FCMP_UNE:
		case llvm::CmpInst::Predicate::ICMP_NE:
			if (lhs->min() == lhs->max()
					&& rhs->min() == rhs->max()
					&& lhs->min() == rhs->min())
			{
				return getAlwaysFalse();
			} else if (lhs->max() < rhs->min() || rhs->max() < lhs->min()) {
				return getAlwaysTrue();
			} else {
				return getGenericBoolRange();
			}
			break;
		case llvm::CmpInst::Predicate::FCMP_ORD: // none of the operands is NaN
		case llvm::CmpInst::Predicate::FCMP_UNO: // one of the operand is NaN
		// TODO implement
		break;
	}
	return nullptr;
}

//-----------------------------------------------------------------------------
// Arithmetic
//-----------------------------------------------------------------------------

/** operator+ */
range_ptr_t taffo::handleAdd(const range_ptr_t &op1, const range_ptr_t &op2)
{
	if (!op1 || !op2) {
		return nullptr;
	}
	num_t a = op1->min() + op2->min();
	num_t b = op1->max() + op2->max();
	return make_range(a,b);
}

/** operator- */
range_ptr_t taffo::handleSub(const range_ptr_t &op1, const range_ptr_t &op2)
{
	if (!op1 || !op2) {
		return nullptr;
	}
	num_t a = op1->min() - op2->max();
	num_t b = op1->max() - op2->min();
	return make_range(a,b);
}

/** operator* */
range_ptr_t taffo::handleMul(const range_ptr_t &op1, const range_ptr_t &op2)
{
	if (!op1 || !op2) {
		return nullptr;
	}
	num_t a = op1->min() * op2->min();
	num_t b = op1->max() * op2->max();
	num_t c = op1->min() * op2->max();
	num_t d = op1->max() * op2->min();
	const num_t r1 = std::min({a,b,c,d});
	const num_t r2 = std::max({a,b,c,d});
	return make_range(r1,r2);
}

/** operator/ */
range_ptr_t taffo::handleDiv(const range_ptr_t &op1, const range_ptr_t &op2)
{
	if (!op1 || !op2) {
		return nullptr;
	}
	num_t a = op1->min() / op2->min();
	num_t b = op1->max() / op2->max();
	num_t c = op1->min() / op2->max();
	num_t d = op1->max() / op2->min();
	const num_t r1 = std::min({a,b,c,d});
	const num_t r2 = std::max({a,b,c,d});
	return make_range(r1,r2);
}

/** operator% */
range_ptr_t taffo::handleRem(const range_ptr_t &op1, const range_ptr_t &op2)
{
	if (!op1 || !op2) {
		return nullptr;
	}
	const bool alwaysNeg = op1->max() <= 0;
	const bool alwaysPos = op1->min() >= 0;
	const num_t bound = fabs(op2->max()) - std::numeric_limits<num_t>::epsilon();
	const num_t r1 = (alwaysNeg) ? - bound : (alwaysPos) ? 0 : -bound;
	const num_t r2 = (alwaysNeg) ? 0 : (alwaysPos) ? bound : bound;
	return make_range(r1,r2);
}

/** CastToUInteger */
generic_range_ptr_t taffo::handleCastToUI(const generic_range_ptr_t &op)
{
	const range_ptr_t scalar = std::static_pointer_cast<VRA_Range<num_t>>(op);
	if (!scalar) {
		return nullptr;
	}
	const num_t r1 = static_cast<num_t>(static_cast<unsigned long>(scalar->min()));
	const num_t r2 = static_cast<num_t>(static_cast<unsigned long>(scalar->max()));
	return make_range(r1,r2);
}

/** CastToUInteger */
generic_range_ptr_t taffo::handleCastToSI(const generic_range_ptr_t &op)
{
	const range_ptr_t scalar = std::static_pointer_cast<VRA_Range<num_t>>(op);
	if (!scalar) {
		return nullptr;
	}
	const num_t r1 = static_cast<num_t>(static_cast<long>(scalar->min()));
	const num_t r2 = static_cast<num_t>(static_cast<long>(scalar->max()));
	return make_range(r1,r2);
}

/** boolean Xor instruction */
range_ptr_t taffo::handleBooleanXor(const range_ptr_t &op1,
                                    const range_ptr_t &op2)
{
	if (!op1 || !op2) {
		return getGenericBoolRange();
	}
	if (!op1->cross() && !op2->cross()) {
		return getAlwaysFalse();
	}
	if (op1->isConstant() && op2->isConstant()) {
		return getAlwaysFalse();
	}
	return getGenericBoolRange();
}

/** boolean And instruction */
range_ptr_t taffo::handleBooleanAnd(const range_ptr_t &op1,
                                    const range_ptr_t &op2)
{
	if (!op1 || !op2) {
		return getGenericBoolRange();
	}
	if (!op1->cross() && !op2->cross()) {
		return getAlwaysTrue();
	}
	if (op1->isConstant() && op2->isConstant()) {
		return getAlwaysFalse();
	}
	return getGenericBoolRange();
}

/** boolean Or instruction */
range_ptr_t taffo::handleBooleanOr(const range_ptr_t &op1,
                                   const range_ptr_t &op2)
{
	if (!op1 || !op2) {
		return getGenericBoolRange();
	}
	if (!op1->cross() || !op2->cross()) {
		return getAlwaysTrue();
	}
	if (op1->isConstant() && op2->isConstant()) {
		return getAlwaysFalse();
	}
	return getGenericBoolRange();
}

/** deep copy of range */
range_ptr_t taffo::copyRange(const range_ptr_t &op)
{
	if (!op) {
		return nullptr;
	}
	range_ptr_t res = make_range(op->min(), op->max());
	return res;
}

/** deep copy of range */
generic_range_ptr_t taffo::copyRange(const generic_range_ptr_t& op)
{
	// TODO implement
	return nullptr;
}

/** create a generic boolean range */
range_ptr_t taffo::getGenericBoolRange()
{
	range_ptr_t res = make_range(static_cast<num_t>(0), static_cast<num_t>(1));
	return res;
}

/** create a always false boolean range */
range_ptr_t taffo::getAlwaysFalse()
{
	range_ptr_t res = make_range(static_cast<num_t>(0), static_cast<num_t>(0));
	return res;
}

/** create a always false boolean range */
range_ptr_t taffo::getAlwaysTrue()
{
	range_ptr_t res = make_range(static_cast<num_t>(1), static_cast<num_t>(1));
	return res;
}

/** create a union between ranges */
range_ptr_t taffo::getUnionRange(const range_ptr_t &op1, const range_ptr_t &op2)
{
	if (!op1) {
		return copyRange(op2);
	}
	if (!op2) {
		return copyRange(op1);
	}
	const num_t min = std::min({op1->min(), op2->min()});
	const num_t max = std::max({op1->max(), op2->max()});
	return make_range(min, max);
}

generic_range_ptr_t taffo::getUnionRange(const generic_range_ptr_t &op1, const generic_range_ptr_t &op2)
{
	if (!op1) {
		return copyRange(op2);
	}
	if (!op2) {
		return copyRange(op1);
	}
	// TODO implement
	return nullptr;
}
