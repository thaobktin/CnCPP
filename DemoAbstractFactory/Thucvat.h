#pragma once
#include "Sinhvat.h"

class Thucvat : public Sinhvat
{
public:
	enum {
		CAY_BANG,
		CAY_LUA
	};
public:
	Thucvat();
	virtual ~Thucvat();
	virtual void hienthiTen();
};

