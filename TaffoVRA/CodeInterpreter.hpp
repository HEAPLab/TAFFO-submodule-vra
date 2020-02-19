#ifndef TAFFO_CODE_SCHEDULER_HPP
#define TAFFO_CODE_SCHEDULER_HPP

#include <memory>
#include "llvm/Support/Casting.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/LoopInfo.h"

namespace taffo {

class CodeInterpreter;

class CodeAnalyzer {
public:
  virtual void convexMerge(const CodeAnalyzer &Other) = 0;
  virtual std::shared_ptr<CodeAnalyzer> newCodeAnalyzer(CodeInterpreter *CI) = 0;
  virtual std::shared_ptr<CodeAnalyzer> clone() = 0;
  virtual void analyzeInstruction(llvm::Instruction *I) = 0;
  virtual void setPathLocalInfo(std::shared_ptr<CodeAnalyzer> SuccAnalyzer,
                                llvm::Instruction *TermInstr, unsigned SuccIdx) = 0;
  virtual bool requiresInterpretation(llvm::Instruction *I) const;
  virtual void prepareForCall(llvm::Instruction *I, const CodeAnalyzer &GlobalAnalyzer);
  virtual void returnFromCall(llvm::Instruction *I, const CodeAnalyzer &GlobalAnalyzer);

  virtual ~CodeAnalyzer() = default;

  bool isFinal() const { return Final; }
  void setFinal() { Final = true; }

  enum CodeAnalyzerKind { CAK_VRA };
  CodeAnalyzerKind getKind() const { return Kind; }

protected:
  CodeAnalyzer(CodeAnalyzerKind K) : Final(false), Kind(K) {}

private:
  bool Final;
  const CodeAnalyzerKind Kind;
};

class CodeInterpreter {
  CodeInterpreter(llvm::Pass &P, std::shared_ptr<CodeAnalyzer> GlobalAnalyzer)
    : GlobalAnalyzer(GlobalAnalyzer), BBAnalyzers(), Pass(P), LoopInfo(nullptr) {}

  void interpretFunction(llvm::Function *F);
  std::shared_ptr<CodeAnalyzer> getAnalyzerForValue(const llvm::Value *V) const;

  static void getAnalysisUsage(llvm::AnalysisUsage &AU);

protected:
  std::shared_ptr<CodeAnalyzer> GlobalAnalyzer;
  llvm::DenseMap<llvm::BasicBlock *, std::shared_ptr<CodeAnalyzer> > BBAnalyzers;
  llvm::Pass &Pass;
  llvm::LoopInfo *LoopInfo;
  llvm::DenseMap<llvm::BasicBlock *, unsigned> LoopIterCount;
  llvm::DenseMap<llvm::Function *, unsigned> RecursionCount;

private:
  bool wasVisited(llvm::BasicBlock *BB) const;
  bool hasUnvisitedPredecessors(llvm::BasicBlock *BB) const;
  bool isLoopBackEdge(llvm::BasicBlock *Src, llvm::BasicBlock *Dst) const;
  llvm::Loop *getLoopForBackEdge(llvm::BasicBlock *Src, llvm::BasicBlock *Dst) const;
  bool followEdge(llvm::BasicBlock *Src, llvm::BasicBlock *Dst);
  void updateSuccessorAnalyzer(std::shared_ptr<CodeAnalyzer> CurrentAnalyzer,
                               std::shared_ptr<CodeAnalyzer> PathLocal,
                               llvm::Instruction *TermInstr, unsigned SuccIdx);
  void interpretCall(std::shared_ptr<CodeAnalyzer> CurAnalyzer,
		     llvm::Instruction *I);
  void updateLoopInfo(llvm::Function *F);
  void retrieveLoopIterCount(llvm::Function *F);
  bool updateRecursionCount(llvm::Function *F);
};

} // end namespace taffo

#endif
