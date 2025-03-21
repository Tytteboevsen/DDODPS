# DDODPS
These four programs calculates the dps on a recording of Dungeons &amp; Dragons online where the combat log is visable.





The program depends on two open source libraries: Opencv and tesseract.
Opencv is used to load a video file and extract images and tesseract is used to do OCR
on the images to extract the text.

Place the video file in the same directory as the program and run the program from the terminal.
Use the second argument to define the video file. as in:
"./ocr_video OCRtest1-2025-02-03_21.09.35.mkv"
without the "" in linux.

The third argument is optional and is the number of threads to use. 
This will default to all cores if left out.
"./ocr_video OCRtest1-2025-02-03_21.09.35.mkv 8"


It takes four programs to calculate the DPS.
The first one is the most intesive and is the one described above.
    1) ocr_video_mt             (produces "combat_log_raw.txt")
    2) correct_combat_log       (produces "corrected_data.txt")
    3) combat_log_cleaner       (produces "combat_log_cleaned.txt")
    4) sum_hits                 (prints the actual dps to terminal)

Maybe a batch script to automate this can be made...

The first program will open the first frame of the video.
Here you are meant to select a square indicating the region of interest which is the combat log area.
This can be a bit tricky.

Also the resolution of the original video can effect the OCR.
I have done my testing with 1440p resolution on the original video.
Test showed that lower resolutions might require an upscale.



Installing dependencies:
LINUX debian:
sudo apt install libopencv-dev
sudo apt install tesseract-ocr libtesseract-dev
sudo apt install libleptonica-dev
Compile with:
g++ ocr_video_mt.cpp -o ocr_video_mt -std=c++17 -pthread `pkg-config --cflags --libs opencv4 tesseract lept`


Unfortunately i have not been able to compile for windows and have given up on that.
Maybe it is possible to use the Linux version inside windows to run it.
Or perpaps somebody can compile it for windows.
