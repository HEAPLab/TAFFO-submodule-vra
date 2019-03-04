#ifndef TAFFO_VALUE_RANGE_ANALYSIS_HPP
#define TAFFO_VALUE_RANGE_ANALYSIS_HPP

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Support/Debug.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/CommandLine.h"

#include "Range.hpp"

#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#define DEBUG_TYPE "taffo-vra"
#define DEBUG_VRA "ValueRangeAnalysis"

namespace taffo {

struct ValueRangeAnalysis : public llvm::ModulePass {
	static char ID;

public:
	ValueRangeAnalysis(): ModulePass(ID) { }

	bool runOnModule(llvm::Module &M) override;

	void getAnalysisUsage(llvm::AnalysisUsage &) const override;

	// methods
private:
	void harvestMetadata(llvm::Module &M);

	void processModule(llvm::Module &M);

	void processFunction(llvm::Function& F);

	void processBasicBlock(llvm::BasicBlock& BB);

	// terminator instructions cannot be offloaded to other files as they act on
	// local structures
	void handleTerminators(const llvm::Instruction* term);

	// llvm::CallBase was promoted from template to Class in LLVM 8.0.0
#if LLVM_VERSION < 8
	inline void handleCallInst(const llvm::Instruction* call);

	inline void handleInvokeInst(const llvm::Instruction* call);

	template<typename CallBase>
#endif
	inline void handleCallBase(const llvm::Instruction* call);

	inline void handleReturn(const llvm::Instruction* ret);

	void saveResults(llvm::Module &M);

	inline void handleStoreInstr(const llvm::Instruction* store);

	inline range_ptr_t handleLoadInstr(llvm::Instruction* load);

	inline range_ptr_t handleCmpInstr(const llvm::Instruction* cmp);

	inline range_ptr_t handlePhiNode(const llvm::Instruction* phi);

	inline range_ptr_t find_ret_val(const llvm::Function* f);

	inline unsigned find_recursion_count(const llvm::Function* f);

protected:
	const range_ptr_t fetchInfo(const llvm::Value* v) const;

	void saveValueInfo(const llvm::Value* v, const range_ptr_t& info);

	static inline range_ptr_t fetchConstant(const llvm::Constant* v);

	static void emitError(const std::string& message);

	// data structures
private:
	const unsigned bb_base_priority = 1;
	unsigned default_loop_iteration_count = 1;
	const unsigned default_function_recursion_count = 0;

	// TODO find a better ID than pointer to llvm::Value. Value name?
	llvm::DenseMap<const llvm::Value*, range_ptr_t> user_input;
	llvm::DenseMap<const llvm::Value*, range_ptr_t> derived_ranges;
	llvm::DenseMap<const llvm::Function*, std::list<range_ptr_t> > fun_arg_input;
	llvm::DenseMap<const llvm::Function*, std::list<range_ptr_t> > fun_arg_derived;
	llvm::DenseMap<const llvm::Value*, range_ptr_t> memory;
	llvm::DenseMap<const llvm::Function*, unsigned> fun_rec_count;
	llvm::DenseMap<const llvm::Loop*, unsigned> user_loop_iterations;
	llvm::DenseMap<const llvm::Loop*, unsigned> derived_loop_iterations;
	llvm::DenseMap<const llvm::BasicBlock*, unsigned> bb_priority;

	std::set<llvm::Function*> f_unvisited_set;
	std::unordered_map<std::string, llvm::Function*> known_functions;

	std::vector<llvm::Function*> call_stack;
	llvm::DenseMap<const llvm::Function*, range_ptr_t> return_values;
	// TODO return_values needs to be duplicated into partial and final to properly handle recursion

};

}

#endif /* end of include guard: TAFFO_VALUE_RANGE_ANALYSIS_HPP */
