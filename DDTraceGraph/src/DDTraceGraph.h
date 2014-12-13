#pragma once
#include <assert.h>

#include "ofMain.h"
#include "ofConstants.h"

class DDTraceGraph : public ofBaseApp{
	public:
                //Currently takes command line arguments
                DDTraceGraph(int argc, char** argv):
                argc(argc), argv(argv)
                {
                }

		void setup();
		//void update();
		void draw();

                /*
		void keyPressed  (int key);
		void keyReleased(int key);
		void mouseMoved(int x, int y );
		void mouseDragged(int x, int y, int button);
		void mousePressed(int x, int y, int button);
		void mouseReleased(int x, int y, int button);
		void windowResized(int w, int h);
		void dragEvent(ofDragInfo dragInfo);
		void gotMessage(ofMessage msg);
                */
		
		ofTrueTypeFont font;
                int argc;
                char** argv;
};
