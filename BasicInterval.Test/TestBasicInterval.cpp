// TestBasicInterval.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "..\BasicInterval\BasicInterval.h"
#include "..\BasicInterval\BasicIntervalRU.h"
#include "..\BasicInterval\interval.h"

#include <Windows.h>

using namespace numerics;

#include <cassert>
#include <complex>
#include <ctime>
#include <iostream>
#include <random>
#include <string>
#include <typeinfo>


#pragma optimize("", off)

int const count = 100 * 1000 * 1000;

std::default_random_engine generator;
std::uniform_real_distribution<double> distribution(-1024.0, 1024.0);
std::uniform_real_distribution<double> distributionAbs((std::numeric_limits<double>::min)(), 1024.0);

class Clock
{
public:
	Clock(int count, std::pair<double, double> overhead, std::string const& desc = "")
		: m_Desc(desc),
		m_Count(count),
		m_Overhead(overhead)
	{
		SetStartTime();
	}

	Clock(int count, std::string const& desc = "")
		: m_Desc(desc),
		  m_Count(count)
	{
		m_Overhead.first = m_Overhead.second = 0.0;
		SetStartTime();
	}

	void SetStartTime()
	{
		FILETIME creation, exit, kernel, user;

		GetThreadTimes(GetCurrentThread(), &creation, &exit, &kernel, &user);
		m_KernelTime.LowPart = kernel.dwLowDateTime;
		m_KernelTime.HighPart = kernel.dwHighDateTime;
		m_UserTime.LowPart = user.dwLowDateTime;
		m_UserTime.HighPart = user.dwHighDateTime;
	}

	std::pair<double, double> GetTime() const
	{
		FILETIME creation, exit, kernel, user;
		LARGE_INTEGER liKernel, liUser;

		GetThreadTimes(GetCurrentThread(), &creation, &exit, &kernel, &user);
		liKernel.LowPart = kernel.dwLowDateTime;
		liKernel.HighPart = kernel.dwHighDateTime;
		liUser.LowPart = user.dwLowDateTime;
		liUser.HighPart = user.dwHighDateTime;

		liKernel.QuadPart -= m_KernelTime.QuadPart;
		liUser.QuadPart -= m_UserTime.QuadPart;

		double KernelTime = 100.0 * liKernel.QuadPart;
		double UserTime = 100.0 * liUser.QuadPart;
		return std::make_pair((UserTime - m_Overhead.first) / m_Count, (KernelTime - m_Overhead.second) / m_Count);
	}

	~Clock()
	{
		if (!m_Desc.empty())
		{
			auto timespan = GetTime();

			std::cout << m_Desc << ": Kernel = " << timespan.second << ", User = " << timespan.first << " nsec\n";
		}
	}

	std::string m_Desc;
	ULARGE_INTEGER m_KernelTime, m_UserTime;
	int m_Count;
	std::pair<double, double> m_Overhead;
};

void testConsistency(int count)
{
	for (auto i = 0; i < count; i++)
	{
		double a1 = distribution(generator),
			a2 = distribution(generator),
			b1 = distribution(generator),
			b2 = distribution(generator),
			c1 = distributionAbs(generator),
			c2 = distributionAbs(generator);
		BasicInterval a((std::min)(a1, a2), (std::max)(a1, a2));
		BasicInterval b((std::min)(b1, b2), (std::max)(b1, b2));
		BasicInterval c((std::min)(c1, c2), (std::max)(c1, c2));
		BasicIntervalRU aRU((std::min)(a1, a2), (std::max)(a1, a2));
		BasicIntervalRU bRU((std::min)(b1, b2), (std::max)(b1, b2));
		BasicIntervalRU cRU((std::min)(c1, c2), (std::max)(c1, c2));
		interval aRN((std::min)(a1, a2), (std::max)(a1, a2));
		interval bRN((std::min)(b1, b2), (std::max)(b1, b2));
		interval cRN((std::min)(c1, c2), (std::max)(c1, c2));

		std::cout << i << "\r";

		auto add = a + b;
		auto addRU = aRU + bRU;
		auto addRN = aRN + bRN;

		if ((inf(add) != inf(addRU)) || (sup(add) != sup(addRU)))
			std::cout << "addition mismatch (basic, RU):" << add << " + " << addRU << "\n";

		if ((inf(add) != inf(addRN)) || (sup(add) != sup(addRN)))
			std::cout << "addition mismatch (basic, RN):" << add << " + " << addRU << "\n";

		auto sub = a - b;
		auto subRU = aRU - bRU;
		auto subRN = aRN - bRN;

		if ((inf(sub) != inf(subRU)) || (sup(sub) != sup(subRU)))
			std::cout << "subtraction mismatch (basic, RU):" << a << " - " << b << "\n";

		if ((inf(sub) != inf(subRN)) || (sup(sub) != sup(subRN)))
			std::cout << "subtraction mismatch (basic, RN):" << a << " - " << b << "\n";

		auto mul = a * b;
		auto mulRU = aRU * bRU;
		auto mulRN = aRN * bRN;

		if ((inf(mul) != inf(mulRU)) || (sup(mul) != sup(mulRU)))
			std::cout << "multiplication mismatch (basic, RU):" << a << " * " << b << "\n";

		if ((inf(mul) != inf(mulRN)) || (sup(mul) != sup(mulRN)))
			std::cout << "multiplication mismatch (basic, RN):" << a << " * " << b << "\n";

		auto recip = reciprocal(c);
		auto recipRU = reciprocal(cRU);
		auto recipRN = reciprocal(cRN);

		if ((inf(recip) != inf(recipRU)) || (sup(recip) != sup(recipRU)))
			std::cout << "reciprocal mismatch (basic, RU):" << c << "\n";

		if ((inf(recip) != inf(recipRN)) || (sup(recip) != sup(recipRN)))
			std::cout << "reciprocal mismatch (basic, RN):" << c << "\n";

		auto div = a / c;
		auto divRU = aRU / cRU;
		auto divRN = aRN / cRN;

		if ((inf(div) != inf(divRU)) || (sup(div) != sup(divRU)))
			std::cout << "division mismatch (basic, RU):" << a << " / " << c << "\n";

		if ((inf(div) != inf(divRN)) || (sup(div) != sup(divRN)))
			std::cout << "division mismatch (basic, RN):" << a << " / " << c << "\n";

		auto Sqr = sqr(a);
		auto SqrRU = sqr(aRU);
		auto SqrRN = sqr(aRN);

		if ((inf(Sqr) != inf(SqrRU)) || (sup(Sqr) != sup(SqrRU)))
			std::cout << "square mismatch (basic, RU):" << a << "\n";

		if ((inf(Sqr) != inf(SqrRN)) || (sup(Sqr) != sup(SqrRN)))
			std::cout << "square mismatch (basic, RN):" << a << "\n";

		auto Sqrt = sqrt(c);
		auto SqrtRU = sqrt(cRU);
		auto SqrtRN = sqrt(cRN);

		if ((inf(Sqrt) != inf(SqrtRU)) || (sup(Sqrt) != sup(SqrtRU)))
			std::cout << "square root mismatch (basic, RU):" << c << "\n";

		if ((inf(Sqrt) != inf(SqrtRN)) || (sup(Sqrt) != sup(SqrtRN)))
			std::cout << "square root mismatch (basic, RN):" << c << "\n";

		i++;
		i--;
	}
}

template <class T>
void testInterval()
{
	std::string const name = typeid(T).name();
	std::pair<double, double> binaryOverhead;
	std::pair<double, double> unaryOverhead;

	std::cout << name << ":\n";

	{
		Clock clock(count);

		for (auto i = 0; i < count; i++)
		{
			double a1 = distribution(generator),
				a2 = distribution(generator),
				b1 = distribution(generator),
				b2 = distribution(generator);
			T a((std::min)(a1, a2), (std::max)(a1, a2));
			T b((std::min)(b1, b2), (std::max)(b1, b2));
		}

		binaryOverhead = clock.GetTime();
		std::cout << "  empty loop - binary operators" << ": Kernel = " << binaryOverhead.second << ", User = " << binaryOverhead.first << " nsec\n";
	}

	{
		Clock clock(count);

		for (auto i = 0; i < count; i++)
		{
			double a1 = distribution(generator),
				a2 = distribution(generator);
			T a((std::min)(a1, a2), (std::max)(a1, a2));
		}

		unaryOverhead = clock.GetTime();
		std::cout << "  empty loop - unary operators" << ": Kernel = " << unaryOverhead.second << ", User = " << unaryOverhead.first << " nsec\n";
	}

	{
		Clock clock(count, binaryOverhead, "  addition");

		for (auto i = 0; i < count; i++)
		{
			double a1 = distribution(generator),
				a2 = distribution(generator),
				b1 = distribution(generator),
				b2 = distribution(generator);
			T a((std::min)(a1, a2), (std::max)(a1, a2));
			T b((std::min)(b1, b2), (std::max)(b1, b2));
			auto c = a + b;
		}
	}

	{
		Clock clock(count, binaryOverhead, "  multiplication");

		for (auto i = 0; i < count; i++)
		{
			double a1 = distribution(generator),
				a2 = distribution(generator),
				b1 = distribution(generator),
				b2 = distribution(generator);
			T a((std::min)(a1, a2), (std::max)(a1, a2));
			T b((std::min)(b1, b2), (std::max)(b1, b2));
			auto c = a * b;
		}
	}

	{
		Clock clock(count, binaryOverhead, "  division");

		for (auto i = 0; i < count; i++)
		{
			double a1 = distribution(generator),
				a2 = distribution(generator),
				b1 = std::abs(distribution(generator)),
				b2 = std::abs(distribution(generator));
			T a((std::min)(a1, a2), (std::max)(a1, a2));
			T b((std::min)(b1, b2), (std::max)(b1, b2));
			auto c = a / b;
		}
	}

	{
		Clock clock(count, unaryOverhead, "  square");

		for (auto i = 0; i < count; i++)
		{
			double a1 = distribution(generator),
				a2 = distribution(generator);
			T a((std::min)(a1, a2), (std::max)(a1, a2));
			auto c = sqr(a);
		}
	}

	{
		Clock clock(count, unaryOverhead, "  square root");

		for (auto i = 0; i < count; i++)
		{
			double a1 = distribution(generator),
				a2 = distribution(generator);
			T a((std::min)(a1, a2), (std::max)(a1, a2));
			auto c = sqrt(a);
		}
	}

	{
		Clock clock(count, binaryOverhead, "  hypot");

		for (auto i = 0; i < count; i++)
		{
			double a1 = distribution(generator),
				a2 = distribution(generator),
				b1 = distribution(generator),
				b2 = distribution(generator);
			T a((std::min)(a1, a2), (std::max)(a1, a2));
			T b((std::min)(b1, b2), (std::max)(b1, b2));
			auto hypot = sqrt(sqr(a) + sqr(a));
		}
	}

	std::cout << "\n";
}

int _tmain(int argc, _TCHAR* argv[])
{
	testConsistency(1000 * 1000);

	testInterval<BasicInterval>();
	testInterval<BasicIntervalRU>();
	testInterval<interval>();

	return 0;
}

#pragma optimize("", on)
