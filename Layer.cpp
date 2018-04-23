#include "Layer.h"
#include <stdlib.h>
#include <stdio.h>

Layer::Layer(int iWidth, int oWidth) {
	inputWidth = iWidth;
	outputWidth = oWidth;

	neurons = (Neuron*) malloc(sizeof(Neuron)*outputWidth);
	for(Neuron* n = neurons; n < neurons+outputWidth; ++n) *n = Neuron(inputWidth);
}

void Layer::eval(float* i, float* res) {
	for(int x = 0; x < outputWidth; x++) {
		res[x] = neurons[x].eval(i);
	}
}

void Layer::train(float** pLayerError, float* layerOutput, float* lastLayerInput) {
	
	float*& layerError = *pLayerError;

		//TODO(Sam): let network handle this and just pass layer the mem
	float* newLayerError = cachedLayerError ? cachedLayerError : new float[inputWidth];
	for(float* f = newLayerError; f < newLayerError+inputWidth; ++f) *f = 0;

	for(int x = 0; x < outputWidth; ++x) {

		float output = layerOutput[x];
		float errDeriv;

		//check errDeriv overFlow 
		if(output == 1) {
	
			#if(PRINT_LAYER_OVERFLOW == 1)
				printf("\t - WARNING! output == 1\n"); 
			#endif
			errDeriv = layerError[x]*derivOverflowVal;
	
		} else if(layerError[x] && !output) {

			#if(PRINT_LAYER_UNDERFLOW == 1)
				printf("\t - WARNING! output == 0 with LayerError\n");
			#endif
			errDeriv = layerError[x] * outputUnderflowVal;

		} else errDeriv = layerError[x] * output*(1-output);
		
		neurons[x].train(errDeriv, lastLayerInput, newLayerError);
	}
	
	//check layerError overflow
	for(float* f = newLayerError; f < newLayerError+inputWidth; ++f) {
		if(*f > maxLayerError) {
			#if(PRINT_LAYER_MAXERROR_OVERFLOW)
				printf("\t - WARNING! maxLayerError overflow!\n");
			#endif
			*f = maxLayerError;
		}
	}
	
	layerError = newLayerError;
}

//TODO: play with these
float Layer::outputUnderflowVal = .1f*(1-.1f);
float Layer::derivOverflowVal = .9f*(1-.9f);
float Layer::maxLayerError = 1000.f;
