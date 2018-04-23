This is a simple feed forward neural network programmed from scratch in c++

attached is the source code for the neural network as well as the compiled program (gc.exe) and npy images used in: 'https://youtu.be/gqTTb9w7IS8'

gc.exe should be launched from the command line and works by scanning the 'images' directory for *.npy files.
These files are packed with centered 28x28 pixel images from the google quickdraw dataset: 'https://console.cloud.google.com/storage/browser/quickdraw_dataset/full/numpy_bitmap'

gc.exe then trains two independent networks- a classifier and a generator.
While training the classifier window displays the current training image and the generator window displays the 'eigenvectors' of the generator in real-time.
The command window will display the current training progress and output verbose information about the networks' error every 1% 

After training the classifier window switches to input mode where an image can be drawn and evaluated with the classifier network
The classifier window has the following 'commands':
	- 'E': switch to the eraser tool
	- 'B': switch to the brush tool
	- 'C': clear the current drawing
	- 'ENTER': evaluate the drawing
After evaluating a drawing the classifier window will display the results in the top of the window in the form: {'1.1111', ..., 'n.nnnn'} where xxx.x is the percent classification of the xth loaded category.
Example: a classifier network trained on npy files 'cat.npy' and 'dog.npy' with an output of {1.0000, 0.0000} is to be interpreted as 100% cat.npy and 0% dog.npy

After training the generator window switches to input mode where sliders appear under each eigenvector. Adjusting these sliders reevaluates the generator network and displays the computed image where the eigenvector used to be. Sliders are numbered from the top down and correspond to the order in which npy files where loaded.
Example: a generator network trained on npy files 'cat.npy' and 'dog.npy' will display two sliders under each eigenvector. The top slider corresponds to cat.npy while the lower slider corresponds to dog.npy

After training the command window switches to input mode where it has the following commands
	- 'exit': closes the program
	- 'openClassifier': reopens the classifier window if it was closed
	- 'openGenerator': reopens the generator window if it was closed (Note: this command hasn't been updated to handle sliders and therefore will not allow for input)
Note: reopening windows requires them to be repainted to redraw their contents. This can be achieved by resizing them


Things to consider: 
	- This is not a polished application and was designed as a tool used to help we with researching neural network
	- The classifier and generator where added to the neural network as an after thought and therefore are not well implemented or trained
	- The training parameters are adjusted with the generator in mind and better classification can be achieved by changing them
	- Please feel free to modify and use this code as you see fit. I hope this inspires or helps others research neural networks

If you have any questions or use this in one of your projects please email me at: samrg123@gmail.com