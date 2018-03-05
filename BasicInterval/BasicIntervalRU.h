/*
** BasicIntervalRU.h		- 'BasicIntervalRU' numeric type
**
**	BasicIntervalRU is a type that represents a set of numbers [a, b], such that
**	X = { x | (a <= x) && (x <= b) }. Intervals may be finite (a,b both finite),
**  semi-finite (a or b infinite), or infinite (a == -inf, b == +inf).
**
**	A scalar is a special form of BasicIntervalRU, where a == b.
**
**	The BasicIntervalRU type performs calculations on BasicIntervalRUs. Each calculation
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

#if !defined( BASIC_INTERVAL_RU_H__ )
#define BASIC_INTERVAL_RU_H__

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

#include "SIMD.h"

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

	//	class BasicIntervalRU
	//
	//	Defines a set of floating-point values that represent a range of numbers.
	//  It is assumed that the "real" value" is inside the range.
	//
	class BASIC_INTERVAL_API BasicIntervalRU
	{
	public:
		enum CLASS {
			CLASS_EMPTY = -1,
			CLASS_ZERO,
			CLASS_M,
			CLASS_P1,
			CLASS_P0,
			CLASS_N1,
			CLASS_N0,
			CLASS_COUNT
		};

		friend BASIC_INTERVAL_API BasicIntervalRU fma(BasicIntervalRU const& op1, BasicIntervalRU const& op2, BasicIntervalRU const& op3);
		friend BASIC_INTERVAL_API BasicIntervalRU reciprocal(BasicIntervalRU const& op);
		friend BASIC_INTERVAL_API BasicIntervalRU sqr(BasicIntervalRU const& op);
		friend BASIC_INTERVAL_API BasicIntervalRU sqrt(BasicIntervalRU const& op);

		void* operator new(std::size_t);
		void operator delete(void*);
		void* operator new[](std::size_t);
		void operator delete[](void*);

		static BasicIntervalRU const& empty()			{ return s_Empty; }
		static BasicIntervalRU const& entire()			{ return s_Entire; }

		//	constructors
		//
		BasicIntervalRU();
		BasicIntervalRU(BasicIntervalRU const& src) = default;

		BasicIntervalRU(double src);
		BasicIntervalRU(double inf, double sup);

		//	destructor
		//
		~BasicIntervalRU() = default;

		//	assignment operators
		//
		BasicIntervalRU& operator=(BasicIntervalRU const& rhs) = default;

		BasicIntervalRU& operator=(double rhs)
		{
			simd::_mm_set_pd(rhs, -rhs);
			return *this;
		}

		BasicIntervalRU& operator+=(BasicIntervalRU const& rhs);
		BasicIntervalRU& operator-=(BasicIntervalRU const& rhs);
		BasicIntervalRU& operator*=(BasicIntervalRU const& rhs);
		BasicIntervalRU& operator/=(BasicIntervalRU const& rhs);

		//	accessors
		//
		double inf() const
		{
			return -simd::a_pd(value_, 0);
		}

		double sup() const
		{
			return simd::a_pd(value_, 1);
		}

		bool isEmpty() const
		{
			return std::isnan(inf()) || std::isnan(sup());
		}

		CLASS classify() const;

	private:
		static BasicIntervalRU reciprocal(BasicIntervalRU const& op);
		static BasicIntervalRU sqr(BasicIntervalRU const& op);
		static BasicIntervalRU sqrt(BasicIntervalRU const& op);

		BasicIntervalRU(__m128d src)
			: value_(src)
		{
		}

		static BasicIntervalRU const s_Empty;
		static BasicIntervalRU const s_Entire;

		__m128d value_;
	};

	//	IEEE 1788 9.4 - Numeric functions of BasicIntervalRUs
	//
	BASIC_INTERVAL_API inline double inf(BasicIntervalRU const& op)
	{
		return op.inf();
	}

	BASIC_INTERVAL_API inline double sup(BasicIntervalRU const& op)
	{
		return op.sup();
	}

	BASIC_INTERVAL_API inline double mid(BasicIntervalRU const& op)
	{
		//	written this way so [-Inf, Inf] is handled properly
		return inf(op) == -sup(op) ? 0.0 : (inf(op) + sup(op)) / 2.0;
	}

	BASIC_INTERVAL_API inline double wid(BasicIntervalRU const& op)
	{
		//	written this way so [-Inf, -Inf], [Inf, Inf] are handled properly
		return inf(op) == sup(op) ? 0.0 : (inf(op) - sup(op));
	}

	BASIC_INTERVAL_API inline double rad(BasicIntervalRU const& op)
	{
		return wid(op) / 2.0;
	}

	BASIC_INTERVAL_API inline double mag(BasicIntervalRU const& op)
	{
		return (std::max)(std::abs(inf(op)), std::abs(sup(op)));
	}

	BASIC_INTERVAL_API inline double mig(BasicIntervalRU const& op)
	{
		return std::signbit(inf(op)) != std::signbit(sup(op)) ? 0.0 : (std::min)(std::abs(inf(op)), std::abs(sup(op)));
	}

	//	IEEE 1788 9.4 - Boolean functions of BasicIntervalRUs
	//	IEEE 1788 10.5.10
	//	IEEE 1788 10.6.3
	//
	BASIC_INTERVAL_API inline bool isEqual(BasicIntervalRU const& lhs, BasicIntervalRU const& rhs)
	{
		return (inf(lhs) == inf(rhs)) && (sup(lhs) == sup(rhs));
	}

	BASIC_INTERVAL_API inline bool isLess(BasicIntervalRU const& lhs, BasicIntervalRU const& rhs)
	{
		return (inf(lhs) <= inf(rhs)) && (sup(lhs) <= sup(rhs));
	}

	BASIC_INTERVAL_API inline bool isStrictLess(BasicIntervalRU const& lhs, BasicIntervalRU const& rhs)
	{
		return ((inf(lhs) < inf(rhs)) || (std::isinf(inf(lhs)) && (inf(lhs) == inf(rhs)))) &&
			   ((sup(lhs) < sup(rhs)) || (std::isinf(sup(lhs)) && (sup(lhs) == sup(rhs))));
	}

	BASIC_INTERVAL_API inline bool isPrecedes(BasicIntervalRU const& lhs, BasicIntervalRU const& rhs)
	{
		return sup(lhs) <= inf(rhs);
	}

	BASIC_INTERVAL_API inline bool isStrictPrecedes(BasicIntervalRU const& lhs, BasicIntervalRU const& rhs)
	{
		return sup(lhs) < inf(rhs);
	}

	BASIC_INTERVAL_API inline bool isSubset(BasicIntervalRU const& lhs, BasicIntervalRU const& rhs)
	{
		return (inf(rhs) <= inf(lhs)) && (sup(lhs) <= sup(rhs));
	}

	BASIC_INTERVAL_API inline bool isInterior(BasicIntervalRU const& lhs, BasicIntervalRU const& rhs)
	{
		return (inf(rhs) < inf(lhs)) && (sup(lhs) < sup(rhs));
	}

	BASIC_INTERVAL_API inline bool isDisjoint(BasicIntervalRU const& lhs, BasicIntervalRU const& rhs)
	{
		return (sup(lhs) < inf(rhs)) || (sup(rhs) < inf(lhs));
	}

	BASIC_INTERVAL_API inline bool isMember(BasicIntervalRU const& lhs, double rhs)
	{
		return !std::isinf(rhs) && (inf(lhs) <= rhs) && (rhs <= sup(lhs));
	}

	BASIC_INTERVAL_API inline bool isSingleton(BasicIntervalRU const& op)
	{
		return !std::isinf(inf(op)) && (inf(op) == sup(op));
	}

///	INTERVAL_API inline bool isCommonInterval(BasicIntervalRU const& op)
	
	//	IEEE 1788 9.1 - Arithmetic operations
	//

	//		Basic operations
	//
	BASIC_INTERVAL_API inline BasicIntervalRU operator+(BasicIntervalRU const& op)
	{
		return op;
	}

	BASIC_INTERVAL_API inline BasicIntervalRU operator-(BasicIntervalRU const& op)
	{
		return BasicIntervalRU(-sup(op), -inf(op));
	}

	BASIC_INTERVAL_API inline BasicIntervalRU operator+(BasicIntervalRU const& lhs, BasicIntervalRU const& rhs)
	{
		BasicIntervalRU retval = lhs;
		return retval += rhs;
	}

	BASIC_INTERVAL_API inline BasicIntervalRU operator-(BasicIntervalRU const& lhs, BasicIntervalRU const& rhs)
	{
		BasicIntervalRU retval = lhs;
		return retval -= rhs;
	}

	BASIC_INTERVAL_API inline BasicIntervalRU operator*(BasicIntervalRU const& lhs, BasicIntervalRU const& rhs)
	{
		BasicIntervalRU retval = lhs;
		return retval *= rhs;
	}

	BASIC_INTERVAL_API inline BasicIntervalRU operator/(BasicIntervalRU const& lhs, BasicIntervalRU const& rhs)
	{
		BasicIntervalRU retval = lhs;
		return retval /= rhs;
	}

	BASIC_INTERVAL_API BasicIntervalRU fma(BasicIntervalRU const& op1, BasicIntervalRU const& op2, BasicIntervalRU const& op3);
	BASIC_INTERVAL_API BasicIntervalRU reciprocal(BasicIntervalRU const& op);
	BASIC_INTERVAL_API BasicIntervalRU sqr(BasicIntervalRU const& op);
	BASIC_INTERVAL_API BasicIntervalRU sqrt(BasicIntervalRU const& op);

	//	IEEE 1788 9.3 - Set operations
	//
	BASIC_INTERVAL_API inline BasicIntervalRU intersection(BasicIntervalRU const& x, BasicIntervalRU const& y)
	{
		if (!isDisjoint(x, y))
			return BasicIntervalRU((std::max)(inf(x), inf(y)), (std::min)(sup(x), sup(y)));

		errno = EDOM;
		return std::numeric_limits<double>::quiet_NaN();
	}

	BASIC_INTERVAL_API inline BasicIntervalRU convexHull(BasicIntervalRU const& x, BasicIntervalRU const& y)
	{
		return BasicIntervalRU((std::min)(inf(x), inf(y)), (std::max)(sup(x), sup(y)));
	}

	//	IEEE 1788 x.xx - I/O
	//
	template<class _Elem, class _Tr>
	inline std::basic_istream<_Elem, _Tr>& operator>>(std::basic_istream<_Elem, _Tr>& _Istr, BasicIntervalRU& _Right)
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
			_Right = BasicInterval(_Real, _Imag);
		}
		return (_Istr);
	}

	template<class _Elem, class _Tr>
	inline std::basic_ostream<_Elem, _Tr>& operator<<(std::basic_ostream<_Elem, _Tr>& _Ostr, const BasicIntervalRU& _Right)
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

	//	non-Standard operations
	//
	BASIC_INTERVAL_API inline bool isNaI(BasicIntervalRU const& op)
	{
		return std::isnan(inf(op)) || std::isnan(sup(op));
	}

}

#endif
