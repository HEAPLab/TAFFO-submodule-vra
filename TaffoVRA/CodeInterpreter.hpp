#ifndef TAFFO_CODE_SCHEDULER_HPP
#define TAFFO_CODE_SCHEDULER_HPP

#include <memory>
#include "llvm/ADT/DenseMap.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/LoopInfo.h"

namespace taffo {

// TODO: add function call stuff

class CodeAnalyzer {
public:
  virtual void convexMerge(const CodeAnalyzer &Other) = 0;
  virtual std::shared_ptr<CodeAnalyzer> newCodeAnalyzer() = 0;
  virtual std::shared_ptr<CodeAnalyzer> clone() = 0;
  virtual void analyzeInstruction(llvm::Instruction *I) = 0;
  virtual void setPathLocalInfo(std::shared_ptr<CodeAnalyzer> SuccAnalyzer,
                                llvm::Instruction *TermInstr, unsigned SuccIdx) = 0;
  virtual ~CodeAnalyzer() = default;

  bool isFinal() const { return Final; }
  void setFinal() { Final = true; }

protected:
  CodeAnalyzer() : Final(false) {}

private:
  bool Final;
};



class CodeInterpreter {
  CodeInterpreter(llvm::Pass &P, std::unique_ptr<CodeAnalyzer> &&GlobalAnalyzer)
    : GlobalAnalyzer(std::move(GlobalAnalyzer)), BBAnalyzers(), Pass(P), LoopInfo(nullptr) {}

  void interpretFunction(llvm::Function *F);

  static void getAnalysisUsage(llvm::AnalysisUsage &AU);

protected:
  std::unique_ptr<CodeAnalyzer> GlobalAnalyzer;
  llvm::DenseMap<llvm::BasicBlock *, std::shared_ptr<CodeAnalyzer> > BBAnalyzers;
  llvm::Pass &Pass;
  llvm::LoopInfo *LoopInfo;
  llvm::DenseMap<llvm::BasicBlock *, unsigned> LoopIterCount;

private:
  bool wasVisited(llvm::BasicBlock *BB) const;
  bool hasUnvisitedPredecessors(llvm::BasicBlock *BB) const;
  bool isLoopBackEdge(llvm::BasicBlock *Src, llvm::BasicBlock *Dst) const;
  llvm::Loop *getLoopForBackEdge(llvm::BasicBlock *Src, llvm::BasicBlock *Dst) const;
  bool followEdge(llvm::BasicBlock *Src, llvm::BasicBlock *Dst);
  void updateSuccessorAnalyzer(std::shared_ptr<CodeAnalyzer> CurrentAnalyzer,
                               std::shared_ptr<CodeAnalyzer> PathLocal,
                               llvm::Instruction *TermInstr, unsigned SuccIdx);
  void updateLoopInfo(llvm::Function *F);
  void retrieveLoopIterCount(llvm::Function *F);
};

} // end namespace taffo

#endif
