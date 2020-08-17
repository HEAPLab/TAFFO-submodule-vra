#ifndef TAFFO_RANGE_OPERATIONS_CALL_WHITELIST_HPP
#define TAFFO_RANGE_OPERATIONS_CALL_WHITELIST_HPP

#include "Range.hpp"
#include "CodeInterpreter.hpp"

#include <list>
#include <map>

namespace taffo {

#define CMATH_WHITELIST_FUN(BASE_NAME, POINTER) \
        {BASE_NAME,                POINTER}, \
        {BASE_NAME "f",            POINTER}, \
        {BASE_NAME "l",            POINTER}, \
        {"llvm." BASE_NAME ".f32", POINTER}, \
        {"llvm." BASE_NAME ".f64", POINTER}

using map_value_t = range_ptr_t(*)(const std::list<range_ptr_t>&);
using openmp_map_value_t = range_ptr_t(*)(const std::list<range_ptr_t>&, const std::shared_ptr<CodeAnalyzer>&);
extern const std::map<const std::string, map_value_t> functionWhiteList;
extern const std::map<const std::string, openmp_map_value_t > OpenMPfunctionWhiteList;
};

#endif /* end of include guard: TAFFO_RANGE_OPERATIONS_CALL_WHITELIST_HPP */
