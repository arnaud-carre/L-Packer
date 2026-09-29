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
#include "amigaExeLoader.h"
#include "amigaExePack.h"
#include "packEngine.h"
#include "bootstrap.h"

// TODO
// [ ] half depack in place
// [ ] full depack in place
// [ ] clear BSS

#define	D_PACKING		1
#define	D_SORT			1
#define	D_PRINT_RELOC	0

extern const char* sHunkTxt[2];


lpackElf::lpackElf()
{
}

lpackElf::lpackHunk::lpackHunk()
{
	m_dataSize = 0;
	m_allocatedSize = 0;
	m_data = nullptr;
}

lpackElf::lpackHunk::~lpackHunk()
{
	free(m_data);
}





void lpackElf::updateMergeOffsets(const AmigaExePack& elf, eHunkType t, uint32_t* lpackHunkPos, bool containsByte)
{

	int hunkCount = elf.GetHunkCount();
	for (int i = 0; i < hunkCount; i++)
	{
		const Hunk& hunk = elf.GetHunk(i);
		assert(hunk.m_id == i);
		const bool filter = containsByte ? (hunk.m_dataSize > 0) : (0 == hunk.m_dataSize);
		if (( hunk.m_hunkType == t ) && (filter))
		{
			const int slotId = memToSlot(hunk.m_memType);
			m_mergeOffsets[i] = lpackHunkPos[slotId];
			lpackHunkPos[slotId] += hunk.m_allocSize;
		}
	}
}

bool lpackElf::buildMergeOffset(const AmigaExePack& elf)
{

	int hunkCount = elf.GetHunkCount();
	assert(hunkCount > 0);

	// hunk 0 is always CODE (whatever CHIP or ANY)
	for (int i=0;i<hunkCount;i++)
		m_mergeOffsets[i] = 0;

	uint32_t lpackHunkPos[2] = {};

	updateMergeOffsets(elf, eHunkType::eHunkCode, lpackHunkPos, true);
	updateMergeOffsets(elf, eHunkType::eHunkData, lpackHunkPos, true);
	updateMergeOffsets(elf, eHunkType::eHunkCode, lpackHunkPos, false);
	updateMergeOffsets(elf, eHunkType::eHunkData, lpackHunkPos, false);
	updateMergeOffsets(elf, eHunkType::eHunkBss, lpackHunkPos, false);

	return true;
}

//  abcd
//  dcba
static uint32_t bswap32(uint32_t v)
{
	return (((v>>0)&255)<<24)|
		(((v>>8)&255)<<16)|
		(((v>>16)&255)<<8)|
		(((v >> 24) & 255) << 0);
}

void	AddBigEndianU32(uint8_t* buffer, int offset, uint32_t inc)
{
	uint32_t* p32 = (uint32_t *)(buffer + offset);
	*p32 = bswap32(bswap32(*p32) + inc);
}

bool lpackElf::hunkMerge(const AmigaExePack& elf, const Hunk& hunk)
{
	if (( hunk.m_hunkType != eHunkBss ) && (hunk.m_dataSize>0))
	{
		// copy data at the right place
		const int slotId = memToSlot(hunk.m_memType);
		lpackHunk& dst = m_hunks[slotId];
		uint32_t mergeOffset = m_mergeOffsets[hunk.m_id];
		if ( mergeOffset + hunk.m_dataSize <= dst.m_dataSize )
		{
			memcpy(dst.m_data + mergeOffset, hunk.m_data, hunk.m_dataSize);

			for (int rl=0;rl<hunk.m_relocListCount;rl++)
			{
				const RelocList& relocIn = hunk.m_relocLists[rl];
				RelocList& relocOut = dst.m_relocList[memToSlot(elf.GetHunk(relocIn.referToHunkId).m_memType)];
				uint32_t baseOffset = m_mergeOffsets[relocIn.referToHunkId];
				for (int j=0;j<relocIn.relocCount;j++)
				{
					// store the right offset to patch data
					uint32_t patchOffset = relocIn.offsets[j] + m_mergeOffsets[hunk.m_id];
					assert((hunk.m_dataSize >= 4) && (relocIn.offsets[j] <= hunk.m_dataSize - 4));
					if ((hunk.m_dataSize < 4) || (relocIn.offsets[j] > hunk.m_dataSize - 4))
						return false;
					if (!relocOut.Push(patchOffset))
						return false;
					// increment the inner code 32bits value with the hunk refered to
					AddBigEndianU32(dst.m_data, patchOffset, baseOffset);
				}
			}
		}
		else
		{
			printf("ERROR: Buffer overrun when merging hunks\n");
			return false;
		}
	}
	return true;
}

bool lpackElf::insertReloc()
{

	for (int h = 0; h < 2; h++)
	{
		lpackHunk& hunk = m_hunks[h];

		// first, sort the reloc list (to improve small deltas)
		for (int j = 0; j < 2; j++)
			hunk.m_relocList[j].Sort();

		// then output to special 3bytes AOS struct for better compression
		hunk.m_relocSize = RelocList::ComputeOutSize(hunk.m_relocList, 2);
		if (hunk.m_relocSize < 0)
			return false;

		assert(0 == (hunk.m_dataSize & 1));
		hunk.m_data = (uint8_t *)realloc(hunk.m_data, hunk.m_dataSize + hunk.m_relocSize);

		if (!RelocList::outputData(hunk.m_data + hunk.m_dataSize, hunk.m_relocSize, hunk.m_relocList, 2))
			return false;
	}
	return true;
}

bool lpackElf::Build(const AmigaExePack& elf, const Parameters& params)
{
	int hunkCount = elf.GetHunkCount();
	if (0 == hunkCount)
		return false;

	if (elf.GetHunk(0).m_hunkType != eHunkCode)
	{
		printf("ERROR: First hunk should always be a CODE hunk\n");
		return false;
	}

	if (IsLPackerExecutable(elf.GetHunk(0).m_data, elf.GetHunk(0).m_dataSize, eAmiga))
	{
		printf("ERROR: This EXE is already packed with L-Packer\n");
		return false;
	}


	m_codeMemType = elf.GetHunk(0).m_memType;

	m_slotIsChip[0] = (m_codeMemType == eMemoryType::eChipMemory);
	m_slotIsChip[1] = !m_slotIsChip[0];

	if (!buildMergeOffset(elf))
		return false;

	uint32_t dataSize[2] = {};
	uint32_t allocSize[2] = {};

	for (int h = 0; h < hunkCount; h++)
	{
		const Hunk& hunk = elf.GetHunk(h);
		const int slotId = memToSlot(hunk.m_memType);
		if (hunk.m_dataSize > 0)
		{
			const uint32_t endOffset = m_mergeOffsets[h] + hunk.m_dataSize;
			if (endOffset > dataSize[slotId])
				dataSize[slotId] = endOffset;
		}
		allocSize[slotId] += hunk.m_allocSize;
	}

	// alloc l-packer data
	for (int i=0;i<2;i++)
	{
		m_hunks[i].m_dataSize = dataSize[i];
		m_hunks[i].m_allocatedSize = allocSize[i];
		m_hunks[i].m_data = (uint8_t *)malloc(m_hunks[i].m_dataSize);
		memset(m_hunks[i].m_data, 0, m_hunks[i].m_dataSize);
	}

	printf("Merging %d hunk(s) into 2\n", hunkCount);
	for (int i=0;i<hunkCount;i++)
	{
		const Hunk& hunk = elf.GetHunk(i);
		assert(hunk.m_id == i);
		if (!hunkMerge(elf, hunk))
			return false;
	}

	if (!insertReloc())
		return false;

	return true;
}

void lpackElf::Print(bool verbose)
{
//	printf("L-Packer ELF\n");
	printf("           Mem      Disk    Alloc  Reloc\n");
	for (int h=0;h<2;h++)
	{
		const lpackHunk& hunk = m_hunks[h];
		printf("Hunk #%02d:  %s  %7d  %7d", h, sHunkTxt[slotToMem(h)], hunk.m_dataSize, hunk.m_allocatedSize);
		const int relCount = hunk.m_relocList[0].relocCount + hunk.m_relocList[1].relocCount;
		if ( relCount > 0)
		{
			printf("  %3d ( %d + %d )", relCount, hunk.m_relocList[0].relocCount, hunk.m_relocList[1].relocCount);
		}
		printf("\n");
	}
/*
	if (verbose)
	{
		for (int h=0;h<2;h++)
		{
			for (int r = 0; r < 2; r++)
			{
				RelocList& rl = m_hunks[h].m_relocList[r];
				printf("Hunk #%02d, reloc #%d: bytes, words, longs: %d, %d, %d\n", h,r, rl.byteCount, rl.wordCount, rl.longCount);
			}
		}
	}
*/
}

bool lpackElf::Save(const char* fname, const Parameters& params)
{
	ByteStream out;

	out.AddLong(0x3f3);
	out.AddLong(0);
	out.AddLong(3);		// always 3 hunks (L-Packer hunk + 2 user hunks)

	out.AddLong(0);		// first hunk
	out.AddLong(2);		// last hunk

	InputBlob blobs[2];
	for (int s = 0; s < 2; s++)
	{
		blobs[s].data = m_hunks[s].m_data;
		blobs[s].size = m_hunks[s].m_dataSize+m_hunks[s].m_relocSize;
	}

	OutputBlob outBlobs[2];

	int bestSize = multiPackAndKeepBest(eAmiga, blobs, 2, outBlobs, params);
	if (bestSize < 0)
		return false;

	assert(outBlobs[0].algo == outBlobs[1].algo);
	const Bootstrap* bootstrap = GetRightBootstrap(eAmiga, outBlobs[0].algo, params.flash, params.IsText());
	if (nullptr == bootstrap)
		return false;

	srand(bestSize);

	//
	int expectedFileSize = kAmigaElfAddSize;
	for (int i = 0; i < 2; i++)
	{
		int roundedHunkSize = (outBlobs[i].packedSize + 3)&(-4);
		roundedHunkSize += 2*4;	// 2 LONG before packed data
		expectedFileSize += roundedHunkSize;
	}
	const uint32_t bootAndTextSize = bootstrap->size + params.text.GetSize();
	expectedFileSize += (bootAndTextSize + 3)&(-4);

	// extend a bit allocated size if needed (for depack in place)
	for (int i = 0; i < 2; i++)
	{
		uint32_t requieredBufferSize = m_hunks[i].m_dataSize + m_hunks[i].m_relocSize + kDepackInPlaceMargin;
		if (requieredBufferSize > m_hunks[i].m_allocatedSize)
		{
			int diff = (requieredBufferSize - m_hunks[i].m_allocatedSize + 3)&(-4);
			if (params.verbose)
				printf("Warning: added %d memory bytes in hunk #%d for depack in-place\n", diff, i);
			m_hunks[i].m_allocatedSize += diff;
		}
	}

	// lpack boot section is put in the smallest RAM section ( CHIP or ANY )
	const int smallestHunk = (m_hunks[0].m_allocatedSize <= m_hunks[1].m_allocatedSize) ? 0 : 1;
	if (params.verbose)
		printf("L-Packer boot hunk RAM type: %s\n", sHunkTxt[slotToMem(smallestHunk)]);

	int padLen = 0;
	// handle padding. always pad second data hunk
	if ((params.targetSize > 0) && (params.pad))
	{
		padLen = (params.targetSize * 1024) - expectedFileSize;
		assert(padLen >= 0);
		printf("Adding %d random padding bytes to reach %dKiB\n", padLen, params.targetSize);
	}

	const uint32_t ramFlag = slotIsChip(smallestHunk) ? (1 << 30) : 0;

	uint32_t bootSize = padLen + ((bootAndTextSize+3)&(-4));
	if (bootSize & 3)
	{
		printf("ERROR: bootsize not rounded\n");
		return false;
	}
	assert(0 == (bootSize & 3));
	out.AddLong((bootSize>>2)|ramFlag);

	// hunk 0 is root CODE
	for (int s = 0; s <2; s++)
	{
		uint32_t hs = m_hunks[s].m_allocatedSize;
		if (m_hunks[s].m_allocatedSize & 3)
		{
			printf("ERROR: hunk #%d rounded error (%d)\n", s, m_hunks[s].m_allocatedSize);
			return false;
		}
		hs = (hs >> 2) | (slotIsChip(s) ? (1<<30) : 0);
		out.AddLong(hs);
	}	

	// write boot strap
	out.AddLong(0x3e9);
	assert(0 == (bootSize & 3));
	out.AddLong(bootSize>>2);

	const uint32_t bootCodeOffset = out.GetPos();
	out.AddData(bootstrap->data, bootstrap->size);

	// patch flash hx reg if needed
	if ((params.flash) && (params.hwReg > 0))
	{
		if ((params.hwReg < 0x002) || (params.hwReg >= 0x1fc))
		{
			printf("ERROR: bad Amiga -hwreg address ($dff%04x)\n", params.hwReg);
			return false;
		}
		if (!out.SearchAndPatchLongs(bootCodeOffset, bootstrap->size, 0xdff1a2, 0xdff000+params.hwReg, true))
			return false;
	}

	// add the text
	if (params.IsText())
	{
		out.AddData(params.text.GetRawArray(),params.text.GetSize());
		if (!out.SearchAndPatchLongs(0, bootstrap->size, 0x4afc4afc, params.text.GetSize()))
		{
			printf("Fatal ERROR adding text in lpackElf::Save\n");
			return false;
		}
	}

	out.Complete(bootAndTextSize, 4);

	const int addedPad = RoundTo(padLen, 4);
	while (padLen > 0)
	{
		out.AddByte(rand());
		padLen--;
	}
	out.Align(4);

	for (int s = 0; s < 2; s++)
	{
		if (m_hunks[s].m_allocatedSize & 3)
		{
			printf("ERROR1\n");
			return false;
		}
		uint32_t packedSize = outBlobs[s].packedSize;
		uint32_t alignedPackedSize = (packedSize + 3)&(-4);

		uint32_t hunkSize = alignedPackedSize + 8;			// 2 long added
		out.AddLong(0x3e9);
		out.AddLong(hunkSize>>2);

		if (m_hunks[s].m_allocatedSize < alignedPackedSize + 4)
		{
			printf("ERROR: bss should be at least 4 bytes\n");
			return false;
		}

		// only LZ4 requires to keep exact orignal packed size (byte aligned)
		if ( kLz4 == bootstrap->algo )
			out.AddLong(packedSize);
		else
			out.AddLong(alignedPackedSize);

		out.AddLong(m_hunks[s].m_allocatedSize);

		out.AddData(outBlobs[s].packed, uint32_t(outBlobs[s].packedSize));
		out.Complete(uint32_t(outBlobs[s].packedSize), 4);
	}	

	out.AddLong(0x3f2);

	bool ret = false;
	int realSize = out.GetSize();
	int expectedSize = ComputeFinalExeSize(eAmiga, bootstrap, outBlobs, 2, params) + addedPad;
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
