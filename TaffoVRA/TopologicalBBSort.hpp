#ifndef TAFFO_TOPOLOGICALBBSORT_HPP
#define TAFFO_TOPOLOGICALBBSORT_HPP

#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/BasicBlock.h"

namespace taffo {

#define DEFAULT_SIZE 16U

class TopologicalBBQueue {
public:
  TopologicalBBQueue() : BBSet(), SortedQueue() {}
  void insert(llvm::BasicBlock* BB) {
    BBSet.insert(BB);
    SortedQueue.clear();
  }
  llvm::BasicBlock* popFirst();
  bool empty() const { return BBSet.empty(); }

private:
  llvm::SmallPtrSet<llvm::BasicBlock*, DEFAULT_SIZE> BBSet;
  llvm::SmallVector<llvm::BasicBlock*, DEFAULT_SIZE> SortedQueue;

  bool isUpToDate() const { return BBSet.size() == SortedQueue.size(); }
  void sortQueue();
  void visitBasicBlock(llvm::BasicBlock* BB,
		       llvm::SmallPtrSetImpl<llvm::BasicBlock*>& VisitedSet);
};

llvm::BasicBlock* TopologicalBBQueue::popFirst() {
  if (BBSet.empty())
    return nullptr;

  if (!isUpToDate())
    sortQueue();

  llvm::BasicBlock* Ret = SortedQueue.back();
  SortedQueue.pop_back();
  BBSet.erase(Ret);
  return Ret;
}

void TopologicalBBQueue::sortQueue() {
  using namespace llvm;
  if (BBSet.empty())
    return;

  SortedQueue.clear();
  SmallPtrSet<BasicBlock*, DEFAULT_SIZE> VisitedSet;
  for (BasicBlock *BB : BBSet) {
    if (!VisitedSet.count(BB))
      visitBasicBlock(BB, VisitedSet);
  }
}

void TopologicalBBQueue::
visitBasicBlock(llvm::BasicBlock* BB,
		llvm::SmallPtrSetImpl<llvm::BasicBlock*>& VisitedSet) {
  using namespace llvm;
  VisitedSet.insert(BB);
  for (BasicBlock* Succ : successors(BB)) {
    if (!VisitedSet.count(Succ))
      visitBasicBlock(Succ, VisitedSet);
  }
  if (BBSet.count(BB))
    SortedQueue.push_back(BB);
}

} // end namespace taffo

#endif
