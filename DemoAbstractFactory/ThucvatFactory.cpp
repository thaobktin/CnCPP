#include "ThucvatFactory.h"
#include "Caybang.h"
#include "Caylua.h"
#include <iostream>

ThucvatFactory::ThucvatFactory()
{
	std::cout << "Tao lop ThucvatFactory" << std::endl;
}


ThucvatFactory::~ThucvatFactory()
{
	std::cout << "Huy lop ThucvatFactory" << std::endl;
}

Sinhvat* ThucvatFactory::getSinhvat(int id) {
	switch (id) {
	case Thucvat::CAY_BANG:
		return new Caybang;
		break;
	case Thucvat::CAY_LUA:
		return new Caylua;
		break;
	default:
		return new Thucvat;
		break;
	}
}