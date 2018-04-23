#pragma once

#include "Bitmap.h"


void rgbToHSL(int rgb, float* h, float* s, float*l);
void hslToRGB(float h, float s, float l, int* rgb);

void drawLine(Bitmap* bmp, int x1, int x2, int y1, int y2, int lineWidth, int color);