
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

// Statically allocate and initialize a spinlock
static portMUX_TYPE spinlock;

class ESP32HWEncoder : public Sensor{
    public:
        /**
        Encoder class constructor
        @param ppr  impulses per rotation  (cpr=ppr*4)
        */
        explicit ESP32HWEncoder(int pinA, int pinB, int32_t ppr, int pinI=-1);

        void init() override;
        int needsSearch() override;
        int hasIndex();
        float getSensorAngle() override;
        void setCpr(int32_t ppr);
        int32_t getCpr();
        bool initialized = false;

        

        Pullup pullup; //!< Configuration parameter internal or external pullups



    protected:
        

        void IRAM_ATTR indexHandler();
        
        bool indexFound = false;

        int _pinA, _pinB, _pinI;

        pcnt_config_t pcnt_config;
        overflowISR_args_t overflowISR_args;

        int16_t angleCounter; // Stores the PCNT value
        int32_t angleOverflow; // In case the PCNT peripheral overflows, this receives the max count to keep track of large counts/angles. On index, this gets reset.
        int32_t angleSum; // sum of angle 

        int32_t cpr; // Counts per rotation = 4 * ppr for quadrature encoders
        float inv_cpr;
};

#endif