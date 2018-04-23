#include "Neuron.h"
#include "nMath.h"

Neuron::Neuron(int iSize) {
	numOfInputs = iSize;

	weights = new float[iSize*3];
	learnRates = weights+iSize;
	stdDevs = learnRates+iSize;

	float invISize = 1.f/iSize;
	for(int x = 0; x < iSize; ++x) {
		weights[x] = nRand() * invISize; // so large number of inputs wont make sigmoid overflow!
		learnRates[x] = (maxLearnRate-minLearnRate)/2 + minLearnRate;
		stdDevs[x] = 0;
	}
}

float Neuron::eval(float* i) {
	float alpha = 0;

	for(int x = 0; x < numOfInputs; ++x) alpha+= weights[x] * i[x];

	return sigmoid(alpha); //we could normalizethe so alpha wont make sigmoid 1 with big iSize but then back prog deriv changes!
}

void Neuron::train(float errDeriv, float* input, float* layerError) {

	for(int x = 0; x < numOfInputs; ++x) {

		layerError[x]+= errDeriv*weights[x]; //for backPropigation

		float& learnRate = learnRates[x];

			//NOTE(Sam): putting momentum on delta makes it harder to converge!
		// float delta = (1-momentum)*(learnRate * input[x]*errDeriv) + momentum*lastDelta; 
		// float delta = (learnRate * input[x]*errDeriv) + momentum*lastDelta;
		float delta = learnRate * input[x]*errDeriv;

		//minus because we train to minimize error
		weights[x]-= delta;
		// lastDelta = delta; //this is for momentum


					//TODO(Sam): think of better algoritm that reduces oscillations between layers!
		//adjustLearnRate
		float& stdDev = stdDevs[x];
		// float newStdDev = (1-momentum)*(delta >= 0 ? delta : -delta) + momentum*stdDev;
		// float newStdDev = (delta >= 0 ? delta : -delta) + momentum*stdDev;
		float newStdDev = delta >= 0 ? delta : -delta;

		if(newStdDev != stdDev) {
			if(newStdDev > stdDev) {
				float newLearnRate = learnRate + newStdDev;
				learnRate = newLearnRate <= maxLearnRate ? newLearnRate : maxLearnRate;

			} else {
				float newLearnRate = learnRate - newStdDev;
				learnRate = newLearnRate >= minLearnRate ? newLearnRate : minLearnRate;
			}

			stdDev = newStdDev;
		} 
	}

}

float Neuron::maxLearnRate = 1.5f, // 1, 10
	  Neuron::minLearnRate = .01f; //.1 .001, .5f