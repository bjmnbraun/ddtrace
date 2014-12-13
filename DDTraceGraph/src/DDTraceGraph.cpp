#include <string>

#include <DDTrace.h>
//#include <VectorClock.h>

#include "DDTraceGraph.h"
#include "VectorClockUtils.h"
#include "TraceReader.h"

//--------------------------------------------------------------
void DDTraceGraph::setup(){
	ofBackground(255,255,255);
	ofSetVerticalSync(true);
	
	font.loadFont("frabk.ttf", 10, true, false, true);
}

//--------------------------------------------------------------
/*
void DDTraceGraph::update(){
	angle++;
}
*/

/**
  * Constructs a representation of an abstract number. Used to convert numbers
  * to identifiers where the numbers should not convey any particular ordering
  * Succeeds as long as num is in [0, 25].
  */
char hashID(int num){
    return 'A' + ((num*37 + 13)%26);
}

const float offx = 100;
const float offy = 300;
//1 microsecond = 45 pixels
const float pixPerNanosecond = 0.045;
const float pixPerRow = 60;
const float rowThickness = 20;
const float axisBufX = 20;
const float axisBufY = 20 + rowThickness / 2;

void centeredText(ofTrueTypeFont* font, const std::string& str){
    float width = font->stringWidth(str);
    font->drawStringAsShapes(str, -width/2, 0);
}


class Axis {
  public:
    Axis(
    std::string name,
    ofTrueTypeFont* font,
    bool horizontal,
    ofVec2f origin,
    int max,
    float scale,
    int interval, 
    int tickInterval
    ):
    name(name),
    font(font),
    horizontal(horizontal),
    origin(origin),
    max(max),
    scale(scale),
    interval(interval),
    tickInterval(tickInterval)
    { 
    }

    const float smallTickSize = 5;
    const float bigTickSize = 10;

    void draw(){
        ofPushMatrix();
        //Start the intersection points axisBuf away
        ofTranslate(origin.x - axisBufX, origin.y + axisBufY);
        float axisBuf;
        if (!horizontal){
            axisBuf = axisBufY;
            ofRotate(270);
            ofScale(1,-1,1);
        } else {
            axisBuf = axisBufX;
        }
        ofSetColor(0);
        ofLine(0,0,axisBuf + max * scale,0);
        ofTranslate(axisBuf, 0);
        for(int i = 0; i < max; i+=tickInterval){
            ofPushMatrix();
            ofTranslate(i * scale,0); 
            ofLine(0,0,0,smallTickSize);
            ofPopMatrix();
        }
        for(int i = 0; i <= max; i+=interval){
            ofPushMatrix();
            ofTranslate(i * scale,0); 
            ofLine(0,0,0,bigTickSize);
            ofTranslate(0, bigTickSize + 15);
            if (!horizontal){
                ofScale(1,-1,1);
                ofRotate(90);
                ofTranslate(2,3);
            }
            centeredText(font, std::to_string(i));
            ofPopMatrix();
        }
        ofTranslate(max * scale / 2, 40);
        if (!horizontal){
            ofScale(1,-1,1);
            ofTranslate(2,3);
        }
        centeredText(font, name);
        ofPopMatrix();
    }

    std::string name;
    ofTrueTypeFont* font;
    bool horizontal;
    ofVec2f origin;
    int max;
    float scale;
    int interval; 
    int tickInterval;
};

/**
  * A "row" of the interval graph
  */
class IntervalGraphRow {
  public:
    IntervalGraphRow(int whichRow) :
    whichRow(whichRow),
    offsetNs(0),
    lastSeenIntervalStart(0)
    {
    }

    /*
    void adjustMinimumOffset(uint64_t ns){
        if (!hasMinimumOffset){
            offsetNs = ns;
            hasMinimumOffset = true;
        }
        lastSeenIntervalStart = ns;
    }
    */

    uint64_t offsetInRow(uint64_t ns){
        return ns - offsetNs;
    }

    uint64_t getElapsedNS(){
        return lastSeenIntervalStart - offsetNs;
    }

    float getX(uint64_t ns){
        return offx + offsetInRow(ns) * pixPerNanosecond;
    }
    float getY(){
        return offy - whichRow * pixPerRow - rowThickness / 2;
    }

    int whichRow;
    uint64_t offsetNs;
    int numDrawn;
    //bool hasMinimumOffset;

    uint64_t lastSeenIntervalStart;
};

class IntervalGraphState {
  public:
    ofTrueTypeFont* font;
    IntervalGraphState(ofTrueTypeFont* font):
    font(font)
    {
    }

    //.4 microseconds
    const uint16_t NETWORK_SEND_TIME_NS = 400;

    void drawInterval(const DDTrace::IntervalRecord& record,
        const DDTrace::IntervalRecord* predecessor){
        IntervalGraphRow* row = getRow(record.getServerID());  
        

        //First record should be the one on the farthest left.
        //row->adjustMinimumOffset(record.getStartNanoseconds());
        if (predecessor){
            if (predecessor->getServerID() == record.getServerID()){
                //Already set offset.
            } else {
                //Compute offset based on predecessor row's lastSeen
                uint16_t preId = predecessor->getServerID();
                IntervalGraphRow* preRow = getRow(preId);
                row->offsetNs = record.getStartNanoseconds() - 
                    (preRow->getElapsedNS() +
                    NETWORK_SEND_TIME_NS);

                //Draw the link
                ofSetColor(128);
                ofLine(preRow->getX(preRow->lastSeenIntervalStart),
                       preRow->getY() - 1,
                       row->getX(record.getStartNanoseconds()),
                       row->getY() + rowThickness + 1);
            }
        } else {
            row->numDrawn = 0;
            row->offsetNs = record.getStartNanoseconds();
        }
        row->lastSeenIntervalStart = record.getStartNanoseconds();
        float sx = row->getX(record.getStartNanoseconds());
        float ex = row->getX(record.getEndNanoseconds());
        float y = row->getY();


        ofEnableAlphaBlending();
        ofColor myColor = ofColor::fromHsb((row->whichRow * 40) % 256,
        128, 256, 128);
        ofFill();
        ofSetColor(myColor);
        ofRect(sx, y, ex - sx, rowThickness);
        ofNoFill();
        //myColor.a = 255;
        ofSetColor(0);
        ofRect(sx, y, ex - sx, rowThickness);
        //Restore
        ofDisableAlphaBlending();
        ofFill();

        ofPushMatrix();
        ofTranslate((sx + ex)/2, y);
        //Draw L3 misses
        bool above = (row->numDrawn++)%2;
        if (above){
            ofTranslate(0,-7);
        } else {
            ofTranslate(0,rowThickness + 15);
        }
        uint64_t l3Misses;
        bool hasL3Misses = record.getCountersDiff().getL3Misses(&l3Misses);
        if (hasL3Misses){
            centeredText(font, std::to_string(l3Misses));
        }
	//font.drawStringAsShapes(std::to_string(), 0, 0);
        ofPopMatrix();
    }

    IntervalGraphRow* getRow(uint64_t row){
        if (rows.find(row) == rows.end()){
            //First row gets 0
            int rowIndex = 0;
            //Remainder get 3, then 2, then 1
            if (!rows.empty()){
                rowIndex = 4 - rows.size();
            }
            rows[row].reset(new IntervalGraphRow(rowIndex));
        }
        return rows[row].get();
    }

    std::unordered_map<uint64_t, std::unique_ptr<IntervalGraphRow>> rows;
};

//--------------------------------------------------------------
void DDTraceGraph::draw(){
        ofBeginSaveScreenAsPDF(
            ("DDTraceGraph.pdf"), false);
	
        ofSetColor(0);
        ofPushMatrix();
        ofTranslate(400, 40);
        centeredText(&font, 
        std::string("RAMCloud Rpc Performance (L3 Misses & Real Time)"));
        ofPopMatrix();
        
        //TODO this is a pretty complicated iteration procedure, maybe take it
        //into a library?

        // Map of Rpc ID to events associated with it
        IntervalMap eventMap;

        // Process one file at a time.
        for (int i = 1; i < argc; i++) {
            readEvents(argv[i], &eventMap);
        }

        ofVec2f origin(offx,offy);
        Axis myHorAxis(
        "Real Time (microseconds)",
        &font,
        true,
        origin,
        15,
        pixPerNanosecond * 1001,
        5,
        1
        );
        myHorAxis.draw();
        Axis myVertAxis(
        "Server",
        &font,
        false,
        origin,
        3,
        pixPerRow,
        1,
        1
        );
        myVertAxis.draw();

        int whichRecord = 5;
        for (auto kv = eventMap.begin(); kv != eventMap.end(); kv++) {
            auto& v = kv->second;
            // Sort the result by vector clock
            //std::sort(v.begin(), v.end(), vectorClockLessThan);
            // Some adjacent vector clocks will be 

            // Compute a partial order graph corresponding to vector clock less than
            int V = v.size();
            #if 0
            if (V < 26){
                continue;
            }
            if (--whichRecord){
                continue;
            }
            #endif

            IntervalGraphState currentGraph(&font);

            PartialOrderGraph dag;
            dag.resize(V);
            for(int i = 0; i < V; i++){
                dag[i].resize(V);
                for(int j = 0; j < V; j++){
                    dag[i][j] = vectorClockLessThan(v[i], v[j]);
                }
            }

            // Copy the dag to an intermediate and compute transitive reduction
            //(I.e. remove edges i,j if there is a path from i to j through at least
            //one other node k
            Adjacencies adj;
            makeClosestAdjacencies(&dag, &adj);

            std::vector<std::vector<int>> incomingEdges;
            std::vector<int> numIncomingEdges;
            numIncomingEdges.resize(V);
            incomingEdges.resize(V);
            for(int i = 0; i < V; i++){
                getIncomingEdges(&adj, i, &incomingEdges[i]);
                numIncomingEdges[i] = incomingEdges[i].size();
            }

            std::stack<int> printOrdering;
            //Print each disjoint component in any order
            for(int i = 0; i < V; i++){
                if (numIncomingEdges[i] == 0){
                    printOrdering.push(i);
                }
            }
            printf("ID: %ld ", kv->first);
            while(!printOrdering.empty()){
                int thisNode = printOrdering.top();
                printOrdering.pop();

                DDTrace::IntervalRecord* predecessor = NULL;
                switch (incomingEdges[thisNode].size()){
                case 0:
                //K.
                    break; 
                case 1:
                    predecessor = &v[incomingEdges[thisNode][0]];
                    break;
                default:
                    assert(false);
                }
        
                currentGraph.drawInterval(v[thisNode], predecessor);

                printf("(%c: %p %d ->", hashID(thisNode), &v[thisNode],
                predecessor != NULL);
                std::vector<int> forkNexts;
                for(int i = 0; i < V; i++){
                    if (adj[thisNode][i]){
                        printf("%c, ", hashID(i));
                        assert(numIncomingEdges[i] > 0);
                        int otherIncomingEdges = numIncomingEdges[i] - 1;
                        //Decrement numIncomingEdges[i]
                        numIncomingEdges[i] = otherIncomingEdges;
                        if (otherIncomingEdges == 0){
                            //If the next node is on a different server, do it
                            //before we do the nodes on this server
                            //Because printOrdering is a stack, this
                            //means we defer the push of nodes on a
                            //different server until after we
                            //push the nodes from the same node.
                            if (v[i].getServerID() == v[thisNode].getServerID()){
                                printf("Nonfork\n");
                                //Print the node next
                                printOrdering.push(i);
                            } else {
                                printf("Fork\n");
                                forkNexts.push_back(i);
                            }
                        }
                    }
                }
                for(auto itr = forkNexts.begin(); 
                        itr != forkNexts.end();
                        ++itr){
                    printOrdering.push(*itr);
                }
                printf(") ");
            }
            printf("\n");
            //Check that all nodes got printed
            for(int i = 0; i < V; i++){
                assert(numIncomingEdges[i] == 0);
            }

            break; //Just do one for now..
        }
	
        ofEndSaveScreenAsPDF();

        ofExit();
}

//--------------------------------------------------------------
/*
void DDTraceGraph::keyPressed(int key){
	
	if( key=='r'){
		pdfRendering = !pdfRendering;	
		if( pdfRendering ){
			ofSetFrameRate(12);  // so it doesn't generate tons of pages
			ofBeginSaveScreenAsPDF("recording-"+ofGetTimestampString()+".pdf", true);
		}else{
			ofSetFrameRate(60);
			ofEndSaveScreenAsPDF();		
		}
	}
	
	if( !pdfRendering && key == 's' ){
		oneShot = true;
	}
}

//--------------------------------------------------------------
void DDTraceGraph::keyReleased(int key){

}

//--------------------------------------------------------------
void DDTraceGraph::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void DDTraceGraph::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){ 
	for(unsigned int j = 0; j < dropZoneRects.size(); j++){
		if( dropZoneRects[j].inside( dragInfo.position ) ){
			images[j].loadImage( dragInfo.files[0] );
			break;
		}
	}
}
*/

