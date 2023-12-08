# MultiFilter

The `MultiFilter` class implements a state variable filter that can compute low-pass, high-pass, bandpass, and notch filters with variable resonance (Q) in one operation. It also allows for seamless switching between different filter types. By default, it acts just like the normal LowPassFilter class.


## Usage

To use the `MultiFilter` class, you need to include the following header file:

```cpp
#include <Arduino.h>
#include "SimpleFOC.h"
#include "SimpleFOCDrivers.h"
#include "utilities/MultiFilter/MultiFilter.h"
```

Then, create an instance of the `MultiFilter` class by providing the filter time constant (`Tf`) and optionally the resonance (`q`). The time constant represents the inverse of the center frequency for bandpass and notch filters or -3dB cutoff frequency for low-pass and high-pass filters.

```cpp
float Tf = 0.01; // Filter time constant
float q = 0.707; // Filter resonance (optional)
MultiFilter filter(Tf, q);
```

You can then use the `operator()` function to apply filtering on input values:

```cpp
float filteredValue = filter(inputValue);
```

The default return type is set to low-pass filtering. You can change it using the `setReturnType()` function:

```cpp
filter.setReturnType(MultiFilter::MULTI_FILTER_HIGHPASS); // Set return type to high-pass filtering
```

You can also adjust other parameters of the filter dynamically:

- To change Q (resonance), use `setQ()`. The Notch and Bandpass Filters will be normalized to unity gain regardless of Q, but High- and Lowpass will have amplification greater than 1 at their resonant peak, in case Q gets larger than about 0.707.
- To change Tf (time constant), use either `setTf()` or `setFrequency()`. The latter sets Tf based on a desired center frequency.
- To set notch depth for notch filters, use `setNotchDepth()`. The set value determines how deep a notch is cut by the notch filter. A notchDepth of 0.0 cuts everything at peak amplitude, while larger values tend towards no cutting/filtering.

Additionally, you can retrieve specific outputs from each type of filter using these functions:

- Low-Pass: `getLp()`
- High-Pass: `getHp()`
- Band-Pass: `getBp()`
- Notch: `getNotch()`

These functions return the filtered output based on the current filter state.

If you want to retrieve filter outputs while updating the filter state, you can use these functions:

- Low-Pass: `getLp(float x)`
- High-Pass: `getHp(float x)`
- Band-Pass: `getBp(float x)`
- Notch: `getNotch(float x)`

These functions take an input value (`x`) and update the filter state accordingly before returning the filtered output.