//--------------------------------------------------------------------
//		L-Packer
//		Amiga & Atari executable packer
//		Written by Arnaud Carré aka Leonard/Oxygene (@leonard_coder)
//		https://github.com/arnaud-carre/L-Packer
//--------------------------------------------------------------------

#pragma once
#include <stdint.h>
#include "lpacker.h"

static const uint32_t kAmigaElfAddSize = 15*4;		// hardcoded added size for elf header + 3 hunks
static const uint32_t kAtariPrgAddSize = 28;

static const uint32_t kDepackInPlaceMargin = 256;

// packing algorithms sorted from faster/low ratio to slow/good ratio
enum ePackingAlgorithm_t
{
	kLz4,
	kZx0,
	kDeflate,
	kMaxAlgorithm
};

struct InputBlob
{
	const uint8_t* data;
	int size;
};

struct OutputBlob
{
	ePackingAlgorithm_t algo;
	const uint8_t* input;
	int originalSize;
	uint8_t* packed;
	int packedSize;
	int timeMs;
	int safeDist;
	bool ok;
	bool verbose;

	bool packZx0();
	bool packDeflate();
	bool packLz4();
};
struct Parameters;
struct Bootstrap;

int multiPackAndKeepBest(Platform_e platform, const InputBlob* inputArray, int count, OutputBlob* outputArray, const Parameters& params);
int ComputeFinalExeSize(Platform_e platform, const Bootstrap* bootstrap, const OutputBlob* packedChunks, int chunkCount, const Parameters& params);
int RoundTo(int size, int align);