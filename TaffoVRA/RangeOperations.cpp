#include "RangeOperations.hpp"
#include "RangeOperationsCallWhitelist.hpp"

#include <assert.h>
#include <limits>
#include <map>

using namespace taffo;

//-----------------------------------------------------------------------------
// Wrappers
//-----------------------------------------------------------------------------

/** Handle binary instructions */
range_ptr_t handleBinaryInstruction(const range_ptr_t &op1,
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

/** Memory instructions */
range_ptr_t handleMemoryInstruction(const range_ptr_t &op,
                                     const unsigned OpCode)
{
	switch (OpCode) {
		case llvm::Instruction::Alloca:
		case llvm::Instruction::Load:
		case llvm::Instruction::Store:
		case llvm::Instruction::GetElementPtr:
		case llvm::Instruction::Fence:
		case llvm::Instruction::AtomicCmpXchg:
		case llvm::Instruction::AtomicRMW:
			break; // TODO implement
		default:
			assert(false); // unsupported operation
			break;
	}
	return nullptr;
}

#if LLVM_VERSION > 7
range_ptr_t handleUnaryInstruction(const range_ptr_t &op,
                                    const unsigned OpCode)
{
	switch (OpCode) {
		case llvm::Instruction::FNeg:
			// TODO implement
			break;
		default:
			assert(false); // unsupported operation
			break;
	}
	return nullptr;
}
#endif

/** Cast instructions */
range_ptr_t handleCastInstruction(const range_ptr_t &op,
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
		case llvm::Instruction::FPExt: // TODO implement
			break;
		case llvm::Instruction::PtrToInt:
		case llvm::Instruction::IntToPtr:
			return handleCastToSI(op);
			break;
		case llvm::Instruction::BitCast: // TODO implement
		case llvm::Instruction::AddrSpaceCast: // TODO implement
			break;
		default:
			assert(false); // unsupported operation
			break;
	}
	return nullptr;
}

using map_value_t = decltype(&handleCallToCeil);
const std::map<const std::string, map_value_t> functionWhiteList =
{
	{"ceil",  &handleCallToCeil},
	{"floor", &handleCallToFloor},
	{"fabs",  &handleCallToFabs},
	{"log",   &handleCallToLog},
	{"log10", &handleCallToLog10},
	{"log2f", &handleCallToLog2f},
	{"sqrt",  &handleCallToSqrt},
	{"exp",   &handleCallToExp},
	{"sin",   &handleCallToSin},
	{"cos",   &handleCallToCos},
	{"acos",  &handleCallToAcos},
	{"tanh",  &handleCallToTanh},
};


/** Handle call to known math functions. Return nullptr if unknown */
range_ptr_t handleMathCallInstruction(const std::list<range_ptr_t>& ops,
                                      const std::string &function)
{
	const auto it = functionWhiteList.find(function);
	if (it != functionWhiteList.end()) {
		return it->second(ops);
	}
	return nullptr;
}

/** Other instructions */
range_ptr_t handleOtherInstructions(const std::list<range_ptr_t > &op,
                                     const unsigned OpCode)
{
	switch (OpCode) {
		case llvm::Instruction::ICmp: // TODO implement
		case llvm::Instruction::FCmp: // TODO implement
		case llvm::Instruction::PHI: // TODO implement
		case llvm::Instruction::Call: // TODO implement
		case llvm::Instruction::Select: // TODO implement
		case llvm::Instruction::UserOp1: // TODO implement
		case llvm::Instruction::UserOp2: // TODO implement
		case llvm::Instruction::VAArg: // TODO implement
		case llvm::Instruction::ExtractElement: // TODO implement
		case llvm::Instruction::InsertElement: // TODO implement
		case llvm::Instruction::ShuffleVector: // TODO implement
		case llvm::Instruction::ExtractValue: // TODO implement
		case llvm::Instruction::InsertValue: // TODO implement
		case llvm::Instruction::LandingPad: // TODO implement
			break;
		default:
			assert(false); // unsupported operation
			break;
	}
	return nullptr;
}


//-----------------------------------------------------------------------------
// Arithmetic
//-----------------------------------------------------------------------------

/** operator+ */
range_ptr_t handleAdd(const range_ptr_t &op1, const range_ptr_t &op2)
{
	num_t a = op1->min() + op2->min();
	num_t b = op1->max() + op2->max();
	return make_range(a,b);
}

/** operator- */
range_ptr_t handleSub(const range_ptr_t &op1, const range_ptr_t &op2)
{
	num_t a = op1->min() - op2->max();
	num_t b = op1->max() - op2->min();
	return make_range(a,b);
}

/** operator* */
range_ptr_t handleMul(const range_ptr_t &op1, const range_ptr_t &op2)
{
	num_t a = op1->min() * op2->min();
	num_t b = op1->max() * op2->max();
	num_t c = op1->min() * op2->max();
	num_t d = op1->max() * op2->min();
	const num_t r1 = std::min({a,b,c,d});
	const num_t r2 = std::max({a,b,c,d});
	return make_range(r1,r2);
}

/** operator/ */
range_ptr_t handleDiv(const range_ptr_t &op1, const range_ptr_t &op2)
{
	num_t a = op1->min() / op2->min();
	num_t b = op1->max() / op2->max();
	num_t c = op1->min() / op2->max();
	num_t d = op1->max() / op2->min();
	const num_t r1 = std::min({a,b,c,d});
	const num_t r2 = std::max({a,b,c,d});
	return make_range(r1,r2);
}

/** operator% */
range_ptr_t handleRem(const range_ptr_t &op1, const range_ptr_t &op2)
{
	const bool alwaysNeg = op1->max() <= 0;
	const bool alwaysPos = op1->min() >= 0;
	const num_t bound = fabs(op2->max()) - std::numeric_limits<num_t>::epsilon();
	const num_t r1 = (alwaysNeg) ? - bound : (alwaysPos) ? 0 : -bound;
	const num_t r2 = (alwaysNeg) ? 0 : (alwaysPos) ? bound : bound;
	return make_range(r1,r2);
}

/** CastToUInteger */
range_ptr_t handleCastToUI(const range_ptr_t &op)
{
	const num_t r1 = static_cast<num_t>(static_cast<unsigned long>(op->min()));
	const num_t r2 = static_cast<num_t>(static_cast<unsigned long>(op->max()));
	return make_range(r1,r2);
}

/** CastToUInteger */
range_ptr_t handleCastToSI(const range_ptr_t &op)
{
	const num_t r1 = static_cast<num_t>(static_cast<long>(op->min()));
	const num_t r2 = static_cast<num_t>(static_cast<long>(op->max()));
	return make_range(r1,r2);
}

/** boolean Xor instruction */
range_ptr_t handleBooleanXor(const range_ptr_t &op1, const range_ptr_t &op2)
{
	if (!op1->cross() && !op2->cross()) {
		return getAlwaysFalse();
	}
	if (op1->isConstant() && op2->isConstant()) {
		return getAlwaysFalse();
	}
	return getGenericBoolRange();
}

/** boolean And instruction */
range_ptr_t handleBooleanAnd(const range_ptr_t &op1, const range_ptr_t &op2)
{
	if (!op1->cross() && !op2->cross()) {
		return getAlwaysTrue();
	}
	if (op1->isConstant() && op2->isConstant()) {
		return getAlwaysFalse();
	}
	return getGenericBoolRange();
}

/** boolean Or instruction */
range_ptr_t handleBooleanOr(const range_ptr_t &op1, const range_ptr_t &op2)
{
	if (!op1->cross() || !op2->cross()) {
		return getAlwaysTrue();
	}
	if (op1->isConstant() && op2->isConstant()) {
		return getAlwaysFalse();
	}
	return getGenericBoolRange();
}

/** deep copy of range */
range_ptr_t copyRange(const range_ptr_t &op)
{
	range_ptr_t res = make_range(op->min(), op->max());
	return res;
}

/** create a generic boolean range */
range_ptr_t getGenericBoolRange()
{
	range_ptr_t res = make_range(static_cast<num_t>(0), static_cast<num_t>(1));
	return res;
}

/** create a always false boolean range */
range_ptr_t getAlwaysFalse()
{
	range_ptr_t res = make_range(static_cast<num_t>(0), static_cast<num_t>(0));
	return res;
}

/** create a always false boolean range */
range_ptr_t getAlwaysTrue()
{
	range_ptr_t res = make_range(static_cast<num_t>(1), static_cast<num_t>(1));
	return res;
}
