/*
**	intervalError.h
**
**	Defines a pair of numbers that represent the error from the exact
**	result calculated by TwoSum(), TwoProd(), etc. This class exists
**	because the error pair does not necessarily obey the constraints
**	on an intervalBase instance - specifically, inf() < sup().
**
**	The intervalError returned from an exact operation is guaranteeed
**	to be less than or equal to 0.5 ulp of the result.
**
**	An element of an intervalError instance may be either:
**
**		0.0		- exact result, or infinite result
**		(else)	- |x| < DBL_MAX / 2^(DBL_DIGITS+1)
**
**	It is therefore evident that additions of intervalBase and intervalError
**	values can never overflow.
*/

#if !defined(NUMERICS_INTERVAL_ERROR_H__)
#define NUMERICS_INTERVAL_ERROR_H__

#ifdef INTERVAL_EXPORTS
#define BASIC_INTERVAL_API __declspec(dllexport)
#else
#define BASIC_INTERVAL_API __declspec(dllimport)
#endif

#include "SIMD.h"


namespace numerics
{

	class BASIC_INTERVAL_API intervalBase;

	class BASIC_INTERVAL_API intervalError
	{
	public:
		intervalError() = default;

		intervalError(double err_inf, double err_sup)
			: value_(simd::_mm_set_pd(err_sup, err_inf))
		{
		}

		intervalError(__m128d val)
			: value_(val)
		{
		}

		intervalError& operator=(intervalBase const& rhs);

		double err_inf() const						{ return simd::a_pd(value(), 0); }
		void err_inf(double val)					{ simd::seta_pd(value(), 0, val); }

		double err_sup() const						{ return simd::a_pd(value(), 1); }
		void err_sup(double val)					{ simd::seta_pd(value(), 1, val); }

		__m128d value() const						{ return value_; }
		__m128d& value()							{ return value_; }

	private:
		__m128d value_;
	};

}

#endif
