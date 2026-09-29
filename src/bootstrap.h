//--------------------------------------------------------------------
//		L-Packer
//		Amiga & Atari executable packer
//		Written by Arnaud Carré aka Leonard/Oxygene (@leonard_coder)
//		https://github.com/arnaud-carre/L-Packer
//--------------------------------------------------------------------

#pragma once
#include <stdint.h>
#include "packEngine.h"

struct	Bootstrap
{
	Platform_e platform;
	ePackingAlgorithm_t algo;
	bool flash;
	bool text;
	const uint8_t* data;
	uint32_t size;
};

const Bootstrap* GetRightBootstrap(Platform_e platform, ePackingAlgorithm_t algo, bool flash, bool text);
bool IsLPackerExecutable(const uint8_t* data, int size, Platform_e platform);
