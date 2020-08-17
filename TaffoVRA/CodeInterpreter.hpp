#ifndef TAFFO_CODE_SCHEDULER_HPP
#define TAFFO_CODE_SCHEDULER_HPP

#include <memory>
#include "llvm/Support/Casting.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/LoopInfo.h"

#include <Metadata.h>
#include "PtrCasts.hpp"

namespace taffo {

class CodeInterpreter;
class CodeAnalyzer;

class CILogger {
public:
  virtual const char *getDebugType() const = 0;
  virtual void logBasicBlock(const llvm::BasicBlock *BB) const = 0;
  virtual void logStartFunction(const llvm::Function *F) = 0;
  virtual void logEndFunction(const llvm::Function *F) = 0;

  enum CILoggerKind { CILK_VRALogger };
  CILoggerKind getKind() const { return Kind; }

protected:
  CILogger(CILoggerKind K) : Kind(K) {}

private:
  CILoggerKind Kind;
};

class AnalysisStore {
public:
  virtual void convexMerge(const AnalysisStore &Other) = 0;
  virtual std::shared_ptr<CodeAnalyzer> newCodeAnalyzer(CodeInterpreter &CI) = 0;
  virtual std::shared_ptr<AnalysisStore> newFunctionStore(CodeInterpreter &CI) = 0;
  virtual bool hasValue(const llvm::Value *V) const = 0;
  virtual std::shared_ptr<CILogger> getLogger() const = 0;

  enum AnalysisStoreKind { ASK_VRAGlobalStore, ASK_VRAnalyzer, ASK_VRAFunctionStore };
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
  virtual void prepareForCall(llvm::Instruction *I,
                              std::shared_ptr<AnalysisStore> FunctionStore) = 0;
  virtual void prepareForOpenMPCall(llvm::Instruction *I,
                              std::shared_ptr<AnalysisStore> FunctionStore) = 0;
  virtual void returnFromCall(llvm::Instruction *I,
                              std::shared_ptr<AnalysisStore> FunctionStore) = 0;

  static bool classof(const AnalysisStore *AS) {
    return AS->getKind() >= ASK_VRAGlobalStore
      && AS->getKind() <= ASK_VRAnalyzer;
  }

protected:
  CodeAnalyzer(AnalysisStoreKind K) : AnalysisStore(K) {}
};

struct FunctionScope {
  FunctionScope(std::shared_ptr<AnalysisStore> FS)
    : FunctionStore(FS), BBAnalyzers(), EvalCount() {}

  std::shared_ptr<AnalysisStore> FunctionStore;
  llvm::DenseMap<llvm::BasicBlock *, std::shared_ptr<CodeAnalyzer>> BBAnalyzers;
  llvm::DenseMap<llvm::BasicBlock *, unsigned> EvalCount;
};

class CodeInterpreter {
public:
  CodeInterpreter(llvm::Pass &P, std::shared_ptr<AnalysisStore> GlobalStore,
                  unsigned LoopUnrollCount = 1U)
    : GlobalStore(GlobalStore), Scopes(),
      Pass(P), LoopInfo(nullptr), LoopTripCount(), RecursionCount(),
      DefaultTripCount(LoopUnrollCount) {}

  void interpretFunction(llvm::Function *F,
                         std::shared_ptr<AnalysisStore> FunctionStore = nullptr);
  std::shared_ptr<AnalysisStore> getStoreForValue(const llvm::Value *V) const;

  std::shared_ptr<AnalysisStore> getGlobalStore() const {
    return GlobalStore;
  }
  std::shared_ptr<AnalysisStore> getFunctionStore() const {
    return Scopes.back().FunctionStore;
  }

  llvm::Pass& getPass() const {
    return Pass;
  }

  static void getAnalysisUsage(llvm::AnalysisUsage &AU);

protected:
  std::shared_ptr<AnalysisStore> GlobalStore;
  llvm::SmallVector<FunctionScope, 4U> Scopes;
  llvm::Pass &Pass;
  llvm::LoopInfo *LoopInfo;
  llvm::DenseMap<llvm::BasicBlock *, unsigned> LoopTripCount;
  llvm::DenseMap<llvm::Function *, unsigned> RecursionCount;
  unsigned DefaultTripCount;

private:
  bool isLoopBackEdge(llvm::BasicBlock *Src, llvm::BasicBlock *Dst) const;
  llvm::Loop *getLoopForBackEdge(llvm::BasicBlock *Src, llvm::BasicBlock *Dst) const;
  bool followEdge(llvm::BasicBlock *Src, llvm::BasicBlock *Dst);
  void updateSuccessorAnalyzer(std::shared_ptr<CodeAnalyzer> CurrentAnalyzer,
                               std::shared_ptr<CodeAnalyzer> PathLocal,
                               llvm::Instruction *TermInstr, unsigned SuccIdx);
  void interpretCall(std::shared_ptr<CodeAnalyzer> CurAnalyzer,
		     llvm::Instruction *I);
  void interpretOpenMPCall(std::shared_ptr<CodeAnalyzer> CurAnalyzer,
    llvm::Instruction *I);
  void updateLoopInfo(llvm::Function *F);
  void retrieveLoopTripCount(llvm::Function *F);
  bool updateRecursionCount(llvm::Function *F);
};

} // end namespace taffo

#endif
