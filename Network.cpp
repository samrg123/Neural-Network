#include "Network.h"
#include <stdlib.h>

Network::Network(int iw, int hlw, int ow, int hlCount) {
	inputWidth = iw;
	hiddenLayerWidth = hlw ? hlw : inputWidth;
	outputWidth = ow; 

	numOfHiddenLayers = hlCount;
	layers = (Layer*)malloc(sizeof(Layer)*(2+hlCount));

	//NOTE(Sam): If we make hidden layer depth variable these have to move
	lastLayerInputSize = inputWidth + (numOfHiddenLayers+1)*hiddenLayerWidth; //the 1 is for the output layers input
	cachedHiddenLayerResultSize = hiddenLayerWidth*2;

	Layer* l = layers;
	*l = Layer(inputWidth, hiddenLayerWidth);

	while(++l <= layers+hlCount) {
		*l = Layer(hiddenLayerWidth, hiddenLayerWidth);
	}

	*l = Layer(hiddenLayerWidth, outputWidth);
}

void Network::enableTurbo() {
	turboMode = true;

				//TODO: ENABLE MULTITHREADING!

	cachedHiddenLayerResult = new float[cachedHiddenLayerResultSize];
	cachedLayerError = new float[outputWidth];
	lastLayerInput = new float[lastLayerInputSize];

	//TODO(Sam): we can pass this as a pointer to layers while training 
		//	 so they don't have to do an additional check
	for(Layer* l = layers; l < layers+numOfHiddenLayers+2; ++l) {
		l->cachedLayerError = new float[l->inputWidth]; 
	}

}

void Network::disableTurbo() {
	turboMode = false;

	delete[] cachedHiddenLayerResult;
	delete[] cachedLayerError;
	delete[] lastLayerInput;
	lastLayerInput = 0;

	for(Layer* l = layers; l < layers+numOfHiddenLayers+2; ++l) {
		delete[] l->cachedLayerError;
		l->cachedLayerError = 0;
	}
}

void Network::eval(float* i, float* res) {

	#define storeHlLayerInput(input, layerInputIndex) {for(int x = 0; x < hiddenLayerWidth; ++x) lastLayerInput[layerInputIndex++] = input[x];}
	#define trainEval(hlr, hlrSize) {									\
																		\
		for(int x = 0; x < inputWidth; ++x) lastLayerInput[x] = i[x]; 	\
		int layerInputIndex = inputWidth;								\
																		\
		Layer* l = layers;												\
		l->eval(i, hlr);												\
																		\
		int lResOffset = 0;												\
		while(++l <= layers+numOfHiddenLayers) {						\
			int newOffset = lResOffset+hiddenLayerWidth;				\
			if(newOffset == hlrSize) newOffset = 0; 					\
																		\
			float* lResInput = hlr+lResOffset;							\
																		\
			storeHlLayerInput(lResInput, layerInputIndex);				\
			l->eval(lResInput, hlr+newOffset);							\
																		\
			lResOffset = newOffset;										\
		}																\
																		\
		float* lResInput = hlr+lResOffset;								\
		storeHlLayerInput(lResInput, layerInputIndex);					\
																		\
		l->eval(lResInput, res);										\
	}

	// ------

	if(turboMode) trainEval(cachedHiddenLayerResult, cachedHiddenLayerResultSize)
	else {
	
		//NOTE(Sam): this is a ring buffer
		int lResSize = cachedHiddenLayerResultSize; //NOTE(Sam): this needs to move if hl width become dynamic
		float* lRes =  new float[lResSize];

		if(lastLayerInput) trainEval(lRes, lResSize)
		else {
			Layer* l = layers;
			l->eval(i, lRes);	
		
			int lResOffset = 0;
			while(++l <= layers+numOfHiddenLayers) {
				int newOffset = lResOffset+hiddenLayerWidth;
				if(newOffset == lResSize) newOffset = 0;

				l->eval(lRes+lResOffset, lRes+newOffset);
				lResOffset = newOffset;
			}
		
			l->eval(lRes+lResOffset, res);
		}

		delete[] lRes;
	}

	#undef trainEval
	#undef storeHlLayerInput
	#undef storeILayerInput
}

void Network::train(float* i, float* ans, float* res) {

	if(turboMode) {

		eval(i, res);

			//NOTE(Sam): error is .5(a-o)^2 so deriv is o-a 
		for(int x = 0; x < outputWidth; ++x) cachedLayerError[x] = res[x] - ans[x];

		float* layerError = cachedLayerError;
		float* layerOutput = res, *layerInput = lastLayerInput + lastLayerInputSize;
		
			//train from back to front
		for(Layer* l = layers+numOfHiddenLayers+1; l >= layers; --l) {
	
			layerInput-= l->inputWidth;

			l->train(&layerError, layerOutput, layerInput);

			layerOutput = layerInput;
		}

	} else {

			//NOTE(Sam): eval check for lastLayerInput to enable training!
		lastLayerInput = new float[lastLayerInputSize];
		eval(i, res);

		float* layerError = new float[outputWidth];
		for(int x = 0; x < outputWidth; ++x) layerError[x] = res[x] - ans[x];

		
		float* layerOutput = res, *layerInput = lastLayerInput + lastLayerInputSize;

		for(Layer* l = layers+numOfHiddenLayers+1; l >= layers; --l) {
			float* oldError = layerError;
			
			layerInput-= l->inputWidth;

				//this advances layerError
			l->train(&layerError, layerOutput, layerInput);
			
			layerOutput = layerInput;

			delete[] oldError;
		}

		delete[] layerError;
		delete[] lastLayerInput;
		lastLayerInput = 0;
	}
}