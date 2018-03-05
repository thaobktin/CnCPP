/*
**	interval.cpp
**
*/

#include "stdafx.h"
#include "interval.h"


namespace numerics
{

	void* interval::operator new(std::size_t size)
	{
		void* retval = _aligned_malloc(size, 16);
		if (!retval)
			throw std::bad_alloc();
		return retval;
	}

		void interval::operator delete(void* p)
	{
		_aligned_free(p);
	}

	void* interval::operator new[](std::size_t size)
	{
		void* retval = ::_aligned_malloc(size, __alignof(interval));
		if (!retval)
			throw std::bad_alloc();
		return retval;
	}

	void interval::operator delete[](void* p)
	{
		::_aligned_free(p);
	}

	interval::interval()
		: intervalBase(zero().value())
	{
	}

	interval::interval(double src)
		: intervalBase(src, src)
	{
		if (std::isinf(src))
			throw std::runtime_error("interval ctor: singleton may not be infinite.");

		if (src == 0.0)
			value() = zero().value();
	}

	interval::interval(double inf_, double sup_)
		: intervalBase(inf_, sup_)
	{
		if (sup_ < inf_)
			throw std::runtime_error("interval ctor: inf > sup.");
		if (inf_ == std::numeric_limits<double>::infinity())
			throw std::runtime_error("interval ctor: inf may not be +infinity.");
		if (sup_ == -std::numeric_limits<double>::infinity())
			throw std::runtime_error("interval ctor: sup may not be -infinity.");

		if ((inf_ == sup_) && (inf_ == 0.0))
		{
			value() = zero().value();
		}
		else
		{
			if (inf_ == 0.0)
			{
				value() = simd::abs_pd(value());
			}
			else
			{
				if (sup_ == 0.0)
				{
					value() = simd::chgsign_pd(simd::abs_pd(value()));
				}
			}
		}
	}

	//	assignment operators
	//
	interval& interval::operator+=(interval const& rhs)
	{
		if (isEmpty())
			return *this;
		if (rhs.isEmpty())
		{
			*this = rhs;
			return *this;
		}

		intervalError err;
		*this = TwoSum(*this, rhs, err);
		adjust(err);

		return *this;
	}

	interval& interval::operator-=(interval const& rhs)
	{
		if (isEmpty())
			return *this;
		if (rhs.isEmpty())
		{
			*this = rhs;
			return *this;
		}

		intervalError err;
		*this = TwoDiff(*this, rhs, err);
		adjust(err);

		return *this;
	}

	interval& interval::operator*=(interval const& rhs)
	{
		if (isEmpty())
			return *this;
		if (rhs.isEmpty())
		{
			*this = rhs;
			return *this;
		}

		intervalError err;
		*this = TwoProd(*this, rhs, err);
		adjust(err);

		return *this;
	}

	interval& interval::operator/=(interval const& rhs)
	{
		if (isEmpty())
			return *this;
		if (rhs.isEmpty())
		{
			*this = rhs;
			return *this;
		}

		intervalError residual;
		*this = Divide(*this, rhs, residual);
		adjust(residual);

		return *this;
	}

	interval& interval::operator=(double rhs)
	{
		if (std::isinf(rhs))
			throw std::runtime_error("interval::operator=(): singleton may not be infinite.");

		if (rhs == 0.0)
		{
			value() = zero().value();
		}
		else
		{
			value() = simd::_mm_set_pd(rhs, rhs);
		}
		return *this;
	}

	interval fma(interval const& a, interval const& b, interval const& c)
	{
		if (a.isEmpty())
			return a;
		if (b.isEmpty())
			return b;
		if (c.isEmpty())
			return c;

		intervalError err1, err2;
		interval retval = intervalBase::ThreeFma(a, b, c, err1, err2);
		retval.adjust(err1);

		return retval;
	}

	interval reciprocal(interval const& op)
	{
		if (op.isEmpty())
			return op;

		intervalError residual;
		interval recip = intervalBase::Reciprocal(op, residual);
		recip.adjust(residual);

		return recip;
	}

	interval sqr(interval const& op)
	{
		if (op.isEmpty())
			return op;

		intervalError err;
		interval prod = intervalBase::TwoSqr(op, err);
		prod.adjust(err);

		return prod;
	}

	interval sqrt(interval const& op)
	{
		if (op.isEmpty())
			return op;

		intervalError residual;
		interval retval = intervalBase::Sqrt(op, residual);
		retval.adjust(residual);

		return retval;
	}

	interval hypot(interval const& x, interval const& y)
	{
		intervalError errX;
		interval x2 = interval::TwoSqr(x, errX);
		intervalError errY;
		interval y2 = interval::TwoSqr(y, errY);

		//	x2 + y2 + errX + errY is the EXACT sum...
		//
		__m128d vec[4] = { x2.value(), y2.value(), errX.value(), errY.value() };
		intervalError err;
		intervalBase sum = simd::Sum_pd(4, vec, err.value());
		sum.adjust(err);

		intervalBase retval = intervalBase::Sqrt(sum, err);
		retval.adjust(err);

		return retval;
	}
}
