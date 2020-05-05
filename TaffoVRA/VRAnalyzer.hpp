#ifndef TAFFO_VRANALIZER_HPP
#define TAFFO_VRANALIZER_HPP

#include "VRAStore.hpp"
#include "VRAGlobalStore.hpp"
#include "CodeInterpreter.hpp"
#include "VRALogger.hpp"

namespace taffo {

class VRAnalyzer : protected VRAStore, public CodeAnalyzer {
public:

  VRAnalyzer(CodeInterpreter &CI)
    : VRAStore(VRASK_VRAnalyzer,
               std::static_ptr_cast<VRALogger>(CI.getGlobalStore()->getLogger())),
      CodeAnalyzer(ASK_VRAnalyzer),
      CodeInt(CI) {}

  void convexMerge(const AnalysisStore &Other) override;
  std::shared_ptr<CodeAnalyzer> newCodeAnalyzer(CodeInterpreter &CI) override;
  std::shared_ptr<CILogger> getLogger() const override { return Logger; }
  std::shared_ptr<CodeAnalyzer> clone() override;
  void analyzeInstruction(llvm::Instruction *I) override;
  void setPathLocalInfo(std::shared_ptr<CodeAnalyzer> SuccAnalyzer,
                        llvm::Instruction *TermInstr, unsigned SuccIdx) override;
  bool requiresInterpretation(llvm::Instruction *I) const override;
  void prepareForCall(llvm::Instruction *I) override;
  void returnFromCall(llvm::Instruction *I) override;

  static bool classof(const AnalysisStore *AS) {
    return AS->getKind() == ASK_VRAnalyzer;
  }

  static bool classof(const VRAStore *VS) {
    return VS->getKind() == VRASK_VRAnalyzer;
  }

private:
  // Instruction Handlers
  void handleSpecialCall(const llvm::Instruction* I);
  void handleMemCpyIntrinsics(const llvm::Instruction* memcpy);
  void handleReturn(const llvm::Instruction* ret);

  void handleStoreInstr(const llvm::Instruction* store);
  generic_range_ptr_t handleLoadInstr(llvm::Instruction* load);
  generic_range_ptr_t handleGEPInstr(const llvm::Instruction* gep);
  bool isDescendant(const llvm::Value* parent, const llvm::Value* desc) const;

  range_ptr_t handleCmpInstr(const llvm::Instruction* cmp);
  generic_range_ptr_t handlePhiNode(const llvm::Instruction* phi);
  generic_range_ptr_t handleSelect(const llvm::Instruction* i);

  // Data handling
  const generic_range_ptr_t fetchInfo(const llvm::Value* v, bool derived_or_final = false) override;
  range_node_ptr_t getNode(const llvm::Value* v) const override;
  range_node_ptr_t getOrCreateNode(const llvm::Value* v) override;
  void setNode(const llvm::Value* V, range_node_ptr_t Node);

  // Interface with CodeInterpreter
  std::shared_ptr<VRAGlobalStore> getGlobalStore() const {
    return std::static_ptr_cast<VRAGlobalStore>(CodeInt.getGlobalStore());
  }

  std::shared_ptr<VRAStore> getAnalysisStoreForValue(const llvm::Value *V) const {
    std::shared_ptr<AnalysisStore> AStore = CodeInt.getAnalyzerForValue(V);
    if (!AStore) {
      return nullptr;
    }

    if (std::shared_ptr<VRAnalyzer> VRA =
        std::dynamic_ptr_cast<VRAnalyzer>(AStore)) {
      return std::static_ptr_cast<VRAStore>(VRA);
    } else if (std::shared_ptr<VRAGlobalStore> VRAGS =
        std::dynamic_ptr_cast<VRAGlobalStore>(AStore)) {
      return std::static_ptr_cast<VRAStore>(VRAGS);
    }
    return nullptr;
  }

  // Logging
  void logRangeln(const llvm::Value* v);

  CodeInterpreter &CodeInt;
};

} // end namespace taffo

#endif
