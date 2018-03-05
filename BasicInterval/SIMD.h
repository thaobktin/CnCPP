/*
**	SIMD.h				- extensions of the SIMD instruction set
**
*/

#if !defined( SIMD_H__ )
#define SIMD_H__

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

#include <emmintrin.h>


namespace simd
{

	//	arithmetic - SD
	//
	using ::_mm_add_sd;
	using ::_mm_sub_sd;
	using ::_mm_mul_sd;
	using ::_mm_sqrt_sd;
	using ::_mm_div_sd;
	using ::_mm_min_sd;
	using ::_mm_max_sd;

	//	arithmetic - PD
	//
	using ::_mm_add_pd;
	using ::_mm_sub_pd;
	using ::_mm_mul_pd;
	using ::_mm_sqrt_pd;
	using ::_mm_div_pd;
	using ::_mm_min_pd;
	using ::_mm_max_pd;

	//	logical - PD
	//
	using ::_mm_and_pd;
	using ::_mm_andnot_pd;
	using ::_mm_or_pd;
	using ::_mm_xor_pd;

	//	comparisons - SD
	//
	using ::_mm_cmpeq_sd;
	using ::_mm_cmplt_sd;
	using ::_mm_cmple_sd;
	using ::_mm_cmpgt_sd;
	using ::_mm_cmpge_sd;
	using ::_mm_cmpneq_sd;
	using ::_mm_cmpnlt_sd;
	using ::_mm_cmpnle_sd;
	using ::_mm_cmpngt_sd;
	using ::_mm_cmpnge_sd;
	using ::_mm_cmpord_sd;
	using ::_mm_cmpunord_sd;

	//using int _mm_comieq_sd(__m128d _A, __m128d _B);
	//using int _mm_comilt_sd(__m128d _A, __m128d _B);
	//using int _mm_comile_sd(__m128d _A, __m128d _B);
	//using int _mm_comigt_sd(__m128d _A, __m128d _B);
	//using int _mm_comige_sd(__m128d _A, __m128d _B);
	//using int _mm_comineq_sd(__m128d _A, __m128d _B);
	//using int _mm_ucomieq_sd(__m128d _A, __m128d _B);
	//using int _mm_ucomilt_sd(__m128d _A, __m128d _B);
	//using int _mm_ucomile_sd(__m128d _A, __m128d _B);
	//using int _mm_ucomigt_sd(__m128d _A, __m128d _B);
	//using int _mm_ucomige_sd(__m128d _A, __m128d _B);
	//using int _mm_ucomineq_sd(__m128d _A, __m128d _B);

	//	comparisons - PD
	//
	using ::_mm_cmpeq_pd;
	using ::_mm_cmplt_pd;
	using ::_mm_cmple_pd;
	using ::_mm_cmpgt_pd;
	using ::_mm_cmpge_pd;
	using ::_mm_cmpneq_pd;
	using ::_mm_cmpnlt_pd;
	using ::_mm_cmpnle_pd;
	using ::_mm_cmpngt_pd;
	using ::_mm_cmpnge_pd;
	using ::_mm_cmpord_pd;
	using ::_mm_cmpunord_pd;

	///*
	//* DP, converts
	//*/

	//using __m128d _mm_cvtepi32_pd(__m128i _A);
	//using __m128i _mm_cvtpd_epi32(__m128d _A);
	//using __m128i _mm_cvttpd_epi32(__m128d _A);
	//using __m128 _mm_cvtepi32_ps(__m128i _A);
	//using __m128i _mm_cvtps_epi32(__m128 _A);
	//using __m128i _mm_cvttps_epi32(__m128 _A);
	//using __m128 _mm_cvtpd_ps(__m128d _A);
	//using __m128d _mm_cvtps_pd(__m128 _A);
	//using __m128 _mm_cvtsd_ss(__m128 _A, __m128d _B);
	//using __m128d _mm_cvtss_sd(__m128d _A, __m128 _B);

	//using int _mm_cvtsd_si32(__m128d _A);
	//using int _mm_cvttsd_si32(__m128d _A);
	//using __m128d _mm_cvtsi32_sd(__m128d _A, int _B);

	//using __m64 _mm_cvtpd_pi32(__m128d _A);
	//using __m64 _mm_cvttpd_pi32(__m128d _A);
	//using __m128d _mm_cvtpi32_pd(__m64 _A);

	//	miscellaneous - PD
	//
	using ::_mm_unpackhi_pd;
	using ::_mm_unpacklo_pd;
	using ::_mm_movemask_pd;
	using ::_mm_shuffle_pd;

	//	loads - SD
	//
	using ::_mm_load_sd;

	//	loads - PD
	//
	using ::_mm_load_pd;
	using ::_mm_load1_pd;
	using ::_mm_loadr_pd;
	using ::_mm_loadu_pd;
	using ::_mm_loadh_pd;
	using ::_mm_loadl_pd;


	//	sets - SD
	//
	using ::_mm_set_sd;
	using ::_mm_move_sd;

	//	sets - PD
	//
	using ::_mm_set1_pd;
	using ::_mm_set_pd;
	using ::_mm_setr_pd;
	using ::_mm_setzero_pd;

	//	stores - SD
	//
	using ::_mm_store_sd;

	//	stores - PD
	//
	using ::_mm_store1_pd;
	using ::_mm_store_pd;
	using ::_mm_storeu_pd;
	using ::_mm_storer_pd;
	using ::_mm_storeh_pd;
	using ::_mm_storel_pd;


	inline double a_pd(__m128d x, int i)
	{
		__declspec(align(16)) double retval[2];
		_mm_store_pd(retval, x);
		return retval[i];
	}

	inline void seta_pd(__m128d& x, int i, double val)
	{
		__declspec(align(16)) double retval[2];
		_mm_store_pd(retval, x);
		retval[i] = val;
		x = _mm_load_pd(retval);
	}

	inline __m128d abs_pd(__m128d x)
	{
		static __m128d const nzero = _mm_set1_pd(-0.0);

		return _mm_andnot_pd(nzero, x);
	}

	inline __m128d chgsign_pd(__m128d x)
	{
		static __m128d const nzero = _mm_set1_pd(-0.0);

		return _mm_xor_pd(nzero, x);
	}

	inline __m128d swap_pd(__m128d x)
	{
		return _mm_shuffle_pd(x, x, 1);
	}

	//	returns a mask (a la the _mm_cmpxx_pd functions) indicating whether the
	//	components are finite
	//
	inline __m128d isfinite_pd(__m128d x)
	{
		static __m128d const infinity = _mm_set1_pd(std::numeric_limits<double>::infinity());

		return _mm_cmplt_pd(abs_pd(x), infinity);
	}

	//	returns a mask (a la the _mm_cmpxx_pd functions) indicating whether the
	//	components are subnormal
	//
	inline __m128d issubnormal_pd(__m128d x)
	{
		static __m128d const minval = _mm_set1_pd((std::numeric_limits<double>::min)());

		return _mm_and_pd(_mm_cmplt_pd(abs_pd(x), minval), _mm_cmpneq_pd(x, _mm_setzero_pd()));
	}

	//	returns a mask (a la the _mm_cmpxx_pd functions) indicating whether the
	//	components are infinite
	//
	inline __m128d isinf_pd(__m128d x)
	{
		static __m128d const infinity = _mm_set1_pd(std::numeric_limits<double>::infinity());

		return _mm_cmpeq_pd(abs_pd(x), infinity);
	}

	//	returns a mask (a la the _mm_cmpxx_pd functions) indicating whether the
	//	components are NaN
	//
	inline __m128d isnan_pd(__m128d x)
	{
		return _mm_cmpunord_pd(x, x);
	}

	//	returns a mask (a la the _mm_cmpxx_pd functions) indicating whether the
	//	components are zero
	//
	inline __m128d iszero_pd(__m128d x)
	{
		return _mm_cmpeq_pd(x, _mm_setzero_pd());
	}

	//	returns a mask (a la the _mm_cmpxx_pd functions) indicating whether the
	//	components are negative
	//
	inline __m128d isnegative_pd(__m128d x)
	{
		return _mm_cmplt_pd(x, _mm_setzero_pd());
	}

	//	returns a mask (a la the _mm_cmpxx_pd functions) indicating whether the
	//	components are positive
	//
	inline __m128d ispositive_pd(__m128d x)
	{
		return _mm_cmpgt_pd(x, _mm_setzero_pd());
	}

	//	returns a mask (a la the _mm_cmpxx_pd functions) indicating whether the
	//	components are non-zero
	//
	inline __m128d isnonzero_pd(__m128d x)
	{
		return _mm_cmpneq_pd(x, _mm_setzero_pd());
	}

	//	returns a mask (a la the _mm_cmpxx_pd functions) indicating whether the
	//	components are non-negative
	//
	inline __m128d isnonnegative_pd(__m128d x)
	{
		return _mm_cmpge_pd(x, _mm_setzero_pd());
	}

	//	returns a mask (a la the _mm_cmpxx_pd functions) indicating whether the
	//	components are non-positive
	//
	inline __m128d isnonpositive_pd(__m128d x)
	{
		return _mm_cmple_pd(x, _mm_setzero_pd());
	}

	//	Performs an if/then/else statement as follows:
	//
	//		For each component:
	//			if the mask is FFFFFFFFFFFFFFFF, set the value to trueval
	//			if the mask is 0000000000000000, set the value to falseval
	inline __m128d if_pd(__m128d mask, __m128d trueval, __m128d falseval)
	{
		__m128d true_ = _mm_and_pd(mask, trueval);
		__m128d false_ = _mm_andnot_pd(mask, falseval);
		return _mm_or_pd(true_, false_);
	}

	//	classifies a __m128d.
	//	'first' contains the classification of a0
	//	'second' contains the classification of a1
	//
	inline std::pair<int, int> fpclassify_pd(__m128d x)
	{
		static __m128d const subnormalmask = _mm_set1_pd(8.0);
		static __m128d const nanmask = _mm_set1_pd(4.0);
		static __m128d const infmask = _mm_set1_pd(2.0);
		static __m128d const zeromask = _mm_set1_pd(1.0);
		static int const fpclass[] = {
			FP_NORMAL,
			FP_ZERO,
			FP_INFINITE,
			INT_MIN,					//	error
			FP_NAN,
			INT_MIN,
			INT_MIN,
			INT_MIN,
			FP_SUBNORMAL
		};

		//	note that all masks are mutually exclusive
		__m128d isZero = if_pd(iszero_pd(x), zeromask, _mm_setzero_pd());
		__m128d isNaN = if_pd(isnan_pd(x), nanmask, _mm_setzero_pd());
		__m128d isInf = if_pd(isinf_pd(x), infmask, _mm_setzero_pd());
		__m128d isSubnormal = if_pd(issubnormal_pd(x), subnormalmask, _mm_setzero_pd());
		__m128d mask = _mm_or_pd(isSubnormal, _mm_or_pd(isZero, _mm_or_pd(isNaN, isInf)));
		double hi = a_pd(mask, 1);
		double lo = a_pd(mask, 0);
		
		return std::make_pair(fpclass[(int)lo], fpclass[(int)hi]);
	}

	//	Error-free transforms
	//
	//	The following set of functions perform EXACT floating-point arithmetic
	//	by returning the result of the operation rounded to nearest, and an
	//	error term.
	//
	//		TwoSum_pd(a, b, err)			- retval = RN(a + b), err = error from exact result
	//		TwoDiff_pd(a, b, err)			- retval = RN(a - b), err = error from exact result
	//		TwoProd_pd(a, b, err)			- retval = RN(a * b), err = error from exact result
	//		TwoSqr(a, err)					- retval = RN(a^2), err = error from exact result
	//		ThreeFma_pd(a, b, c)			- retval = RN(a * b + c), no error returned
	//		ThreeFma_pd(a, b, c, e1, e2)	- retval = RN(a * b + c), e1 + e2 = error from exact result
	//		ThreeFms_pd(a, b, c)			- retval = RN(a * b - c), no error returned
	//		ThreeFms_pd(a, b, c, e1, e2)	- retval = RN(a * b - c), e1 + e2 = error from exact result
	//		ThreeSum_pd(a, b, c, e1, e2)	- retval = RN(a + b + c), e1 + e2 = error from exact result
	inline __m128d TwoSum_pd(__m128d a_, __m128d b_, __m128d& err_)
	{
		// s.inf = a.inf + b.inf;
		// s.sup = a.sup + b.sup;
		__m128d s_ = _mm_add_pd(a_, b_);
		__m128d isfinite_ = isfinite_pd(s_);

		//	bb,inf = s.inf - a.inf;
		//	bb.sup = s.sup - a.sup;
		__m128d bb_ = _mm_sub_pd(s_, a_);

		//	e.inf = (a.inf - (s.inf - bb.inf)) + (b.inf - bb.inf);
		//	e.sup = (a.sup - (s.sup - bb.sup)) + (b.sup - bb,sup);
		__m128d e_ = _mm_add_pd(_mm_sub_pd(a_, _mm_sub_pd(s_, bb_)), _mm_sub_pd(b_, bb_));

		//	if s is infinite, err == 0.0
		err_ = _mm_and_pd(isfinite_, e_);

		return s_;
	}

	inline __m128d TwoDiff_pd(__m128d a_, __m128d b_, __m128d& err_)
	{
		// s.inf = a.inf - b.inf;
		// s.sup = a.sup - b.sup;
		__m128d s_ = _mm_sub_pd(a_, b_);
		__m128d isfinite_ = isfinite_pd(s_);

		//	bb,inf = s.inf - a.inf;
		//	bb.sup = s.sup - a.sup;
		__m128d bb_ = _mm_sub_pd(s_, a_);

		//	e.inf = (a.inf - (s.inf - bb.inf)) - (b.inf + bb.inf);
		//	e.sup = (a.sup - (s.sup - bb.sup)) - (b.sup + bb,sup);
		__m128d e_ = _mm_sub_pd(_mm_sub_pd(a_, _mm_sub_pd(s_, bb_)), _mm_add_pd(b_, bb_));

		//	if s is infinite, err == 0.0
		err_ = _mm_and_pd(isfinite_, e_);

		return s_;
	}

	__m128d TwoProd_pd(__m128d a, __m128d b, __m128d& err);
	__m128d TwoSqr_pd(__m128d a, __m128d& err);
	__m128d ThreeFma_pd(__m128d a, __m128d b, __m128d c);
	__m128d ThreeFma_pd(__m128d a, __m128d b, __m128d c, __m128d& e1, __m128d& e2);
	__m128d ThreeFms_pd(__m128d a, __m128d b, __m128d c);
	__m128d ThreeFms_pd(__m128d a, __m128d b, __m128d c, __m128d& e1, __m128d& e2);
	__m128d ThreeSum_pd(__m128d a, __m128d b, __m128d c, __m128d& e1, __m128d& e2);

	//	Transforms with residual
	//
	//	The following set of functions calculate the result rounded to nearest,
	//	and return a residual. The residual may be used in order to infer the
	//	direction in which the returned value was rounded from the exact result.
	//
	//		Divide_pd(a, b, residual)		- retval = RN(a / b), residual = a - result * b
	//		Reciprocal_pd(a, residual)		- retval = RN(1.0 / a), residual = 1.0 - result * a
	//		Sqrt_pd(a, residual)			- retval = RN(sqrt(a)), residual = a - retval * retval
	inline __m128d Divide_pd(__m128d a, __m128d b, __m128d& residual)
	{
		__m128d q_ = _mm_div_pd(a, b);
		__m128d isfinite_ = isfinite_pd(q_);

		__m128d r_ = chgsign_pd(ThreeFms_pd(q_, b, a));

		residual = _mm_and_pd(isfinite_, r_);

		return q_;
	}

	inline __m128d Reciprocal_pd(__m128d op, __m128d& residual)
	{
		__m128d q_ = _mm_div_pd(_mm_set1_pd(1.0), op);
		__m128d isfinite_ = isfinite_pd(q_);

		__m128d r_ = chgsign_pd(ThreeFms_pd(q_, op, _mm_set1_pd(1.0)));

		residual = _mm_and_pd(isfinite_, r_);

		return q_;
	}

	inline __m128d Sqrt_pd(__m128d op, __m128d& residual)
	{
		__m128d q_ = _mm_sqrt_pd(op);
		__m128d isfinite_ = isfinite_pd(q_);

		__m128d r_ = chgsign_pd(ThreeFms_pd(q_, q_, op));

		residual = _mm_and_pd(isfinite_, r_);

		return q_;
	}

	//	Sums
	//
	//	The following set of functions implement compensated addition of
	//	vectors of numbers, returning the sum and the APPROXIMATE error
	//
	//		Sum_pd(len, vec, err)		- summate arbitrary values
	//		SumAbs_pd(len, vec, err)	- summate values known to be of one sign
	__m128d Sum_pd(std::size_t len, __m128d* vec, __m128d& err);
	__m128d SumAbs_pd(std::size_t len, __m128d* vec, __m128d& err);
}

#endif
