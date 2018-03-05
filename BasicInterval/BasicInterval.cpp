// Interval.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "BasicInterval.h"

#include <algorithm>

#include <emmintrin.h>

#include "RoundingControl.h"


namespace numerics
{

	BasicInterval const BasicInterval::s_Empty(std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN());
	BasicInterval const BasicInterval::s_Entire(-std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());

	BasicInterval BasicInterval::reciprocal(BasicInterval const& op)
	{
		double inf = 0.0, sup = 0.0;

		switch (classify(op.inf(), op.sup()))
		{
			case CLASS_ZERO:
				return empty();

			case CLASS_P1:
			case CLASS_P0:
			case CLASS_N1:
			case CLASS_N0:
			{
					RoundingControl rc(RC_DOWN);
					unsigned int newCW;
					inf = 1.0 / op.sup();
					_controlfp_s(&newCW, RC_UP, MCW_RC);
					sup = 1.0 / op.inf();
				}
				break;

			case CLASS_M:
				return entire();
		}

		return BasicInterval(inf, sup);
	}

	BasicInterval BasicInterval::sqr(BasicInterval const& op)
	{
		double op_[2] = { mig(op), mag(op) };
		double x[2];

		{
			RoundingControl rc(RC_DOWN);
			unsigned int newCW;

			x[0] = op_[0] * op_[0];
			_controlfp_s(&newCW, RC_UP, MCW_RC);
			x[1] = op_[1] * op_[1];
		}

		return BasicInterval(x[0], x[1]);
	}

	BasicInterval BasicInterval::sqrt(BasicInterval const& op)
	{
		double x[2];

		switch (classify(op.inf(), op.sup()))
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

			case CLASS_N0:
				return BasicInterval(0.0);

			case CLASS_N1:
				return empty();
		}

		return BasicInterval(x[0], x[1]);
	}

	void* BasicInterval::operator new(std::size_t size)
	{
		void* retval = _aligned_malloc(size, 16);
		if (!retval)
			throw std::bad_alloc();
		return retval;
	}

	void BasicInterval::operator delete(void* p)
	{
		_aligned_free(p);
	}

	void* BasicInterval::operator new[](std::size_t size)
	{
		void* retval = _aligned_malloc(size, 16);
		if (!retval)
			throw std::bad_alloc();
		return retval;
	}

	void BasicInterval::operator delete[](void* p)
	{
		_aligned_free(p);
	}

	BasicInterval::BasicInterval()
	{
		m_x[0] = -0.0;									//	IEEE 754 -0.0
		m_x[1] = 0.0;
	}

	BasicInterval::BasicInterval(double src)
	{
		if (std::isinf(src))
			throw std::runtime_error("interval ctor: infinite singletons not allowed");

		if (src == 0.0)
		{
			m_x[0] = -0.0;								//	IEEE 754 -0.0
			m_x[1] = 0.0;
		}
		else
		{
			m_x[0] = src;
			m_x[1] = src;
		}
	}

	BasicInterval::BasicInterval(double inf, double sup)
	{
		if (sup < inf)
			throw std::runtime_error("interval ctor: inf cannot be greater than sup");
		if (inf == std::numeric_limits<double>::infinity())
			throw std::runtime_error("interval ctor: inf cannot be +infinity");
		if (sup == -std::numeric_limits<double>::infinity())
			throw std::runtime_error("interval ctor: sup cannot be -infinity");

		if ((inf == sup) && (inf == 0))
		{
			m_x[0] = -0.0;								//	IEEE 754 -0.0
			m_x[1] = 0.0;
		}
		else
		{
			if (inf < 0.0)
			{
				m_x[0] = inf;
				m_x[1] = sup == 0.0 ? -0.0 : sup;
			}
			else
			{
				m_x[0] = inf == 0.0 ? 0.0 : inf;
				m_x[1] = sup;
			}
		}
	}

	//	assignment operators
	//
	BasicInterval& BasicInterval::operator+=(BasicInterval const& rhs)
	{
		if (isEmpty())
			return *this;
		if (rhs.isEmpty())
		{
			*this = rhs;
			return *this;
		}

		RoundingControl rc(RC_DOWN);
		unsigned int newCW;

		m_x[0] += rhs.m_x[0];
		_controlfp_s(&newCW, RC_UP, MCW_RC);
		m_x[1] += rhs.m_x[1];
		return *this;
	}

	BasicInterval& BasicInterval::operator-=(BasicInterval const& rhs)
	{
		if (isEmpty())
			return *this;
		if (rhs.isEmpty())
		{
			*this = rhs;
			return *this;
		}

		RoundingControl rc(RC_DOWN);
		unsigned int newCW;

		m_x[0] -= rhs.m_x[0];
		_controlfp_s(&newCW, RC_UP, MCW_RC);
		m_x[1] -= rhs.m_x[1];
		return *this;
	}

	BasicInterval& BasicInterval::operator*=(BasicInterval const& rhs)
	{
		if (isEmpty())
			return *this;
		if (rhs.isEmpty())
		{
			*this = rhs;
			return *this;
		}

		switch (CLASS_COUNT*classify(inf(), sup()) + classify(rhs.inf(), rhs.sup()))
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
					RoundingControl rc(RC_DOWN);
					unsigned int newCW;
					m_x[0] = inf() * rhs.inf();
					_controlfp_s(&newCW, RC_UP, MCW_RC);
					m_x[1] = sup() * rhs.sup();
				}
				break;

			case CLASS_COUNT*CLASS_P1 + CLASS_M:
			case CLASS_COUNT*CLASS_P0 + CLASS_M:
				{
					RoundingControl rc(RC_DOWN);
					unsigned int newCW;
					m_x[0] = sup() * rhs.inf();
					_controlfp_s(&newCW, RC_UP, MCW_RC);
					m_x[1] = sup() * rhs.sup();
				}
				break;

			case CLASS_COUNT*CLASS_P1 + CLASS_N1:
			case CLASS_COUNT*CLASS_P1 + CLASS_N0:
			case CLASS_COUNT*CLASS_P0 + CLASS_N1:
			case CLASS_COUNT*CLASS_P0 + CLASS_N0:
				{
					RoundingControl rc(RC_DOWN);
					unsigned int newCW;
					double r0 = sup() * rhs.inf();
					_controlfp_s(&newCW, RC_UP, MCW_RC);
					double r1 = inf() * rhs.sup();
					m_x[0] = r0;
					m_x[1] = r1;
				}
				break;

			case CLASS_COUNT*CLASS_M + CLASS_P1:
			case CLASS_COUNT*CLASS_M + CLASS_P0:
				{
					RoundingControl rc(RC_DOWN);
					unsigned int newCW;
					m_x[0] = inf() * rhs.sup();
					_controlfp_s(&newCW, RC_UP, MCW_RC);
					m_x[1] = sup() * rhs.sup();
				}
				break;

			case CLASS_COUNT*CLASS_M + CLASS_M:
				{
					RoundingControl rc(RC_DOWN);
					unsigned int newCW;
					double r0 = (std::min)(inf()*rhs.sup(), sup()*rhs.inf());
					_controlfp_s(&newCW, RC_UP, MCW_RC);
					double r1 = (std::max)(inf()*rhs.inf(), sup()*rhs.sup());
					m_x[0] = r0;
					m_x[1] = r1;
				}
				break;

			case CLASS_COUNT*CLASS_M + CLASS_N1:
			case CLASS_COUNT*CLASS_M + CLASS_N0:
				{
					RoundingControl rc(RC_DOWN);
					unsigned int newCW;
					double r0 = sup() * rhs.inf();
					_controlfp_s(&newCW, RC_UP, MCW_RC);
					double r1 = inf() * rhs.inf();
					m_x[0] = r0;
					m_x[1] = r1;
				}
				break;

			case CLASS_COUNT*CLASS_N1 + CLASS_P1:
			case CLASS_COUNT*CLASS_N1 + CLASS_P0:
			case CLASS_COUNT*CLASS_N0 + CLASS_P1:
			case CLASS_COUNT*CLASS_N0 + CLASS_P0:
				{
					RoundingControl rc(RC_DOWN);
					unsigned int newCW;
					m_x[0] = inf() * rhs.sup();
					_controlfp_s(&newCW, RC_UP, MCW_RC);
					m_x[1] = sup() * rhs.inf();
				}
				break;

			case CLASS_COUNT*CLASS_N1 + CLASS_M:
			case CLASS_COUNT*CLASS_N0 + CLASS_M:
				{
					RoundingControl rc(RC_DOWN);
					unsigned int newCW;
					double r0 = inf() * rhs.sup();
					_controlfp_s(&newCW, RC_UP, MCW_RC);
					double r1 = inf() * rhs.inf();
					m_x[0] = r0;
					m_x[1] = r1;
				}
				break;

			case CLASS_COUNT*CLASS_N1 + CLASS_N1:
			case CLASS_COUNT*CLASS_N1 + CLASS_N0:
			case CLASS_COUNT*CLASS_N0 + CLASS_N1:
			case CLASS_COUNT*CLASS_N0 + CLASS_N0:
				{
					RoundingControl rc(RC_DOWN);
					unsigned int newCW;
					double r0 = sup() * rhs.sup();
					_controlfp_s(&newCW, RC_UP, MCW_RC);
					double r1 = inf() * rhs.inf();
					m_x[0] = r0;
					m_x[1] = r1;
				}
				break;
		}

		return *this;
	}

	BasicInterval& BasicInterval::operator/=(BasicInterval const& rhs)
	{
		if (isEmpty())
			return *this;
		if (rhs.isEmpty())
		{
			*this = rhs;
			return *this;
		}

		switch (CLASS_COUNT*classify(inf(), sup()) + classify(rhs.inf(), rhs.sup()))
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
					RoundingControl rc(RC_DOWN);
					unsigned int newCW;
					m_x[0] /= rhs.sup();
					_controlfp_s(&newCW, RC_UP, MCW_RC);
					m_x[1] /= rhs.inf();
				}
				break;

			case CLASS_COUNT*CLASS_P0 + CLASS_P1:
			case CLASS_COUNT*CLASS_P0 + CLASS_P0:
				m_x[0] = 0.0;
				{
					RoundingControl rc(RC_UP);
					m_x[1] /= rhs.inf();
				}
				break;

			case CLASS_COUNT*CLASS_M + CLASS_P1:
			case CLASS_COUNT*CLASS_M + CLASS_P0:
			{
				RoundingControl rc(RC_DOWN);
				unsigned int newCW;
				m_x[0] /= rhs.inf();
				_controlfp_s(&newCW, RC_UP, MCW_RC);
				m_x[1] /= rhs.inf();
			}
			break;

			case CLASS_COUNT*CLASS_N0 + CLASS_P1:
			case CLASS_COUNT*CLASS_N0 + CLASS_P0:
				{
					RoundingControl rc(RC_DOWN);
					m_x[0] /= rhs.inf();
					m_x[1] = -0.0;
				}
				break;

			case CLASS_COUNT*CLASS_N1 + CLASS_P1:
			case CLASS_COUNT*CLASS_N1 + CLASS_P0:
				{
					RoundingControl rc(RC_DOWN);
					unsigned int newCW;
					m_x[0] /= rhs.inf();
					_controlfp_s(&newCW, RC_UP, MCW_RC);
					m_x[1] /= rhs.sup();
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
					RoundingControl rc(RC_DOWN);
					unsigned int newCW;
					double r0 = sup() / rhs.sup();
					_controlfp_s(&newCW, RC_UP, MCW_RC);
					double r1 = inf() / rhs.inf();
					m_x[0] = r0;
					m_x[1] = r1;
				}
				break;

			case CLASS_COUNT*CLASS_P0 + CLASS_N1:
			case CLASS_COUNT*CLASS_P0 + CLASS_N0:
				{
					RoundingControl rc(RC_DOWN);
					unsigned int newCW;
					m_x[0] = sup() / rhs.sup();
					_controlfp_s(&newCW, RC_UP, MCW_RC);
					m_x[1] = -0.0;
				}
				break;

			case CLASS_COUNT*CLASS_M + CLASS_N1:
			case CLASS_COUNT*CLASS_M + CLASS_N0:
			{
				RoundingControl rc(RC_DOWN);
				unsigned int newCW;
				double r0 = sup() / rhs.sup();
				_controlfp_s(&newCW, RC_UP, MCW_RC);
				double r1 = inf() / rhs.inf();
				m_x[0] = r0;
				m_x[1] = r1;
			}
			break;

			case CLASS_COUNT*CLASS_N0 + CLASS_N1:
			case CLASS_COUNT*CLASS_N0 + CLASS_N0:
				{
					RoundingControl rc(RC_DOWN);
					unsigned int newCW;
					m_x[0] = 0.0;
					_controlfp_s(&newCW, RC_UP, MCW_RC);
					m_x[1] = inf() / rhs.sup();
				}
				break;

			case CLASS_COUNT*CLASS_N1 + CLASS_N1:
			case CLASS_COUNT*CLASS_N1 + CLASS_N0:
			{
				RoundingControl rc(RC_DOWN);
				unsigned int newCW;
				double r0 = sup() / rhs.inf();
				_controlfp_s(&newCW, RC_UP, MCW_RC);
				double r1 = inf() / rhs.sup();
				m_x[0] = r0;
				m_x[1] = r1;
			}
			break;
		}

		return *this;
	}

	BasicInterval reciprocal(BasicInterval const& op)
	{
		if (op.isEmpty())
			return op;

		return BasicInterval::reciprocal(op);
	}

	BasicInterval sqr(BasicInterval const& op)
	{
		if (isEmpty(op))
			return op;

		return BasicInterval::sqr(op);
	}

	BasicInterval sqrt(BasicInterval const& op)
	{
		if (isEmpty(op))
			return op;

		return BasicInterval::sqrt(op);
	}

}
