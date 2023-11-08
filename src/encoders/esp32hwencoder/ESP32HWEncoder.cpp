#include "ESP32HWEncoder.h"

#if defined(ESP_H) && defined(ARDUINO_ARCH_ESP32)



ESP32HWEncoder::ESP32HWEncoder(uint8_t pinA, uint8_t pinB, uint32_t ppr, int8_t pinI)
{
    _pinA = pinA;
    _pinB = pinB;
    _pinI = pinI;

    cpr = ppr * 4; // 4x for quadrature

    pcnt_config.ctrl_gpio_num =  static_cast<int>(pinA);;
    pcnt_config.pulse_gpio_num = static_cast<int>(pinB);;
    pcnt_config.counter_l_lim = INT16_MIN; // center on zero to use the index pin to reset the value to 0.
    pcnt_config.counter_h_lim = INT16_MAX;
    pcnt_config.unit = PCNT_UNIT_0;
} 

void IRAM_ATTR overflowCounter(void *arg)                // Interrupt handler for overflowing the pulsecounter count
{
    // uint32_t count = ((uint32_t*) arg)[0];
    // pcnt_unit_t unit = *(pcnt_unit_t*) (*(uint32_t*) arg);

    switch (PCNT.status_unit[*(pcnt_unit_t*) (*(uint32_t*) arg)].val)
    {
    case PCNT_EVT_L_LIM:
        ((uint32_t*) arg)[0] += INT16_MIN; // arg is pointer to angleOverflow
        pcnt_counter_clear(PCNT_UNIT_0); // reset counter
        break;
    case PCNT_EVT_H_LIM:
        ((uint32_t*) arg)[0] += INT16_MAX;
        pcnt_counter_clear(PCNT_UNIT_0); // reset counter
        break;
    default:
        break;
    }

    PCNT.int_clr.val = BIT(*(pcnt_unit_t*) (*(uint32_t*) arg));
}

void IRAM_ATTR ESP32HWEncoder::indexHandler()                // Interrupt handler for zeroing the pulsecounter count
{
    pcnt_counter_clear(pcnt_config.unit);
    angleOverflow = 0;
}

void ESP32HWEncoder::init()
{

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
        overflowISR_args_t overflowISR_args;
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
            pinMode(_pinA, INPUT_PULLUP);
            pinMode(_pinB, INPUT_PULLUP);
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

    // Retrieve the count register into a variable
    pcnt_get_counter_value(pcnt_config.unit, &angleCounter);
    
    // Calculate the shaft angle
    angleOverflow %= cpr; // trim the overflow variable to prevent issues with itself overflowing
    return _2PI * ((angleOverflow + angleCounter) % cpr) / (float)cpr;
}

#endif