#include "AmodeConfig.h"

#include <fstream>
#include <set>
#include <sstream>
#include <iostream>

#include "qdebug.h"
#include "qlogging.h"
#include "ultrasoundconfig.h"

AmodeConfig::AmodeConfig(const std::string& filepath, const std::string& filedir_window) {
    // get the filename and the file dir
    std::filesystem::path path(filepath);
    filename_ = path.stem().string();
    filedir_  = path.parent_path().string();

    // initialize the name for the window config file
    // i put it in the constructor because i want to make the name once for the
    // entire session of using the software.
    // if i initialize it in the function
    // where i export the config file, it will create new file everytime i call
    // the function. so better i initialize the name here
    filepath_window_ = filedir_window + "/" + filename_+ "_window"+ getCurrentDateTime() +".csv";

    // load the Amode configuration data
    loadData(filepath);

    // copy some fields from dataMap to dataWindow for initialization
    for (const auto& entry : dataMap)
    {
        AmodeConfig::Window window;
        window.number       = entry.second.number;
        window.group        = entry.second.group;
        window.groupname    = entry.second.groupname;
        window.isset        = 0;
        window.lowerbound   = 0.0;
        window.middle       = 0.0;
        window.upperbound   = 0.0;

        // Insert the data into the map.
        dataWindow[window.number] = window;
    }
}

void AmodeConfig::loadData(const std::string& filepath) {
    std::ifstream file(filepath);
    std::string line;
    std::getline(file, line); // Skip the header row.

    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string cell;
        std::vector<std::string> tmp_data;
        AmodeConfig::Data data;

        // Split the line by ';' and store in tmp_data.
        while (std::getline(ss, cell, ';')) {
            tmp_data.push_back(cell);
        }

        // Store the data properly to our Data
        data.number = std::stoi(tmp_data[0]);
        data.group  = std::stoi(tmp_data[1]);
        data.groupname = tmp_data[2];
        data.local_R   = {std::stod(tmp_data[3]), std::stod(tmp_data[4]),std::stod(tmp_data[5])};
        data.local_t   = {std::stod(tmp_data[6]), std::stod(tmp_data[7]),std::stod(tmp_data[8])};

        // Insert the data into the map.
        dataMap[data.number] = data;
    }
}

bool AmodeConfig::exportWindow(const std::string& newfilepath_window)
{
    // Determine the file path to use, if the user don't specify the path, it will use filepath_window_ (initialized in the constructor)
    // else, use the newfilepath_window
    std::string export_path = newfilepath_window.empty() ? filepath_window_ : newfilepath_window;

    // Create new file at the determined path
    std::ofstream file(export_path, std::ios::out);

    if (!file.is_open()) {
        qDebug() << "AmodeConfig::exportWindow() Error: Unable to open file " << filepath_window_;
        return false;
    }

    // Write CSV header
    file << "Number,Group,GroupName,IsSet,LowerBound,Middle,UpperBound\n";

    // Write data
    for (const auto& pair : dataWindow) {
        const Window& window = pair.second;
        file << window.number << ","
             << window.group << ","
             << window.groupname << ","
             << window.isset << ","
             << window.lowerbound << ","
             << window.middle << ","
             << window.upperbound << "\n";

        if (file.fail()) {
            qDebug() << "AmodeConfig::exportWindow() Error: Failed to write data to file";
            file.close();
            return false;
        }
    }

    file.close();

    if (file.fail()) {
        qDebug() << "AmodeConfig::exportWindow() Error: Failed to close file properly";
        return false;
    }

    qDebug() << "AmodeConfig::exportWindow() Data exported successfully to " << filepath_window_;
    return true;
}

AmodeConfig::Data AmodeConfig::getDataByNumber(int number) const {
    auto it = dataMap.find(number);
    if (it != dataMap.end()) {
        return it->second;
    } else {
        throw std::runtime_error("Data not found for the given number.");
    }
}

// Function to get all Data with the specified group name.
std::vector<AmodeConfig::Data> AmodeConfig::getDataByGroupName(const std::string& groupname) const {
    // prepare the variable to store the result
    std::vector<Data> result;
    // loop for all entry in the dataMap
    for (const auto& entry : dataMap)
    {
        // if we find an entry with a groupname we are searching for
        if (entry.second.groupname == groupname)
        {
            // get that entry and push to our result
            result.push_back(entry.second);
        }
    }
    return result;
}

// Function to get all unique group names from dataMap.
std::vector<std::string> AmodeConfig::getAllGroupNames() const {
    std::set<std::string> groupNamesSet;
    for (const auto& entry : dataMap) {
        if (entry.second.groupname!="") groupNamesSet.insert(entry.second.groupname);
    }
    // Convert the set to a vector to return.
    return std::vector<std::string>(groupNamesSet.begin(), groupNamesSet.end());
}

void AmodeConfig::setWindowByNumber(int number, std::array<std::optional<double>, 3> window)
{
    // Check if the key exists in the map
    if (dataWindow.find(number) != dataWindow.end())
    {
        // If the middle of the window does not have any value, it means the user never
        // click (set the window) for that particular plot. Well, it should be like that,
        // but apparently, you still can click outside the axis but still in the plot.
        // However, that just does not make sense. Okay, if this is true, set everything to 0
        if (!window[1].has_value())
        {
            dataWindow[number].isset      = 0;
            dataWindow[number].middle     = 0.0;
            dataWindow[number].lowerbound = 0.0;
            dataWindow[number].upperbound = 0.0;
        }

        // Lowerbound and upperbound, on the other hand, can has no value even though middle
        // does not have any value, it is when the window is outside the plot.
        // If lowerbound has no value, let's set the minimum x value of the plot, 0.0
        // If upperbound has no value, let's put the maximum x value of the plot, n_sample * d_s
        else
        {
            dataWindow[number].isset      = 1;
            dataWindow[number].middle     = window[1].value();
            dataWindow[number].lowerbound = window[0].has_value() ? window[0].value() : 0.0;
            dataWindow[number].upperbound = window[2].has_value() ? window[2].value() : UltrasoundConfig::N_SAMPLE*UltrasoundConfig::DS;
        }
    }
    else
    {
        throw std::runtime_error("Data not found for the given number.");
    }
}

AmodeConfig::Window AmodeConfig::getWindowByNumber(int number)
{
    auto it = dataWindow.find(number);
    if (it != dataWindow.end()) {
        // Please notice that the window is not in a type std::array<std::optional<double>, 3>
        // since this class will handle exporting data to csv, we only can write or read with double
        // so this function will return a Window object with a double lowerbound,middle,upperbound
        return it->second;
    } else {
        throw std::runtime_error("Data not found for the given number.");
    }
}

// Function to get the current date and time as a formatted string
std::string AmodeConfig::getCurrentDateTime()
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm = *std::localtime(&time_t);

    // Format the date and time as a string (e.g., "2024-04-22_16-18-42")
    char buffer[20];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d_%H-%M-%S", &local_tm);
    return std::string(buffer);
}
