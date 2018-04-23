#include "Network.h"
#include "NeuronDrawer.h"


#define _CRT_SECURE_NO_WARNINGS 1 //sdl checks are on by default
#include <stdio.h>
#include <math.h>

#include <windows.h>
#include <Windowsx.h>

// ----------

#define USE_GUI 1
// #define WORDS 1
#define IMG 1

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 800
#define WINDOW_TITLE "Nerual Network 2.0"


HWND window = 0, bmpWindow = 0;
BITMAPINFO backBufferInfo = {sizeof(BITMAPINFOHEADER)};
BITMAPINFO bmpBackBufferInfo = {sizeof(BITMAPINFOHEADER)};
Bitmap* windowBmp;
int *windowBackBuffer, 
	*bmpBackBuffer;



//lazy
Network* trainedNet;
bool bmpDrawingEnabled = false;
LRESULT CALLBACK BmpWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

	enum Mode {Drawing, Erasing};
	static Mode mode = Drawing;

	switch(uMsg) {

		case WM_DESTROY: {
			bmpWindow = 0;
			PostQuitMessage(0);
		} break;

		case WM_PAINT: {

			PAINTSTRUCT ps;
			BeginPaint(hwnd, &ps);

					//TODO(Sam): fix drawing dirty region
			RECT& r = ps.rcPaint;
			StretchDIBits(	ps.hdc, 
							r.left, r.top,
							r.right, r.bottom,
							0, 0,
							bmpBackBufferInfo.bmiHeader.biWidth, -bmpBackBufferInfo.bmiHeader.biHeight,
							bmpBackBuffer,
							&bmpBackBufferInfo,
							DIB_RGB_COLORS,
							SRCCOPY
						);


			EndPaint(hwnd, &ps);
		} break;

		default: {

			//Note- getting lazzy
			if(bmpDrawingEnabled) {
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
								for(int* ptr = bmpBackBuffer; ptr < bmpBackBuffer+bmpBackBufferInfo.bmiHeader.biSizeImage/4; ++ptr){
									*ptr = 0;
								}

								HDC hdc = GetDC(hwnd);
								RECT clientRect;
								GetClientRect(hwnd, &clientRect);
								StretchDIBits(	hdc, 
												0, 0,
												clientRect.right, clientRect.bottom,
												0, 0,
												bmpBackBufferInfo.bmiHeader.biWidth, -bmpBackBufferInfo.bmiHeader.biHeight,
												bmpBackBuffer,
												&bmpBackBufferInfo,
												DIB_RGB_COLORS,
												SRCCOPY
											);
								ReleaseDC(hwnd, hdc);
							} break;

							case VK_RETURN: {
								
								//create the normalized data
								int size = bmpBackBufferInfo.bmiHeader.biSizeImage/4;
								static float* normData = new float[size];

								for(int x = 0; x < size; ++x) {
									normData[x] = (bmpBackBuffer[x]&0xFF)/255.f;
									bmpBackBuffer[x] = 0; //clear the buffer
								}

								//eval
								static float* output = new float[trainedNet->outputWidth];
								trainedNet->eval(normData, output);

								//output the results
								char strBuffer[256];
								char* currStrPos = strBuffer;
								currStrPos+= sprintf(strBuffer, "{ ");
								for(int i = 0; i < trainedNet->outputWidth; ++i) {
									int stride = sprintf(currStrPos, "%1.3f, ", output[i]);
									currStrPos+= stride;
								}
								sprintf(currStrPos-2, " }"); //WARN - this fails if outputWidth is 0 .. don't have to change curr pos we delete 2 and write 2

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

					case WM_MOUSEMOVE: {

						SetCursor(LoadCursor(0, IDC_CROSS));


						if(wParam&MK_LBUTTON) {

							HDC hdc = GetDC(hwnd);
							RECT clientRect;
							GetClientRect(hwnd, &clientRect);

							int windowW = clientRect.right,
								windowH = clientRect.bottom;

							int bmpW = bmpBackBufferInfo.bmiHeader.biWidth,
								bmpH = -bmpBackBufferInfo.bmiHeader.biHeight;

							//get x&y coords - relative to upper left, we use float so we can map it below
							float x = GET_X_LPARAM(lParam);
							float y = GET_Y_LPARAM(lParam);

							//map this to our image
							int bufferX = (int)((x/windowW)*bmpW + .5f);
							int bufferY = (int)((y/windowH)*bmpH + .5f);

							//this assumes window is bigger than bmp;
							int* pixel = bmpBackBuffer + (bufferY*bmpW + bufferX);
							//start painting
							switch(mode) {
								case Drawing: {
									*pixel = 0xFFFFFFFF; //white
								} break;

								case Erasing: {
									*pixel = 0; // black
								} break;
							}
							
							StretchDIBits(	hdc, 
											0, 0,
											windowW, windowH,
											0, 0,
											bmpW, bmpH,
											bmpBackBuffer,
											&bmpBackBufferInfo,
											DIB_RGB_COLORS,
											SRCCOPY
										);

							ReleaseDC(hwnd, hdc);
						}

					} break;

					default: return DefWindowProc(hwnd, uMsg, wParam, lParam);
				
				}
			} else return DefWindowProc(hwnd, uMsg, wParam, lParam);
		}
	}

	return 0;
}

void switchBmpOutputToInput(Network* n) {

	//create actual backbuffer - our original one was tied to PackedData
	int size = bmpBackBufferInfo.bmiHeader.biSizeImage/4;
	bmpBackBuffer = new int[size];
	for(int* ptr = bmpBackBuffer; ptr < bmpBackBuffer+size; ++ptr) {
		*ptr = 0; // white
	}
	RedrawWindow(bmpWindow, 0, 0, RDW_INVALIDATE|RDW_UPDATENOW);

	trainedNet = n;
	bmpDrawingEnabled = true;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch(uMsg) {

		case WM_DESTROY: {
			window = 0;
			PostQuitMessage(0);
		} break;

		case WM_PAINT: {

			PAINTSTRUCT ps;
			BeginPaint(hwnd, &ps);

					//TODO(Sam): fix drawing dirty region
			RECT& r = ps.rcPaint;
			StretchDIBits(	ps.hdc, 
							r.left, r.top,
							r.right, r.bottom,
							0, 0,
							windowBmp->width, windowBmp->height,
							windowBackBuffer,
							&backBufferInfo,
							DIB_RGB_COLORS,
							SRCCOPY
						);


			EndPaint(hwnd, &ps);
		} break;

		default: {
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
		}
	}

	return 0;
}

DWORD WINAPI bmpWindowLoop(LPVOID lpParameter) {
	enum {FAILED_TO_REGISTER_CLASS=1, FAILED_TO_CREATE_WINDOW};

	char* windClassName = "Nerual Network BMP Window Class";

	HINSTANCE inst = GetModuleHandleA(0);
	WNDCLASSEXA wndClass {
		sizeof(WNDCLASSEXA),
		CS_HREDRAW|CS_VREDRAW,
		BmpWindowProc,
		0,
		0,
		inst,
		0,
		0,
		0,
		0,
		windClassName,
		0
	};

	if(!RegisterClassExA(&wndClass)) return FAILED_TO_REGISTER_CLASS;

	bmpWindow = CreateWindowExA(0, 
								windClassName, WINDOW_TITLE, 
								WS_VISIBLE|WS_OVERLAPPEDWINDOW, 
								CW_USEDEFAULT, CW_USEDEFAULT,
								WINDOW_WIDTH, WINDOW_HEIGHT, 
								0, 0, inst, 0);

	if(!bmpWindow) return FAILED_TO_CREATE_WINDOW;

	MSG msg;
	while(GetMessage(&msg, bmpWindow, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	UnregisterClass(windClassName, inst);
	bmpWindow = 0;
	return 0;
}

DWORD WINAPI windowLoop(LPVOID lpParameter) {
	enum {FAILED_TO_REGISTER_CLASS=1, FAILED_TO_CREATE_WINDOW};

	char* windClassName = "Nerual Network Window Class";

	HINSTANCE inst = GetModuleHandleA(0);

	WNDCLASSEXA wndClass {
		sizeof(WNDCLASSEXA),
		CS_HREDRAW|CS_VREDRAW,
		WindowProc,
		0,
		0,
		inst,
		0,
		0,
		0,
		0,
		windClassName,
		0
	};

	if(!RegisterClassExA(&wndClass)) return FAILED_TO_REGISTER_CLASS;



	window = CreateWindowExA(	0, 
								windClassName, WINDOW_TITLE, 
								WS_VISIBLE|WS_OVERLAPPEDWINDOW, 
								CW_USEDEFAULT, CW_USEDEFAULT,
								WINDOW_WIDTH, WINDOW_HEIGHT, 
								0, 0, inst, 0);

	if(!window) return FAILED_TO_CREATE_WINDOW;


	MSG msg;
	while(GetMessage(&msg, window, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	UnregisterClass(windClassName, inst);
	window = 0;
	return 0;
}

HANDLE createWindowThread(LPDWORD threadId) {
	return CreateThread(0,
						0,
						&windowLoop,
						0,
						0,
						threadId);
}

HANDLE createBmpWindowThread(LPDWORD threadId) {
	return CreateThread(0,
						0,
						&bmpWindowLoop,
						0,
						0,
						threadId);
}


char* openTxtFile(const char* filename, int* txtSize) {
	FILE* file = fopen(filename, "r");
	
	fseek(file, 0, SEEK_END);
	int fileSize = ftell(file);
	*txtSize = fileSize;
	fseek(file, 0, SEEK_SET);

	char* str = new char[fileSize];
	fread(str, 1, fileSize, file);
	fclose(file);

	return str;
}

float* strToFStr(char* str, int size, float mult) {
	
	float* fstr = new float[size];
	for(int x = 0; x < size; ++x) {
		fstr[x] = str[x] * mult;
	}

	return fstr;
}

struct PackedImageData {
	int width, height;
	int size, stride;
	char category[256];
	unsigned int pixelPos;
	float pixels[1];
};

struct PackedImageList {
	struct node {
		node* next;
		PackedImageData* image;
		int* origPixels;
	};

	void append(PackedImageData* img, int* origPixels) {
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

	node* head = 0,
		* tail = 0;
	int size = 0;
};

//NOTE - we pass iList because we want to delete image in destructor. if we return iList we make a copy and delete image when we pop the orig off the stack!
void loadNPY(PackedImageList* iList, int width, int height) {
	//get num of files in dir
	WIN32_FIND_DATA fData;

	LPSTR searchStr = "images\\*.npy";
	HANDLE file = FindFirstFile(searchStr, &fData);
	if(file == INVALID_HANDLE_VALUE) {
		printf("Failed to open files in the form: '%s'\n", searchStr);
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
		unsigned int fSize = ftell(file);
		fseek(file, curPos+header.headerLen, SEEK_SET);

		int numOfImgs = fSize/(width*height);
		if(!numOfImgs) {
			printf("Failed to find images in: '%s' {width: %d, height: %d, filesize: %d}\n", filepath, width, height, fSize);
			return;
		}

		int stride = width*height;
		int size = stride*numOfImgs;
		PackedImageData* data = (PackedImageData*) new char[sizeof(PackedImageData)-sizeof(float) + 4*size];
		data->width = width;
		data->height = height;
		data->size = size;
		data->stride = stride;
		data->pixelPos = 0;
		wsprintf(data->category, "%s", fData.cFileName);

		printf("Loading '%s'(%d @ %dx%d)\n", filepath, numOfImgs, width, height);
		
		unsigned char* rawPixels = new unsigned char[size];
		int* origPixels = new int[size];
		fread(rawPixels, 1, size, file);
		for(int i = 0; i < size; ++i) {
			unsigned char pixel = rawPixels[i];
			data->pixels[i] = pixel/255.f;
			origPixels[i] = (0xFF<<24)|(pixel<<16)|(pixel<<8)|pixel;
			//TODO: STAIN CURRENT WORKING SET!
		}
		delete[] rawPixels;
	
		iList->append(data, origPixels);
	} while(FindNextFile(file, &fData));
}


void main() {

	// const int inputSize = 3, outputSize = 3, 
	// 		  hlSize = 3, hlDepth = 1;	

	const int inputSize = 28*28, outputSize = 4,
			  hlSize = 25, hlDepth = 1;

	// unsigned int batchSize = 1, trainings = 10000000; //Note - for WORD
	unsigned int batchSize = 25, trainings = 10000;

	int verboseOutEvery = outputSize*trainings/400; //every .25%
	int fps = 60;
	const int verboseProgressRes = 5;

	float output[outputSize];
	float answers[outputSize];


	#ifdef WORDS
		//const char* txtFile = "sherlock.txt";
		const char* txtFile = "lastLeaf.txt";
		// const char* txtFile = "simpleSentence.txt";
		// const char* txtFile = "numbers.txt";

		int txtSize;
		char* txt = openTxtFile(txtFile, &txtSize);
		float fStrDiv = 255.f * inputSize;

		// fStrDiv = 255.f/26; //good for numbers
		fStrDiv = 255.f * 1; //good for text

		float fStrMult = 1.f/fStrDiv;
		float* fstr = strToFStr(txt, txtSize, fStrMult);
	#endif
	
	#ifdef IMG 

		PackedImageList imgList;
		loadNPY(&imgList, 28, 28);
		PackedImageList::node* currNode = imgList.head;
		if(!currNode) {
			printf("NO IMAGES!\n");
			return;
		}
		printf("Creating Network...\n");
		//init answers to category 1;
		answers[0] = 1.f;
		for(float* aPtr = answers+1; aPtr < answers+outputSize; ++aPtr) *aPtr = 0; 
	#endif

	Network network(inputSize, hlSize, outputSize, hlDepth);

	NeuronDrawerPref prefs;
	prefs.layerPadding = 200;
	prefs.neuronRadius = 5; //25
	prefs.neuronPadding = 5; //5
	prefs.neuronOutlineWidth = 1;  //3
	prefs.lineWidth = 1;
	windowBmp = drawNetwork(&network, &prefs, 0);


	LARGE_INTEGER freq, time;
	long long cyclesPerFrame;
	#if USE_GUI
		
		{
			Bitmap::Header& header = windowBmp->header;
			BITMAPINFOHEADER& bmiHeader = backBufferInfo.bmiHeader;
			bmiHeader.biWidth = header.width;
			bmiHeader.biHeight = header.height;
			bmiHeader.biPlanes = 1;
			bmiHeader.biBitCount = header.bitsPerPixel;
			bmiHeader.biCompression = BI_RGB;
		}

		int size = windowBmp->width*windowBmp->height;
		windowBackBuffer = new int[size];

		#ifdef IMG
			{
				BITMAPINFOHEADER& bmiHeader = bmpBackBufferInfo.bmiHeader;
				bmiHeader.biWidth = 28;
				bmiHeader.biHeight = -28;
				bmiHeader.biSizeImage = 28*28*4; //not needed but we use to preare for drawing bmp in input mode
				bmiHeader.biPlanes = 1;
				bmiHeader.biBitCount = 32;
				bmiHeader.biCompression = BI_RGB;
			}
			bmpBackBuffer = currNode->origPixels;
		#endif

		for(int x = 0; x < size; ++x) {
			windowBackBuffer[x] = windowBmp->pixels[x];
		}

		DWORD threadId;
		HANDLE windowThread = createWindowThread(&threadId);	
		#ifdef IMG
			HANDLE bmpWindowThread = createBmpWindowThread(&threadId);
		#endif
	#endif


	HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);	

	QueryPerformanceFrequency(&freq);
	cyclesPerFrame =  freq.QuadPart/fps;
	QueryPerformanceCounter(&time);


	network.enableTurbo();

	#ifdef IMG

		//TODO: add scrambling to training order!!!

		unsigned int totalCycles = trainings*outputSize;
		for(unsigned int x = 0; x < totalCycles; x++) { //NOTE - kinda a hack. we mul by output size to make sure we train each subset training times

			//train
			PackedImageData* curImg = currNode->image;
			float* curPixels = curImg->pixels+curImg->pixelPos;
			for(int i = 0; i < batchSize; ++i) {
				network.train(curPixels, answers, output);

				//check for new frame
				LARGE_INTEGER newTime;
				QueryPerformanceCounter(&newTime);
				if(newTime.QuadPart - time.QuadPart >= cyclesPerFrame) {
					
					CONSOLE_SCREEN_BUFFER_INFO cinf;
					GetConsoleScreenBufferInfo(console, &cinf);

					const int progBufferSize = verboseProgressRes+2;  //. and %s
					char progBuffer[progBufferSize];
					sprintf(progBuffer, "%0*.*f%%", progBufferSize-1, verboseProgressRes-2, 100.f*(x*batchSize + i)/(totalCycles*batchSize));
					DWORD charsWritten;
					WriteConsoleOutputCharacterA(console, progBuffer, progBufferSize, 
												 cinf.dwCursorPosition, &charsWritten);

					time = newTime;
					
					if(window) {
						drawNetwork(&network, &prefs, windowBmp);
						
						int* tmp = windowBackBuffer;
						windowBackBuffer = windowBmp->pixels;
						windowBmp->pixels = tmp;

						RedrawWindow(window, 0, 0, RDW_INVALIDATE|RDW_UPDATENOW);
					}

					//draw the bmpWindow
					if(bmpWindow) {
						bmpBackBuffer = currNode->origPixels + curImg->pixelPos;
						RedrawWindow(bmpWindow, 0, 0, RDW_INVALIDATE|RDW_UPDATENOW);
					}
				}
			}
			
			//go to next subImg
			curImg->pixelPos+= curImg->stride;
			if(curImg->pixelPos >= curImg->size) curImg->pixelPos = 0; 


			//verbout	
			if((x%verboseOutEvery) < outputSize || x >= totalCycles-outputSize) {
				
				int i = 0;
				for(PackedImageList::node* n = imgList.head; n; n = n->next) {
					float ans = answers[i];
					float out = output[i++];

					printf("\t - category %15.15s: | Ans: {%1.5f)} | Output: {%1.5f)} | Error: ~ % 010.5f%%\n",
							n->image->category, ans, out, 100.f*(out-ans)/(ans+1)); //+1 skews error but fixes /0
				}

				printf("--------------\n\n");				
			}

			//sel next node & answers
			static int aIndex = 0;
			answers[aIndex] = 0;
			if(!(currNode = currNode->next)) {
				currNode = imgList.head;
				aIndex = 0;
			} else aIndex++;

			answers[aIndex] = 1;
		}	
	#endif


	#ifdef WORDS
		for(unsigned int x = 0; x < trainings; x++) {

				//slopy but hey..
			int fStrIndex = x%abs(txtSize - (inputSize+outputSize)); //crawl input
			// fStrIndex = (x*inputSize)%abs(txtSize - (inputSize+outputSize)); //seq input
			// fStrIndex = (x*(inputSize+outputSize))%txtSize; //jump input

			float* currFStr = fstr + fStrIndex;
			char* ansTxtStr = txt + fStrIndex + inputSize;

			for(int i = 0; i < outputSize; ++i) {
				answers[i] = *(ansTxtStr+i) * (1.f/255.f);
			}

			for(int i = 0; i < batchSize; ++i) {

				network.train(currFStr, answers, output);

				//check for new frame
				LARGE_INTEGER newTime;
				QueryPerformanceCounter(&newTime);
				if(newTime.QuadPart - time.QuadPart >= cyclesPerFrame) {
					
					CONSOLE_SCREEN_BUFFER_INFO cinf;
					GetConsoleScreenBufferInfo(console, &cinf);

					const int progBufferSize = verboseProgressRes+2;  //. and %s
					char progBuffer[progBufferSize];
					sprintf(progBuffer, "%0*.*f%%", progBufferSize-1, verboseProgressRes-2, 100.f*(x*batchSize + i)/(trainings*batchSize));
					DWORD charsWritten;
					WriteConsoleOutputCharacterA(console, progBuffer, progBufferSize, 
												 cinf.dwCursorPosition, &charsWritten);

					time = newTime;
					
					if(window) {
						drawNetwork(&network, &prefs, bmp);
						
						int* tmp = backBuffer;
						backBuffer = bmp->pixels;
						bmp->pixels = tmp;

						RedrawWindow(window, 0, 0, RDW_INVALIDATE|RDW_UPDATENOW);
					}
				}
			}

			if(!(x%verboseOutEvery) || x == trainings-1) {

				printf("Input: {%-*.*s}\n", inputSize, inputSize, txt+fStrIndex);

				for(int i = 0; i < outputSize; ++i) {
					unsigned char outputVal = (unsigned char)(output[i]*255.f + .5f);
					unsigned char ansVal = (unsigned char) *(ansTxtStr + i);

					unsigned char outputChar;
					if(outputVal < ' ' || outputVal > '~') outputChar = 219;
					else outputChar = outputVal;

					unsigned char ansChar;
					if(ansVal < ' ' || ansVal > '~') ansChar = 219;
					else ansChar = ansVal;

					printf("\t - Ans: {%c(%03d)} | Output: {%c(%03d)} | Error: ~ %f%%\n", 
							ansChar,ansVal, outputChar,outputVal, ((float)(outputVal-ansVal)/(ansVal+1))*100.f); //+1 skews error but fixes /0

				}

				printf("--------------\n\n");
			}

		}
	#endif

	network.disableTurbo();	

	#ifdef WORDS
		const char* filename = "TextNetwork.bmp";
	#endif
	#ifdef IMG
		const char* filename = "ImgNetwork.bmp";
	#endif

	printf("Drawing Neural Netowrk to: %s...\n", filename);
	drawNetwork(&network, &prefs, windowBmp)->write(filename);

	#ifdef IMG
		//TODO: cleanup mem - Im too lazy right now
		switchBmpOutputToInput(&network);
		char strIn[256];
		
		printf("-----------\n\n");
		for(;;) {
			printf("type 'exit' to quit\n");
			gets_s(strIn);
			if(!strcmp(strIn, "exit")) return;
			else if(!strcmp(strIn, "openbmp")) {
			
				if(bmpWindow) printf("BMP window already open\n");
				else bmpWindowThread = createBmpWindowThread(&threadId);
			
			} else if(!strcmp(strIn, "opennet")) {
				
				if(window) printf("Network window already open\n");
				else windowThread = createWindowThread(&threadId);
			
			} else printf("Invalid Command '%s'\n", strIn);
		}
	#endif


	#ifdef WORDS
		delete[] txt;
		delete[] fstr;

		float input[inputSize];
		char inputStr[inputSize];

		for(;;) {
			char strIn[256];
			gets_s(strIn);

			int strPos = 0;
			for(int x = 0; x < inputSize; x++) {
				char c = strIn[strPos];
				
				if(c) ++strPos;	
				else {
					strPos = 0;
					c = ' ';
				}

				input[x] = (inputStr[x] = c) * fStrMult;
			}

			printf("%*.*s|", inputSize, inputSize, inputStr);

			for(int cycle = 0; cycle < 10; ++cycle) {

				network.eval(input, output);		

				for(int i = 0; i < outputSize; ++i) {
					char outChar = output[i]*255.f + .5f;
					printf("%c", ((outChar < ' ' || outChar > '~') ? 219 : outChar));

					//int index = abs(inputSize - (outputSize-i))%inputSize;
					
					if(inputSize > outputSize) {

						//TODO(Sam): support larger inputs
						printf(" |ERR| ");
					
					} else if(i < inputSize) input[i] = outChar * fStrMult;

				}
			}

			printf("\n--------\n\n");

		}
	#endif

}