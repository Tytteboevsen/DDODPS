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
#include <vector>
#include <string>
#include <regex>

// Structure to store a combat log entry
struct CombatEntry {
    std::string type;  // "attack" or "hit"
    std::vector<std::string> variables; // Extracted wildcard values
    double timestamp;
    int frameNumber;
};

// Function to remove commas from numbers
std::string removeCommas(const std::string &numStr) {
    std::string cleanStr;
    for (char c : numStr) {
        if (c != ',') cleanStr += c;
    }
    return cleanStr;
}

// Function to check if two numbers are "almost equal" (fixes OCR mistakes like `6 ↔ 0`)
bool areNumbersSimilar(const std::string &num1, const std::string &num2) {
    if (num1 == num2) return true;  // Exact match

    if (num1.size() != num2.size()) return false;  // Different length → not similar

    int diffCount = 0;
    for (size_t i = 0; i < num1.size(); i++) {
        if (num1[i] != num2[i]) {
            if (!((num1[i] == '6' && num2[i] == '0') || (num1[i] == '0' && num2[i] == '6'))) {
                diffCount++;//return false;  // If mismatch is not a `6 ↔ 0` mistake, reject
            }
        }
    }

    if ((double)diffCount / num1.size() <= 0.25){
        return true;    
    }
    else {
        return false;
    }
    //return diffCount <= 0;  // Allow only 1 character difference (`6 ↔ 0`)
}

// Function to extract wildcard data from attack or hit lines
CombatEntry extractCombatData(const std::string &line, double timestamp, int frameNumber) {
    std::regex attackRegex(R"(\(Combat\): You attack (.+?)\. You roll a (\d+) \(\+(\d+)\): you (.+?)!*)");
    std::regex hitRegex(R"(\(Combat\): You hit (.+?) for ([\d,]+) points of (.+?) damage\..*)");

    std::smatch match;
    CombatEntry entry;
    entry.timestamp = timestamp;
    entry.frameNumber = frameNumber;

    if (std::regex_match(line, match, attackRegex)) {
        entry.type = "attack";
        entry.variables = { match[1], match[2], match[3], match[4] }; // Extracted wildcards
    } 
    else if (std::regex_match(line, match, hitRegex)) {
        entry.type = "hit";
        entry.variables = { match[1], removeCommas(match[2]), match[3] }; // Remove commas from numbers
    } 
    else {
        entry.type = "unknown"; // Entry didn't match
    }

    return entry;
}

// Function to compare hit entries based only on wildcards
bool areHitEntriesSimilar(const CombatEntry &entry1, const CombatEntry &entry2, double threshold = 1) {
    if (entry1.type != "hit" || entry2.type != "hit") return false;  // Ensure both are hit entries
    //if (entry1.variables.size() != entry2.variables.size()) return false; // Ensure same structure

    int matchedParts = 0;
    //int relevantParts = 1;//entry1.variables.size(); // Wildcard parts to compare

    //for (size_t i = 1; i < relevantParts+2; i++) {
    //    if (i == 1) { // Index 1 is always the damage number
            if (areNumbersSimilar(entry1.variables[1], entry2.variables[1])) {
                matchedParts++;
            }
        /*} else {  // Regular text wildcard comparison
            if (entry1.variables[i] == entry2.variables[i]) {
                matchedParts++;
            }
        }
    }*/

    //double similarity = (double)matchedParts;// / relevantParts;  // Compute match ratio
    return matchedParts;//similarity >= threshold; // Require 80% match
}



// **Function to check if two entries (hit or attack) are similar**
bool areEntriesSimilar(const CombatEntry& e1, const CombatEntry& e2) {
    if (e1.type != e2.type) return false;  // Compare only same types
    return areHitEntriesSimilar(e1, e2);   // Use hit similarity for both (adjust if needed)
}

// **Merge frames (handles both hits and attacks)**
std::vector<CombatEntry> mergeFrames(std::vector<CombatEntry> frame1, std::vector<CombatEntry> frame2, double threshold) {
    std::vector<CombatEntry> merged = frame1;
    
    int maxLines1 = frame1.size();
    int maxLines2 = frame2.size();
    bool matchfound = false;
    
    for (size_t i=0; i<maxLines2; i++){
        matchfound=false;
        for (size_t j=std::max(0,maxLines1-(int)(1.4*maxLines2) ); j< maxLines1; j++){
            if (areHitEntriesSimilar(frame1[j],frame2[i])>0){
                matchfound=true;            
            }
        } 
        /*if (areHitEntriesSimilar(frame1[0],frame2[0])>0){
            matchfound=true;
        }*/
        if (matchfound==false){
            merged.push_back(frame2[i]);
        }
    }

    return merged;
}





// **🔧 Added: Function to load combat log from file**
std::vector<std::vector<CombatEntry>> loadCombatLog(const std::string &filename) {
    std::vector<std::vector<CombatEntry>> frames;
    std::ifstream file(filename);
    std::string line;
    std::regex frameRegex(R"(Frame:\s+(\d+)\s+\|\s+Time:\s+([\d\.]+)s)");

    double timestamp = 0;
    int frameNumber = -1;
    std::vector<CombatEntry> currentFrame;

    while (std::getline(file, line)) {
        std::smatch match;
        if (std::regex_match(line, match, frameRegex)) {
            if (!currentFrame.empty()) {
                frames.push_back(currentFrame);
                currentFrame.clear();
            }
            frameNumber = std::stoi(match[1]);
            timestamp = std::stod(match[2]);
        } 
        else if (line.rfind("(Combat):", 0) != std::string::npos) {
            CombatEntry entry = extractCombatData(line, timestamp, frameNumber);
            if (entry.type != "unknown") {
                currentFrame.push_back(entry);
            }
        }
    }
    if (!currentFrame.empty()) {
        frames.push_back(currentFrame);
    }
    return frames;
}

// **🔧 Added: Function to save cleaned combat log**
void saveCleanedLog(const std::string &filename, const std::vector<CombatEntry> &entries) {
    std::ofstream file(filename);
    for (const auto &entry : entries) {
        file << "Frame: " << entry.frameNumber << " | Time: " << entry.timestamp << "s | Type: " << entry.type << " | ";
        for (const auto &var : entry.variables) {
            file << var << " | ";
        }
        file << "\n";
    }
}

int main() {
    std::string inputFile = "corrected_data.txt";  
    std::string outputFile = "combat_log_cleaned.txt";

    auto frames = loadCombatLog(inputFile);
    auto mergedEntries = frames[0];

    for (size_t i = 1; i < frames.size(); i++) {
        mergedEntries = mergeFrames(mergedEntries, frames[i], 0.00); //0.225
    }

    saveCleanedLog(outputFile, mergedEntries);
    std::cout << "✅ Cleaning complete! Log saved as: " << outputFile << std::endl;
}

