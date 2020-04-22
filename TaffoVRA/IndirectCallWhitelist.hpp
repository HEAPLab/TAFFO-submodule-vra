#ifndef TAFFO_INDIRECTCALLWHITELIST_HPP
#define TAFFO_INDIRECTCALLWHITELIST_HPP
#include "llvm/IR/Dominators.h"
#include "RangeNode.hpp"
#include <list>

namespace taffo {

    /** Function type for handlers **/
    using handler_function = void(*)(std::string& callee, llvm::User::const_op_iterator& arg_it, std::list<taffo::range_node_ptr_t>& arg_ranges);

    /** Map function names to handlers **/
    extern const std::map<const std::string, handler_function> indirectCallFunctions;

    /** Patch the function name and arguments with the handler **/
    void handleIndirectCall(std::string& callee, llvm::User::const_op_iterator& arg_it, std::list<taffo::range_node_ptr_t>& arg_ranges);
}

#endif //TAFFO_INDIRECTCALLWHITELIST_HPP

