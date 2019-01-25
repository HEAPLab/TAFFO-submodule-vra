#ifndef TAFFO_VRA_RANGE_HPP
#define TAFFO_VRA_RANGE_HPP

#include <limits>

namespace taffo {

template<
	typename num_t,
	typename = typename std::enable_if<std::is_arithmetic<num_t>::value, num_t>::type
	>
struct Range
{
public:
	Range(const num_t min, const num_t max) : _min(min), _max(max) {}
	Range() : _min(std::numeric_limits<num_t>::lowest()), _max(std::numeric_limits<num_t>::max()) {}
	Range(const Range& rhs) : _min(rhs.min()), _max(rhs.max()) {}

private:
	num_t _min, _max;

public:
	inline const num_t min() {return _min;}
	inline const num_t max() {return _max;}
};

}

#endif /* end of include guard: TAFFO_VRA_RANGE_HPP */
