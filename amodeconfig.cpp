#include "AmodeConfig.h"

#include <fstream>
#include <set>
#include <sstream>
#include <iostream>

AmodeConfig::AmodeConfig(const std::string& filepath) {
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
    filepath_window = filedir_ + filename_+ "_window"+ getCurrentDateTime() +".csv";

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

bool AmodeConfig::exportWindow()
{
    // create new file, in the same directory as the amodeconfig file
    std::ofstream file(filepath_window, std::ios::out);

    if (!file.is_open()) {
        std::cerr << "Error: Unable to open file " << filepath_window << std::endl;
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
            std::cerr << "Error: Failed to write data to file" << std::endl;
            file.close();
            return false;
        }
    }

    file.close();

    if (file.fail()) {
        std::cerr << "Error: Failed to close file properly" << std::endl;
        return false;
    }

    std::cout << "Data exported successfully to " << filepath_window << std::endl;
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
        dataWindow[number].isset = 1;
        dataWindow[number].lowerbound = window[0].has_value() ? window[0].value() : 0.0;
        dataWindow[number].middle     = window[1].has_value() ? window[1].value() : 0.0;
        dataWindow[number].upperbound = window[2].has_value() ? window[2].value() : 0.0;
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
