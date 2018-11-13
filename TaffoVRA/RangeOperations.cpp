#include "RangeOperations.hpp"

using namespace taffo;

template<typename num_t>
Range<num_t> handleInstruction(const Range<num_t> op1, const Range<num_t> op2, const unsigned OpCode)
{
	switch (OpCode) {
		case llvm::Instruction::Add:
			return handleAdd(op1,op2);
			break;
		case llvm::Instruction::Mul:
			return handleMul(op1,op2);
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

/*
template<typename num_t, unsigned OpCode>
Range<num_t> handleInstruction(const Range<num_t> op);

template<typename num_t, unsigned OpCode>
Range<num_t> handleInstruction(const std::list<Range<num_t> > op_list);
*/
