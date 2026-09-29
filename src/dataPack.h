//--------------------------------------------------------------------
//		L-Packer
//		Amiga & Atari executable packer
//		Written by Arnaud Carré aka Leonard/Oxygene (@leonard_coder)
//		https://github.com/arnaud-carre/L-Packer
//--------------------------------------------------------------------

#pragma once
#include <stdint.h>
#include "byteStream.h"
#include "packEngine.h"

class	DataPack
{
public:

	DataPack();
	bool Build(const ByteStream& data, const Parameters& params);
	bool Save(const char* fname, const Parameters& params);

private:
	OutputBlob m_packedBlob;
};
