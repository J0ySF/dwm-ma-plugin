extern "C" {
#include <dwm_ma.h>
}

#include <ambi_bin.h>
#include <array2sh.h>
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
    };

    /**
     * dwm boundary parameters
     */
    const float bounds_params[6][2] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}};

    /**
     * Reconfigure everything
     */
    void reconfigure(const MA_CONFIG ma_conf) {
        dwm_ma_init(dwm_ma, bounds_params, true);

        const ma_layout *layout = ma_config_layout(ma_conf);
        SH_ORDERS order;
        if (layout->channel_count >= 25) {
            order = SH_ORDER_FOURTH;
            ambi_channel_count = 25;
        } else if (layout->channel_count >= 16) {
            order = SH_ORDER_THIRD;
            ambi_channel_count = 16;
        } else if (layout->channel_count >= 9) {
            order = SH_ORDER_SECOND;
            ambi_channel_count = 9;
        } else {
            order = SH_ORDER_FIRST;
            ambi_channel_count = 4;
        }

        array2sh_init(array2sh, DWM_MA_SAMPLE_RATE);
        array2sh_setArrayType(array2sh, ARRAY_SPHERICAL);
        array2sh_setNumSensors(array2sh, DWM_MA_MAX_OUTPUT_COUNT);
        for (int i = 0; i < layout->channel_count; i++) {
            array2sh_setSensorAzi_rad(array2sh, i, layout->mic_azi_elev[i][0]);
            array2sh_setSensorElev_rad(array2sh, i, layout->mic_azi_elev[i][1]);
        }
        array2sh_setNormType(ambi_bin, NORM_SN3D);
        array2sh_setc(array2sh, 343.0f);
        array2sh_setChOrder(array2sh, CH_ACN);
        array2sh_setEncodingOrder(array2sh, order);
        array2sh_setWeightType(array2sh, WEIGHT_OPEN_OMNI);
        array2sh_setr(array2sh, layout->radius_m);
        array2sh_setRegPar(array2sh, 15.0f);
        array2sh_setGain(array2sh, 0.0f);

        ambi_bin_init(ambi_bin, DWM_MA_SAMPLE_RATE);
        ambi_bin_setNormType(ambi_bin, NORM_SN3D);
        ambi_bin_setChOrder(ambi_bin, CH_ACN);
        ambi_bin_setDecodingMethod(ambi_bin, DECODING_METHOD_MAGLS);
        ambi_bin_setHRIRsPreProc(ambi_bin, HRIR_PREPROC_EQ);
        ambi_bin_setInputOrderPreset(ambi_bin, order);
        ambi_bin_initCodec(ambi_bin);

        prev_config = ma_conf;
        reset = false;
    }

public:
    plugin_processor() :
        AudioProcessor(
                BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::discreteChannels(DWM_MA_MAX_INPUT_COUNT), true)
                        .withOutput("Output", juce::AudioChannelSet::discreteChannels(2), true)) {
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
        array2sh_create(&array2sh);
        ambi_bin_create(&ambi_bin);
        reconfigure(MA_CONFIG_6_POINTS_SQRT_1);
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
        return layouts.getMainInputChannels() == DWM_MA_MAX_INPUT_COUNT && layouts.getMainOutputChannels() == 2;
    }

    using AudioProcessor::processBlock;
    void processBlock(juce::AudioBuffer<float> &juce_buffer, juce::MidiBuffer &) override {
        juce::ScopedNoDenormals noDenormals;

        // If the input sample rate, buffer size or channel counts do not satisfy
        // the required specs, clear all output channels and stop the simulation
        if (DWM_MA_SAMPLE_RATE != static_cast<int>(getSampleRate()) ||
            DWM_MA_BUFFER_SIZE != juce_buffer.getNumSamples() || getTotalNumInputChannels() < DWM_MA_MAX_INPUT_COUNT ||
            getTotalNumOutputChannels() < 2) {
            reset = true;
            for (auto i = 0; i < getTotalNumOutputChannels(); i++)
                juce_buffer.clear(i, 0, juce_buffer.getNumSamples());
            return;
        }

        // TODO: ???

        // Handle situations where the simulation needs to restart
        const auto out_config = static_cast<MA_CONFIG>(ma_config->getIndex() + 2); // Skip mono and stereo
        if (reset || out_config != prev_config)
            reconfigure(out_config);
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
        const ma_layout *layout = ma_config_layout(out_config);
        float output_position[3];
        output_position[0] = ma_pos[0]->get();
        output_position[1] = ma_pos[1]->get();
        output_position[2] = ma_pos[2]->get();

        // Intermediate buffers
        float ma_buffer[DWM_MA_MAX_OUTPUT_COUNT][DWM_MA_BUFFER_SIZE];
        float *ma_buffer_ptr[DWM_MA_MAX_OUTPUT_COUNT];
        float ambi_buffer[DWM_MA_MAX_OUTPUT_COUNT][DWM_MA_BUFFER_SIZE];
        float *ambi_buffer_ptr[DWM_MA_MAX_OUTPUT_COUNT];
        for (int i = 0; i < DWM_MA_MAX_OUTPUT_COUNT; i++) {
            ma_buffer_ptr[i] = ma_buffer[i];
            ambi_buffer_ptr[i] = ambi_buffer[i];
        }

        // Batch process all DWM_MA_BUFFER_SIZE simulation steps
        dwm_ma_process_interpolated(dwm_ma, input_buffers, input_positions_ptr, input_active_counter, out_config, 1.0f,
                                    ma_buffer_ptr, output_position);

        // Convert from mic array to ambisonics
        array2sh_process(array2sh, ma_buffer_ptr, ambi_buffer_ptr, layout->channel_count, ambi_channel_count,
                         DWM_MA_BUFFER_SIZE);
        ambi_bin_process(ambi_bin, ambi_buffer_ptr, buffers, ambi_channel_count, 2, DWM_MA_BUFFER_SIZE);
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

    void *dwm_ma{};
    void *array2sh{};
    void *ambi_bin{};
    MA_CONFIG prev_config;
    int ambi_channel_count;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(plugin_processor)
};

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() { return new plugin_processor(); }
