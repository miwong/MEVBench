/*
 * Copyright (c) 2006-2009 The Regents of The University of Michigan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * FaceDetection.cpp
 *
 *  Created on: June 11, 2011
 *      Author: jlclemon
 */
 
#include "FaceDetection.hpp"
#ifdef USE_MARSS
#warning "Using MARSS"
#include "ptlcalls.h"
#endif
#ifdef USE_GEM5
#warning "Using GEM5"
extern "C"
{
	#include "m5op.h"
}
#endif


//#define TSC_TIMING
#ifdef TSC_TIMING
#include "tsc_class.hpp"

#endif

//#define CLOCK_GETTIME_TIMING
#ifdef CLOCK_GETTIME_TIMING
#include "time.h"
#ifndef GET_TIME_WRAPPER
#define GET_TIME_WRAPPER(dataStruct) clock_gettime(CLOCK_THREAD_CPUTIME_ID, &dataStruct)
#endif
#endif

#include <chrono>

#ifdef TSC_TIMING
vector<TSC_VAL_w> fd_timingVector;
#endif
 
#ifdef CLOCK_GETTIME_TIMING
vector<struct timespec> fd_timeStructVector;
#endif


#ifdef TSC_TIMING
		void fd_writeTimingToFile(vector<TSC_VAL_w> timingVector)
		{

			ofstream outputFile;
			outputFile.open("timing.csv");
			outputFile << "Thread,Start,Finish" << endl;
			for(int i = 0; i<(timingVector.size()/2); i++)
			{
				outputFile << i << "," << timingVector[i] << "," << timingVector[i+timingVector.size()/2] << endl;

			}
			outputFile.close();
		}
#endif

#ifdef CLOCK_GETTIME_TIMING
		void fd_writeTimingToFile(vector<struct timespec> timingVector)
		{

			ofstream outputFile;
			outputFile.open("timing.csv");
			outputFile << "Thread,Start sec, Start nsec,Finish sec, Finish nsec" << endl;
			for(int i = 0; i<(timingVector.size()/2); i++)
			{
				outputFile << i << "," << timingVector[i].tv_sec<<","<< timingVector[i].tv_nsec << "," << timingVector[i+timingVector.size()/2].tv_sec<<","<< timingVector[i+timingVector.size()/2].tv_nsec <<endl;

			}
			outputFile.close();
		}
#endif

#define PIPELINE
#define FRAMERATE

using namespace std::chrono;
extern double t_detectSingleScale, t_groupRectangles, t_multiLoopCalc, t_detectMultiScaleInternal;
extern double t_parallelFor;
double t_detectMultiScale = 0.0, t_getNextFrames = 0.0, t_setupInput = 0.0, t_videoWriter = 0.0, t_total = 0.0;

#ifdef PROFILING
#define PROFILE_FUNC(counter, func) \
	system_clock::time_point counter ## _1 = system_clock::now(); \
	func; \
	system_clock::time_point counter ## _2 = system_clock::now(); \
	counter += duration_cast<duration<double>>(counter ## _2 - counter ## _1).count();
#else
#define PROFILE_FUNC(counter, func) func;
#endif

VideoCapture *videoCapture = NULL;
VideoWriter *videoWriter = NULL;

void readFileListFromFile(string fileListFilename, string filesBaseDir,vector<string> &fileList)
{
	ifstream fileListFile;

	fileListFile.open(fileListFilename.c_str(),ios::in);


	if(fileListFile.is_open())
	{
		while(fileListFile.good())
		{
			string currentLine;

			getline(fileListFile,currentLine);
			if(currentLine != "")
			{
				fileList.push_back(filesBaseDir+currentLine);
			}
		}

		fileListFile.close();
	}


}


bool loadFaceDetectionConfigFile(vector<string> &commandArgs, string filename)
{

	std::ifstream configFile;
//	int i = 3;
	string tmpString;
	bool returnVal = false;


	configFile.open(filename.c_str());


	if(configFile.good())
	{

		while(!configFile.eof())
		{
			getline(configFile,tmpString);
			commandArgs.push_back(tmpString);

		}
		returnVal = true;
		cout << "Config file loaded!!!" << endl;
	}
	else
	{

		cout << "WARNING: Unable to open params config file" << endl;

	}
	return returnVal;

}
 
 
void parseFaceDetectionConfigCommandVector(FaceDetectionConfig & faceDetectionConfig, vector<string> & commandStringVector)
{



	vector<string>::iterator it_start = commandStringVector.begin();
	vector<string>::iterator it_end = commandStringVector.end();
	vector<string>::iterator it_current;

	stringstream stringBuffer;



	faceDetectionConfig.cameraId[0] = 1;
	faceDetectionConfig.cameraId[1] = 0;

	faceDetectionConfig.inputMethod =FACE_DETECT_SINGLE_STREAM;

	faceDetectionConfig.faceScale = 2.0;

	faceDetectionConfig.outputRawImageExt = ".png";
	faceDetectionConfig.outputRawImageBaseName = "";
	faceDetectionConfig.bufferedImageList = false;

	faceDetectionConfig.showWindows = false;

	faceDetectionConfig.noWaitKey = false;

	//-desc sift -match knn_flann -classify linear_svm -queryDescFile test.yml -trainDescFile test.yml -loadClassificationStruct test.yml -loadMatchStruct test.yml
	for(it_current=it_start; it_current!=it_end; ++it_current)
	{

		if(!it_current->compare("-inputMethod") || !it_current->compare("-InputMethod"))
		{
			if(!(it_current+1)->compare("monoStream"))
			{
				faceDetectionConfig.inputMethod=FACE_DETECT_SINGLE_STREAM;
			}
			if(!(it_current+1)->compare("stereoStream"))
			{
				faceDetectionConfig.inputMethod=FACE_DETECT_STEREO_STREAM;
			}
			if(!(it_current+1)->compare("imageFileList"))
			{
				faceDetectionConfig.useImageList = true;
				faceDetectionConfig.inputMethod=FACE_DETECT_IMAGE_FILE_LIST;
			}


			if(!(it_current+1)->compare("videoFile"))
			{
				faceDetectionConfig.inputMethod=FACE_DETECT_VIDEO_FILE;


			}

		}

		if(!it_current->compare("-camera0") || !it_current->compare("-Camera0"))
		{
			stringBuffer.str("");
			stringBuffer << *(it_current+1);
			if((stringBuffer >> faceDetectionConfig.cameraId[0]).fail())
			{
				cout << "Camera0 Id could not be parsed." << endl;
			}

		}

		if(!it_current->compare("-camera1") || !it_current->compare("-Camera1"))
		{
			stringBuffer.str("");
			stringBuffer << *(it_current+1);
			if((stringBuffer >> faceDetectionConfig.cameraId[1]).fail())
			{
				cout << "Camera0 Id could not be parsed." << endl;
			}

		}

		if(!it_current->compare("-faceCascadeFile") || !it_current->compare("-FaceCascadeFile"))
		{

			faceDetectionConfig.cascadeFaceDetectorFilename = *(it_current+1);

		}
		if(!it_current->compare("-imageList") || !it_current->compare("-ImageList"))
		{


			faceDetectionConfig.imageListFilename = *(it_current+1);


		}

		if(!it_current->compare("-imageListBaseDir") || !it_current->compare("-ImageListBaseDir"))
		{


			faceDetectionConfig.imageListBaseDir = *(it_current+1);


		}



		if(!it_current->compare("-nVertCores"))
		{
			stringBuffer.str("");
			stringBuffer.clear();
			stringBuffer << *(it_current+1);
			cout << *(it_current+1) << endl;
			if((stringBuffer >> faceDetectionConfig.numberOfVerticalProcs).fail())
			{
				cout << "Number of vertical cores could not be parsed." << endl;
			}

		}

		if(!it_current->compare("-nHoriCores"))
		{
			stringBuffer.str("");
			stringBuffer.clear();
			stringBuffer << *(it_current+1);
			cout << *(it_current+1) << endl;
			if((stringBuffer >> faceDetectionConfig.numberOfHorizontalProcs).fail())
			{
				cout << "Number of horizontal cores could not be parsed." << endl;
			}

		}

		if(!it_current->compare("-faceScale") ||!it_current->compare("-FaceScale"))
		{
			stringBuffer.str("");
			stringBuffer.clear();
			stringBuffer << *(it_current+1);
			cout << *(it_current+1) << endl;
			if((stringBuffer >> faceDetectionConfig.faceScale).fail())
			{
				cout << "Face scale could not be parsed." << endl;
			}

		}

		if( it_current->compare("-singleThreaded") == 0 || it_current->compare("-SingleThreaded") == 0)
		{
			faceDetectionConfig.singleThreaded= true;
		}


		if( it_current->compare("-showWindows") == 0 || it_current->compare("-ShowWindows") == 0)
		{

			faceDetectionConfig.showWindows = true;
		}

		if( it_current->compare("-noWaitKey") == 0 || it_current->compare("-NoWaitKey") == 0)
		{

			faceDetectionConfig.noWaitKey = true;
		}



		if( it_current->compare("-bufferedImageList") == 0 || it_current->compare("-BufferedImageList") == 0)
		{

			faceDetectionConfig.bufferedImageList = true;
		}
		if(!it_current->compare("-inputVideoFile") || !it_current->compare("-InputVideoFile"))
		{


			faceDetectionConfig.inputVideoFilename = *(it_current+1);


		}
		if(!it_current->compare("-outputRawVideoFile") || !it_current->compare("-OutputRawVideoFile"))
		{


			faceDetectionConfig.outputRawStreamFilename = *(it_current+1);


		}

		if(!it_current->compare("-outputAugmentedVideoFile") || !it_current->compare("-OutputAugmentedVideoFile"))
		{


			faceDetectionConfig.outputAugmentedStreamFilename = *(it_current+1);


		}
		if(!it_current->compare("-outputRawImageBaseName") || !it_current->compare("-OutputRawImageBaseName"))
		{


			faceDetectionConfig.outputRawImageBaseName = *(it_current+1);


		}
		if(!it_current->compare("-outputRawImageExt") || !it_current->compare("-OutputRawImageExt"))
		{


			faceDetectionConfig.outputRawImageExt = *(it_current+1);


		}



	}


	if(faceDetectionConfig.numberOfVerticalProcs ==1 && faceDetectionConfig.numberOfHorizontalProcs ==1)
	{
		faceDetectionConfig.singleThreaded = true;


	}
	else
	{
		faceDetectionConfig.singleThreaded = false;

	}


	faceDetectionConfig.bufferedImageListBufferSize = 5;


	return;

}

void faceDetectionSetupOutput( FaceDetectionConfig &faceDetectionConfig, FaceDetectionData &faceDetectionData)
{
	if(faceDetectionConfig.outputRawStreamFilename!= "")
	{

		faceDetectionData.rawStreamWriter.open(faceDetectionConfig.outputRawStreamFilename,CV_FOURCC('M','J','P','G'),30.0,faceDetectionData.frameSize,true);


	}
	if(faceDetectionConfig.outputAugmentedStreamFilename!= "")
	{

		faceDetectionData.augmentedStreamWriter.open(faceDetectionConfig.outputAugmentedStreamFilename,CV_FOURCC('P','I','M','1'),30.0,faceDetectionData.frameSize, true);


	}
	if(faceDetectionConfig.outputRawImageBaseName != "")
	{
		faceDetectionData.currentOutputRawImageId = 0;
	}


}

void faceDetectionHandleWritingOutput( FaceDetectionConfig &faceDetectionConfig, FaceDetectionData &faceDetectionData)
{
	if(faceDetectionData.rawStreamWriter.isOpened())
	{


		faceDetectionData.rawStreamWriter << faceDetectionData.currentFrame[0];

	}
	if(faceDetectionData.augmentedStreamWriter.isOpened())
	{

		faceDetectionData.augmentedStreamWriter << faceDetectionData.currentAgumentedFrame[0];


	}
	if(faceDetectionConfig.outputRawImageBaseName != "")
	{
		stringstream streamForId;
		string currentOutputId;
		streamForId << "_" <<faceDetectionData.currentOutputRawImageId;

		streamForId >> currentOutputId;

		string imageFileName = faceDetectionConfig.outputRawImageBaseName + currentOutputId + faceDetectionConfig.outputRawImageExt;
		imwrite(imageFileName,faceDetectionData.currentFrame[0]);
		faceDetectionData.currentOutputRawImageId++;
	}


}


void faceDetectionSetupInput( FaceDetectionConfig &faceDetectionConfig, FaceDetectionData &faceDetectionData)
{
	switch(faceDetectionConfig.inputMethod)
	{
		case FACE_DETECT_VIDEO_FILE:
		{
			videoCapture = new VideoCapture;
			videoCapture->open(faceDetectionConfig.imageListFilename);
			faceDetectionData.frameSize.width = videoCapture->get(CV_CAP_PROP_FRAME_WIDTH);
			faceDetectionData.frameSize.height = videoCapture->get(CV_CAP_PROP_FRAME_HEIGHT);
			break;
		}
		case FACE_DETECT_STEREO_STREAM:
		{

			break;
		}
		case FACE_DETECT_IMAGE_FILE_LIST:
		{

			if(faceDetectionConfig.imageListFilename != "")
			{


				readFileListFromFile(faceDetectionConfig.imageListFilename , faceDetectionConfig.imageListBaseDir,faceDetectionData.imageList);
				faceDetectionData.currentInputImageId = 0;
				if(faceDetectionConfig.bufferedImageList)
				{
					for(int i = faceDetectionData.currentInputImageId; ((i < faceDetectionConfig.bufferedImageListBufferSize)&& (i<faceDetectionData.imageList.size())); i++)
					{
						faceDetectionData.imageListBuffer[0].push_back(imread(faceDetectionData.imageList[i]));
					}
				}

				Mat tmpMat = imread(faceDetectionData.imageList[0]);
				faceDetectionData.frameSize.width = tmpMat.cols;
				faceDetectionData.frameSize.height = tmpMat.rows;


			}
			else
			{
				cout << "Invalid image list." << endl;
				exit(0);
			}



			break;
		}

		case FACE_DETECT_SINGLE_STREAM:
		default:
		{
			faceDetectionData.capture[0].open(faceDetectionConfig.cameraId[0]);

			if(!faceDetectionData.capture[0].isOpened())
			{
				cout << "Error: Unable to opeb input stream" << endl;
				exit(0);

			}
			faceDetectionData.frameSize.width = faceDetectionData.capture[0].get(CV_CAP_PROP_FRAME_WIDTH);
			faceDetectionData.frameSize.height = faceDetectionData.capture[0].get(CV_CAP_PROP_FRAME_HEIGHT);
			faceDetectionData.capture[0].set(CV_CAP_PROP_FRAME_WIDTH,640);
			faceDetectionData.capture[0].set(CV_CAP_PROP_FRAME_HEIGHT,480);



			break;
		}




	};


	faceDetectionData.outOfImages = false;

}



void faceDetectionGetNextFrames(FaceDetectionConfig &faceDetectionConfig, FaceDetectionData &faceDetectionData)
{

	switch(faceDetectionConfig.inputMethod)
	{
		case FACE_DETECT_VIDEO_FILE:
		{
			videoCapture->read(faceDetectionData.currentFrame[0]);
			faceDetectionData.currentInputImageId++;
			break;
		}
		case FACE_DETECT_STEREO_STREAM:
		{

			break;
		}
		case FACE_DETECT_IMAGE_FILE_LIST:
		{
			if(!faceDetectionConfig.bufferedImageList)
			{
				faceDetectionData.currentFrame[0] = imread(faceDetectionData.imageList[faceDetectionData.currentInputImageId]);
			}
			else
			{
				if(faceDetectionData.imageListBuffer[0].size() <=0)
				{
					if(faceDetectionData.currentInputImageId <faceDetectionData.imageList.size())
					{
						for(int i = faceDetectionData.currentInputImageId; ((i < faceDetectionConfig.bufferedImageListBufferSize)&& (i<faceDetectionData.imageList.size())); i++)
						{
							faceDetectionData.imageListBuffer[0].push_back(imread(faceDetectionData.imageList[i]));
						}
					}
					else
					{


						faceDetectionData.outOfImages = true;
					}

				}
				faceDetectionData.currentFrame[0]= faceDetectionData.imageListBuffer[0].front();
				faceDetectionData.imageListBuffer[0].pop_front();
			}
			faceDetectionData.currentInputImageId++;


			break;
		}

		case FACE_DETECT_SINGLE_STREAM:
		default:
		{

			faceDetectionData.capture[0] >> faceDetectionData.frameBuffer[0];

			faceDetectionData.currentFrame[0]=faceDetectionData.frameBuffer[0].clone();



			break;
		}




	};

	if(faceDetectionData.currentFrame[0].empty())
	{
		faceDetectionData.outOfImages = true;

	}

}

void setupFaceDetectionData(FaceDetectionConfig & faceDetectionConfig, FaceDetectionData & faceDetectionData)
{
	faceDetectionData.faceCascade.load(faceDetectionConfig.cascadeFaceDetectorFilename);

}

#ifdef PIPELINE
struct GetNextFramesArgs {
	FaceDetectionData *faceDetectionData;
	FaceDetectionConfig *faceDetectionConfig;
	pthread_barrier_t *barrier;
};

void *faceDetectionGetNextFramesThread(void *args)
{
	GetNextFramesArgs *thread_args = (GetNextFramesArgs *)args;
	FaceDetectionData *faceDetectionData = thread_args->faceDetectionData;
	FaceDetectionConfig *faceDetectionConfig = thread_args->faceDetectionConfig;

	while (1) {

		switch(faceDetectionConfig->inputMethod)
		{
			case FACE_DETECT_VIDEO_FILE:
			{
				videoCapture->read(faceDetectionData->currentFrame[1]);
				faceDetectionData->currentInputImageId++;
				break;
			}
			case FACE_DETECT_IMAGE_FILE_LIST:
			{
				faceDetectionData->currentFrame[1] = imread(faceDetectionData->imageList[faceDetectionData->currentInputImageId]);
				faceDetectionData->currentInputImageId++;
				break;
			}	
		}

		pthread_barrier_wait(thread_args->barrier);

		faceDetectionData->currentFrame[0] = faceDetectionData->currentFrame[1];

		if(faceDetectionData->currentFrame[0].empty())
		{
			faceDetectionData->outOfImages = true;
			pthread_barrier_wait(thread_args->barrier);
			break;
		}
		
		pthread_barrier_wait(thread_args->barrier);
	}

	return NULL;
}
#endif
 
 int faceDetection_main(int argc, const char * argv[])
{
	int key = -1;
	int lastKey = -1;
	FaceDetectionConfig faceDetectionConfig;
	FaceDetectionData faceDetectionData;


	cout << "Face Detection Application Starting" << endl;


	cout << "Beginning Setup" << endl;
	vector<string> commandLineArgs;
	for(int i = 0; i < argc; i++)
	{
		string currentString = argv[i];
		commandLineArgs.push_back(currentString);
	}

	if((commandLineArgs[1].compare("-configFile") ==0) || (commandLineArgs[1].compare("-ConfigFile") ==0))
	{
		loadFaceDetectionConfigFile(commandLineArgs, commandLineArgs[2]);



	}


	parseFaceDetectionConfigCommandVector(faceDetectionConfig, commandLineArgs);


	/*
	system_clock::time_point t_setupInput_1 = system_clock::now();

	faceDetectionSetupInput(	faceDetectionConfig,faceDetectionData);

	system_clock::time_point t_setupInput_2 = system_clock::now();
	t_setupInput += duration_cast<duration<double>>(t_setupInput_2 - t_setupInput_1).count();
	*/

	PROFILE_FUNC(t_setupInput, faceDetectionSetupInput(faceDetectionConfig, faceDetectionData));

	setupFaceDetectionData(faceDetectionConfig, faceDetectionData);


	cout << "Setup Complete" << endl;

	if(faceDetectionConfig.inputMethod ==FACE_DETECT_SINGLE_STREAM)
	{


		cout << "Allowing stream to setup" << endl;
		namedWindow("Stream warmup");

		key = -1;
		lastKey = -1;
		while(key != 27 && key != 'r' && key != 'd' && key != ' ')
		{
			faceDetectionData.capture[0] >> faceDetectionData.frameBuffer[0];
			faceDetectionData.currentFrame[0]=faceDetectionData.frameBuffer[0].clone();
			imshow("Stream warmup",faceDetectionData.currentFrame[0]);

			key = waitKey(33) & 0xff;
		}

		destroyWindow("Stream warmup");

	}

	faceDetectionSetupOutput( faceDetectionConfig,faceDetectionData);
	if(faceDetectionConfig.showWindows)
	{
		cout << "Creating display windows" << endl;

		//namedWindow("Current Frame Original");
		//namedWindow("Current Frame Grayscale");
		namedWindow("Current Frame Augmented");

		if (faceDetectionConfig.inputMethod == FACE_DETECT_VIDEO_FILE) {
			videoWriter = new VideoWriter("./faceDetectOutput.avi", 
										  static_cast<int>(videoCapture->get(CV_CAP_PROP_FOURCC)), 
										  videoCapture->get(CV_CAP_PROP_FPS),
										  Size((int) videoCapture->get(CV_CAP_PROP_FRAME_WIDTH), (int) videoCapture->get(CV_CAP_PROP_FRAME_HEIGHT)));
		}
	}
	cout << "Beginning Main Loop" << endl;


	key = -1;
	lastKey = -1;

	bool done = false;

	Mat tmpBuffer;



    vector<Rect> faces;
//    const static Scalar colors[] =  { CV_RGB(0,0,255),
//        CV_RGB(0,128,255),
//        CV_RGB(0,255,255),
//        CV_RGB(0,255,0),
//        CV_RGB(255,128,0),
//        CV_RGB(255,255,0),
//        CV_RGB(255,0,0),
//        CV_RGB(255,0,255)} ;
    Mat gray, smallImg( cvRound (faceDetectionData.frameSize.height/faceDetectionConfig.faceScale), cvRound(faceDetectionData.frameSize.width/faceDetectionConfig.faceScale), CV_8UC1 );

	Mat augmentedFrame;

#ifdef USE_MARSS
	cout << "Switching to simulation in Face Detection." << endl;
	ptlcall_switch_to_sim();
	//ptlcall_single_enqueue("-logfile augReality.log");
	ptlcall_single_enqueue("-stats faceDetect.stats");
	//ptlcall_single_enqueue("-loglevel 0");
#endif

#ifdef USE_GEM5
	#ifdef GEM5_CHECKPOINT_WORK
		m5_checkpoint(0, 0);
	#endif

	#ifdef GEM5_SWITCHCPU_AT_WORK
		m5_switchcpu();
	#endif 
	m5_dumpreset_stats(0, 0);
#endif



#ifdef TSC_TIMING
	READ_TIMESTAMP_WITH_WRAPPER( fd_timingVector[0] );
#endif
#ifdef CLOCK_GETTIME_TIMING
	GET_TIME_WRAPPER(fd_timeStructVector[0]);
#endif

#ifdef PIPELINE
	pthread_barrier_t pipelineBarrier;
	pthread_barrier_init(&pipelineBarrier, NULL, 2);

	struct GetNextFramesArgs getNextFramesArgs;
	getNextFramesArgs.faceDetectionData = &faceDetectionData;
	getNextFramesArgs.faceDetectionConfig = &faceDetectionConfig;
	getNextFramesArgs.barrier = &pipelineBarrier;

	pthread_t nextFramesThread;
	pthread_create(&nextFramesThread, NULL, faceDetectionGetNextFramesThread, (void *)(&getNextFramesArgs));

#ifdef FRAMERATE
	FILE *framerateFile = fopen("profiling/framerate.csv", "w");
	system_clock::time_point t_framerate_1, t_framerate_2;
#endif

	// Wait until first image is ready
	pthread_barrier_wait(&pipelineBarrier);
#endif

	while(!done)
	{
#ifdef FRAMERATE
		t_framerate_1 = system_clock::now();
#endif
		//system_clock::time_point t_frame_1 = system_clock::now();
		/*
		system_clock::time_point t_getNextFrames_1 = system_clock::now();

		faceDetectionGetNextFrames(faceDetectionConfig, faceDetectionData);
		
		system_clock::time_point t_getNextFrames_2 = system_clock::now();
		t_getNextFrames += duration_cast<duration<double>>(t_getNextFrames_2 - t_getNextFrames_1).count();
		//duration<double> t_getNextFrames_span = duration_cast<duration<double>>(t_getNextFrames_2 - t_getNextFrames_1);
		//std::cout << "faceDetectionGetNextFrames: " << t_getNextFrames_span.count() << " seconds" << std::endl;
		*/

#ifdef PIPELINE
		// Wait until image Mat is in currentFrames[0]
		PROFILE_FUNC(t_getNextFrames, pthread_barrier_wait(&pipelineBarrier));
#else
		PROFILE_FUNC(t_getNextFrames, faceDetectionGetNextFrames(faceDetectionConfig, faceDetectionData));
#endif
		

		if(faceDetectionData.outOfImages == true)
		{

			break;
		}



	    cvtColor( faceDetectionData.currentFrame[0], gray, CV_BGR2GRAY );
	    resize( gray, smallImg, smallImg.size(), 0, 0, INTER_LINEAR );
	    equalizeHist( smallImg, smallImg );

		/*
		system_clock::time_point t_detectMultiScale_1 = system_clock::now();

	    faceDetectionData.faceCascade.detectMultiScale( smallImg, faces,
	        1.1, 2, 0
	        //|CV_HAAR_FIND_BIGGEST_OBJECT
	        //|CV_HAAR_DO_ROUGH_SEARCH
	        |CV_HAAR_SCALE_IMAGE
	        ,
	        Size(30, 30) );

		system_clock::time_point t_detectMultiScale_2 = system_clock::now();
		t_detectMultiScale += duration_cast<duration<double>>(t_detectMultiScale_2 - t_detectMultiScale_1).count();
		//duration<double> time_span = duration_cast<duration<double>>(t_detectMultiScale_2 - t_detectMultiScale_1);
		//std::cout << "detectMultiScale: " << time_span.count() << " seconds" << std::endl;
		*/
		PROFILE_FUNC(t_detectMultiScale, faceDetectionData.faceCascade.detectMultiScale( smallImg, faces,
	        1.1, 2, 0
	        //|CV_HAAR_FIND_BIGGEST_OBJECT
	        //|CV_HAAR_DO_ROUGH_SEARCH
	        |CV_HAAR_SCALE_IMAGE
	        ,
	        Size(30, 30) )
		);

		//system_clock::time_point t_frame_2 = system_clock::now();
		//duration<double> t_frame_span = duration_cast<duration<double>>(t_frame_2 - t_frame_1);
		//std::cout << "frame time: " << t_frame_span.count() << " seconds" << std::endl;

		if(faceDetectionConfig.showWindows)
		{

			//imshow("Current Frame Original", faceDetectionData.currentFrame[0]);
			//imshow("Current Frame Grayscale", smallImg);
	    	augmentedFrame = faceDetectionData.currentFrame[0].clone();
		    for( vector<Rect>::const_iterator r = faces.begin(); r != faces.end(); r++ )
		    {

		        vector<Rect> nestedObjects;
		        Point center;
		        Scalar color = CV_RGB(255,0,0);
		        int radius;
		        center.x = cvRound((r->x + r->width*0.5)*faceDetectionConfig.faceScale);
		        center.y = cvRound((r->y + r->height*0.5)*faceDetectionConfig.faceScale);
		        radius = cvRound((r->width + r->height)*0.25*faceDetectionConfig.faceScale);
		        circle( augmentedFrame, center, radius, color, 3, 8, 0 );

		    }

			if (faceDetectionConfig.inputMethod != FACE_DETECT_VIDEO_FILE) {
				imshow("Current Frame Augmented", augmentedFrame);
				waitKey(0);
			} else {
				PROFILE_FUNC(t_videoWriter, videoWriter->write(augmentedFrame));
				//PROFILE_FUNC(t_videoWriter, imshow("Current Frame Augmented", augmentedFrame));
				//waitKey(1);
			}
		}

		//std::cout << "Face Detection completed. "  << faces.size()<< " Faces found in current iteration" <<endl;

		lastKey = key;
		if(!faceDetectionConfig.noWaitKey)
		{
			key = waitKey(30) & 0xff;
		}
		else
		{
			key = -1;

		}


		if(key == 'q' || key =='d' || key == 27)
		{

			done = true;
		}

#ifdef PIPELINE
		pthread_barrier_wait(&pipelineBarrier);
#endif

#ifdef FRAMERATE
		t_framerate_2 = system_clock::now();
		fprintf(framerateFile, "%f\n", duration_cast<duration<double>>(t_framerate_2 - t_framerate_1).count());
#endif
	}

#ifdef PIPELINE
	pthread_join(nextFramesThread, NULL);
	pthread_barrier_destroy(&pipelineBarrier);
#endif

#ifdef FRAMERATE
	fclose(framerateFile);
#endif

#ifdef FRAMERATE
	fclose(framerateFile);
#endif

	if (videoCapture) {
		videoCapture->release();
		delete videoCapture;
	}

	if (videoWriter) {
		delete videoWriter;
	}

#ifdef USE_MARSS
	ptlcall_kill();
#endif

#ifdef USE_GEM5

	m5_dumpreset_stats(0, 0);
	#ifdef GEM5_EXIT_AFTER_WORK
		m5_exit(0);
	#endif


#endif


#ifdef TSC_TIMING
	READ_TIMESTAMP_WITH_WRAPPER( fd_timingVector[0+ fd_timingVector.size()/2] );
#endif
#ifdef CLOCK_GETTIME_TIMING
	GET_TIME_WRAPPER(fd_timeStructVector[0+ fd_timeStructVector.size()/2]);
#endif
#ifdef TSC_TIMING
		fd_writeTimingToFile(fd_timingVector);
#endif
#ifdef CLOCK_GETTIME_TIMING
		fd_writeTimingToFile(fd_timeStructVector);
#endif


	cout << "Face Detection completed. "  << faces.size()<< " Faces found in last iteration" <<endl;
	if(faceDetectionConfig.showWindows)
	{
		cout << "Destroying display windows" << endl;

		destroyWindow("Current Frame Original");
		destroyWindow("Current Frame Grayscale");
		destroyWindow("Current Frame Augmented");
	}




 	return 0;
 }
 
 
#ifndef FACE_DETECTION_MODULE

int main(int argc, const char * argv[])
{
#ifdef TSC_TIMING
	fd_timingVector.resize(16);
#endif

#ifdef TSC_TIMING
	fd_timingVector.resize(16);
#endif

	/*
	system_clock::time_point t_total_1 = system_clock::now();

	int ret = faceDetection_main(argc, argv);

	system_clock::time_point t_total_2 = system_clock::now();
	double t_total = duration_cast<duration<double>>(t_total_2 - t_total_1).count();
	*/
	PROFILE_FUNC(t_total, int ret = faceDetection_main(argc, argv));


#ifdef PROFILING
	// Report timing
	std::cout << "\nTiming results\n";
	std::cout << "==============\n";
	std::cout << "faceDetectionSetupInput:     " << t_setupInput << " s  \t" << t_setupInput/t_total * 100 << " %\n";
	std::cout << "faceDetectionGetNextFrames:  " << t_getNextFrames << " s  \t" << t_getNextFrames/t_total * 100 << " %\n";
	std::cout << "detectMultiScale:            " << t_detectMultiScale << " s  \t" << t_detectMultiScale/t_total * 100 << " %\n";
	std::cout << "videoWriter->write:          " << t_videoWriter << " s  \t" << t_videoWriter/t_total * 100 << " %\n";
	std::cout << std::endl;
	std::cout << "Within detectMultiScale:\n";
	std::cout << "\tdetectSingleScale:           " << t_detectSingleScale << " s  \t" << t_detectSingleScale/t_total * 100 << " %  \t" << t_detectSingleScale/t_detectMultiScale * 100 << "%\n";
	std::cout << "\tgroupRectangles:             " << t_groupRectangles << " s  \t" << t_groupRectangles/t_total * 100 << " %  \t" << t_groupRectangles/t_detectMultiScale * 100 << "%\n";
	std::cout << "\tmultiLoopCalc:               " << t_multiLoopCalc << " s  \t" << t_multiLoopCalc/t_total * 100 << " %  \t" << t_multiLoopCalc/t_detectMultiScale * 100 << "%\n";
	std::cout << "\tdetectMultiScaleInternal:    " << t_detectMultiScaleInternal << " s  \t" << t_detectMultiScaleInternal/t_total * 100 << " %\n";
	std::cout << std::endl;
	std::cout << "Within detectSingleScale:\n";
	std::cout << "\tparallel_for:                " << t_parallelFor << " s  \t" << t_parallelFor/t_total * 100 << " %  \t" << t_parallelFor/t_detectSingleScale * 100 << "%\n";
	std::cout << std::endl;
	std::cout << "Total time:  " << t_total << " s\n\n";
#endif

	return ret;
}
#endif
 
