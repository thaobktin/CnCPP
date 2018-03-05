#pragma once
#include "AbstractFactory.h"
class DongvatFactory :
	public AbstractFactory
{
public:
	DongvatFactory();
	~DongvatFactory();
	virtual Sinhvat* getSinhvat(int id);
};

