#pragma once
#include <string>

class DockingStation {
public:
    DockingStation();

    void start();  // Entry point
    std::string getIpAddress();

private:
    bool hasStoredCredentials();
    bool connectToWiFi();
    void startBLEProvisioning();
    std::string getDeviceName();
};