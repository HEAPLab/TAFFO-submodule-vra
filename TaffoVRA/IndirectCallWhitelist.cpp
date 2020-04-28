#include "IndirectCallWhitelist.hpp"

using namespace taffo;

std::string taffo::allocatedTask = std::string();

/** Patch the __kmpc_fork_call for parallel and for regions in OpenMP **/
void handleCallToKmpcFork(std::string& callee, llvm::User::const_op_iterator& arg_it, std::list<taffo::range_node_ptr_t>& arg_ranges) {
    // Extract the function from the third argument
    auto micro_task = llvm::dyn_cast<llvm::ConstantExpr>(arg_it + 2)->getOperand(0);
    callee = llvm::dyn_cast<llvm::Function>(micro_task)->getName();

    // Add empty ranges to account for internal OpenMP parameters
    arg_ranges.push_back(nullptr);
    arg_ranges.push_back(nullptr);

    // Skip already analyzed arguments
    arg_it += 3;
}

/** Patch the __kmpc_task_call replacing it with the associated task function **/
void handleCallToKmpcOmpTask(std::string& callee, llvm::User::const_op_iterator& arg_it, std::list<taffo::range_node_ptr_t>& arg_ranges) {
    callee = allocatedTask;

    // Add empty range for the first i32 argument of the task_entry function
    arg_ranges.push_back(nullptr);

    // Ignore internal OpenMP LLVM implementation arguments
    arg_it += 2;
}

/** Save the name of the function that will be executed as a task **/
void handleCallToKmpcOmpTaskAlloc(std::string& callee, llvm::User::const_op_iterator& arg_it, std::list<taffo::range_node_ptr_t>& arg_ranges) {
    // Extract the function from the kmp_routine_entry_t argument
    auto routineEntry = llvm::dyn_cast<llvm::ConstantExpr>(arg_it+5)->getOperand(0);
    allocatedTask = routineEntry->getName();
}

const std::map<const std::string, handler_function> taffo::indirectCallFunctions = {
        {"__kmpc_fork_call", &handleCallToKmpcFork} ,
        {"__kmpc_omp_task_alloc", &handleCallToKmpcOmpTaskAlloc},
        {"__kmpc_omp_task", &handleCallToKmpcOmpTask}
};

void taffo::handleIndirectCall(std::string& callee, llvm::User::const_op_iterator& arg_it, std::list<taffo::range_node_ptr_t>& arg_ranges) {
    auto it = indirectCallFunctions.find(callee);
    if (it != indirectCallFunctions.end()) {
        it->second(callee, arg_it, arg_ranges);
    }
}

