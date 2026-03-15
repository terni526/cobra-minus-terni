/*
    * Prototype of auto-indent removal for 0B.6.
    * SAMPLE3.txt is no longer a temporary sample.
*/

#include <iostream>
#include <fstream>

#include <string>

#include <regex>

const char COLON = ':';

bool runTest = true;

int numberOfSamplesToTest = 3;

int main()
{
    for (int sampleNumber = 1; sampleNumber <= numberOfSamplesToTest; sampleNumber++)
    {
        std::cout << "SAMPLE " << std::to_string(sampleNumber) << "\n";

        std::string samplePath = "sample\\SAMPLE" + std::to_string(sampleNumber) + ".txt";
        std::string outputPath = "output\\OUTPUT" + std::to_string(sampleNumber) + ".txt";

        std::regex multipleIndentsPattern(R"( {4,})");
    
        std::ifstream fileToRemoveIndents(samplePath);
        std::ofstream resultOfIndentRemoval(outputPath);

        bool removingIndents = false;

        std::string textToRemoveIndents;

        if (!fileToRemoveIndents || !resultOfIndentRemoval)
        {
            std::cerr << "FAILED TO OPEN FILES.";
        }

        while (std::getline(fileToRemoveIndents, textToRemoveIndents))
        {
            bool isEmpty = textToRemoveIndents.empty();

            bool isColonLine =
                !textToRemoveIndents.empty() &&
                textToRemoveIndents.back() == COLON;

            bool hasIndent =
                textToRemoveIndents.rfind("    ", 0) == 0 ||
                textToRemoveIndents.rfind("  ", 0) == 0;

            bool hasMultipleIndents = std::regex_search(textToRemoveIndents, multipleIndentsPattern);

            if (isColonLine && !hasMultipleIndents)
            {
                removingIndents = true;
                textToRemoveIndents.clear();
            }
            else if (removingIndents && hasIndent)
            {
                textToRemoveIndents.erase(0, 4);
            }
            else if (removingIndents && !hasIndent && !isEmpty)
            {
                removingIndents = false;
            }

            resultOfIndentRemoval << textToRemoveIndents << "\n";
        }

        fileToRemoveIndents.close();
        resultOfIndentRemoval.close();
    }

    return 0;
}
