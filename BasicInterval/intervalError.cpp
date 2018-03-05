/*
**	intervalError.cpp
**
*/

#include "stdafx.h"
#include "intervalError.h"

#include "intervalBase.h"


namespace numerics
{

	intervalError& intervalError::operator=(intervalBase const& rhs)
	{
		value_ = rhs.value();
		return *this;
	}

}
