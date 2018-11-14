#include "ValueRangeAnalysis.hpp"
#include "Metadata.h"

#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/OptimizationRemarkEmitter.h"
#include "llvm/Analysis/MemorySSA.h"

using namespace llvm;
using namespace taffo;


char ValueRangeAnalysis::ID = 0;

static RegisterPass<ValueRangeAnalysis> X(
	"taffoVRA",
	"TAFFO Framework Value Range Analysis Pass",
	false /* does not affect the CFG */,
	false /* only Analysis */);

bool ValueRangeAnalysis::runOnModule(Module &M)
{
	// DEBUG_WITH_TYPE(DEBUG_VRA, printAnnotatedObj(m));

	harvestMetadata(M);

	processModule(M);

	saveResults();

	return true;
}

void ValueRangeAnalysis::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequiredTransitive<DominatorTreeWrapperPass>();
  AU.addRequiredTransitive<LoopInfoWrapperPass>();
  AU.addRequiredTransitive<AssumptionCacheTracker>();
  AU.addRequiredTransitive<ScalarEvolutionWrapperPass>();
  AU.addRequiredTransitive<OptimizationRemarkEmitterWrapperPass>();
  AU.addRequiredTransitive<MemorySSAWrapperPass>();
  AU.setPreservesAll();
}

void ValueRangeAnalysis::harvestMetadata(const Module &M)
{
	for (const auto& gv : M.globals()) {
		// retrieve shit from global vars
	}
	for (const auto& f : M.ifuncs()) {
		//retrieve shit from function declarations
	}
	for (const auto& f : M.functions()) {
		//retrieve shit from function parameters

		for (const auto& bb : f.getBasicBlockList()) {
			for (const auto& i : bb.getInstList()) {
				// fetch instuction shit
			}
		}
	}

	return;
}

void ValueRangeAnalysis::processModule(Module &M)
{
	return;
}

void ValueRangeAnalysis::saveResults()
{
	return;
}
