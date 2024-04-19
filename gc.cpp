#include "network.h"
#include "Bitmap.h"
#include "Graphics.h"
#include "NeuronDrawer.h"

#include <windows.h>
#include <Windowsx.h>

#include <queue>
#include <string>
#include <stdio.h>
#include <cassert>
#include <filesystem>

typedef unsigned int uint;
typedef unsigned char uchar;
typedef long long int64;

#define _CRT_SECURE_NO_WARNINGS 1

#define ImgWidth			28
#define ImgHeight 		 	28
#define HiddenLayerWidth 	25 //5x5
#define HiddenLayers 	 	 1

#define ImgSize (ImgWidth*ImgHeight)

#define	npyDir "images"
#define npySearchStr npyDir##"\\*.npy"

struct TrainingParams {
	uint batchLimit;		//trains each image at most this long
	float targetError;		//trains each image until the error falls below TargetError or BatchLimit is exceeded
	float minTargetError;
	float maxTargetError;
	float dynamicMult;
};

#define TrainingsPerCategory	1000 /*1000*/	//trains each category with this many different images	

TrainingParams generatorTrainingParams =  {
	5,	//5
	50.f,
	0,
	0,
	0.f
};

TrainingParams classifierTrainingParams =  {
	20,	//20
	.05f,
	0,
	0,
	0.f
};

#define TargetFPS 			 60
#define VerboseEveryNPercent 1.f	//prints out detailed information about the network error
#define VerbosePrecision 	 5		//num of decimal places to print out

#define WindowScale		  15
#define Version "Neural Network v2.5"
#define StartGeneratorWindowOpen	1
#define StartClassifierWindowOpen   1

#define GeneratorWindowTitle	Version##" - Generator"
#define ClassifierWindowTitle	Version##" - Classifier"
#define GeneratorWindowClass 	Version##" Generator Class"
#define ClassifierWindowClass	Version##" Classifier Class"

#define SliderWindowClass "Slider Window Class"
#define SliderBgColor 	RGB(100, 100, 100)
#define SliderHeight	15
#define SliderWidth	  	60
#define SliderPadding	10
#define SliderPrecision 2

enum Error {ERROR_None, ERROR_FailedToRegisterWindowClass, ERROR_FailedToCreateWindow, ERROR_FailedToCreateWindowThread, Error_FailedToLoadNPY};

//These are computed at runtime
int numOfCategories;
uint totalTrainings;

Network *generator,
		*classifier;

struct TrainingData {
	float* out;
	float* ans;
	float* in;
} generatorData, classifierData;

struct Window {
	
	LPSTR title;
	LPSTR className;
	WNDPROC proc;
	
	int width, height;

	HWND hwnd = 0;
	DWORD threadId;

	bool inputMode = false;
	uint* backBuffer;

	BITMAPINFO bmpInfo = {
		sizeof(BITMAPINFOHEADER),
		ImgWidth,
		-ImgHeight,
		1,
		32,
		BI_RGB
	};
} classifierWindow,
  generatorWindow;


HANDLE console;

struct PackedImageData {
	int width, height;
	uint size;
	int stride;
	char category[256];
	uint pixelPos;
	float pixels[1];
};

struct PackedImageList {
	struct node {
		node* next;
		PackedImageData* image;
		uint* origPixels; //super inefficient (this is 32bpp) but whatevers
	};

	void append(PackedImageData* img, uint* origPixels) {
		node* nNode = new node{0, img, origPixels};

		if(!head) {
			head = nNode;
			tail = nNode;
		} else {
			tail->next = nNode;
			tail = nNode;
		}
		++size;
	}

	~PackedImageList() {
		if(!head) return;
		
		node* n = head;
		for(;;) {
			delete[] n->image; 
			delete[] n->origPixels;
			node* nNode = n->next;
			delete n;
			if(nNode) n = nNode;
			else return; 
		}
	}

	node *head = 0,
		 *tail = 0;
	uint size = 0;
} iList;


Error windowFunc(Window* window) {
	HINSTANCE inst = GetModuleHandleA(0);
	WNDCLASSEXA wndClass {
		sizeof(WNDCLASSEXA),
		CS_HREDRAW|CS_VREDRAW,
		window->proc,
		0,
		0,
		inst,
		0,
		0,
		0,
		0,
		window->className,
		0
	};

	if(!RegisterClassExA(&wndClass)) return ERROR_FailedToRegisterWindowClass;

	window->hwnd = CreateWindowExA(	0, 
									 window->className, window->title, 
									 WS_VISIBLE|WS_OVERLAPPEDWINDOW, 
									 CW_USEDEFAULT, CW_USEDEFAULT,
									 window->width, window->height, 
									 0, 0, inst, 0);

	if(!window->hwnd) return ERROR_FailedToCreateWindow;

	MSG msg;
	while(GetMessage(&msg, window->hwnd, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	UnregisterClass(window->className, inst);
	return ERROR_None;
}

HANDLE createWindowThread(Window* window) {
	return CreateThread(0,
						0,
						(LPTHREAD_START_ROUTINE)windowFunc,
						window,
						0,
						&window->threadId);
}

void mapClientDstRectToSrc(HWND hwnd, RECT dst, RECT* src) {
	RECT clientRect;
	GetClientRect(hwnd, &clientRect);

	float dstWidthScale	 = (float)src->right/clientRect.right,
		  dstHeightScale = (float)src->bottom/clientRect.bottom;

	src->top 	= dstHeightScale * dst.top 		+ .5f;
	src->bottom	= dstHeightScale * dst.bottom;  + .5f;
	src->left	= dstWidthScale  * dst.left;    + .5f;
	src->right	= dstWidthScale  * dst.right;   + .5f;	
}


void drawWindow(Window* w) {
	HDC hdc = GetDC(w->hwnd);
	RECT clientRect;
	GetClientRect(w->hwnd, &clientRect);

	BITMAPINFOHEADER& bmi = w->bmpInfo.bmiHeader;
	StretchDIBits(	hdc, 
					0, 0,
					clientRect.right, clientRect.bottom,
					0, 0,
					bmi.biWidth, -bmi.biHeight,
					w->backBuffer,
					&w->bmpInfo,
					DIB_RGB_COLORS,
					SRCCOPY
				);
	ReleaseDC(w->hwnd, hdc);
}

void drawGeneratorImg(int imgIndex) {
	generator->eval(generatorData.in, generatorData.out);
	for(int r = 0; r < ImgHeight; ++r) {
		for(int c = 0; c < ImgWidth; ++c) {
			uint val = (uint)(generatorData.out[r*ImgWidth + c]*255);
			generatorWindow.backBuffer[imgIndex*ImgWidth + r*ImgWidth*numOfCategories + c] = (val<<24) | (val<<16) | (val<<8) | val;
		}
	}
}

void drawGeneratorEigenVectors() {
	
	//clear the input
	for(float* ptr = generatorData.in+1; ptr < generatorData.in+numOfCategories; ++ptr) *ptr = 0;
	for(int x = 0; x < numOfCategories; ++x) {
		generatorData.in[x] = 1;
		drawGeneratorImg(x);
		generatorData.in[x] = 0;
	}
}

// NOTE - this is just to get generator window take input
struct Slider {
	HWND hwnd;
	float position;
	int image;
	int category;
};

int numOfSliders;
Slider* generatorSliders;

void drawSlider(Slider* slider) {
	HDC dc = GetDC(slider->hwnd);

	static RECT drawRect = {0, 0, SliderWidth, SliderHeight};

	const int bufferSize = SliderPrecision + 5; 
	char buffer[bufferSize];
	sprintf(buffer, "%*.*f%%", bufferSize-2, SliderPrecision, slider->position*100.f);
	
	SetBkColor(dc, SliderBgColor);
	DrawText(dc, buffer, bufferSize, &drawRect, DT_CENTER|DT_VCENTER|DT_INTERNAL);

	ReleaseDC(slider->hwnd, dc);
}

void positionSlider(Slider* slider) {
	//this is slow and should be cached but we just want this to work for tomorrow
	RECT cRect;
	GetClientRect(generatorWindow.hwnd, &cRect);

	int iWidth = cRect.right/numOfCategories;
	float slideWidth = iWidth - (SliderPadding<<1) - SliderWidth;

	SetWindowPos(	slider->hwnd, 0,
					(slider->image*iWidth + SliderPadding) + (int)(slider->position*slideWidth),
					cRect.bottom - (numOfCategories - slider->category)*(SliderHeight+SliderPadding),
					SliderWidth,
					SliderHeight,
					SWP_ASYNCWINDOWPOS);
}

void positionAndDrawSliders() {
	for(Slider* s = generatorSliders; s < generatorSliders+numOfSliders; ++s) {
		positionSlider(s);
		drawSlider(s);
	}
}

LRESULT WINAPI sliderProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	
	Slider* slider = (Slider*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
	
	static int lockedX = -1;
	switch(uMsg) {
		case WM_PAINT: {
			PAINTSTRUCT ps;
			BeginPaint(hwnd, &ps); //needed to validate the region.. I quess we could do it manualy but Im lazy
			drawSlider(slider);
			EndPaint(hwnd, &ps);
		} break;

		case WM_LBUTTONDOWN: {
			lockedX = GET_X_LPARAM(lParam);
		} break;

		case WM_MOUSELEAVE: {
			lockedX = -1;
		} break;

		case WM_MOUSEMOVE: {
			if(lockedX != -1 && (wParam&MK_LBUTTON)) {

				int newX = GET_X_LPARAM(lParam);
				int deltaX = newX - lockedX;

				float oldPos = slider->position;
				if(deltaX > 0 && oldPos >= 1) break;
				if(deltaX < 0 && oldPos <= 0) break;

				RECT gc;
				GetClientRect(generatorWindow.hwnd, &gc);
				
				float slideWidth = (float)gc.right/numOfCategories - (SliderPadding<<1) - SliderWidth;
				float newPos = oldPos + deltaX/slideWidth;

				if(newPos > 1) newPos = 1.f;
				else if(newPos < 0) newPos = 0;

				if(newPos == oldPos) break;


				slider->position = newPos;

				//Note- this repositions window so we don't chage lockedPos
				positionSlider(slider);

				
				//load the input data
				Slider* s = generatorSliders + slider->image*numOfCategories;
				for(int c = 0; c < numOfCategories; ++c, ++s) {
					generatorData.in[c] = s->position;
				}

				//update and draw the backbuffer
				drawGeneratorImg(slider->image);
				drawWindow(&generatorWindow);

				//we have to redraw all sliders... drawWindow drew over them
				for(Slider* s = generatorSliders; s < generatorSliders+numOfSliders; ++s) {
					drawSlider(s);
				}
			}
		} break;

		default: return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}

	return 0;
}

DWORD sliderThread(LPVOID pSlider) {

	Slider* slider = (Slider*) pSlider;
	slider->hwnd = CreateWindowExA(	WS_EX_TOPMOST, 
								    SliderWindowClass, 
								    0,
								    WS_VISIBLE|WS_CHILD, 
								    0, 
								    0,
								    SliderWidth,
								    SliderHeight,
								    generatorWindow.hwnd,
								    0,
								    0,
								    0);
	SetWindowLongPtrA(slider->hwnd, GWLP_USERDATA, (LONG_PTR)slider);

	positionSlider(slider);
	drawSlider(slider);

	MSG msg;
	while(GetMessage(&msg, slider->hwnd, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}

Error createGeneratorSliders() {

	generatorSliders = new Slider[(numOfSliders = numOfCategories*numOfCategories)];
	HINSTANCE gInst = (HINSTANCE) GetWindowLongPtrA(generatorWindow.hwnd, GWLP_HINSTANCE);
	WNDCLASSEX sliderClass = {
		  	sizeof(WNDCLASSEX),
		  	CS_NOCLOSE,
		  	&sliderProc,
  			0,
  			0,
  			gInst,
  			0,
  			LoadCursor(0, IDC_ARROW),
  			0,
  			0,
  			SliderWindowClass,
  			0
	};
	if(!RegisterClassExA(&sliderClass)) return ERROR_FailedToRegisterWindowClass;

	for (int iIndex = 0; iIndex < numOfCategories; ++iIndex) {
		for(int c = 0; c < numOfCategories; ++c) {

			int index = iIndex*numOfCategories +c;
			generatorSliders[index] = {0, (c == iIndex ? 1.f : 0), iIndex, c};
			CreateThread(0, 0, &sliderThread, (LPVOID)(generatorSliders+index), 0, 0);
		}
	}

	return ERROR_None;
} 


LRESULT WINAPI generatorProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch(uMsg) {

		case WM_DESTROY: {
			generatorWindow.hwnd = 0;
			PostQuitMessage(0);
		} break;

		case WM_PAINT: {

			PAINTSTRUCT ps;
			BeginPaint(hwnd, &ps);

			RECT& pRect = ps.rcPaint;
			RECT src 	= {};
			src.right 	= ImgWidth*numOfCategories;
			src.bottom 	= ImgHeight;
			mapClientDstRectToSrc(hwnd, pRect, &src);

			StretchDIBits(	ps.hdc, 
							
							pRect.left,  				pRect.top,
							(pRect.right - pRect.left), (pRect.bottom - pRect.top),
							
							src.left, 				src.top,
							(src.left - src.right), (src.top - src.bottom), //no idea why windows wants to flip us again.. but it does so we flip us back
							
							generatorWindow.backBuffer,
							&generatorWindow.bmpInfo,
							DIB_RGB_COLORS,
							SRCCOPY
						);

			EndPaint(hwnd, &ps);
		} break;

		case WM_SIZE: {
			drawWindow(&generatorWindow);
			if(generatorWindow.inputMode) positionAndDrawSliders();
		} break;

		default: return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}

	return 0;
}


void clearClassifierBackBuffer() {
	uint* bBuff = classifierWindow.backBuffer;
	for(uint* ptr = bBuff; ptr < bBuff+ImgSize; ++ptr){
		*ptr = 0;
	}
}

LRESULT WINAPI classifierProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	enum Mode {Drawing, Erasing};
	static Mode mode = Drawing;

	switch(uMsg) {

		case WM_DESTROY: {
			classifierWindow.hwnd = 0;
			PostQuitMessage(0);
		} break;

		case WM_PAINT: {

			PAINTSTRUCT ps;
			BeginPaint(hwnd, &ps);

			RECT& pRect = ps.rcPaint;
			RECT src 	= {};
			src.right 	= ImgWidth;
			src.bottom 	= ImgHeight;
			mapClientDstRectToSrc(hwnd, pRect, &src);

			StretchDIBits(	ps.hdc, 
							
							pRect.left,  				pRect.top,
							(pRect.right - pRect.left), (pRect.bottom - pRect.top),
							
							src.left, 				src.top,
							(src.left - src.right), (src.top - src.bottom), //no idea why windows wants to flip us again.. but it does so we flip us back
							
							classifierWindow.backBuffer,
							&classifierWindow.bmpInfo,
							DIB_RGB_COLORS,
							SRCCOPY
						);


			EndPaint(hwnd, &ps);
		} break;

		case WM_SIZE: {
			drawWindow(&classifierWindow);
		} break;

		default: {
			if(classifierWindow.inputMode) {
				switch(uMsg) {

					case WM_KEYDOWN: {
						switch(wParam) {
							case 'E': {
								mode = Erasing;
							} break;

							case 'B': {
								mode = Drawing;
							} break;

							case 'C': { //clear buffer
								clearClassifierBackBuffer();
								drawWindow(&classifierWindow);

							} break;

							case VK_RETURN: {
								
								//create the normalized data .. getting real lazy here, but who cares
								static float* normData = new float[ImgSize];

								for(int x = 0; x < ImgSize; ++x) {
									normData[x] = (classifierWindow.backBuffer[x]&0xFF)/255.f;
									// classifierWindow.backBuffer[x] = 0; //clear the buffer
								}

								//eval
								classifier->eval(normData, classifierData.out);

								//output the results
								char strBuffer[256];
								char* currStrPos = strBuffer;
								currStrPos+= sprintf(strBuffer, "{ ");
								for(int i = 0; i < numOfCategories; ++i) {
									int stride = sprintf(currStrPos, "%1.3f, ", classifierData.out[i]);
									currStrPos+= stride;
								}
								sprintf(currStrPos-2, " }"); // don't have to change curr pos we delete 2 and write 2

								HDC hdc = GetDC(hwnd);
								RECT clientRect;
								GetClientRect(hwnd, &clientRect);

								SetBkColor(hdc, RGB(0, 0, 0));	//black
								SetTextColor(hdc, RGB(255, 255, 255)); //white								
								DrawText(hdc, strBuffer, (currStrPos - strBuffer), &clientRect, DT_CENTER|DT_VCENTER);

								ReleaseDC(hwnd, hdc);
							}
						}
					} break;


					case WM_LBUTTONDOWN: case WM_MOUSEMOVE: {

						SetCursor(LoadCursor(0, IDC_CROSS));


						if(wParam&MK_LBUTTON) {

							RECT clientRect;
							GetClientRect(hwnd, &clientRect);

							//get x&y coords - relative to upper left, we use float so we can map it below
							float x = GET_X_LPARAM(lParam);
							float y = GET_Y_LPARAM(lParam);

							//map this to our image ... note - we don't round or else we can round out of our buffer
							int bufferX = (int)(ImgWidth*x/clientRect.right);
							int bufferY = (int)(ImgHeight*y/clientRect.bottom);

							//this assumes window is bigger than bmp;
							uint* pixel = classifierWindow.backBuffer + (bufferY*ImgWidth + bufferX);
							//start painting
							switch(mode) {
								case Drawing: {
									*pixel = 0xFFFFFFFF; //white
								} break;

								case Erasing: {
									*pixel = 0; // black
								} break;
							}
							drawWindow(&classifierWindow);
						}

					} break;

					default: return DefWindowProc(hwnd, uMsg, wParam, lParam);
				
				}
			} else return DefWindowProc(hwnd, uMsg, wParam, lParam);
		}
	}

	return 0;
}


//NOTE - we pass iList because we want to delete image in destructor. if we return iList we make a copy and delete image when we pop the orig off the stack!
void loadNPY(PackedImageList* iList, int width, int height) {
	//get num of files in dir
	WIN32_FIND_DATA fData;

	HANDLE file = FindFirstFile(npySearchStr, &fData);
	if(file == INVALID_HANDLE_VALUE) {
		printf("Failed to open files in the form: '%s'\n", npySearchStr);
		return;
	}

	#pragma pack(push, 1) 
		struct NPYHeader {
			char magic[6];
			char majorVersion = 1;
			char minorVersion = 0;
			short headerLen;
		};
	#pragma pack(pop)

	do {
		char filepath[256];
		wsprintf(filepath, "images\\%s", fData.cFileName);
		FILE* file = fopen(filepath, "rb");
		NPYHeader header;
		fread(&header, sizeof(NPYHeader), 1, file);	
		
		int curPos = ftell(file);
		fseek(file, 0, SEEK_END);
		uint fSize = ftell(file);
		fseek(file, curPos+header.headerLen, SEEK_SET);

		uint numOfImgs = fSize/(width*height);
		if(!numOfImgs) {
			printf("Failed to find images in: '%s' {width: %d, height: %d, filesize: %u}\n", filepath, width, height, fSize);
			return;
		}

		//to save ram
		if(numOfImgs > TrainingsPerCategory) numOfImgs = TrainingsPerCategory;

		int stride = width*height;
		uint size = stride*numOfImgs;
		PackedImageData* data = (PackedImageData*) new char[sizeof(PackedImageData)-sizeof(float) + 4*size]; //could overflow, but it wont
		data->width = width;
		data->height = height;
		data->size = size;
		data->stride = stride;
		data->pixelPos = 0;
		wsprintf(data->category, "%s", fData.cFileName);

		printf("Loading '%s'(%d @ %dx%d)\n", filepath, numOfImgs, width, height);
		
		uchar* rawPixels = new uchar[size];
		uint* origPixels = new uint[size];
		fread(rawPixels, 1, size, file);
		fclose(file);
		for(int i = 0; i < size; ++i) {
			uchar pixel = rawPixels[i];
			data->pixels[i] = pixel/255.f;
			origPixels[i] = (0xFF<<24)|(pixel<<16)|(pixel<<8)|pixel;
			//TODO: STAIN CURRENT WORKING SET!
		}
		delete[] rawPixels;
	
		iList->append(data, origPixels);
	} while(FindNextFile(file, &fData));


}

inline
float getPercentProgress(uint trainingCycle, int nodeIndex) {
	return 100.f*(trainingCycle*numOfCategories + nodeIndex)/totalTrainings; // not perfect, but close enough
}

LARGE_INTEGER cpuFrequency;
int64 cpuCyclesPerFrame;
LARGE_INTEGER lastWindowUpdate;
inline
void updateWindows(PackedImageList::node* trainingNode, float progress) {
	LARGE_INTEGER newTime;
	QueryPerformanceCounter(&newTime);
	if(newTime.QuadPart - lastWindowUpdate.QuadPart >= cpuCyclesPerFrame) {
		lastWindowUpdate = newTime;
	
		//update the console progress
		CONSOLE_SCREEN_BUFFER_INFO cinf;
		GetConsoleScreenBufferInfo(console, &cinf);

		const int progBufferSize = VerbosePrecision+2;  //. and %s
		char progBuffer[progBufferSize];
		sprintf(progBuffer, "%0*.*f%%", progBufferSize-1, VerbosePrecision-2, progress);
		DWORD charsWritten;
		WriteConsoleOutputCharacterA(console, progBuffer, progBufferSize, 
									 cinf.dwCursorPosition, &charsWritten);

		//update the classifier window
		if(classifierWindow.hwnd) {
			classifierWindow.backBuffer = trainingNode->origPixels+trainingNode->image->pixelPos;
			drawWindow(&classifierWindow);
		}

		//update the generateor window
		if(generatorWindow.hwnd) {
			drawGeneratorEigenVectors();
			drawWindow(&generatorWindow);
		}
	}
}

inline
float trainNet(Network* n, TrainingData data, PackedImageList::node* trainingNode, TrainingParams params, float progress) {
	float totalError;
	int remaingTrainings = params.batchLimit;
	do {
		n->train(data.in, data.ans, data.out);
		
		totalError = 0;
		for(int x = 0; x < n->outputWidth; ++x) {
			float e = data.ans[x] - data.out[x];
			totalError+= e < 0 ? -e : e;
		}
		
		updateWindows(trainingNode, progress);

	} while((--remaingTrainings && totalError > params.targetError));

	return totalError;
}

void swapWindowsToInput() {
	classifierWindow.backBuffer = new uint[ImgSize]; //researve memory for drawing.. have to be careful to not let this leak

	clearClassifierBackBuffer();
	drawWindow(&classifierWindow);
	classifierWindow.inputMode = true;

	generatorWindow.inputMode = (createGeneratorSliders() == ERROR_None);
}

struct AvgError {
	float cError = 0;
	float gError = 0;
	uint count = 0;
} avgError;

void generateClassifierSolutions(int solutionsPerCategory = 5) {

	float* result = new float[numOfCategories];
	float currentPixels[ImgSize];

	struct MSEImage {
		float mse;
		float pixels[ImgSize];

		MSEImage(float mse, float(&srcPixels)[ImgSize]): mse{mse} {
			memcpy(pixels, srcPixels, sizeof(srcPixels));
		}

		bool operator<(const MSEImage& img) const { return mse < img.mse; } 
	};

	using HeapT = std::priority_queue<MSEImage>;
	HeapT* bestResults = new HeapT[numOfCategories];

	
	// Iterate over each permutation of input and remember the best performing solutions for each category
	constexpr int xBruteForceBits = 4;
	constexpr int yBruteForceBits = 4;
	constexpr int nBruteForceBits = xBruteForceBits * yBruteForceBits;

	constexpr int xPixelsPerBruteForceBit = ImgWidth / xBruteForceBits;
	constexpr int yPixelsPerBruteForceBit = ImgHeight / xBruteForceBits;

	for(int bruteForceBits = 0; bruteForceBits < (1 << nBruteForceBits); ++bruteForceBits) {

		// create image permutation from bruteForceBits
		// Note: there is probably a faster way to do this, but this is fine for now
		for(int pixelIndex = 0; pixelIndex < ImgSize; ++pixelIndex) {

			int pixelY = pixelIndex / ImgWidth;
			int pixelX = pixelIndex - pixelY*ImgWidth;

			int bruteForceY = pixelY / yPixelsPerBruteForceBit;
			int bruteForceX = pixelX / xPixelsPerBruteForceBit;

			int bruteForceBitIndex = bruteForceY*xBruteForceBits + bruteForceX;
			float pixelValue = (bruteForceBits >> bruteForceBitIndex) & 1;

			currentPixels[pixelIndex] = pixelValue;
		}

		// evaluate image
		classifier->eval(currentPixels, result);

		// compute result MSE for each category and store best results
		for(int categoryIndex = 0; categoryIndex < numOfCategories; ++categoryIndex) {
			
			float mse = 0;
			for(int i = 0; i < numOfCategories; ++i) {
				float expectedValue = (categoryIndex == i) ? 1 : 0;
				float delta = result[i] - expectedValue;
				mse+= delta*delta;
			}

			HeapT& bestResult = bestResults[categoryIndex];			
			if(bestResult.size() < solutionsPerCategory) {

				bestResult.push(MSEImage(mse, currentPixels));

				printf("Initializing Category: %d, MSE: %f\n", categoryIndex, mse);

			} else if(bestResult.top().mse > mse) {

				bestResult.pop();
				bestResult.push(MSEImage(mse, currentPixels));
				printf("Updating Category: %d, MSE: %f\n", categoryIndex, mse);
			}
		}
	}

	// save off best images for each category
	// Warn: this destroys bestResult
	std::filesystem::path basePath("./classifierSolutions");
	std::filesystem::create_directories(basePath);
	
	PackedImageList::node* node = iList.head;
	Bitmap bmp(ImgWidth, ImgHeight);
	for(int categoryIndex = 0; categoryIndex < numOfCategories; ++categoryIndex) {
		assert(node);
		
		std::filesystem::path bmpBasename = (basePath / node->image->category);
		
		HeapT& bestResult = bestResults[categoryIndex];
		for(int resultRank = solutionsPerCategory; !bestResult.empty(); bestResult.pop(), --resultRank) {

			const MSEImage& mseImage = bestResult.top();		

			std::string filename = bmpBasename.string() + "_" + std::to_string(resultRank) + ".bmp";
			printf("Saving: '%s' with MSE of: %f\n", filename.c_str(), mseImage.mse);

			// copy floating point image to bmp buffer
			for(int i = 0; i < ImgSize; ++i) {

				float lightness = mseImage.pixels[i];

				int rgb;
				hslToRGB(0, 0, lightness, &rgb);

				bmp.pixels[i] = rgb;
			}
			
			bmp.write(filename.c_str());
		}

		printf("---\n");
		node = node->next;
	}
	assert(!node);

	delete[] result;
	delete[] bestResults;
}

void drawNetworkToFile(Network* network, const char* filename) {
	
	printf("Saving network drawing to '%s'\n", filename);

	NeuronDrawerPref prefs;
	prefs.layerPadding = 200;
	prefs.neuronRadius = 5; //25
	prefs.neuronPadding = 5; //5
	prefs.neuronOutlineWidth = 1;  //3
	prefs.lineWidth = 1;

	Bitmap* bmp = drawNetwork(network, &prefs, nullptr);
	bmp->write(filename);
	delete bmp;
}

int main() {

	//load images
	loadNPY(&iList, ImgWidth, ImgHeight);
	numOfCategories = (int)iList.size; //cache it so i don't forget what num of categories is 
	if(!numOfCategories) return Error_FailedToLoadNPY;
	totalTrainings = numOfCategories*TrainingsPerCategory;


	//Setup windows
	//wow - c++11 took away designator initialization... still not going to waste time writing a constructor
	generatorWindow = {
		GeneratorWindowTitle,
		GeneratorWindowClass,
		generatorProc,
		WindowScale*ImgWidth*numOfCategories,
		WindowScale*ImgHeight
	};
	generatorWindow.backBuffer = new uint[numOfCategories*ImgSize];
	generatorWindow.bmpInfo.bmiHeader.biWidth*= numOfCategories;

	classifierWindow = {
		ClassifierWindowTitle,
		ClassifierWindowClass,
		classifierProc,
		WindowScale*ImgWidth,
		WindowScale*ImgHeight
	};

	if(StartGeneratorWindowOpen) createWindowThread(&generatorWindow);	
	if(StartClassifierWindowOpen) createWindowThread(&classifierWindow);


	//create the networks
	printf("Creating classifier network:\t{InputSize: %d, OutputSize: %d, NumOfHiddenLayers: %d, HiddenLayerWidth: %d}...\n", 
			ImgSize, numOfCategories, HiddenLayers, HiddenLayerWidth);

	classifier = new Network(ImgSize, HiddenLayerWidth, numOfCategories, HiddenLayers);
	classifierData = {
		new float[numOfCategories],
		new float[numOfCategories]
	};
	for(float* ptr = classifierData.ans+1; ptr < classifierData.ans+numOfCategories; ++ptr) *ptr = 0; // no need to clear the first ans we do in the training loop 
	int classifierAnsIndex = 0;

	printf("Creating generator network:\t{InputSize: %d, OutputSize: %d, NumOfHiddenLayers: %d, HiddenLayerWidth: %d}...\n", 
			numOfCategories, ImgSize, HiddenLayers, HiddenLayerWidth);

	generator = new Network(numOfCategories, HiddenLayerWidth, ImgSize, HiddenLayers);
	generatorData.out = new float[ImgSize]; // we will just pass the current img for the ans


	printf("Enabling Classifier Turbo...\n");
	classifier->enableTurbo();
	printf("Enabling Generator Turbo\n...");
	generator->enableTurbo();


	// setup console and cpu Frequency
	console = GetStdHandle(STD_OUTPUT_HANDLE);
	QueryPerformanceFrequency(&cpuFrequency);
	cpuCyclesPerFrame = cpuFrequency.QuadPart/TargetFPS;
	QueryPerformanceCounter(&lastWindowUpdate);

	// start the training!
	printf("Training Networks: {numOfCategories: %d, TrainingsPerCategory: %d, BatchLimit/TargetError: {Generator: %d/%f, Classifier: %d/%f}}\n", 
			numOfCategories, TrainingsPerCategory, generatorTrainingParams.batchLimit, generatorTrainingParams.targetError, classifierTrainingParams.batchLimit, classifierTrainingParams.targetError);

	float lastVerbOut = 0;
	for(uint x = 0; x < TrainingsPerCategory; ++x) {
		for(auto node = iList.head; node; node = node->next) {

			//set the current ans
			classifierData.ans[classifierAnsIndex] = 1;

			PackedImageData* iData = node->image;
			generatorData.ans = iData->pixels+iData->pixelPos;

			//set the current input
			classifierData.in = generatorData.ans;
			generatorData.in = classifierData.ans;

			float progress = getPercentProgress(x, classifierAnsIndex);

			avgError.cError+= trainNet(classifier, classifierData, node, classifierTrainingParams, progress);
			avgError.gError+= trainNet(generator, generatorData, node, generatorTrainingParams, progress);
			avgError.count++;

			//verbose out
			if((progress-lastVerbOut) >= VerboseEveryNPercent) {
				printf("\tAverage Classifier Error: %03.*f | Average Generator Error: %03.*f\n", VerbosePrecision, avgError.cError/avgError.count, VerbosePrecision, avgError.gError/avgError.count);
				
				//WARN: this is just experimental code
				{

					// float tmp;
					// if(avgError.cError <= (tmp = classifierTrainingParams.targetError/classifierTrainingParams.dynamicMult)) classifierTrainingParams.targetError = tmp;
					// else if(avgError.cError >= (tmp = classifierTrainingParams.targetError*classifierTrainingParams.dynamicMult)) classifierTrainingParams.targetError = tmp;

					// if(avgError.cError <= (tmp = generatorTrainingParams.targetError/generatorTrainingParams.dynamicMult)) generatorTrainingParams.targetError = tmp;
					// else if(avgError.cError >= (tmp = generatorTrainingParams.targetError*generatorTrainingParams.dynamicMult)) generatorTrainingParams.targetError = tmp;


					// /---------

					// float tmp;
					// if(classifierTrainingParams.targetError >  classifierTrainingParams.minTargetError &&
					// 	avgError.cError <= (tmp = classifierTrainingParams.targetError / classifierTrainingParams.dynamicMult)) {

					// 	classifierTrainingParams.targetError = tmp > classifierTrainingParams.minTargetError ? tmp : classifierTrainingParams.minTargetError;
					
					// } else if(classifierTrainingParams.targetError < classifierTrainingParams.maxTargetError &&
					// 		  avgError.cError >= (tmp = classifierTrainingParams.targetError * classifierTrainingParams.dynamicMult)) {

					// 	classifierTrainingParams.targetError = tmp < classifierTrainingParams.maxTargetError ? tmp : classifierTrainingParams.maxTargetError;
					// }

					// if(generatorTrainingParams.targetError >  generatorTrainingParams.minTargetError &&
					// 	avgError.cError <= (tmp = generatorTrainingParams.targetError / generatorTrainingParams.dynamicMult)) {

					// 	generatorTrainingParams.targetError = tmp > generatorTrainingParams.minTargetError ? tmp : generatorTrainingParams.minTargetError;
					
					// } else if(generatorTrainingParams.targetError < generatorTrainingParams.maxTargetError &&
					// 		  avgError.cError >= (tmp = generatorTrainingParams.targetError * generatorTrainingParams.dynamicMult)) {

					// 	generatorTrainingParams.targetError = tmp < generatorTrainingParams.maxTargetError ? tmp : generatorTrainingParams.maxTargetError;
					// }
				}

				lastVerbOut = progress;
				avgError = {};
			}

			//increment the ptrs .. we do this after we train because we want to reuse classiferAnsIndex
			classifierData.ans[classifierAnsIndex] = 0; //clear the previous ans
			if(++classifierAnsIndex >= numOfCategories) classifierAnsIndex = 0;
			if((iData->pixelPos+=iData->stride) >= iData->size) iData->pixelPos = 0;
		}
	}

	// this way they are the latest ones
	drawGeneratorEigenVectors(); 
	drawWindow(&generatorWindow);

	//prepare windows for input
	swapWindowsToInput();

	//input loop
	//TODO - add commands to load and eval images from the dataset
	char strIn[256];
	printf("-----------\n\n");
	for(;;) {
		printf("type 'exit' to quit\n");
		gets_s(strIn);
		
		if(!strcmp(strIn, "exit")) { 
		
			break;
		
		} else if(!strcmp(strIn, "openClassifier")) {
			
			if(classifierWindow.hwnd) printf("Classifier window already open\n");
			else createWindowThread(&classifierWindow);

		} else if(!strcmp(strIn, "openGenerator")) {
			
			if(generatorWindow.hwnd) printf("Generator window already open\n");
			else createWindowThread(&generatorWindow);
		
		} else if(!strcmp(strIn, "generateClassifierSolutions")) {
		
			generateClassifierSolutions();

		} else if(!strcmp(strIn, "drawClassifier")) {
	
			drawNetworkToFile(classifier, "classifier.bmp");
		
		} else if(!strcmp(strIn, "drawGenerator")) {
			
			drawNetworkToFile(generator, "generator.bmp");

		} else printf("Invalid Command '%s'\n", strIn);
	}

	generator->disableTurbo();
	classifier->disableTurbo();	

	return ERROR_None;
}