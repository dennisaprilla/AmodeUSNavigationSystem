#ifndef AMODECONFIG_H
#define AMODECONFIG_H

#include <map>
#include <optional>
#include <string>
#include <vector>
#include <filesystem>
#include <array>

/**
 * @class AmodeConfig
 * @brief For reading and storing a-mode configuration; also for writing and storing a-mode window.
 *
 * A-mode transducers comes with groups (holder). The group is defined in a configuration file (.csv)
 * At some point you want to work with A-mode transducers but in group (for example visualizing 3d
 * signals in a group at once). This class provides functions to work with groups.
 *
 * At some point, you also want to store a window interval for each A-mode transducer in the same way
 * as your a-mode configuration file. This class also provide function to store the window interval
 * and write it to another (window) configuration file (.csv).
 *
 */

class AmodeConfig {
public:

    /**
     * @struct Data
     * @brief Represents the configuration of A-mode transducers.
     *
     * This struct holds some configuration data corresponds to how A-mode transducer arranged.
     * The data includes the probe's number, probe probe's (index), probe probe's name, probe's
     * local rotation and translation (relative to the holder's origin).
     */
    struct Data {
        int number;
        int group;
        std::string groupname;
        std::vector<double> local_R;
        std::vector<double> local_t;
    };

    /**
     * @struct Data
     * @brief Represents the interval window of A-mode transducers
     *
     * This struct holds some window specification of each A-mode transducer. This struct will be
     * filled with this entire software. I put several same fields as AmodeConfig::Data struct, just
     * for consistency (probe's number, group (index), and group name). Additional fields including
     * isset (boolean that described if this particular probe had been set or not, useful for some
     * part in the program), and the window specification (lower-, middle, and upper-bound).
     */
    struct Window {
        int number;
        int group;
        std::string groupname;
        int isset;
        double lowerbound;
        double middle;
        double upperbound;
    };

    /**
     * @brief Constructor function, it loads the csv file directly by calling loadData() function
     */
    AmodeConfig(const std::string& filepath);

    /**
     * @brief To GET the configuration data with the provided probe number
     */
    Data getDataByNumber(int number) const;

    /**
     * @brief To GET the collection of configuration data with the provided group name
     */
    std::vector<Data> getDataByGroupName(const std::string& groupname) const;

    /**
     * @brief To GET unique group names in the configuration file
     */
    std::vector<std::string> getAllGroupNames() const;

    /**
     * @brief To SET the window configuration data with the provided probe number
     */
    void setWindowByNumber(int number, std::array<std::optional<double>, 3> window);

    /**
     * @brief To GET the window configuration data with the provided probe number
     */
    Window getWindowByNumber(int number);

    /**
     * @brief To export dataWindow to a csv file in the directory provided by filename_
     */
    bool exportWindow();

private:

    /**
     * @brief For loading data, called by constructor
     */
    void loadData(const std::string& filepath);

    /**
     * @brief returns a string of current date and time
     */
    std::string getCurrentDateTime();

    std::string filepath_;              //!< a full path to the file
    std::string filename_;              //!< the name of the amode config file
    std::string filedir_;               //!< the directory to the amode config file
    std::string filepath_window;        //!< a full path to the window config file

    std::map<int, Data> dataMap;         //!< a map variable that maps index to data, used for storing the configuration data
    std::map<int, Window> dataWindow;    //!< a map variable that maps index to data, used for storing the configuration data

};

#endif // AMODECONFIG_H
