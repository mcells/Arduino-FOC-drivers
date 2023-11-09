#include "ESP32HWEncoder.h"

#if defined(ESP_H) && defined(ARDUINO_ARCH_ESP32)



ESP32HWEncoder::ESP32HWEncoder(int pinA, int pinB, int32_t ppr, int pinI)
{
    #ifdef USE_ARDUINO_PINOUT
        // Handle Arduino Nano ESP32 quirks with the pin assignments
        _pinA = digitalPinToGPIO(pinA);
        _pinB = digitalPinToGPIO(pinB);
        _pinI = digitalPinToGPIO(pinI);
    #else
        _pinA = pinA;
        _pinB = pinB;
        _pinI = pinI;
    #endif

    cpr = ppr * 4; // 4x for quadrature

    pcnt_config.ctrl_gpio_num =  _pinA;
    pcnt_config.pulse_gpio_num = _pinB;
    pcnt_config.counter_l_lim = INT16_MIN;
    pcnt_config.counter_h_lim = INT16_MAX;
} 

// Interrupt handler for overflowing the pulsecounter count
void IRAM_ATTR overflowCounter(void* arg)
{
    int32_t* count = (*(overflowISR_args_t*) arg).angleoverflow_val;
    pcnt_unit_t unit = (*(overflowISR_args_t*) arg).unit;

    // Add or subtract depending on the direction of the overflow
    switch (PCNT.status_unit[unit].val)
    {
    case PCNT_EVT_L_LIM:
        *count += INT16_MIN/2; // This _should_ be just INT16_MIN, but it has to be halved for some reason
        break;
    case PCNT_EVT_H_LIM:
        *count += INT16_MAX/2;
        break;
    default:
        break;
    }

    // Clear the interrupt
    PCNT.int_clr.val = BIT(unit);
}

// Interrupt handler for zeroing the pulsecounter count
void IRAM_ATTR ESP32HWEncoder::indexHandler()
{
    pcnt_counter_clear(pcnt_config.unit);
    angleOverflow = 0;
    indexFound = true;
}

void ESP32HWEncoder::init()
{

    // Statically allocate and initialize the spinlock
    spinlock = portMUX_INITIALIZER_UNLOCKED;

    // find a free pulsecount unit
    for (uint8_t i = 0; i < PCNT_UNIT_MAX; i++)
    {
        pcnt_config.unit = (pcnt_unit_t) i;
        if(pcnt_unit_config(&pcnt_config) == ESP_OK){
            initialized = true;
            break;
        }

    }
    if (initialized)
    {
        // Set up the PCNT peripheral
        pcnt_set_pin(pcnt_config.unit, PCNT_CHANNEL_0, pcnt_config.ctrl_gpio_num, pcnt_config.pulse_gpio_num);
        pcnt_set_pin(pcnt_config.unit, PCNT_CHANNEL_1, pcnt_config.pulse_gpio_num, pcnt_config.ctrl_gpio_num);
        pcnt_set_mode(pcnt_config.unit, PCNT_CHANNEL_0, PCNT_COUNT_INC, PCNT_COUNT_DEC, PCNT_MODE_REVERSE, PCNT_MODE_KEEP);
        pcnt_set_mode(pcnt_config.unit, PCNT_CHANNEL_1, PCNT_COUNT_DEC, PCNT_COUNT_INC, PCNT_MODE_REVERSE, PCNT_MODE_KEEP);
        
        pcnt_counter_pause(pcnt_config.unit);
        pcnt_counter_clear(pcnt_config.unit);
        
        // Select interrupt on reaching high and low counter limit
        pcnt_event_enable(pcnt_config.unit, PCNT_EVT_L_LIM);
        pcnt_event_enable(pcnt_config.unit, PCNT_EVT_H_LIM);

        // Pass pointer to the angle accumulator and the current PCNT unit to the ISR
        overflowISR_args.angleoverflow_val = &angleOverflow;
        overflowISR_args.unit = pcnt_config.unit;

        // Register and enable the interrupt
        pcnt_isr_register(overflowCounter, (void*)&overflowISR_args, 0, (pcnt_isr_handle_t*) NULL);
        pcnt_intr_enable(pcnt_config.unit);
        
        // Just check the last command for errors
        if(pcnt_counter_resume(pcnt_config.unit) != ESP_OK){
            initialized = false;
        }
        
        // If an index Pin is defined, create a ISR to zero the angle when the index fires
        if (hasIndex())
        {
            attachInterrupt(static_cast<u_int8_t>(_pinI), std::bind(&ESP32HWEncoder::indexHandler,this), RISING);
        }
        
        // Optionally use pullups
        if (pullup == USE_INTERN)
        {
            pinMode(static_cast<u_int8_t>(_pinA), INPUT_PULLUP);
            pinMode(static_cast<u_int8_t>(_pinB), INPUT_PULLUP);
            if (hasIndex()) {pinMode(static_cast<u_int8_t>(_pinI), INPUT_PULLUP);}
        }

    }

}

int ESP32HWEncoder::needsSearch()
{
    return !((indexFound && hasIndex()) || !hasIndex());
}

int ESP32HWEncoder::hasIndex()
{
    return _pinI != -1;
}

float ESP32HWEncoder::getSensorAngle()
{
    if(!initialized){return -1.0f;}

    taskENTER_CRITICAL(&spinlock);
    // We are now in a critical section to prevent interrupts messing with angleOverflow and angleCounter

    // Retrieve the count register into a variable
    pcnt_get_counter_value(pcnt_config.unit, &angleCounter);

    // Trim the overflow variable to prevent issues with it overflowing
    // Make the % operand behave mathematically correct (-5 modulo 4 == 3; -5 % 4 == -1)
    angleOverflow %= cpr;
    if (angleOverflow < 0){
        angleOverflow += cpr;
    }

    angleSum = (angleOverflow + angleCounter) % cpr;
    if (angleSum < 0) {
        angleSum += cpr;
    }

    taskEXIT_CRITICAL(&spinlock); // Exit critical section

    // Calculate the shaft angle
    return _2PI * (float)angleSum / (float)cpr;
}

#endif