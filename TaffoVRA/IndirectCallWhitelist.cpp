#include "IndirectCallWhitelist.hpp"

llvm::Function* handleCallToKmpcFork(llvm::User::op_iterator& arg_it, std::list<taffo::range_node_ptr_t> arg_ranges) {
    // Extract the function from the third argument
    auto micro_task = llvm::dyn_cast<llvm::ConstantExpr>(arg_it + 2)->getOperand(0);
    auto callee = llvm::dyn_cast<llvm::Function>(micro_task);

    // Add empty ranges to account for internal OpenMP parameters
    arg_ranges.push_back(nullptr);
    arg_ranges.push_back(nullptr);

    // Skip already analyzed arguments
    arg_it += 3;

    return callee;
}