#include "RangeOperations.hpp"
#include <assert.h>
#include <limits>

using namespace taffo;

template<typename num_t>
Range<num_t> handleInstruction(const Range<num_t> op1, const Range<num_t> op2, const unsigned OpCode)
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
		default:
			assert(false); // unsupported operation
			break;
	}
	return nullptr;
}

template<typename num_t>
Range<num_t> handleAdd(const Range<num_t> op1, const Range<num_t> op2)
{
	num_t a = op1.min() + op2.min();
	num_t b = op1.max() + op2.max();
	return Range<num_t>(a,b);
}

template<typename num_t>
Range<num_t> handleSub(const Range<num_t> op1, const Range<num_t> op2)
{
	num_t a = op1.min() - op2.max();
	num_t b = op1.max() - op2.min();
	return Range<num_t>(a,b);
}

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

/*
template<typename num_t, unsigned OpCode>
Range<num_t> handleInstruction(const Range<num_t> op);

template<typename num_t, unsigned OpCode>
Range<num_t> handleInstruction(const std::list<Range<num_t> > op_list);
*/
