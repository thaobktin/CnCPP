// Interval.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "BasicIntervalRU.h"

#include <algorithm>
#include <cassert>

#include <emmintrin.h>

#include "RoundingControl.h"


namespace numerics
{

	BasicIntervalRU const BasicIntervalRU::s_Empty(std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN());
	BasicIntervalRU const BasicIntervalRU::s_Entire(-std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());

	BasicIntervalRU BasicIntervalRU::reciprocal(BasicIntervalRU const& op)
	{
		switch (op.classify())
		{
			case CLASS_P1:
			case CLASS_P0:
			case CLASS_N1:
			case CLASS_N0:
			{
				RoundingControl rc(RC_UP);
				//	-x[0] = -1.0 / op.sup();
				//	x[1] = 1.0 / op.inf();
				return BasicIntervalRU(simd::_mm_div_pd(simd::_mm_set1_pd(1.0), simd::chgsign_pd(simd::swap_pd(op.value_))));
			}
			break;

			case CLASS_M:
				return entire();

			case CLASS_ZERO:
			default:
				return empty();
		}
	}

	BasicIntervalRU BasicIntervalRU::sqr(BasicIntervalRU const& op)
	{
		__declspec(align(16)) double op_[2] = { mig(op), mag(op) };
		__m128d x;

		RoundingControl rc(RC_UP);

		// -inf = -inf() * inf();
		// sup = sup() * sup();
		x = _mm_mul_pd(_mm_set_pd(op_[1], -op_[0]), _mm_load_pd(op_));

		return BasicIntervalRU(x);
	}

	BasicIntervalRU BasicIntervalRU::sqrt(BasicIntervalRU const& op)
	{
		double x[2];

		switch (op.classify())
		{
			case CLASS_ZERO:
				return op;

			case CLASS_P1:
				{
					RoundingControl rc(RC_UP);
					__m128d zero = _mm_setzero_pd();
					unsigned int cw;

					_mm_store_sd(x + 1, _mm_sqrt_sd(zero, _mm_set_sd(op.sup())));

					_controlfp_s(&cw, RC_DOWN, MCW_RC);
					_mm_store_sd(x, _mm_sqrt_sd(zero, _mm_set_sd(op.inf())));
				}
				break;

			case CLASS_P0:
			case CLASS_M:
				{
					RoundingControl rc(RC_UP);
					__m128d zero = _mm_setzero_pd();

					_mm_store_sd(x + 1, _mm_sqrt_sd(zero, _mm_set_sd(op.sup())));

					x[0] = 0.0;
				}
				break;

			case CLASS_N1:
				return empty();

			case CLASS_N0:
				return BasicIntervalRU(0.0);
		}

		return BasicIntervalRU(x[0], x[1]);
	}

	void* BasicIntervalRU::operator new(std::size_t size)
	{
		void* retval = ::_aligned_malloc(size, __alignof(BasicIntervalRU));
		if (!retval)
			throw std::bad_alloc();
		return retval;
	}

	void BasicIntervalRU::operator delete(void* p)
	{
		::_aligned_free(p);
	}

	void* BasicIntervalRU::operator new[](std::size_t size)
	{
		void* retval = ::_aligned_malloc(size, __alignof(BasicIntervalRU));
		if (!retval)
			throw std::bad_alloc();
		return retval;
	}

	void BasicIntervalRU::operator delete[](void* p)
	{
		::_aligned_free(p);
	}

	BasicIntervalRU::BasicIntervalRU()
	{
		value_ = simd::_mm_setzero_pd();
	}

	BasicIntervalRU::BasicIntervalRU(double src)
	{
		if (std::isinf(src))
			throw std::runtime_error("interval ctor: infinite singletons not allowed");

		if (src == 0.0)
		{
			value_ = simd::_mm_setzero_pd();
		}
		else
		{
			value_ = simd::_mm_set_pd(src, -src);
		}
	}

	BasicIntervalRU::BasicIntervalRU(double inf, double sup)
	{
		if (sup < inf)
			throw std::runtime_error("interval ctor: inf cannot be greater than sup");
		if (inf == std::numeric_limits<double>::infinity())
			throw std::runtime_error("interval ctor: inf cannot be +infinity");
		if (sup == -std::numeric_limits<double>::infinity())
			throw std::runtime_error("interval ctor: sup cannot be -infinity");

		if ((inf == sup) && (inf == 0))
		{
			value_ = simd::_mm_setzero_pd();
		}
		else
		{
			if (inf < 0.0)
			{
				value_ = simd::_mm_set_pd(sup == 0.0 ? -0.0 : sup, -inf);
			}
			else
			{
				value_ = simd::_mm_set_pd(sup, inf == 0.0 ? -0.0 : -inf);
			}
		}
	}

	//	assignment operators
	//
	BasicIntervalRU& BasicIntervalRU::operator+=(BasicIntervalRU const& rhs)
	{
		if (isEmpty())
			return *this;
		if (rhs.isEmpty())
		{
			*this = rhs;
			return *this;
		}

		RoundingControl rc(RC_UP);
		value_ = _mm_add_pd(value_, rhs.value_);
		return *this;
	}

	BasicIntervalRU& BasicIntervalRU::operator-=(BasicIntervalRU const& rhs)
	{
		if (isEmpty())
			return *this;
		if (rhs.isEmpty())
		{
			*this = rhs;
			return *this;
		}

		RoundingControl rc(RC_UP);
		value_ = _mm_sub_pd(value_, rhs.value_);
		return *this;
	}

	BasicIntervalRU& BasicIntervalRU::operator*=(BasicIntervalRU const& rhs)
	{
		if (isEmpty())
			return *this;
		if (rhs.isEmpty())
		{
			*this = rhs;
			return *this;
		}

		switch (CLASS_COUNT*classify() + rhs.classify())
		{
			case CLASS_COUNT*CLASS_ZERO + CLASS_ZERO:
			case CLASS_COUNT*CLASS_ZERO + CLASS_P1:
			case CLASS_COUNT*CLASS_ZERO + CLASS_P0:
			case CLASS_COUNT*CLASS_ZERO + CLASS_M:
			case CLASS_COUNT*CLASS_ZERO + CLASS_N0:
			case CLASS_COUNT*CLASS_ZERO + CLASS_N1:
				break;

			case CLASS_COUNT*CLASS_P1 + CLASS_ZERO:
			case CLASS_COUNT*CLASS_P0 + CLASS_ZERO:
			case CLASS_COUNT*CLASS_M + CLASS_ZERO:
			case CLASS_COUNT*CLASS_N0 + CLASS_ZERO:
			case CLASS_COUNT*CLASS_N1 + CLASS_ZERO:
				*this = rhs;
				break;

			case CLASS_COUNT*CLASS_P1 + CLASS_P1:
			case CLASS_COUNT*CLASS_P1 + CLASS_P0:
			case CLASS_COUNT*CLASS_P0 + CLASS_P1:
			case CLASS_COUNT*CLASS_P0 + CLASS_P0:
				{
					RoundingControl rc(RC_UP);
					//	m_x[0] = inf() * rhs.inf();
					//	m_x[1] = sup() * rhs.sup();
					value_ = _mm_mul_pd(_mm_set_pd(sup(), inf()), rhs.value_);
				}
				break;

			case CLASS_COUNT*CLASS_P1 + CLASS_M:
			case CLASS_COUNT*CLASS_P0 + CLASS_M:
				{
					RoundingControl rc(RC_UP);
					//	m_x[0] = sup() * rhs.inf();
					//	m_x[1] = sup() * rhs.sup();
					value_ = _mm_mul_pd(_mm_set1_pd(sup()), rhs.value_);
				}
				break;

			case CLASS_COUNT*CLASS_P1 + CLASS_N1:
			case CLASS_COUNT*CLASS_P1 + CLASS_N0:
			case CLASS_COUNT*CLASS_P0 + CLASS_N1:
			case CLASS_COUNT*CLASS_P0 + CLASS_N0:
				{
					RoundingControl rc(RC_UP);
					//	m_x[0] = sup() * rhs.inf();
					//	m_x[1] = inf() * rhs.sup();
					value_ = _mm_mul_pd(_mm_set_pd(inf(), sup()), rhs.value_);
				}
				break;

			case CLASS_COUNT*CLASS_M + CLASS_P1:
			case CLASS_COUNT*CLASS_M + CLASS_P0:
				{
					RoundingControl rc(RC_UP);
					//	m_x[0] = inf() * rhs.sup();
					//	m_x[1] = sup() * rhs.sup();
					value_ = _mm_mul_pd(value_, _mm_set1_pd(rhs.sup()));
				}
				break;

			case CLASS_COUNT*CLASS_M + CLASS_M:
				{
					RoundingControl rc(RC_UP);
					double r0 = (std::max)(-inf()*rhs.sup(), -sup()*rhs.inf());
					double r1 = (std::max)(inf()*rhs.inf(), sup()*rhs.sup());
					value_ = _mm_set_pd(r1, r0);
				}
				break;

			case CLASS_COUNT*CLASS_M + CLASS_N1:
			case CLASS_COUNT*CLASS_M + CLASS_N0:
				{
					RoundingControl rc(RC_UP);
					//	m_x[0] = sup() * rhs.inf();
					//	m_x[1] = inf() * rhs.inf();
					value_ = _mm_mul_pd(_mm_set_pd(inf(), -sup()), _mm_set1_pd(rhs.inf()));
				}
				break;

			case CLASS_COUNT*CLASS_N1 + CLASS_P1:
			case CLASS_COUNT*CLASS_N1 + CLASS_P0:
			case CLASS_COUNT*CLASS_N0 + CLASS_P1:
			case CLASS_COUNT*CLASS_N0 + CLASS_P0:
				{
					RoundingControl rc(RC_UP);
					//	m_x[0] = inf() * rhs.sup();
					//	m_x[1] = sup() * rhs.inf();
					value_ = _mm_mul_pd(value_, _mm_set_pd(rhs.inf(), rhs.sup()));
				}
				break;

			case CLASS_COUNT*CLASS_N1 + CLASS_M:
			case CLASS_COUNT*CLASS_N0 + CLASS_M:
				{
					RoundingControl rc(RC_UP);
					//	m_x[0] = inf() * rhs.sup();
					//	m_x[1] = inf() * rhs.inf();
					value_ = _mm_mul_pd(_mm_set1_pd(inf()), _mm_set_pd(rhs.inf(), -rhs.sup()));
				}
				break;

			case CLASS_COUNT*CLASS_N1 + CLASS_N1:
			case CLASS_COUNT*CLASS_N1 + CLASS_N0:
			case CLASS_COUNT*CLASS_N0 + CLASS_N1:
			case CLASS_COUNT*CLASS_N0 + CLASS_N0:
				{
					RoundingControl rc(RC_UP);
					//	m_x[0] = sup() * rhs.sup();
					//	m_x[1] = inf() * rhs.inf();
					value_ = _mm_mul_pd(_mm_set_pd(inf(), -sup()), _mm_set_pd(rhs.inf(), rhs.sup()));
				}
				break;
		}

		return *this;
	}

	BasicIntervalRU& BasicIntervalRU::operator/=(BasicIntervalRU const& rhs)
	{
		if (isEmpty())
			return *this;
		if (rhs.isEmpty())
		{
			*this = rhs;
			return *this;
		}

		switch (CLASS_COUNT*classify() + rhs.classify())
		{
			case CLASS_COUNT*CLASS_ZERO + CLASS_ZERO:
			case CLASS_COUNT*CLASS_P1 + CLASS_ZERO:
			case CLASS_COUNT*CLASS_P0 + CLASS_ZERO:
			case CLASS_COUNT*CLASS_M + CLASS_ZERO:
			case CLASS_COUNT*CLASS_N0 + CLASS_ZERO:
			case CLASS_COUNT*CLASS_N1 + CLASS_ZERO:
				*this = empty();
				break;

			case CLASS_COUNT*CLASS_ZERO + CLASS_P1:
			case CLASS_COUNT*CLASS_ZERO + CLASS_P0:
			case CLASS_COUNT*CLASS_ZERO + CLASS_M:
			case CLASS_COUNT*CLASS_ZERO + CLASS_N0:
			case CLASS_COUNT*CLASS_ZERO + CLASS_N1:
				break;

			case CLASS_COUNT*CLASS_P1 + CLASS_P1:
			case CLASS_COUNT*CLASS_P1 + CLASS_P0:
			{
				RoundingControl rc(RC_UP);
				//	m_x[0] /= rhs.sup();
				//	m_x[1] /= rhs.inf();
				value_ = _mm_div_pd(value_, _mm_set_pd(rhs.inf(), rhs.sup()));
			}
			break;

			case CLASS_COUNT*CLASS_P0 + CLASS_P1:
			case CLASS_COUNT*CLASS_P0 + CLASS_P0:
				{
					RoundingControl rc(RC_UP);
					value_ = _mm_set_pd(sup() / rhs.inf(), -0.0);
				}
				break;

			case CLASS_COUNT*CLASS_M + CLASS_P1:
			case CLASS_COUNT*CLASS_M + CLASS_P0:
			{
				RoundingControl rc(RC_UP);
				//	m_x[0] /= rhs.inf();
				//	m_x[1] /= rhs.inf();
				value_ = _mm_div_pd(value_, _mm_set1_pd(rhs.inf()));
			}
			break;

			case CLASS_COUNT*CLASS_N0 + CLASS_P1:
			case CLASS_COUNT*CLASS_N0 + CLASS_P0:
			{
				RoundingControl rc(RC_UP);
				value_ = _mm_set_pd(-0.0, inf() / rhs.inf());
			}
			break;

			case CLASS_COUNT*CLASS_N1 + CLASS_P1:
			case CLASS_COUNT*CLASS_N1 + CLASS_P0:
			{
				RoundingControl rc(RC_UP);
				//	m_x[0] /= rhs.inf();
				//	m_x[1] /= rhs.sup();
				value_ = _mm_div_pd(value_, _mm_set_pd(rhs.sup(), rhs.inf()));
			}
			break;

			case CLASS_COUNT*CLASS_P1 + CLASS_M:
			case CLASS_COUNT*CLASS_N1 + CLASS_M:
				*this = empty();
				break;

			case CLASS_COUNT*CLASS_P0 + CLASS_M:
			case CLASS_COUNT*CLASS_M + CLASS_M:
			case CLASS_COUNT*CLASS_N0 + CLASS_M:
				*this = entire();
				break;

			case CLASS_COUNT*CLASS_P1 + CLASS_N1:
			case CLASS_COUNT*CLASS_P1 + CLASS_N0:
				{
					RoundingControl rc(RC_UP);
					//	m_x[0] = sup() / rhs.sup();
					//	m_x[1] = inf() / rhs.inf();
					value_ = _mm_div_pd(_mm_set_pd(inf(), -sup()), _mm_set_pd(rhs.inf(), rhs.sup()));
				}
				break;

			case CLASS_COUNT*CLASS_P0 + CLASS_N1:
			case CLASS_COUNT*CLASS_P0 + CLASS_N0:
				{
					RoundingControl rc(RC_UP);
					value_ = _mm_set_pd(-0.0, sup() / rhs.sup());
				}
				break;

			case CLASS_COUNT*CLASS_M + CLASS_N1:
			case CLASS_COUNT*CLASS_M + CLASS_N0:
				{
					RoundingControl rc(RC_UP);
					//	m_x[0] = sup() / rhs.sup();
					//	m_x[1] = inf() / rhs.inf();
					value_ = _mm_div_pd(_mm_set_pd(inf(), -sup()), _mm_set_pd(rhs.inf(), rhs.sup()));
				}
				break;

			case CLASS_COUNT*CLASS_N0 + CLASS_N1:
			case CLASS_COUNT*CLASS_N0 + CLASS_N0:
				{
					RoundingControl rc(RC_UP);
					value_ = _mm_set_pd(inf() / rhs.sup(), -0.0);
				}
				break;

			case CLASS_COUNT*CLASS_N1 + CLASS_N1:
			case CLASS_COUNT*CLASS_N1 + CLASS_N0:
				{
					RoundingControl rc(RC_UP);
					//	m_x[0] = sup() / rhs.inf();
					//	m_x[1] = inf() / rhs.sup();
					value_ = _mm_div_pd(_mm_set_pd(inf(), -sup()), rhs.value_);
				}
				break;
		}

		return *this;
	}

	BasicIntervalRU::CLASS BasicIntervalRU::classify() const
	{
		static CLASS const results[] = {			// sup inf
			CLASS_P1,						//	00  00	[P, P]
			CLASS_P0,						//	00  01	[Z, P]
			CLASS_M,						//	00  10	[N, P]
			CLASS_P0,						//	00  11	[Z, P]

			CLASS_EMPTY,					//	01  00	[P, Z]	error
			CLASS_ZERO,					//	01  01	[Z, Z]
			CLASS_N0,						//	01  10	[N, Z]
			CLASS_ZERO,					//	01  11	[Z, Z]

			CLASS_EMPTY,					//	10  00	[P, N]	error
			CLASS_EMPTY,					//	10  01	[Z, N]	error
			CLASS_N1,						//	10  10	[N, N]
			CLASS_EMPTY,					//	10  11	[Z, N]	error

			CLASS_EMPTY,					//	11  00	[P, Z]	error
			CLASS_ZERO,					//	11  01	[Z, Z]
			CLASS_N0,						//	11  10	[N, Z]
			CLASS_ZERO,					//	11  11	[Z, Z]
		};

		if (isEmpty())
			return CLASS_EMPTY;

		unsigned int mask1 = _mm_movemask_pd(value_);
		mask1 ^= 0x01u;				//	remember: we store inf as -inf - invert sign
		mask1 += 0x02u;				//	move b1 to b2, leaving b0 unchanged
		mask1 &= ~0x02u;
		__m128d z_ = _mm_cmpeq_pd(value_, _mm_setzero_pd());
		unsigned int mask2 = _mm_movemask_pd(z_);
		mask2 += 0x02u;				//	move b1 to b2, leaving b0 unchanged
		mask2 &= ~0x02u;
		mask1 <<= 1;				//	move b0, b2 to b3, b1
		mask1 |= mask2;				//	b1..b0 == 00 - positive nonzero
		//			  01 - positive zero
		//			  10 - negative nonzero
		//			  11 - negative zero
		auto retval = results[mask1];
		assert(retval != CLASS_EMPTY);
		return retval;
	}

	BasicIntervalRU reciprocal(BasicIntervalRU const& op)
	{
		if (op.isEmpty())
			return op;

		return BasicIntervalRU::reciprocal(op);
	}

	BasicIntervalRU sqr(BasicIntervalRU const& op)
	{
		if (op.isEmpty())
			return op;

		return BasicIntervalRU::sqr(op);
	}

	BasicIntervalRU sqrt(BasicIntervalRU const& op)
	{
		//	monotonic for a >= 0.0
		if (op.isEmpty())
			return op;

		if (op.inf() < 0.0)
		{
			errno = EDOM;
			return BasicIntervalRU::empty();
		}

		if (isSingleton(op) && (op.inf() == 0.0))		//	sqrt(+/-0.0) => +/-0.0
			return op;

		return BasicIntervalRU::sqrt(op);
	}

}
