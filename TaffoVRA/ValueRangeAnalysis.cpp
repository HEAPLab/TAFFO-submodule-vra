#include "ValueRangeAnalysis.hpp"
#include "InputInfo.h"
#include "RangeOperations.hpp"
#include "Metadata.h"

#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/OptimizationRemarkEmitter.h"
#include "llvm/Analysis/MemorySSA.h"

#include <set>

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

	saveResults(M);

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
void ValueRangeAnalysis::harvestMetadata(Module &M)
{
	MetadataManager &MDManager = MetadataManager::getMetadataManager();

	for (const auto &v : M.globals()) {
		// retrieve info about global var v, if any
		InputInfo *II = MDManager.retrieveInputInfo(v);
		if (II != nullptr && II->IRange != nullptr) {
			const llvm::Value* v_ptr = &v;
			user_input[v_ptr] = make_range(II->IRange->Min, II->IRange->Max);
		}
	}

	for (llvm::Function &f : M.functions()) {
		// retrieve info about loop iterations
		LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>(f).getLoopInfo();
		for (auto &loop : LI.getLoopsInPreorder()) {
			Optional<unsigned> lic = MetadataManager::retrieveLoopUnrollCount(*loop, &LI);
			if (lic.hasValue()) {
				user_loop_iterations[loop] = lic.getValue();
				for (auto& bb : loop->getBlocks()) {
					const auto it = bb_priority.find(bb);
					if (it != bb_priority.end()) {
						bb_priority[bb] += bb_base_priority * lic.getValue();
					} else {
						bb_priority[bb] = bb_base_priority * lic.getValue();
					}
				}

			}
		}

		// retrieve info about function parameters
		SmallVector<mdutils::InputInfo*, 5> argsII;
		MDManager.retrieveArgumentInputInfo(f, argsII);
		auto arg = f.arg_begin();
		fun_arg_input[&f] = std::vector<range_ptr_t>();
		for (auto itII = argsII.begin(); itII != argsII.end(); itII++) {
			if (*itII != nullptr && (*itII)->IRange != nullptr) {
				fun_arg_input[&f].push_back(make_range((*itII)->IRange->Min,
				                                      (*itII)->IRange->Max));
			} else {
				fun_arg_input[&f].push_back(nullptr);
			}
			arg++;
		}

		// retrieve info about instructions, for each basic block bb
		for (const auto &bb : f.getBasicBlockList()) {
			for (const auto &i : bb.getInstList()) {
				// fetch info about Instruction i, if any
				InputInfo *II = MDManager.retrieveInputInfo(i);
				if (II != nullptr && II->IRange != nullptr) {
					const llvm::Value* i_ptr = &i;
					user_input[i_ptr] = make_range(II->IRange->Min,
					                               II->IRange->Max);
				}
			}
		}

	} // end iteration over Function in Module
	return;
}

//-----------------------------------------------------------------------------
// ACTUAL PROCESSING
//-----------------------------------------------------------------------------
void ValueRangeAnalysis::processModule(Module &M)
{
	// TODO try to implement symbolic execution of loops

	// first create processing queue, then evaluate them
	for (auto f = M.begin(); f != M.end(); ++f) {
		llvm::Function* f_ptr = &*f;
		f_unvisited_set.insert(f_ptr);
	}

	// fetch initial function
	llvm::Function* current_f = *f_unvisited_set.begin();

	while (current_f != nullptr) {
		processFunction(*current_f);
		f_unvisited_set.erase(current_f);

		// update current_f
		current_f = (f_unvisited_set.empty()) ? nullptr : *f_unvisited_set.begin();
	}


	return;
}

void ValueRangeAnalysis::processFunction(llvm::Function& F)
{
	call_stack.push_back(&F);
	// get function entry point: getEntryBlock
	std::set<llvm::BasicBlock*> bb_queue;
	std::set<llvm::BasicBlock*> bb_unvisited_set;
	for (auto &bb : F.getBasicBlockList()) {
		llvm::BasicBlock* bb_ptr = &bb;
		// bb_queue.insert(bb_ptr);
		bb_unvisited_set.insert(bb_ptr);
	}
	llvm::BasicBlock* current_bb = &F.getEntryBlock();

	while(current_bb != nullptr)
	{
		processBasicBlock(*current_bb);

		// update bb_queue by removing current bb and insert its successors
		bb_queue.erase(current_bb);
		bb_unvisited_set.erase(current_bb);

		llvm::BasicBlock* unique_successor = current_bb->getUniqueSuccessor();
		if (unique_successor != nullptr) {
			bb_queue.insert(unique_successor);
		} else {
			auto successors = llvm::successors(current_bb);
			for (auto successor : successors) {
				llvm::BasicBlock* succ = successor;
				if (bb_priority[succ] > 0) {
					bb_queue.insert(succ);
					bb_priority[succ] -= bb_base_priority;
				}
			}
		}

		if (bb_queue.empty()) {
			if (bb_unvisited_set.empty()) {
				bb_queue.insert(nullptr);
			} else {
				llvm::BasicBlock* bb_ptr = *bb_unvisited_set.begin();
				bb_queue.insert(bb_ptr);
			}
		}

		// update current_bb
		current_bb = *bb_queue.begin();
	} // end iteration over bb

	call_stack.pop_back();
	return;
}

void ValueRangeAnalysis::processBasicBlock(llvm::BasicBlock& BB)
{
	for (auto &i : BB.getInstList()) {
		const unsigned opCode = i.getOpcode();
		if (opCode == Instruction::Call)
		{
			// TODO fetch function name
			const std::string calledFunctionName = "";
			llvm::CallInst* call_i = dyn_cast<llvm::CallInst>(&i);
			if (!call_i) {
				// TODO handle error
				assert(false);
			}
			std::list<range_ptr_t> arg_ranges;
			for(auto arg_it = call_i->arg_begin(); arg_it != call_i->arg_end(); ++arg_it)
			{
				const llvm::Value* arg = *arg_it;
				const range_ptr_t arg_info = fetchInfo(arg);
				arg_ranges.push_back(arg_info);
			}

			// first check if it is among the whitelisted functions we can handle
			range_ptr_t res = handleMathCallInstruction(arg_ranges, calledFunctionName);

			// if not a whitelisted then try to fetch it from Module
			if (!res) {
				// TODO check for recursion
				// TODO call processFunction
			}
			saveValueInfo(&i, res);
		}
		else if (Instruction::isTerminator(opCode))
		{
			// TODO handle special case
		}
		else if (Instruction::isCast(opCode))
		{
			const llvm::Value* op = i.getOperand(0);
			const auto info = fetchInfo(op);
			const auto res = handleCastInstruction(info, opCode);
			saveValueInfo(&i, res);
		}
		else if (Instruction::isBinaryOp(opCode))
		{
			const llvm::Value* op1 = i.getOperand(0);
			const llvm::Value* op2 = i.getOperand(1);
			const auto info1 = fetchInfo(op1);
			const auto info2 = fetchInfo(op2);
			const auto res = handleBinaryInstruction(info1, info2, opCode);
			saveValueInfo(&i, res);
		}
#if LLVM_VERSION > 7
		else if (Instruction::isUnaryOp(opCode))
		{
			const llvm::Value* op1 = i.getOperand(0);
			const auto info1 = fetchInfo(op1);
			const auto res = handleBinaryInstruction(info1, info2, opCode);
			saveValueInfo(&i, res);
		}
#endif
		else {
			// TODO here be dragons
		}
	}
	return;
}

//-----------------------------------------------------------------------------
// FINALIZATION
//-----------------------------------------------------------------------------
void ValueRangeAnalysis::saveResults(const llvm::Module &M)
{
	MetadataManager &MDManager = MetadataManager::getMetadataManager();
	for (const auto &v : M.globals()) {
		// retrieve info about global var v, if any
		InputInfo *II = MDManager.retrieveInputInfo(v);
		if (II != nullptr) {
			const llvm::Value* v_ptr = &v;
			const auto range = fetchInfo(v_ptr);
			if (range != nullptr) {
				II->IRange = new Range(range->min(), range->max());
			} else {
				// TODO set default
			}
		}
	} // end globals

	for (const auto &f : M.functions()) {
		// // retrieve info about function parameters
		// SmallVector<mdutils::InputInfo*, 5> argsII;
		// MDManager.retrieveArgumentInputInfo(f, argsII);
		// auto arg = f.arg_begin();
		// for (auto itII = argsII.begin(); itII != argsII.end(); itII++) {
		// 	if (*itII != nullptr && (*itII)->IRange != nullptr) {
		// 		if (FPType *fpInfo  = dyn_cast<FPType>((*itII)->IRange)) {
		// 			parseMetaData(variables, fpInfo, arg);
		// 		}
		// 	}
		// 	arg++;
		// }

		// retrieve info about instructions, for each basic block bb
		for (const auto &bb : f.getBasicBlockList()) {
			for (const auto &i : bb.getInstList()) {
				// fetch info about Instruction i, if any
				InputInfo *II = MDManager.retrieveInputInfo(i);
				if (II != nullptr) {
					const llvm::Value* v_ptr = &i;
					const auto range = fetchInfo(v_ptr);
					if (range != nullptr) {
						II->IRange = new Range(range->min(), range->max());
					} else {
						// TODO set default
					}
				}
			} // end inst list
		} // end bb
	} // end function
	return;
}

//-----------------------------------------------------------------------------
// RETRIEVE INFO
//-----------------------------------------------------------------------------
const range_ptr_t ValueRangeAnalysis::fetchInfo(const llvm::Value* v) const
{
	using iter_t = decltype(user_input)::const_iterator;
	iter_t it = user_input.find(v);
	if (it != user_input.end()) {
		return it->second;
	}
	it = derived_ranges.find(v);
	if (it != derived_ranges.end()) {
		return it->second;
	}
	// no info available
	return nullptr;
}

//-----------------------------------------------------------------------------
// SAVE VALUE INFO
//-----------------------------------------------------------------------------
void ValueRangeAnalysis::saveValueInfo(const llvm::Value* v, const range_ptr_t& info)
{
	// if (const auto it = user_input.find(v) != user_input.end()) {
	// 	// TODO maybe check if more/less accurate
	// }
	// if (const auto it = derived_ranges.find(v) != derived_ranges.end()) {
	// 	// TODO maybe check if more/less accurate
	// }
	derived_ranges[v] = info;
	return;
}
