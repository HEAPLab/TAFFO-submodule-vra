#include "IndirectCallWhitelist.hpp"

#include "llvm/IR/Intrinsics.h"
#include "TypeUtils.h"
#include "MemSSAUtils.hpp"

using namespace taffo;
using namespace llvm;

std::string taffo::allocatedTask = std::string();

/** Patch the __kmpc_fork_call for parallel and for regions in OpenMP **/
llvm::Function* handleCallToKmpcFork(llvm::User::op_iterator& arg_it, std::list<taffo::NodePtrT>& arg_ranges) {
  // Extract the function from the third argument
  auto micro_task = llvm::dyn_cast<llvm::ConstantExpr>(arg_it + 2)->getOperand(0);
  auto micro_task_function = llvm::dyn_cast<llvm::Function>(micro_task);

  // Add empty ranges to account for internal OpenMP parameters
  arg_ranges.push_back(nullptr);
  arg_ranges.push_back(nullptr);

  // Skip already analyzed arguments
  arg_it += 3;

  return micro_task_function;
}

/** Patch the __kmpc_task_call replacing it with the associated task function **/
llvm::Function* handleCallToKmpcOmpTask(llvm::User::op_iterator& arg_it, std::list<taffo::NodePtrT>& arg_ranges) {
  // TODO change behaviour with the new scheduler
  // Add empty range for the first i32 argument of the task_entry function
    arg_ranges.push_back(nullptr);

    // Ignore internal OpenMP LLVM implementation arguments
    arg_it += 2;

    return nullptr;
}

/** Save the name of the function that will be executed as a task **/
llvm::Function* handleCallToKmpcOmpTaskAlloc(llvm::User::op_iterator& arg_it, std::list<taffo::NodePtrT>& arg_ranges) {
  // TODO change behaviour with the new scheduler
  // Extract the function from the kmp_routine_entry_t argument
  auto routineEntry = llvm::dyn_cast<llvm::ConstantExpr>(arg_it+5)->getOperand(0);
  allocatedTask = routineEntry->getName();

}

const std::map<const std::string, handler_function> taffo::indirectCallFunctions = {
        {"__kmpc_fork_call", &handleCallToKmpcFork} ,
        {"__kmpc_omp_task_alloc", &handleCallToKmpcOmpTaskAlloc},
        {"__kmpc_omp_task", &handleCallToKmpcOmpTask}
};

llvm::Function* taffo::handleIndirectCall(std::string& callee, llvm::User::op_iterator& arg_it, std::list<taffo::NodePtrT>& arg_ranges) {
    auto it = indirectCallFunctions.find(callee);
    if (it != indirectCallFunctions.end()) {
        return it->second(arg_it, arg_ranges);
    }
}

bool taffo::isIndirectFunction(llvm::Function *Function) {
  return indirectCallFunctions.count(Function->getName());
}

