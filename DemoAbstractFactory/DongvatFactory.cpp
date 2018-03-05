#include "DongvatFactory.h"
#include "Dongvat.h"
#include "Concho.h"
#include "Conga.h"
#include <iostream>

DongvatFactory::DongvatFactory()
{
	std::cout << "Tao lop DongvatFactory" << std::endl;
}


DongvatFactory::~DongvatFactory()
{
	std::cout << "Huy lop DongvatFactory" << std::endl;
}

Sinhvat* DongvatFactory::getSinhvat(int id) {
	switch (id) {
	case Dongvat::CON_CHO:
		return new Concho;
	case Dongvat::CON_GA:
		return new Conga;
		break;

	}

	return new Dongvat;
}