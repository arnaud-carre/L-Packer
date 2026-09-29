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
#include <time.h>
#include "lpacker.h"
#include "byteStream.h"
#include "atariExePack.h"
#include "packEngine.h"
#include "bootstrap.h"

// TODO
// [x] depack in place
// [x] compatible base page setup
// [x] clear BSS

AtariExePack::AtariExePack()
{
	m_originalTextSize = 0;
	m_originalBssSize = 0;
	m_data = nullptr;
}

static int	Round2Word(int v)
{
	return (v + 1)&(-2);
}

bool AtariExePack::Load(const ByteStream& prg, const Parameters& params)
{
	assert(prg.IsAtariExecutable());

	m_originalTextSize = Round2Word(prg.GetLong(2)) + Round2Word(prg.GetLong(6));
	m_originalFileSize = Round2Word(prg.GetSize());
	m_originalBssSize = Round2Word(prg.GetLong(10));
	if (0 == prg.GetWord(0x1a))		// if abs is non zero, then NO relocation table
	{
		if (prg.GetLong(14))
			printf("WARNING: PRG still contains debug symbols\n");

		uint32_t readOffset = 0x1c;
		readOffset += prg.GetLong(2);	// text section
		readOffset += prg.GetLong(6);	// data section
		readOffset += prg.GetLong(14);	// symbol table size
		if (readOffset+4 <= prg.GetSize())
		{
			uint32_t offset = prg.GetLong(readOffset);
			if (offset != 0)		// classic first 0 offset means no relocation
			{
				if (offset + 4 > m_originalTextSize)			// 32bits reloc offset after code+data section!
					return false;
				if (!m_relocList.Push(offset))
					return false;
				readOffset += 4;
				for (;;)
				{
					if (readOffset + 1 > prg.GetSize())
						return false;

					uint8_t c = prg.GetByte(readOffset++);
					if (0 == c)
						break;
					if (1 == c)
						offset += 254;
					else
					{
						offset += c;
						if (offset + 4 > m_originalTextSize)			// 32bits reloc offset after code+data section!
							return false;
						if (!m_relocList.Push(offset))
							return false;
					}
				}
			}
		}
		else
			return false;
	}

	m_relocList.Sort();

	m_lpkRelocSize = RelocList::ComputeOutSize(&m_relocList, 1);

	assert(0 == (m_originalTextSize & 1));
	m_data = (uint8_t *)malloc(m_originalTextSize + m_lpkRelocSize);

	const uint8_t* pr = (const uint8_t *)prg.GetData(0x1c+m_originalTextSize);
	assert(pr);
	memcpy(m_data, pr+0x1c, m_originalTextSize);

	if (!RelocList::outputData(m_data + m_originalTextSize, m_lpkRelocSize, &m_relocList, 1))
		return false;

	return true;
}

void AtariExePack::Print(bool verbose)
{
//	printf("L-Packer PRG\n");
	printf("           Mem      Disk    Alloc  Reloc\n");
	printf("Hunk #00:  Any   %7d  %7d", m_originalTextSize, m_originalTextSize+m_originalBssSize);
	printf("  %3d\n", m_relocList.relocCount);
	if ( verbose )
		printf("Reloc bytes, word and long: %d, %d, %d\n", m_relocList.byteCount, m_relocList.wordCount, m_relocList.longCount);
}

bool AtariExePack::Build(const Parameters& params)
{

	if (IsLPackerExecutable(m_data, m_originalTextSize, eAtari))
	{
		printf("ERROR: This EXE is already packed with L-Packer\n");
		return false;
	}

	InputBlob inBlob;
	inBlob.data = m_data;
	inBlob.size = m_originalTextSize+m_lpkRelocSize;

	int bestSize = multiPackAndKeepBest(eAtari, &inBlob, 1, &m_packedBlob, params);

	return (bestSize > 0);
}
/*
 * Original
 * | text+data                            | bss_size          |
 * 
 * L-Packer
 * | boot | packed data rounded | pad | lpk_bss               |
 * 
 * Depacking:
 * | text+data                            | reloc bytes | bss |
 * 
 */
bool AtariExePack::Save(const char* fname, const Parameters& params)
{

	// alwas align original bss size on 8 bytes (to speed up bss clear)
	m_originalBssSize = (m_originalBssSize + 7)&(-8);

	const Bootstrap* bootstrap = GetRightBootstrap(eAtari, m_packedBlob.algo, params.flash, params.IsText());
	if (nullptr == bootstrap)
		return false;

	uint32_t originalPackedSize = m_packedBlob.packedSize;
	if (params.IsText())
		originalPackedSize += params.text.GetSize();

	uint32_t roundedPackedSize = RoundTo(originalPackedSize, 4);
	const uint32_t lpkDataSize = bootstrap->size + roundedPackedSize;
	srand(lpkDataSize);

	if (lpkDataSize + 0x1c >= m_originalFileSize)
	{
		printf("ERROR: Exe may already be compressed\n");
		return false;
	}

	int padLen = 0;
	// handle padding. always pad second data hunk
	if ((params.targetSize > 0) && (params.pad))
	{
		padLen = (params.targetSize * 1024) - (lpkDataSize+0x1c);
		assert(padLen >= 0);
		if (padLen >= 0)
			printf("Adding %d random padding bytes to reach %dKiB\n", padLen, params.targetSize);
		else
		{
			printf("ERROR: Negative pad calculation (%d)\n", padLen);
			return false;
		}
	}

	// finalExecBufferSize)
	int finalExecBufferSize = m_originalTextSize + m_originalBssSize;
	int depackingBufferSize = m_originalTextSize + Round2Word(m_lpkRelocSize) + kDepackInPlaceMargin;
	if (depackingBufferSize > finalExecBufferSize)
	{
		int diff = depackingBufferSize - finalExecBufferSize;
		diff = (diff + 7)&(-8);	// always align on 8 bytes to speed up bss clear
		printf("Warning: using %d more memory bytes for in-place depacking\n", diff);
		finalExecBufferSize += diff;
	}

	int lpkBss = finalExecBufferSize - (lpkDataSize+padLen);
	if (lpkBss < 0)
	{
		printf("ERROR: lpkBss value (%d)\n", lpkBss);
		return false;
	}
	if (padLen & 1)
	{
		printf("ERROR: odd padLen (%d)\n", padLen);
		return false;
	}

	ByteStream out;
	out.AddWord(0x601a);
	out.AddLong(lpkDataSize+padLen);	// packed "code" size
	out.AddLong(0);
	out.AddLong(lpkBss);
	out.AddLong(0);
	out.AddLong(0);
	out.AddLong(0);
	out.AddWord(0xffff);		// no relocation table (the real one is packed)

	out.AddData(bootstrap->data, bootstrap->size);

	if (params.IsText())
	{
		// patch text size skip
		if (kLz4 == m_packedBlob.algo)
			out.SearchAndPatchLongs(0x1c, bootstrap->size, 0x323c4e71, 0x323c0000 + params.text.GetSize());	// patch move.w #xxxx,d1
		else
			out.SearchAndPatchLongs(0x1c, bootstrap->size, 0x41e84e71, 0x41e80000 + params.text.GetSize());	// patch lea $xxxx(a0),a0
	}

	// patch bootstrap with unpacked exe values
	const uint32_t ssize = bootstrap->size;

	if ((params.flash) && (params.hwReg > 0))
	{
		if ((params.hwReg < 0x8200) || (params.hwReg >= 0x8900))
		{
			printf("ERROR: bad Atari -hwreg address ($ffff%04x)\n", params.hwReg);
			return false;
		}
		if (!out.SearchAndPatchWords(0x1c, ssize, 0x825e, params.hwReg, true))
			return false;
	}

	// only LZ4 requires to keep exact orignal packed size (byte aligned)
	if (kLz4 == bootstrap->algo)
	{
		if (!out.SearchAndPatchLongs(0x1c, ssize, 0x4afc4afc, originalPackedSize))
			return false;
	}
	else
	{
		if (!out.SearchAndPatchLongs(0x1c, ssize, 0x4afc4afc, roundedPackedSize))
			return false;
	}

	if (!out.SearchAndPatchLongs(0x1c, ssize, 0x4afc4afc, lpkDataSize + padLen + lpkBss))
		return false;
	if (!out.SearchAndPatchLongs(0x1c, ssize, 0x4afc4afc, m_originalTextSize))
		return false;

	if (params.IsText())
		out.AddData(params.text.GetRawArray(), params.text.GetSize());

	out.AddData(m_packedBlob.packed, m_packedBlob.packedSize);

	uint32_t sizeToAlign = m_packedBlob.packedSize + params.text.GetSize();
	while (sizeToAlign & 3)
	{
		out.AddByte(rand());
		sizeToAlign++;
	}

	const int addedPad = RoundTo(padLen, 2);
	while (padLen > 0)
	{
		out.AddByte(rand());
		padLen--;
	}
	assert(0 == (out.GetPos() & 1));

	bool ret = false;
	int realSize = out.GetSize();
	int expectedSize = ComputeFinalExeSize(eAtari, bootstrap, &m_packedBlob, 1, params) + addedPad;
	if (realSize == expectedSize)
	{
		printf("Writing \"%s\"\n", fname);
		printf("L-Packer size: %dKiB (%d bytes)\n", (out.GetSize() + 1023) >> 10, out.GetSize());
		ret = out.SaveToFile(fname);
	}
	else
	{
		printf("ERROR: Internal (%d != %d)\n", realSize, expectedSize);
	}

	return ret;
}
