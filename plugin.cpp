extern "C" {
#include <dwm_ma.h>
}

#include <juce_audio_processors/juce_audio_processors.h>

class plugin_processor final : public juce::AudioProcessor {

    /**
     * Create and add the XYZ axis parameters for something
     * @param name parameter name prefix
     * @param index index used to position the default value around the listener
     * @param pos XYZ juce float parameters
     */
    void add_position_parameters(const char *const name, const int index, juce::AudioParameterFloat *pos[3]) {
        const char *const axis[3] = {"x", "y", "z"};
        const float max[3] = {DWM_MA_SIZE_X_M, DWM_MA_SIZE_Y_M, DWM_MA_SIZE_Z_M};
        float def[3] = {DWM_MA_SIZE_X_M / 2.0f, DWM_MA_SIZE_Y_M / 2.0f, DWM_MA_SIZE_Z_M / 2.0f};
        if (index >= 0) {
            const float angle = static_cast<float>(index) / DWM_MA_MAX_INPUT_COUNT * static_cast<float>(M_PI * 2.0f);
            def[0] += std::cos(angle) * DWM_MA_SIZE_X_M / 4.0f;
            def[2] += std::sin(angle) * DWM_MA_SIZE_Z_M / 4.0f;
        }
        for (int i = 0; i < 3; i++) {
            const auto id = std::string(name) + " position " + axis[i];
            pos[i] = new juce::AudioParameterFloat(id, id, 0.0f, max[i], def[i]);
            addParameter(pos[i]);
        }
    }

    /**
     * Auxiliary function for destructor code
     * @param pos the XYZ juce float params to delete
     */
    static void delete_position_parameters(juce::AudioParameterFloat *pos[3]) {
        for (int i = 0; i < 3; i++)
            delete pos[i];
    }

    /**
     * Parameters corresponding to the MA_CONFIG values in dwm-ma/ma_config.h
     */
    const juce::StringArray ma_config_choices = juce::StringArray{
            "6_POINTS_SQRT_1",   "8_POINTS_SQRT_3",   "12_POINTS_SQRT_2",  "24_POINTS_SQRT_5", "24_POINTS_SQRT_6",
            "24_POINTS_SQRT_10", "24_POINTS_SQRT_11", "24_POINTS_SQRT_13", "30_POINTS_SQRT_9",
            // TODO: investigate the anomalous behavior of 48_POINTS_SQRT_14
            //"48_POINTS_SQRT_14",
    };

    /**
     * dwm boundary parameters
     */
    const float bounds_params[6][2] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}};

public:
    plugin_processor() :
        AudioProcessor(
                BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::discreteChannels(DWM_MA_MAX_INPUT_COUNT), true)
                        .withOutput("Output", juce::AudioChannelSet::discreteChannels(DWM_MA_MAX_OUTPUT_COUNT), true)) {
        // Create mic array output parameters
        const auto ma_name = "Mic Array";
        {
            const auto ma_config_id = std::string(ma_name) + " configuration";
            ma_config = new juce::AudioParameterChoice(ma_config_id, ma_config_id, ma_config_choices, 0);
            addParameter(ma_config);
            add_position_parameters(ma_name, -1, ma_pos);
        }

        // Create input control parameters
        const auto input_name = "Input";
        for (int i = 0; i < DWM_MA_MAX_INPUT_COUNT; i++) {
            const auto id = std::string(input_name) + " " + std::to_string(i);
            const auto enabled_id = id + " enabled";
            input_enabled[i] = new juce::AudioParameterBool(enabled_id, enabled_id, false);
            addParameter(input_enabled[i]);
            add_position_parameters(id.c_str(), i, input_pos[i]);
        }

        // Instantiate and initialize the dwm-ma instance
        dwm_ma_create(&dwm_ma);
        dwm_ma_init(dwm_ma, bounds_params, true);
        reset = false;
    }

    ~plugin_processor() override {
        dwm_ma_destroy(&dwm_ma);

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
        return layouts.getMainInputChannels() == DWM_MA_MAX_INPUT_COUNT &&
               layouts.getMainOutputChannels() == DWM_MA_MAX_OUTPUT_COUNT;
    }

    using AudioProcessor::processBlock;
    void processBlock(juce::AudioBuffer<float> &juce_buffer, juce::MidiBuffer &) override {
        juce::ScopedNoDenormals noDenormals;

        // If the input sample rate, buffer size or channel counts do not satisfy
        // the required specs, clear all output channels and stop the simulation
        if (DWM_MA_SAMPLE_RATE != static_cast<int>(getSampleRate()) ||
            DWM_MA_BUFFER_SIZE != juce_buffer.getNumSamples() || getTotalNumInputChannels() < DWM_MA_MAX_INPUT_COUNT ||
            getTotalNumOutputChannels() < DWM_MA_MAX_OUTPUT_COUNT) {
            reset = true;
            for (auto i = 0; i < getTotalNumOutputChannels(); i++)
                juce_buffer.clear(i, 0, juce_buffer.getNumSamples());
            return;
        }

        // Handle situations where the simulation needs to restart
        if (reset) {
            dwm_ma_init(dwm_ma, bounds_params, true);
            reset = false;
        }
        // Buffers shared for both input and output
        float *const *buffers = juce_buffer.getArrayOfWritePointers();

        // Handle input information
        int input_active_counter = 0;
        float *input_buffers[DWM_MA_MAX_INPUT_COUNT];
        float input_positions[DWM_MA_MAX_INPUT_COUNT][3];
        float *input_positions_ptr[DWM_MA_MAX_INPUT_COUNT];
        for (int i = 0; i < DWM_MA_MAX_INPUT_COUNT; i++) {
            if (!input_enabled[i]->get())
                continue;
            input_buffers[input_active_counter] = buffers[i];
            input_positions[input_active_counter][0] = input_pos[i][0]->get();
            input_positions[input_active_counter][1] = input_pos[i][1]->get();
            input_positions[input_active_counter][2] = input_pos[i][2]->get();
            input_positions_ptr[input_active_counter] = input_positions[input_active_counter];
            input_active_counter++;
        }

        // Handle output information
        const auto out_config = static_cast<MA_CONFIG>(ma_config->getIndex() + 2); // Skip mono and stereo
        const ma_layout *out_layout = ma_config_layout(out_config);
        float output_position[3];
        output_position[0] = ma_pos[0]->get();
        output_position[1] = ma_pos[1]->get();
        output_position[2] = ma_pos[2]->get();

        // Batch process all DWM_MA_BUFFER_SIZE simulation steps
        dwm_ma_process_interpolated(dwm_ma, input_buffers, input_positions_ptr, input_active_counter, out_config, 1.0f,
                                    buffers, output_position);

        // Mute all unused output channels
        for (int i = out_layout->channel_count; i < DWM_MA_MAX_OUTPUT_COUNT; i++)
            juce_buffer.clear(i, 0, DWM_MA_BUFFER_SIZE);
    }

    double getTailLengthSeconds() const override { return 0.0; }

    bool acceptsMidi() const override { return false; }

    bool producesMidi() const override { return false; }

    juce::AudioProcessorEditor *createEditor() override { return new juce::GenericAudioProcessorEditor(*this); }

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

    void *dwm_ma;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(plugin_processor)
};

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() { return new plugin_processor(); }
