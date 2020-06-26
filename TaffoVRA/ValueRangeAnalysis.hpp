#ifndef TAFFO_VALUE_RANGE_ANALYSIS_HPP
#define TAFFO_VALUE_RANGE_ANALYSIS_HPP

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/CommandLine.h"
#include "CodeInterpreter.hpp"

namespace taffo {

llvm::cl::opt<bool> PropagateAll("propagate-all",
				 llvm::cl::desc("Propagate ranges for all functions, "
						"not only those marked as starting point."),
				 llvm::cl::init(false));
llvm::cl::opt<bool> Unroll("unroll",
                           llvm::cl::desc("Default loop unroll count. "
                                          "Setting this to 0 disables loop unrolling. "
                                          "(Default: 1)"),
                           llvm::cl::value_desc("count"),
                           llvm::cl::init(1U));

struct ValueRangeAnalysis : public llvm::ModulePass {
  static char ID;

public:
  ValueRangeAnalysis(): ModulePass(ID) { }

  bool runOnModule(llvm::Module &M) override;

  void getAnalysisUsage(llvm::AnalysisUsage &) const override;

private:
  void processModule(CodeInterpreter &CodeInt, llvm::Module &M);
};

}

#endif
