#pragma once
#include "Sinhvat.h"
class Dongvat :
	public Sinhvat
{
public:
	enum {
		CON_CHO,
		CON_GA
	};
public:
	Dongvat();
	virtual ~Dongvat();
	virtual void hienthiTen();
};

