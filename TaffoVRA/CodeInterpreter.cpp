#include "CodeInterpreter.hpp"

#include <deque>
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/InstrTypes.h"
#include <Metadata.h>

namespace taffo {

void CodeInterpreter::interpretFunction(llvm::Function *F) {
  updateLoopInfo(F);
  retrieveLoopIterCount(F);

  std::deque<llvm::BasicBlock *> Worklist;
  Worklist.push_back(&F->getEntryBlock());
  BBAnalyzers[&F->getEntryBlock()] = GlobalStore->newCodeAnalyzer(*this);

  while (!Worklist.empty()) {
    llvm::BasicBlock *BB = Worklist.front();
    Worklist.pop_front();

    if (hasUnvisitedPredecessors(BB)) {
      continue;
    }

    auto CAIt = BBAnalyzers.find(BB);
    assert(CAIt != BBAnalyzers.end());
    std::shared_ptr<CodeAnalyzer> CurAnalyzer = CAIt->second;
    std::shared_ptr<CodeAnalyzer> PathLocal = CurAnalyzer->clone();

    for (llvm::Instruction &I : *BB) {
      if (CurAnalyzer->requiresInterpretation(&I))
	interpretCall(CurAnalyzer, &I);
      else
	CurAnalyzer->analyzeInstruction(&I);
    }

    llvm::Instruction *Term = BB->getTerminator();
    for (unsigned NS = 0; NS < Term->getNumSuccessors(); ++NS) {
      llvm::BasicBlock *Succ = Term->getSuccessor(NS);
      if (followEdge(BB, Succ)) {
	Worklist.push_front(Succ);
	updateSuccessorAnalyzer(CurAnalyzer, PathLocal, Term, NS);
      }
    }

    CurAnalyzer->setFinal();
    GlobalStore->convexMerge(*CurAnalyzer);
  }
}

std::shared_ptr<AnalysisStore> CodeInterpreter::getAnalyzerForValue(const llvm::Value *V) const {
  // TODO add assert(v) here and see what happens
  if (!V) return nullptr;

  if (llvm::isa<llvm::GlobalValue>(V)
      || llvm::isa<llvm::Argument>(V)
      || llvm::isa<llvm::Function>(V))
    return GlobalStore;

  if (const llvm::Instruction *I = llvm::dyn_cast<llvm::Instruction>(V)) {
    auto BBAIt = BBAnalyzers.find(I->getParent());
    if (BBAIt != BBAnalyzers.end())
      return BBAIt->second;
  }
  return nullptr;
}

bool CodeInterpreter::wasVisited(llvm::BasicBlock *BB) const {
  auto BBAIt = BBAnalyzers.find(BB);
  if (BBAIt == BBAnalyzers.end())
    return false;

  return BBAIt->second->isFinal();
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

bool CodeInterpreter::followEdge(llvm::BasicBlock *Src, llvm::BasicBlock *Dst) {
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
  llvm::BasicBlock *SuccBB = TermInstr->getSuccessor(SuccIdx);

  std::shared_ptr<CodeAnalyzer> SuccAnalyzer;
  auto SAIt = BBAnalyzers.find(SuccBB);
  if (SAIt == BBAnalyzers.end()) {
    SuccAnalyzer = (SuccIdx < TermInstr->getNumSuccessors()) ? PathLocal->clone() : PathLocal;
    BBAnalyzers[SuccBB] = SuccAnalyzer;
  }
  else {
    SuccAnalyzer = SAIt->second;
    SuccAnalyzer->convexMerge(*PathLocal);
  }

  CurrentAnalyzer->setPathLocalInfo(SuccAnalyzer, TermInstr, SuccIdx);
}

void CodeInterpreter::interpretCall(std::shared_ptr<CodeAnalyzer> CurAnalyzer,
                                    llvm::Instruction *I) {
  llvm::CallBase *CB = llvm::cast<llvm::CallBase>(I);
  llvm::Function *F = CB->getCalledFunction();
  if (!F || F->empty())
    return;

  CurAnalyzer->prepareForCall(I);

  if (updateRecursionCount(F))
    interpretFunction(F);

  CurAnalyzer->returnFromCall(I);

  updateLoopInfo(I->getFunction());
}

void CodeInterpreter::updateLoopInfo(llvm::Function *F) {
  assert(F);
  LoopInfo = &Pass.getAnalysis<llvm::LoopInfoWrapperPass>(*F).getLoopInfo();
}

void CodeInterpreter::retrieveLoopIterCount(llvm::Function *F) {
  assert(LoopInfo && F);
  llvm::ScalarEvolution *SE = nullptr;
  for (llvm::Loop *L : *LoopInfo) {
    if (llvm::BasicBlock *Latch = L->getLoopLatch()) {
      unsigned IterCount = 0;
      // Get user supplied unroll count
      llvm::Optional<unsigned> OUC = mdutils::MetadataManager::retrieveLoopUnrollCount(*L, LoopInfo);
      if (OUC.hasValue()) {
	IterCount = OUC.getValue();
      }
      else {
	// Compute loop trip count
	if (!SE)
	  SE = &Pass.getAnalysis<llvm::ScalarEvolutionWrapperPass>(*F).getSE();
	IterCount = SE->getSmallConstantTripCount(L);
      }
      if (IterCount > 0)
	LoopIterCount[Latch] = IterCount;
    }
  }
}

bool CodeInterpreter::updateRecursionCount(llvm::Function *F) {
  auto RCIt = RecursionCount.find(F);
  if (RCIt == RecursionCount.end()) {
    unsigned FromMD = mdutils::MetadataManager::retrieveMaxRecursionCount(*F);
    if (FromMD > 0)
      --FromMD;

    RecursionCount[F] = FromMD;
    return true;
  }
  unsigned &Remaining = RCIt->second;
  if (Remaining > 0) {
    --Remaining;
    return true;
  }
  return false;
}

void CodeInterpreter::getAnalysisUsage(llvm::AnalysisUsage &AU) {
  AU.addRequiredTransitive<llvm::LoopInfoWrapperPass>();
  AU.addRequiredTransitive<llvm::ScalarEvolutionWrapperPass>();
}


} // end namespace taffo
