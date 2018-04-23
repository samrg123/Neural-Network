#pragma once

#include "Layer.h"

class Network {
	public:
		Layer* layers;

		int inputWidth, hiddenLayerWidth, outputWidth;
		int numOfHiddenLayers;

		bool turboMode = false; //This implies training

		float* cachedHiddenLayerResult;
		int cachedHiddenLayerResultSize; //this is set in constructor!
		
		float* cachedLayerError;

		float* lastLayerInput; //this acts as a bool for training
		int lastLayerInputSize; //this is set in constructor!

		Network(int iw, int hlW, int ow, int hlCount);

		void enableTurbo();
		void disableTurbo();

		void eval(float* i, float* res);
		void train(float* i, float* ans, float* res);
};