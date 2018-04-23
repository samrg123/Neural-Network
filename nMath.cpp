#include "nMath.h"
#include <math.h>
#include <random>

float sigmoid(float x) {
	float ex = expf(x);
	if(ex > 3.402e38f) return 1; //float overflowed to inf
	return ex / (ex+1);
}

float invSigmoid(float x) {
	return logf(x/(1-x));
}

float nRand() {
	return (float)rand()/RAND_MAX;
}

float nAvg(float* v, int size) {
	float sum = 0;
	for(float* f = v; f < v+size; f++) sum+= *f;
	return sum/size;
}