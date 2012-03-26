#include "cinder/app/AppBasic.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/Texture.h"
#include "cinder/Capture.h"
#include "cinder/params/Params.h"

#include "smile.h"

using namespace ci;
using namespace ci::app;
using namespace std;

//static const int CAMWIDTH = 640, CAMHEIGHT = 480;
static const int CAMWIDTH = 1280, CAMHEIGHT = 720;

class SmilesApp : public AppBasic {
  public:
    void prepareSettings( Settings *settings );
	void setup();
	void update();
	void draw();
    void drawAllCams();
    
    void        mouseDown( MouseEvent event );
    void        mouseDrag( MouseEvent event );
    void        mouseUp( MouseEvent event );
    
    bool setupSmileDetector(int camWidth , int camHeight);
    void detectSmiles(const RImage<float> &pixels);
    
    MPSmile*            mMPSmile;
    
    vector<Capture>		mCaptures;
	vector<gl::Texture>	mTextures;
    
    Surface8u           mSurface;
    Channel             mGreyChannel;
    
    int                 mCamIndex;
    
    Rectf               mSmileRect;
    int                 mSmileRectDraggingCorner;
    
    unsigned char *     mDetectionPixels; // detection pixels
    RImage<float>*      mRImage_pixels;
    VisualObject        mFaces; // Smile Value
    
    float               mSmileResponse;
    float               mSmileLimit;
    int                 mSmileAverageNumOfFrames;
    //float				mSmileAverage[5];
    vector<float>       mSmileAverage;
    unsigned			mSmileAverageIndex;
    float               mSmileThreshold;
    FaceObject*         mFace;
    
    params::InterfaceGl	mParams;
    float           mFps;
};

void SmilesApp::prepareSettings( Settings *settings ){
    settings->setWindowSize( 720, 540 );
    settings->setFrameRate( 60.0f );
    //settings->enableSecondaryDisplayBlanking( false );
}

void SmilesApp::setup()
{	
    mSmileLimit = 5.0f;
    mSmileAverageNumOfFrames = 10;
    mCamIndex = 0;
    mFps = getAverageFps();
    
	// list out the devices
	vector<Capture::DeviceRef> devices( Capture::getDevices() );
	for( vector<Capture::DeviceRef>::const_iterator deviceIt = devices.begin(); deviceIt != devices.end(); ++deviceIt ) {
		Capture::DeviceRef device = *deviceIt;
		console() << "Found Device " << device->getName() << " ID: " << device->getUniqueId() << std::endl;
		try {
			if( device->checkAvailable() ) {
				mCaptures.push_back( Capture( CAMWIDTH, CAMHEIGHT, device ) );
				//mCaptures.back().start();
				// placeholder text
				mTextures.push_back( gl::Texture() );
			}
			else
				console() << "device is NOT available" << std::endl;
		}
		catch( CaptureExc & ) {
			console() << "Unable to initialize device: " << device->getName() << endl;
		}
	}
    if(mCaptures.size() > mCamIndex){
        mCaptures[mCamIndex].start();
    }
    
    mSmileRect = Rectf(300,100,600,400);
    setupSmileDetector(mSmileRect.getInteriorArea().getWidth(), mSmileRect.getInteriorArea().getHeight());
    console()<< mSmileRect.getInteriorArea().getWidth() << mSmileRect.getInteriorArea().getHeight() << endl;
	mSmileThreshold = 0;
	mSmileAverageIndex = 0;
    
    mParams = params::InterfaceGl( "Parameters", Vec2i( 220, 170 ) );
    mParams.addParam( "FPS", &mFps,"", true );
    mParams.addSeparator();
	mParams.addParam( "SmileResponse", &mSmileResponse, "", true );
    mParams.addParam( "SmileThreshold", &mSmileThreshold, "", true );
    
    mParams.addParam( "mSmileLimit", &mSmileLimit );
    mParams.addParam( "mSmileAverageNumOfFrames", &mSmileAverageNumOfFrames );
    
}

void SmilesApp::mouseDown( MouseEvent event ){
    
    if(mSmileRect.getUpperLeft().distance(event.getPos())<10){mSmileRectDraggingCorner=0;}
    if(mSmileRect.getUpperRight().distance(event.getPos())<10){mSmileRectDraggingCorner=1;}
    if(mSmileRect.getLowerLeft().distance(event.getPos())<10){mSmileRectDraggingCorner=2;}
    if(mSmileRect.getLowerRight().distance(event.getPos())<10){mSmileRectDraggingCorner=3;}
    
}
void SmilesApp::mouseUp( MouseEvent event ){
    mSmileRectDraggingCorner = -1;
    setupSmileDetector(mSmileRect.getInteriorArea().getWidth(), mSmileRect.getInteriorArea().getHeight());
}

void SmilesApp::mouseDrag( MouseEvent event ){
    if(mSmileRectDraggingCorner >= 0){
        Vec2f p = event.getPos();
        switch(mSmileRectDraggingCorner){
            case 0:
                mSmileRect.x1 = p.x;
                mSmileRect.y1 = p.y;
                break;
            case 1:
                mSmileRect.x2 = p.x;
                mSmileRect.y1 = p.y;
                break;
            case 2:
                mSmileRect.x1 = p.x;
                mSmileRect.y2 = p.y;
                break;
            case 3:
                mSmileRect.x2 = p.x;
                mSmileRect.y2 = p.y;
                break;
        }
    }
}

bool SmilesApp::setupSmileDetector(int camWidth , int camHeight)
{
    mSmileAverage.clear();
	for(int i=0; i<60; i++){
		mSmileAverage.push_back(0);
	}
	// Initialize MPSmile class
    mMPSmile = NULL;
    if( ( mMPSmile = new MPSmile() ) == NULL ){
        //console() << " new MPSmile error" << endl;
        return false;
    } else {
        mRImage_pixels = new RImage<float>(camWidth, camHeight);
        return true;
    }
}

void SmilesApp::detectSmiles(const RImage<float> &pixels)
{
    mFaces.clear();
	// Find Smiles
	if( mMPSmile->findSmiles( pixels, mFaces, 1.00, wt_avg ) ){
        //console() << " findSmiles error " << endl;
        return;
	}
    mSmileResponse = 0;
    // The result is in faces(VisualObject).
    if(!mFaces.empty()) {
        for(list<VisualObject *>::iterator it = mFaces.begin(); it != mFaces.end(); ++it) {
            mFace = static_cast<FaceObject*>(*it);
            mSmileResponse = mFace->activation;
            //if(mSmileResponse > mSmileLimit) mSmileResponse = mSmileLimit;
            //if(mSmileResponse < 0) mSmileResponse = 0;
            //mSmileResponse = (mSmileResponse / mSmileLimit);
            mSmileResponse = max( 0.0f, min( mSmileResponse, mSmileLimit ) ) / mSmileLimit;
        }
        //console() << mFaces.size() << " faces detected!" << endl;
    }else{
        //console() << " no faces detected " << endl;
    }
    
	mSmileAverage[mSmileAverageIndex] = mSmileResponse;
	mSmileAverageIndex++;
	if(mSmileAverageIndex>=mSmileAverageNumOfFrames)mSmileAverageIndex=0;
	
	float smileSum = 0;
	for(int i=0;i<mSmileAverageNumOfFrames;i++){
		smileSum += mSmileAverage[i];
	}
	mSmileThreshold = smileSum/mSmileAverageNumOfFrames;
}

void SmilesApp::update()
{
    mFps = getAverageFps();
    /*
    for( vector<Capture>::iterator cIt = mCaptures.begin(); cIt != mCaptures.end(); ++cIt ) {
		if( cIt->checkNewFrame() ) {
			mSurface = cIt->getSurface();
			mTextures[cIt - mCaptures.begin()] = gl::Texture( cIt->getSurface() );
		}
	}*/
    if( getElapsedFrames()%3 == 0  ){
        if(mCaptures.size() > mCamIndex && mCaptures[mCamIndex].checkNewFrame() ){
                mSurface = mCaptures[mCamIndex].getSurface();
        }
        //*
        if (mSurface){
            mGreyChannel = Channel( mSurface.clone(mSmileRect.getInteriorArea()) );
            int totalDetectionPixels = mGreyChannel.getWidth()*mGreyChannel.getHeight();
            unsigned char * detectionPixels = mGreyChannel.getData();
            for (int i = 0; i < totalDetectionPixels; i++){
                mRImage_pixels->array[i] = detectionPixels[i];
            }
            detectSmiles(*mRImage_pixels);
            //console() << smileThreshold  << endl;
        }
        //*/
    }
}

void SmilesApp::draw()
{
	gl::enableAlphaBlending();
	gl::clear( Color::black() );
    gl::color(1.0f, 1.0f, 1.0f);
    
    // draw webcam capture
	if( mCaptures.empty() || !mSurface )
		return;
    //gl::draw( gl::Texture(mSurface) );
    //gl::drawStrokedRect(Rectf( 0,0,mSurface.getWidth(),mSurface.getHeight()));
    
    if(mGreyChannel){
    gl::pushMatrices();{
        gl::translate(mSmileRect.getUpperLeft());
        gl::draw( gl::Texture(mGreyChannel) );
    }gl::popMatrices();
    }
    
    // draw rect that actually gets analysed:
    gl::color(ColorA(0.0f, 1.0f, 0.0f, 1.0f));
    gl::drawStrokedRect(mSmileRect);
    gl::drawStrokedCircle(mSmileRect.getUpperLeft(), 10);
    gl::drawStrokedCircle(mSmileRect.getUpperRight(), 10);
    gl::drawStrokedCircle(mSmileRect.getLowerLeft(), 10);
    gl::drawStrokedCircle(mSmileRect.getLowerRight(), 10);
    
    
    //draw mSmileThreshold
    gl::pushMatrices();{
        gl::color(ColorA(1.0f, 0.0f, 0.0f, 0.3f));
        gl::drawSolidRect(Rectf( 0, (1-mSmileThreshold)*getWindowHeight(), getWindowWidth(), getWindowHeight() ) );
        gl::drawStrokedRect( Rectf(0, 0, getWindowWidth(), getWindowHeight() ) );
    }gl::popMatrices();
    
    //draw faces
    if(!mFaces.empty()){
        gl::pushMatrices();{
            gl::translate(mSmileRect.getUpperLeft());
            gl::color(1.0f, 1.0f, 1.0f);
            for(list<VisualObject *>::iterator it = mFaces.begin(); it != mFaces.end(); ++it) {
                mFace = static_cast<FaceObject*>(*it);
                gl::drawStrokedRect(Rectf(mFace->x, mFace->y, mFace->x+mFace->xSize, mFace->y+mFace->ySize ));
            }
        }gl::popMatrices();
    }
    
    mParams.draw();
}

void SmilesApp::drawAllCams()
{
    /*
    float width = getWindowWidth() / mCaptures.size();	
	float height = width / ( CAMWIDTH / (float)CAMHEIGHT );
	float x = 0, y = ( getWindowHeight() - height ) / 2.0f;
	for( vector<Capture>::iterator cIt = mCaptures.begin(); cIt != mCaptures.end(); ++cIt ) {	
		// draw the latest frame
		gl::color( Color::white() );
		if( mTextures[cIt-mCaptures.begin()] )
			gl::draw( mTextures[cIt-mCaptures.begin()], Rectf( x, y, x + width, y + height ) );
		x += width;
	}
    */
}

CINDER_APP_BASIC( SmilesApp, RendererGl )
