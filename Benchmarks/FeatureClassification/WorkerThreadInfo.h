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
 * WorkerThreadInfo.h
 *
 *  Created on: Apr 30, 2011
 *      Author: jlclemon
 */




#ifndef WORKERTHREADINFO_H_
#define WORKERTHREADINFO_H_


#include "ThreadManager.h"
#include "MultiThreadedMatResult.h"
#include "FeatureClassification.hpp"
#include "MultiThreadAlgorithmData.h"

struct FeatureClassificationConfig;
struct WorkerThreadInfo
{
	Thread * myThread;
	FeatureClassificationConfig * myFeatureClassificationInfo;
	int verticalThreadId;
	int horizontalThreadId;
	int descriptorStartIndex;
	Mat descriptorsToProcess;
	bool standAlone;
	bool coordinator;

	bool done;
	vector<KeyPoint> keyPointsToProcess;
	MultiThreadedMatchResult * multiThreadMatchResults;
	MultiThreadedMatResult* multiThreadResults;
	FeatureClassificationConfig * featureClassificationInfo;
	ThreadManager * threadManager;
	Mat * allTrainDescriptors;
	Mat * allQueryDescriptors;
	vector<KeyPoint> * allTrainKeypoints;
	vector<KeyPoint> * allQueryKeypoints;
	Mat * trainImage;
	Mat * queryImage;
	struct MultiThreadAlgorithmData * multiThreadAlgorithmData;

};







#endif /* WORKERTHREADINFO_H_ */
