#include "NeuronDrawer.h"
#include "Network.h"
#include "Neuron.h"
#include "Graphics.h"
#include <math.h>


#define NeuronSize(pPrefs) (2 * (pPrefs->neuronRadius+pPrefs->neuronPadding))
#define LayerXOffset(pPrefs) ((NeuronSize(pPrefs)+pPrefs->layerPadding)/2)
#define LayerYOffset(pBmp, pPrefs, layerWidth) ((pBmp->height - layerWidth*NeuronSize(pPrefs))/2)   



static void drawLayer(Network* n, NeuronDrawerPref* prefs, Bitmap* pBmp, int layerWidth, int layerNumber) {
	
	int vOffset = LayerYOffset(pBmp, prefs, layerWidth);

	int nSize = NeuronSize(prefs);

	int xCenter = layerNumber*(prefs->layerPadding + nSize) + LayerXOffset(prefs);
	int yCenter = vOffset + nSize/2;	

	int nRad = prefs->neuronRadius;
	int oWidth = prefs->neuronOutlineWidth;
	int innerRad = nRad-oWidth; 

	float h, s, l;
	rgbToHSL(prefs->neuronColor, &h, &s, &l);

	int width = pBmp->width;
	int*& pixels = pBmp->pixels;

	Neuron*& neurons = n->layers[layerNumber].neurons;
	for(int n = 0; n < layerWidth; ++n) {

		s = 0;
		
		int len = neurons[n].numOfInputs;
		float*& lr = neurons[n].learnRates;
		for(float* f = lr; f < lr+len; ++f) s+= *f;		
		
		s = (1.f - ((s/len)-Neuron::minLearnRate) / (Neuron::maxLearnRate-Neuron::minLearnRate));

		int nColor;
		hslToRGB(h, s, l, &nColor);

		int y, x;
		for(y = 0; y <= nRad; ++y) {
			for(x = 0; x <= nRad; ++x) {

				int color;
				int dist = sqrtf(x*x + y*y) + .5f; //TODO(Sam): optimize away dist and use r^2
				if(dist <= innerRad) color = nColor;
				else if(dist <= nRad) color = prefs->neuronOutlineColor;
				else continue;

				pixels[(yCenter+y)*width + xCenter+x] = color;
				pixels[(yCenter-y)*width + xCenter+x] = color;
				pixels[(yCenter-y)*width + xCenter-x] = color;
				pixels[(yCenter+y)*width + xCenter-x] = color;
			}
		}
	
		yCenter+= nSize; 
	}
}

static void drawConnections(Network* n, NeuronDrawerPref* prefs, Bitmap* pBmp, float maxWeight, int layerNumber) {

	int x1, x2;
	int y1, y2;
	int l1Neurons, l2Neurons;

	int nSize = NeuronSize(prefs);
	int halfNSize = nSize/2;
	int xOffset = LayerXOffset(prefs) - halfNSize;

	int lineWidth = prefs->lineWidth;
	
	if(layerNumber == -1) {
		x1 = prefs->neuronPadding;
		x2 = x1 + xOffset;
	
		l1Neurons = n->inputWidth;
		l2Neurons = n->hiddenLayerWidth;

	} else {

		int nPadding = prefs->neuronPadding;
		int lPadding = prefs->layerPadding;

		x1 = layerNumber*(lPadding+nSize) + nPadding + 2*prefs->neuronRadius + xOffset;	
		l1Neurons = n->layers[layerNumber].outputWidth;

		if(layerNumber >= n->numOfHiddenLayers+1) {
			
			//NOTE(Sam): we juset handle this special case here
			x2 = pBmp->width - nPadding;
			y1 = (pBmp->height - l1Neurons*nSize)/2 + halfNSize;

			for(int l1n = 0; l1n < l1Neurons; ++l1n) {
				drawLine(pBmp, x1, x2, y1, y1, lineWidth, prefs->outputLineColor);
				y1+= nSize;
			}
			return;

		} else {
			x2 = x1 + lPadding + 2*nPadding;
			l2Neurons = n->layers[layerNumber+1].outputWidth;
		}
	}

	y1 = LayerYOffset(pBmp, prefs, l1Neurons) + halfNSize;
	y2 = LayerYOffset(pBmp, prefs, l2Neurons) + halfNSize;

	//general case -------------
	
	float hue, saturation, lightness;
	rgbToHSL(prefs->lineColor, &hue, &saturation, &lightness);

	for(int l1n = 0; l1n < l1Neurons; ++l1n) {

		int y2n = y2;
		for(int l2n = 0; l2n < l2Neurons; ++l2n) {
				
			int color;
			saturation = fabs(n->layers[layerNumber+1].neurons[l2n].weights[l1n]) / maxWeight;
			hslToRGB(hue, saturation, lightness, &color);				
		
			drawLine(pBmp, x1, x2, y1, y2n, lineWidth, color);
			y2n+= nSize;
		}
		y1+= nSize;
	}
}


Bitmap* drawNetwork(Network* n, NeuronDrawerPref* prefs, Bitmap* bmpIn) {
	
	Bitmap* bmp;
	if(bmpIn) bmp = bmpIn;	
	else {
		int maxNuerons;
		if(n->inputWidth >= n->hiddenLayerWidth) {
			if(n->inputWidth >= n->outputWidth) maxNuerons = n->inputWidth;
			else maxNuerons = n->outputWidth;
		} else {
			if(n->hiddenLayerWidth >= n->outputWidth) maxNuerons = n->hiddenLayerWidth;
			else maxNuerons = n->outputWidth;
		}

		int nSize = NeuronSize(prefs);

		int height = maxNuerons * nSize;
		int width = (2+n->numOfHiddenLayers) * (nSize + prefs->layerPadding);

		bmp = new Bitmap(width, height);
	}

	for(int* i = bmp->pixels; (char*)i < (char*)(bmp->pixels)+bmp->header.sizeOfBMPData; ++i) *i = prefs->bgColor;

	float maxWeight = 0;
	for(Layer* l = n->layers; l < n->layers+n->numOfHiddenLayers+2; ++l) {
		for(Neuron* nur = l->neurons; nur < l->neurons+l->outputWidth; ++nur) {
			for(float* w = nur->weights; w < nur->weights+nur->numOfInputs; ++w) {
				float absw = fabs(*w);
				if(absw > maxWeight) maxWeight = absw;
			} 
		}
	}

	//draw connections
	for(int l = -1; l < n->numOfHiddenLayers+2; ++l) {
		drawConnections(n, prefs, bmp, maxWeight, l);
	}

	//draw layers
	drawLayer(n, prefs, bmp, n->hiddenLayerWidth, 0); //inputLayer has outputWidth of hiddenLayerWidth
	
	for(int l = 1; l <= n->numOfHiddenLayers; ++l) {
		drawLayer(n, prefs, bmp, n->hiddenLayerWidth, l);
	}
	drawLayer(n, prefs, bmp, n->outputWidth, n->numOfHiddenLayers+1);

	return bmp;
}

#undef LayerYOffset
#undef LayerYOffset
#undef NeuronSize