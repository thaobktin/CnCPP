/*
**	SIMD.cpp
**
*/

#include "stdafx.h"
#include "SIMD.h"

//#define SIMD_FAST_FMA

namespace
{

	int const BITS = (std::numeric_limits< double >::digits + 1) / 2;
	double const SPLITTER = (1lu << BITS) + 1;
	double const SCALE = 1.0 / (1lu << (BITS + 1));
	double const SCALE_INV = 1lu << (BITS + 1);
	double const SPLIT_THRESH = SCALE * (std::numeric_limits< double >::max)();
	__m128d const thresh_ = _mm_set1_pd(SPLIT_THRESH);
	__m128d const factorGT = _mm_set1_pd(SCALE);
	__m128d const inv_factorGT = _mm_set1_pd(SCALE_INV);
	__m128d const factorLTE = _mm_set1_pd(1.0);

	//	Computes high words and low words of a[0], a[1]
	//
	inline void split_pd(__m128d a_, __m128d& hi_, __m128d& lo_)
	{
		__m128d isGT = _mm_cmpgt_pd(simd::abs_pd(a_), thresh_);
		__m128d scale_ = simd::if_pd(isGT, factorGT, factorLTE);
		__m128d scale_inv_ = simd::if_pd(isGT, inv_factorGT, factorLTE);

		//	x[0] = a[0] * SCALE;
		//	x[1] = a[1] * SCALE;
		__m128d x_ = simd::_mm_mul_pd(a_, scale_);

		//	temp[0] = SPLITTER * x[0];
		//	temp[1] = SPLITTER * x[1];
		__m128d temp = simd::_mm_mul_pd(x_, _mm_set1_pd(SPLITTER));

		//	x_hi[0] = temp[0] - (temp[0] - x[0]);
		//	x_hi[1] = temp[1] - (temp[1] - x[1]);
		__m128d xHi = simd::_mm_sub_pd(temp, _mm_sub_pd(temp, x_));

		//	x_lo[0] = x[0] - x_hi[0];
		//	x_lo[1] = x[1] - x_hi[1];
		__m128d xLo = simd::_mm_sub_pd(x_, xHi);

		//	hi[0] *= SCALE_INV;
		//	hi[1] *= SCALE_INV;
		hi_ = simd::_mm_mul_pd(xHi, scale_inv_);

		//	lo[0] *= SCALE_INV;
		//	lo[1] *= SCALE_INV;
		lo_ = simd::_mm_mul_pd(xLo, scale_inv_);
	}

}

namespace simd
{

	__m128d ThreeFma_pd(__m128d a, __m128d b, __m128d c)
	{
#if defined(SIMD_FAST_FMA)
		__m128d q_, p_ = _mm_fmadd_pd(a, b, c);
		q_ = p_;
#else
		__m128d err1, err2;
		__m128d p_ = TwoSum_pd(TwoProd_pd(a, b, err1), c, err2);

		//	retval + err1 + err2 == the exact Fma() result. Normalize retval...
		__m128d q_ = TwoSum_pd(TwoSum_pd(p_, err1, err1), err2, err2);
//		err1 = TwoSum_pd(err1, err2, err2);

//		err1 = _mm_and_pd(isfinite_, err1);
//		err2 = _mm_and_pd(isfinite_, err2);
#endif
		return if_pd(isfinite_pd(p_), q_, p_);
	}

	__m128d ThreeFma_pd(__m128d a, __m128d b, __m128d c, __m128d& err1, __m128d& err2)
	{
#if defined(SIMD_FAST_FMA)
		__m128d q_, p_ = _mm_fmadd_pd(a, b, c);
		__m128d isfinite_ = isfinite_pd(p_);
		q_ = p_;
		//	calculate the error
		__m128d u2, u1 = TwoProd_pd(a, b, u2);
		__m128d a2, a1 = TwoSum_pd(c, u2, a2);
		__m128d b2, b1 = TwoSum_pd(u1, a1, b2);
		__m128d g = _mm_add_pd(_mm_sub_pd(b1, p_), b2);
		err1 = TwoSum_pd(g, a2, err2);
#else
		__m128d p_ = TwoSum_pd(TwoProd_pd(a, b, err1), c, err2);
		__m128d isfinite_ = isfinite_pd(p_);

		//	retval + err1 + err2 == the exact Fma() result. Normalize retval...
		__m128d q_ = TwoSum_pd(TwoSum_pd(p_, err1, err1), err2, err2);
		err1 = TwoSum_pd(err1, err2, err2);
#endif

		err1 = _mm_and_pd(isfinite_, err1);
		err2 = _mm_and_pd(isfinite_, err2);

		return if_pd(isfinite_, q_, p_);
	}

	__m128d ThreeFms_pd(__m128d a, __m128d b, __m128d c)
	{
#if defined(SIMD_FAST_FMA)
		__m128d q_, p_ = _mm_fmsub_pd(a, b, c);
		q_ = p_;
#else
		__m128d err1, err2;
		__m128d p_ = TwoDiff_pd(TwoProd_pd(a, b, err1), c, err2);

		//	retval + err1 + err2 == the exact Fma() result. Normalize retval...
		__m128d q_ = TwoSum_pd(TwoSum_pd(p_, err1, err1), err2, err2);
		//		err1 = TwoSum_pd(err1, err2, err2);

		//		err1 = _mm_and_pd(isfinite_, err1);
		//		err2 = _mm_and_pd(isfinite_, err2);
#endif
		return if_pd(isfinite_pd(p_), q_, p_);
	}

	__m128d ThreeFms_pd(__m128d a, __m128d b, __m128d c, __m128d& err1, __m128d& err2)
	{
#if defined(SIMD_FAST_FMA)
		__m128d q_, p_ = _mm_fmadd_pd(a, b, c);
		__m128d isfinite_ = isfinite_pd(p_);
		q_ = p_;
		//	calculate the error
		__m128d u2, u1 = TwoProd_pd(a, b, u2);
		__m128d a2, a1 = TwoDiff_pd(u2, c, a2);
		__m128d b2, b1 = TwoSum_pd(u1, a1, b2);
		__m128d g = _mm_add_pd(_mm_sub_pd(b1, p_), b2);
		err1 = TwoSum_pd(g, a2, err2);
#else
		__m128d p_ = TwoDiff_pd(TwoProd_pd(a, b, err1), c, err2);
		__m128d isfinite_ = isfinite_pd(p_);

		//	retval + err1 + err2 == the exact Fma() result. Normalize retval...
		__m128d q_ = TwoSum_pd(TwoSum_pd(p_, err1, err1), err2, err2);
		err1 = TwoSum_pd(err1, err2, err2);
#endif

		err1 = _mm_and_pd(isfinite_, err1);
		err2 = _mm_and_pd(isfinite_, err2);

		return if_pd(isfinite_, q_, p_);
	}

	//
	//	perform TwoSum_pd(a, -b, err)
	//
	__m128d TwoProd_pd(__m128d a_, __m128d b_, __m128d& err_)
	{
		// p[0] = inf() * inf()		- positive, +Inf
		// p[1] = sup() * sup()		- positive, +Inf
		__m128d p_ = _mm_mul_pd(a_, b_);
		__m128d isfinite_ = isfinite_pd(p_);

#if defined(SIMD_FAST_FMA)
		// e[0] = fms(op.inf(), op.inf(), p[0]);
		// e[1] = fms(op.sup(), op.sup(), p[1]);
		__m128d e_ = _mm_fmsub_pd(a_, b_, p_);
#else
		__m128d a_lo_;
		__m128d a_hi_;
		split_pd(a_, a_hi_, a_lo_);
		__m128d b_lo_;
		__m128d b_hi_;
		split_pd(b_, b_hi_, b_lo_);

		//	e[0] = ((a_hi[0] * b_hi[0] - p[0]) + a_hi[0] * b_lo[0] + a_lo[0] * b_hi[0]) + a_lo[0] * b_lo[0];
		//	e[1] = ((a_hi[1] * b_hi[1] - p[1]) + a_hi[1] * b_lo[1] + a_lo[1] * b_hi[1]) + a_lo[1] * b_lo[1];
		__m128d e_ = _mm_sub_pd(_mm_mul_pd(a_hi_, b_hi_), p_);
		e_ = _mm_add_pd(e_, _mm_mul_pd(a_hi_, b_lo_));
		e_ = _mm_add_pd(e_, _mm_mul_pd(a_lo_, b_hi_));
		e_ = _mm_add_pd(e_, _mm_mul_pd(a_lo_, b_lo_));
#endif
		err_ = _mm_and_pd(e_, isfinite_);

		return p_;
	}

	__m128d TwoSqr_pd(__m128d op_, __m128d& err_)
	{
		// p[0] = inf() * inf()		- positive, +Inf
		// p[1] = sup() * sup()		- positive, +Inf
		__m128d p_ = _mm_mul_pd(op_, op_);
		__m128d isfinite_ = isfinite_pd(p_);

#if defined(SIMD_FAST_FMA)
		// e[0] = ::fma(op.inf(), op.inf(), -p[0]);
		// e[1] = ::fma(op.sup(), op.sup(), -p[1]);
		__m128d e_ = _mm_fmsub_pd(op_, op_, p_);
#else
		__m128d lo_;
		__m128d hi_;
		split_pd(op_, hi_, lo_);

		//	e[0] = ((lhs_hi[0] * lhs_hi[0] - p[0]) + lhs_hi[0] * lhs_lo[0] + lhs_lo[0] * lhs_hi[0]) + lhs_lo[0] * lhs_lo[0];
		//	e[1] = ((lhs_hi[1] * lhs_hi[1] - p[1]) + lhs_hi[1] * lhs_lo[1] + lhs_lo[1] * lhs_hi[1]) + lhs_lo[1] * lhs_lo[1];
		__m128d e_ = _mm_sub_pd(_mm_mul_pd(hi_, hi_), p_);
		e_ = _mm_add_pd(e_, _mm_mul_pd(_mm_set1_pd(2.0), _mm_mul_pd(hi_, lo_)));
		e_ = _mm_add_pd(e_, _mm_mul_pd(lo_, lo_));
#endif
		err_ = _mm_and_pd(isfinite_, e_);

		return p_;
	}

	__m128d Sum_pd(std::size_t len, __m128d* vec, __m128d& err)
	{
		__m128d sum = _mm_setzero_pd();
		__m128d error = _mm_setzero_pd();

		//	summate vec[i], leaving an error in vec[i]
		for (auto i = 0u; i < len; i++)
		{
			sum = TwoSum_pd(sum, vec[i], vec[i]);
		}

		//	the exact sum is sum+(sum of all errors)
		for (auto i = 1u; i < len; i++)
		{
			error = TwoSum_pd(error, vec[i], vec[i]);
		}

		sum = TwoSum_pd(sum, error, error);
		for (auto i = 2u; i < len; i++)
		{
			error = _mm_add_pd(error, vec[i]);
		}

		err = error;
		return sum;
	}

	__m128d SumAbs_pd(std::size_t len, __m128d* vec, __m128d& err)
	{
		__m128d sum = _mm_setzero_pd();
		__m128d error = _mm_setzero_pd();

		//	summate vec[i], leaving an error in vec[i]
		for (auto i = 0u; i < len; i++)
		{
			sum = TwoSum_pd(sum, vec[i], vec[i]);
		}

		//	the exact sum is sum+(sum of all errors)
		for (auto i = 1u; i < len; i++)
		{
			error = TwoSum_pd(error, vec[i], vec[i]);
		}

		sum = TwoSum_pd(sum, error, error);
		for (auto i = 2u; i < len; i++)
		{
			error = _mm_add_pd(error, vec[i]);
		}

		err = error;
		return sum;
	}

}
