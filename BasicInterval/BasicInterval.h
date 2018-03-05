/*
** BasicInterval.h		- 'BasicInterval' numeric type
**
**	An BasicInterval is a type that represents a set of numbers [a, b], such that
**	X = { x | (a <= x) && (x <= b) }. Intervals may be finite (a,b both finite),
**  semi-finite (a or b infinite), or infinite (a == -inf, b == +inf).
**
**	A scalar is a special for of BasicInterval, where a == b.
**
**	The numerics::BasicInterval type performs calculations on BasicIntervals. Each calculation
**	is performed as if the following procedure were followed:
**
**		foreach (point in numerics::BasicInterval)
**			transform point ==> point'
**		BasicInterval' = the set of all point'
**
**	For binary operators:
**
**		foreach (point1 in BasicInterval1)
**			foreach (point2 in BasicInterval2)
**				transform (point1, point2) ==> point'
**		BasicInterval' = the set of all point'
**
**
**	A 'double' value is treated as a closed numerics::BasicInterval [d, d].
*/

#if !defined( BASIC_INTERVAL_H__ )
#define BASIC_INTERVAL_H__

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

	//	class BasicInterval
	//
	//	Defines a set of floating-point values that represent a range of numbers.
	//  It is assumed that the "real" value" is inside the range.
	//
	class BASIC_INTERVAL_API BasicInterval
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

		friend BASIC_INTERVAL_API BasicInterval fma(BasicInterval const& op1, BasicInterval const& op2, BasicInterval const& op3);
		friend BASIC_INTERVAL_API BasicInterval reciprocal(BasicInterval const& op);
		friend BASIC_INTERVAL_API BasicInterval sqr(BasicInterval const& op);
		friend BASIC_INTERVAL_API BasicInterval sqrt(BasicInterval const& op);

		void* operator new(std::size_t);
		void operator delete(void*);
		void* operator new[](std::size_t);
		void operator delete[](void*);

		static BasicInterval const& empty()				{ return s_Empty; }
		static BasicInterval const& entire()			{ return s_Entire; }

		//	constructors
		//
		BasicInterval();

		BasicInterval(BasicInterval const& src) = default;

		BasicInterval(double src);
		BasicInterval(double inf, double sup);

		//	destructor
		//
		~BasicInterval() = default;

		//	assignment operators
		//
		BasicInterval& operator=(BasicInterval const& rhs) = default;

		BasicInterval& operator=(double rhs)
		{
			m_x[0] = rhs;
			m_x[1] = rhs;
			return *this;
		}

		BasicInterval& operator+=(BasicInterval const& rhs);
		BasicInterval& operator-=(BasicInterval const& rhs);
		BasicInterval& operator*=(BasicInterval const& rhs);
		BasicInterval& operator/=(BasicInterval const& rhs);

		//	accessors
		//
		double inf() const
		{
			return m_x[0];
		}

		double sup() const
		{
			return m_x[1];
		}

		bool isEmpty() const
		{
			return std::isnan(inf()) || std::isnan(sup());
		}

		CLASS classify() const
		{
			return isEmpty() ? CLASS_EMPTY : classify(inf(), sup());
		}

	private:
		static BasicInterval const s_Empty;
		static BasicInterval const s_Entire;

		__declspec(align(16)) double m_x[2];

		static BasicInterval reciprocal(BasicInterval const& op);
		static BasicInterval sqr(BasicInterval const& op);
		static BasicInterval sqrt(BasicInterval const& op);

		static CLASS classify(double inf, double sup)
		{
			if (inf == 0.0)
				return sup == 0.0 ? CLASS_ZERO : CLASS_P0;
			if (sup == 0.0)
				return CLASS_N0;
			if (inf > 0.0)
				return CLASS_P1;
			if (sup < 0.0)
				return CLASS_N1;

			return CLASS_M;
		}

	};

	//	IEEE 1788 9.4 - Numeric functions of BasicIntervals
	//
	BASIC_INTERVAL_API inline double inf(BasicInterval const& op)
	{
		return op.inf();
	}

	BASIC_INTERVAL_API inline double sup(BasicInterval const& op)
	{
		return op.sup();
	}

	BASIC_INTERVAL_API inline double mid(BasicInterval const& op)
	{
		//	written this way so [-Inf, Inf] is handled properly
		return inf(op) == -sup(op) ? 0.0 : (inf(op) + sup(op)) / 2.0;
	}

	BASIC_INTERVAL_API inline double wid(BasicInterval const& op)
	{
		//	written this way so [-Inf, -Inf], [Inf, Inf] are handled properly
		return inf(op) == sup(op) ? 0.0 : (inf(op) - sup(op));
	}

	BASIC_INTERVAL_API inline double rad(BasicInterval const& op)
	{
		return wid(op) / 2.0;
	}

	BASIC_INTERVAL_API inline double mag(BasicInterval const& op)
	{
		return (std::max)(std::abs(inf(op)), std::abs(sup(op)));
	}

	BASIC_INTERVAL_API inline double mig(BasicInterval const& op)
	{
		return std::signbit(inf(op)) != std::signbit(sup(op)) ? 0.0 : (std::min)(std::abs(inf(op)), std::abs(sup(op)));
	}

	//	IEEE 1788 9.4 - Boolean functions of BasicIntervals
	//	IEEE 1788 10.5.10
	//	IEEE 1788 10.6.3
	//
	BASIC_INTERVAL_API inline bool isEqual(BasicInterval const& lhs, BasicInterval const& rhs)
	{
		return (inf(lhs) == inf(rhs)) && (sup(lhs) == sup(rhs));
	}

	BASIC_INTERVAL_API inline bool isLess(BasicInterval const& lhs, BasicInterval const& rhs)
	{
		return (inf(lhs) <= inf(rhs)) && (sup(lhs) <= sup(rhs));
	}

	BASIC_INTERVAL_API inline bool isStrictLess(BasicInterval const& lhs, BasicInterval const& rhs)
	{
		return ((inf(lhs) < inf(rhs)) || (std::isinf(inf(lhs)) && (inf(lhs) == inf(rhs)))) &&
			   ((sup(lhs) < sup(rhs)) || (std::isinf(sup(lhs)) && (sup(lhs) == sup(rhs))));
	}

	BASIC_INTERVAL_API inline bool isPrecedes(BasicInterval const& lhs, BasicInterval const& rhs)
	{
		return sup(lhs) <= inf(rhs);
	}

	BASIC_INTERVAL_API inline bool isStrictPrecedes(BasicInterval const& lhs, BasicInterval const& rhs)
	{
		return sup(lhs) < inf(rhs);
	}

	BASIC_INTERVAL_API inline bool isSubset(BasicInterval const& lhs, BasicInterval const& rhs)
	{
		return (inf(rhs) <= inf(lhs)) && (sup(lhs) <= sup(rhs));
	}

	BASIC_INTERVAL_API inline bool isInterior(BasicInterval const& lhs, BasicInterval const& rhs)
	{
		return (inf(rhs) < inf(lhs)) && (sup(lhs) < sup(rhs));
	}

	BASIC_INTERVAL_API inline bool isDisjoint(BasicInterval const& lhs, BasicInterval const& rhs)
	{
		return (sup(lhs) < inf(rhs)) || (sup(rhs) < inf(lhs));
	}

	BASIC_INTERVAL_API inline bool isMember(BasicInterval const& lhs, double rhs)
	{
		return !std::isinf(rhs) && (inf(lhs) <= rhs) && (rhs <= sup(lhs));
	}

	BASIC_INTERVAL_API inline bool isSingleton(BasicInterval const& op)
	{
		return !std::isinf(inf(op)) && (inf(op) == sup(op));
	}

///	INTERVAL_API inline bool isCommonInterval(BasicInterval const& op)
	
	//	IEEE 1788 9.1 - Arithmetic operations
	//

	//		Basic operations
	//
	BASIC_INTERVAL_API inline BasicInterval operator+(BasicInterval const& op)
	{
		return op;
	}

	BASIC_INTERVAL_API inline BasicInterval operator-(BasicInterval const& op)
	{
		return BasicInterval(-sup(op), -inf(op));
	}

	BASIC_INTERVAL_API inline BasicInterval operator+(BasicInterval const& lhs, BasicInterval const& rhs)
	{
		BasicInterval retval = lhs;
		return retval += rhs;
	}

	BASIC_INTERVAL_API inline BasicInterval operator-(BasicInterval const& lhs, BasicInterval const& rhs)
	{
		BasicInterval retval = lhs;
		return retval -= rhs;
	}

	BASIC_INTERVAL_API inline BasicInterval operator*(BasicInterval const& lhs, BasicInterval const& rhs)
	{
		BasicInterval retval = lhs;
		return retval *= rhs;
	}

	BASIC_INTERVAL_API inline BasicInterval operator/(BasicInterval const& lhs, BasicInterval const& rhs)
	{
		BasicInterval retval = lhs;
		return retval /= rhs;
	}

	BASIC_INTERVAL_API BasicInterval fma(BasicInterval const& op1, BasicInterval const& op2, BasicInterval const& op3);
	BASIC_INTERVAL_API BasicInterval reciprocal(BasicInterval const& op);
	BASIC_INTERVAL_API BasicInterval sqr(BasicInterval const& op);
	BASIC_INTERVAL_API BasicInterval sqrt(BasicInterval const& op);

	//	IEEE 1788 9.3 - Set operations
	//
	BASIC_INTERVAL_API inline BasicInterval intersection(BasicInterval const& x, BasicInterval const& y)
	{
		if (!isDisjoint(x, y))
			return BasicInterval((std::max)(inf(x), inf(y)), (std::min)(sup(x), sup(y)));

		errno = EDOM;
		return std::numeric_limits<double>::quiet_NaN();
	}

	BASIC_INTERVAL_API inline BasicInterval convexHull(BasicInterval const& x, BasicInterval const& y)
	{
		return BasicInterval((std::min)(inf(x), inf(y)), (std::max)(sup(x), sup(y)));
	}

	//	IEEE 1788 x.xx - I/O
	//
	template<class _Elem, class _Tr>
	inline std::basic_istream<_Elem, _Tr>& operator>>(std::basic_istream<_Elem, _Tr>& _Istr, BasicInterval& _Right)
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
	inline std::basic_ostream<_Elem, _Tr>& operator<<(std::basic_ostream<_Elem, _Tr>& _Ostr, const BasicInterval& _Right)
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
	BASIC_INTERVAL_API inline bool isEmpty(BasicInterval const& op)
	{
		return op.isEmpty();
	}

}

#endif
