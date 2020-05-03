#include "RangeOperations.hpp"
#include "RangeOperationsCallWhitelist.hpp"

#include <assert.h>
#include <limits>
#include <map>
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/APSInt.h"

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
		case llvm::Instruction::Shl:
			return handleShl(op1,op2);
		case llvm::Instruction::LShr: // TODO implement
		case llvm::Instruction::AShr:
			return handleAShr(op1,op2);
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

/** Cast instructions */
generic_range_ptr_t taffo::handleCastInstruction(const generic_range_ptr_t &op,
						 const unsigned OpCode,
						 const llvm::Type *dest)
{
  const range_ptr_t scalar = std::dynamic_ptr_cast_or_null<range_t>(op);
  switch (OpCode) {
		case llvm::Instruction::Trunc:
			return handleTrunc(scalar, dest);
			break;
		case llvm::Instruction::ZExt:
		case llvm::Instruction::SExt:
			return copyRange(scalar);
			break;
		case llvm::Instruction::FPToUI:
			return handleCastToUI(scalar);
			break;
		case llvm::Instruction::FPToSI:
			return handleCastToSI(scalar);
			break;
		case llvm::Instruction::UIToFP:
		case llvm::Instruction::SIToFP:
			return copyRange(scalar);
			break;
		case llvm::Instruction::FPTrunc:
		  	return handleFPTrunc(scalar, dest);
		case llvm::Instruction::FPExt:
			return copyRange(scalar);
			break;
		case llvm::Instruction::PtrToInt:
		case llvm::Instruction::IntToPtr:
			return handleCastToSI(scalar);
			break;
		case llvm::Instruction::BitCast: // TODO check
			return copyRange(scalar);
			break;
		case llvm::Instruction::AddrSpaceCast:
			return copyRange(scalar);
			break;
		default:
			assert(false); // unsupported operation
			break;
	}
	return nullptr;
}

/** Return true if this function call can be handled by taffo::handleMathCallInstruction */
bool taffo::isMathCallInstruction(const std::string &function)
{
  return functionWhiteList.count(function);
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
	range_ptr_t lhs = std::dynamic_ptr_cast_or_null<range_t>(ops.front());
	range_ptr_t rhs = std::dynamic_ptr_cast_or_null<range_t>(ops.back());
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
          default:
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
	if (op1 == op2) {
		// This is a square.
		num_t a = op1->min() * op1->min();
		num_t b = op1->max() * op1->max();
		num_t r1 = (op1->min() <= 0.0 && op1->max() >= 0) ? 0.0 : std::min(a, b);
		num_t r2 = std::max(a, b);
		return make_range(r1, r2);
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
	// Avoid division by 0
	num_t op2_min = (op2->min() == 0.0 && op2->max() > 0.0)
	  ? 1.0 // std::nextafter(static_cast<num_t>(0.0), static_cast<num_t>(op2->max()))
	  : op2->min();
	num_t op2_max = (op2->max() == 0.0 && op2->min() < 0.0)
	  ? -1.0 // std::nextafter(static_cast<num_t>(0.0), static_cast<num_t>(op2->min()))
	  : op2->max();
	num_t a = op1->min() / op2_min;
	num_t b = op1->max() / op2_max;
	num_t c = op1->min() / op2_max;
	num_t d = op1->max() / op2_min;
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

range_ptr_t taffo::handleShl(const range_ptr_t &op1, const range_ptr_t &op2)
{
	// FIXME: it only works if no overflow occurs.
	if (!op1 || !op2) {
		return nullptr;
	}
	const unsigned sh_min = static_cast<unsigned>(op1->min());
	const unsigned sh_max = static_cast<unsigned>(op1->max());
	const long op_min = static_cast<long>(op2->min());
	const long op_max = static_cast<long>(op2->max());
	return make_range(static_cast<num_t>(op_min << ((op_min < 0) ? sh_max : sh_min)),
			  static_cast<num_t>(op_max << ((op_max < 0) ? sh_min : sh_max)));
}

range_ptr_t taffo::handleAShr(const range_ptr_t &op1, const range_ptr_t &op2)
{
	if (!op1 || !op2) {
		return nullptr;
	}
	const unsigned sh_min = static_cast<unsigned>(op1->min());
	const unsigned sh_max = static_cast<unsigned>(op1->max());
	const long op_min = static_cast<long>(op2->min());
	const long op_max = static_cast<long>(op2->max());
	return make_range(static_cast<num_t>(op_min >> ((op_min > 0) ? sh_max : sh_min)),
			  static_cast<num_t>(op_max >> ((op_max > 0) ? sh_min : sh_max)));
}

/** Trunc */
range_ptr_t taffo::handleTrunc(const range_ptr_t &op,
			       const llvm::Type *dest)
{
	using namespace llvm;
	if (!op)
		return nullptr;
	const IntegerType *itype = cast<IntegerType>(dest);

	APSInt imin(64U, true), imax(64U, true);
	bool isExact;
	APFloat(op->min()).convertToInteger(imin,
					    llvm::APFloatBase::roundingMode::rmTowardNegative,
					    &isExact);
	APFloat(op->max()).convertToInteger(imax,
					    llvm::APFloatBase::roundingMode::rmTowardPositive,
					    &isExact);
	APSInt new_imin(imin.trunc(itype->getBitWidth()));
	APSInt new_imax(imax.trunc(itype->getBitWidth()));

	return make_range(new_imin.getExtValue(), new_imax.getExtValue());
}

/** CastToUInteger */
range_ptr_t taffo::handleCastToUI(const range_ptr_t &op)
{
	if (!op) {
		return nullptr;
	}
	const num_t r1 = static_cast<num_t>(static_cast<unsigned long>(op->min()));
	const num_t r2 = static_cast<num_t>(static_cast<unsigned long>(op->max()));
	return make_range(r1,r2);
}

/** CastToUInteger */
range_ptr_t taffo::handleCastToSI(const range_ptr_t &op)
{
	if (!op) {
		return nullptr;
	}
	const num_t r1 = static_cast<num_t>(static_cast<long>(op->min()));
	const num_t r2 = static_cast<num_t>(static_cast<long>(op->max()));
	return make_range(r1,r2);
}

/** FPTrunc */
range_ptr_t taffo::handleFPTrunc(const range_ptr_t &gop,
				 const llvm::Type *dest)
{
	if (!gop) {
		return nullptr;
	}
	assert(dest && dest->isFloatingPointTy()
	       && "Non-floating-point destination Type.");

	llvm::APFloat apmin(gop->min());
	llvm::APFloat apmax(gop->max());
	// Convert with most conservative rounding mode
	bool losesInfo;
	apmin.convert(dest->getFltSemantics(),
		      llvm::APFloatBase::roundingMode::rmTowardNegative,
		      &losesInfo);
	apmax.convert(dest->getFltSemantics(),
		      llvm::APFloatBase::roundingMode::rmTowardPositive,
		      &losesInfo);

	// Convert back to double
	apmin.convert(llvm::APFloat::IEEEdouble(),
		      llvm::APFloatBase::roundingMode::rmTowardNegative,
		      &losesInfo);
	apmax.convert(llvm::APFloat::IEEEdouble(),
		      llvm::APFloatBase::roundingMode::rmTowardPositive,
		      &losesInfo);
	return make_range(apmin.convertToDouble(), apmax.convertToDouble());
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
generic_range_ptr_t taffo::copyRange(const generic_range_ptr_t &op)
{
	if (!op) {
		return nullptr;
	}
	return op->clone();
}

range_ptr_t taffo::copyRange(const range_ptr_t &op)
{
	if (!op) {
		return nullptr;
	}
	return std::static_ptr_cast<range_t>(op->clone());
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

generic_range_ptr_t taffo::getUnionRange(const generic_range_ptr_t &op1,
					 const generic_range_ptr_t &op2)
{
	if (!op1) return copyRange(op2);
	if (!op2) return copyRange(op1);

	if (const range_ptr_t sop1 = std::dynamic_ptr_cast_or_null<range_t>(op1)) {
		const range_ptr_t sop2 = std::dynamic_ptr_cast_or_null<range_t>(op2);
		return getUnionRange(sop1, sop2);
	}

	const range_s_ptr_t op1_s = std::static_ptr_cast<range_s_t>(op1);
	const range_s_ptr_t op2_s = std::static_ptr_cast<range_s_t>(op2);
	unsigned num_fields = std::max(op1_s->getNumRanges(), op2_s->getNumRanges());
	std::vector<generic_range_ptr_t> new_fields;
	new_fields.reserve(num_fields);
	for (unsigned i = 0; i < num_fields; ++i) {
		new_fields.push_back(getUnionRange(op1_s->getRangeAt(i), op2_s->getRangeAt(i)));
	}
	return make_s_range(new_fields);
}

generic_range_ptr_t taffo::fillRangeHoles(const generic_range_ptr_t &src,
					  const generic_range_ptr_t &dst)
{
	if (!src) return copyRange(dst);
	if (!dst || std::isa_ptr<range_t>(src)) {
		return copyRange(src);
	}
	const range_s_ptr_t src_s = std::static_ptr_cast<range_s_t>(src);
	const range_s_ptr_t dst_s = std::static_ptr_cast<range_s_t>(dst);
	std::vector<generic_range_ptr_t> new_fields;
	new_fields.reserve(src_s->getNumRanges());
	for (unsigned i = 0; i < src_s->getNumRanges(); ++i) {
		if (i < dst_s->getNumRanges())
			new_fields.push_back(fillRangeHoles(src_s->getRangeAt(i),
							    dst_s->getRangeAt(i)));
	}
	return make_s_range(new_fields);
}
