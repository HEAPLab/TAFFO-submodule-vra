#ifndef TAFFO_VALUE_RANGE_ANALIZER_HPP
#define TAFFO_VALUE_RANGE_ANALIZER_HPP

#include "CodeInterpreter.hpp"

namespace taffo {

class ValueRangeAnalyzer : public CodeInterpreter {
public:
  void convexMerge(const CodeAnalyzer &Other);
  std::shared_ptr<CodeAnalyzer> newCodeAnalyzer(CodeInterpreter *CI);
  std::shared_ptr<CodeAnalyzer> clone();
  void analyzeInstruction(llvm::Instruction *I);
  void setPathLocalInfo(std::shared_ptr<CodeAnalyzer> SuccAnalyzer,
                        llvm::Instruction *TermInstr, unsigned SuccIdx);
  bool requiresInterpretation(llvm::Instruction *I) const;
  void prepareForCall(llvm::Instruction *I, const CodeAnalyzer &GlobalAnalyzer);
  void returnFromCall(llvm::Instruction *I, const CodeAnalyzer &GlobalAnalyzer);

  static bool classof(const CodeInterpreter *CI) {
    return CI->getKind() == CAK_VRA;
  }

private:
  void handleTerminators(const llvm::Instruction* term);

  void handleCallBase(const llvm::Instruction* call);

  void handleMemCpyIntrinsics(const llvm::Instruction* memcpy);

  void handleReturn(const llvm::Instruction* ret);

  void saveResults(llvm::Module &M);
  void refreshRange(const llvm::Instruction* i);

  void handleStoreInstr(const llvm::Instruction* store);

  generic_range_ptr_t handleLoadInstr(llvm::Instruction* load);

  generic_range_ptr_t handleGEPInstr(const llvm::Instruction* gep);

  range_ptr_t handleCmpInstr(const llvm::Instruction* cmp);

  generic_range_ptr_t handlePhiNode(const llvm::Instruction* phi);
  generic_range_ptr_t handleSelect(const llvm::Instruction* i);

  generic_range_ptr_t find_ret_val(const llvm::Function* f);

  unsigned find_recursion_count(const llvm::Function* f);

  const generic_range_ptr_t fetchInfo(const llvm::Value* v, bool derived_or_final = false);

  void saveValueInfo(const llvm::Value* v, const generic_range_ptr_t& info);

  range_node_ptr_t getNode(const llvm::Value* v) const;
  range_node_ptr_t getOrCreateNode(const llvm::Value* v);

  generic_range_ptr_t fetchRange(const range_node_ptr_t node,
                                 std::list<std::vector<unsigned>>& offset) const;

  void setRange(range_node_ptr_t node, const generic_range_ptr_t& info,
                std::list<std::vector<unsigned>>& offset);

  bool isDescendant(const llvm::Value* parent, const llvm::Value* desc) const;
  bool extractGEPOffset(const llvm::Type* source_element_type,
                        const llvm::iterator_range<llvm::User::const_op_iterator> indices,
                        std::vector<unsigned>& offset);


  generic_range_ptr_t fetchConstant(const llvm::Constant* v);

  static std::shared_ptr<mdutils::MDInfo> toMDInfo(const generic_range_ptr_t &r);
  static void updateMDInfo(std::shared_ptr<mdutils::MDInfo> mdi,
                           const generic_range_ptr_t &r);
  static void setConstRangeMetadata(mdutils::MetadataManager &MDManager,
                                    llvm::Instruction &i);

  static void emitError(const std::string& message);
  static std::string to_string(const generic_range_ptr_t& range);
  static void logInstruction(const llvm::Value* v);
  void logRangeln(const llvm::Value* v);
  static void logRangeln(const generic_range_ptr_t& range);
  static void logInfo(const llvm::StringRef info);
  static void logInfoln(const llvm::StringRef info);
  static void logError(const llvm::StringRef error);

  llvm::DenseMap<const llvm::Value*, generic_range_ptr_t> user_input;
  llvm::DenseMap<const llvm::Value*, range_node_ptr_t> derived_ranges;
  llvm::DenseMap<const llvm::Function*, std::list<generic_range_ptr_t> > fun_arg_input;
  llvm::DenseMap<const llvm::Function*, std::list<range_node_ptr_t> > fun_arg_derived;
};

} // end namespace taffo

#endif
