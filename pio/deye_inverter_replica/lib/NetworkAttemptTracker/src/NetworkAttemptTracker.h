#pragma once

#include <stdint.h>

// Shared in-memory connection-attempt totals for one MCU boot.
class NetworkAttemptTracker
{
public:
    void recordWiFiAttempt()
    {
        ++wifiAttemptCount;
        ++totalAttemptCount;
    }

    void recordMqttAttempt()
    {
        ++mqttAttemptCount;
        ++totalAttemptCount;
    }

    uint32_t wifiAttempts() const
    {
        return wifiAttemptCount;
    }

    uint32_t mqttAttempts() const
    {
        return mqttAttemptCount;
    }

    uint32_t totalAttempts() const
    {
        return totalAttemptCount;
    }

private:
    uint32_t wifiAttemptCount = 0;
    uint32_t mqttAttemptCount = 0;
    uint32_t totalAttemptCount = 0;
};
