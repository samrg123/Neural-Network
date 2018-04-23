#pragma once

class Bitmap {
	public:

		#pragma pack(push, 1)
		struct Header {
			char signature[2] = {'B', 'M'};
			int fileSize = 54;
			int reserverd = 0;
			int startOfImage = 54;

			int dibSize = 40;
			int width = 0;
			int height = 0;	//should be neg to place origin in upper left
			short colorPlanes = 1;
			short bitsPerPixel = 32;
			int compression = 0;
			int sizeOfBMPData = 0;
			int horizPrintRes = 2835; 	//72dpi
			int vertPrintRes = 2835;	//72 dpi
			int colorPalettes = 0;
			int importantColors = 0;
		} header;
		#pragma pack(pop)

		int width, height;
		int* pixels;

		Bitmap(char* filename);
		Bitmap(int width, int height);
		~Bitmap();

		void write(const char* filename);
};