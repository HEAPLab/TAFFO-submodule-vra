#include "VRAnalyzer.hpp"

#include "llvm/IR/Intrinsics.h"
#include "llvm/Support/Debug.h"
#include "RangeOperations.hpp"
#include "TypeUtils.h"
#include "MemSSAUtils.hpp"

using namespace llvm;
using namespace taffo;

void
VRAnalyzer::convexMerge(const AnalysisStore &Other) {
  // Since llvm::dyn_cast<T>() does not do cross-casting, we must do this:
  if (llvm::isa<VRAnalyzer>(Other)) {
    VRAStore::convexMerge(llvm::cast<VRAStore>(llvm::cast<VRAnalyzer>(Other)));
  } else if (llvm::isa<VRAGlobalStore>(Other)) {
    VRAStore::convexMerge(llvm::cast<VRAStore>(llvm::cast<VRAGlobalStore>(Other)));
  } else {
    VRAStore::convexMerge(llvm::cast<VRAStore>(llvm::cast<VRAFunctionStore>(Other)));
  }
}

std::shared_ptr<CodeAnalyzer>
VRAnalyzer::newCodeAnalyzer(CodeInterpreter &CI) {
  return std::make_shared<VRAnalyzer>(CI);
}

std::shared_ptr<AnalysisStore>
VRAnalyzer::newFunctionStore(CodeInterpreter &CI) {
  return std::make_shared<VRAFunctionStore>(CI);
}

std::shared_ptr<CodeAnalyzer>
VRAnalyzer::clone() {
  return std::make_shared<VRAnalyzer>(*this);
}

void
VRAnalyzer::analyzeInstruction(llvm::Instruction *I) {
  assert(I);
  Instruction &i = *I;
  const unsigned OpCode = i.getOpcode();
  if (OpCode == Instruction::Call
      || OpCode == Instruction::Invoke) {
    handleSpecialCall(&i);
  }
  else if (Instruction::isCast(OpCode)) {
    LLVM_DEBUG(Logger->logInstruction(&i));
    const llvm::Value* op = i.getOperand(0);
    const generic_range_ptr_t info = fetchInfo(op);
    const auto res = handleCastInstruction(info, OpCode, i.getType());
    saveValueInfo(&i, res);

    LLVM_DEBUG(if (!info) Logger->logInfo("operand range is null"));
    LLVM_DEBUG(logRangeln(&i));
  }
  else if (Instruction::isBinaryOp(OpCode)) {
    LLVM_DEBUG(Logger->logInstruction(&i));
    const llvm::Value* op1 = i.getOperand(0);
    const llvm::Value* op2 = i.getOperand(1);
    const range_ptr_t info1 =
      std::dynamic_ptr_cast_or_null<range_t>(fetchInfo(op1));
    const range_ptr_t info2 =
      std::dynamic_ptr_cast_or_null<range_t>(fetchInfo(op2));
    const auto res = handleBinaryInstruction(info1, info2, OpCode);
    saveValueInfo(&i, res);

    LLVM_DEBUG(if (!info1) Logger->logInfo("first range is null"));
    LLVM_DEBUG(if (!info2) Logger->logInfo("second range is null"));
    LLVM_DEBUG(logRangeln(&i));
  }
  else if (OpCode == llvm::Instruction::FNeg) {
    LLVM_DEBUG(Logger->logInstruction(&i));
    const llvm::Value* op1 = i.getOperand(0);
    const range_ptr_t info1 =
      std::dynamic_ptr_cast_or_null<range_t>(fetchInfo(op1));
    const auto res = handleUnaryInstruction(info1, OpCode);
    saveValueInfo(&i, res);

    LLVM_DEBUG(if (!info1) Logger->logInfo("operand range is null"));
    LLVM_DEBUG(logRangeln(&i));
  }
  else {
    generic_range_ptr_t tmp;
    switch (OpCode) {
      // memory operations
      case llvm::Instruction::Alloca:
        DerivedRanges[&i] = make_range_node(fetchInfo(&i));
        break;
      case llvm::Instruction::Load:
        tmp = handleLoadInstr(&i);
        saveValueInfo(&i, tmp);
        LLVM_DEBUG(logRangeln(&i));
        break;
      case llvm::Instruction::Store:
        handleStoreInstr(&i);
        break;
      case llvm::Instruction::GetElementPtr:
        tmp = handleGEPInstr(&i);
        break;
      case llvm::Instruction::Fence:
        LLVM_DEBUG(Logger->logErrorln("Handling of Fence not supported yet"));
        break; // TODO implement
      case llvm::Instruction::AtomicCmpXchg:
        LLVM_DEBUG(Logger->logErrorln("Handling of AtomicCmpXchg not supported yet"));
        break; // TODO implement
      case llvm::Instruction::AtomicRMW:
        LLVM_DEBUG(Logger->logErrorln("Handling of AtomicRMW not supported yet"));
        break; // TODO implement

        // other operations
      case llvm::Instruction::Ret:
        handleReturn(I);
        break;
      case llvm::Instruction::Br:
        // do nothing
        break;
      case llvm::Instruction::ICmp:
      case llvm::Instruction::FCmp:
        tmp = handleCmpInstr(&i);
        saveValueInfo(&i, tmp);
        break;
      case llvm::Instruction::PHI:
        tmp = handlePhiNode(&i);
        saveValueInfo(&i, tmp);
        break;
      case llvm::Instruction::Select:
        tmp = handleSelect(&i);
        saveValueInfo(&i, tmp);
        break;
      case llvm::Instruction::UserOp1: // TODO implement
      case llvm::Instruction::UserOp2: // TODO implement
        LLVM_DEBUG(Logger->logErrorln("Handling of UserOp not supported yet"));
        break;
      case llvm::Instruction::VAArg: // TODO implement
        LLVM_DEBUG(Logger->logErrorln("Handling of VAArg not supported yet"));
        break;
      case llvm::Instruction::ExtractElement: // TODO implement
        LLVM_DEBUG(Logger->logErrorln("Handling of ExtractElement not supported yet"));
        break;
      case llvm::Instruction::InsertElement: // TODO implement
        LLVM_DEBUG(Logger->logErrorln("Handling of InsertElement not supported yet"));
        break;
      case llvm::Instruction::ShuffleVector: // TODO implement
        LLVM_DEBUG(Logger->logErrorln("Handling of ShuffleVector not supported yet"));
        break;
      case llvm::Instruction::ExtractValue: // TODO implement
        LLVM_DEBUG(Logger->logErrorln("Handling of ExtractValue not supported yet"));
        break;
      case llvm::Instruction::InsertValue: // TODO implement
        LLVM_DEBUG(Logger->logErrorln("Handling of InsertValue not supported yet"));
        break;
      case llvm::Instruction::LandingPad: // TODO implement
        LLVM_DEBUG(Logger->logErrorln("Handling of LandingPad not supported yet"));
        break;
      default:
        LLVM_DEBUG(Logger->logErrorln("unhandled instruction " + std::to_string(OpCode)));
        break;
    }
  } // end else
}

void
VRAnalyzer::setPathLocalInfo(std::shared_ptr<CodeAnalyzer> SuccAnalyzer,
                                     llvm::Instruction *TermInstr, unsigned SuccIdx) {
  // TODO extract more specific ranges from cmp
}

bool
VRAnalyzer::requiresInterpretation(llvm::Instruction *I) const {
  assert(I);
  if (llvm::CallBase *CB = llvm::dyn_cast<llvm::CallBase>(I)) {
    if (!CB->isIndirectCall()) {
      llvm::Function *Called = CB->getCalledFunction();
      if (Called->isIntrinsic())
        return false;

      if (isMathCallInstruction(Called->getName())) {
        return false;
      }
    }
    // I is a call, but we have no special way of handling it.
    return true;
  }
  // I is not a call.
  return false;
}

void
VRAnalyzer::prepareForCall(llvm::Instruction *I,
                           std::shared_ptr<AnalysisStore> FunctionStore) {
  llvm::CallBase *CB = llvm::cast<llvm::CallBase>(I);
  assert(!CB->isIndirectCall());

  LLVM_DEBUG(Logger->logInstruction(I));
  LLVM_DEBUG(Logger->logInfoln("preparing for function interpretation..."));

  LLVM_DEBUG(Logger->lineHead(); llvm::dbgs() << "Loading argument ranges: ");
  // fetch ranges of arguments
  std::list<range_node_ptr_t> ArgRanges;
  for (Value *Arg : CB->args()) {
    if (isa<Constant>(Arg))
      fetchInfo(Arg);
    ArgRanges.push_back(getOrCreateNode(Arg));

    LLVM_DEBUG(llvm::dbgs() << VRALogger::toString(fetchInfo(Arg)) << ", ");
  }
  LLVM_DEBUG(llvm::dbgs() << "\n");

  std::shared_ptr<VRAFunctionStore> FStore =
    std::static_ptr_cast<VRAFunctionStore>(FunctionStore);
  FStore->setArgumentRanges(*CB->getCalledFunction(), ArgRanges);
}

void
VRAnalyzer::returnFromCall(llvm::Instruction *I,
                           std::shared_ptr<AnalysisStore> FunctionStore) {
  llvm::CallBase *CB = llvm::cast<llvm::CallBase>(I);
  assert(!CB->isIndirectCall());

  LLVM_DEBUG(Logger->logInstruction(I); Logger->logInfo("returning from call"));

  std::shared_ptr<VRAFunctionStore> FStore =
    std::static_ptr_cast<VRAFunctionStore>(FunctionStore);
  generic_range_ptr_t Ret = FStore->getRetVal();
  if (Ret) {
    saveValueInfo(I, Ret);
    LLVM_DEBUG(logRangeln(I));
  } else {
    LLVM_DEBUG(Logger->logInfoln("function returns nothing"));
  }
}


////////////////////////////////////////////////////////////////////////////////
// Instruction Handlers
////////////////////////////////////////////////////////////////////////////////

void
VRAnalyzer::handleSpecialCall(const llvm::Instruction* I) {
  const CallBase* CB = cast<CallBase>(I);
  LLVM_DEBUG(Logger->logInstruction(I));

  // fetch function name
  llvm::Function* Callee = CB->getCalledFunction();
  if (Callee == nullptr) {
    LLVM_DEBUG(Logger->logInfo("indirect calls not supported"));
    return;
  }

  const std::string FunctionName = Callee->getName();
  if (isMathCallInstruction(FunctionName)) {
    // fetch ranges of arguments
    std::list<range_ptr_t> ArgScalarRanges;
    for (Value *Arg : CB->args()) {
      ArgScalarRanges.push_back(std::dynamic_ptr_cast_or_null<range_t>(fetchInfo(Arg)));
    }
    generic_range_ptr_t Res = handleMathCallInstruction(ArgScalarRanges, FunctionName);
    saveValueInfo(I, Res);
    LLVM_DEBUG(Logger->logInfo("whitelisted"));
    LLVM_DEBUG(Logger->logRangeln(Res));
  }
  else if (Callee->isIntrinsic()) {
    const auto IntrinsicsID = Callee->getIntrinsicID();
    switch (IntrinsicsID) {
      case llvm::Intrinsic::memcpy:
        handleMemCpyIntrinsics(CB);
        break;
      default:
        LLVM_DEBUG(Logger->logInfo("skipping intrinsic " + FunctionName));
    }
  }
  else {
    LLVM_DEBUG(Logger->logInfo("unsupported call"));
  }
}

void
VRAnalyzer::handleMemCpyIntrinsics(const llvm::Instruction* memcpy) {
  assert(isa<CallInst>(memcpy) || isa<InvokeInst>(memcpy));
  LLVM_DEBUG(Logger->logInfo("llvm.memcpy"));
  const BitCastInst* dest_bitcast =
    dyn_cast<BitCastInst>(memcpy->getOperand(0U));
  const BitCastInst* src_bitcast =
    dyn_cast<BitCastInst>(memcpy->getOperand(1U));
  if (!(dest_bitcast && src_bitcast)) {
    LLVM_DEBUG(Logger->logInfo("operand is not bitcast, aborting"));
    return;
  }
  const Value* dest = dest_bitcast->getOperand(0U);
  const Value* src = src_bitcast->getOperand(0U);

  const generic_range_ptr_t src_range = fetchInfo(src);
  saveValueInfo(dest, src_range);
  LLVM_DEBUG(Logger->logRangeln(src_range));
}

void
VRAnalyzer::handleReturn(const llvm::Instruction* ret) {
  const llvm::ReturnInst* ret_i = cast<llvm::ReturnInst>(ret);
  LLVM_DEBUG(Logger->logInstruction(ret));
  if (const llvm::Value* ret_val = ret_i->getReturnValue()) {
    const llvm::Function* ret_fun = ret_i->getFunction();
    generic_range_ptr_t range = fetchInfo(ret_val);

    std::shared_ptr<VRAFunctionStore> FStore =
      std::static_ptr_cast<VRAFunctionStore>(CodeInt.getFunctionStore());
    FStore->setRetVal(range);

    LLVM_DEBUG(Logger->logRangeln(range));
  } else {
    LLVM_DEBUG(Logger->logInfoln("void return."));
  }
}

void
VRAnalyzer::handleStoreInstr(const llvm::Instruction* store) {
  const llvm::StoreInst* store_i = cast<llvm::StoreInst>(store);
  LLVM_DEBUG(Logger->logInstruction(store));
  const llvm::Value* address_param = store_i->getPointerOperand();
  const llvm::Value* value_param = store_i->getValueOperand();

  if (value_param->getType()->isPointerTy()) {
    LLVM_DEBUG(Logger->logInfoln("pointer store"));
    if (isa<llvm::ConstantPointerNull>(value_param))
      return;

    if (isDescendant(address_param, value_param))
      LLVM_DEBUG(Logger->logInfo("pointer circularity!"));
    else {
      const generic_range_ptr_t old_range = fetchInfo(address_param);
      setNode(address_param, make_range_node(value_param, std::vector<unsigned>()));
      saveValueInfo(address_param, old_range);
    }
    return;
  }

  const generic_range_ptr_t range = fetchInfo(value_param);
  saveValueInfo(address_param, range);
  saveValueInfo(store_i, range);
  LLVM_DEBUG(logRangeln(store_i));
  return;
}

generic_range_ptr_t
VRAnalyzer::handleLoadInstr(llvm::Instruction* load) {
  llvm::LoadInst* load_i = cast<llvm::LoadInst>(load);
  LLVM_DEBUG(Logger->logInstruction(load));

  if (load_i->getType()->isPointerTy()) {
    LLVM_DEBUG(Logger->logInfo("pointer load"));
    setNode(load_i, make_range_node(load_i->getPointerOperand(),
                                    std::vector<unsigned>()));
    return nullptr;
  }

  MemorySSA& memssa = CodeInt.getPass().getAnalysis<MemorySSAWrapperPass>(
                        *load->getFunction()).getMSSA();
  MemSSAUtils memssa_utils(memssa);
  SmallVectorImpl<Value*>& def_vals = memssa_utils.getDefiningValues(load_i);
  def_vals.push_back(load_i->getPointerOperand());

  Type *load_ty = fullyUnwrapPointerOrArrayType(load->getType());
  generic_range_ptr_t res = nullptr;
  for (Value *dval : def_vals) {
    if (dval &&
        load_ty->canLosslesslyBitCastTo(fullyUnwrapPointerOrArrayType(dval->getType())))
      res = getUnionRange(res, fetchInfo(dval));
  }
  return res;
}

generic_range_ptr_t
VRAnalyzer::handleGEPInstr(const llvm::Instruction* gep) {
  const llvm::GetElementPtrInst* gep_i = dyn_cast<llvm::GetElementPtrInst>(gep);
  LLVM_DEBUG(Logger->logInstruction(gep_i));

  range_node_ptr_t node = getNode(gep);
  if (node != nullptr) {
    if (node->hasRange())
      return node->getRange();
    LLVM_DEBUG(Logger->logInfoln("has node"));
  } else {
    std::vector<unsigned> offset;
    if (!extractGEPOffset(gep_i->getSourceElementType(),
                          iterator_range<User::const_op_iterator>(gep_i->idx_begin(),
                                                                  gep_i->idx_end()),
                          offset))
      return nullptr;
    node = make_range_node(gep_i->getPointerOperand(), offset);
    DerivedRanges[gep] = node;
  }

  return fetchInfo(gep_i);
}

bool
VRAnalyzer::isDescendant(const llvm::Value* parent,
                                 const llvm::Value* desc) const {
  if (!(parent && desc)) return false;
  if (parent == desc) return true;
  while (desc) {
    range_node_ptr_t desc_node = getNode(desc);
    desc = (desc_node) ? desc_node->getParent() : nullptr;
    if (desc == parent)
      return true;
  }
  return false;
}

range_ptr_t
VRAnalyzer::handleCmpInstr(const llvm::Instruction* cmp) {
  const llvm::CmpInst* cmp_i = cast<llvm::CmpInst>(cmp);
  LLVM_DEBUG(Logger->logInstruction(cmp));
  const llvm::CmpInst::Predicate pred = cmp_i->getPredicate();
  std::list<generic_range_ptr_t> ranges;
  for (unsigned index = 0; index < cmp_i->getNumOperands(); index++) {
    const llvm::Value* op = cmp_i->getOperand(index);
    generic_range_ptr_t op_range = fetchInfo(op);
    ranges.push_back(op_range);
  }
  range_ptr_t result = std::dynamic_ptr_cast_or_null<range_t>(handleCompare(ranges, pred));
  LLVM_DEBUG(Logger->logRangeln(result));
  return result;
}

generic_range_ptr_t
VRAnalyzer::handlePhiNode(const llvm::Instruction* phi) {
  const llvm::PHINode* phi_n = cast<llvm::PHINode>(phi);
  if (phi_n->getNumIncomingValues() < 1) {
    return nullptr;
  }
  LLVM_DEBUG(Logger->logInstruction(phi));
  const llvm::Value* op0 = phi_n->getIncomingValue(0);
  generic_range_ptr_t res = fetchInfo(op0);
  for (unsigned index = 1; index < phi_n->getNumIncomingValues(); index++) {
    const llvm::Value* op = phi_n->getIncomingValue(index);
    generic_range_ptr_t op_range = fetchInfo(op);
    res = getUnionRange(res, op_range);
  }
  LLVM_DEBUG(Logger->logRangeln(res));
  return res;
}

generic_range_ptr_t
VRAnalyzer::handleSelect(const llvm::Instruction* i) {
  const llvm::SelectInst* sel = cast<llvm::SelectInst>(i);
  LLVM_DEBUG(Logger->logInstruction(sel));
  // TODO: actually handle comparison.
  generic_range_ptr_t res = getUnionRange(fetchInfo(sel->getFalseValue()),
                                          fetchInfo(sel->getTrueValue()));
  LLVM_DEBUG(Logger->logRangeln(res));
  return res;
}


////////////////////////////////////////////////////////////////////////////////
// Data Handling
////////////////////////////////////////////////////////////////////////////////

const generic_range_ptr_t
VRAnalyzer::fetchInfo(const llvm::Value* V) {
  const generic_range_ptr_t Derived = VRAStore::fetchInfo(V);
  if (Derived) {
    if (std::isa_ptr<VRA_Structured_Range>(Derived)
        && V->getType()->isPointerTy()) {
      if (generic_range_ptr_t InputRange = getGlobalStore()->getUserInput(V)) {
        // fill null input_range fields with corresponding derived fields
        return fillRangeHoles(Derived, InputRange);
      }
    }
    return Derived;
  }

  const generic_range_ptr_t InputRange = getGlobalStore()->getUserInput(V);
  if (InputRange) {
    // Save it in this store, so we don't overwrite it if it's final.
    saveValueInfo(V, InputRange);
  }
  return InputRange;
}

range_node_ptr_t
VRAnalyzer::getNode(const llvm::Value* v) const {
  range_node_ptr_t LocalNode = VRAStore::getNode(v);
  if (LocalNode)
    return LocalNode;

  std::shared_ptr<VRAStore> ExternalStore = getAnalysisStoreForValue(v);
  if (ExternalStore) {
    return ExternalStore->VRAStore::getNode(v);
  }
  return nullptr;
}

range_node_ptr_t
VRAnalyzer::getOrCreateNode(const llvm::Value* v) {
  range_node_ptr_t Node = VRAStore::getOrCreateNode(v);
  if (Node) {
    return Node;
  }

  if (isa<GlobalVariable>(v) || isa<Argument>(v)) {
      // create root node in global analyzer
      return getGlobalStore()->getOrCreateNode(v);
  }

  return nullptr;
}

void
VRAnalyzer::setNode(const llvm::Value* V, range_node_ptr_t Node) {
  if (isa<GlobalVariable>(V)) {
      // set node in global analyzer
      getGlobalStore()->setNode(V, Node);
      return;
  }
  if (isa<Argument>(V)) {
    std::shared_ptr<VRAFunctionStore> FStore =
      std::static_ptr_cast<VRAFunctionStore>(CodeInt.getFunctionStore());
    FStore->setNode(V, Node);
    return;
  }

  DerivedRanges[V] = Node;
}

void
VRAnalyzer::logRangeln(const llvm::Value* v) {
  LLVM_DEBUG(if (getGlobalStore()->getUserInput(v)) dbgs() << "(from metadata) ");
  LLVM_DEBUG(Logger->logRangeln(fetchInfo(v)));
}
