#ifndef TAFFO_VRA_RANGE_NODE_HPP
#define TAFFO_VRA_RANGE_NODE_HPP

#include "Range.hpp"

#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"

#include <memory>

namespace taffo {

struct VRA_RangeNode {
private:
	const llvm::Value* parent;
	unsigned parent_offset;
	generic_range_ptr_t range;
	bool _hasRange;

public:
	VRA_RangeNode() {}
	VRA_RangeNode(const generic_range_ptr_t& r)
		: parent(nullptr), parent_offset(0), range(r), _hasRange(true) {}

	VRA_RangeNode(const llvm::Value* p, unsigned offset)
		: parent(p), parent_offset(offset), range(nullptr), _hasRange(false) {}

public:
	inline const llvm::Value* getParent() const {return parent;}

	inline unsigned getOffset() const {return parent_offset;}

	inline generic_range_ptr_t getRange() const {return range;}

	inline bool hasRange() const {return _hasRange;}

	inline bool isScalar() const {return hasRange() && std::dynamic_pointer_cast<range_t>(range) != nullptr;}

	inline bool isStruct() const {return hasRange() && std::dynamic_pointer_cast<VRA_Structured_Range>(range) != nullptr;}

	inline void setScalarRange(const range_ptr_t& r) {
		range = r;
		_hasRange = true;
		return;
	}

	inline void setRange(const generic_range_ptr_t& r) {
		range = r;
		_hasRange = true;
		return;
	}

	inline void setStructRange(const range_s_ptr_t& r) {
		range = r;
		_hasRange = true;
		return;
	}

	inline range_ptr_t getScalarRange() const {
		if (!isScalar()) {
			return nullptr;
		}
		return std::static_pointer_cast<VRA_Range<num_t>>(range);
	}

	inline range_s_ptr_t getStructRange() const {
		if (!isStruct()) {
			return nullptr;
		}
		return std::static_pointer_cast<VRA_Structured_Range>(range);
	}

};

// someday I will remember why I wrote it....
static bool isStructEquivalent(const llvm::Type* type) {
	if (type->isStructTy()) {
		return true;
	}
	if (type->isArrayTy()) {
		return isStructEquivalent(type->getArrayElementType());
	}
	if (type->isPointerTy()) {
		return isStructEquivalent(type->getPointerElementType());
	}
	return false;
}

} /* taffo */

#endif /* end of include guard: TAFFO_VRA_RANGE_NODE_HPP */
