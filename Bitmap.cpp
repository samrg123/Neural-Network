#include "Bitmap.h"
#include <stdio.h>

Bitmap::Bitmap(char* filename) {
	FILE* file = fopen(filename, "r");

	fread(&header, sizeof(Header), 1, file);

	int numOfPixels = header.width * header.height;
	pixels = new int[numOfPixels];
	fread(pixels, sizeof(int), numOfPixels, file);

	fclose(file);
}

Bitmap::Bitmap(int width, int height) {
	this->width = width;
	this->height = height;
	header.width = width;
	header.height = -height;

	int pSize = width*height;
	header.sizeOfBMPData = 4*pSize;
	header.fileSize = header.sizeOfBMPData + (14+header.dibSize);

	pixels = new int[pSize];
}

Bitmap::~Bitmap() {
	delete[] pixels;
}

void Bitmap::write(const char* filename) {
	FILE* file = fopen(filename, "wb");
	fwrite(&header, 1, sizeof(Header), file);
	fwrite(pixels, 1, header.sizeOfBMPData, file);
	fclose(file);
}