//--------------------------------------------------------------------
//		L-Packer
//		Amiga & Atari executable packer
//		Written by Arnaud Carré aka Leonard/Oxygene (@leonard_coder)
//		https://github.com/arnaud-carre/L-Packer
//--------------------------------------------------------------------

#pragma once
#include <stdint.h>
#include "atariExePack.h"
#include "byteStream.h"
#include "relocList.h"
#include "packEngine.h"

struct Parameters;

class	AtariExePack
{
public:

	AtariExePack();

	bool Load(const ByteStream& prg, const Parameters& params);
	bool Build(const Parameters& params);
	bool Save(const char* fname, const Parameters& params);
	void Print(bool verbose);

private:
	uint32_t m_originalFileSize;		// 
	uint32_t m_originalTextSize;
	uint32_t m_originalBssSize;
	uint8_t* m_data;
	uint32_t m_lpkRelocSize;
	RelocList m_relocList;
	OutputBlob m_packedBlob;
};
