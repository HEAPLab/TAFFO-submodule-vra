#ifndef TAFFO_VRA_RANGE_HPP
#define TAFFO_VRA_RANGE_HPP

#include <limits>
#include <memory>
#include <vector>

namespace taffo {

struct VRA_Generic_Range {
	virtual ~VRA_Generic_Range() = default;
};

using generic_range_ptr_t = std::shared_ptr<VRA_Generic_Range>;

template<
	typename num_t,
	typename = typename std::enable_if<std::is_arithmetic<num_t>::value, num_t>::type
	>
struct VRA_Range : VRA_Generic_Range
{
public:
	VRA_Range(const num_t min, const num_t max) : _min(min), _max(max) {}
	VRA_Range() : _min(std::numeric_limits<num_t>::lowest()), _max(std::numeric_limits<num_t>::max()) {}
	VRA_Range(const VRA_Range& rhs) : _min(rhs.min()), _max(rhs.max()) {}

private:
	num_t _min, _max;

public:
	inline const num_t min() const {return _min;}
	inline const num_t max() const {return _max;}
	inline const bool isConstant() const {return min() == max();}
	inline const bool cross(const num_t val = 0.0) const {
		return min() <= val && max() >= val;
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
	VRA_Structured_Range() {_ranges = {nullptr};}
	VRA_Structured_Range(const generic_range_ptr_t& r) {_ranges = {r};}
	VRA_Structured_Range(const VRA_Structured_Range& rhs) : _ranges(rhs.ranges()) {}

private:
	std::vector<generic_range_ptr_t> _ranges;

public:
	inline const std::vector<generic_range_ptr_t> ranges() const {return _ranges; }

	inline bool isScalarOrArray() const {return _ranges.size() == 1;}

	inline bool isStruct() const {return _ranges.size() > 1;}

	inline generic_range_ptr_t getRangeAt(unsigned index) const {return _ranges[index];}

	inline range_ptr_t toScalarRange(unsigned index = 0) const {
		return std::static_pointer_cast<range_t>(getRangeAt(index));
	}

	inline range_s_ptr_t toStructRange(unsigned index = 0) const {
		return std::static_pointer_cast<VRA_Structured_Range>(getRangeAt(index));
	}

	inline void setRangeAt(unsigned index, const range_ptr_t& range) {
		_ranges[index] = range;
	}

};

template<class... Args>
static inline range_s_ptr_t make_s_range(Args&&... args) {
  return std::make_shared<VRA_Structured_Range>(std::forward<Args>(args)...);
}

} //end namespace

#endif /* end of include guard: TAFFO_VRA_RANGE_HPP */
