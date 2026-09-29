//--------------------------------------------------------------------
//		L-Packer
//		Amiga & Atari executable packer
//		Written by Arnaud Carré aka Leonard/Oxygene (@leonard_coder)
//		https://github.com/arnaud-carre/L-Packer
//--------------------------------------------------------------------

#pragma once

#define	D_LPACKER_VERSION		"1.00"

#include "byteStream.h"

struct Parameters
{
	const char* sInputFile = nullptr;
	const char* sOutputFile = nullptr;
	bool verbose = false;
	int targetSize = 0;
	int hwReg = 0;
	bool pad = false;
	bool flash = false;
	int algoMask = 0;
	bool dataMode = false;
	ByteStream text;
	bool IsText() const { return text.GetSize() > 0; }
};

enum Platform_e
{
	eAtari,
	eAmiga,
	eData,
	eMaxPlatform,
};
