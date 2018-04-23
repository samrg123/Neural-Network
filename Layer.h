#include "Neuron.h"

#define PRINT_LAYER_OVERFLOW  			0
#define PRINT_LAYER_UNDERFLOW		  	0
#define PRINT_LAYER_MAXERROR_OVERFLOW	0

class Layer {
	public:
		Neuron* neurons;
		int inputWidth, outputWidth;

		static float outputUnderflowVal; //if output goes to 0 but layer error != 0;
		static float derivOverflowVal; // if derivative goes to 0 this kicks in to pull Nuerons back in range
		static float maxLayerError;

		float* cachedLayerError = 0;

		// --------

		Layer(int iWidth, int oWidth);

		void eval(float* i, float* res);

		//WARN(Sam): trainMode MUST be true, and eval must be executed before call
		//			 train also modifies pLayerError to ease backwards propigation.
		void train(float** pLayerError, float* layerOutput, float* lastLayerInput);
};