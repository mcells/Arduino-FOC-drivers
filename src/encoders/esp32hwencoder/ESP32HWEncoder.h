
#pragma once

#include <Arduino.h>


#if defined(ESP_H) && defined(ARDUINO_ARCH_ESP32)

#include "driver/pcnt.h"
#include "soc/pcnt_struct.h"
#include "common/base_classes/Sensor.h"
#include "common/foc_utils.h"
#include "FunctionalInterrupt.h"

typedef struct overflowISR_args_t {
    int32_t* angleoverflow_val;
    pcnt_unit_t unit;
}overflowISR_args;

class ESP32HWEncoder : public Sensor{
    public:
        /**
        Encoder class constructor
        @param ppr  impulses per rotation  (cpr=ppr*4)
        */
        explicit ESP32HWEncoder(uint8_t pinA, uint8_t pinB, uint32_t ppr, int8_t pinI=-1);

        void init() override;
        int needsSearch() override;
        int hasIndex();

        bool initialized = false;
        uint32_t cpr; // Counts per rotation = 4 * ppr for quadrature encoders

        Pullup pullup; //!< Configuration parameter internal or external pullups

    protected:
        float getSensorAngle() override;

        void IRAM_ATTR indexHandler();
        
        bool indexFound = false;

        uint8_t _pinA, _pinB;
        int8_t _pinI;
        
        pcnt_config_t pcnt_config;

        int16_t angleCounter;
        int32_t angleOverflow;

};

#endif