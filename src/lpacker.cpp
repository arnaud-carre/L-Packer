//--------------------------------------------------------------------
//		L-Packer
//		Amiga & Atari executable packer
//		Written by Arnaud Carré aka Leonard/Oxygene (@leonard_coder)
//		https://github.com/arnaud-carre/L-Packer
//--------------------------------------------------------------------
#define _CRT_SECURE_NO_WARNINGS
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include "lpacker.h"
#include "byteStream.h"
#include "amigaExePack.h"
#include "amigaExeLoader.h"
#include "atariExePack.h"
#include "dataPack.h"


void Help()
{
	printf("Usage:\n"
		   "\tL-Packer <src> <dst> [-options]\n"
		   "Options:\n"
		   "\t-text <txt filename> : display a text during depacking\n"
		   "\t-target <x> : target size limit in KiB (ex -target 64)\n"
		   "\t-pad : add random bytes to pad up to the target size\n"
		   "\t-flash : color flash when depacking\n"
		   "\t-hwreg <hex> : flash hw reg (ex -hwreg 8240 (atari) or -hwreg 180 (amiga))\n"
		   "\t-data : raw data mode (no executable)\n"
		   "\t-deflate : include DEFLATE algorithm (by default)\n"
		   "\t-zx0 : include ZX0 algorithm (by default)\n"
		   "\t-lz4 : include LZ4 algorithm (by default)\n"
		   "\t-v : verbose mode\n\n");
	printf("More info: https://github.com/arnaud-carre/L-Packer\n\n");
}

bool	ParseArgs(int argc, char* argv[], Parameters& params)
{

	int nameCount = 0;
	int argId = 1;

	params.algoMask = 0;

	while (argId < argc)
	{
		if ('-' == argv[argId][0])
		{
			if (0 == strcmp(argv[argId], "-v"))
			{
				params.verbose = true;
			}
			else if (0 == strcmp(argv[argId], "-pad"))
			{
				params.pad = true;
			}
			else if (0 == strcmp(argv[argId], "-flash"))
			{
				params.flash = true;
			}
			else if (0 == strcmp(argv[argId], "-text"))
			{
				if (argId < argc - 1)
				{
					if (argv[argId + 1][0] != '-')
					{
						if (!params.text.LoadFromFile(argv[argId + 1]))
							return false;
						argId++;
					}
					else
					{
						printf("ERROR: -text <txt filename>\n");
						return false;
					}
				}
				else
				{
					printf("ERROR: -text <txt filename>\n");
					return false;
				}
			}
			else if (0 == strcmp(argv[argId], "-deflate"))
			{
				params.algoMask |= 1 << kDeflate;
			}
			else if (0 == strcmp(argv[argId], "-zx0"))
			{
				params.algoMask |= 1 << kZx0;
			}
			else if (0 == strcmp(argv[argId], "-lz4"))
			{
				params.algoMask |= 1 << kLz4;
			}
			else if (0 == strcmp(argv[argId], "-target"))
			{
				if (argId < argc - 1)
				{
					int kb = atoi(argv[argId+1]);
					if (kb > 0)
						params.targetSize = kb;
				}
				if ((argId >= argc - 1) || (0 == params.targetSize))
				{
					printf("ERROR: -target arg\n");
					return false;
				}
				argId++;
			}
			else if (0 == strcmp(argv[argId], "-hwreg"))
			{
				if (argId < argc - 1)
				{
					unsigned int value;
					sscanf(argv[argId+1], "%x", &value);
					if ( value > 0 )
						params.hwReg = value;
				}
				if ((argId >= argc - 1) || (0 == params.hwReg))
				{
					printf("ERROR: -hwreg arg\n");
					return false;
				}
				argId++;
			}
			else if (0 == strcmp(argv[argId], "-data"))
			{
				params.dataMode = true;
			}
			else
			{
				printf("Unknown option \"%s\"\n", argv[argId]);
				return false;
			}
		}
		else
		{
			nameCount++;
			if (1 == nameCount)
				params.sInputFile = argv[argId];
			else if (2 == nameCount)
				params.sOutputFile = argv[argId];
			else
			{
				printf("Too much files specified (\"%s\")\n", argv[argId]);
				return false;
			}
		}
		argId++;
	}

	// if not specified, allow ALL packing algorithms
	if (0 == params.algoMask)
		params.algoMask = (1 << kMaxAlgorithm)-1;

	if (params.pad && (params.targetSize <= 0))
	{
		printf("ERROR: You should specify a target size (-t) when using -pad\n");
		return false;
	}

	if (params.dataMode)
	{
		if ((params.targetSize > 0) || (params.pad))
		{
			printf("ERROR: -data not compatible with -t or -pad\n");
			return false;
		}
		if (params.IsText())
		{
			printf("ERROR: can't use -text option with -data mode\n");
			return false;
		}
	}

	return (2==nameCount);
}


int main(int argc, char* argv[])
{
#ifdef _WIN32
    // Force the Windows console to interpret stdout as UTF-8 (Code Page 65001)
	// not needed on Linux or MacOS (utf-8 console by default)
    SetConsoleOutputCP(CP_UTF8);
#endif	
	
	printf("L-Packer " D_LPACKER_VERSION " - Atari & Amiga executable cruncher\n"
		   "Written by Arnaud Carré aka Leonard/Oxygene\n\n");

	int ret = 1;	//error by default

	Parameters params;
	if (ParseArgs(argc, argv, params))
	{
		ByteStream binFile;
		if (binFile.LoadFromFile(params.sInputFile))
		{
			if (params.targetSize > 0)
				printf("Target size specified: %dKiB\n", params.targetSize);

			printf("Using %d chars display text\n", params.text.GetSize());

			if (params.dataMode)
			{
				DataPack dataPack;
				if (dataPack.Build(binFile, params))
					if (dataPack.Save(params.sOutputFile, params))
						ret = 0;		// ok
			}
			else if (binFile.IsAmigaExecutable())
			{
				printf("AMIGA Executable detected\n");
				AmigaExePack elf;
				if ( elf.Load(binFile, params.verbose))
				{
					if ( params.verbose)
						elf.Print();
					lpackElf out;
					if (out.Build(elf, params))
					{
						out.Print(params.verbose);
						if (out.Save(params.sOutputFile, params))
							ret = 0;		// ok
					}
				}
			}
			else if (binFile.IsAtariExecutable())
			{
				printf("ATARI Executable detected\n");
				AtariExePack prg;
				if (prg.Load(binFile, params))
				{
					if (params.IsText())
					{
						// add "clear screen" and "end 0 char" to display text
						ByteStream nbs;
						nbs.AddByte(27);
						nbs.AddByte('E');	// 27,'E' is "clear screen"command
						nbs.AddData(params.text.GetRawArray(), params.text.GetSize());
						nbs.AddByte(0);		// ending 0
						params.text.SetStream(nbs);
					}

					prg.Print(params.verbose);
					if ( prg.Build(params))
						if (prg.Save(params.sOutputFile, params))
							ret = 0;		// ok
				}
				else
					printf("ERROR while parsing \"%s\"\n", params.sInputFile);
			}
			else
			{
				printf("ERROR: Not an Amiga or Atari executable\n"
					   "(maybe you want to use -data mode)\n");
			}
		}
		else
			printf("ERROR: Unable to load \"%s\"\n", params.sInputFile);
	}
	else
		Help();

	return ret;
}
