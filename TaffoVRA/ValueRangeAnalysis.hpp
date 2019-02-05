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

	void saveResults(const llvm::Module &M);

	const range_ptr_t fetchInfo(const llvm::Value* v) const;

	void saveValueInfo(const llvm::Value* v, const range_ptr_t& info);

	// data structures
private:
	const unsigned bb_base_priority = 1;

	// TODO find a better ID than pointer to llvm::Value. Value name?
	llvm::DenseMap<const llvm::Value*, range_ptr_t> user_input;
	llvm::DenseMap<const llvm::Value*, range_ptr_t> derived_ranges;
	llvm::DenseMap<const llvm::Function*, std::vector<range_ptr_t> > fun_arg_input;
	llvm::DenseMap<const llvm::Function*, std::vector<range_ptr_t> > fun_arg_derived;
	llvm::DenseMap<const llvm::Loop*, unsigned> user_loop_iterations;
	llvm::DenseMap<const llvm::Loop*, unsigned> derived_loop_iterations;
	llvm::DenseMap<const llvm::BasicBlock*, unsigned> bb_priority;

	std::set<llvm::Function*> f_unvisited_set;

	std::vector<llvm::Function*> call_stack;

};

}

#endif /* end of include guard: TAFFO_VALUE_RANGE_ANALYSIS_HPP */
