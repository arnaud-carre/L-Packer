//--------------------------------------------------------------------
//		L-Packer
//		Amiga & Atari executable packer
//		Written by Arnaud Carré aka Leonard/Oxygene (@leonard_coder)
//		https://github.com/arnaud-carre/L-Packer
//--------------------------------------------------------------------

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include "lpacker.h"
#include "byteStream.h"
#include "packEngine.h"
#include "dataPack.h"

DataPack::DataPack()
{
}

bool	DataPack::Build(const ByteStream& data, const Parameters& params)
{

	InputBlob inBlob;
	inBlob.data = (const uint8_t*)data.GetData(data.GetSize());
	inBlob.size = data.GetSize();

	int bestSize = multiPackAndKeepBest(eData, &inBlob, 1, &m_packedBlob, params);

	return (bestSize > 0);
}

#define	MAKE_FOURCC(a)	(uint32_t(a[0])<<24)|(uint32_t(a[1])<<16)|(uint32_t(a[2])<<8)|(uint32_t(a[3])<<0)

bool DataPack::Save(const char* fname, const Parameters& params)
{

	ByteStream out;
	switch (m_packedBlob.algo)
	{
		case ePackingAlgorithm_t::kLz4:
			out.AddLong(MAKE_FOURCC("LPZ4"));
			break;
		case ePackingAlgorithm_t::kZx0:
			out.AddLong(MAKE_FOURCC("LPX0"));
			break;
		case ePackingAlgorithm_t::kDeflate:
			out.AddLong(MAKE_FOURCC("LPDE"));
			break;
		default:
			assert(false);
			break;
	};

	out.AddLong(m_packedBlob.originalSize);
	out.AddLong(m_packedBlob.packedSize);
	out.AddData(m_packedBlob.packed, m_packedBlob.packedSize);

	printf("Writing \"%s\"\n", fname);
	printf("Raw data file size: %dKiB (%d bytes)\n", (out.GetSize() + 1023) >> 10, out.GetSize());
	bool ret = out.SaveToFile(fname);

	return ret;
}
