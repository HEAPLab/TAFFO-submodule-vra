#ifndef TAFFO_VALUE_RANGE_ANALYSIS_HPP
#define TAFFO_VALUE_RANGE_ANALYSIS_HPP

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/Debug.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/CommandLine.h"

#include "Range.hpp"

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
	void harvestMetadata(const llvm::Module &M);

	void processModule(llvm::Module &M);

	void saveResults();

	const Range<double> fetchInfo(const llvm::Value* v) const;

	void saveValueInfo(const llvm::Value* v, const Range<double>& info);

	// data structures
private:
	llvm::DenseMap<const llvm::Value*, Range<double> > user_input;
	llvm::DenseMap<const llvm::Value*, Range<double> > derived_ranges;
};

}

#endif /* end of include guard: TAFFO_VALUE_RANGE_ANALYSIS_HPP */
