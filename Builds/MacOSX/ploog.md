#include <JuceHeader.h>

class MidiVolumeGateAudioProcessor : public juce::AudioProcessor
{
public:
MidiVolumeGateAudioProcessor()
: parameters(\*this, nullptr, "PARAMETERS", createParameterLayout())
{
// Initialize envelope state
currentEnvelopeLevel = 0.0f;
targetEnvelopeLevel = 0.0f;
}

    ~MidiVolumeGateAudioProcessor() override {}

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override
    {
        // Reset envelope timing based on sample rate
        currentSampleRate = sampleRate;
        updateEnvelopeTimes();
    }

    void releaseResources() override {}

    // Main processing block: process both audio and MIDI
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override
    {
        // Retrieve current parameter values
        auto midiChannelParam = parameters.getRawParameterValue("midiChannel");
        auto noteMinParam     = parameters.getRawParameterValue("noteMin");
        auto noteMaxParam     = parameters.getRawParameterValue("noteMax");
        auto attackParam      = parameters.getRawParameterValue("attack");
        auto holdParam        = parameters.getRawParameterValue("hold");
        auto releaseParam     = parameters.getRawParameterValue("release");

        int midiChannel = static_cast<int>(*midiChannelParam);
        int noteMin     = static_cast<int>(*noteMinParam);
        int noteMax     = static_cast<int>(*noteMaxParam);

        // Update envelope times (in samples) based on parameters and sample rate
        updateEnvelopeTimes(*attackParam, *holdParam, *releaseParam);

        // Look through the incoming MIDI messages
        for (const auto metadata : midiMessages)
        {
            const auto message = metadata.getMessage();
            // Check if message is from the correct MIDI channel
            if (message.getChannel() == midiChannel)
            {
                int noteNumber = message.getNoteNumber();

                // Check if note is within our trigger range
                if (noteNumber >= noteMin && noteNumber <= noteMax)
                {
                    if (message.isNoteOn())
                    {
                        // On note-on, set target envelope to 1 (gate open)
                        targetEnvelopeLevel = 1.0f;
                        // Reset hold timer if needed
                        holdSamplesRemaining = holdTimeSamples;
                    }
                    else if (message.isNoteOff())
                    {
                        // On note-off, schedule envelope to drop to 0 (gate close)
                        targetEnvelopeLevel = 0.0f;
                    }
                }
            }
        }

        // Process the envelope for each sample block
        const int numSamples = buffer.getNumSamples();
        for (int sample = 0; sample < numSamples; ++sample)
        {
            // Simple envelope processing logic:
            if (targetEnvelopeLevel > currentEnvelopeLevel)
            {
                // Attack phase: increase envelope level
                currentEnvelopeLevel += attackIncrement;
                if (currentEnvelopeLevel > targetEnvelopeLevel)
                    currentEnvelopeLevel = targetEnvelopeLevel;
            }
            else if (targetEnvelopeLevel < currentEnvelopeLevel)
            {
                // Hold phase: if hold time is remaining, do nothing
                if (holdSamplesRemaining > 0)
                {
                    --holdSamplesRemaining;
                }
                else
                {
                    // Release phase: decrease envelope level
                    currentEnvelopeLevel -= releaseIncrement;
                    if (currentEnvelopeLevel < targetEnvelopeLevel)
                        currentEnvelopeLevel = targetEnvelopeLevel;
                }
            }

            // Apply the envelope (gate) to each channel in the audio buffer
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                float* sampleData = buffer.getWritePointer(channel);
                sampleData[sample] *= currentEnvelopeLevel;
            }
        }
    }

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override          { return new juce::GenericAudioProcessorEditor (*this); }
    bool hasEditor() const override                              { return true; }

    //==============================================================================
    const juce::String getName() const override                  { return "MidiVolumeGate"; }
    bool acceptsMidi() const override                            { return true; }
    bool producesMidi() const override                           { return false; }
    double getTailLengthSeconds() const override                 { return 0.0; }

    //==============================================================================
    int getNumPrograms() override                                { return 1; }
    int getCurrentProgram() override                             { return 0; }
    void setCurrentProgram (int) override                        {}
    const juce::String getProgramName (int) override             { return {}; }
    void changeProgramName (int, const juce::String&) override   {}

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override
    {
        // Save parameters state
        if (auto state = parameters.copyState())
        {
            std::unique_ptr<juce::XmlElement> xml (state.createXml());
            copyXmlToBinary (*xml, destData);
        }
    }

    void setStateInformation (const void* data, int sizeInBytes) override
    {
        // Restore parameters state
        std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

        if (xmlState.get() != nullptr)
            if (xmlState->hasTagName (parameters.state.getType()))
                parameters.replaceState (juce::ValueTree::fromXml (*xmlState));
    }

private:
//==============================================================================
// Parameter setup using AudioProcessorValueTreeState
juce::AudioProcessorValueTreeState parameters;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        std::vector <std::unique_ptr<juce::RangedAudioParameter>> params;

        // MIDI source and note range
        params.push_back (std::make_unique<juce::AudioParameterInt> ("midiChannel", "MIDI Channel", 1, 16, 1));
        params.push_back (std::make_unique<juce::AudioParameterInt> ("noteMin", "Note Range Min", 0, 127, 60)); // Default C4 (MIDI 60)
        params.push_back (std::make_unique<juce::AudioParameterInt> ("noteMax", "Note Range Max", 0, 127, 60));

        // Envelope parameters (in seconds for simplicity)
        params.push_back (std::make_unique<juce::AudioParameterFloat> ("attack", "Attack", 0.0f, 1.0f, 0.0f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> ("hold", "Hold", 0.0f, 1.0f, 0.0f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> ("release", "Release", 0.0f, 1.0f, 0.0f));

        return { params.begin(), params.end() };
    }

    //==============================================================================
    // Envelope state variables
    float currentEnvelopeLevel = 0.0f;    // Current gain multiplier
    float targetEnvelopeLevel  = 0.0f;    // Target gain (1.0 = open, 0.0 = closed)

    double currentSampleRate = 44100.0;

    // Envelope timing in samples
    int attackTimeSamples  = 0;
    int holdTimeSamples    = 0;
    int releaseTimeSamples = 0;

    // Pre-calculated per-sample increments for envelope transitions
    float attackIncrement  = 0.0f;
    float releaseIncrement = 0.0f;

    // Hold counter
    int holdSamplesRemaining = 0;

    // Update envelope timing and increments based on parameter values and sample rate
    void updateEnvelopeTimes(float attackSeconds = 0.0f, float holdSeconds = 0.0f, float releaseSeconds = 0.0f)
    {
        attackTimeSamples  = static_cast<int> (attackSeconds  * currentSampleRate);
        holdTimeSamples    = static_cast<int> (holdSeconds    * currentSampleRate);
        releaseTimeSamples = static_cast<int> (releaseSeconds * currentSampleRate);

        // Avoid division by zero; if time is zero, make the increment instantaneous
        attackIncrement  = (attackTimeSamples  > 0) ? (1.0f / attackTimeSamples)  : 1.0f;
        releaseIncrement = (releaseTimeSamples > 0) ? (1.0f / releaseTimeSamples) : 1.0f;
    }

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiVolumeGateAudioProcessor)

};

// This creates new instances of the plugin..
juce::AudioProcessor\* JUCE_CALLTYPE createPluginFilter()
{
return new MidiVolumeGateAudioProcessor();
}

Concept: Develop a plugin effect that, when inserted on any channel in a DAW, dynamically controls that channel’s volume gate based on external MIDI note events. The plugin listens for MIDI input from a user-specified source (MIDI channel) and triggers its gating behavior based on a designated note (or note range). Key Features & Functionality: MIDI Source Configuration: MIDI Channel Selection: The plugin provides a UI option to select which external MIDI channel to listen to. This allows the effect to be controlled by any available MIDI source in the DAW Note Filtering: Users can specify a single note or a note range (e.g., C1, or C1–E1) as the trigger. The plugin should filter incoming MIDI events so that only those that match the set key trigger the effect. Volume Gate Behavior: Triggering Mechanism: On receiving a MIDI note-on event (for the specified note/key), the plugin immediately opens the volume gate on the host channel. Conversely, a corresponding note-off event closes the gate. Envelope Parameters: Offer adjustable parameters for attack, hold, and release times. For a “hard” gate effect, defaults can be set to zero or near-zero values, but users should have the flexibility to adjust these settings to shape the gate’s response. Instance Independence & Routing: The plugin is designed as an effect insert, so multiple instances can run simultaneously on different channels. Each instance operates independently, allowing the user to bind different channels to different MIDI trigger sources. Cross-Channel Triggering: For example, one instance on Channel A could be set to trigger on MIDI Channel 1’s C1, while another instance on Channel B might trigger on MIDI Channel 2’s C1. Despite the same note (C1) being used, the plugin instances remain independent due to their distinct MIDI channel assignments. Flexible Mapping: The plugin’s design should allow any MIDI channel and any key to be chosen as the trigger, giving complete control over how various MIDI piano rolls (or controllers) drive different gating effects across the project. Example Use Case: When a MIDI note C1 is played on Channel 1, Instance 1’s gate opens. Simultaneously, if a separate C1 note is played on MIDI Channel 2, Instance 2’s gate opens. Meanwhile, a D1 note on Channel 1 triggers Instance 3. This setup lets the producer control the volume gating of different channels independently, all through distinct key mappings and MIDI channels from one or multiple MIDI sources.
