#ifndef TAFFO_INDIRECTCALLWHITELIST_HPP
#define TAFFO_INDIRECTCALLWHITELIST_HPP
#include "llvm/IR/Dominators.h"
#include "RangeNode.hpp"
#include <list>


/** Path the __kmpc_fork_call for parallel and for regions in OpenMP **/
llvm::Function* handleCallToKmpcFork(llvm::User::op_iterator& arg_it, std::list<taffo::range_node_ptr_t> arg_ranges);


#endif //TAFFO_INDIRECTCALLWHITELIST_HPP
