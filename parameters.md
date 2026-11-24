# Lost Technology Parameter Reference

This document records the legacy VST2 parameter layout and how the current JUCE-based VST3 and CLAP builds expose it. The mapping formulas describe the DSP's interpretation of normalized values; they do not imply that the wrappers publish typed, stepped host parameters.

## Plug-in identity

| Property | Value |
|---|---|
| Effect name | Lost Technology |
| Product string | Lost Recombinant Audio Technology |
| Vendor | Transvaal Audio |
| VST2 unique ID | `CQCQ` |
| VST2 unique ID expression | `CCONST('C', 'Q', 'C', 'Q')` |
| VST2 unique ID, hexadecimal | `0x43514351` |
| VST2 unique ID, decimal | `1129395025` |
| CLAP plug-in ID | `com.transvaalaudio.losttech` |
| VST3 plug-in identity | JUCE `PLUGIN_CODE CQCQ`; VST3 replacement compatibility enabled |
| Plug-in category | Effect |
| Programs | 20 factory presets, defined in `LostTechParameters.cpp` |
| Audio inputs | 2 |
| Audio outputs | 2 |
| Total VST2 parameters | 54 |
| Mutable legacy parameters | 53 |

The CLAP ID above is the one configured in `juce/CMakeLists.txt`.

## Parameter organization

The legacy VST2 parameter order is:

1. Global parameters, indices `0` and `1`
2. Channel 1 parameters, indices `2` through `27`
3. Channel 2 parameters, indices `28` through `53`

Each channel contains the same block of 26 parameters. The JUCE processor registers all 54 parameters in this legacy order, including `Version` at index 0. The parameter table's index column is the position used by the implementation; it should not be read as a published CLAP parameter ID. `JUCE_FORCE_USE_LEGACY_PARAM_IDS=1` is enabled for VST3 compatibility.

The JUCE wrapper uses a generic `AudioProcessorParameter` for each entry. It supplies normalized value, default, name, label, and display text from the core; it marks only `Version` non-automatable. It does not declare parameter classes with boolean, integer, or choice metadata, nor custom ranges or step counts. Consequently, the core may interpret a parameter discretely (for example, a modulation type) while the host-facing wrapper still exposes a normalized parameter. The CLAP extension adapts these JUCE parameters; the project does not define a separate CLAP parameter table or explicit per-parameter CLAP IDs/flags.

## Mapping conventions

- `v` is the normalized legacy VST2 value in the range `[0.0, 1.0]`.
- `G` is `globalMaxFrequency`, which is derived from the current sample rate.
- Values marked as sample-rate dependent must be recalculated when the sample rate changes.
- Boolean parameters evaluate to `On` only when `v > 0.5`.
- Enum mappings use `0.999` to ensure that `v = 1.0` selects the final valid enum entry.

## Complete parameter index

| Legacy/JUCE parameter index | Legacy name | Scope | Default `v` | Effective default | Effective range | Unit | Core mapping |
|---:|---:|---|---|---:|---|---|---|---|
| 0 | `Version` | Global | 0.0 | Version string | Not applicable | None | Display-only; wrapper marks non-automatable |
| 1 | `BufrSize` | Global | 0.285714 | 4x | 1x to 128x | Multiplier | `1 << round(7 * v)` |
| 2 | `AIncLag1` | Channel 1 | 0.1 | 88.773% | 0% to 100% | % | `100 * pow(log10(9*v + 1), 0.2)` |
| 3 | `ADecLag1` | Channel 1 | 0.8 | 99.909% | 0% to 100% | % | `100 * pow(log10(9*v + 1), 0.01)` |
| 4 | `GatLvlA1` | Channel 1 | 0.05 | 0.0126% | 0% to 100% | % | `100 * (pow(2, 12*v) - 1) / 4095` |
| 5 | `GatLvlS1` | Channel 1 | 0.75 | 12.4817% | 0% to 100% | % | `100 * (pow(2, 12*v) - 1) / 4095` |
| 6 | `HiTrig1` | Channel 1 | 0.783 | 84.9% | -150% to +150% | % | `300 * (v - 0.5)` |
| 7 | `LowTrig1` | Channel 1 | 0.217 | -84.9% | -150% to +150% | % | `300 * (v - 0.5)` |
| 8 | `FMin1` | Channel 1 | 0.00055 | Sample-rate dependent | 1 Hz to G Hz | Hz | `(G - 1) * (pow(2, 9*v) - 1) / 511 + 1` |
| 9 | `FMax1` | Channel 1 | 0.73 | Sample-rate dependent | 1 Hz to G Hz | Hz | `(G - 1) * (pow(2, 9*v) - 1) / 511 + 1` |
| 10 | `FLag1` | Channel 1 | 0.0 | 0% | 0% to 100% | % | `100 * pow(log10(9*v + 1), 0.4)` |
| 11 | `WLag1` | Channel 1 | 0.0 | 0% | 0% to 100% | % | `100 * pow(log10(9*v + 1), 0.4)` |
| 12 | `InvTrig1` | Channel 1 | 0.0 | Off | Off or On | Boolean | `v > 0.5` |
| 13 | `Interp1` | Channel 1 | 1.0 | On | Off or On | Boolean | `v > 0.5` |
| 14 | `AModTyp1` | Channel 1 | 0.0 | Add | 8 enum values | Type | `floor(0.999 * v * 8)` |
| 15 | `AModMix1` | Channel 1 | 0.0 | 0% | 0% to 100% | % | `100 * v` |
| 16 | `FModTyp1` | Channel 1 | 0.0 | Add | 8 enum values | Type | `floor(0.999 * v * 8)` |
| 17 | `FModMix1` | Channel 1 | 0.0 | 0% | 0% to 100% | % | `100 * v` |
| 18 | `WModTyp1` | Channel 1 | 0.0 | Add | 8 enum values | Type | `floor(0.999 * v * 8)` |
| 19 | `WModMix1` | Channel 1 | 0.0 | 0% | 0% to 100% | % | `100 * v` |
| 20 | `AOffset1` | Channel 1 | 0.5 | 0% | -100% to +100% | % | `200 * (v - 0.5)` |
| 21 | `AGain1` | Channel 1 | 0.5 | 0 dB, gain 1.0 | Quiet to gain 5000 | dB display | Piecewise gain mapping described below |
| 22 | `FOffset1` | Channel 1 | 0.5 | 0 Hz | -G Hz to +G Hz | Hz | Symmetric exponential mapping described below |
| 23 | `FGain1` | Channel 1 | 0.5 | 0 octaves, factor 1.0 | Minimum to factor 5000 | Octaves | Gain mapping as `AGain`; display is `log2(gain)` |
| 24 | `Oversmp1` | Channel 1 | 0.0 | 1x | 1x to 16x | Multiplier | `1 + floor(15 * v)` |
| 25 | `SmooWin1` | Channel 1 | 0.0 | 0 samples | 0 to 250 samples | Samples | `floor(250 * v)` |
| 26 | `OModTyp1` | Channel 1 | 0.0 | Add | 6 enum values | Type | `floor(0.999 * v * 6)` |
| 27 | `OModMix1` | Channel 1 | 0.0 | 0% | 0% to 100% | % | `100 * v` |
| 28 | `AIncLag2` | Channel 2 | 0.1 | 88.773% | 0% to 100% | % | `100 * pow(log10(9*v + 1), 0.2)` |
| 29 | `ADecLag2` | Channel 2 | 0.8 | 99.909% | 0% to 100% | % | `100 * pow(log10(9*v + 1), 0.01)` |
| 30 | `GatLvlA2` | Channel 2 | 0.05 | 0.0126% | 0% to 100% | % | `100 * (pow(2, 12*v) - 1) / 4095` |
| 31 | `GatLvlS2` | Channel 2 | 0.75 | 12.4817% | 0% to 100% | % | `100 * (pow(2, 12*v) - 1) / 4095` |
| 32 | `HiTrig2` | Channel 2 | 0.783 | 84.9% | -150% to +150% | % | `300 * (v - 0.5)` |
| 33 | `LowTrig2` | Channel 2 | 0.217 | -84.9% | -150% to +150% | % | `300 * (v - 0.5)` |
| 34 | `FMin2` | Channel 2 | 0.00055 | Sample-rate dependent | 1 Hz to G Hz | Hz | `(G - 1) * (pow(2, 9*v) - 1) / 511 + 1` |
| 35 | `FMax2` | Channel 2 | 0.73 | Sample-rate dependent | 1 Hz to G Hz | Hz | `(G - 1) * (pow(2, 9*v) - 1) / 511 + 1` |
| 36 | `FLag2` | Channel 2 | 0.0 | 0% | 0% to 100% | % | `100 * pow(log10(9*v + 1), 0.4)` |
| 37 | `WLag2` | Channel 2 | 0.0 | 0% | 0% to 100% | % | `100 * pow(log10(9*v + 1), 0.4)` |
| 38 | `InvTrig2` | Channel 2 | 0.0 | Off | Off or On | Boolean | `v > 0.5` |
| 39 | `Interp2` | Channel 2 | 1.0 | On | Off or On | Boolean | `v > 0.5` |
| 40 | `AModTyp2` | Channel 2 | 0.0 | Add | 8 enum values | Type | `floor(0.999 * v * 8)` |
| 41 | `AModMix2` | Channel 2 | 0.0 | 0% | 0% to 100% | % | `100 * v` |
| 42 | `FModTyp2` | Channel 2 | 0.0 | Add | 8 enum values | Type | `floor(0.999 * v * 8)` |
| 43 | `FModMix2` | Channel 2 | 0.0 | 0% | 0% to 100% | % | `100 * v` |
| 44 | `WModTyp2` | Channel 2 | 0.0 | Add | 8 enum values | Type | `floor(0.999 * v * 8)` |
| 45 | `WModMix2` | Channel 2 | 0.0 | 0% | 0% to 100% | % | `100 * v` |
| 46 | `AOffset2` | Channel 2 | 0.5 | 0% | -100% to +100% | % | `200 * (v - 0.5)` |
| 47 | `AGain2` | Channel 2 | 0.5 | 0 dB, gain 1.0 | Quiet to gain 5000 | dB display | Piecewise gain mapping described below |
| 48 | `FOffset2` | Channel 2 | 0.5 | 0 Hz | -G Hz to +G Hz | Hz | Symmetric exponential mapping described below |
| 49 | `FGain2` | Channel 2 | 0.5 | 0 octaves, factor 1.0 | Minimum to factor 5000 | Octaves | Gain mapping as `AGain`; display is `log2(gain)` |
| 50 | `Oversmp2` | Channel 2 | 0.0 | 1x | 1x to 16x | Multiplier | `1 + floor(15 * v)` |
| 51 | `SmooWin2` | Channel 2 | 0.0 | 0 samples | 0 to 250 samples | Samples | `floor(250 * v)` |
| 52 | `OModTyp2` | Channel 2 | 0.0 | Add | 6 enum values | Type | `floor(0.999 * v * 6)` |
| 53 | `OModMix2` | Channel 2 | 0.0 | 0% | 0% to 100% | % | `100 * v` |

## Parameter definitions

### Version

Legacy VST2 index: `0`

The legacy parameter is a display-only dummy used to expose the plug-in version through the VST2 parameter interface. In the JUCE implementation it remains registered and the wrapper marks it non-automatable; writes stored by the core do not affect DSP behavior.

**Current implementation:** This is registered at index 0 and displayed as the plug-in version. The JUCE wrapper marks it non-automatable; writes are accepted by the core parameter store but do not affect DSP behavior.

### Buffer Size

Legacy VST2 index: `1`

Defines an internal buffer-size multiplier. The normalized mapping is:

```text
1 << round(7 * v)
```

Available values:

| Enum | Value |
|---:|---:|
| 0 | 1x |
| 1 | 2x |
| 2 | 4x |
| 3 | 8x |
| 4 | 16x |
| 5 | 32x |
| 6 | 64x |
| 7 | 128x |

Default: `4x`.

**Current implementation:** Registered as an ordinary normalized JUCE parameter and automatable by the wrapper. The core quantizes its value to one of eight buffer multipliers.

### Amplitude Increase Lag

Legacy names: `AIncLag1`, `AIncLag2`

Controls smoothing or lag when the analyzed amplitude increases.

```text
valuePercent = 100 * pow(log10(9*v + 1), 0.2)
```

- Default normalized value: `0.1`
- Effective default: approximately `88.773%`
- Range: `0%` to `100%`
- Host metadata: generic normalized JUCE parameter; automatable in the current wrapper

### Amplitude Decrease Lag

Legacy names: `ADecLag1`, `ADecLag2`

Controls smoothing or lag when the analyzed amplitude decreases.

```text
valuePercent = 100 * pow(log10(9*v + 1), 0.01)
```

- Default normalized value: `0.8`
- Effective default: approximately `99.909%`
- Range: `0%` to `100%`
- Host metadata: generic normalized JUCE parameter; automatable in the current wrapper

### Gate Level, Amplitude

Legacy names: `GatLvlA1`, `GatLvlA2`

Defines the amplitude gate threshold.

```text
valuePercent = 100 * (pow(2, 12*v) - 1) / 4095
```

- Default normalized value: `0.05`
- Effective default: approximately `0.0126%`
- Range: `0%` to `100%`
- Host metadata: generic normalized JUCE parameter; automatable in the current wrapper

### Gate Level, Signal

Legacy names: `GatLvlS1`, `GatLvlS2`

Defines the signal gate threshold using the same exponential mapping as the amplitude gate.

- Default normalized value: `0.75`
- Effective default: approximately `12.4817%`
- Range: `0%` to `100%`
- Host metadata: generic normalized JUCE parameter; automatable in the current wrapper

### High Trigger

Legacy names: `HiTrig1`, `HiTrig2`

Defines the upper trigger level.

```text
valuePercent = 300 * (v - 0.5)
```

- Default normalized value: `0.783`
- Effective default: `84.9%`
- Range: `-150%` to `+150%`
- Host metadata: generic normalized JUCE parameter; automatable in the current wrapper

### Low Trigger

Legacy names: `LowTrig1`, `LowTrig2`

Defines the lower trigger level.

```text
valuePercent = 300 * (v - 0.5)
```

- Default normalized value: `0.217`
- Effective default: `-84.9%`
- Range: `-150%` to `+150%`
- Host metadata: generic normalized JUCE parameter; automatable in the current wrapper

### Minimum Frequency

Legacy names: `FMin1`, `FMin2`

Defines the minimum analyzed or generated frequency.

```text
frequencyHz = (G - 1) * (pow(2, 9*v) - 1) / 511 + 1
```

- Default normalized value: `0.00055`
- Range: `1 Hz` to `G Hz`
- Host metadata: generic normalized JUCE parameter; automatable in the current wrapper
- Important: The effective value is sample-rate dependent.

### Maximum Frequency

Legacy names: `FMax1`, `FMax2`

Defines the maximum analyzed or generated frequency using the same mapping as the minimum frequency.

- Default normalized value: `0.73`
- Range: `1 Hz` to `G Hz`
- Host metadata: generic normalized JUCE parameter; automatable in the current wrapper
- Important: The effective value is sample-rate dependent.

### Frequency Lag

Legacy names: `FLag1`, `FLag2`

Controls smoothing or lag for frequency changes.

```text
valuePercent = 100 * pow(log10(9*v + 1), 0.4)
```

- Default normalized value: `0.0`
- Effective default: `0%`
- Range: `0%` to `100%`
- Host metadata: generic normalized JUCE parameter; automatable in the current wrapper

### Waveform Lag

Legacy names: `WLag1`, `WLag2`

Controls smoothing or lag for waveform changes using the same mapping as Frequency Lag.

- Default normalized value: `0.0`
- Effective default: `0%`
- Range: `0%` to `100%`
- Host metadata: generic normalized JUCE parameter; automatable in the current wrapper

### Invert Trigger

Legacy names: `InvTrig1`, `InvTrig2`

Inverts the trigger behavior.

```text
value = v > 0.5
```

- Default: `Off`
- Values: `Off`, `On`
- Host metadata: generic normalized JUCE parameter; automatable in the current wrapper

### Interpolation

Legacy names: `Interp1`, `Interp2`

Enables waveform interpolation.

```text
value = v > 0.5
```

- Default: `On`
- Values: `Off`, `On`
- Host metadata: generic normalized JUCE parameter; automatable in the current wrapper

### Amplitude Modulation Type

Legacy names: `AModTyp1`, `AModTyp2`

Selects the amplitude modulation algorithm.

```text
selection = floor(0.999 * v * 8)
```

| Value | Display | Meaning |
|---:|---|---|
| 0 | Add | Mix input 1 with input 2 |
| 1 | MinDiff | Combine the lower input with the absolute difference |
| 2 | MaxDiff | Combine the higher input with the absolute difference |
| 3 | Fine | Fine tuning relative to the magnitude of input 1 |
| 4 | Mult | Blend using the product of both inputs |
| 5 | Dist | Distort input 1 under control of input 2 |
| 6 | Top | Mix into the remaining range up to the maximum |
| 7 | Pong | LFO-controlled movement between both inputs |

- Default: `Add`
- Host metadata: generic normalized JUCE parameter; automatable in the current wrapper

### Amplitude Modulation Mix

Legacy names: `AModMix1`, `AModMix2`

Controls the amount of amplitude modulation.

```text
valuePercent = 100 * v
```

- Default: `0%`
- Range: `0%` to `100%`
- Host metadata: generic normalized JUCE parameter; automatable in the current wrapper

### Frequency Modulation Type

Legacy names: `FModTyp1`, `FModTyp2`

Selects the frequency modulation algorithm. It uses the same eight values and mapping as Amplitude Modulation Type.

- Default: `Add`
- Host metadata: generic normalized JUCE parameter; automatable in the current wrapper

### Frequency Modulation Mix

Legacy names: `FModMix1`, `FModMix2`

Controls the amount of frequency modulation.

- Default: `0%`
- Range: `0%` to `100%`
- Mapping: `100 * v`
- Host metadata: generic normalized JUCE parameter; automatable in the current wrapper

### Waveform Modulation Type

Legacy names: `WModTyp1`, `WModTyp2`

Selects the waveform modulation algorithm.

```text
selection = floor(0.999 * v * 8)
```

| Value | Display | Meaning |
|---:|---|---|
| 0 | Add | Mix both waveforms |
| 1 | Diff | Mix with the inverted second waveform |
| 2 | Mult | Multiply both waveforms |
| 3 | Dist | Apply distortion |
| 4 | Top | Fill the remaining range up to the maximum |
| 5 | Comp | Functional composition |
| 6 | Conv | Convolution using input 2 as an FIR impulse response |
| 7 | Pong | LFO-controlled movement between both waveforms |

- Default: `Add`
- Host metadata: generic normalized JUCE parameter; automatable in the current wrapper

### Waveform Modulation Mix

Legacy names: `WModMix1`, `WModMix2`

Controls the amount of waveform modulation.

- Default: `0%`
- Range: `0%` to `100%`
- Mapping: `100 * v`
- Host metadata: generic normalized JUCE parameter; automatable in the current wrapper

### Amplitude Offset

Legacy names: `AOffset1`, `AOffset2`

Adds a bipolar amplitude offset.

```text
valuePercent = 200 * (v - 0.5)
```

- Default normalized value: `0.5`
- Effective default: `0%`
- Range: `-100%` to `+100%`
- Host metadata: generic normalized JUCE parameter; automatable in the current wrapper

### Amplitude Gain

Legacy names: `AGain1`, `AGain2`

Applies amplitude gain using a piecewise mapping:

```text
gain(v) =
    2 * v                    when v <= 0.5
    1 / (2 * (1 - v))        when 0.5 < v <= 0.9999
    5000                     when v > 0.9999

displayDb = 20 * log10(gain)
```

Representative values:

| Normalized value | Linear gain | Display |
|---:|---:|---:|
| 0.0 | 0 | Quiet |
| 0.25 | 0.5 | -6.02 dB |
| 0.5 | 1.0 | 0 dB |
| 0.75 | 2.0 | +6.02 dB |
| 0.9 | 5.0 | +13.98 dB |
| 1.0 | 5000 | approximately +73.98 dB |

- Default normalized value: `0.5`
- Host metadata: generic normalized JUCE parameter; automatable in the current wrapper

### Frequency Offset

Legacy names: `FOffset1`, `FOffset2`

Adds a bipolar frequency offset with a symmetric exponential mapping around `v = 0.5`.

```text
normalizedOffset(v) =
    +(pow(2, 32*(v - 0.5)) - 1) / 65535   when v > 0.5
    -(pow(2, 32*(0.5 - v)) - 1) / 65535   when v <= 0.5

frequencyOffsetHz = G * normalizedOffset(v)
```

- Default normalized value: `0.5`
- Effective default: `0 Hz`
- Range: `-G Hz` to `+G Hz`
- Host metadata: generic normalized JUCE parameter; automatable in the current wrapper
- Important: The range is sample-rate dependent.

### Frequency Gain

Legacy names: `FGain1`, `FGain2`

Uses the same piecewise factor mapping as Amplitude Gain, but displays the result in octaves:

```text
frequencyFactor = gain(v)
displayOctaves = log2(frequencyFactor)
```

Representative values:

| Normalized value | Frequency factor | Display |
|---:|---:|---:|
| 0.0 | 0 | Minimum |
| 0.25 | 0.5 | -1 octave |
| 0.5 | 1.0 | 0 octaves |
| 0.75 | 2.0 | +1 octave |
| 0.9 | 5.0 | approximately +2.322 octaves |
| 1.0 | 5000 | approximately +12.288 octaves |

- Default normalized value: `0.5`
- Host metadata: generic normalized JUCE parameter; automatable in the current wrapper

### Oversampling

Legacy names: `Oversmp1`, `Oversmp2`

Defines the oversampling factor.

```text
factor = 1 + floor(15 * v)
```

- Default: `1x`
- Integer range: `1x` to `16x`
- Host metadata: generic normalized JUCE parameter; automatable in the current wrapper

### Smoothing Window

Legacy names: `SmooWin1`, `SmooWin2`

Defines the smoothing-window size in samples.

```text
samples = floor(250 * v)
```

- Default: `0 samples`
- Integer range: `0` to `250 samples`
- Host metadata: generic normalized JUCE parameter; automatable in the current wrapper

### Output Modulation Type

Legacy names: `OModTyp1`, `OModTyp2`

Selects the output modulation algorithm.

```text
selection = floor(0.999 * v * 6)
```

| Value | Display | Meaning |
|---:|---|---|
| 0 | Add | Mix both outputs |
| 1 | Diff | Mix with the inverted second output |
| 2 | Mult | Multiply both outputs |
| 3 | Dist | Apply distortion |
| 4 | Top | Fill the remaining range up to the maximum |
| 5 | Pong | LFO-controlled movement between both outputs |

- Default: `Add`
- Host metadata: generic normalized JUCE parameter; automatable in the current wrapper

### Output Modulation Mix

Legacy names: `OModMix1`, `OModMix2`

Controls the amount of output modulation.

- Default: `0%`
- Range: `0%` to `100%`
- Mapping: `100 * v`
- Host metadata: generic normalized JUCE parameter; automatable in the current wrapper

## Current VST3 and CLAP parameter behavior

Both formats are produced from the same `LostTechJUCE` processor and its 54 generic JUCE parameters. All parameters except `Version` report automatable. The wrapper forwards normalized values to the core, and supplies the core's names, labels, default values, and formatted display strings. The core clamps incoming values to `[0, 1]`; discrete behavior is implemented inside the DSP mapping, not declared through JUCE choice/boolean/integer parameter types.

For VST3, `JUCE_FORCE_USE_LEGACY_PARAM_IDS=1` is set to retain JUCE's legacy parameter identification behavior. For CLAP, the configured plug-in ID is `com.transvaalaudio.losttech`; this project does not assign explicit CLAP parameter IDs or flags independently of the JUCE parameter list. Do not assume the table indices are CLAP IDs.

The processor also exposes the 20 factory presets through JUCE's program interface. Its state serialization stores a format number, parameter count, current program, and all 54 normalized parameter values.

## Source files

The parameter layout and mappings are implemented in the core and JUCE wrapper. The mapping was derived from these source files:

- `waveplugparams.h`
- `LostTechParameters.cpp`
- `WavePlug.cpp`
- `WavePlugEditor.cpp`
- `wpmodulators.cpp`
- `wpfunc.cpp`
