#pragma once

class Neuron {
	public:

		float* learnRates;
		float* stdDevs; //this is abs val
		static float minLearnRate, maxLearnRate;
		
		float lastDelta = 0;
		float momentum = 0.70f*0; //this makes it harder to converge absolutly

		int numOfInputs;
		float* weights;

		Neuron(int iSize);

		float eval(float* input);
		void train(float errDeriv, float* input, float* layerError);
};