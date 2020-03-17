#include "ValueRangeAnalyzer.hpp"

namespace taffo {

using namespace llvm;

void ValueRangeAnalyzer::convexMerge(const CodeAnalyzer &Other) {
  // TODO
}

std::shared_ptr<CodeAnalyzer> ValueRangeAnalyzer::newCodeAnalyzer(CodeInterpreter *CI);
std::shared_ptr<CodeAnalyzer> ValueRangeAnalyzer::clone();

void ValueRangeAnalyzer::analyzeInstruction(llvm::Instruction *I) {
  assert(I);
  Instruction &i = *I;
  const unsigned OpCode = i.getOpcode();
  if (OpCode == Instruction::Call) {
    handleCallBase(&i);
  }
  else if (Instruction::isTerminator(OpCode)) {
    handleTerminators(&i);
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
    const auto node1 = getNode(op1);
    const auto node2 = getNode(op2);
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
#if LLVM_VERSION > 7
  else if (Instruction::isUnaryOp(OpCode)) {
    logInstruction(&i);
    const llvm::Value* op1 = i.getOperand(0);
    const auto node = fetchInfo(op1);
    const range_ptr_t info1 = (node1) ? node1->getScalarRange() : nullptr;
    const auto res = handleUnaryInstruction(info1, OpCode);
    saveValueInfo(&i, res);

    LLVM_DEBUG(if (!info1) logInfo("operand range is null"));
    logRangeln(&i);
  }
#endif
  else {
    generic_range_ptr_t tmp;
    switch (OpCode) {
      // memory operations
      case llvm::Instruction::Alloca:
        derived_ranges[&i] = make_range_node(fetchInfo(&i));
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
        emitError("unknown instruction " + std::to_string(OpCode));
        break;
    }
    // TODO here be dragons
  } // end else
}

void ValueRangeAnalyzer::setPathLocalInfo(std::shared_ptr<CodeAnalyzer> SuccAnalyzer,
                                          llvm::Instruction *TermInstr, unsigned SuccIdx);
bool ValueRangeAnalyzer::requiresInterpretation(llvm::Instruction *I) const;
void ValueRangeAnalyzer::prepareForCall(llvm::Instruction *I, const CodeAnalyzer &GlobalAnalyzer);
void ValueRangeAnalyzer::returnFromCall(llvm::Instruction *I, const CodeAnalyzer &GlobalAnalyzer);



} // end namespace taffo
