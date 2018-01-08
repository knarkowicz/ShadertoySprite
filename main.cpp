#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdint.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

struct SColor
{
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t a;
};

SColor const* ColorCmpColorArr = nullptr;
int ColorCmp(void const* ptr0, void const* ptr1)
{
	uint8_t idx0 = *((uint8_t*)ptr0);
	uint8_t idx1 = *((uint8_t*)ptr1);

	SColor c0 = ColorCmpColorArr[ idx0 ];
	SColor c1 = ColorCmpColorArr[ idx1 ];

	return c0.r + c0.g + c0.b + c0.a < c1.r + c1.g + c1.b + c1.a ? -1 : 1;
};

int main(int argc, char *argv[])
{
	if (argc < 3)
	{
		printf("Usage: ShadertoySprite.exe src.png dst.txt\n");
		return 0;
	}


	int width, height, channelNum;
	uint8_t* imgData = (uint8_t*) stbi_load(argv[1], &width, &height, &channelNum, 0);
	if (!imgData)
	{
		printf("Can't load image: %s\n", argv[1]);
		return 0;
	}

	FILE* f = fopen(argv[2], "w");
	if (!f)
	{
		printf("Can't open destination file: %s\n", argv[2]);
		return 0;
	}


	// prepare color palette
	SColor colorArr[256];
	ColorCmpColorArr = colorArr;
	uint8_t colorRemap[256];
	uint8_t colorInvRemap[256];
	unsigned colorNum = 0;
	uint8_t* imgColorIdArr = new uint8_t[ width * height ];
	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			uint8_t r = imgData[(x + y * width) * channelNum + 0];
			uint8_t g = imgData[(x + y * width) * channelNum + 1];
			uint8_t b = imgData[(x + y * width) * channelNum + 2];
			uint8_t a = 255;
			if (channelNum == 4 && imgData[(x + y * width) * channelNum + 3] < 128)
			{
				r = 0;
				g = 0;
				b = 0;
				a = 0;
			}

			bool found = false;
			for (unsigned i = 0; i < colorNum; ++i)
			{
				if (colorArr[i].r == r && colorArr[i].g == g && colorArr[i].b == b && colorArr[i].a == a)
				{
					imgColorIdArr[x + y * width] = i;
					found = true;
					break;
				}
			}

			if (!found)
			{
				colorArr[colorNum].r = r;
				colorArr[colorNum].g = g;
				colorArr[colorNum].b = b;
				colorArr[colorNum].a = a;
				imgColorIdArr[x + y * width] = colorNum;
				++colorNum;
			}
		}
	}

	// sort color palette
	for (unsigned i = 0; i < colorNum; ++i)
	{
		colorRemap[i] = i;
	}
	qsort(colorRemap, colorNum, sizeof(colorRemap[0]), ColorCmp);
	for (unsigned i = 0; i < colorNum; ++i)
	{
		colorInvRemap[colorRemap[i]] = i;
	}


	printf("Image width: %u, height: %u, color num: %u\n", width, height, colorNum);
	if (colorNum > 255)
	{
		printf("Error: image has more than 256 distinct colors\n");
		return 0;
	}
	unsigned const paletteBits = (unsigned) ceilf(log2f( colorNum + 0.5f ));
	unsigned const paletteStep = 32 / paletteBits;

	fprintf(f, "void Sprite(inout vec3 color, vec2 p)\n");
	fprintf(f, "{\n");
	fprintf(f, "    uint v = 0u;\n");

	// encode sprite
	for (int y = 0; y < height; ++y)
	{
		unsigned encData[256];
		memset(encData, 0, sizeof(encData));

		for (int x = 0; x < width; ++x)
		{
			unsigned const colorId = colorInvRemap[imgColorIdArr[x + y * width]];
			unsigned const uintId = x / paletteStep;
			unsigned const offset = paletteBits * (x - uintId * paletteStep);
			encData[uintId] |= colorId << offset;
		}

		unsigned uintNum = (width + paletteStep - 1) / paletteStep;
		while (uintNum > 1 && encData[uintNum - 1] == encData[uintNum - 2])
		{
			--uintNum;
		}

		if (uintNum > 0)
		{
			fprintf(f, "	v = p.y == %u. ? ", height - y - 1);
			for (unsigned i = 1; i < uintNum; ++i)
			{
				fprintf(f, "(p.x < %u. ? %uu : ", i * paletteStep, encData[i - 1]);
			}
			fprintf(f, "%uu", encData[uintNum - 1]);
			for (unsigned i = 1; i < uintNum; ++i)
			{
				fprintf(f, ")");
			}
			fprintf(f, " : v;\n");
		}
	}

	fprintf(f, "    v = p.x >= 0. && p.x < %u. ? v : 0u;\n", width);
	fprintf(f, "\n");


	// decoding and palette
	fprintf(f, "    float i = float((v >> uint(%u. * p.x)) & %uu);\n", paletteBits, (1 << paletteBits) - 1);
	SColor color = colorArr[colorRemap[0]];
	if (color.a > 0)
	{
		if (color.r == color.g && color.g == color.b)
		{
			fprintf(f, "    color = vec3(%.2g);\n", color.r / 255.0f);
		}
		else
		{
			fprintf(f, "    color = vec3(%.2g, %.2g, %.2g);\n", color.r / 255.0f, color.g / 255.0f, color.b / 255.0f);
		}
	}
	
	for (unsigned i = 1; i < colorNum; ++i)
	{
		color = colorArr[colorRemap[i]];
		if (color.r == color.g && color.g == color.b)
		{
			fprintf(f, "    color = i == %i. ? vec3(%.2g) : color;\n", i, color.r / 255.0f);
		}
		else
		{
			fprintf(f, "    color = i == %i. ? vec3(%.2g, %.2g, %.2g) : color;\n", i, color.r / 255.0f, color.g / 255.0f, color.b / 255.0f);
		}
	}
	fprintf(f, "}\n");

	delete[] imgColorIdArr;
	return 0;
}