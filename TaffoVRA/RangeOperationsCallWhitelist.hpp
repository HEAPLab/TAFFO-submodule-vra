#ifndef TAFFO_RANGE_OPERATIONS_CALL_WHITELIST_HPP
#define TAFFO_RANGE_OPERATIONS_CALL_WHITELIST_HPP

#include "Range.hpp"

#include <map>
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Regex.h"

namespace taffo {

#define CMATH_WHITELIST_FUN(BASE_NAME, POINTER) \
        {BASE_NAME,                POINTER}, \
        {BASE_NAME "f",            POINTER}, \
        {BASE_NAME "l",            POINTER}, \
        {"llvm." BASE_NAME ".f32", POINTER}, \
        {"llvm." BASE_NAME ".f64", POINTER}

#define LIBM_STUB_REGEX_TAIL "(\\.[0-9]+)+"
#define LIBM_STUB_REGEX(BASE_NAME) \
	std::unique_ptr<llvm::Regex>(new llvm::Regex(BASE_NAME LIBM_STUB_REGEX_TAIL))

using map_value_t = range_ptr_t(*)(const std::list<range_ptr_t>&);
extern const std::map<const std::string, map_value_t> functionWhiteList;
extern const std::pair<std::unique_ptr<llvm::Regex>, map_value_t> libmStubMatchList[2];
};

#endif /* end of include guard: TAFFO_RANGE_OPERATIONS_CALL_WHITELIST_HPP */
