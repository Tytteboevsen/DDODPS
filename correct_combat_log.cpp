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



#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>
#include <vector>
#include <string>
#include <algorithm>
#include <unordered_map>
#include <string>

// Struct to store combat entries
struct CombatEntry {
    int frame;
    double time;
    std::string text;
};

// Helper function to remove commas from numbers
std::string removeCommas(const std::string& str) {
    std::string result;
    for (char c : str) {
        if (c != ',') result += c;
    }
    return result;
}

// Levenshtein Distance for fuzzy matching
int levenshteinDistance(const std::string& s1, const std::string& s2) {
    int m = s1.size(), n = s2.size();
    std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1));

    for (int i = 0; i <= m; i++) dp[i][0] = i;
    for (int j = 0; j <= n; j++) dp[0][j] = j;

    for (int i = 1; i <= m; i++) {
        for (int j = 1; j <= n; j++) {
            if (s1[i - 1] == s2[j - 1])
                dp[i][j] = dp[i - 1][j - 1];
            else
                dp[i][j] = 1 + std::min({ dp[i - 1][j], dp[i][j - 1], dp[i - 1][j - 1] });
        }
    }
    return dp[m][n];
}

// Function to compare two strings and find the max number of matches at an index offset
std::string correctline(const std::string &str1, const std::string &str2) {
    const std::string &shorter = (str1.size() < str2.size()) ? str1 : str2;
    const std::string &longer = (str1.size() >= str2.size()) ? str1 : str2;

    double complength = shorter.size();
    double maxcounter = 0;

    int indexMatches[1000];  // Map to store offset index and its match count

    int savedj = 0;

    std::string outputstring = longer;



    for (size_t i = 0; i < 1000; i++) {
        indexMatches[i] = 0;
    }

    // Compare each character in the shorter string to all characters in the longer string
    for (size_t i = 0; i < shorter.size(); i++) {
        char ch = shorter[i];
        for (size_t j = i; j < longer.size(); j++) {
            if (ch == longer[j]) {
                int indexOffset = j - i;  // Calculate the offset
                indexMatches[indexOffset]++;  // Count the match for this offset
            }
        }
    }
    
    for (size_t j = 0; j < longer.size(); j++) {
        if (indexMatches[j]>maxcounter) {
            maxcounter = indexMatches[j];
            savedj = j;
           
        }
    }

    if ((double) (maxcounter / complength) >= 0.0) {
        for (size_t i=0; i<complength; i++){
            if (savedj+i<longer.size()){
                outputstring[savedj+i] = shorter[i];        
            }
        }
        
    }

    return outputstring;
}


// Function to compare two strings and find the max number of matches at an index offset
double findMaxMatchingIndex(const std::string &str1, const std::string &str2) {
    const std::string &shorter = (str1.size() < str2.size()) ? str1 : str2;
    const std::string &longer = (str1.size() >= str2.size()) ? str1 : str2;

    double complength = shorter.size();
    double maxcounter = 0;

    int indexMatches[1000];  // Map to store offset index and its match count

    int savedj = 0;

    std::string outputstring = longer;



    for (size_t i = 0; i < 1000; i++) {
        indexMatches[i] = 0;
    }

    // Compare each character in the shorter string to all characters in the longer string
    for (size_t i = 0; i < shorter.size(); i++) {
        char ch = shorter[i];
        for (size_t j = 0; j < longer.size(); j++) {
            if (ch == longer[j]) {
                int indexOffset = j - i;  // Calculate the offset
                indexMatches[500+indexOffset]++;  // Count the match for this offset
            }
        }
    }

    for (size_t j = 0; j < 1000; j++) {
        if (indexMatches[j]>maxcounter) {
            maxcounter = indexMatches[j];
            savedj = j;
           
        }
    }



    return maxcounter/complength;
}
    

// Function to correct and format combat lines
CombatEntry correctCombatLine(const std::string& line, int frame, double time) {
    std::regex hitRegex(R"(\(Combat\): You hit (.+?) for ([\d,]+) points of (.+?) damage\.*)");
    std::regex missRegex(R"(\(Combat\): You attack (.+?)\. You roll a (\d+); you miss!*)");
    std::regex hitAttackRegex(R"(\(Combat\): You attack (.+?)\. You roll a (\d+) \(\+(\d+)\): you hit!*)");
    std::regex hitCritRegex(R"(\(Combat\): You attack (.+?)\. You roll a (\d+) \(\+(\d+)\): you critical hit!*)");

    const std::string hitstrings[4]={"(Combat): You hit ", " for ", " points of ", " damage."};
    const std::string missstrings[3]={"(Combat): You attack ", ". You roll a ", "; you miss!"};
    const std::string attackstrings[4]={"(Combat): You attack ", ". You roll a ", "(+", "): you hit!"};
    const std::string critstrings[4]={"(Combat): You attack ", ". You roll a ", "(+", "): you critical hit!"};

    std::smatch match;
    CombatEntry entry{ frame, time, "" };

    double matchscore = 0;

    std::string correctedhitstr, correctedmissstr, correctedattackstr, correctedcritstr ;  

    correctedhitstr=line;
    //correctedhitstr = std::string(correctedhitstr.size(), '\0');
    if (findMaxMatchingIndex(hitstrings[0], line)>0.8 && findMaxMatchingIndex(hitstrings[3], line) > 0.8){
        for (int i=0; i<4; i++){
            correctedhitstr=correctline(hitstrings[i], correctedhitstr);
            //correctedhitstr=correctline(hitstrings[0], line) + correctline(hitstrings[1], line) + correctline(hitstrings[2], line) + correctline(hitstrings[3], line);
        }
    }
    
    /*if (findMaxMatchingIndex(missstrings[2], line) > 0.8){    
        for (int i=0; i<3; i++){
            correctedmissstr+=correctline(missstrings[i], line);
        }
    }*/

    /*if (findMaxMatchingIndex(attackstrings[3], line) > 0.8){   
        for (int i=0; i<4; i++){
            correctedattackstr+=correctline(attackstrings[i], line);     
        }
    }*/
    
    /*if (findMaxMatchingIndex(critstrings[3], line) > 0.8){   
        for (int i=0; i<4; i++){
            correctedcritstr+=correctline(critstrings[i], line);     
        }
    }*/

//    if (std::regex_search(line, match, hitRegex)) {
    /*if (std::regex_search(correctedcritstr, match, hitCritRegex)) {
        entry.text = "(Combat): You attack " + match[1].str() +
                     ". You roll a " + match[2].str() +
                     " (+" + match[3].str() + "): you critical hit!";
    }
    else if (std::regex_search(correctedattackstr, match, hitAttackRegex)) {
        entry.text = "(Combat): You attack " + match[1].str() +
                     ". You roll a " + match[2].str() +
                     " (+" + match[3].str() + "): you hit!";
    }  
    else if (std::regex_search(correctedmissstr, match, missRegex)) {
        entry.text = "(Combat): You attack " + match[1].str() +
                     ". You roll a " + match[2].str() + "; you miss!";
    }*/   
    if (std::regex_search(correctedhitstr, match, hitRegex)) {
    //if (true) {
        //entry.text = correctedhitstr;      
        entry.text = "(Combat): You hit " + match[1].str() + " for " +
                     removeCommas(match[2].str()) + " points of " +
                     match[3].str() + " damage.";
    } 
    /*else {
        entry.text = "";  // Not recognized
    }*/

    return entry;
}

int main() {
    std::ifstream inputFile("combat_log_raw.txt");
    std::ofstream outputFile("corrected_data.txt");

    if (!inputFile.is_open() || !outputFile.is_open()) {
        std::cerr << "❌ Error opening files!" << std::endl;
        return 1;
    }

    std::string line;
    int currentFrame = -1;
    double currentTime = 0.0;
    std::vector<CombatEntry> entries;

    while (std::getline(inputFile, line)) {
        if (line.find("Frame:") != std::string::npos) {
            // Extract frame and time
            std::regex frameRegex(R"(Frame:\s*(\d+)\s*\|\s*Time:\s*([\d.]+)s)");
            std::smatch frameMatch;
            if (std::regex_search(line, frameMatch, frameRegex)) {
                currentFrame = std::stoi(frameMatch[1]);
                currentTime = std::stod(frameMatch[2]);
            }
        } 
        else if (line.find("(Combat):") != std::string::npos) {
            CombatEntry corrected = correctCombatLine(line, currentFrame, currentTime);
            if (!corrected.text.empty()) {
                entries.push_back(corrected);
            }
        }
    }

    // Save corrected data
    int lastFrame = -1;
    for (const auto& entry : entries) {
        if (entry.frame != lastFrame) {
            outputFile << "---------------------------------\n";
            outputFile << "Frame: " << entry.frame << " | Time: " << entry.time << "s\n";
            lastFrame = entry.frame;
        }
        outputFile << entry.text << "\n";
    }

    std::cout << "✅ Data correction completed!" << std::endl;

    inputFile.close();
    outputFile.close();
    return 0;
}

