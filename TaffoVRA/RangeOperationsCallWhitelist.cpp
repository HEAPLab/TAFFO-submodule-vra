#include <string>
#include <list>
#include <math.h>
#include <cassert>
#include <limits>
#include "Range.hpp"
#include "RangeOperationsCallWhitelist.hpp"

using namespace taffo;

static range_ptr_t handleCallToCeil(const std::list<range_ptr_t>& operands)
{
	assert(operands.size() == 1 && "too many operands in function ceil");
	range_ptr_t op = operands.front();
	if (!op) {
		return nullptr;
	}
	return make_range(static_cast<num_t>(ceil(static_cast<double>(op->min()))),
	                  static_cast<num_t>(ceil(static_cast<double>(op->max()))));
}

static range_ptr_t handleCallToFloor(const std::list<range_ptr_t>& operands)
{
	assert(operands.size() == 1 && "too many operands in function floor");
	range_ptr_t op = operands.front();
	if (!op) {
		return nullptr;
	}
	return make_range(static_cast<num_t>(floor(static_cast<double>(op->min()))),
	                  static_cast<num_t>(floor(static_cast<double>(op->max()))));
}

static range_ptr_t handleCallToFabs(const std::list<range_ptr_t>& operands)
{
	assert(operands.size() == 1 && "too many operands in function fabs");
	range_ptr_t op = operands.front();
	if (!op) {
		return nullptr;
	}
	num_t min = static_cast<num_t>(fabs(static_cast<double>(op->min())));
	num_t max = static_cast<num_t>(fabs(static_cast<double>(op->max())));
	if (min <= max) {
		return make_range(min, max);
	}
	return make_range(max, min);
}

static range_ptr_t handleCallToLog(const std::list<range_ptr_t>& operands)
{
	assert(operands.size() == 1 && "too many operands in function Log");
	range_ptr_t op = operands.front();
	if (!op) {
		return nullptr;
	}
	assert(op->max() >= 0);
	num_t min = (op->min() < 0) ? std::numeric_limits<num_t>::epsilon() : op->min();
	min = static_cast<num_t>(log(static_cast<double>(min)));
	num_t max = static_cast<num_t>(log(static_cast<double>(op->max())));
	return make_range(min, max);
}

static range_ptr_t handleCallToLog10(const std::list<range_ptr_t>& operands)
{
	assert(operands.size() == 1 && "too many operands in function Log10");
	range_ptr_t op = operands.front();
	if (!op) {
		return nullptr;
	}
	assert(op->max() >= 0);
	num_t min = (op->min() < 0) ? std::numeric_limits<num_t>::epsilon() : op->min();
	min = static_cast<num_t>(log10(static_cast<double>(min)));
	num_t max = static_cast<num_t>(log10(static_cast<double>(op->max())));
	return make_range(min, max);
}

static range_ptr_t handleCallToLog2f(const std::list<range_ptr_t>& operands)
{
	assert(operands.size() == 1 && "too many operands in function Log2f");
	range_ptr_t op = operands.front();
	if (!op) {
		return nullptr;
	}
	assert(op->max() >= 0);
	num_t min = (op->min() < 0) ? std::numeric_limits<num_t>::epsilon() : op->min();
	min = static_cast<num_t>(log2f(static_cast<double>(min)));
	num_t max = static_cast<num_t>(log2f(static_cast<double>(max)));
	return make_range(min, max);
}

static range_ptr_t handleCallToSqrt(const std::list<range_ptr_t>& operands)
{
	assert(operands.size() == 1 && "too many operands in function Sqrt");
	range_ptr_t op = operands.front();
	if (!op) {
		return nullptr;
	}
	assert(op->max() >= 0);
	num_t min = (op->min() < 0) ? 0 : op->min();
	min = static_cast<num_t>(sqrt(static_cast<double>(min)));
	num_t max = static_cast<num_t>(sqrt(static_cast<double>(op->max())));
	if (min <= max) {
		return make_range(min, max);
	}
	return make_range(max, min);
}

static range_ptr_t handleCallToExp(const std::list<range_ptr_t>& operands)
{
	assert(operands.size() == 1 && "too many operands in function Exp");
	range_ptr_t op = operands.front();
	if (!op) {
		return nullptr;
	}
	num_t min = static_cast<num_t>(exp(static_cast<double>(op->min())));
	num_t max = static_cast<num_t>(exp(static_cast<double>(op->max())));
	return make_range(min, max);
}

static range_ptr_t handleCallToSin(const std::list<range_ptr_t>& operands)
{
	// TODO implement
	return nullptr;
}

static range_ptr_t handleCallToCos(const std::list<range_ptr_t>& operands)
{
	// TODO implement
	return nullptr;
}

static range_ptr_t handleCallToAcos(const std::list<range_ptr_t>& operands)
{
	// TODO implement
	return nullptr;
}

static range_ptr_t handleCallToTanh(const std::list<range_ptr_t>& operands)
{
	// TODO implement
	return nullptr;
}

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
