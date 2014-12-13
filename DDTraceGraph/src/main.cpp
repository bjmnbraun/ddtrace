#include "ofMain.h"
#include "ofAppNoWindow.h"

#include "DDTraceGraph.h"

//========================================================================
int main(int argc, char** argv){
        ofAppNoWindow w;

	ofSetupOpenGL(&w, 800, 400, OF_WINDOW);			// <-------- setup the GL context

	// this kicks off the running of my app
	// can be OF_WINDOW or OF_FULLSCREEN
	// pass in width and height too:
	ofRunApp(new DDTraceGraph(argc, argv));
}
