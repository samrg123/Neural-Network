#include "Graphics.h"
#include <math.h>

void rgbToHSL(int rgb, float* h, float* s, float*l) {
	float 	r = ((rgb&0xFF0000)>>16)/255.f,
			g = ((rgb&0x00FF00)>>8)/255.f,
			b = (rgb&0x0000FF)/255.f;

	float max, min;
	if(r >= g) {
		if(g >= b) {		
			max = r;
			min = b;
		} else if(b > r) {
			max = b;
			min = g;
		} else {
			max = r;
			min = g;
		}
	} else if(r >= b) {
		max = g;
		min = b;
	} else if(b > g) {
		max = b;
		min = r;
	} else {
		max = g;
		min = r;
	}

	float lightness = (max+min)/2;
	float saturation;

	if(lightness >= .5f) saturation = (max-min)/(2.f-max-min);
	else saturation = (max-min)/(max+min);

	float hue;
	if(max == r) hue = (g-b)/(max-min);
	else if(max == g) hue = 2.f + (b-r)/(max-min);
	else hue = 4.f + (r-g)/(max-min); 

	hue*=60;
	if(hue < 0) hue+= 360; //spin around the circle	


	*h = hue;
	*s = saturation;
	*l = lightness;
}

void hslToRGB(float h, float s, float l, int* rgb) {
	float t1;
	if(l >= .5) t1 = l+s - l*s;
	else t1 = l*(1.f+s);

	float t2 = 2*l - t1;

	h/=360.f;
	float r = h + .333f,
		  g = h,
		  b = h - .333f;

	if(r > 1) r-= 1;
	if(b < 1) b+= 1;

	

	if(6*r < 1) r = t2 + (t1-t2)*6*r; 
	else if(2*r < 1) r = t1;
	else if (3*r <= 2) r = t2 + (t1-t2)*(.666f-r)*6;
	else r = t2;

	if(6*g < 1) g = t2 + (t1-t2)*6*g;
	else if(2*g < 1) g = t1;
	else if(3*g <= 2) g = t2 + (t1-t2)*(.666f-g)*6;
	else g = t2;

	if(6*b < 1) b = t2 + (t1-t2)*6*b;
	else if(2*b < 1) b = t1;
	else if(3*b <= 2) b = t2 + (t1-t2)*(.666f-b)*6;
	else b = t2;

	int red  = 	r*255;
	int green = g*255;
	int blue =	b*255;

	*rgb = (red<<16) | (green<<8) | blue;
}

void drawLine(Bitmap* bmp, int x1, int x2, int y1, int y2, int lineWidth, int color) {

	int deltaX = x2-x1,
		deltaY = y2-y1;

	int absDeltaX = abs(deltaX),
		absDeltaY = abs(deltaY);


	bool xDrawSystem = absDeltaX > absDeltaY;
	bool inLeftPlane = x1 > x2,
		 inTopPlane = y1 > y2;


	//NOTE(Sam): change of base to make draw system work in all quads
	if(	inLeftPlane && (inTopPlane || xDrawSystem) || 
	 	inTopPlane && !xDrawSystem) {

		int t = y1;
		y1 = y2;
		y2 = t;

		t = x1;
		x1 = x2;
		x2 = t;
	}

	int halfWidth = lineWidth/2;
	int roundHalfWidth = halfWidth + lineWidth%2;

	int bmpWidth  = bmp->width,
		bmpHeight = bmp->height;

	if(xDrawSystem) {

		float slope = (float)deltaY/deltaX;
		for(int x = 0; x <= absDeltaX; ++x) {
		
			int y = x*slope;
			int yCoord = y+y1,
				xCoord = x+x1;

			for(int offset = -halfWidth; offset < roundHalfWidth; ++offset) {
				
				int offsetYIndex = yCoord+offset;
				int absYIndex;

				if(offsetYIndex > 0) {
					
					if(offsetYIndex < bmpHeight) absYIndex = offsetYIndex;	
					else absYIndex = bmpHeight - 1;
				} else absYIndex = 0;

				bmp->pixels[absYIndex*bmpWidth + xCoord] = color;
			}
		}

	} else {

		float slope = (float)deltaX/deltaY;
		for(int y = 0; y <= absDeltaY; ++y) {
		
			int x = y*slope;
			int xCoord = x+x1,
				yCoord = y+y1;


			for(int offset = -halfWidth; offset < roundHalfWidth; ++offset) {
				
				int offsetXIndex = xCoord+offset;
				int absXIndex;

				if(offsetXIndex > 0) {
					if(offsetXIndex < bmpWidth) absXIndex = offsetXIndex;	
					else absXIndex = bmpWidth - 1;
				} else absXIndex = 0;

				bmp->pixels[yCoord*bmpWidth + absXIndex] = color;			
			}
		}
	}
}