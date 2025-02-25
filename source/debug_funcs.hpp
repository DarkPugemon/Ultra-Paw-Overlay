/********************************************************************************
 * File: debug_funcs.hpp
 * Authors: ppkantorski, pugemon
 * Description:
 *   This header file contains debugging functions for the Ultra Paw Overlay project.
 *   These functions allow logging messages with timestamps to a log file.
 *
 *   For the latest updates and contributions, visit the project's GitHub repository.
 *   (GitHub Repository: https://github.com/Ultra-NX/Ultra-Paw-Overlay)
 *
 *  Copyright (c) 2023 ppkantorski, pugemon, redraz
 *  All rights reserved.
 ********************************************************************************/

#pragma once
#include <time.h>
#include <cstdio>

// Specify the log file path
const std::string logFilePath = "sdmc:/config/ultra-paw/log.txt";
const int maxLines = 5000;

// This function looks at a .txt file and removes the begging lines until the log is of maxLines size.
void trimLog(FILE* file) {
    if (file != nullptr) {
        // Read the existing lines into a buffer
        fseek(file, 0, SEEK_END);
        long fileSize = ftell(file);
        fseek(file, 0, SEEK_SET);

        char* buffer = nullptr;
        if (fileSize > 0) {
            buffer = new char[fileSize];
            fread(buffer, 1, fileSize, file);
        }

        // Close the file
        fclose(file);

        // Reopen the file for writing
        file = fopen(logFilePath.c_str(), "w");
        if (file != nullptr) {
            // Split the buffer into lines
            char* line = strtok(buffer, "\n");
            bool firstLine = true;
            size_t lineCount = 0;

            while (line != nullptr) {
                if (!firstLine) {
                    fputc('\n', file);
                }

                // Only append if the line count is less than maxLines
                if (lineCount >= (fileSize / 2)) {
                    fputs(line, file);
                }

                line = strtok(nullptr, "\n");
                firstLine = false;
                ++lineCount;
            }

            delete[] buffer;

            // Close the file
            fclose(file);
        }
    }
}

/**
 * @brief Logs a message with a timestamp to a log file.
 *
 * @param message The message to be logged.
 */
void logMessage(const std::string& message) {
    std::time_t currentTime = std::time(nullptr);
    std::string logEntry = std::asctime(std::localtime(&currentTime));
    std::size_t lastNonNewline = logEntry.find_last_not_of("\r\n");
    if (lastNonNewline != std::string::npos) {
        logEntry.erase(lastNonNewline + 1);
    }
    logEntry = "[" + logEntry + "] " + message + "\n";

    FILE* file = fopen(logFilePath.c_str(), "a");
    if (file != nullptr) {
        fputs(logEntry.c_str(), file);
        // trimLog(file);
        fclose(file);
    }
}
