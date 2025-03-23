/*
   Copyright 2025 Morten Michael Lindahl

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/







#include <opencv2/opencv.hpp>
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <mutex>
#include <thread>
#include <cstdio>
#include <fcntl.h>
#include <filesystem>

#if defined(_WIN32) || defined(_WIN64)
    #include <io.h>
    #define NULL_DEVICE "NUL"
    namespace fs = std::filesystem;
#else
    #include <unistd.h>
    #define NULL_DEVICE "/dev/null"
    namespace fs = std::filesystem;
#endif

std::mutex mtx;  // Mutex for thread-safe access
std::map<int, std::string> results;  // Stores OCR results in order
int processedFrames = 0;  // Counter for processed frames
int totalFrames;  // Total number of frames
int maxThreads;   // Max number of threads to use

std::vector<cv::Point> points;    // To store the two points (start and end)
bool drawing = false;    // Flag to check if we're currently drawing the rectangle
cv::Mat originalFrame;       // To reset the frame for redrawing

// Mouse callback function
void mouseCallback(int event, int x, int y, int, void* param) {
    cv::Mat* img = (cv::Mat*)param;

    if (event == cv::EVENT_LBUTTONDOWN) {
        points.push_back(cv::Point(x, y));
        drawing = true;  // Start drawing after the first click
    }
    else if (event == cv::EVENT_MOUSEMOVE && drawing && points.size() == 1) {
        // Draw dynamic rectangle as the mouse moves
        cv::Mat temp = originalFrame.clone();
        rectangle(temp, points[0], cv::Point(x, y), cv::Scalar(0, 255, 0), 2);  // Green rectangle
        imshow("Video Frame", temp);
    }
    else if (event == cv::EVENT_LBUTTONUP && points.size() == 1) {
        // Finalize the rectangle after the second click
        points.push_back(cv::Point(x, y));
        rectangle(*img, points[0], points[1], cv::Scalar(0, 0, 255), 2);  // Red rectangle
        imshow("Video Frame", *img);
        drawing = false;
    }
}

// Function to mute Tesseract debug logs
void muteTesseractOutput(int &old_stdout, int &old_stderr) {
#if defined(_WIN32) || defined(_WIN64)
    old_stdout = _dup(_fileno(stdout));
    old_stderr = _dup(_fileno(stderr));
    int nullfd = _open(NULL_DEVICE, _O_WRONLY);
    _dup2(nullfd, _fileno(stdout));
    _dup2(nullfd, _fileno(stderr));
    _close(nullfd);
#else
    old_stdout = dup(STDOUT_FILENO);
    old_stderr = dup(STDERR_FILENO);
    int nullfd = open(NULL_DEVICE, O_WRONLY);
    dup2(nullfd, STDOUT_FILENO);
    dup2(nullfd, STDERR_FILENO);
    close(nullfd);
#endif
}

// Function to restore standard output
void restoreTesseractOutput(int old_stdout, int old_stderr) {
#if defined(_WIN32) || defined(_WIN64)
    _dup2(old_stdout, _fileno(stdout));
    _dup2(old_stderr, _fileno(stderr));
    _close(old_stdout);
    _close(old_stderr);
#else
    dup2(old_stdout, STDOUT_FILENO);
    dup2(old_stderr, STDERR_FILENO);
    close(old_stdout);
    close(old_stderr);
#endif
}

// Function to generate the output file name based on input video
std::string generateOutputFilename(const std::string &inputFile) {
    fs::path inputPath(inputFile);
    std::string filenameWithoutExt = inputPath.stem().string();  // Remove extension
    //return filenameWithoutExt + "_combat_log_raw.txt";
    return "combat_log_raw.txt";
}

// Function to print a live progress bar
void printProgress() {
    int barWidth = 50;
    float progress = (float)processedFrames / totalFrames;
    int pos = barWidth * progress;

    std::cout << "\r\033[1;32m[";  // Green color start
    for (int i = 0; i < barWidth; i++) {
        if (i < pos) std::cout << "=";
        else if (i == pos) std::cout << ">";
        else std::cout << " ";
    }
    std::cout << "] " << int(progress * 100.0) << "%  (" 
              << processedFrames << "/" << totalFrames << " frames)\033[0m" << std::flush;
}

// **Function to compare input string to the template "(Combat)"**
double compareToTemplate(const std::string& input, const std::string& templateStr = "(Combat):") {
    std::vector<int> indexDistances;  // Store index distances for matched characters

    for (size_t tIndex = 0; tIndex < templateStr.size(); tIndex++) {
        char tChar = templateStr[tIndex];

        // **Find the first occurrence of the template character in the input string**
        size_t inputIndex = input.find(tChar);

        if (inputIndex != std::string::npos) {
            int distance = static_cast<int>(inputIndex) - static_cast<int>(tIndex);
            indexDistances.push_back(distance);
        }
    }

    // **Count occurrences of each distance value**
    std::sort(indexDistances.begin(), indexDistances.end());

    int maxCount = 0, currentCount = 1;
    for (size_t i = 1; i < indexDistances.size(); i++) {
        if (indexDistances[i] == indexDistances[i - 1]) {
            currentCount++;
        } else {
            maxCount = std::max(maxCount, currentCount);
            currentCount = 1;
        }
    }
    maxCount = std::max(maxCount, currentCount);  // Final max update

    // **Return similarity score**
    return static_cast<double>(maxCount) / templateStr.size();
}

// Function to process a single frame (Threaded)
void processFrame(int frameNumber, double timestamp, cv::Mat frame) {
    thread_local tesseract::TessBaseAPI ocr;
    int old_stdout, old_stderr;
    
    // Mute Tesseract output
    //muteTesseractOutput(old_stdout, old_stderr);

    if (ocr.Init(NULL, "eng")) {
        restoreTesseractOutput(old_stdout, old_stderr);
        std::cerr << "Error: Could not initialize Tesseract!" << std::endl;
        return;
    }

    // Use PSM_SINGLE_BLOCK for faster performance if layout is simple
    ocr.SetPageSegMode(tesseract::PSM_SINGLE_BLOCK); 
    //ocr.SetPageSegMode(tesseract::PSM_AUTO);

    // Convert to grayscale & preprocess
    cv::cvtColor(frame, frame, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(frame, frame, cv::Size(3, 3), 0);
    cv::threshold(frame, frame, 50, 255, cv::THRESH_BINARY);// | cv::THRESH_OTSU);

    // Convert OpenCV Mat to Pix for Tesseract
    Pix *pixImage = pixCreate(frame.cols, frame.rows, 8);
    for (int y = 0; y < frame.rows; y++) {
        for (int x = 0; x < frame.cols; x++) {
            pixSetPixel(pixImage, x, y, frame.at<uchar>(y, x));
        }
    }

    // Run OCR
    ocr.SetImage(pixImage);
    char *text = ocr.GetUTF8Text();

    // Restore output immediately after OCR
    //restoreTesseractOutput(old_stdout, old_stderr);

    // Filter text: Only keep lines that start with "(Combat)"
    std::string ocrText(text);
    std::istringstream textStream(ocrText);
    std::string line;
    std::ostringstream filteredText;
    while (std::getline(textStream, line)) {
        if (line.find("(Combat):") == 0) {
            filteredText << line << "\n";
        }
        else if (compareToTemplate(line) >= 0.5) {
            filteredText << line << "\n";
        }
    }

    // Store results
    if (!filteredText.str().empty()) {
        std::ostringstream oss;
        oss << "Frame: " << frameNumber << " | Time: " << timestamp << "s\n"
            << filteredText.str() << "---------------------------------\n";
        
        std::lock_guard<std::mutex> lock(mtx);
        results[frameNumber] = oss.str();
    }

    // Update progress
    /*{
        std::lock_guard<std::mutex> lock(mtx);
        processedFrames+=1;
    }*/

    delete[] text;
    pixDestroy(&pixImage);
    ocr.Clear();
}

int main(int argc, char* argv[]) {
    int old_stdout, old_stderr;
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <video_file> [max_threads]" << std::endl;
        return -1;
    }

    std::string videoFile = argv[1];
    cv::VideoCapture cap(videoFile);
    if (!cap.isOpened()) {
        std::cerr << "Error: Could not open video file!" << std::endl;
        return -1;
    }



    /*VideoCapture cap("OCRtest4-2025-02-07_09.41.51.mkv");  // Replace with your video file
    if (!cap.isOpened()) {
        cerr << "❌ Error opening video file!" << endl;
        return -1;
    }*/

    cv::Mat frame;
    cap >> frame;  // Read the first frame
    if (frame.empty()) {
        std::cerr << "❌ Error reading the first frame!" << std::endl;
        return -1;
    }
    originalFrame = frame.clone();  // Keep the original frame for redrawing

    cv::namedWindow("Video Frame", cv::WINDOW_AUTOSIZE);
    setMouseCallback("Video Frame", mouseCallback, &frame);

    imshow("Video Frame", frame);
    std::cout << "🖱️ Click two points to define a rectangle." << std::endl;

    cv::waitKey(0);  // Wait for a key press before exiting

    if (points.size() == 2) {
        std::cout << "✅ Rectangle defined by points: (" << points[0].x << ", " << points[0].y
             << ") and (" << points[1].x << ", " << points[1].y << ")" << std::endl;
    } else {
        std::cout << "⚠️ Rectangle was not completed." << std::endl;
    }

    cv::destroyAllWindows();

    cv::Rect roi(std::min(points[0].x,points[1].x), std::min(points[0].y,points[1].y), std::max(points[0].x,points[1].x)-std::min(points[0].x,points[1].x), std::max(points[0].y,points[1].y)-std::min(points[0].y,points[1].y));


    // Set max threads (default: all cores)
    maxThreads = (argc >= 3) ? std::stoi(argv[2]) : std::thread::hardware_concurrency();

    // Get FPS & total frames
    double fps = cap.get(cv::CAP_PROP_FPS);
    totalFrames = (int) cap.get(cv::CAP_PROP_FRAME_COUNT);
    std::cout << "Processing video at " << fps << " FPS | Total Frames: " << totalFrames 
              << " | Using " << maxThreads << " threads\n";

    const int batchSize = maxThreads;  // Process frames in batches of maxThreads
    int frameNumber = 0;
    std::vector<std::thread> threads;
    cv::Mat baseFrame;

    cap.read(frame);
    if (roi.x + roi.width <= frame.cols && roi.y + roi.height <= frame.rows) {
        // Crop the frame
        baseFrame = frame(roi).clone();
    }
    double timestamp = frameNumber / fps;

    
    //processFrame(frameNumber, timestamp, baseFrame.clone());
    threads.emplace_back(processFrame, frameNumber, timestamp, baseFrame.clone());
    frameNumber++;
    
    
    while (frameNumber < totalFrames and cap.read(frame)) {
                
        int skippedframes = 0;
        // Mute Tesseract output
        muteTesseractOutput(old_stdout, old_stderr);

        //Fix:
        double timestamp = frameNumber / fps;
        //threads.emplace_back(processFrame, frameNumber, timestamp, baseFrame.clone());
        //frameNumber++;
        
        //if (!cap.read(frame)) break;
        // Convert images to grayscale for easier comparison
        cv::Mat gray1, gray2;
        cv::cvtColor(baseFrame.clone(), gray1, cv::COLOR_BGR2GRAY);
        cv::cvtColor(frame(roi).clone(), gray2, cv::COLOR_BGR2GRAY);

        // Subtract the images
        cv::Mat diff;
        cv::absdiff(gray1, gray2, diff);  // Absolute difference

        // Threshold the difference to highlight changes
        cv::Mat thresh;
        cv::threshold(diff, thresh, 30, 255, cv::THRESH_BINARY);// | cv::THRESH_OTSU);  // 30 is the threshold for change sensitivity
        

        // Count the number of non-zero (changed) pixels
        double changedPixelspercent = (double)cv::countNonZero(thresh) / ( (double)( std::max(points[0].x,points[1].x)-std::min(points[0].x,points[1].x)) * (std::max(points[0].y,points[1].y)-std::min(points[0].y,points[1].y))  );
        //double changedPixelspercent = 1;
        
        if (changedPixelspercent>=0.01){            
            double timestamp = frameNumber / fps;
            //processFrame(frameNumber, timestamp, frame(roi).clone());
            threads.emplace_back(processFrame, frameNumber, timestamp, frame(roi).clone());
            baseFrame = frame(roi).clone();
        }
        else {
            //std::lock_guard<std::mutex> lock(mtx);
            skippedframes+=1;
        }

        frameNumber+=1;

        // Read frames and launch threads
        int i = 0;
        while ( i < batchSize-1 && frameNumber < totalFrames ) {
            /*for (int j=0; j<1; j++){            
                if (!cap.read(frame)) break;
                frameNumber+=1;
                processedFrames+=1;
            }*/
            if (!cap.read(frame)) break;
            // Convert images to grayscale for easier comparison
            cv::Mat gray1, gray2;
            cv::cvtColor(baseFrame.clone(), gray1, cv::COLOR_BGR2GRAY);
            cv::cvtColor(frame(roi).clone(), gray2, cv::COLOR_BGR2GRAY);

            // Subtract the images
            cv::Mat diff;
            cv::absdiff(gray1, gray2, diff);  // Absolute difference

            // Threshold the difference to highlight changes
            cv::Mat thresh;
            cv::threshold(diff, thresh, 30, 255, cv::THRESH_BINARY);// | cv::THRESH_OTSU);  // 30 is the threshold for change sensitivity
            

            // Count the number of non-zero (changed) pixels
            double changedPixelspercent = (double)cv::countNonZero(thresh) / ( (double)( std::max(points[0].x,points[1].x)-std::min(points[0].x,points[1].x)) * (std::max(points[0].y,points[1].y)-std::min(points[0].y,points[1].y))  );
            //double changedPixelspercent = 1;
            
            if (changedPixelspercent>=0.01){            
                double timestamp = frameNumber / fps;
                baseFrame = frame(roi).clone();
                //processFrame(frameNumber, timestamp, frame(roi).clone());
                threads.emplace_back(processFrame, frameNumber, timestamp, frame(roi).clone());
                i+=1;
            }
            /*else {
                processedFrames+=1;
            }*/
            frameNumber+=1;
        }

        // Join all threads in the batch
        for (auto &t : threads) {
            t.join();
        }
        threads.clear();

        std::lock_guard<std::mutex> lock(mtx);
        processedFrames=frameNumber;

        // Restore output immediately after OCR
        restoreTesseractOutput(old_stdout, old_stderr);

        printProgress();
    }

    std::cout << std::endl;

    std::ofstream outputFile(generateOutputFilename(videoFile));
    for (const auto& entry : results) {
        outputFile << entry.second;
    }
    outputFile.close();

    cap.release();
    std::cout << "\n\033[1;34mOCR processing complete.\033[0m" << std::endl;
    return 0;
}
