****************************************************************************
3D Imaging - a repository of code snippets related to 3D imaging
****************************************************************************

https://github.com/mh-skjelvareid/3d-imaging

LICENSE DISCLAIMER:
Individual files under this repository may be subject to licenses - see the headers for each file.

OVERVIEW:
Gocator - this folder contains files related to the Gocator cameras manufactured by LMI Technologies. The "ReceiveSurfaceAsync.c" file is used to automatically log the complete 3D dataset to file, rather than just performing measurements on the data. As a researcher using the 3D camera together with other cameras, I found that this functionality was not available in the web interface, but that I could be written using the Gocator SDK. I based the code on example code from LMI, and used Microsoft Visual Studio to edit and compile the code. Note that a set of paths to the Gocator SDK must be set up before it is possible to compile the code. Try setting up your environment to compile the example code from LMI "as is" first, and if you succeed, try compiling my code. Good luck - I hope you find it useful!
