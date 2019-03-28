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
#include "RangeNode.hpp"

#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include <list>
#include <InputInfo.h>

#define DEBUG_TYPE "taffo-vra"
#define DEBUG_VRA "ValueRangeAnalysis"
#define DEBUG_HEAD "[TAFFO][VRA]"

namespace taffo {

llvm::cl::opt<bool> PropagateAll("propagate-all",
				 llvm::cl::desc("Propagate ranges for all functions, "
						"not only those marked as starting point."),
				 llvm::cl::init(false));

struct ValueRangeAnalysis : public llvm::ModulePass {
	static char ID;

public:
	ValueRangeAnalysis(): ModulePass(ID) { }

	bool runOnModule(llvm::Module &M) override;

	void getAnalysisUsage(llvm::AnalysisUsage &) const override;

	// methods
private:
	void harvestMetadata(llvm::Module &M);

	generic_range_ptr_t harvestStructMD(mdutils::MDInfo *MD);

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

	void handleMemCpyIntrinsics(const llvm::Instruction* memcpy);

	inline void handleReturn(const llvm::Instruction* ret);

	void saveResults(llvm::Module &M);

	inline void handleStoreInstr(const llvm::Instruction* store);

	inline generic_range_ptr_t handleLoadInstr(llvm::Instruction* load);

	inline generic_range_ptr_t handleGEPInstr(const llvm::Instruction* gep);

	inline range_ptr_t handleCmpInstr(const llvm::Instruction* cmp);

	inline generic_range_ptr_t handlePhiNode(const llvm::Instruction* phi);

	inline generic_range_ptr_t find_ret_val(const llvm::Function* f);

	inline unsigned find_recursion_count(const llvm::Function* f);

protected:
	const generic_range_ptr_t fetchInfo(const llvm::Value* v);

	void saveValueInfo(const llvm::Value* v, const generic_range_ptr_t& info);

	range_node_ptr_t getNode(const llvm::Value* v) const;
	range_node_ptr_t getOrCreateNode(const llvm::Value* v);

	generic_range_ptr_t fetchRange(const range_node_ptr_t node,
				       std::list<std::vector<unsigned>>& offset) const;

	void setRange(range_node_ptr_t node, const generic_range_ptr_t& info,
		      std::list<std::vector<unsigned>>& offset);

	bool isDescendant(const llvm::Value* parent, const llvm::Value* desc) const;

	static inline range_ptr_t fetchConstant(const llvm::Constant* v);

	static std::shared_ptr<mdutils::MDInfo> toMDInfo(const generic_range_ptr_t &r);
	static void updateMDInfo(std::shared_ptr<mdutils::MDInfo> mdi,
				 const generic_range_ptr_t &r);

	static void emitError(const std::string& message);
	static std::string to_string(const generic_range_ptr_t& range);
        static void logInstruction(const llvm::Value* v);
        static void logRangeln(const generic_range_ptr_t& range);
        static void logInfo(const llvm::StringRef info);
	static void logInfoln(const llvm::StringRef info);
        static void logError(const llvm::StringRef error);

	// data structures
private:
	const unsigned bb_base_priority = 1;
	unsigned default_loop_iteration_count = 1;
	const unsigned default_function_recursion_count = 0;

	llvm::DenseMap<const llvm::Value*, generic_range_ptr_t> user_input;
	llvm::DenseMap<const llvm::Value*, range_node_ptr_t> derived_ranges;
	llvm::DenseMap<const llvm::Function*, std::list<generic_range_ptr_t> > fun_arg_input;
	llvm::DenseMap<const llvm::Function*, std::list<range_node_ptr_t> > fun_arg_derived;
	llvm::DenseMap<const llvm::Function*, unsigned> fun_rec_count;
	llvm::DenseMap<const llvm::Loop*, unsigned> user_loop_iterations;
	llvm::DenseMap<const llvm::Loop*, unsigned> derived_loop_iterations;
	llvm::DenseMap<const llvm::BasicBlock*, unsigned> bb_priority;

	std::set<llvm::Function*> f_unvisited_set;
	std::unordered_map<std::string, llvm::Function*> known_functions;

	std::vector<llvm::Function*> call_stack;
	llvm::DenseMap<const llvm::Function*, generic_range_ptr_t> return_values;
	// TODO return_values needs to be duplicated into partial and final to properly handle recursion

};

}

#endif /* end of include guard: TAFFO_VALUE_RANGE_ANALYSIS_HPP */
