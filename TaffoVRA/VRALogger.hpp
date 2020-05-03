#ifndef TAFFO_VRALOGGER_HPP
#define TAFFO_VRALOGGER_HPP

#include <string>
#include "llvm/Support/Debug.h"
#include "CodeInterpreter.hpp"
#include "Range.hpp"

namespace taffo {

#define DEBUG_TYPE "taffo-vra"
#define DEBUG_HEAD "[TAFFO][VRA]"

class VRALogger : public CILogger {
public:
  VRALogger() : CILogger(CILK_VRALogger), IndentLevel(0U) {}

  const char *getDebugType() const { return DEBUG_TYPE; }

  void logBasicBlock(const llvm::BasicBlock *BB) const {
    assert(BB);
    lineHead();
    llvm::dbgs() << BB->getName() << "\n";
  }

  void logStartFunction(const llvm::Function *F) {
    assert(F);
    ++IndentLevel;
    llvm::dbgs() << "\n";
    lineHead();
    llvm::dbgs() << "Interpreting function " << F->getName() << "\n";
  }

  void logEndFunction(const llvm::Function *F) {
    assert(F);
    lineHead();
    llvm::dbgs() << "Finished interpreting function " << F->getName() << "\n\n";
    if (IndentLevel > 0) --IndentLevel;
  }

  void logInstruction(const llvm::Value* V) {
    assert(V);
    lineHead();
    llvm::dbgs() << *V << ": ";
  }

  void logRange(const generic_range_ptr_t Range) {
    llvm::dbgs() << toString(Range);
  }

  void logRangeln(const generic_range_ptr_t Range) {
    llvm::dbgs() << toString(Range) << "\n";
  }

  void logInfo(const llvm::StringRef Info) {
    llvm::dbgs() << "(" << Info << ") ";
  }

  void logInfoln(const llvm::StringRef Info) {
    llvm::dbgs() << Info << "\n";
  }

  void logErrorln(const llvm::StringRef Error) {
    lineHead();
    llvm::dbgs() << Error << "\n";
  }

  void lineHead() const {
     llvm::dbgs() << DEBUG_HEAD << std::string(IndentLevel * 2U, ' ');
  }

  static std::string toString(const generic_range_ptr_t& Range) {
    if (Range) {
      const range_ptr_t scalar = std::dynamic_ptr_cast<range_t>(Range);
      if (scalar) {
        return "[" + std::to_string(scalar->min()) + ", "
          + std::to_string(scalar->max()) + "]";
      } else {
        const range_s_ptr_t structured = std::dynamic_ptr_cast<range_s_t>(Range);
        assert(structured);
        std::string result("{ ");
        for (const generic_range_ptr_t field : structured->ranges()) {
          result.append(toString(field));
          result.append(", ");
        }
        result.append("}");
        return result;
      }
    } else {
      return "null range";
    }
  }

  static bool classof(const CILogger *L) {
    return L->getKind() == CILK_VRALogger;
  }

private:
  unsigned IndentLevel;
};

}

#endif
