## Table of Contents

  - [Sample Introduction](#sample-introduction)
  - [Sample Flowchart](#sample-flowchart)
  - [Directory Structure](#directory-structure)
  - [Getting Source Code](#getting-source-code) 
  - [Third-party Dependency Installation](#third-party-dependency-installation)
  - [Sample Running](#sample-running)
  - [Other Resources](#other-resources)
  - [Update Instructions](#update-instructions)
  - [Known Issues](#known-issues)
    
## Sample Introduction
Function: Uses the YOLOv7 model to perform inference and prediction on input data, detecting all detectable objects in images/videos, and prints the inference results to the output. This is a high-performance sample based on a multi-channel and multi-threaded architecture, processing multi-channel data in parallel across multiple cards and outputting the results. It supports various inputs and outputs.    
Sample Input: Raw JPG image files / MP4 video files / H.26X video files / RTSP video streams.   
Sample Output: Images with inference results / Video files with inference results / RTSP video stream display / cv::imshow window display / On-screen display. 

## Sample Flowchart
The one-stop general object detection and recognition solution is a high-performance sample based on a multi-channel and multi-threaded design, processing multi-channel data in parallel via multiple cards.
The overall flowchart is shown below:
![Flowchart](https://obs-9be7.obs.cn-east-2.myhuaweicloud.com/003_Atc_Models/AE/ATC%20Model/sampleYolov7MultiInput/%E6%B5%81%E7%A8%8B%E5%9B%BE.png)

- Management Thread: Packages threads and queues together, completing process creation, message queue creation, message sending, and message receiving daemons.
- Data Input Thread: Decodes input images or videos.
- Data Preprocessing Thread: Processes YUV images sent from the Data Input Thread (resize and other operations).
- Inference Thread: Performs inference using the YOLOv7 model.
- Data Postprocessing Thread: Analyzes inference results and outputs bounding box points and label information.
- Data Output Thread: Marks bounding box points and label information onto the output data.


## Directory Structure

```
├── model                       // Model folder, stores model files required for running the sample
│   └── xxxx.onnx

├── data                        // Data folder
│   └── xxxx                    // Test data, input videos
├── pic                         // Data folder
│   └── xxxx                    // Test data, input images
├── inc                         // Header files folder
│   ├── Params.h                // Header file declaring data structures used by the sample
│   └── label.h                 // Header file declaring category labels used by the sample model
├── out                         // Build output folder, stores executable files generated from compilation
│   ├── xxxx                    // Executable files
│   └── xxxx                    // Output result images/videos from running the sample
├── scripts                     // Configuration files + scripts folder
│   ├── test.json               // Parameter configuration file used for running the sample
│   ├── sample_build.sh         // Quick build script
│   ├── sample_run.sh           // Quick run script
├── src
│   ├── acl.json                // System initialization configuration file
│   ├── CMakeLists.txt          // CMake build file
│   ├── dataInput               // Data input & decoding thread folder, stores headers and source code for this business thread
│   ├── dataOutput              // Data output processing thread folder, stores headers and source code for this business thread
│   ├── detectInference         // Detection model inference thread folder, stores headers and source code for this business thread
│   ├── detectPreprocess        // Detection model preprocessing thread folder, stores headers and source code for this business thread
│   ├── detectPostprocess       // Detection model postprocessing thread folder, stores headers and source code for this business thread
│   ├── pushrtsp                // RTSP display thread folder, stores headers and source code for this business thread
│   └── main.cpp                // Main function, implementation file for YOLO detection features
├── configDemo.md               // Configuration examples for test.json under other input/output scenarios, multi-batch model scenarios, and single-channel input corresponding to multi-postprocessing scenarios

└── CMakeLists.txt              // Build script entry point, calls the CMakeLists file under the src directory
```

## Getting Source Code
    
 You can use either of the following two methods to download the code. Please choose one to prepare the source code.

 - Command-line download (longer download time, but simpler steps).
   ```    
   # In the development environment, execute the following command as a non-root user to clone the repository.
cd ${HOME}

git clone https://gitee.com/ascend/samples.git
   ```
   **Note: If you need to switch to another tag version (e.g., v0.5.0), execute the following command.**
   ```
   git checkout v0.5.0
   ```   
 - ZIP package download (shorter download time, but slightly more complex steps).   
**Note: If you need to download code for other versions, please switch the samples repository branch according to the prerequisites instructions first.** ``` 
# 1. In the upper right corner of the samples repository, click [Clone/Download] dropdown and select [Download ZIP].    
# 2. Upload the ZIP package to the home directory of a normal user in the development environment [e.g., ${HOME}/ascend-samples-master.zip].     
# 3. In the development environment, execute the following command to unzip the package.     
cd ${HOME}    
unzip ascend-samples-master.zip
   ```

## Third-party Dependency Installation
 Set environment variables to configure the header file and library paths required for program compilation. Replace "$HOME/Ascend" with the actual installation path of your "Ascend-cann-toolkit" package.
   ```
    export DDK_PATH=$HOME/Ascend/ascend-toolkit/latest
    export NPU_HOST_LIB=$DDK_PATH/runtime/lib64/stub
    export THIRDPART_PATH=${DDK_PATH}/thirdpart
    export LD_LIBRARY_PATH=${THIRDPART_PATH}/lib:$LD_LIBRARY_PATH
   ```
  Create the THIRDPART_PATH directory

   ```
    mkdir -p ${THIRDPART_PATH}
   ```
- x264

    Execute the following commands to install x264:
   ```
   cd ${HOME}
   git clone https://code.videolan.org/videolan/x264.git
   cd x264
   # install x264
   ./configure --enable-shared --disable-asm
   make
   sudo make install
   sudo cp /usr/local/lib/libx264.so.164 /lib
   ```   

- ffmpeg

  Execute the following commands to install ffmpeg
   ```
   cd ${HOME}
   wget http://www.ffmpeg.org/releases/ffmpeg-4.1.3.tar.gz --no-check-certificate
   tar -zxvf ffmpeg-4.1.3.tar.gz
   cd ffmpeg-4.1.3
   # install ffmpeg
   ./configure --enable-shared --enable-pic --enable-static --disable-x86asm --enable-libx264 --enable-gpl --prefix=${THIRDPART_PATH}
   make -j8
   make install
   ```   
   
   </details> 

- opencv

  Execute the following commands to install opencv (Note: Ensure version is 3.x)
  ```
  sudo apt-get install libopencv-dev
  ```   
  If you are using OpenCV version 4.x, you can execute the following command (tested with 4.5.4 and 4.9.0):
  ```
  sudo ln -s /usr/include/opencv4/opencv2 /usr/include/opencv2
  ```

- jsoncpp

  Install via apt command jsoncpp：

   ```
   # After installation, static libraries are in /usr/include; dynamic libraries are in /usr/lib/x86_64-linux-gnu
   sudo apt-get install libjsoncpp-dev 
   sudo ln -s /usr/include/jsoncpp/json/ /usr/include/json
   ```

## Sample Running
   > Note: Here, one MP4 video is used as input, and saving the output as an offline MP4 video is taken as an example to demonstrate and verify sample execution. For more parameter configuration details regarding sample execution, please refer to [Sample Parameter Configuration Instructions](./configDemo.md).

  - Data Preparation

    Please obtain the input video for this sample from the link below and place it in the data directory.        
    ```    
    cd $HOME/samples/inference/modelInference/sampleYOLOV7MultiInput/data
    wget https://obs-9be7.obs.cn-east-2.myhuaweicloud.com/003_Atc_Models/AE/ATC%20Model/YOLOV3_carColor_sample/data/car0.mp4 --no-check-certificate
    ```

  - ATC Model Conversion

    Convert the original YOLOv7 model into an offline model (*.om file) adapted for the Ascend 310 processor and place it under the model path.
   
    ```
    # For convenience, the raw model download and model conversion commands are provided directly here and can be copied and executed.
    cd $HOME/samples/inference/modelInference/sampleYOLOV7MultiInput/model
    # Download the YOLOv7 original model file and AIPP configuration file
    wget https://obs-9be7.obs.cn-east-2.myhuaweicloud.com/003_Atc_Models/yolov7/yolov7x.onnx --no-check-certificate
    wget https://obs-9be7.obs.cn-east-2.myhuaweicloud.com/003_Atc_Models/yolov7/aipp.cfg --no-check-certificate
    # Please use the <soc_version> value corresponding to your chip name for model conversion before performing inference
    atc --model=yolov7x.onnx --framework=5 --output=yolov7x --input_shape="images:1,3,640,640"  --soc_version=Ascend310  --insert_op_conf=aipp.cfg
    ```

  - Sample Compilation

    Execute the following commands to run the build script and start sample compilation.
    ```
    cd $HOME/samples/inference/modelInference/sampleYOLOV7MultiInput/scripts
    bash sample_build.sh
    ```

  - Sample Running

    Execute the run script to start running the sample.
    ```
    bash sample_run.sh
    ```

  - Sample Results Display
    
    The application's inference results, including object detection box information, will be output according to the result output mode configured in the `scripts/test.json` file.
    When the output data type is configured as video, the output data will be stored in the `out` folder as a video named similarly to: **XXXX.mp4**.

    Upon successful execution, the sample will output different files according to the configured output data type. Note that details such as result box coordinates may vary depending on versions and environments; please refer to the actual scenario.

    - If the output data type is configured as pic:
      The output data is stored in the `out/` folder as images named similarly to **channel_X_out_pic_Y.jpg**, where X represents channel x and Y represents the y-th image.

    - If the output data type is configured as video:
      The output data is stored in the `out/output` folder as videos named similarly to: **XXXX.mp4**, where X represents channel x.

## Other Resources

The following resources offer further insights into the general object recognition sample, including custom development and performance improvement:

**Documentation**
- [General Object Recognition Sample](https://gitee.com/ascend/samples/wikis/%E9%80%9A%E7%94%A8%E7%9B%AE%E6%A0%87%E8%AF%86%E5%88%AB%E6%A0%B7%E4%BE%8B/%E5%89%8D%E8%A8%80/%E6%A6%82%E8%BF%B0)
- [AscendCL Samples介绍](../README_CN.md)
- [Developing Deep Neural Network Applications Using AscendCL API Library](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/600alpha006/infacldevg/aclcppdevg/aclcppdevg_000000.html)
- [Ascend Documentation](https://www.hiascend.com/document?tag=community-developer)


  
## Known Issues

  None
