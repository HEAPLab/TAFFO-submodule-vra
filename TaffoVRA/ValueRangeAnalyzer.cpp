#include "ValueRangeAnalyzer.hpp"

#include "llvm/IR/Intrinsics.h"
#include "llvm/Support/Debug.h"
#include "RangeOperations.hpp"
#include "TypeUtils.h"
#include "MemSSAUtils.hpp"

using namespace llvm;
using namespace taffo;

void
ValueRangeAnalyzer::convexMerge(const AnalysisStore &Other) {
  if (isa<ValueRangeAnalyzer>(Other)) {
    VRAStore::convexMerge(llvm::cast<VRAStore>(llvm::cast<ValueRangeAnalyzer>(Other)));
  } else {
    VRAStore::convexMerge(llvm::cast<VRAStore>(llvm::cast<VRAGlobalStore>(Other)));
  }
}

std::shared_ptr<CodeAnalyzer>
ValueRangeAnalyzer::newCodeAnalyzer(CodeInterpreter &CI) {
  return std::make_shared<ValueRangeAnalyzer>(CI);
}

std::shared_ptr<CodeAnalyzer>
ValueRangeAnalyzer::clone() {
  return std::make_shared<ValueRangeAnalyzer>(*this);
}

void
ValueRangeAnalyzer::analyzeInstruction(llvm::Instruction *I) {
  assert(I);
  Instruction &i = *I;
  const unsigned OpCode = i.getOpcode();
  if (OpCode == Instruction::Call
      || OpCode == Instruction::Invoke) {
    handleSpecialCall(&i);
  }
  else if (Instruction::isCast(OpCode)) {
    logInstruction(&i);
    const llvm::Value* op = i.getOperand(0);
    const generic_range_ptr_t info = fetchInfo(op);
    const auto res = handleCastInstruction(info, OpCode, i.getType());
    saveValueInfo(&i, res);

    LLVM_DEBUG(if (!info) logInfo("operand range is null"));
    logRangeln(&i);
  }
  else if (Instruction::isBinaryOp(OpCode)) {
    logInstruction(&i);
    const llvm::Value* op1 = i.getOperand(0);
    const llvm::Value* op2 = i.getOperand(1);
    const range_ptr_t info1 =
      std::dynamic_ptr_cast_or_null<range_t>(fetchInfo(op1));
    const range_ptr_t info2 =
      std::dynamic_ptr_cast_or_null<range_t>(fetchInfo(op2));
    const auto res = handleBinaryInstruction(info1, info2, OpCode);
    saveValueInfo(&i, res);

    LLVM_DEBUG(if (!info1) logInfo("first range is null"));
    LLVM_DEBUG(if (!info2) logInfo("second range is null"));
    logRangeln(&i);
  }
  else if (OpCode == llvm::Instruction::FNeg) {
    logInstruction(&i);
    const llvm::Value* op1 = i.getOperand(0);
    const range_ptr_t info1 =
      std::dynamic_ptr_cast_or_null<range_t>(fetchInfo(op1));
    const auto res = handleUnaryInstruction(info1, OpCode);
    saveValueInfo(&i, res);

    LLVM_DEBUG(if (!info1) logInfo("operand range is null"));
    logRangeln(&i);
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
        logRangeln(&i);
        break;
      case llvm::Instruction::Store:
        handleStoreInstr(&i);
        break;
      case llvm::Instruction::GetElementPtr:
        tmp = handleGEPInstr(&i);
        break;
      case llvm::Instruction::Fence:
        emitError("Handling of Fence not supported yet");
        break; // TODO implement
      case llvm::Instruction::AtomicCmpXchg:
        emitError("Handling of AtomicCmpXchg not supported yet");
        break; // TODO implement
      case llvm::Instruction::AtomicRMW:
        emitError("Handling of AtomicRMW not supported yet");
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
        emitError("Handling of UserOp not supported yet");
        break;
      case llvm::Instruction::VAArg: // TODO implement
        emitError("Handling of VAArg not supported yet");
        break;
      case llvm::Instruction::ExtractElement: // TODO implement
        emitError("Handling of ExtractElement not supported yet");
        break;
      case llvm::Instruction::InsertElement: // TODO implement
        emitError("Handling of InsertElement not supported yet");
        break;
      case llvm::Instruction::ShuffleVector: // TODO implement
        emitError("Handling of ShuffleVector not supported yet");
        break;
      case llvm::Instruction::ExtractValue: // TODO implement
        emitError("Handling of ExtractValue not supported yet");
        break;
      case llvm::Instruction::InsertValue: // TODO implement
        emitError("Handling of InsertValue not supported yet");
        break;
      case llvm::Instruction::LandingPad: // TODO implement
        emitError("Handling of LandingPad not supported yet");
        break;
      default:
        emitError("unhandled instruction " + std::to_string(OpCode));
        break;
    }
  } // end else
}

void
ValueRangeAnalyzer::setPathLocalInfo(std::shared_ptr<CodeAnalyzer> SuccAnalyzer,
                                     llvm::Instruction *TermInstr, unsigned SuccIdx) {
  // TODO extract more specific ranges from cmp
}

bool
ValueRangeAnalyzer::requiresInterpretation(llvm::Instruction *I) const {
  assert(I);
  if (llvm::CallBase *CB = llvm::dyn_cast<llvm::CallBase>(I)) {
    if (!CB->isIndirectCall()) {
      llvm::Function *Called = CB->getCalledFunction();
      if (Called->isIntrinsic())
        return false;

      if (CB->getNumArgOperands() == 1U
          && isMathCallInstruction(Called->getName())) {
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
ValueRangeAnalyzer::prepareForCall(llvm::Instruction *I) {
  llvm::CallBase *CB = llvm::cast<llvm::CallBase>(I);
  assert(!CB->isIndirectCall());

  logInstruction(I);
  LLVM_DEBUG(dbgs() << "preparing for function interpretation...\n\n");

  // fetch ranges of arguments
  std::list<range_node_ptr_t> ArgRanges;
  for (Value *Arg : CB->args()) {
    if (isa<Constant>(Arg))
      fetchInfo(Arg);
    ArgRanges.push_back(getOrCreateNode(Arg));
  }

  getGlobalStore()->setArgumentRanges(*CB->getCalledFunction(), ArgRanges);
}

void
ValueRangeAnalyzer::returnFromCall(llvm::Instruction *I) {
  llvm::CallBase *CB = llvm::cast<llvm::CallBase>(I);
  assert(!CB->isIndirectCall());

  LLVM_DEBUG(dbgs() << "\n");
  logInstruction(I);
  logInfo("returning from call");

  generic_range_ptr_t Ret = getGlobalStore()->findRetVal(CB->getCalledFunction());
  if (Ret) {
    saveValueInfo(I, Ret);
    logRangeln(I);
  } else {
    logInfoln("function returns nothing");
  }
}


////////////////////////////////////////////////////////////////////////////////
// Instruction Handlers
////////////////////////////////////////////////////////////////////////////////

void
ValueRangeAnalyzer::handleSpecialCall(const llvm::Instruction* I) {
  const CallBase* CB = cast<CallBase>(I);
  logInstruction(I);

  // fetch function name
  llvm::Function* Callee = CB->getCalledFunction();
  if (Callee == nullptr) {
    logError("indirect calls not supported");
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
    logInfo("whitelisted");
    VRAStore::logRangeln(Res);
  }
  else if (Callee->isIntrinsic()) {
    const auto IntrinsicsID = Callee->getIntrinsicID();
    switch (IntrinsicsID) {
      case llvm::Intrinsic::memcpy:
        handleMemCpyIntrinsics(CB);
        break;
      default:
        logInfo("skipping intrinsic " + FunctionName);
    }
  }
  else {
    logError("unsupported call");
  }
}

void
ValueRangeAnalyzer::handleMemCpyIntrinsics(const llvm::Instruction* memcpy) {
  assert(isa<CallInst>(memcpy) || isa<InvokeInst>(memcpy));
  logInfo("llvm.memcpy");
  const BitCastInst* dest_bitcast =
    dyn_cast<BitCastInst>(memcpy->getOperand(0U));
  const BitCastInst* src_bitcast =
    dyn_cast<BitCastInst>(memcpy->getOperand(1U));
  if (!(dest_bitcast && src_bitcast)) {
    logError("operand is not bitcast, aborting");
    return;
  }
  const Value* dest = dest_bitcast->getOperand(0U);
  const Value* src = src_bitcast->getOperand(0U);

  const generic_range_ptr_t src_range = fetchInfo(src);
  saveValueInfo(dest, src_range);
  VRAStore::logRangeln(src_range);
}

void
ValueRangeAnalyzer::handleReturn(const llvm::Instruction* ret) {
  const llvm::ReturnInst* ret_i = cast<llvm::ReturnInst>(ret);
  logInstruction(ret);
  const llvm::Value* ret_val = ret_i->getReturnValue();
  const llvm::Function* ret_fun = ret_i->getFunction();
  generic_range_ptr_t range = fetchInfo(ret_val);
  generic_range_ptr_t partial = getGlobalStore()->findRetVal(ret_fun);
  generic_range_ptr_t returned = getUnionRange(partial, range);
  getGlobalStore()->setRetVal(ret_fun, returned);
  VRAStore::logRangeln(returned);
}

void
ValueRangeAnalyzer::handleStoreInstr(const llvm::Instruction* store) {
  const llvm::StoreInst* store_i = cast<llvm::StoreInst>(store);
  logInstruction(store);
  const llvm::Value* address_param = store_i->getPointerOperand();
  const llvm::Value* value_param = store_i->getValueOperand();

  if (value_param->getType()->isPointerTy()) {
    logInfoln("pointer store");
    if (isa<llvm::ConstantPointerNull>(value_param))
      return;

    if (isDescendant(address_param, value_param))
      logError("pointer circularity!");
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
  logRangeln(store_i);
  return;
}

generic_range_ptr_t
ValueRangeAnalyzer::handleLoadInstr(llvm::Instruction* load) {
  llvm::LoadInst* load_i = dyn_cast<llvm::LoadInst>(load);
  if (!load_i) {
    emitError("Could not convert load instruction to LoadInst");
    return nullptr;
  }
  logInstruction(load);

  if (load_i->getType()->isPointerTy()) {
    logInfoln("pointer load");
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
ValueRangeAnalyzer::handleGEPInstr(const llvm::Instruction* gep) {
  const llvm::GetElementPtrInst* gep_i = dyn_cast<llvm::GetElementPtrInst>(gep);
  logInstruction(gep_i);

  range_node_ptr_t node = getNode(gep);
  if (node != nullptr) {
    if (node->hasRange())
      return node->getRange();
    logInfoln("has node");
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
ValueRangeAnalyzer::isDescendant(const llvm::Value* parent,
                                 const llvm::Value* desc) const {
  if (!(parent && desc)) return false;
  if (parent == desc) return true;
  while (desc) {
    range_node_ptr_t desc_node = getNode(desc);
    desc = desc_node->getParent();
    if (desc == parent)
      return true;
  }
  return false;
}

range_ptr_t
ValueRangeAnalyzer::handleCmpInstr(const llvm::Instruction* cmp) {
  const llvm::CmpInst* cmp_i = dyn_cast<llvm::CmpInst>(cmp);
  if (!cmp_i) {
    emitError("Could not convert Compare instruction to CmpInst");
    return nullptr;
  }
  logInstruction(cmp);
  const llvm::CmpInst::Predicate pred = cmp_i->getPredicate();
  std::list<generic_range_ptr_t> ranges;
  for (unsigned index = 0; index < cmp_i->getNumOperands(); index++) {
    const llvm::Value* op = cmp_i->getOperand(index);
    generic_range_ptr_t op_range = fetchInfo(op);
    ranges.push_back(op_range);
  }
  range_ptr_t result = std::dynamic_ptr_cast_or_null<range_t>(handleCompare(ranges, pred));
  VRAStore::logRangeln(result);
  return result;
}

generic_range_ptr_t
ValueRangeAnalyzer::handlePhiNode(const llvm::Instruction* phi) {
  const llvm::PHINode* phi_n = dyn_cast<llvm::PHINode>(phi);
  if (!phi_n) {
    emitError("Could not convert Phi instruction to PHINode");
    return nullptr;
  }
  if (phi_n->getNumIncomingValues() < 1) {
    return nullptr;
  }
  logInstruction(phi);
  const llvm::Value* op0 = phi_n->getIncomingValue(0);
  generic_range_ptr_t res = fetchInfo(op0);
  for (unsigned index = 1; index < phi_n->getNumIncomingValues(); index++) {
    const llvm::Value* op = phi_n->getIncomingValue(index);
    generic_range_ptr_t op_range = fetchInfo(op);
    res = getUnionRange(res, op_range);
  }
  VRAStore::logRangeln(res);
  return res;
}

generic_range_ptr_t
ValueRangeAnalyzer::handleSelect(const llvm::Instruction* i) {
  const llvm::SelectInst* sel = cast<llvm::SelectInst>(i);
  logInstruction(sel);
  // TODO: actually handle comparison.
  generic_range_ptr_t res = getUnionRange(fetchInfo(sel->getFalseValue()),
                                          fetchInfo(sel->getTrueValue()));
  VRAStore::logRangeln(res);
  return res;
}


////////////////////////////////////////////////////////////////////////////////
// Data Handling
////////////////////////////////////////////////////////////////////////////////

const generic_range_ptr_t
ValueRangeAnalyzer::fetchInfo(const llvm::Value* v,
                              bool derived_or_final) {
  generic_range_ptr_t input_range = getGlobalStore()->getUserInput(v);
  if (input_range && (!derived_or_final || input_range->isFinal())) {
    return input_range;
  }

  const generic_range_ptr_t Derived = VRAStore::fetchInfo(v);
  if (input_range
      && Derived
      && std::isa_ptr<VRA_Structured_Range>(Derived)
      && v->getType()->isPointerTy()) {
    // fill null input_range fields with corresponding derived fields
    return fillRangeHoles(Derived, input_range);
  }

  return Derived;
}

range_node_ptr_t
ValueRangeAnalyzer::getNode(const llvm::Value* v) const {
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
ValueRangeAnalyzer::getOrCreateNode(const llvm::Value* v) {
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
ValueRangeAnalyzer::setNode(const llvm::Value* V, range_node_ptr_t Node) {
  if (isa<GlobalVariable>(V) || isa<Argument>(V)) {
      // set node in global analyzer
      return getGlobalStore()->setNode(V, Node);
  }
  DerivedRanges[V] = Node;
}

void
ValueRangeAnalyzer::logRangeln(const llvm::Value* v) {
  LLVM_DEBUG(if (getGlobalStore()->getUserInput(v)) dbgs() << "(from metadata) ");
  VRAStore::logRangeln(v);
}
