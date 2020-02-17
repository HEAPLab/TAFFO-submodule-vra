#include "CodeScheduler.hpp"

namespace taffo {

void CodeInterpreter::interpretFunction(llvm::Function *F) {
  std::deque<llvm::BasicBlock *> Worklist;
  Worklist.push_back(&F->getEntryBlock());
  BBAnalyzers[&F->getEntryBlock()] = GlobalAnalyzer->newCodeAnalyzer();

  while (!Worklist.empty()) {
    llvm::BasicBlock *BB = Worklist.front();
    Worklist.pop_front();

    if (hasUnvisitedPredecessors(BB)) {
      Worklist.push_back(BB);
      continue;
    }

    auto CAIt = BBnalyzers.find(BB);
    assert(CAIt != BBAnalyzers.end());
    std::shared_ptr<CodeAnalyzer> CurAnalyzer = *CAIt;
    std::shared_ptr<CodeAnalyzer> PathLocal = CurAnalyzer->clone();

    for (llvm::Instruction *I : *BB) {
      CurAnalyzer->analyzeInstruction(I);
    }

    llvm::Instruction Term = BB->getTerminator();
    for (unsigned NS = 0; NS < Term->getNumSuccessors(); ++ NS) {
      llvm::BasicBlock *Succ = Term->getSuccessor(NS);
      Worklist.push_front(Succ);
      updateSuccessorAnalyzer(CurAnalyzer, PathLocal, Term, NS);
    }

    CurAnalyzer->setFinal();
  }
}

bool CodeInterpreter::wasVisited(llvm::BasicBlock *BB) const {
  auto BBAIt = BBAnalyzers.find(BB);
  if (BBAIt == BBAnalyzers.end())
    return false;

  return (*BBAIt)->isFinal();
}

bool CodeInterpreter::hasUnvisitedPredecessors(llvm::BasicBlock *BB) const {
  for (llvm::BasicBlock *Pred : predecessors(BB)) {
    if (!wasVisited(Pred))
      return true;
  }
  return false;
}

void CodeInterpreter::updateSuccessorAnalyzer(std::shared_ptr<CodeAnalyzer> CurrentAnalyzer,
					      std::shared_ptr<CodeAnalyzer> PathLocal,
                                              llvm::Instruction *TermInstr,
                                              unsigned SuccIdx) {
  llvm::BasicBlock *SuccBB = TermInstr.getSuccessor(SuccIdx);

  std::shared_ptr<CodeAnalyzer> SuccAnalyzer;
  auto SAIt = BBAnalyzers.find(SuccBB);
  if (SAIt == BBAnalyzers.end()) {
    SuccAnalyzer = (SuccIdx < TermInstr->getNumSuccessors()) ? PathLocal.clone() : PathLocal;
    BBAnalyzers[SuccBB] = SuccAnalyzer;
  }
  else {
    SuccAnalyzer = *SAIt;
    SuccAnalyzer.convexMerge(PathLocal);
  }

  CurrentAnalyzer.setPathLocalInfo(SuccAnalyzer, TermInstr, SuccIdx);
}



} // end namespace taffo
