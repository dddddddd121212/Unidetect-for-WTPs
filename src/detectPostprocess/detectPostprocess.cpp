/**
* Copyright (c) Huawei Technologies Co., Ltd. 2020-2022. All rights reserved.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at

* http://www.apache.org/licenses/LICENSE-2.0

* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.

* File sample_process.cpp
* Description: handle acl resource
*/
#include <fstream>//
//#include <time.h>
#include <iostream>
#include "acl/acl.h"
#include "Params.h"
#include "detectPostprocess.h"
#include "AclLiteUtils.h"
#include "AclLiteApp.h"
#include "label.h"
#include <ctime>//we add
#include <string>// we add
#include <unistd.h>  	
#include <arpa/inet.h>
#include "../dataSend/Server.h"
#include <mutex>
#include <atomic>
#include <vector>
#include <dirent.h>//wyk add
#include <sys/stat.h>
#include <deque>//wyk wdd
#include <unordered_map>//wyk wdd
#include <numeric>//wyk wdd
#include <sstream>//wyk wdd
//#include <filesystem>//wyk wdd



using namespace std;
//extern Server server;

namespace {
    const uint32_t kSleepTime = 500;
    const double kFountScale = 0.5;
    const cv::Scalar kFountColor(0, 0, 255);
    const uint32_t kLabelOffset = 11;
    const uint32_t kLineSolid = 2;
    const vector <cv::Scalar> kColors{
        cv::Scalar(237, 149, 100), cv::Scalar(0, 215, 255),
        cv::Scalar(50, 205, 50), cv::Scalar(139, 85, 26)};
    typedef struct BoundBox {
        float x;
        float y;
        float width;
        float height;
        float score;
        size_t classIndex;
        size_t index;
    } BoundBox;

    bool sortScore(BoundBox box1, BoundBox box2)
    {
        return box1.score > box2.score;
    }
}
const int num_categories = 17;  
std::unordered_map<int, int> category_window_sizes = {
    {0, 3}, {1, 3}, {2, 3}, {3, 3}, {4, 3}, {5, 3},
    {6, 4}, {7, 3}, {8, 1}, {9, 3}, {10, 3}, {11, 3},
    {12, 3}, {13, 3}, {14, 3}, {15, 3}, {16, 3}
};

std::unordered_map<int, int> category_vote_thresholds = {
    {0, 1}, {1, 1}, {2, 1}, {3, 1}, {4, 1}, {5, 1},
    {6, 1}, {7, 1}, {8, 1}, {9, 1}, {10, 1}, {11, 1},
    {12, 1}, {13, 1}, {14, 1}, {15, 1}, {16, 1}
};
std::mutex vote_mutex;

std::unordered_map<int, std::unordered_map<int, std::deque<int>>> video_vote_buffers;
void update_vote_buffer(int video_id, const std::string& detection_result_str) {
    std::lock_guard<std::mutex> lock(vote_mutex);

    for (int i = 0; i < detection_result_str.size(); ++i) {
        int result = detection_result_str[i] - '0'; 
        int win_size = category_window_sizes[i];   
        video_vote_buffers[video_id][i].push_back(result);

        if (video_vote_buffers[video_id][i].size() > win_size) {
            video_vote_buffers[video_id][i].pop_front(); 
        }
    }
}


bool is_detection_valid(int video_id, int category_id) {
    std::lock_guard<std::mutex> lock(vote_mutex);

    const auto& votes = video_vote_buffers[video_id][category_id];
    int vote_count = std::accumulate(votes.begin(), votes.end(), 0);
    int threshold = category_vote_thresholds[category_id]; 
    return vote_count >= threshold; 
}

std::string process_frame(int video_id, const std::string& detection_result_str) {
    update_vote_buffer(video_id, detection_result_str);

    std::string result_str;
    for (int category_id = 0; category_id < num_categories; ++category_id) {
        if (is_detection_valid(video_id, category_id)) {
            result_str += "1"; 
        }
        else {
            result_str += "0"; 
        }
    }
    return result_str; 
}

DetectPostprocessThread::DetectPostprocessThread(uint32_t modelWidth, uint32_t modelHeight,
    aclrtRunMode& runMode, uint32_t batch, int32_t channelId,string inputDataPath)
    :modelWidth_(modelWidth), modelHeight_(modelHeight), runMode_(runMode),
    sendLastBatch_(false), batch_(batch), channelId_(channelId),inputDataPath_(inputDataPath)
{

}

DetectPostprocessThread::~DetectPostprocessThread() {
}

AclLiteError DetectPostprocessThread::Init()
{
	//server.start();
    return ACLLITE_OK;
}

AclLiteError DetectPostprocessThread::Process(int msgId, shared_ptr<void> data)
{
    //auto detectDataMsg = static_pointer_cast<DetectDataMsg>(data);
    //dectDataMsg->rtspIp = rtspIp_;
    AclLiteError ret = ACLLITE_OK;
    switch (msgId) {
        case MSG_POSTPROC_DETECTDATA:
            InferOutputProcess(static_pointer_cast<DetectDataMsg>(data));
            MsgSend(static_pointer_cast<DetectDataMsg>(data));
            break;
        default:
            ACLLITE_LOG_INFO("Detect PostprocessThread thread ignore msg %d", msgId);
            break;
    }

    return ret;
}


AclLiteError DetectPostprocessThread::InferOutputProcess(shared_ptr<DetectDataMsg> detectDataMsg)
{		
    size_t pos = 0;
    for (int n = 0; n < detectDataMsg->decodedImg.size(); n++) {
        void* dataBuffer = CopyDataToHost(detectDataMsg->inferenceOutput[0].data.get() + pos,
            detectDataMsg->inferenceOutput[0].size / batch_, runMode_, MEMORY_NORMAL);
        if (dataBuffer == nullptr) {
            ACLLITE_LOG_ERROR("Copy inference output to host failed");
            return ACLLITE_ERROR_COPY_DATA;
        }
        pos = pos + detectDataMsg->inferenceOutput[0].size / batch_;
        float* detectBuff = static_cast<float*>(dataBuffer);

        // confidence threshold
        float confidenceThreshold = 0.3;

        // class number
        size_t classNum = 31; //here 80!!!

        // number of (x, y, width, hight, confidence)
        size_t offset = 5;

        // total number = class number + (x, y, width, hight, confidence)
        size_t totalNumber = classNum + offset;

        // total number of boxs
        size_t modelOutputBoxNum = 25200;

        // top 5 indexes correspond (x, y, width, hight, confidence),
        // and 5~85 indexes correspond object's confidence
        size_t startIndex = 5;

        // get srcImage width height
        int srcWidth = detectDataMsg->decodedImg[n].width;
        int srcHeight = detectDataMsg->decodedImg[n].height;

        // filter boxes by confidence threshold
        vector <BoundBox> boxes;
        size_t yIndex = 1;
        size_t widthIndex = 2;
        size_t heightIndex = 3;
        size_t classConfidenceIndex = 4;
        for (size_t i = 0; i < modelOutputBoxNum; ++i) {
            float maxValue = 0;
            float maxIndex = 0;
            for (size_t j = startIndex; j < totalNumber; ++j) {
                float value = detectBuff[i * totalNumber + j] * detectBuff[i * totalNumber + classConfidenceIndex];
                    if (value > maxValue) {
                    // index of class
                    maxIndex = j - startIndex;
                    maxValue = value;
                }
            }
            float classConfidence = detectBuff[i * totalNumber + classConfidenceIndex];
            if (classConfidence >= confidenceThreshold) {
                // index of object's confidence
                size_t index = i * totalNumber + maxIndex + startIndex;

                // finalConfidence = class confidence * object's confidence
                float finalConfidence =  classConfidence * detectBuff[index];
                BoundBox box;
                box.x = detectBuff[i * totalNumber] * srcWidth / modelWidth_;
                box.y = detectBuff[i * totalNumber + yIndex] * srcHeight / modelHeight_;
                box.width = detectBuff[i * totalNumber + widthIndex] * srcWidth/modelWidth_;
                box.height = detectBuff[i * totalNumber + heightIndex] * srcHeight / modelHeight_;
                box.score = finalConfidence;
                box.classIndex = maxIndex;
                box.index = i;
                if (maxIndex < classNum) {
                    boxes.push_back(box);
                }
            }
            }

        // filter boxes by NMS
        vector <BoundBox> result;
        result.clear();
        float NMSThreshold = 0.45;
        int32_t maxLength = modelWidth_ > modelHeight_ ? modelWidth_ : modelHeight_;
        std::sort(boxes.begin(), boxes.end(), sortScore);
        BoundBox boxMax;
        BoundBox boxCompare;
        while (boxes.size() != 0) {
            size_t index = 1;
            result.push_back(boxes[0]);
            while (boxes.size() > index) {
                boxMax.score = boxes[0].score;
                boxMax.classIndex = boxes[0].classIndex;
                boxMax.index = boxes[0].index;

                // translate point by maxLength * boxes[0].classIndex to
                // avoid bumping into two boxes of different classes
                boxMax.x = boxes[0].x + maxLength * boxes[0].classIndex;
                boxMax.y = boxes[0].y + maxLength * boxes[0].classIndex;
                boxMax.width = boxes[0].width;
                boxMax.height = boxes[0].height;

                boxCompare.score = boxes[index].score;
                boxCompare.classIndex = boxes[index].classIndex;
                boxCompare.index = boxes[index].index;

                // translate point by maxLength * boxes[0].classIndex to
                // avoid bumping into two boxes of different classes
                boxCompare.x = boxes[index].x + boxes[index].classIndex * maxLength;
                boxCompare.y = boxes[index].y + boxes[index].classIndex * maxLength;
                boxCompare.width = boxes[index].width;
                boxCompare.height = boxes[index].height;

                // the overlapping part of the two boxes
                float xLeft = max(boxMax.x, boxCompare.x);
                float yTop = max(boxMax.y, boxCompare.y);
                float xRight = min(boxMax.x + boxMax.width, boxCompare.x + boxCompare.width);
                float yBottom = min(boxMax.y + boxMax.height, boxCompare.y + boxCompare.height);
                float width = max(0.0f, xRight - xLeft);
                float hight = max(0.0f, yBottom - yTop);
                float area = width * hight;
                float iou =  area / (boxMax.width * boxMax.height + boxCompare.width * boxCompare.height - area);

                // filter boxes by NMS threshold
                if (iou > NMSThreshold) {
                    boxes.erase(boxes.begin() + index);
                    continue;
                }
                ++index;
            }
            boxes.erase(boxes.begin());
        }

        // opencv draw label params
        int half = 2;
	    //int count = 0;

        cv::Point leftTopPoint;  // left top
        cv::Point rightBottomPoint;  // right bottom
        string className;  // yolo detect output

        // calculate framenum
        int frameCnt = (detectDataMsg->msgNum) * batch_ + n + 1;

        stringstream sstream;
        sstream.str("");
        sstream << "Channel-" << detectDataMsg->channelId << "-Frame-" << to_string(frameCnt) << "-result: ";
        
        string textHead = "";
        sstream >> textHead;
        string textMid = "[";
        for (size_t i = 0; i < result.size(); ++i) {
            leftTopPoint.x = result[i].x - result[i].width / half;
            leftTopPoint.y = result[i].y - result[i].height / half;
            rightBottomPoint.x = result[i].x + result[i].width / half;
            rightBottomPoint.y = result[i].y + result[i].height / half;
            className = label[result[i].classIndex] + ":" + to_string(result[i].score);
           //ACLLITE_LOG_INFO("Detect resule class type name is %d", result[i].classIndex); //here !!! show classnum on terminal
            cv::rectangle(detectDataMsg->frame[n], leftTopPoint,
                rightBottomPoint, kColors[i % kColors.size()], kLineSolid);
            cv::putText(detectDataMsg->frame[n], className,
                cv::Point(leftTopPoint.x, leftTopPoint.y + kLabelOffset),
                cv::FONT_HERSHEY_COMPLEX, kFountScale, kFountColor);
            textMid = textMid + className + " ";            
        }
        string textPrint = textHead + textMid + "]";
        detectDataMsg->textPrint.push_back(textPrint);

	int rtsp_start =  inputDataPath_.find("@");
	int rtsp_end = inputDataPath_.find('/',rtsp_start);
	std::string rtspIP = inputDataPath_.substr(rtsp_start+1,rtsp_end-rtsp_start-1);

        int personCount = 0, helmetCount = 0, saftyVestCount = 0,shortSleeveCount =0, longSleeveCount=0,shortsCount = 0, trousers = 0, coverallShortsleeveCount = 0, coverallLongsleeveCount = 0, coverallJacketCount = 0, black_trousers = 0, falldownCount = 0, phoningCount = 0, smokingbehaviorCount = 0, firesamllCount = 0, firenormalCount = 0, smokeCount = 0, soilCount = 0, coverallwhitecoatCount = 0, liquidCount =0, flowCount = 0, dogCount = 0, catCount = 0, vehicleCount = 0, electricVehicleCount = 0, playmobileCount = 0, waterCount = 0, lightCount = 0, birdCount = 0, errorCount = 0, sunsetCount = 0; 
		for(size_t i=0; i<result.size(); ++i)
		{
			if(result[i].classIndex == 0 && result[i].score > 0.6 && (((rightBottomPoint.x-leftTopPoint.x) > 40) && ((rightBottomPoint.y-leftTopPoint.y) >60))){
				personCount++;
				//int centerX = result[i].x + result[i].width/2;
				//int centerY = result[i].y + result[i].height/2;
				//std::cout << "P" << centerX << "," << centerY << endl; 
				//ACLLITE_LOG_INFO("(%d,%d)",result[i].x,result[i].y);
			}
			if(result[i].classIndex == 1 && result[i].score > 0.0){
				helmetCount++;
			}
			if(result[i].classIndex == 2 && result[i].score > 0.1){
				saftyVestCount++;
			}
			if(result[i].classIndex == 3 && result[i].score > 0.1){
				shortSleeveCount++;
			}
			if(result[i].classIndex == 4 && result[i].score > 0.1){
				longSleeveCount++;
			}
			if(result[i].classIndex == 5 && result[i].score > 0.1){
				shortsCount++;
			}
			if(result[i].classIndex == 6 && result[i].score > 0.1){
				trousers++;
			}
			if(result[i].classIndex == 7 && result[i].score > 0.0){
				coverallShortsleeveCount++;
			}
			if(result[i].classIndex == 8 && result[i].score > 0.0){
				coverallLongsleeveCount++;
			}
			if(result[i].classIndex == 9 && result[i].score > 0.0){
				coverallJacketCount++;
			}
			if(result[i].classIndex == 10 && result[i].score > 0.0){
				black_trousers++;
			}
			if(result[i].classIndex == 11 && result[i].score > 0.4 && result[i].x>150 &&(((rightBottomPoint.x-leftTopPoint.x) > 80) && ((rightBottomPoint.y-leftTopPoint.y) >60))){
				falldownCount++;
			}
			if(result[i].classIndex == 12 && result[i].score > 0.6){
				phoningCount++;
			}
			if(result[i].classIndex == 13 && result[i].score > 0.6){
				smokingbehaviorCount++;
			}
			if(result[i].classIndex == 14 && result[i].score > 0.4){
				firesamllCount++;
			}
			if(result[i].classIndex == 15 && result[i].score > 0.6){
				smokeCount++;
			}
			//if(result[i].classIndex == 15 && result[i].score > 0.8 && (((rightBottomPoint.x-leftTopPoint.x) > 80) && ((rightBottomPoint.y-leftTopPoint.y) >60))){
			//	smokeCount++;
			//}
			if(result[i].classIndex == 16 && result[i].score > 0.1){
				soilCount++;
			}
			if(result[i].classIndex == 17 && result[i].score > 0.1){
				coverallwhitecoatCount++;
			}
			if(result[i].classIndex == 18 && result[i].score > 0.1){
				liquidCount++;
			}
			if(result[i].classIndex == 19 && result[i].score > 0.1){
				flowCount++;
			}
			if(result[i].classIndex == 20 && result[i].score > 0.1){
				dogCount++;
			}
			if(result[i].classIndex == 21 && result[i].score > 0.1){
				catCount++;
			}
			if(result[i].classIndex == 22 && result[i].score > 0.3){
				vehicleCount++;
			}
			if(result[i].classIndex == 23 && result[i].score > 0.3){
				electricVehicleCount++;
			}
			if(result[i].classIndex == 24 && result[i].score > 0.5){
				playmobileCount++;
			}
			if(result[i].classIndex == 25 && result[i].score > 0.4){
				firenormalCount++;
			}
			if(result[i].classIndex == 26 && result[i].score > 0.7){
				lightCount++;
			}
			if(result[i].classIndex == 27 && result[i].score > 0.7){
				birdCount++;
			}
			if(result[i].classIndex == 28 && result[i].score > 0.1){
				errorCount++;
			}
			if(result[i].classIndex == 29 && result[i].score > 0.7){
				sunsetCount++;
			}
			if(result[i].classIndex == 30 && result[i].score > 0.7){
				sunsetCount++;
			}
		}
		
	int alarmSignal_helmet = 0, alarmSignal_safetyVest = 0, alarmSignal_shorts = 0, alarmSignal_coverall = 0, alarmSignal_fall = 0, alarmSignal_leavePost = 0, alarmSignal_somking = 0, alarmSignal_phoning = 0, alarmSignal_wander = 0, alarmSignal_fire = 0, alarmSignal_smoke = 0, alarmSignal_choke = 0, alarmSignal_soil = 0, alarmSignal_leak = 0,alarmSignal_medicate = 0, alarmSignal_filer = 0, alarmSignal_liquidLevel = 0; 

        if (personCount > (helmetCount)) {
        		alarmSignal_helmet = 1;
        }
        if (personCount > saftyVestCount) {
        		alarmSignal_safetyVest  = 1;
        }
        if (shortsCount > 0) {
        		alarmSignal_shorts  = 1;
        }
        if ((personCount > (coverallShortsleeveCount+coverallLongsleeveCount+coverallJacketCount+2) && (coverallwhitecoatCount == 0) )) {
        		alarmSignal_coverall  = 1;
        }
		if (falldownCount > 0){
			alarmSignal_fall = 1;
		}
		if (personCount == 0){
			alarmSignal_leavePost = 1;
		}
		if (smokingbehaviorCount > 0 && personCount > 0){
			alarmSignal_somking = 1;
		}
		if ((phoningCount > 0 || playmobileCount > 0) && personCount > 0){
			alarmSignal_phoning = 1;
		}

		if (firesamllCount > 0 || firenormalCount > 0){
			alarmSignal_fire = 1;
		}
		if (smokeCount > 0){
			alarmSignal_smoke = 1;
		}

		if (vehicleCount > 0 || electricVehicleCount > 0){
			alarmSignal_choke = 1;
		}
		if (soilCount > 0){
			alarmSignal_soil = 1;
		}
		if (personCount > 0){
			alarmSignal_wander = 1;
		}
		if(flowCount == 0){
			alarmSignal_medicate = 0;			
		}
		if(liquidCount == 0){
			alarmSignal_filer = 0;
			alarmSignal_liquidLevel = 0;		
		}
		if(waterCount > 0){
			alarmSignal_leak = 0;				
		}

        std::stringstream alarmStream;

    alarmStream <<alarmSignal_helmet 
                <<alarmSignal_safetyVest 
                <<alarmSignal_shorts 
                <<alarmSignal_coverall
                <<alarmSignal_fall
                <<alarmSignal_leavePost 
                <<alarmSignal_somking
                <<alarmSignal_phoning
                <<alarmSignal_fire
                <<alarmSignal_smoke
				<<alarmSignal_choke
                <<alarmSignal_soil
                <<alarmSignal_leak
                <<alarmSignal_medicate
                <<alarmSignal_filer
                <<alarmSignal_liquidLevel
                <<alarmSignal_wander; 

		std::string alarmData_temp1 = alarmStream.str();
        std::string alarmResult = process_frame(channelId_, alarmData_temp1);
		std::stringstream alarmResultStream;
		alarmResultStream << rtspIP << "+" << alarmResult;
		for(size_t i=0; i<result.size(); ++i)
			{
				if(result[i].classIndex == 0 && result[i].score > 0.7){
				int centerX = (result[i].x)*798/srcWidth;
				int centerY = (result[i].y)*598/srcHeight;
				alarmResultStream << "P(" << centerX << "," << centerY <<")";
				}
				if((result[i].classIndex == 14 || result[i].classIndex == 25 || result[i].classIndex == 26 )&& result[i].score > 0.4){
				int centerX = (result[i].x)*798/srcWidth;
				int centerY = (result[i].y)*598/srcHeight;
				alarmResultStream << "F(" << centerX << "," << centerY <<")";
				}
			}
			alarmResultStream << "&";
			std::string alarmData = alarmResultStream.str();
			if(errorCount == 0){
			server.sendData(alarmData);
				}
			//std::cout<<alarmData<<std::endl;
			/*if(frameCnt%20==0 && errorCount == 0){
				server.sendData(alarmData);
				}*/
			//cout<<"sendData address: "<<(void*)&sendMessage <<endl;
	
        free(detectBuff);
        detectBuff = nullptr;
    }
    return ACLLITE_OK;
}
    
AclLiteError DetectPostprocessThread::MsgSend(shared_ptr<DetectDataMsg> detectDataMsg)
{
    if (!sendLastBatch_) {
        while (1) {
            AclLiteError ret = SendMessage(detectDataMsg->dataOutputThreadId, MSG_OUTPUT_FRAME, detectDataMsg);
            if (ret == ACLLITE_ERROR_ENQUEUE) {
                usleep(kSleepTime);
                continue;
            } else if(ret == ACLLITE_OK) {
                break;
            } else {
                ACLLITE_LOG_ERROR("Send read frame message failed, error %d", ret);
                return ret;
            }
        }
    }
    if (detectDataMsg->isLastFrame && sendLastBatch_) {
        while (1) {
            AclLiteError ret = SendMessage(detectDataMsg->dataOutputThreadId, MSG_ENCODE_FINISH, detectDataMsg);
            if (ret == ACLLITE_ERROR_ENQUEUE) {
                usleep(kSleepTime);
                continue;
            } else if(ret == ACLLITE_OK) {
                break;
            } else {
                ACLLITE_LOG_ERROR("Send read frame message failed, error %d", ret);
                return ret;
            }
        }
    }
    if (detectDataMsg->isLastFrame && !sendLastBatch_) {
        while (1) {
            AclLiteError ret = SendMessage(detectDataMsg->dataOutputThreadId, MSG_ENCODE_FINISH, detectDataMsg);
            if (ret == ACLLITE_ERROR_ENQUEUE) {
                usleep(kSleepTime);
                continue;
            } else if(ret == ACLLITE_OK) {
                break;
            } else {
                ACLLITE_LOG_ERROR("Send read frame message failed, error %d", ret);
                return ret;
            }
        }
        sendLastBatch_ = true;
    }

    return ACLLITE_OK;
}
