/*
**	intervalBase.cpp
**
*/

#include "stdafx.h"
#include "intervalBase.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>

#include <emmintrin.h>


namespace
{

	__m128d const phi_pd = _mm_set1_pd(0.5*std::numeric_limits<double>::epsilon()*(1.0 + std::numeric_limits<double>::epsilon()));
	__m128d const eta_pd = _mm_set1_pd(std::numeric_limits<double>::denorm_min());
	__m128d const nzero_pd = _mm_set1_pd(-0.0);

	inline __m128d abs_pd(__m128d x)
	{
		return _mm_andnot_pd(nzero_pd, x);
	}

	inline __m128d nextafter_pd(__m128d x)
	{
		__m128d e = _mm_add_pd(_mm_mul_pd(phi_pd, abs_pd(x)), eta_pd);
		return _mm_add_pd(x, e);
	}

	inline __m128d if_pd(__m128d mask, __m128d trueval, __m128d falseval)
	{
		__m128d true_ = _mm_and_pd(mask, trueval);
		__m128d false_ = _mm_andnot_pd(mask, falseval);
		return _mm_or_pd(true_, false_);
	}
}


namespace numerics
{

	intervalBase const intervalBase::s_Empty(std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN());
	intervalBase const intervalBase::s_Entire(-std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
	intervalBase const intervalBase::s_Zero(-0.0, 0.0);

	void intervalBase::adjust(intervalError const& err)
	{
		__m128d static const pn_one = _mm_set_pd(1.0, -1.0);

		__m128d pn_x = _mm_mul_pd(pn_one, value());
		__m128d mask = _mm_cmpgt_pd(_mm_mul_pd(pn_one, err.value()), _mm_setzero_pd());
		__m128d next = nextafter_pd(pn_x);
		pn_x = simd::if_pd(mask, next, pn_x);

		value() = _mm_mul_pd(pn_one, pn_x);
	}

	intervalBase intervalBase::ThreeFma(intervalBase const& a, intervalBase const& b, intervalBase const& c, intervalError& err1, intervalError& err2)
	{
		return simd::ThreeFma_pd(a.value(), b.value(), c.value(), err1.value(), err2.value());
	}

	intervalBase intervalBase::ThreeFms(intervalBase const& a, intervalBase const& b, intervalBase const& c, intervalError& err1, intervalError& err2)
	{
		return simd::ThreeFms_pd(a.value(), b.value(), c.value(), err1.value(), err2.value());
	}

	intervalBase intervalBase::TwoDiff(intervalBase const& a, intervalBase const& b, intervalError& err)
	{
		auto s = simd::TwoDiff_pd(a.value(), b.value(), err.value());
		auto cls = simd::fpclassify_pd(s);

		//	adjust p[0], p[1] so the zero convention is maintained
		//

		//	[0, 0] - return a canonical zero
		if ((cls.first == FP_ZERO) && (cls.second == FP_ZERO))
			return zero();

		//	[0, b]	- return [+0, b]
		if (cls.first == FP_ZERO)
			return simd::abs_pd(s);

		//	[a, 0]	- return [a, -0]
		if (cls.second == FP_ZERO)
			return simd::chgsign_pd(simd::abs_pd(s));

		//	return [a, b]
		return s;
	}

	intervalBase intervalBase::TwoProd(intervalBase const& a, intervalBase const& b, intervalError& err)
	{
		intervalBase retval;
		INTERVAL_CLASS aClass = a.classify();
		INTERVAL_CLASS bClass = b.classify();

		switch (INTERVAL_CLASS_COUNT * aClass + bClass)
		{
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_ZERO + INTERVAL_CLASS_ZERO:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_ZERO + INTERVAL_CLASS_P1:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_ZERO + INTERVAL_CLASS_P0:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_ZERO + INTERVAL_CLASS_M:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_ZERO + INTERVAL_CLASS_N0:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_ZERO + INTERVAL_CLASS_N1:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_P1 + INTERVAL_CLASS_ZERO:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_P0 + INTERVAL_CLASS_ZERO:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_M + INTERVAL_CLASS_ZERO:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_N0 + INTERVAL_CLASS_ZERO:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_N1 + INTERVAL_CLASS_ZERO:
				err = zero();
				return zero();

			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_P1 + INTERVAL_CLASS_P1:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_P1 + INTERVAL_CLASS_P0:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_P0 + INTERVAL_CLASS_P1:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_P0 + INTERVAL_CLASS_P0:
				retval = simd::TwoProd_pd(a.value(), b.value(), err.value());
				break;

			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_M + INTERVAL_CLASS_P1:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_M + INTERVAL_CLASS_P0:
				retval = simd::TwoProd_pd(a.value(), simd::_mm_set1_pd(b.sup()), err.value());
				break;

			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_N1 + INTERVAL_CLASS_P1:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_N1 + INTERVAL_CLASS_P0:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_N0 + INTERVAL_CLASS_P1:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_N0 + INTERVAL_CLASS_P0:
				retval = simd::TwoProd_pd(a.value(), simd::swap_pd(b.value()), err.value());
				break;

			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_P1 + INTERVAL_CLASS_M:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_P0 + INTERVAL_CLASS_M:
				retval = simd::TwoProd_pd(_mm_set1_pd(a.sup()), b.value(), err.value());
				break;

			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_M + INTERVAL_CLASS_M:
			{
				__m128d e;
				__m128d p;
				__declspec(align(16)) double r_[2];
				__declspec(align(16)) double e_[2];
				__declspec(align(16)) double p_[2];
				__declspec(align(16)) double q_[2];

				p = simd::TwoProd_pd(a.value(), simd::swap_pd(b.value()), e);
				simd::_mm_store_pd(p_, p);
				simd::_mm_store_pd(q_, e);
				if ((p_[0] < p_[1]) || ((p_[0] == p_[1]) && (q_[0] < q_[1])))
				{
					r_[0] = p_[0];
					e_[0] = q_[0];
				}
				else
				{
					r_[0] = p_[1];
					e_[0] = q_[1];
				}

				p = simd::TwoProd_pd(a.value(), b.value(), e);
				simd::_mm_store_pd(p_, p);
				simd::_mm_store_pd(q_, e);
				if ((p_[0] > p_[1]) || ((p_[0] == p_[1]) && (q_[0] > q_[1])))
				{
					r_[1] = p_[0];
					e_[1] = q_[0];
				}
				else
				{
					r_[1] = p_[1];
					e_[1] = q_[1];
				}

				retval.value() = _mm_load_pd(r_);
				err.value() = _mm_load_pd(e_);
			}
			break;

			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_N1 + INTERVAL_CLASS_M:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_N0 + INTERVAL_CLASS_M:
				retval = simd::TwoProd_pd(simd::_mm_set1_pd(a.inf()), simd::swap_pd(b.value()), err.value());
				break;

			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_P1 + INTERVAL_CLASS_N1:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_P1 + INTERVAL_CLASS_N0:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_P0 + INTERVAL_CLASS_N1:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_P0 + INTERVAL_CLASS_N0:
				retval = simd::TwoProd_pd(simd::swap_pd(a.value()), b.value(), err.value());
				break;

			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_M + INTERVAL_CLASS_N1:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_M + INTERVAL_CLASS_N0:
				retval = simd::TwoProd_pd(simd::swap_pd(a.value()), simd::_mm_set1_pd(b.inf()), err.value());
				break;

			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_N1 + INTERVAL_CLASS_N1:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_N1 + INTERVAL_CLASS_N0:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_N0 + INTERVAL_CLASS_N1:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_N0 + INTERVAL_CLASS_N0:
				retval = simd::TwoProd_pd(simd::swap_pd(a.value()), simd::swap_pd(b.value()), err.value());
				break;
		}

		return retval;
	}

	intervalBase intervalBase::TwoSqr(intervalBase const& op, intervalError& err)
	{
		intervalBase retval;
		INTERVAL_CLASS opClass = op.classify();

		switch (opClass)
		{
			case INTERVAL_CLASS_ZERO:
				err = zero();
				return zero();

			case INTERVAL_CLASS_P1:
			case INTERVAL_CLASS_P0:
				retval = simd::TwoProd_pd(op.value(), op.value(), err.value());
				break;

			case INTERVAL_CLASS_M:
			{
				__m128d e;
				__m128d p = simd::TwoProd_pd(op.value(), op.value(), e);
				if ((simd::a_pd(p, 0) > simd::a_pd(p, 1)) || ((simd::a_pd(p, 0) == simd::a_pd(p, 1)) && (simd::a_pd(e, 0) > simd::a_pd(e, 1))))
				{
					retval = intervalBase(0.0, simd::a_pd(p, 0));
					err = intervalError(0.0, simd::a_pd(e, 0));
				}
				else
				{
					retval = intervalBase(0.0, simd::a_pd(p, 1));
					err = intervalError(0.0, simd::a_pd(e, 1));
				}
			}
			break;

			case INTERVAL_CLASS_N1:
			case INTERVAL_CLASS_N0:
				retval = simd::TwoProd_pd(simd::swap_pd(op.value()), simd::swap_pd(op.value()), err.value());
				break;
		}

		return retval;
	}

	intervalBase intervalBase::TwoSum(intervalBase const& a, intervalBase const& b, intervalError& err)
	{
		auto s = simd::TwoSum_pd(a.value(), b.value(), err.value());
		auto cls = simd::fpclassify_pd(s);

		//	adjust p[0], p[1] so the zero convention is maintained
		//

		//	[0, 0] - return a canonical zero
		if ((cls.first == FP_ZERO) && (cls.second == FP_ZERO))
			return zero();

		//	[0, b]	- return [+0, b]
		if (cls.first == FP_ZERO)
			return simd::abs_pd(s);

		//	[a, 0]	- return [a, -0]
		if (cls.second == FP_ZERO)
			return simd::chgsign_pd(simd::abs_pd(s));

		//	return [a, b]
		return s;
	}

	intervalBase intervalBase::Divide(intervalBase const& a, intervalBase const& b, intervalError& residual)
	{
		INTERVAL_CLASS aClass = a.classify();
		INTERVAL_CLASS bClass = b.classify();

		switch (INTERVAL_CLASS_COUNT * aClass + bClass)
		{
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_ZERO + INTERVAL_CLASS_P1:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_ZERO + INTERVAL_CLASS_P0:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_ZERO + INTERVAL_CLASS_M:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_ZERO + INTERVAL_CLASS_N0:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_ZERO + INTERVAL_CLASS_N1:
				residual = zero();
				return zero();

			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_P1 + INTERVAL_CLASS_ZERO:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_P0 + INTERVAL_CLASS_ZERO:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_N0 + INTERVAL_CLASS_ZERO:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_N1 + INTERVAL_CLASS_ZERO:
				residual = zero();
				return empty();

			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_M + INTERVAL_CLASS_ZERO:
				residual = zero();
				return entire();

			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_P1 + INTERVAL_CLASS_P1:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_P1 + INTERVAL_CLASS_P0:
				return simd::Divide_pd(a.value(), simd::swap_pd(b.value()), residual.value());

			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_P0 + INTERVAL_CLASS_P1:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_P0 + INTERVAL_CLASS_P0:
//				retval = intervalBase(0.0, a.sup() / b.inf());
				return simd::Divide_pd(_mm_set_pd(a.sup(), 0.0), _mm_set_pd(b.inf(), 1.0), residual.value());

			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_M + INTERVAL_CLASS_P1:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_M + INTERVAL_CLASS_P0:
				return simd::Divide_pd(a.value(), _mm_set1_pd(b.inf()), residual.value());

			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_N0 + INTERVAL_CLASS_P1:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_N0 + INTERVAL_CLASS_P0:
//				retval = intervalBase(a.inf() / b.inf(), -0.0);
				return simd::Divide_pd(_mm_set_pd(-0.0, a.inf()), _mm_set_pd(1.0, b.inf()), residual.value());

			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_N1 + INTERVAL_CLASS_P1:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_N1 + INTERVAL_CLASS_P0:
				return simd::Divide_pd(a.value(), b.value(), residual.value());

			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_P1 + INTERVAL_CLASS_M:
				residual = zero();
				return entire();

			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_P0 + INTERVAL_CLASS_M:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_M + INTERVAL_CLASS_M:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_N0 + INTERVAL_CLASS_M:
				residual = zero();
				return entire();

			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_N1 + INTERVAL_CLASS_M:
				residual = zero();
				return entire();

			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_P1 + INTERVAL_CLASS_N1:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_P1 + INTERVAL_CLASS_N0:
				return simd::Divide_pd(simd::swap_pd(a.value()), simd::swap_pd(b.value()), residual.value());

			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_P0 + INTERVAL_CLASS_N1:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_P0 + INTERVAL_CLASS_N0:
//				retval = intervalBase(a.sup() / b.sup(), -0.0);
				return simd::Divide_pd(_mm_set_pd(-0.0, a.sup()), _mm_set_pd(1.0, b.sup()), residual.value());

			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_M + INTERVAL_CLASS_N1:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_M + INTERVAL_CLASS_N0:
				return simd::Divide_pd(simd::swap_pd(a.value()), simd::_mm_set1_pd(b.sup()), residual.value());

			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_N0 + INTERVAL_CLASS_N1:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_N0 + INTERVAL_CLASS_N0:
//				retval = intervalBase(0.0, a.inf() / b.sup());
				return simd::Divide_pd(_mm_set_pd(a.inf(), 0.0), _mm_set_pd(b.sup(), 1.0), residual.value());

			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_N1 + INTERVAL_CLASS_N1:
			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_N1 + INTERVAL_CLASS_N0:
				return simd::Divide_pd(simd::swap_pd(a.value()), b.value(), residual.value());

			case INTERVAL_CLASS_COUNT * INTERVAL_CLASS_ZERO + INTERVAL_CLASS_ZERO:
			default:
				residual = zero();
				return empty();
		}
	}

	intervalBase intervalBase::Reciprocal(intervalBase const& op, intervalError& residual)
	{
		INTERVAL_CLASS opClass = op.classify();

		switch (opClass)
		{
			case INTERVAL_CLASS_P1:
			case INTERVAL_CLASS_P0:
			case INTERVAL_CLASS_N0:
			case INTERVAL_CLASS_N1:
				//	retval.inf() = 1.0 / op.sup();
				//	retval.sup() = 1.0 / op.inf();
				return simd::Reciprocal_pd(simd::swap_pd(op.value()), residual.value());

			case INTERVAL_CLASS_M:
				residual = zero();
				return entire();

			case INTERVAL_CLASS_ZERO:
			default:
				residual = zero();
				return empty();
		}
	}

	intervalBase intervalBase::Sqrt(intervalBase const& op, intervalError& residual)
	{
		INTERVAL_CLASS opClass = op.classify();

		switch (opClass)
		{
			case INTERVAL_CLASS_ZERO:
				residual = zero();
				return zero();

			case INTERVAL_CLASS_P1:
				return simd::Sqrt_pd(op.value(), residual.value());

			case INTERVAL_CLASS_P0:
			case INTERVAL_CLASS_M:
				return simd::Sqrt_pd(simd::_mm_set_pd(op.sup(), 0.0), residual.value());

			case INTERVAL_CLASS_N0:
				residual = zero();
				return zero();

			case INTERVAL_CLASS_N1:
			default:
				residual = zero();
				return empty();
		}
	}

	INTERVAL_CLASS intervalBase::classify() const
	{
		static INTERVAL_CLASS const results[] = {	// sup inf
			INTERVAL_CLASS_P1,						//	00  00	[P, P]
			INTERVAL_CLASS_P0,						//	00  01	[Z, P]
			INTERVAL_CLASS_M,						//	00  10	[N, P]
			INTERVAL_CLASS_P0,						//	00  11	[Z, P]

			INTERVAL_CLASS_EMPTY,					//	01  00	[P, Z]	error
			INTERVAL_CLASS_ZERO,					//	01  01	[Z, Z]
			INTERVAL_CLASS_N0,						//	01  10	[N, Z]
			INTERVAL_CLASS_ZERO,					//	01  11	[Z, Z]

			INTERVAL_CLASS_EMPTY,					//	10  00	[P, N]	error
			INTERVAL_CLASS_EMPTY,					//	10  01	[Z, N]	error
			INTERVAL_CLASS_N1,						//	10  10	[N, N]
			INTERVAL_CLASS_EMPTY,					//	10  11	[Z, N]	error

			INTERVAL_CLASS_EMPTY,					//	11  00	[P, Z]	error
			INTERVAL_CLASS_ZERO,					//	11  01	[Z, Z]
			INTERVAL_CLASS_N0,						//	11  10	[N, Z]
			INTERVAL_CLASS_ZERO,					//	11  11	[Z, Z]
		};

		unsigned int mask1 = _mm_movemask_pd(value());
		mask1 += 0x02u;				//	move b1 to b2, leaving b0 unchanged
		mask1 &= ~0x02u;
		__m128d z_ = _mm_cmpeq_pd(value(), _mm_setzero_pd());
		unsigned int mask2 = _mm_movemask_pd(z_);
		mask2 += 0x02u;				//	move b1 to b2, leaving b0 unchanged
		mask2 &= ~0x02u;
		mask1 <<= 1;				//	move b0, b2 to b3, b1
		mask1 |= mask2;				//	b1..b0 == 00 - positive nonzero
									//			  01 - positive zero
									//			  10 - negative nonzero
									//			  11 - negative zero
		auto retval = results[mask1];
		assert(retval != INTERVAL_CLASS_EMPTY);
		return retval;
	}

	intervalBase intervalBase::TwoSum(intervalBase const& a, intervalError const& b, intervalError& err)
	{
		auto s = simd::TwoSum_pd(a.value(), b.value(), err.value());
		auto cls = simd::fpclassify_pd(s);

		//	adjust p[0], p[1] so the zero convention is maintained
		//

		//	[0, 0] - return a canonical zero
		if ((cls.first == FP_ZERO) && (cls.second == FP_ZERO))
			return zero();

		//	[0, b]	- return [+0, b]
		if (cls.first == FP_ZERO)
			return simd::abs_pd(s);

		//	[a, 0]	- return [a, -0]
		if (cls.second == FP_ZERO)
			return simd::chgsign_pd(simd::abs_pd(s));

		//	return [a, b]
		return s;
	}

	intervalBase intervalBase::TwoSum(intervalError const& a, intervalError const& b, intervalError& err)
	{
		auto s = simd::TwoSum_pd(a.value(), b.value(), err.value());
		auto cls = simd::fpclassify_pd(s);

		//	adjust p[0], p[1] so the zero convention is maintained
		//

		//	[0, 0] - return a canonical zero
		if ((cls.first == FP_ZERO) && (cls.second == FP_ZERO))
			return zero();

		//	[0, b]	- return [+0, b]
		if (cls.first == FP_ZERO)
			return simd::abs_pd(s);

		//	[a, 0]	- return [a, -0]
		if (cls.second == FP_ZERO)
			return simd::chgsign_pd(simd::abs_pd(s));

		//	return [a, b]
		return s;
	}

}
