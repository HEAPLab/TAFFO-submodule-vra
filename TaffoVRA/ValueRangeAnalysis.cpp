#include "ValueRangeAnalysis.hpp"
#include "InputInfo.h"
#include "Metadata.h"

#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/OptimizationRemarkEmitter.h"
#include "llvm/Analysis/MemorySSA.h"

using namespace llvm;
using namespace taffo;
using namespace mdutils;


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

//-----------------------------------------------------------------------------
// PREPROCESSING
//-----------------------------------------------------------------------------
void ValueRangeAnalysis::harvestMetadata(const Module &M)
{
	MetadataManager &MDManager = MetadataManager::getMetadataManager();

	// const auto harvest = [&]() {
	// 	InputInfo *II = MDManager.retrieveInputInfo(v);
	// 	if (II != nullptr && II->IType != nullptr) {
	// 		user_input[&v] = Range<double>(II->IType->getMinValueBound(), II->IType->getMaxValueBound());
	// 	}
	// 	return;
	// };

	// TODO find a better ID than pointer to llvm::Value. Value name?

	for (const auto &v : M.globals()) {
		// retrieve info about global var v, if any
		InputInfo *II = MDManager.retrieveInputInfo(v);
		if (II != nullptr && II->IType != nullptr) {
			const llvm::Value* v_ptr = &v;
			user_input[v_ptr] = Range<double>(II->IType->getMinValueBound(), II->IType->getMaxValueBound());
		}
	}

	// TODO check for function declarations M.ifuncs()

	for (const auto &f : M.functions()) {
		// retrieve info about function parameters
		SmallVector<mdutils::InputInfo*, 5> argsII;
		MDManager.retrieveArgumentInputInfo(f, argsII);
		auto arg = f.arg_begin();
		for (auto itII = argsII.begin(); itII != argsII.end(); itII++) {
			if (*itII != nullptr && (*itII)->IType != nullptr) {
				if (FPType *fpInfo  = dyn_cast<FPType>((*itII)->IType)) {
					parseMetaData(variables, fpInfo, arg);
				}
			}
			arg++;
		}

		// retrieve info about instructions, for each basic block bb
		for (const auto &bb : f.getBasicBlockList()) {
			for (const auto &i : bb.getInstList()) {
				// fetch info about Instruction i, if any
				InputInfo *II = MDManager.retrieveInputInfo(i);
				if (II != nullptr && II->IType != nullptr) {
					const llvm::Value* i_ptr = &i;
					user_input[i_ptr] = Range<double>(II->IType->getMinValueBound(),
					                                  II->IType->getMaxValueBound());
				}
			}
		}

		// TODO fetch info about loops for each function f
	}

	return;
}

//-----------------------------------------------------------------------------
// ACTUAL PROCESSING
//-----------------------------------------------------------------------------
void ValueRangeAnalysis::processModule(Module &M)
{
	// TODO try to implement symbolic execution of loops

	// iterate over functions till something is changed or up to MAX_IT iterations
	const size_t MAX_IT = 3;
	size_t count_iterations = 0;
	bool changed = false;
	do {
		for (const auto &f : M.functions()) {
			for (const auto &bb : f.getBasicBlockList()) {
				for (const auto &i : bb.getInstList()) {
					const unsigned opCode = i.getOpcode();
					if (Instruction::isTerminator(opCode))
					{
						// TODO handle special case
					} else if (Instruction::isCast(opCode)) {
						// TODO implement

					} else if (Instruction::isBinaryOp(opCode)) {
						const llvm::Value* op1 = i.getOperand(0);
						const llvm::Value* op2 = i.getOperand(1);

						// TODO get both operands' ranges and update them

					} else if (Instruction::isUnaryOp(opCode)) {
						const llvm::Value* op1 = i.getOperand(0);
						// TODO get operator range update it

					} else {
						// TODO here be dragons
					}
				}
			}
		}

		count_iterations++;
	} while(changed && count_iterations < MAX_IT);

	return;
}

//-----------------------------------------------------------------------------
// FINALIZATION
//-----------------------------------------------------------------------------
void ValueRangeAnalysis::saveResults()
{
	return;
}
