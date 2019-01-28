#include "RangeOperations.hpp"
#include <assert.h>
#include <limits>

using namespace taffo;

/** Handle binary instructions */
template<typename num_t>
Range<num_t> handleBinaryInstruction(const Range<num_t> op1,
                                     const Range<num_t> op2,
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
template<typename num_t>
Range<num_t> handleMemoryInstruction(const Range<num_t> op,
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

template<typename num_t>
Range<num_t> handleUnaryInstruction(const Range<num_t> op,
                                    const unsigned OpCode)
{
	switch (OpCode) {
		case llvm::Instruction::Fneg:
			// TODO implement
			break;
		default:
			assert(false); // unsupported operation
			break;
	}
	return nullptr;
}

/** Cast instructions */
template<typename num_t>
Range<num_t> handleCastInstruction(const Range<num_t> op,
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
		case llvm::Instruction::Bitcast: // TODO implement
		case llvm::Instruction::AddrSpaceCast: // TODO implement
			break;
		default:
			assert(false); // unsupported operation
			break;
	}
}

/** Other instructions */
template<typename num_t>
Range<num_t> handleOtherInstructions(const std::vector<Range<num_t> > &op,
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
		case llvm::Instruction::InserValue: // TODO implement
		case llvm::Instruction::LandingPad: // TODO implement
			break;
		default:
			assert(false); // unsupported operation
			break;
	}
	return nullptr;
}



/** operator+ */
template<typename num_t>
Range<num_t> handleAdd(const Range<num_t> op1, const Range<num_t> op2)
{
	num_t a = op1.min() + op2.min();
	num_t b = op1.max() + op2.max();
	return Range<num_t>(a,b);
}

/** operator- */
template<typename num_t>
Range<num_t> handleSub(const Range<num_t> op1, const Range<num_t> op2)
{
	num_t a = op1.min() - op2.max();
	num_t b = op1.max() - op2.min();
	return Range<num_t>(a,b);
}

/** operator* */
template<typename num_t>
Range<num_t> handleMul(const Range<num_t> op1, const Range<num_t> op2)
{
	num_t a = op1.min() * op2.min();
	num_t b = op1.max() * op2.max();
	num_t c = op1.min() * op2.max();
	num_t d = op1.max() * op2.min();
	const num_t r1 = std::min({a,b,c,d});
	const num_t r2 = std::max({a,b,c,d});
	return Range<num_t>(r1,r2);
}

/** operator/ */
template<typename num_t>
Range<num_t> handleDiv(const Range<num_t> op1, const Range<num_t> op2)
{
	num_t a = op1.min() / op2.min();
	num_t b = op1.max() / op2.max();
	num_t c = op1.min() / op2.max();
	num_t d = op1.max() / op2.min();
	const num_t r1 = std::min({a,b,c,d});
	const num_t r2 = std::max({a,b,c,d});
	return Range<num_t>(r1,r2);
}

/** operator% */
template<typename num_t>
Range<num_t> handleRem(const Range<num_t> op1, const Range<num_t> op2)
{
	const bool alwaysNeg = op1.max() <= 0;
	const bool alwaysPos = op1.min() >= 0;
	const num_t bound = fabs(op2.max()) - std::numeric_limits<num_t>::epsilon();
	const num_t r1 = (alwaysNeg) ? - bound : (alwaysPos) ? 0 : -bound;
	const num_t r2 = (alwaysNeg) ? 0 : (alwaysPos) ? bound : bound;
	return Range<num_t>(r1,r2);
}

/** CastToUInteger */
template<typename num_t>
Range<num_t> handleCastToUI(const Range<num_t> op)
{
	const num_t r1 = static_cast<num_t>(static_cast<unsigned long>(op.min));
	const num_t r2 = static_cast<num_t>(static_cast<unsigned long>(op.max));
	return Range<num_t>(r1,r2);
}

/** CastToUInteger */
template<typename num_t>
Range<num_t> handleCastToSI(const Range<num_t> op)
{
	const num_t r1 = static_cast<num_t>(static_cast<long>(op.min));
	const num_t r2 = static_cast<num_t>(static_cast<long>(op.max));
	return Range<num_t>(r1,r2);
}

/** deep copy of range */
template<typename num_t>
Range<num_t> copyRange(const Range<num_t> op)
{
	return op;
}
