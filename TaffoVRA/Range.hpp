#ifndef TAFFO_VRA_RANGE_HPP
#define TAFFO_VRA_RANGE_HPP

#include <limits>
#include <memory>

namespace taffo {

// TODO rename into VRA_range
template<
	typename num_t,
	typename = typename std::enable_if<std::is_arithmetic<num_t>::value, num_t>::type
	>
struct VRA_Range
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
using range_ptr_t = std::shared_ptr<VRA_Range<num_t>>;
template<class... Args>
static inline range_ptr_t make_range(Args&&... args) {
  return std::make_shared<VRA_Range<num_t>>(std::forward<Args>(args)...);
}
}

#endif /* end of include guard: TAFFO_VRA_RANGE_HPP */
