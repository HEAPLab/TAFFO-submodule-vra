#ifndef TAFFO_CODE_SCHEDULER_HPP
#define TAFFO_CODE_SCHEDULER_HPP

#include <memory>
#include "TaffoUtils/MultiValueMap.h"

namespace taffo {

// TODO: add Loop stuff
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
  CodeInterpreter(std::unique_ptr<CodeAnalyzer> GlobalAnalyzer)
    : GlobalAnalyzer(GlobalAnalyzer), BBAnalyzers() {}

  void interpretFunction(llvm::Function *F);

protected:
  std::unique_ptr<CodeAnalyzer> GlobalAnalyzer;
  llvm::DenseMap<llvm::BasicBlock *, std::shared_ptr<CodeAnalyzer> > BBAnalyzers;
  llvm::SmallPtrSet<llvm::BasicBlock *> Visited;

private:
  bool wasVisited(llvm::BasicBlock *BB) const;
  bool hasUnvisitedPredecessors(llvm::BasicBlock *BB) const;
  void updateSuccessorAnalyzer(std::shared_ptr<CodeAnalyzer> CurrentAnalyzer,
                               std::shared_ptr<CodeAnalyzer> PathLocal,
                               llvm::Instruction *TermInstr, unsigned SuccIdx);
};

} // end namespace taffo

#endif
