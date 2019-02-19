#include "ValueRangeAnalysis.hpp"
#include "InputInfo.h"
#include "RangeOperations.hpp"
#include "Metadata.h"

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
		if (II != nullptr && isValidRange(II->IRange)) {
			const llvm::Value* v_ptr = &v;
			user_input[v_ptr] = make_range(II->IRange->Min, II->IRange->Max);
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
		fun_arg_input[&f] = std::list<range_ptr_t>();
		for (auto itII = argsII.begin(); itII != argsII.end(); itII++) {
			// TODO: struct support
			InputInfo *ii = dyn_cast<InputInfo>(*itII);
			if (ii != nullptr && isValidRange(ii->IRange)) {
				fun_arg_input[&f].push_back(make_range(ii->IRange->Min, ii->IRange->Max));
			} else {
				fun_arg_input[&f].push_back(nullptr);
			}
		}

		// retrieve info about instructions, for each basic block bb
		for (const auto &bb : f.getBasicBlockList()) {
			for (const auto &i : bb.getInstList()) {
				// fetch info about Instruction i, if any
				InputInfo *II = MDManager.retrieveInputInfo(i);
				if (II != nullptr && isValidRange(II->IRange)) {
					const llvm::Value* i_ptr = &i;
          user_input[i_ptr] = make_range(II->IRange->Min, II->IRange->Max);
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
    if (f_ptr->empty())
      continue;
		f_unvisited_set.insert(f_ptr);
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
	// if already available, no need to execute again
	range_ptr_t tmp = find_ret_val(&F);
	if (tmp != nullptr) {
		return;
	}

	// fetch info about actual parameters
	auto param_lookup_it = fun_arg_input.find(&F);
	if (param_lookup_it == fun_arg_input.end()) {
		param_lookup_it = fun_arg_derived.find(&F);
		if (param_lookup_it != fun_arg_derived.end()) {
			// save derived info
			auto param_val_it = F.arg_begin();
			auto param_info_it = param_lookup_it->second.begin();
			while (param_val_it != F.arg_end()) {
				llvm::Argument* arg_ptr = param_val_it;
				llvm::Value* arg_val = dyn_cast<llvm::Value>(arg_ptr);
				saveValueInfo(arg_val, *param_info_it);
				param_info_it++;
				param_val_it++;
			}
		}
	} else {
		// save input info
		auto param_val_it = F.arg_begin();
		auto param_info_it = param_lookup_it->second.begin();
		while (param_val_it != F.arg_end()) {
			llvm::Argument* arg_ptr = param_val_it;
			llvm::Value* arg_val = dyn_cast<llvm::Value>(arg_ptr);
			saveValueInfo(arg_val, *param_info_it);
			param_info_it++;
			param_val_it++;
		}
	}

	// update stack for the simulation of execution
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
	f_unvisited_set.erase(&F);
	// TODO keep a range for possible return value
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
			range_ptr_t tmp;
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
					break; // TODO implement
				case llvm::Instruction::Fence:
					break; // TODO implement
				case llvm::Instruction::AtomicCmpXchg:
					break; // TODO implement
				case llvm::Instruction::AtomicRMW:
					break; // TODO implement
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
	// fetch function name
	llvm::Function* callee = call_i->getCalledFunction();
	const std::string calledFunctionName = callee->getName();
	std::list<range_ptr_t> arg_ranges;
	for(auto arg_it = call_i->arg_begin(); arg_it != call_i->arg_end(); ++arg_it)
	{
		const llvm::Value* arg = *arg_it;
		const range_ptr_t arg_info = fetchInfo(arg);
		arg_ranges.push_back(arg_info);
	}

	// first check if it is among the whitelisted functions we can handle
	range_ptr_t res = handleMathCallInstruction(arg_ranges, calledFunctionName);

	if (res) {
		saveValueInfo(call, res);
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
			fun_arg_derived[f] = arg_ranges;
			processFunction(*f);
		} else {
			emitError("Exceeding recursion count - skip call");
		}
		// fetch function return value
		auto res_it = return_values.find(f);
		res = res_it->second;
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
	const llvm::Value* ret_val = ret_i->getReturnValue();
	const llvm::Function* ret_fun = ret_i->getFunction();
	range_ptr_t range = fetchInfo(ret_val);
	range_ptr_t partial = find_ret_val(ret_fun);
	return_values[ret_fun] = getUnionRange(partial, range);
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
				II->IRange = new Range(range->min(), range->max());
				MDManager.setInputInfoMetadata(v, *II);
			} else {
				// TODO set default
			}
		}
	} // end globals

	for (auto &f : M.functions()) {

		// arg range
		SmallVector<mdutils::MDInfo*, 5> argsII;
		MDManager.retrieveArgumentInputInfo(f, argsII);
		auto argsIt = argsII.begin();
		for (Argument &arg : f.args()) {
			const auto range = fetchInfo(&arg);
			if (range != nullptr) {
				// TODO struct support
				InputInfo *ii = cast<InputInfo>(*argsIt);
				ii->IRange = new Range(range->min(), range->max());
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
				if (II != nullptr) {
					const auto range = fetchInfo(&i);
					if (range != nullptr) {
						II->IRange = new Range(range->min(), range->max());
						MDManager.setInputInfoMetadata(i, *II);
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
// HANDLE MEMORY OPERATIONS
//-----------------------------------------------------------------------------
void ValueRangeAnalysis::handleStoreInstr(const llvm::Instruction* store)
{
	const llvm::StoreInst* store_i = dyn_cast<llvm::StoreInst>(store);
	if (!store_i) {
		emitError("Could not convert store instruction to StoreInst");
		return;
	}
	const llvm::Value* address_param = store_i->getPointerOperand();
	const llvm::Value* value_param = store_i->getValueOperand();
	const range_ptr_t range = fetchInfo(value_param);
	// TODO check for possible alias
	memory[address_param] = range;
	return;
}

//-----------------------------------------------------------------------------
// HANDLE MEMORY OPERATIONS
//-----------------------------------------------------------------------------
range_ptr_t ValueRangeAnalysis::handleLoadInstr(const llvm::Instruction* load)
{
	const llvm::LoadInst* load_i = dyn_cast<llvm::LoadInst>(load);
	if (!load_i) {
		emitError("Could not convert load instruction to LoadInst");
		return nullptr;
	}
	const llvm::Value* address_param = load_i->getPointerOperand();
	// TODO check for possible alias
	using const_it = decltype(memory)::const_iterator;
	const_it it = memory.find(address_param);
	if (it != memory.end()) {
		return it->second;
	}
	return nullptr;
}

//-----------------------------------------------------------------------------
// FIND RETURN VALUE RANGE
//-----------------------------------------------------------------------------
range_ptr_t ValueRangeAnalysis::find_ret_val(const llvm::Function* f)
{
	using iter_t = decltype(return_values)::const_iterator;
	iter_t it = return_values.find(f);
	if (it != return_values.end()) {
		range_ptr_t tmp = it->second;
		return tmp;
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
	const llvm::Constant* const_i = dyn_cast<llvm::Constant>(v);
	if (const_i) {
		return fetchConstant(const_i);
	}
	// no info available
	return nullptr;
}

//-----------------------------------------------------------------------------
// HELPER TO EXTRACT VALUE FROM A CONSTANT
//-----------------------------------------------------------------------------
range_ptr_t ValueRangeAnalysis::fetchConstant(const llvm::Constant* kval) const
{
	const llvm::ConstantInt* int_i = dyn_cast<llvm::ConstantInt>(kval);
	if (int_i) {
		const num_t k = static_cast<num_t>(int_i->getSExtValue());
		return make_range(k, k);
	}
	const llvm::ConstantFP* fp_i = dyn_cast<llvm::ConstantFP>(kval);
	if (fp_i) {
		const num_t k = static_cast<num_t>(fp_i->getValueAPF().convertToDouble());
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
	// TODO did I forgot something?
	return nullptr;
}

//-----------------------------------------------------------------------------
// SAVE VALUE INFO
//-----------------------------------------------------------------------------
void ValueRangeAnalysis::saveValueInfo(const llvm::Value* v, const range_ptr_t& info)
{
	using iter_t = decltype(derived_ranges)::const_iterator;
	iter_t it = derived_ranges.find(v);
	if (it != derived_ranges.end()) {
		derived_ranges[v] = getUnionRange(it->second, info);
		return;
	}
	derived_ranges[v] = info;
	return;
}

//-----------------------------------------------------------------------------
// PRINT ERROR MESSAGE
//-----------------------------------------------------------------------------
void ValueRangeAnalysis::emitError(const std::string& message) const
{
	llvm::dbgs() << "[TAFFO] Value Range Analysis: " << message << "\n";
	return;
}
