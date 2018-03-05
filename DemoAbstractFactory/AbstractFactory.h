#pragma once
#include "Sinhvat.h"

class AbstractFactory
{
public:
	AbstractFactory();
	virtual ~AbstractFactory();
	virtual Sinhvat* getSinhvat(int id) = 0;
	
};

