#ifndef TAFFO_CODE_SCHEDULER_HPP
#define TAFFO_CODE_SCHEDULER_HPP

#include <memory>
#include <cassert>
#include "llvm/Support/Casting.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/LoopInfo.h"

#include <Metadata.h>
#include "RangeNode.hpp"

namespace taffo {

class CodeInterpreter;
class CodeAnalyzer;

class AnalysisStore {
public:
  virtual void convexMerge(const AnalysisStore &Other) = 0;
  virtual std::shared_ptr<CodeAnalyzer> newCodeAnalyzer(CodeInterpreter &CI) = 0;

  enum AnalysisStoreKind { ASK_VRAGlobalStore, ASK_ValueRangeAnalyzer };
  AnalysisStoreKind getKind() const { return Kind; }

protected:
  AnalysisStore(AnalysisStoreKind K) : Kind(K) {}

private:
  const AnalysisStoreKind Kind;
};

class CodeAnalyzer : public AnalysisStore {
public:
  virtual std::shared_ptr<CodeAnalyzer> clone() = 0;
  virtual void analyzeInstruction(llvm::Instruction *I) = 0;
  virtual void setPathLocalInfo(std::shared_ptr<CodeAnalyzer> SuccAnalyzer,
                                llvm::Instruction *TermInstr, unsigned SuccIdx) = 0;
  virtual bool requiresInterpretation(llvm::Instruction *I) const = 0;
  virtual void prepareForCall(llvm::Instruction *I) = 0;
  virtual void returnFromCall(llvm::Instruction *I) = 0;

  static bool classof(const AnalysisStore *AS) {
    return AS->getKind() >= ASK_VRAGlobalStore
      && AS->getKind() <= ASK_ValueRangeAnalyzer;
  }

protected:
  CodeAnalyzer(AnalysisStoreKind K) : AnalysisStore(K) {}
};

class CodeInterpreter {
public:
  CodeInterpreter(llvm::Pass &P, std::shared_ptr<AnalysisStore> GlobalStore)
    : GlobalStore(GlobalStore), BBAnalyzers(), EvalCount(),
      Pass(P), LoopInfo(nullptr), LoopTripCount(), RecursionCount() {}

  void interpretFunction(llvm::Function *F);
  // TODO change to getAnalysisStoreForValue
  std::shared_ptr<AnalysisStore> getAnalyzerForValue(const llvm::Value *V) const;

  std::shared_ptr<AnalysisStore> getGlobalStore() const {
    return GlobalStore;
  }

  llvm::Pass& getPass() const {
    return Pass;
  }

  static void getAnalysisUsage(llvm::AnalysisUsage &AU);

protected:
  std::shared_ptr<AnalysisStore> GlobalStore;
  llvm::DenseMap<llvm::BasicBlock *, std::shared_ptr<CodeAnalyzer>> BBAnalyzers;
  llvm::DenseMap<llvm::BasicBlock *, unsigned> EvalCount;
  llvm::Pass &Pass;
  llvm::LoopInfo *LoopInfo;
  llvm::DenseMap<llvm::BasicBlock *, unsigned> LoopTripCount;
  llvm::DenseMap<llvm::Function *, unsigned> RecursionCount;

private:
  bool isLoopBackEdge(llvm::BasicBlock *Src, llvm::BasicBlock *Dst) const;
  llvm::Loop *getLoopForBackEdge(llvm::BasicBlock *Src, llvm::BasicBlock *Dst) const;
  bool followEdge(llvm::BasicBlock *Src, llvm::BasicBlock *Dst);
  void updateSuccessorAnalyzer(std::shared_ptr<CodeAnalyzer> CurrentAnalyzer,
                               std::shared_ptr<CodeAnalyzer> PathLocal,
                               llvm::Instruction *TermInstr, unsigned SuccIdx);
  void interpretCall(std::shared_ptr<CodeAnalyzer> CurAnalyzer,
		     llvm::Instruction *I);
  void updateLoopInfo(llvm::Function *F);
  void retrieveLoopTripCount(llvm::Function *F);
  bool updateRecursionCount(llvm::Function *F);
};

} // end namespace taffo

#endif
