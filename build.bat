set path="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build";%path%

call vcvarsall amd64


set gcFiles=gc.cpp Bitmap.cpp Graphics.cpp Layer.cpp Network.cpp Neuron.cpp NeuronDrawer.cpp nMath.cpp
set mainFiles=main.cpp Bitmap.cpp Graphics.cpp Layer.cpp Network.cpp Neuron.cpp NeuronDrawer.cpp nMath.cpp


@REM cleanup object files
del *.obj

@REM Build GC
del gc.exe
cl /O2 /std:c++17 /EHsc %gcFiles% User32.lib Gdi32.lib


@REM Build main
del main.exe
cl /O2 /std:c++17 /EHsc %mainFiles% User32.lib Gdi32.lib
