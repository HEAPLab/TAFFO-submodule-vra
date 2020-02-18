#include "CodeScheduler.hpp"

namespace taffo {

void CodeInterpreter::interpretFunction(llvm::Function *F) {
  updateLoopInfo(F);
  retrieveLoopIterCount(F);

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
    for (unsigned NS = 0; NS < Term->getNumSuccessors(); ++NS) {
      llvm::BasicBlock *Succ = Term->getSuccessor(NS);
      if (followEdge(BB, Succ)) {
	Worklist.push_front(Succ);
	updateSuccessorAnalyzer(CurAnalyzer, PathLocal, Term, NS);
      }
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
    if (!wasVisited(Pred) && !isLoopBackEdge(Pred, BB))
      return true;
  }
  return false;
}

bool CodeInterpreter::isLoopBackEdge(llvm::BasicBlock *Src, llvm::BasicBlock *Dst) const {
  assert(LoopInfo);
  return LoopInfo->isLoopHeader(Dst) || getLoopForBackEdge(Src, Dst);
}

llvm::Loop *CodeInterpreter::getLoopForBackEdge(llvm::BasicBlock *Src, llvm::BasicBlock *Dst) const {
  assert(LoopInfo);
  llvm::Loop *L = LoopInfo->getLoopFor(Dst);
  while (L && !L->contains(Src))
    L = L->getParentLoop();

  return L;
}

bool CodeInterpreter::followEdge(llvm::BasicBlock *Src, llvm::BasicBlock *Dst) const {
  if (!wasVisited(Dst))
    return true;

  assert(LoopInfo);
  llvm::Loop *L = getLoopForBackEdge(Src, Dst);
  if (!L || L->getLoopLatch() != Src)
    return false; // We only support simple loops, for now.

  auto LICIt = LoopIterCount.find(Src);
  if (LICIt == LoopIterCount.end())
    return false;

  unsigned &Remaining = LICIt->second;
  if (Remaining > 0) {
    --Remaining;
    return true;
  }
  else
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

void CodeInterpreter::retrieveLoopIterCount(llvm::Function *F) {
  assert(LoopInfo);
  ScalarEvolution *SE = nullptr;
  for (Loop *L : LoopInfo) {
    if (llvm::BasicBlock *Latch = L->getLoopLatch()) {
      unsigned IterCount = 0;
      // Get user supplied unroll count
      Optional<unsigned> OUC = mdutils::MetadataManager::retrieveLoopUnrollCount(*L, &LoopInfo);
      if (OUC.hasValue()) {
	IterCount = OUC.getValue();
      }
      else {
	// Compute loop trip count
	if (!SE)
	  SE = &Pass.getAnalysis<ScalarEvolutionWrapperPass>(F).getSE();
	IterCount = SE->getSmallConstantTripCount(L);
      }
      if (IterCount > 0)
	LoopIterCount[Latch] = IterCount;
    }
  }
}

static void CodeInterpreter::getAnalysisUsage(llvm::AnalysisUsage &AU) {
  AU.addRequiredTransitive<LoopInfoWrapperPass>();
  AU.addRequiredTransitive<ScalarEvolutionWrapperPass>();
}


} // end namespace taffo
