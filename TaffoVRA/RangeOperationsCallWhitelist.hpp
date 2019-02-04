#ifndef TAFFO_RANGE_OPERATIONS_CALL_WHITELIST_HPP
#define TAFFO_RANGE_OPERATIONS_CALL_WHITELIST_HPP

#include "Range.hpp"

#include <list>

namespace taffo {

range_ptr_t handleCallToCeil(const std::list<range_ptr_t>& operands);

range_ptr_t handleCallToFloor(const std::list<range_ptr_t>& operands);

range_ptr_t handleCallToFabs(const std::list<range_ptr_t>& operands);

range_ptr_t handleCallToLog(const std::list<range_ptr_t>& operands);

range_ptr_t handleCallToLog10(const std::list<range_ptr_t>& operands);

range_ptr_t handleCallToLog2f(const std::list<range_ptr_t>& operands);

range_ptr_t handleCallToSqrt(const std::list<range_ptr_t>& operands);

range_ptr_t handleCallToExp(const std::list<range_ptr_t>& operands);

range_ptr_t handleCallToSin(const std::list<range_ptr_t>& operands);

range_ptr_t handleCallToCos(const std::list<range_ptr_t>& operands);

range_ptr_t handleCallToAcos(const std::list<range_ptr_t>& operands);

range_ptr_t handleCallToTanh(const std::list<range_ptr_t>& operands);

};

#endif /* end of include guard: TAFFO_RANGE_OPERATIONS_CALL_WHITELIST_HPP */
