/*
** interval.h		- 'interval' numeric type
**
**	interval is a type that represents a set of numbers [a, b], such that
**	X = { x | (a <= x) && (x <= b) }. Intervals may be finite (a,b both finite),
**  semi-finite (a or b infinite), or infinite (a == -inf, b == +inf).
**
**	A scalar is a special for of interval, where a == b.
**
**	The numerics::interval type performs calculations on intervals. Each calculation
**	is performed as if the following procedure were followed:
**
**		foreach (point in interval)
**			transform point ==> point'
**		interval' = the set of all point'
**
**	For binary operators:
**
**		foreach (point1 in interval1)
**			foreach (point2 in interval2)
**				transform (point1, point2) ==> point'
**		interval' = the set of all point'
**
*/

#if !defined( BASIC_INTERVAL_RN_H__ )
#define BASIC_INTERVAL_RN_H__

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <limits>
#include <locale>
#include <sstream>
#include <string>

#ifdef INTERVAL_EXPORTS
#define BASIC_INTERVAL_API __declspec(dllexport)
#else
#define BASIC_INTERVAL_API __declspec(dllimport)
#endif

#include "intervalBase.h"

// Some compilers define isnan, isfinite, and isinf as macros, even for
// C++ codes, which cause havoc when overloading these functions.  We undef
// them here.
#ifdef isnan
#undef isnan
#endif

#ifdef isfinite
#undef isfinite
#endif

#ifdef isinf
#undef isinf
#endif

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

namespace numerics
{

	//	class interval
	//
	//	Defines a set of floating-point values that represent a range of numbers.
	//  It is assumed that the "real" value" is inside the range.
	//
	class BASIC_INTERVAL_API interval : public intervalBase
	{
	public:
		friend BASIC_INTERVAL_API interval fma(interval const& a, interval const& b, interval const& c);
		friend BASIC_INTERVAL_API interval hypot(interval const& x, interval const& y);
		friend BASIC_INTERVAL_API interval reciprocal(interval const& op);
		friend BASIC_INTERVAL_API interval sqr(interval const& op);
		friend BASIC_INTERVAL_API interval sqrt(numerics::interval const& op);

		void* operator new(std::size_t);
		void operator delete(void*);
		void* operator new[](std::size_t);
		void operator delete[](void*);

		static interval empty()					{ return intervalBase::empty(); }
		static interval entire()				{ return intervalBase::entire(); }

		//	constructors
		//
		interval();
		interval(interval const& src) = default;
		interval(double src);
		interval(double inf, double sup);

		interval(intervalBase const& src)
			: intervalBase(src)
		{
		}

		//	destructor
		//
		~interval() = default;

		//	assignment operators
		//
		interval& operator=(interval const& rhs) = default;
		interval& operator+=(interval const& rhs);
		interval& operator-=(interval const& rhs);
		interval& operator*=(interval const& rhs);
		interval& operator/=(interval const& rhs);

		interval& operator=(double rhs);
	};

	//	IEEE 1788 9.4 - Numeric functions of intervals
	//
	BASIC_INTERVAL_API inline double inf(interval const& op)
	{
		return op.inf();
	}

	BASIC_INTERVAL_API inline double sup(interval const& op)
	{
		return op.sup();
	}

	BASIC_INTERVAL_API inline double mid(interval const& op)
	{
		//	written this way so [-Inf, Inf] is handled properly
		return inf(op) == -sup(op) ? 0.0 : (inf(op) + sup(op)) / 2.0;
	}

	BASIC_INTERVAL_API inline double wid(interval const& op)
	{
		//	written this way so [-Inf, -Inf], [Inf, Inf] are handled properly
		return inf(op) == sup(op) ? 0.0 : (inf(op) - sup(op));
	}

	BASIC_INTERVAL_API inline double rad(interval const& op)
	{
		return wid(op) / 2.0;
	}

	BASIC_INTERVAL_API inline double mag(interval const& op)
	{
		return (std::max)(std::abs(inf(op)), std::abs(sup(op)));
	}

	BASIC_INTERVAL_API inline double mig(interval const& op)
	{
		return std::signbit(inf(op)) != std::signbit(sup(op)) ? 0.0 : (std::min)(std::abs(inf(op)), std::abs(sup(op)));
	}

	//	IEEE 1788 9.4 - Boolean functions of intervals
	//	IEEE 1788 10.5.10
	//	IEEE 1788 10.6.3
	//
	BASIC_INTERVAL_API inline bool isEqual(interval const& lhs, interval const& rhs)
	{
		return (inf(lhs) == inf(rhs)) && (sup(lhs) == sup(rhs));
	}

	BASIC_INTERVAL_API inline bool isLess(interval const& lhs, interval const& rhs)
	{
		return (inf(lhs) <= inf(rhs)) && (sup(lhs) <= sup(rhs));
	}

	BASIC_INTERVAL_API inline bool isStrictLess(interval const& lhs, interval const& rhs)
	{
		return ((inf(lhs) < inf(rhs)) || (std::isinf(inf(lhs)) && (inf(lhs) == inf(rhs)))) &&
			   ((sup(lhs) < sup(rhs)) || (std::isinf(sup(lhs)) && (sup(lhs) == sup(rhs))));
	}

	BASIC_INTERVAL_API inline bool isPrecedes(interval const& lhs, interval const& rhs)
	{
		return sup(lhs) <= inf(rhs);
	}

	BASIC_INTERVAL_API inline bool isStrictPrecedes(interval const& lhs, interval const& rhs)
	{
		return sup(lhs) < inf(rhs);
	}

	BASIC_INTERVAL_API inline bool isSubset(interval const& lhs, interval const& rhs)
	{
		return (inf(rhs) <= inf(lhs)) && (sup(lhs) <= sup(rhs));
	}

	BASIC_INTERVAL_API inline bool isInterior(interval const& lhs, interval const& rhs)
	{
		return (inf(rhs) < inf(lhs)) && (sup(lhs) < sup(rhs));
	}

	BASIC_INTERVAL_API inline bool isDisjoint(interval const& lhs, interval const& rhs)
	{
		return (sup(lhs) < inf(rhs)) || (sup(rhs) < inf(lhs));
	}

	BASIC_INTERVAL_API inline bool isMember(interval const& lhs, double rhs)
	{
		return !std::isinf(rhs) && (inf(lhs) <= rhs) && (rhs <= sup(lhs));
	}

	BASIC_INTERVAL_API inline bool isSingleton(interval const& op)
	{
		return !std::isinf(inf(op)) && (inf(op) == sup(op));
	}

///	INTERVAL_API inline bool isCommonInterval(interval const& op)
	
	//	IEEE 1788 9.1 - Arithmetic operations
	//

	//		Basic operations
	//
	BASIC_INTERVAL_API inline interval operator+(interval const& op)
	{
		return op;
	}

	BASIC_INTERVAL_API inline interval operator-(interval const& op)
	{
		return interval(-sup(op), -inf(op));
	}

	BASIC_INTERVAL_API inline interval operator+(interval const& lhs, interval const& rhs)
	{
		interval retval = lhs;
		return retval += rhs;
	}

	BASIC_INTERVAL_API inline interval operator-(interval const& lhs, interval const& rhs)
	{
		interval retval = lhs;
		return retval -= rhs;
	}

	BASIC_INTERVAL_API inline interval operator*(interval const& lhs, interval const& rhs)
	{
		interval retval = lhs;
		return retval *= rhs;
	}

	BASIC_INTERVAL_API inline interval operator/(interval const& lhs, interval const& rhs)
	{
		interval retval = lhs;
		return retval /= rhs;
	}

	BASIC_INTERVAL_API interval fma(interval const& op1, interval const& op2, interval const& op3);
	BASIC_INTERVAL_API interval hypot(interval const& x, interval const& y);
	BASIC_INTERVAL_API interval reciprocal(interval const& op);
	BASIC_INTERVAL_API interval sqr(interval const& op);
	BASIC_INTERVAL_API interval sqrt(interval const& op);

	//	IEEE 1788 9.3 - Set operations
	//
	BASIC_INTERVAL_API inline interval intersection(interval const& x, interval const& y)
	{
		if (!isDisjoint(x, y))
			return interval((std::max)(inf(x), inf(y)), (std::min)(sup(x), sup(y)));

		errno = EDOM;
		return std::numeric_limits<double>::quiet_NaN();
	}

	BASIC_INTERVAL_API inline interval convexHull(interval const& x, interval const& y)
	{
		return interval((std::min)(inf(x), inf(y)), (std::max)(sup(x), sup(y)));
	}

	//	IEEE 1788 x.xx - I/O
	//
	template<class _Elem, class _Tr>
	inline std::basic_istream<_Elem, _Tr>& operator>>(std::basic_istream<_Elem, _Tr>& _Istr, interval& _Right)
	{
		const ctype<_Elem>& _Ctype_fac = _USE(_Istr.getloc(), ctype<_Elem>);
		_Elem _Ch = 0;
		double _Real = 0;
		double _Imag = 0;

		if (_Istr >> _Ch && _Ch != _Ctype_fac.widen('['))
		{	// no leading '[', treat as real only
			_Istr.putback(_Ch);
			_Istr >> _Real;
			_Imag = 0;
		}
		else if (_Istr >> _Real >> _Ch && _Ch != _Ctype_fac.widen(','))
			if (_Ch == _Ctype_fac.widen(']'))
				_Imag = 0;	// (real)
			else
			{	// no trailing ')' after real, treat as bad field
				_Istr.putback(_Ch);
				_Istr.setstate(ios_base::failbit);
			}
		else if (_Istr >> _Imag >> _Ch && _Ch != _Ctype_fac.widen(']'))
		{	// no imag or trailing ']', treat as bad field
			_Istr.putback(_Ch);
			_Istr.setstate(ios_base::failbit);
		}

		if (!_Istr.fail())
		{	// store valid result
			_Right = interval(_Real, _Imag);
		}
		return (_Istr);
	}

	template<class _Elem, class _Tr>
	inline std::basic_ostream<_Elem, _Tr>& operator<<(std::basic_ostream<_Elem, _Tr>& _Ostr, const interval& _Right)
	{
		const std::ctype<_Elem>& _Ctype_fac = std::use_facet<std::ctype<_Elem> >(_Ostr.getloc());
		std::basic_ostringstream<_Elem, _Tr, std::allocator<_Elem> > _Sstr;

		_Sstr.flags(_Ostr.flags());
		_Sstr.imbue(_Ostr.getloc());
		_Sstr.precision(_Ostr.precision());
		_Sstr << _Ctype_fac.widen('[') << inf(_Right)
			<< _Ctype_fac.widen(',') << sup(_Right)
			<< _Ctype_fac.widen(']');

		std::basic_string<_Elem, _Tr, std::allocator<_Elem> > _Str = _Sstr.str();
		return (_Ostr << _Str.c_str());
	}

}

#endif
