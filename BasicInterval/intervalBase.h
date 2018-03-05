#pragma once

#include <cmath>
#include <limits>

#ifdef INTERVAL_EXPORTS
#define BASIC_INTERVAL_API __declspec(dllexport)
#else
#define BASIC_INTERVAL_API __declspec(dllimport)
#endif

#include "intervalError.h"
#include "SIMD.h"


namespace numerics
{

	class BASIC_INTERVAL_API intervalBase;

	enum INTERVAL_CLASS
	{
		INTERVAL_CLASS_EMPTY = -1,
		INTERVAL_CLASS_ZERO,
		INTERVAL_CLASS_M,
		INTERVAL_CLASS_P1,
		INTERVAL_CLASS_P0,
		INTERVAL_CLASS_N1,
		INTERVAL_CLASS_N0,
		INTERVAL_CLASS_COUNT
	};

	class BASIC_INTERVAL_API intervalBase
	{
		friend BASIC_INTERVAL_API intervalBase operator-(intervalBase const& a);

	public:
		//	Returns three results as follows:
		//		1.	RN(a * b + c)
		//		2.	the exact error - err1 + err2
		//
		static intervalBase ThreeFma(intervalBase const& a, intervalBase const& b, intervalBase const& c, intervalError& err1, intervalError& err2);

		//	Returns three results as follows:
		//		1.	RN(a * b - c)
		//		2.	the exact error - err1 + err2
		//
		static intervalBase ThreeFms(intervalBase const& a, intervalBase const& b, intervalBase const& c, intervalError& err1, intervalError& err2);

		//	Returns two results as follows:
		//		1.	RN(a - b)
		//		2.	the exact error - err
		//
		static intervalBase TwoDiff(intervalBase const& a, intervalBase const& b, intervalError& err);

		//	Returns two results as follows:
		//		1.	RN(a * b)
		//		2.	the exact error - err
		//
		static intervalBase TwoProd(intervalBase const& a, intervalBase const& b, intervalError& err);

		//	Returns two results as follows:
		//		1.	RN(op^2)
		//		2.	the exact error - err
		//
		static intervalBase TwoSqr(intervalBase const& op, intervalError& err);

		//	Returns two results as follows:
		//		1.	RN(a + b)
		//		2.	the exact error - err
		//
		static intervalBase TwoSum(intervalBase const& a, intervalBase const& b, intervalError& err);

		//	Returns two results as follows:
		//		1.	RN(a / b)
		//		2.	the residual of a - (retval) * b
		//
		static intervalBase Divide(intervalBase const& a, intervalBase const& b, intervalError& residual);

		//	Returns two results as follows:
		//		1.	RN(1.0 / op)
		//		2.	the residual of 1.0 - (retval) * op
		//
		static intervalBase Reciprocal(intervalBase const& op, intervalError& residual);

		//	Returns two results as follows:
		//		1.	RN(sqrt(op))
		//		2.	the residual of op - (retval)^2
		//
		static intervalBase Sqrt(intervalBase const& op, intervalError& residual);

		intervalBase(__m128d val)
			: value_(val)
		{
		}

		//	Accessors
		//
		double inf() const								{ return simd::a_pd(value_, 0); }
		double sup() const								{ return simd::a_pd(value_, 1); }

		__m128d value() const							{ return value_; }

		//	adjusters
		//
		void adjust(intervalError const& err);

		bool isEmpty() const
		{
			return simd::_mm_movemask_pd(simd::isnan_pd(value_)) != 0;
		}

		INTERVAL_CLASS classify() const;

	protected:
		static intervalBase const& empty()				{ return s_Empty; }
		static intervalBase const& entire()				{ return s_Entire; }
		static intervalBase const& zero()				{ return s_Zero; }

		intervalBase()
		{
		}

//		intervalBase(intervalBase const&) = default;
//		~intervalBase() = default;

//		intervalBase& operator=(intervalBase const&) = default;

		intervalBase(double inf, double sup)
			: value_(simd::_mm_set_pd(sup, inf))
		{
		}

		//	Accessors
		//
		void inf(double val)							{ simd::seta_pd(value_, 0, val); }
		void sup(double val)							{ simd::seta_pd(value_, 1, val); }

		__m128d& value()								{ return value_; }

	private:
		static intervalBase const s_Empty;
		static intervalBase const s_Entire;
		static intervalBase const s_Zero;

		static intervalBase TwoSum(intervalBase const& a, intervalError const& b, intervalError& err);
		static intervalBase TwoSum(intervalError const& a, intervalError const& b, intervalError& err);

		__m128d value_;
	};

	BASIC_INTERVAL_API inline intervalBase operator-(intervalBase const& a)
	{
		return intervalBase(-a.sup(), -a.inf());
	}
}
