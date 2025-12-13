# Simple JUCE plugin wrapper around [Digital Waveguide Mesh with Microphone Array output](https://github.com/J0ySF/dwm-ma)

This JUCE plugin can be used to test the [dwm-ma](https://github.com/J0ySF/dwm-ma) implementation.

## Testing setup with [SPARTA](https://github.com/leomccormack/SPARTA):

### Installing all required components

* Install [SPARTA](https://github.com/leomccormack/SPARTA)
* Build this project with CMake, the build result should automatically install in the appropriate VST3/LV2 folder

You will also need a Digital Audio Workstation program.

### Testing dwm-ma

#### #0 Preliminary steps 

* Build the dwm-ma plugin, you can configure the dmw parameters by re-defining any of the "User-redefinable definitions"
  that are found in `dwm-ma/dwm_ma.h`
* Open your DAW of choice, create a new project
* Set the project sample rate to 16000Hz, DSP buffer size to 128

#### #1 Set up the dwm-ma track

* Add a new track ("DWM-MA") with the dwm-ma plugin, 4 input channels and 2 output channels, outputting to master
* Add the [SPARTA Array2SH](https://leomccormack.github.io/sparta-site/docs/plugins/sparta-suite/#array2sh) plugin
  following dwm-ma, so that microphone array is encoded into ambisonics
* Add the [SPARTA AmbiBIN](https://leomccormack.github.io/sparta-site/docs/plugins/sparta-suite/#ambibin) plugin
  following Array2SH, so that the ambisonics are decoded into binaural audio

#### #2 Set up the audio sources tracks

* Create one additional track for each dwm-ma input (from "INPUT 1" to "INPUT 4")
* Connect each input track to "DWM-MA" channels 1..4
* Mute the master output on these tracks so that they can only be heard from the "DWM-MA" output

#### #2 Configure the dwm-ma plugin in accordance with the SPARTA plugins

* Select a "Mic array configuration" from the dwm-ma plugin
* Select "Array Type" Spherical
* * Select "Baffle-Directivity" Open-Omni
* Load the matching preset (from the directory `array2sh-presets`) with Array2SH, then select the correct array radius
  from the following table (only valid with sample rate 16000Hz):

<table>
  <tr>
  <td> Layout </td><td> Radius [mm] </td>
  </tr>
  <tr>
  <td> 6_POINTS_SQRT_1 </td><td> 37 </td>
  </tr>
  <tr>
  <td> 8_POINTS_SQRT_3 </td><td> 64 </td>
  </tr>
  <tr>
  <td> 12_POINTS_SQRT_2 </td><td> 53 </td>
  </tr>
  <tr>
  <td> 24_POINTS_SQRT_5 </td><td> 83 </td>
  </tr>
  <tr>
  <td> 24_POINTS_SQRT_6 </td><td> 91 </td>
  </tr>
  <tr>
  <td> 24_POINTS_SQRT_10 </td><td> 117 </td>
  </tr>
  <tr>
  <td> 24_POINTS_SQRT_11 </td><td> 123 </td>
  </tr>
  <tr>
  <td> 24_POINTS_SQRT_13 </td><td> 134 </td>
  </tr>
  <tr>
  <td> 30_POINTS_SQRT_9 </td><td> 111 </td>
  </tr>
</table>

* Read the "Encoding Order" reported by Array2SH and select it into AmbiBIN under "Decoding Order"

#### #4 Play

* Remember to enable a source in the dwm-ma plugin if you cannot hear it!