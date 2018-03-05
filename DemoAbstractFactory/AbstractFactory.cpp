#include "AbstractFactory.h"
#include <iostream>

AbstractFactory::AbstractFactory()
{
	std::cout << "Tao lop AbstractFactory" << std::endl;
}


AbstractFactory::~AbstractFactory()
{
	std::cout << "Huy lop AbstractFactory" << std::endl;
}
