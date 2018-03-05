#pragma once

#include <float.h>

class RoundingControl
{
public:
	RoundingControl(unsigned int rc)
	{
		unsigned int newCW;

		_controlfp_s(&m_oldCW, 0, 0);
		_controlfp_s(&newCW, rc, MCW_RC);
	}

	RoundingControl(RoundingControl const&) = delete;

	~RoundingControl()
	{
		_controlfp_s(&m_oldCW, m_oldCW, MCW_RC);
	}

	RoundingControl& operator=(RoundingControl const&) = delete;

private:
	unsigned int m_oldCW;
};
