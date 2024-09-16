#ifndef ULTRASOUNDCONFIG_H
#define ULTRASOUNDCONFIG_H

/**
 * @class UltrasoundConfig
 * @brief A class just to store everything related to ultrasound configuration specification.
 *
 * I should probably call this class as Ultrasound Spesification. But whatever, the damage was done.
 */

class UltrasoundConfig
{
public:

    static const int N_UST    = 30;                                       // [unit]
    static const int N_SAMPLE = 3500;                                     // [unit]
    static const int V_SOUND  = 1540;                                     // [m/s]
    static const int FREQ     = 50000000;                                 // [Hz]
    constexpr static const double DT = 1.0 / FREQ;                        // [s]
    constexpr static const double DS = (1000.0 * V_SOUND) / (2.0 * FREQ); // [mm]
};

#endif // ULTRASOUNDCONFIG_H
