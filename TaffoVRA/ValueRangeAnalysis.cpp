#include "ValueRangeAnalysis.hpp"
#include "InputInfo.h"
#include "RangeOperations.hpp"
#include "Metadata.h"
#include "MemSSAUtils.hpp"
#include "TopologicalBBSort.hpp"

#include "llvm/IR/Constants.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/OptimizationRemarkEmitter.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/Support/Debug.h"

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
bool isValidRange(Range *rng) {
  return rng != nullptr && !std::isnan(rng->Min) && !std::isnan(rng->Max);
}

void ValueRangeAnalysis::harvestMetadata(Module &M)
{
	MetadataManager &MDManager = MetadataManager::getMetadataManager();

	for (const auto &v : M.globals()) {
		// retrieve info about global var v, if any
		InputInfo *II = MDManager.retrieveInputInfo(v);
		if (II != nullptr && isValidRange(II->IRange.get())) {
			user_input[&v] = make_range(II->IRange->Min, II->IRange->Max);
		} else if (StructInfo *SI = MDManager.retrieveStructInfo(v)) {
			user_input[&v] = harvestStructMD(SI);
			derived_ranges[&v] = new VRA_RangeNode(user_input[&v]);
		}
	}

	for (llvm::Function &f : M.functions()) {
		if (f.empty()) {
			continue;
		}
		const std::string name = f.getName();
		known_functions[name] = &f;

		// retrieve information about recursion count
		unsigned recursion_count = MDManager.retrieveMaxRecursionCount(f);
		fun_rec_count[&f] = recursion_count;

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
				} // end iteration over bb in loop
			} // if no value is available, try to infer it
		}

		// retrieve info about function parameters
		SmallVector<mdutils::MDInfo*, 5> argsII;
		MDManager.retrieveArgumentInputInfo(f, argsII);
		if (!argsII.empty()) {
			fun_arg_input[&f] = std::list<generic_range_ptr_t>();
			for (auto itII = argsII.begin(); itII != argsII.end(); itII++) {
				fun_arg_input[&f].push_back(harvestStructMD(*itII));
			}
		}

		// retrieve info about instructions, for each basic block bb
		for (const auto &bb : f.getBasicBlockList()) {
			for (const auto &i : bb.getInstList()) {
				// fetch info about Instruction i, if any
				InputInfo *II = MDManager.retrieveInputInfo(i);
				if (II != nullptr && isValidRange(II->IRange.get())) {
					const llvm::Value* i_ptr = &i;
					user_input[i_ptr] = make_range(II->IRange->Min, II->IRange->Max);
				}
				else if (StructInfo *SI = MDManager.retrieveStructInfo(i)) {
					const llvm::Value* i_ptr = &i;
					user_input[i_ptr] = harvestStructMD(SI);
				}
			}
		}

	} // end iteration over Function in Module
	return;
}

generic_range_ptr_t ValueRangeAnalysis::harvestStructMD(MDInfo *MD) {
	if (MD == nullptr) {
		return make_range();

	} else if (InputInfo *II = dyn_cast<InputInfo>(MD)) {
		if (isValidRange(II->IRange.get()))
			return make_range(II->IRange->Min, II->IRange->Max);
		else
			return nullptr;

	} else if (StructInfo *SI = dyn_cast<StructInfo>(MD)) {
		std::vector<generic_range_ptr_t> rngs;
		for (auto it = SI->begin(); it != SI->end(); it++) {
			rngs.push_back(harvestStructMD(it->get()));
		}

		return make_s_range(rngs);
	}

	llvm_unreachable("unknown type of MDInfo");
}


//-----------------------------------------------------------------------------
// ACTUAL PROCESSING
//-----------------------------------------------------------------------------
void ValueRangeAnalysis::processModule(Module &M)
{
	// TODO try to implement symbolic execution of loops

	// first create processing queue, then evaluate them
	for (llvm::Function &f : M) {
		if (f.empty()
		    || (!PropagateAll && !MetadataManager::isStartingPoint(f)))
		  continue;
		f_unvisited_set.insert(&f);
	}

	// fetch initial function
	llvm::Function* current_f = *f_unvisited_set.begin();

	while (current_f != nullptr) {
		processFunction(*current_f);
		// f_unvisited_set update hasd been moved in processFunction

		// update current_f
		current_f = (f_unvisited_set.empty()) ? nullptr : *f_unvisited_set.begin();
	}


	return;
}

void ValueRangeAnalysis::processFunction(llvm::Function& F)
{
        DEBUG(dbgs() << "\n" DEBUG_HEAD " Processing function " << F.getName() << "...\n");
	// if already available, no need to execute again
	generic_range_ptr_t tmp = find_ret_val(&F);
	if (tmp != nullptr) {
	        DEBUG(dbgs() << " done already, aborting.\n");
		return;
	}

	// fetch info about actual parameters
	bool has_input_info = false;
	bool has_derived_info = false;
	std::list<generic_range_ptr_t>::iterator input_info_it, input_info_end;
	std::list<generic_range_ptr_t>::iterator derived_info_it, derived_info_end;
	auto arg_list_lookup = fun_arg_input.find(&F);
	if (arg_list_lookup != fun_arg_input.end()) {
        	input_info_it = arg_list_lookup->second.begin();
		input_info_end = arg_list_lookup->second.end();
		if (input_info_it != input_info_end) has_input_info = true;
	}
	arg_list_lookup = fun_arg_derived.find(&F);
	if (arg_list_lookup != fun_arg_derived.end()) {
		derived_info_it = arg_list_lookup->second.begin();
		derived_info_end = arg_list_lookup->second.end();
		if (derived_info_it != derived_info_end) has_derived_info = true;
	}
	DEBUG(dbgs() << DEBUG_HEAD " Loading argument ranges: ");
	for (llvm::Argument* formal_arg = F.arg_begin();
	     formal_arg != F.arg_end();
	     ++formal_arg) {
		generic_range_ptr_t info = nullptr;
		if (has_input_info && *input_info_it != nullptr) {
		 	info = *input_info_it;
		} else if (has_derived_info && *derived_info_it != nullptr) {
			info = *derived_info_it;
		}

		saveValueInfo(formal_arg, info);
		DEBUG(dbgs() << "{ " << *formal_arg << " : " << to_string(info) << " }, ");

		if (has_input_info && ++input_info_it == input_info_end)
			has_input_info = false;
		if (has_derived_info && ++derived_info_it == derived_info_end)
			has_derived_info = false;
	}
	DEBUG(dbgs() << "\n");

	// update stack for the simulation of execution
	call_stack.push_back(&F);

	// get function entry point: getEntryBlock
	TopologicalBBQueue bb_queue;
	std::set<llvm::BasicBlock*> bb_unvisited_set;
	for (auto &bb : F.getBasicBlockList()) {
		llvm::BasicBlock* bb_ptr = &bb;
		// bb_queue.insert(bb_ptr);
		bb_unvisited_set.insert(bb_ptr);
	}
	llvm::BasicBlock* current_bb = &F.getEntryBlock();

	// TODO: we should make a local copy of bb_priority in order to support recursion
	// TODO: use a better algorithm based on (post-)dominator tree
	while(current_bb != nullptr)
	{
		processBasicBlock(*current_bb);

		bb_unvisited_set.erase(current_bb);

		// update bb_queue by removing current bb and insert its successors
		llvm::BasicBlock* unique_successor = current_bb->getUniqueSuccessor();
		if (unique_successor != nullptr) {
			bb_queue.insert(unique_successor);
		} else {
			auto successors = llvm::successors(current_bb);
			for (llvm::BasicBlock* succ : successors) {
			  	auto succ_pri = bb_priority.find(succ);
				if (succ_pri == bb_priority.end()) {
					bb_queue.insert(succ);
					bb_priority[succ] = 0;
				} else if (succ_pri->second > 0) {
					bb_queue.insert(succ);
				        succ_pri->second -= bb_base_priority;
				}
			}
		}

		// update current_bb
		if (bb_queue.empty()) {
			if (bb_unvisited_set.empty()) {
			  	current_bb = nullptr;
			} else {
				llvm::BasicBlock* bb_ptr = *bb_unvisited_set.begin();
				current_bb = bb_ptr;
			}
		} else {
			current_bb = bb_queue.popFirst();
		}
	} // end iteration over bb

	call_stack.pop_back();
	f_unvisited_set.erase(&F);
	// TODO keep a range for possible return value

	DEBUG(dbgs() << "[TAFFO][VRA] Finished processing function " << F.getName() << ".\n\n");
	return;
}

void ValueRangeAnalysis::processBasicBlock(llvm::BasicBlock& BB)
{
	for (auto &i : BB.getInstList()) {
		const unsigned opCode = i.getOpcode();
		if (opCode == Instruction::Call)
		{
			#if LLVM_VERSION < 8
			handleCallInst(&i);
			#else
			handleCallBase(&i);
			#endif
		}
		else if (Instruction::isTerminator(opCode))
		{
			handleTerminators(&i);
		}
		else if (Instruction::isCast(opCode))
		{
			logInstruction(&i);
		        const llvm::Value* op = i.getOperand(0);
			const generic_range_ptr_t info = fetchInfo(op);
			const auto res = handleCastInstruction(info, opCode, i.getType());
			saveValueInfo(&i, res);

			DEBUG(if (!info) logInfo("operand range is null"));
			logRangeln(res);
		}
		else if (Instruction::isBinaryOp(opCode))
		{
		        logInstruction(&i);
			const llvm::Value* op1 = i.getOperand(0);
			const llvm::Value* op2 = i.getOperand(1);
			const auto node1 = getNode(op1);
			const auto node2 = getNode(op2);
			const range_ptr_t info1 =
			  std::dynamic_ptr_cast_or_null<range_t>(fetchInfo(op1));
			const range_ptr_t info2 =
			  std::dynamic_ptr_cast_or_null<range_t>(fetchInfo(op2));
			const auto res = handleBinaryInstruction(info1, info2, opCode);
			saveValueInfo(&i, res);

			DEBUG(if (!info1) logInfo("first range is null"));
			DEBUG(if (!info2) logInfo("second range is null"));
			logRangeln(res);
		}
#if LLVM_VERSION > 7
		else if (Instruction::isUnaryOp(opCode))
		{
			logInstruction(&i);
			const llvm::Value* op1 = i.getOperand(0);
			const auto node = fetchInfo(op1);
			const range_ptr_t info1 = (node1) ? node1->getScalarRange() : nullptr;
			const auto res = handleUnaryInstruction(info1, opCode);
			saveValueInfo(&i, res);

			DEBUG(if (!info1) logInfo("operand range is null"));
			logRangeln(res);
		}
#endif
		else {
			generic_range_ptr_t tmp;
			switch (opCode) {
				// memory operations
				case llvm::Instruction::Alloca:
					// do nothing
					break;
				case llvm::Instruction::Load:
					tmp = handleLoadInstr(&i);
					saveValueInfo(&i, tmp);
					break;
				case llvm::Instruction::Store:
					handleStoreInstr(&i);
					break;
				case llvm::Instruction::GetElementPtr:
					tmp = handleGEPInstr(&i);
					saveValueInfo(&i, tmp);
					break;
				case llvm::Instruction::Fence:
					emitError("Handling of Fence not supported yet");
					break; // TODO implement
				case llvm::Instruction::AtomicCmpXchg:
					emitError("Handling of AtomicCmpXchg not supported yet");
					break; // TODO implement
				case llvm::Instruction::AtomicRMW:
					emitError("Handling of AtomicRMW not supported yet");
					break; // TODO implement

				// other operations
				case llvm::Instruction::ICmp:
				case llvm::Instruction::FCmp:
					tmp = handleCmpInstr(&i);
					saveValueInfo(&i, tmp);
					break;
				case llvm::Instruction::PHI:
					tmp = handlePhiNode(&i);
					saveValueInfo(&i, tmp);
					break;
				case llvm::Instruction::Select: // TODO implement
					emitError("Handling of Select not supported yet");
					break;
				case llvm::Instruction::UserOp1: // TODO implement
				case llvm::Instruction::UserOp2: // TODO implement
					emitError("Handling of UserOp not supported yet");
					break;
				case llvm::Instruction::VAArg: // TODO implement
					emitError("Handling of VAArg not supported yet");
					break;
				case llvm::Instruction::ExtractElement: // TODO implement
					emitError("Handling of ExtractElement not supported yet");
					break;
				case llvm::Instruction::InsertElement: // TODO implement
					emitError("Handling of InsertElement not supported yet");
					break;
				case llvm::Instruction::ShuffleVector: // TODO implement
					emitError("Handling of ShuffleVector not supported yet");
					break;
				case llvm::Instruction::ExtractValue: // TODO implement
					emitError("Handling of ExtractValue not supported yet");
					break;
				case llvm::Instruction::InsertValue: // TODO implement
					emitError("Handling of InsertValue not supported yet");
					break;
				case llvm::Instruction::LandingPad: // TODO implement
					emitError("Handling of LandingPad not supported yet");
					break;
				default:
					emitError("unknown instruction " + std::to_string(opCode));
					break;
				}
			// TODO here be dragons
		} // end else
	}
	return;
}

//-----------------------------------------------------------------------------
// HANDLE BB TERMINATORS
//-----------------------------------------------------------------------------
void ValueRangeAnalysis::handleTerminators(const llvm::Instruction* term)
{
	const unsigned opCode = term->getOpcode();
	switch (opCode) {
		case llvm::Instruction::Ret:
			handleReturn(term);
			break;
		case llvm::Instruction::Br:
			// TODO improve by checking condition and relatevely update BB weigths
			// do nothing
			break;
		case llvm::Instruction::Switch:
			emitError("Handling of Switch not implemented yet");
			break; // TODO implement
		case llvm::Instruction::IndirectBr:
			emitError("Handling of IndirectBr not implemented yet");
			break; // TODO implement
		case llvm::Instruction::Invoke:
			#if LLVM_VERSION < 8
			handleInvokeInst(term);
			#else
			handleCallBase(term);
			#endif
			break;
		case llvm::Instruction::Resume:
			emitError("Handling of Resume not implemented yet");
			break; // TODO implement
		case llvm::Instruction::Unreachable:
			emitError("Handling of Unreachable not implemented yet");
			break; // TODO implement
		case llvm::Instruction::CleanupRet:
			emitError("Handling of CleanupRet not implemented yet");
			break; // TODO implement
		case llvm::Instruction::CatchRet:
			emitError("Handling of CatchRet not implemented yet");
			break; // TODO implement
		case llvm::Instruction::CatchSwitch:
			emitError("Handling of CatchSwitch not implemented yet");
			break; // TODO implement
		default:
			break;
	}

	return;
}

//-----------------------------------------------------------------------------
// HANDLE CALL INSTRUCITON
//-----------------------------------------------------------------------------
// llvm::CallBase was promoted from template to Class in LLVM 8.0.0
#if LLVM_VERSION < 8
void ValueRangeAnalysis::handleCallInst(const llvm::Instruction* call)
{
	return handleCallBase<llvm::CallInst>(call);
}

void ValueRangeAnalysis::handleInvokeInst(const llvm::Instruction* call)
{
	return handleCallBase<llvm::InvokeInst>(call);
}

template<typename CallBase>
#else
using CallBase = llvm::CallBase;
#endif
void ValueRangeAnalysis::handleCallBase(const llvm::Instruction* call)
{
	const CallBase* call_i = dyn_cast<CallBase>(call);
	if (!call_i) {
		emitError("Cannot cast a call instruction to llvm::CallBase");
		return;
	}
	logInstruction(call);
	// fetch function name
	llvm::Function* callee = call_i->getCalledFunction();
	if (callee == nullptr) {
		logError("indirect calls not supported yet");
		return;
	}
	const std::string calledFunctionName = callee->getName();

	// fetch ranges of arguments
	std::list<generic_range_ptr_t> arg_ranges;
	std::list<range_ptr_t> arg_scalar_ranges;
	for(auto arg_it = call_i->arg_begin(); arg_it != call_i->arg_end(); ++arg_it)
	{
		const generic_range_ptr_t arg_info = fetchInfo(*arg_it);
		if (arg_info) {
			const range_ptr_t arg_info_scalar = std::dynamic_ptr_cast<range_t>(arg_info);
			if (arg_info_scalar) {
				arg_scalar_ranges.push_back(arg_info_scalar);
			}
		}
		arg_ranges.push_back(arg_info);
	}

	// first check if it is among the whitelisted functions we can handle
	generic_range_ptr_t res = nullptr;
	if (arg_scalar_ranges.size() == arg_ranges.size()) {
		res = handleMathCallInstruction(arg_scalar_ranges, calledFunctionName);
	}

	if (res) {
		saveValueInfo(call, res);
		logInfo("whitelisted");
		logRangeln(res);
		return;
	}

	// if not a whitelisted then try to fetch it from Module
	// fetch llvm::Function
	auto function = known_functions.find(calledFunctionName);
	if (function != known_functions.end()) {
		// got the llvm::Function
		llvm::Function* f = function->second;

		// check for recursion
		size_t call_count = 0;
		for (size_t i = 0; i < call_stack.size(); i++) {
			if (call_stack[i] == f) {
				call_count++;
			}
		}
		if (call_count <= find_recursion_count(f)) {
			// Can process
			// update parameter metadata
		        DEBUG(dbgs() << "processing function...\n");
			fun_arg_derived[f] = arg_ranges;
			processFunction(*f);
			DEBUG(dbgs() << "[TAFFO][VRA] Finished processing call "
			      << *call << " : ");
		} else {
			logError("Exceeding recursion count - skip call");
		}
		// fetch function return value
		auto res_it = return_values.find(f);
		if (res_it != return_values.end()) {
		  res = res_it->second;
		} else {
		  logError("function " + calledFunctionName + " returns nothing");
		}
	} else {
		const auto intrinsicsID = callee->getIntrinsicID();
		if (intrinsicsID != llvm::Intrinsic::not_intrinsic) {
			emitError("call to unknown function " + calledFunctionName);
			// TODO handle case of external function call
		} else {
			emitError("skipping intrinsic " + calledFunctionName);
			// TODO handle case of llvm intrinsics function call
		}
	}
	saveValueInfo(call, res);
	logRangeln(res);
	return;
}

//-----------------------------------------------------------------------------
// HANDLE RETURN INSTRUCITON
//-----------------------------------------------------------------------------
void ValueRangeAnalysis::handleReturn(const llvm::Instruction* ret)
{
	const llvm::ReturnInst* ret_i = dyn_cast<llvm::ReturnInst>(ret);
	if (!ret_i) {
		emitError("Could not convert Return Instruction to llvm::ReturnInstr");
		return;
	}
	logInstruction(ret);
	const llvm::Value* ret_val = ret_i->getReturnValue();
	const llvm::Function* ret_fun = ret_i->getFunction();
	generic_range_ptr_t range = fetchInfo(ret_val);
	generic_range_ptr_t partial = find_ret_val(ret_fun);
	return_values[ret_fun] = getUnionRange(partial, range);
	logRangeln(return_values[ret_fun]);
	return;
}

//-----------------------------------------------------------------------------
// FINALIZATION
//-----------------------------------------------------------------------------
void ValueRangeAnalysis::saveResults(llvm::Module &M)
{
	MetadataManager &MDManager = MetadataManager::getMetadataManager();
	for (auto &v : M.globals()) {
		// retrieve info about global var v, if any
		InputInfo *II = MDManager.retrieveInputInfo(v);
		if (II != nullptr) {
			const auto range = fetchInfo(&v);
			if (range != nullptr) {
				const range_ptr_t scalar =
				  std::dynamic_ptr_cast<range_t>(range);
				if (scalar != nullptr) {
					II->IRange.reset(new Range(scalar->min(), scalar->max()));
					MDManager.setInputInfoMetadata(v, *II);
				}
			} else {
				// TODO set default
			}
		}
	} // end globals

	for (auto &f : M.functions()) {

		// arg range
		SmallVector<mdutils::MDInfo*, 5> argsII;
		MDManager.retrieveArgumentInputInfo(f, argsII);
		std::list<InputInfo> newII;
		auto argsIt = argsII.begin();
		for (Argument &arg : f.args()) {
			const auto range = fetchInfo(&arg);
			if (range != nullptr) {
				const range_ptr_t scalar =
				  std::dynamic_ptr_cast<range_t>(range);
				if (scalar != nullptr) {
			  		std::shared_ptr<Range> newRange =
					  std::make_shared<Range>(scalar->min(), scalar->max());
					if (argsIt != argsII.end()) {
						if (*argsIt != nullptr) {
							InputInfo *ii = cast<InputInfo>(*argsIt);
							ii->IRange.swap(newRange);
						} else {
							newII.push_back(InputInfo(nullptr, newRange, nullptr));
							*argsIt = &newII.back();
						}
					} else {
						newII.push_back(InputInfo(nullptr, newRange, nullptr));
						argsII.push_back(&newII.back());
					}
				}
			} else {
				// TODO set default
			}
			argsIt++;
		}
		MDManager.setArgumentInputInfoMetadata(f, argsII);

		// retrieve info about instructions, for each basic block bb
		for (auto &bb : f.getBasicBlockList()) {
			for (auto &i : bb.getInstList()) {
				// fetch info about Instruction i, if any
				InputInfo *II = MDManager.retrieveInputInfo(i);
				const auto range = fetchInfo(&i);
				const range_ptr_t scalar =
				  std::dynamic_ptr_cast_or_null<range_t>(range);
				if (scalar != nullptr) {
				  if (II != nullptr) {
				    if (scalar != nullptr) {
				      II->IRange.reset(new Range(scalar->min(), scalar->max()));
				      MDManager.setInputInfoMetadata(i, *II);
				    } else {
				      // TODO set default
				    }
				  } else {
				    if (scalar != nullptr) {
				      InputInfo NewII;
				      NewII.IRange.reset(new Range(scalar->min(), scalar->max()));
				      MDManager.setInputInfoMetadata(i, NewII);
				    } else {
				      // TODO set default
				    }
				  }
				}
			} // end inst list
		} // end bb
	} // end function
	return;
}


//-----------------------------------------------------------------------------
// HANDLE MEMORY OPERATIONS
//-----------------------------------------------------------------------------
void ValueRangeAnalysis::handleStoreInstr(const llvm::Instruction* store)
{
	const llvm::StoreInst* store_i = dyn_cast<llvm::StoreInst>(store);
	if (!store_i) {
		emitError("Could not convert store instruction to StoreInst");
		return;
	}
	logInstruction(store);
	const llvm::Value* address_param = store_i->getPointerOperand();
	const llvm::Value* value_param = store_i->getValueOperand();
	const generic_range_ptr_t range = fetchInfo(value_param);
	saveValueInfo(address_param, range);
	saveValueInfo(store_i, range);
	logRangeln(range);
	return;
}

//-----------------------------------------------------------------------------
// HANDLE MEMORY OPERATIONS
//-----------------------------------------------------------------------------
generic_range_ptr_t ValueRangeAnalysis::handleLoadInstr(llvm::Instruction* load)
{
	llvm::LoadInst* load_i = dyn_cast<llvm::LoadInst>(load);
	if (!load_i) {
		emitError("Could not convert load instruction to LoadInst");
		return nullptr;
	}
	logInstruction(load);
	MemorySSA& memssa = getAnalysis<MemorySSAWrapperPass>(*load->getFunction()).getMSSA();
	MemSSAUtils memssa_utils(memssa);
	SmallVectorImpl<Value*>& def_vals = memssa_utils.getDefiningValues(load_i);
	def_vals.push_back(load_i->getPointerOperand());

	generic_range_ptr_t res = nullptr;
	for (Value *dval : def_vals) {
	    res = getUnionRange(res, fetchInfo(dval));
	}
	logRangeln(res);
	return res;
}

generic_range_ptr_t ValueRangeAnalysis::handleGEPInstr(const llvm::Instruction* gep) {
	const llvm::GetElementPtrInst* gep_i = dyn_cast<llvm::GetElementPtrInst>(gep);

	VRA_RangeNode* node = getNode(gep);
	if (node != nullptr) {
		if (node->hasRange())
			return node->getRange();
	} else {
		Type* t = gep_i->getSourceElementType();
		std::vector<unsigned> offset;
		for (auto idx_it = gep_i->idx_begin() + 1; // skip first index
		     idx_it != gep_i->idx_end(); ++idx_it) {
			if (isa<SequentialType>(t))
				continue;
			const llvm::ConstantInt* int_i = dyn_cast<llvm::ConstantInt>(*idx_it);
			if (int_i) {
				int n = static_cast<int>(int_i->getSExtValue());
				offset.push_back(n);
				t = cast<StructType>(t)->getTypeAtIndex(n);
			} else {
				emitError("Index of GEP not constant");
				return nullptr;
			}
		}
		node = new VRA_RangeNode(gep_i->getPointerOperand(), offset);
		derived_ranges[gep] = node;
	}

	return fetchInfo(gep_i);
}


//-----------------------------------------------------------------------------
// HANDLE PHI NODE
//-----------------------------------------------------------------------------
generic_range_ptr_t ValueRangeAnalysis::handlePhiNode(const llvm::Instruction* phi)
{
	const llvm::PHINode* phi_n = dyn_cast<llvm::PHINode>(phi);
	if (!phi_n) {
		emitError("Could not convert Phi instruction to PHINode");
		return nullptr;
	}
	if (phi_n->getNumIncomingValues() < 1) {
		return nullptr;
	}
	logInstruction(phi);
	const llvm::Value* op0 = phi_n->getIncomingValue(0);
	generic_range_ptr_t res = fetchInfo(op0);
	for (unsigned index = 1; index < phi_n->getNumIncomingValues(); index++) {
		const llvm::Value* op = phi_n->getIncomingValue(index);
		generic_range_ptr_t op_range = fetchInfo(op);
		res = getUnionRange(res, op_range);
	}
	logRangeln(res);
	return res;
}

//-----------------------------------------------------------------------------
// HANDLE COMPARE OPERATIONS
//-----------------------------------------------------------------------------
range_ptr_t ValueRangeAnalysis::handleCmpInstr(const llvm::Instruction* cmp)
{
	const llvm::CmpInst* cmp_i = dyn_cast<llvm::CmpInst>(cmp);
	if (!cmp_i) {
		emitError("Could not convert Compare instruction to CmpInst");
		return nullptr;
	}
	logInstruction(cmp);
	const llvm::CmpInst::Predicate pred = cmp_i->getPredicate();
	std::list<generic_range_ptr_t> ranges;
	for (unsigned index = 0; index < cmp_i->getNumOperands(); index++) {
		const llvm::Value* op = cmp_i->getOperand(index);
		generic_range_ptr_t op_range = fetchInfo(op);
		ranges.push_back(op_range);
	}
	range_ptr_t result = std::dynamic_ptr_cast_or_null<range_t>(handleCompare(ranges, pred));
	logRangeln(result);
	return result;
}

//-----------------------------------------------------------------------------
// FIND RETURN VALUE RANGE
//-----------------------------------------------------------------------------
generic_range_ptr_t ValueRangeAnalysis::find_ret_val(const llvm::Function* f)
{
	using iter_t = decltype(return_values)::const_iterator;
	iter_t it = return_values.find(f);
	if (it != return_values.end()) {
		return it->second;
	}
	return nullptr;
}

//-----------------------------------------------------------------------------
// FIND REMAINING RECURSION BUDGET
//-----------------------------------------------------------------------------
unsigned ValueRangeAnalysis::find_recursion_count(const llvm::Function* f)
{
	using iter_t = decltype(fun_rec_count)::const_iterator;
	iter_t it = fun_rec_count.find(f);
	if (it != fun_rec_count.end()) {
		unsigned tmp = it->second;
		return tmp;
	}
	return default_function_recursion_count;
}

//-----------------------------------------------------------------------------
// RETRIEVE INFO
//-----------------------------------------------------------------------------
const generic_range_ptr_t ValueRangeAnalysis::fetchInfo(const llvm::Value* v)
{
	auto input_it = user_input.find(v);
	if (input_it != user_input.end()) {
	 	return input_it->second;
	}

	if (const auto node = getNode(v)) {
		if (node->isScalar()) {
			return node->getRange();
		} else {
			std::list<std::vector<unsigned>> offset;
			return fetchRange(node, offset);
		}
	}
	const llvm::Constant* const_i = dyn_cast_or_null<llvm::Constant>(v);
	if (const_i) {
		const range_ptr_t k = fetchConstant(const_i);
		saveValueInfo(v, k);
		return k;
	}
	// no info available
	return nullptr;
}

//-----------------------------------------------------------------------------
// HELPER TO EXTRACT VALUE FROM A CONSTANT
//-----------------------------------------------------------------------------
range_ptr_t ValueRangeAnalysis::fetchConstant(const llvm::Constant* kval)
{
	const llvm::ConstantInt* int_i = dyn_cast<llvm::ConstantInt>(kval);
	if (int_i) {
		const num_t k = static_cast<num_t>(int_i->getSExtValue());
		return make_range(k, k);
	}
	const llvm::ConstantFP* fp_i = dyn_cast<llvm::ConstantFP>(kval);
	if (fp_i) {
		APFloat tmp = fp_i->getValueAPF();
		bool losesInfo;
		tmp.convert(APFloatBase::IEEEdouble(), APFloat::roundingMode::rmNearestTiesToEven, &losesInfo);
		const num_t k = static_cast<num_t>(tmp.convertToDouble());
		return make_range(k, k);
	}
	const llvm::ConstantData* data = dyn_cast<llvm::ConstantData>(kval);
	if (data) {
		emitError("Extract value from llvm::ConstantData not implemented yet");
		return nullptr;
	}
	const llvm::ConstantExpr* cexp_i = dyn_cast<llvm::ConstantExpr>(kval);
	if (cexp_i) {
		emitError("Could not fold a llvm::ConstantExpr");
		return nullptr;
	}
	const llvm::ConstantAggregate* aggr_i = dyn_cast<llvm::ConstantAggregate>(kval);
	if (aggr_i) {
		// TODO implement
		emitError("Constant aggregates not supported yet");
		return nullptr;
	}
	const llvm::BlockAddress* block_i = dyn_cast<llvm::BlockAddress>(kval);
	if (block_i) {
		emitError("Could not fetch range from llvm::BlockAddress");
		return nullptr;
	}
	const llvm::GlobalValue* gv_i = dyn_cast<llvm::GlobalValue>(kval);
	if (gv_i) {
		const llvm::GlobalVariable* gvar_i = dyn_cast<llvm::GlobalVariable>(kval);
		if (gvar_i) {
			if (gvar_i->hasInitializer()) {
				const llvm::Constant* init_val = gvar_i->getInitializer();
				if (init_val) {
					return fetchConstant(init_val);
				}
			}
			emitError("Could not derive range from a Global Variable");
			return nullptr;
		}
		const llvm::GlobalAlias* alias_i = dyn_cast<llvm::GlobalAlias>(kval);
		if (alias_i) {
			emitError("Found alias");
			const llvm::Constant* aliasee = alias_i->getAliasee();
			return (aliasee) ? fetchConstant(aliasee) : nullptr;
		}
		const llvm::Function* f = dyn_cast<llvm::Function>(kval);
		if (f) {
			emitError("Could not derive range from a Constant Function");
			return nullptr;
		}
		const llvm::GlobalIFunc* fun_decl = dyn_cast<llvm::GlobalIFunc>(kval);
		if (fun_decl) {
			emitError("Could not derive range from a Function declaration");
			return nullptr;
		}
		// this line should never be reached
		emitError("Could not fetch range from llvm::GlobalValue");
		return nullptr;
	}
	emitError("Could not fetch range from llvm::Constant");
	// TODO did I forget something?
	return nullptr;
}

//-----------------------------------------------------------------------------
// SAVE VALUE INFO
//-----------------------------------------------------------------------------
void ValueRangeAnalysis::saveValueInfo(const llvm::Value* v, const generic_range_ptr_t& info)
{
	if (VRA_RangeNode* node = getNode(v)) {
		if (!node->hasRange()) {
  			node->setRange(info);
			return;
		}

		// set
		std::list<std::vector<unsigned>> offset;
		const generic_range_ptr_t old = fetchRange(node, offset);
		const generic_range_ptr_t updated = getUnionRange(old, info);

		offset.clear();
		setRange(node, updated, offset);
		return;
	}

	derived_ranges[v] = new VRA_RangeNode(info);
	return;
}

VRA_RangeNode* ValueRangeAnalysis::getNode(const llvm::Value* v) const
{
	using iter_t = decltype(derived_ranges)::const_iterator;
	iter_t it = derived_ranges.find(v);
	if (it != derived_ranges.end()) {
		return it->second;
	}
	// TODO: get actual parameter if v is an Argument
	return nullptr;
}

VRA_RangeNode* ValueRangeAnalysis::getOrCreateNode(const llvm::Value* v)
{
	VRA_RangeNode* ret = getNode(v);
	if (ret == nullptr
	    && (isa<AllocaInst>(v) || isa<GlobalVariable>(v) || isa<Argument>(v))) {
	    	// create root node
		ret = new VRA_RangeNode();
		derived_ranges[v] = ret;
	}
	return ret;
}

generic_range_ptr_t ValueRangeAnalysis::fetchRange(const VRA_RangeNode* node,
						   std::list<std::vector<unsigned>>& offset) const
{
	if (!node) return nullptr;
	if (node->hasRange()) {
		if (node->isScalar()) {
			return node->getScalarRange();
		} else if (node->isStruct()) {
			if (offset.empty())
				return node->getStructRange();
			range_s_ptr_t current = node->getStructRange();
			generic_range_ptr_t child = nullptr;
			for (auto offset_it = offset.rbegin();
			     offset_it != offset.rend(); ++offset_it) {
				for (unsigned idx : *offset_it) {
					child = current->getRangeAt(idx);
					current = std::dynamic_ptr_cast_or_null<VRA_Structured_Range>(child);
					if (!current)
					  break;
				}
			}
			return child;
		}
		// should have a range, but it is empty
		return nullptr;
	}
	// if this is not a range-node, lookup parent
	offset.push_back(node->getOffset());
	return fetchRange(getNode(node->getParent()), offset);
}


void ValueRangeAnalysis::setRange(VRA_RangeNode* node, const generic_range_ptr_t& info,
				  std::list<std::vector<unsigned>>& offset)
{
	if (!node || !info) return;
	if (node->hasRange() || node->getParent() == nullptr) {
		if (node->isScalar() || (!node->isStruct() && offset.empty())) {
			const range_ptr_t scalar_info = std::static_ptr_cast<range_t>(info);
			node->setRange(scalar_info);
		} else if (offset.empty()) {
			node->setRange(info);
		} else {
			range_s_ptr_t parent = node->getStructRange();
			if (parent == nullptr) {
				parent = make_s_range();
				node->setStructRange(parent);
			}

			range_s_ptr_t child = nullptr;
			unsigned child_idx = 0U;
			for (auto offset_it = offset.rbegin();
			     offset_it != offset.rend(); ++offset_it) {
				for (unsigned idx : *offset_it) {
					child_idx = idx;
					generic_range_ptr_t gen_child = parent->getRangeAt(idx);
					if (gen_child == nullptr) {
						child = make_s_range();
						parent->setRangeAt(idx, child);
					} else {
						child = std::dynamic_ptr_cast<VRA_Structured_Range>(gen_child);
						if (child == nullptr)
						  break;
					}
					parent = child;
				}
			}
			parent->setRangeAt(child_idx, info);
		}
	} else {
		// recursively call parent to update its structure
		offset.push_back(node->getOffset());
		setRange(getOrCreateNode(node->getParent()), info, offset);
	}
}

//-----------------------------------------------------------------------------
// PRINT ERROR MESSAGE
//-----------------------------------------------------------------------------
void ValueRangeAnalysis::emitError(const std::string& message)
{
	llvm::dbgs() << "[TAFFO] Value Range Analysis: " << message << "\n";
	return;
}

void ValueRangeAnalysis::logInstruction(const llvm::Value* v)
{
        assert(v != nullptr);
        DEBUG(dbgs() << DEBUG_HEAD << *v << " : ");
}

std::string ValueRangeAnalysis::to_string(const generic_range_ptr_t& range)
{
        if (range != nullptr) {
	  const range_ptr_t scalar = std::dynamic_ptr_cast<range_t>(range);
	  if (scalar != nullptr) {
	    return "[" + std::to_string(scalar->min()) + ", "
	      + std::to_string(scalar->max()) + "]";
	  } else {
	    return "struct";
	  }
	} else {
	  return "null range!";
	}
}

void ValueRangeAnalysis::logRangeln(const generic_range_ptr_t& range)
{
	DEBUG(dbgs() << to_string(range) << "\n");
}

void ValueRangeAnalysis::logInfo(const llvm::StringRef info)
{
        DEBUG(dbgs() << "(" << info << ") ");
}

void ValueRangeAnalysis::logError(const llvm::StringRef error)
{
        DEBUG(dbgs() << error << "\n");
}
