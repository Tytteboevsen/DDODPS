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
#include <string>

int main() {
    std::string filename = "combat_log_cleaned.txt";  // Replace with your file name
    std::ifstream file(filename);

    if (!file.is_open()) {
        std::cerr << "❌ Error: Could not open file " << filename << std::endl;
        return 1;
    }

    std::string line;
    long long totalDamage = 0;  // To handle large sums

    while (std::getline(file, line)) {
        // Check if the line contains "Type: hit"
        if (line.find("Type: hit") != std::string::npos) {
            std::istringstream iss(line);
            std::string token;
            int damage = 0;

            // Extract the damage value (5th field)
            int fieldCounter = 0;
            while (std::getline(iss, token, '|')) {
                fieldCounter++;
                if (fieldCounter == 5) {  // 5th field contains the damage
                    try {
                        damage = std::stoi(token);
                        totalDamage += damage;
                    } catch (...) {
                        std::cerr << "⚠️ Warning: Invalid number in line: " << line << std::endl;
                    }
                    break;
                }
            }
        }
    }

    file.close();

    // Print the total damage
    std::cout << "🔥 Total Damage from Hits: " << totalDamage << std::endl;

    return 0;
}

