#pragma once

#include <JuceHeader.h>

// Add forward declaration at the top
class MidiVolumeGateAudioProcessorEditor;

class MidiVolumeGateAudioProcessor : public juce::AudioProcessor
{
public:
    MidiVolumeGateAudioProcessor();
    ~MidiVolumeGateAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Plugin state
    juce::AudioProcessorValueTreeState parameters;
    float gateLevel = 0.0f;  // 0 = closed, 1 = open

    // Debug info structure
    struct PluginState {
        int lastNote = -1;
        bool wasNoteOn = false;
        juce::int64 timestamp = 0;
        int messageCount = 0;
        juce::String debugInfo;
        bool isAudioRunning = false;
        float currentGateLevel = 0.0f;
    };

    PluginState getState() const { return state; }

    bool wasProcessBlockCalled = false;  // Flag to show audio theada activity
    int processBlockCallCount = 0;       // Counter for processBlock calls

    bool isMidiEffect() const override { return false; }
    bool canAddBus(bool isInput) const override     { return true; }
    bool canRemoveBus(bool isInput) const override  { return true; }
    bool supportsDoublePrecisionProcessing() const override { return true; }
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message);

    juce::AudioBuffer<float> tempBuffer;

    // Add this to store incoming MIDI messages
    juce::MidiBuffer midiMessages;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    PluginState state;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiVolumeGateAudioProcessor)
};
