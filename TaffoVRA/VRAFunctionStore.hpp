#ifndef TAFFO_VRA_FUNCTION_STORE_HPP
#define TAFFO_VRA_FUNCTION_STORE_HPP

#include "VRAStore.hpp"
#include "CodeInterpreter.hpp"
#include "VRALogger.hpp"

namespace taffo {

class VRAFunctionStore : protected VRAStore, public AnalysisStore {
public:
  VRAFunctionStore(CodeInterpreter &CI)
    : VRAStore(VRASK_VRAFunctionStore,
               std::static_ptr_cast<VRALogger>(CI.getGlobalStore()->getLogger())),
      AnalysisStore(ASK_VRAFunctionStore),
      ReturnValue() {}

  void convexMerge(const AnalysisStore &Other) override;
  std::shared_ptr<CodeAnalyzer> newCodeAnalyzer(CodeInterpreter &CI) override;
  std::shared_ptr<AnalysisStore> newFunctionStore(CodeInterpreter &CI) override;
  bool hasValue(const llvm::Value *V) const override { return DerivedRanges.count(V); }
  std::shared_ptr<CILogger> getLogger() const override { return Logger; }

  void setNode(const llvm::Value* V, range_node_ptr_t Node) override {
    VRAStore::setNode(V, Node);
  }

  // Function handling stuff
  generic_range_ptr_t getRetVal() const { return ReturnValue; }
  void setRetVal(generic_range_ptr_t RetVal);
  void setArgumentRanges(const llvm::Function &F,
                         const std::list<range_node_ptr_t> &AARanges);

  static bool classof(const AnalysisStore *AS) {
    return AS->getKind() == ASK_VRAFunctionStore;
  }

  static bool classof(const VRAStore *VS) {
    return VS->getKind() == VRASK_VRAFunctionStore;
  }

protected:
  generic_range_ptr_t ReturnValue;
};

} // end namespace taffo

#endif
