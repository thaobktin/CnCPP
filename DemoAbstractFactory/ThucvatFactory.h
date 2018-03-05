#pragma once
#include "AbstractFactory.h"
class ThucvatFactory :
	public AbstractFactory
{
public:
	ThucvatFactory();
	~ThucvatFactory();
	virtual Sinhvat* getSinhvat(int id);
};

