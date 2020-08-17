#include "ValueRangeAnalysis.hpp"
#include "IndirectCallWhitelist.hpp"

#include "llvm/Support/Debug.h"
#include "llvm/Analysis/MemorySSA.h"
#include "VRAGlobalStore.hpp"

using namespace llvm;
using namespace taffo;
using namespace mdutils;

char ValueRangeAnalysis::ID = 0;

static RegisterPass<ValueRangeAnalysis> X(
	"taffoVRA",
	"TAFFO Framework Value Range Analysis Pass",
	false /* does not affect the CFG */,
	false /* only Analysis */);

bool
ValueRangeAnalysis::runOnModule(Module &M) {
  std::shared_ptr<VRAGlobalStore> GlobalStore = std::make_shared<VRAGlobalStore>();
  GlobalStore->harvestMetadata(M);

  CodeInterpreter CodeInt(*this, GlobalStore, Unroll);
  processModule(CodeInt, M);

  GlobalStore->saveResults(M);

  return true;
}

void
ValueRangeAnalysis::getAnalysisUsage(AnalysisUsage &AU) const {
  CodeInterpreter::getAnalysisUsage(AU);
  AU.addRequiredTransitive<MemorySSAWrapperPass>();
  AU.setPreservesAll();
}

void
ValueRangeAnalysis::processModule(CodeInterpreter &CodeInt, Module &M) {
  bool FoundVisitableFunction = false;
  for (llvm::Function &F : M) {
    if (!F.empty() && (PropagateAll || MetadataManager::isStartingPoint(F))) {
      CodeInt.interpretFunction(&F);
      FoundVisitableFunction = true;
    //TODO remove old code
    /*
    auto arg_it = call_i->arg_begin();

    // Patch function name and arguments if indirect
    handleIndirectCall(calledFunctionName, arg_it, arg_ranges);

	// fetch ranges of arguments
	for(; arg_it != call_i->arg_end(); ++arg_it)
     */
    }
  }

  if (!FoundVisitableFunction) {
    LLVM_DEBUG(dbgs() << DEBUG_HEAD << " No visitable functions found.\n");
  }
}
