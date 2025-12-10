#include "dwm-ma/dwm_ma.h"
#include <juce_audio_processors/juce_audio_processors.h>

class plugin_processor final : public juce::AudioProcessor {
  void add_position_parameters(const char *const name,
                               juce::AudioParameterFloat *pos[3]) {
    const char *const axis[3] = {"x", "y", "z"};
    const float max[3] = {DWM_MA_SIZE_X_M, DWM_MA_SIZE_Y_M, DWM_MA_SIZE_Z_M};
    for (int i = 0; i < 3; i++) {
      const auto id = std::string(name) + " position " + axis[i];
      pos[i] = new juce::AudioParameterFloat(id, id, 0.0f, max[i], max[i] / 2);
      addParameter(pos[i]);
    }
  }

  static void delete_position_parameters(juce::AudioParameterFloat *pos[3]) {
    for (int i = 0; i < 3; i++)
      delete pos[i];
  }

  const juce::StringArray ma_config_choices = juce::StringArray{
      "6 Points, 1 junction distance",
      "6 Points, 3 junctions distance",
      "30 Points, 3 junctions distance",
  };

public:
  plugin_processor() {
    // TODO: create and initialize dwm-ma handle
    reset = false;

    const auto ma_name = "Mic Array";
    {
      const auto ma_config_id = std::string(ma_name) + " configuration";
      ma_config = new juce::AudioParameterChoice(ma_config_id, ma_config_id,
                                                 ma_config_choices, 0);
      addParameter(ma_config);
      add_position_parameters(ma_name, ma_pos);
    }

    const auto input_name = "Input";
    for (int i = 0; i < DWM_MA_MAX_INPUT_COUNT; i++) {
      const auto id = std::string(input_name) + " " + std::to_string(i);
      const auto enabled_id = id + " enabled";
      input_enabled[i] =
          new juce::AudioParameterBool(enabled_id, enabled_id, false);
      addParameter(input_enabled[i]);
      add_position_parameters(id.c_str(), input_pos[i]);
    }
  }

  ~plugin_processor() override {
    delete ma_config;
    delete_position_parameters(ma_pos);
    for (int i = 0; i < DWM_MA_MAX_INPUT_COUNT; i++) {
      delete input_enabled[i];
      delete_position_parameters(input_pos[i]);
    }
  }

  const juce::String getName() const override { return JucePlugin_Name; }

  void prepareToPlay(const double, const int) override {}

  void releaseResources() override {}

  bool isBusesLayoutSupported(const BusesLayout &layouts) const override {
    const int max_output_channels = 30; // TODO: implement
    return layouts.getMainInputChannels() == DWM_MA_MAX_INPUT_COUNT &&
           layouts.getMainOutputChannels() == max_output_channels;
  }

  void processBlock(juce::AudioBuffer<float> &buffer,
                    juce::MidiBuffer &) override {
    juce::ScopedNoDenormals noDenormals;

    // If the input sample rate and buffer size do not satisfy the required
    // specs, mute the output and stop the simulation
    if (DWM_MA_SAMPLE_RATE != static_cast<int>(getSampleRate()) ||
        DWM_MA_BUFFER_SIZE != buffer.getNumSamples()) {
      reset = true;
      for (auto i = 0; i < getTotalNumOutputChannels(); i++)
        buffer.clear(i, 0, buffer.getNumSamples());
      return;
    }
    if (reset) {
      // TODO: re-initialize dwm-ma handle
      reset = false;
    }

    // TODO: implement

    for (auto i = 0; i < getTotalNumOutputChannels(); i++)
      buffer.clear(i, 0, buffer.getNumSamples());
  }

  double getTailLengthSeconds() const override { return 0.0; }

  bool acceptsMidi() const override { return false; }

  bool producesMidi() const override { return false; }

  juce::AudioProcessorEditor *createEditor() override {
    return new juce::GenericAudioProcessorEditor(*this);
  }

  bool hasEditor() const override { return false; }

  int getNumPrograms() override { return 0; }

  int getCurrentProgram() override { return 0; }

  void setCurrentProgram(const int) override {}

  const juce::String getProgramName(const int) override { return ""; }

  void changeProgramName(int, const juce::String &) override {}

  void getStateInformation(juce::MemoryBlock &) override {
    // TODO: implement
  }

  void setStateInformation(const void *, const int) override {
    // TODO: implement
  }

private:
  bool reset;
  juce::AudioParameterChoice *ma_config;
  juce::AudioParameterFloat *ma_pos[3];
  juce::AudioParameterBool *input_enabled[DWM_MA_MAX_INPUT_COUNT];
  juce::AudioParameterFloat *input_pos[DWM_MA_MAX_INPUT_COUNT][3];

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(plugin_processor)
};

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
  return new plugin_processor();
}
