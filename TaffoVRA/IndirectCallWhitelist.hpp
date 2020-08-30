#ifndef TAFFO_INDIRECTCALLWHITELIST_HPP
#define TAFFO_INDIRECTCALLWHITELIST_HPP
#include "llvm/IR/Dominators.h"
#include "RangeNode.hpp"
#include <list>

namespace taffo {

    /** Function type for handlers **/
    using handler_function = llvm::Function*(*)(llvm::User::op_iterator& arg_it, std::list<taffo::NodePtrT>& arg_ranges);

    /** Map function names to handlers **/
    extern const std::map<const std::string, handler_function> indirectCallFunctions;

    /** Latest allocated task name **/
    extern llvm::Function* allocatedTask;

    /** Patch the function name and arguments with the handler **/
    llvm::Function* handleIndirectCall(std::string& callee, llvm::User::op_iterator& arg_it, std::list<taffo::NodePtrT>& arg_ranges);

    bool isIndirectFunction(llvm::Function* Function);
}

#endif //TAFFO_INDIRECTCALLWHITELIST_HPP

