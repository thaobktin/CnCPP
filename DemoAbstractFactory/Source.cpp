#include "AbstractFactory.h"
#include "Dongvat.h"
#include "Thucvat.h"
#include "DongvatFactory.h"
#include "ThucvatFactory.h"
#include<conio.h>
#include <iostream>

//comment this for demo
#define TAO_DONG_VAT

int main()
{
	AbstractFactory* factory = NULL;
	Sinhvat* sinhvat = NULL;

#ifdef TAO_DONG_VAT
	factory = new DongvatFactory();
	sinhvat = factory->getSinhvat(Dongvat::CON_CHO);
	sinhvat->hienthiTen();

#else
	factory = new ThucvatFactory();
	sinhvat = factory->getSinhvat(Thucvat::CAY_LUA);
	sinhvat->hienthiTen();
#endif

	delete sinhvat;
	delete factory;
	getch();
	return 0;
}