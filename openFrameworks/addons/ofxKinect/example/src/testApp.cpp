#include "testApp.h"


//--------------------------------------------------------------
void testApp::setup() {
	ofSetLogLevel(OF_LOG_VERBOSE);
	
    // enable depth->rgb image calibration
	kinect.setRegistration(true);
    
	kinect.init();
	//kinect.init(true); // shows infrared instead of RGB video image
	//kinect.init(false, false); // disable video image (faster fps)
	kinect.open();
	
#ifdef USE_TWO_KINECTS
	kinect2.init();
	kinect2.open();
#endif
	
	colorImg.allocate(kinect.width, kinect.height);
	grayImage.allocate(kinect.width, kinect.height);
	grayThreshNear.allocate(kinect.width, kinect.height);
	grayThreshFar.allocate(kinect.width, kinect.height);
	
	nearThreshold = 230;
	farThreshold = 70;
	bThreshWithOpenCV = true;
	
	ofSetFrameRate(60);
	
	// zero the tilt on startup
	angle = 0;
	kinect.setCameraTiltAngle(angle);
	
	// start from the front
	bDrawPointCloud = false;
}

//--------------------------------------------------------------
void testApp::update() {
	
	ofBackground(100, 100, 100);
	
	kinect.update();
	
	// there is a new frame and we are connected
	if(kinect.isFrameNew()) {
	  // load grayscale depth image from the kinect source
	  grayImage.setFromPixels(kinect.getDepthPixels(), kinect.width, kinect.height);
	  unsigned char* depthData;
	  vector< pair<int,int> > blobs[NUM_BLOBS];
	  queue< pair<int, int> > que;
	  depthData = kinect.getDepthPixels();

	  bool used[480][640];

	  for (int count = 0; count < NUM_BLOBS; count ++) {
	    int closest=0;
	    pair<int, int> closestPair;
	    for(int i=0; i<480; i++) {
	      for(int j=0; j<640; j++) {
		if(depthData[i*640+j]>closest) {
		  closest=depthData[i*640+j];
		  closestPair=pair<int, int>(i, j);
		  depthData[i*640 + j] = 0;
		}
		used[i][j]=false;
	      }
	    }
	    
	    que.push(closestPair);
	    used[closestPair.first][closestPair.second]=true;
	    while(!que.empty()){
	      int i=que.front().first;
	      int j=que.front().second;
	      que.pop();
	      for(int ii=max(i-1, 0); ii<=min(i+1, 480-1); ii++) {
		for(int jj=max(j-1, 0); jj<=min(j+1, 640-1); jj++) {
		  if(!used[ii][jj] && abs(depthData[ii*640+jj]-closest)<10){//depthData[ii*640+jj]>depthData[i*640+j]){
		    used[ii][jj]=true;
		    blobs[count].push_back(pair<int, int>(ii, jj));
		    que.push(pair<int, int>(ii, jj));
		    depthData[ii*640+jj] = 0;
		  }
		}
	      }
	    }
	  }


	  // we do two thresholds - one for the far plane and one for the near plane
	  // we then do a cvAnd to get the pixels which are a union of the two thresholds
	  /*
	    if(bThreshWithOpenCV) {
	    grayThreshNear = grayImage;
	    grayThreshFar = grayImage;
	    cvAnd(grayThreshNear.getCvImage(), grayThreshFar.getCvImage(), grayImage.getCvImage(), NULL);
	    } else {
	    
	    // or we do it ourselves - show people how they can work with the pixels
	    unsigned char * pix = grayImage.getPixels();
	    int numPixels = grayImage.getWidth() * grayImage.getHeight();
	    for(int i = 0; i < numPixels; i++) {
	    if(pix[i] < nearThreshold && pix[i] > farThreshold) {
	    pix[i] = 255;
	    } else {
	    pix[i] = 0;
	    }
	    }
	    }*/

	  unsigned char * pix = grayImage.getPixels();
	  for(int i = 0; i < grayImage.width*grayImage.height; i++) {
	    pix[i] = 0;
	  }
	  for(int i = 0, count = 0; i < NUM_BLOBS && count < 2; i++) {
	    if (blobs[i].size() > 100) {
	      count ++;
	      for (int j = 0; j < (signed int)blobs[i].size(); j++) {
		pix[blobs[i][j].first*640 + blobs[i][j].second] = 255;
	      }
	    }
	  }

	  // update the cv images
	  grayImage.flagImageChanged();

	  // find contours which are between the size of 20 pixels and 1/3 the w*h pixels.
	  // also, find holes is set to true so we will get interior contours as well....
	  int numBlobs = contourFinder.findContours(grayImage, 2, (kinect.width*kinect.height)/2, 10, false);

	  lhx = -1;
	  lhy = -1;
	  rhx = -1;
	  rhy = -1;

	  if (numBlobs >= 1) {
	    lhx = contourFinder.blobs[0].centroid.x;
	    lhy = contourFinder.blobs[0].centroid.y;
	  }
	  if (numBlobs >= 2) {
	    rhx = contourFinder.blobs[1].centroid.x;
	    rhy = contourFinder.blobs[1].centroid.y;
	  }
	}
	
#ifdef USE_TWO_KINECTS
	kinect2.update();
#endif
}

//--------------------------------------------------------------
void testApp::draw() {
	
	ofSetColor(255, 255, 255);
	
	if(bDrawPointCloud) {
		easyCam.begin();
		drawPointCloud();
		easyCam.end();
	} else {
		// draw from the live kinect
		kinect.drawDepth(10, 10, 400, 300);
		kinect.draw(420, 10, 400, 300);
		
		grayImage.draw(10, 320, 400, 300);
		contourFinder.draw(10, 320, 400, 300);
		
#ifdef USE_TWO_KINECTS
		kinect2.draw(420, 320, 400, 300);
#endif
	}
	
	// draw instructions
	ofSetColor(255, 255, 255);
	stringstream reportStream;
	reportStream << "accel is: " << ofToString(kinect.getMksAccel().x, 2) << " / "
	<< ofToString(kinect.getMksAccel().y, 2) << " / "
	<< ofToString(kinect.getMksAccel().z, 2) << endl
	<< "press p to switch between images and point cloud, rotate the point cloud with the mouse" << endl
	<< "using opencv threshold = " << bThreshWithOpenCV <<" (press spacebar)" << endl
	<< "set near threshold " << nearThreshold << " (press: + -)" << endl
	<< "set far threshold " << farThreshold << " (press: < >) num blobs found " << contourFinder.nBlobs
	<< ", fps: " << ofGetFrameRate() << endl
	<< "press c to close the connection and o to open it again, connection is: " << kinect.isConnected() << endl
	<< "press UP and DOWN to change the tilt angle: " << angle << " degrees" << endl;
	ofDrawBitmapString(reportStream.str(),20,652);
}

void testApp::drawPointCloud() {
	int w = 640;
	int h = 480;
	ofMesh mesh;
	mesh.setMode(OF_PRIMITIVE_POINTS);
	int step = 2;
	for(int y = 0; y < h; y += step) {
		for(int x = 0; x < w; x += step) {
			if(kinect.getDistanceAt(x, y) > 0) {
				mesh.addColor(kinect.getColorAt(x,y));
				mesh.addVertex(kinect.getWorldCoordinateAt(x, y));
			}
		}
	}
	glPointSize(3);
	ofPushMatrix();
	// the projected points are 'upside down' and 'backwards' 
	ofScale(1, -1, -1);
	ofTranslate(0, 0, -1000); // center the points a bit
	glEnable(GL_DEPTH_TEST);
	mesh.drawVertices();
	glDisable(GL_DEPTH_TEST);
	ofPopMatrix();
}

//--------------------------------------------------------------
void testApp::exit() {
	kinect.setCameraTiltAngle(0); // zero the tilt on exit
	kinect.close();
	
#ifdef USE_TWO_KINECTS
	kinect2.close();
#endif
}

//--------------------------------------------------------------
void testApp::keyPressed (int key) {
	switch (key) {
		case ' ':
			bThreshWithOpenCV = !bThreshWithOpenCV;
			break;
			
		case'p':
			bDrawPointCloud = !bDrawPointCloud;
			break;
			
		case '>':
		case '.':
			farThreshold ++;
			if (farThreshold > 255) farThreshold = 255;
			break;
			
		case '<':
		case ',':
			farThreshold --;
			if (farThreshold < 0) farThreshold = 0;
			break;
			
		case '+':
		case '=':
			nearThreshold ++;
			if (nearThreshold > 255) nearThreshold = 255;
			break;
			
		case '-':
			nearThreshold --;
			if (nearThreshold < 0) nearThreshold = 0;
			break;
			
		case 'w':
			kinect.enableDepthNearValueWhite(!kinect.isDepthNearValueWhite());
			break;
			
		case 'o':
			kinect.setCameraTiltAngle(angle); // go back to prev tilt
			kinect.open();
			break;
			
		case 'c':
			kinect.setCameraTiltAngle(0); // zero the tilt
			kinect.close();
			break;
			
		case OF_KEY_UP:
			angle++;
			if(angle>30) angle=30;
			kinect.setCameraTiltAngle(angle);
			break;
			
		case OF_KEY_DOWN:
			angle--;
			if(angle<-30) angle=-30;
			kinect.setCameraTiltAngle(angle);
			break;
	}
}

//--------------------------------------------------------------
void testApp::mouseDragged(int x, int y, int button)
{}

//--------------------------------------------------------------
void testApp::mousePressed(int x, int y, int button)
{}

//--------------------------------------------------------------
void testApp::mouseReleased(int x, int y, int button)
{}

//--------------------------------------------------------------
void testApp::windowResized(int w, int h)
{}
