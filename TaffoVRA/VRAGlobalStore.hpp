#ifndef TAFFO_VRA_GLOBAL_STORE_HPP
#define TAFFO_VRA_GLOBAL_STORE_HPP

#include "llvm/IR/Module.h"
#include "llvm/ADT/DenseMap.h"

#include "VRAStore.hpp"
#include "CodeInterpreter.hpp"
#include "VRALogger.hpp"

namespace taffo {

class VRAGlobalStore : protected VRAStore, public AnalysisStore {
public:
  VRAGlobalStore()
    : VRAStore(VRASK_VRAGlobalStore, std::make_shared<VRALogger>()),
      AnalysisStore(ASK_VRAGlobalStore) {}

  void convexMerge(const AnalysisStore &Other) override;
  std::shared_ptr<CodeAnalyzer> newCodeAnalyzer(CodeInterpreter &CI) override;
  std::shared_ptr<AnalysisStore> newFunctionStore(CodeInterpreter &CI) override;
  bool hasValue(const llvm::Value *V) const override { return DerivedRanges.count(V); }
  std::shared_ptr<CILogger> getLogger() const override { return Logger; }

  // Metadata Processing
  void harvestMetadata(llvm::Module &M);
  generic_range_ptr_t harvestStructMD(mdutils::MDInfo *MD);
  void saveResults(llvm::Module &M);
  bool isValidRange(mdutils::Range *rng) const;
  void refreshRange(const llvm::Instruction* i);
  static std::shared_ptr<mdutils::MDInfo> toMDInfo(const generic_range_ptr_t &r);
  static void updateMDInfo(std::shared_ptr<mdutils::MDInfo> mdi,
                           const generic_range_ptr_t &r);
  static void setConstRangeMetadata(mdutils::MetadataManager &MDManager,
                                    llvm::Instruction &i);

  const generic_range_ptr_t fetchInfo(const llvm::Value* v) override;
  range_node_ptr_t getOrCreateNode(const llvm::Value* v) override;
  void setNode(const llvm::Value* V, range_node_ptr_t Node) override {
    VRAStore::setNode(V, Node);
  }
  generic_range_ptr_t getUserInput(const llvm::Value *V) const;

  static bool classof(const AnalysisStore *AS) {
    return AS->getKind() == ASK_VRAGlobalStore;
  }

  static bool classof(const VRAStore *VS) {
    return VS->getKind() == VRASK_VRAGlobalStore;
  }

protected:
  llvm::DenseMap<const llvm::Value*, generic_range_ptr_t> UserInput;
  llvm::DenseMap<const llvm::Function*, generic_range_ptr_t> ReturnValues;
};

} // end namespace taffo

#endif
