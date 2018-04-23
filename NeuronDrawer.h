#include "Network.h"
#include "Bitmap.h"


struct NeuronDrawerPref {
	int neuronPadding = 10;
	int neuronRadius = 50;

	int layerPadding = 250;
	int lineWidth = 1;

	int neuronOutlineColor = 0x000000;
	int neuronOutlineWidth = 3;
	
	// int neuronColor = 0x202020;
	int neuronColor = 0x00EA00;
	int bgColor = 0xE5E5E5;
	int lineColor = 0xFF0000;
	int outputLineColor = 0x0000E0;
};

Bitmap* drawNetwork(Network* n, NeuronDrawerPref* prefs, Bitmap* bmpIn);