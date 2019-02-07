#ifndef TAFFO_RANGE_OPERATIONS_CALL_WHITELIST_HPP
#define TAFFO_RANGE_OPERATIONS_CALL_WHITELIST_HPP

#include "Range.hpp"

#include <list>
#include <map>

namespace taffo {

using map_value_t = range_ptr_t(*)(const std::list<range_ptr_t>&);
const std::map<const std::string, map_value_t> functionWhiteList;
};

#endif /* end of include guard: TAFFO_RANGE_OPERATIONS_CALL_WHITELIST_HPP */
