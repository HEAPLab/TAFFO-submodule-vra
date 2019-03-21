#ifndef TAFFO_VRA_RANGE_HPP
#define TAFFO_VRA_RANGE_HPP

#include "llvm/Support/Casting.h"

#include <limits>
#include <memory>
#include <vector>

// reimplement dynamic_pointer_cast with LLVM-style RTTI
namespace std {
	template< class T, class U >
	std::shared_ptr<T> dynamic_ptr_cast( const std::shared_ptr<U>& r ) noexcept
	{
		if (auto p = llvm::dyn_cast<typename std::shared_ptr<T>::element_type>(r.get())) {
			return std::shared_ptr<T>(r, p);
		} else {
			return std::shared_ptr<T>();
		}
	}
	template< class T, class U >
	std::shared_ptr<T> dynamic_ptr_cast_or_null( const std::shared_ptr<U>& r ) noexcept
	{
		if (auto p = llvm::dyn_cast_or_null<typename std::shared_ptr<T>::element_type>(r.get())) {
			return std::shared_ptr<T>(r, p);
		} else {
			return std::shared_ptr<T>();
		}
	}
} // end namespace std

namespace taffo {

struct VRA_Generic_Range {

	enum rangeKind {
		kind_scalar,
		kind_structured
	};
private:
	const rangeKind kind;
public:
	rangeKind getKind() const {return kind;}

public:
	VRA_Generic_Range(rangeKind k) : kind(k) {}
	VRA_Generic_Range(const VRA_Generic_Range&) = delete;
	virtual ~VRA_Generic_Range() {};
};

using generic_range_ptr_t = std::shared_ptr<VRA_Generic_Range>;

template<
	typename num_t,
	typename = typename std::enable_if<std::is_arithmetic<num_t>::value, num_t>::type
	>
struct VRA_Range : VRA_Generic_Range
{
public:
	VRA_Range(const num_t min, const num_t max)
		: VRA_Generic_Range(kind_scalar),
		_min(min),
		_max(max)
		{}
	VRA_Range()
		: VRA_Generic_Range(kind_scalar),
		_min(std::numeric_limits<num_t>::lowest()),
		_max(std::numeric_limits<num_t>::max())
		{}
	VRA_Range(const VRA_Range& rhs)
		: VRA_Generic_Range(kind_scalar),
		_min(rhs.min()),
		_max(rhs.max())
		{}
	virtual ~VRA_Range(){}

private:
	num_t _min, _max;

public:
	inline const num_t min() const {return _min;}
	inline const num_t max() const {return _max;}
	inline const bool isConstant() const {return min() == max();}
	inline const bool cross(const num_t val = 0.0) const {
		return min() <= val && max() >= val;
	}

	// LLVM-style RTTI stuff
public:
	static bool classof(const VRA_Generic_Range* range) {
		return range->getKind() == kind_scalar;
	}
};

using num_t = double;
using range_t = VRA_Range<num_t>;
using range_ptr_t = std::shared_ptr<range_t>;
template<class... Args>
static inline range_ptr_t make_range(Args&&... args) {
  return std::make_shared<range_t>(std::forward<Args>(args)...);
}


struct VRA_Structured_Range;
using range_s_ptr_t = std::shared_ptr<VRA_Structured_Range>;

struct VRA_Structured_Range : VRA_Generic_Range
{
public:
	VRA_Structured_Range()
	: VRA_Generic_Range(kind_structured)
	{
		_ranges = {nullptr};
	}
	VRA_Structured_Range(const generic_range_ptr_t& r)
	: VRA_Generic_Range(kind_structured)
	{
		_ranges = {r};
	}
	VRA_Structured_Range(std::vector<generic_range_ptr_t>& rhs)
	: VRA_Generic_Range(kind_structured),
	_ranges(rhs)
	{}
	VRA_Structured_Range(const VRA_Structured_Range& rhs)
	: VRA_Generic_Range(kind_structured),
	_ranges(rhs.ranges())
	{}
	virtual ~VRA_Structured_Range(){}

private:
	std::vector<generic_range_ptr_t> _ranges;

public:
	inline const std::vector<generic_range_ptr_t> ranges() const {return _ranges; }

	inline bool isScalarOrArray() const {return _ranges.size() == 1;}

	inline bool isStruct() const {return _ranges.size() > 1;}

	inline generic_range_ptr_t getRangeAt(unsigned index) const {return _ranges[index];}

	inline range_ptr_t toScalarRange(unsigned index = 0) const {
		return std::dynamic_ptr_cast<range_t>(getRangeAt(index));
	}

	inline range_s_ptr_t toStructRange(unsigned index = 0) const {
		return std::dynamic_ptr_cast<VRA_Structured_Range>(getRangeAt(index));
	}

	inline void setRangeAt(unsigned index, const generic_range_ptr_t& range) {
		_ranges[index] = range;
	}

	// LLVM-style RTTI stuff
public:
	static bool classof(const VRA_Generic_Range* range) {
		return range->getKind() == kind_structured;
	}

};

template<class... Args>
static inline range_s_ptr_t make_s_range(Args&&... args) {
  return std::make_shared<VRA_Structured_Range>(std::forward<Args>(args)...);
}

} //end namespace

#endif /* end of include guard: TAFFO_VRA_RANGE_HPP */
