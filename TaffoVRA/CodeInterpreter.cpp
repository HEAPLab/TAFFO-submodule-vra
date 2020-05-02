#include "CodeInterpreter.hpp"

#include <deque>
#include "llvm/Support/Debug.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/InstrTypes.h"
#include <Metadata.h>

// TODO remove from here
#define DEBUG_TYPE "taffo-vra"

namespace taffo {

void CodeInterpreter::interpretFunction(llvm::Function *F) {
  updateLoopInfo(F);
  retrieveLoopTripCount(F);

  llvm::BasicBlock *EntryBlock = &F->getEntryBlock();
  std::deque<llvm::BasicBlock *> Worklist;
  Worklist.push_back(EntryBlock);
  EvalCount[EntryBlock] = 1U;
  BBAnalyzers[EntryBlock] = GlobalStore->newCodeAnalyzer(*this);

  while (!Worklist.empty()) {
    llvm::BasicBlock *BB = Worklist.front();
    Worklist.pop_front();

    auto CAIt = BBAnalyzers.find(BB);
    assert(CAIt != BBAnalyzers.end());
    std::shared_ptr<CodeAnalyzer> CurAnalyzer = CAIt->second;
    std::shared_ptr<CodeAnalyzer> PathLocal = CurAnalyzer->clone();

    LLVM_DEBUG(llvm::dbgs() << "[TAFFO][VRA] " << BB->getName() << "\n");
    for (llvm::Instruction &I : *BB) {
      if (CurAnalyzer->requiresInterpretation(&I))
	interpretCall(CurAnalyzer, &I);
      else
	CurAnalyzer->analyzeInstruction(&I);
    }

    assert(EvalCount[BB] > 0 && "Trying to evaluated block with 0 EvalCount.");
    --EvalCount[BB];

    llvm::Instruction *Term = BB->getTerminator();
    for (unsigned NS = 0; NS < Term->getNumSuccessors(); ++NS) {
      llvm::BasicBlock *Succ = Term->getSuccessor(NS);
      if (followEdge(BB, Succ)) {
	Worklist.push_front(Succ);
	updateSuccessorAnalyzer(CurAnalyzer, PathLocal, Term, NS);
      }
    }

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

bool CodeInterpreter::isLoopBackEdge(llvm::BasicBlock *Src, llvm::BasicBlock *Dst) const {
  assert(LoopInfo);
  return LoopInfo->isLoopHeader(Dst) && getLoopForBackEdge(Src, Dst);
}

llvm::Loop *CodeInterpreter::getLoopForBackEdge(llvm::BasicBlock *Src, llvm::BasicBlock *Dst) const {
  assert(LoopInfo);
  llvm::Loop *L = LoopInfo->getLoopFor(Dst);
  while (L && !L->contains(Src))
    L = L->getParentLoop();

  return L;
}

bool CodeInterpreter::followEdge(llvm::BasicBlock *Src, llvm::BasicBlock *Dst) {
  // Don't follow edge if Dst has unvisited predecessors.
  unsigned SrcEC = EvalCount[Src];
  for (llvm::BasicBlock *Pred : predecessors(Dst)) {
    auto PredECIt = EvalCount.find(Pred);
    if ((PredECIt == EvalCount.end() || PredECIt->second > SrcEC)
        && !isLoopBackEdge(Pred, Dst))
      return false;
  }

  assert(LoopInfo);
  llvm::Loop *DstLoop = LoopInfo->getLoopFor(Dst);
  if (DstLoop && !DstLoop->contains(Src)) {
    // Entering new loop.
    assert(DstLoop->getHeader() == Dst && "Dst must be Loop header.");
    unsigned TripCount = 1U;
    if (llvm::BasicBlock *Latch = DstLoop->getLoopLatch()) {
      TripCount = LoopTripCount[Latch];
    }
    for (llvm::BasicBlock *LBB : DstLoop->blocks()) {
      EvalCount[LBB] = TripCount;
    }
    if (DstLoop->isLoopExiting(Dst)) {
      ++EvalCount[Dst];
    }
    return true;
  }
  llvm::Loop *SrcLoop = LoopInfo->getLoopFor(Src);
  if (SrcLoop) {
    if (SrcEC == 0U && SrcLoop->isLoopExiting(Src)) {
      // We are in the last evaluation of this loop.
      if (SrcLoop->contains(Dst)) {
        // We follow an internal edge only if it still has to be evaluated this time.
        return EvalCount[Dst] > 0;
      }
      // We can follow the exiting edges.
      EvalCount[Dst] = 1U;
      return true;
    }
    // The loop has to be evaluated more times: we do not follow the exiting edges.
    return SrcLoop->contains(Dst);
  }
  if (!SrcLoop && !DstLoop) {
    // There's no loop, just evaluate Dst once.
    EvalCount[Dst] = 1U;
  }
  return true;
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

void CodeInterpreter::retrieveLoopTripCount(llvm::Function *F) {
  assert(LoopInfo && F);
  llvm::ScalarEvolution *SE = nullptr;
  for (llvm::Loop *L : LoopInfo->getLoopsInPreorder()) {
    if (llvm::BasicBlock *Latch = L->getLoopLatch()) {
      unsigned TripCount = 0U;
      // Get user supplied unroll count
      llvm::Optional<unsigned> OUC = mdutils::MetadataManager::retrieveLoopUnrollCount(*L, LoopInfo);
      if (OUC.hasValue()) {
	TripCount = OUC.getValue();
      } else {
	// Compute loop trip count
	if (!SE)
	  SE = &Pass.getAnalysis<llvm::ScalarEvolutionWrapperPass>(*F).getSE();
	TripCount = SE->getSmallConstantTripCount(L);
      }
      LoopTripCount[Latch] = (TripCount > 0U) ? TripCount : 1U;
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
