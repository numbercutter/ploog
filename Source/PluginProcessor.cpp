#include "PluginProcessor.h"
#include "PluginEditor.h"

MidiVolumeGateAudioProcessor::MidiVolumeGateAudioProcessor()
    : AudioProcessor (BusesProperties()
                     .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                     .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     ),
      parameters(*this, nullptr, "Parameters", createParameterLayout())
{
    setPlayConfigDetails(2, 2, 44100, 512);
    setLatencySamples(0);
}

MidiVolumeGateAudioProcessor::~MidiVolumeGateAudioProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout MidiVolumeGateAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        "triggerNote",
        "Trigger Note",
        0,    // min value
        127,  // max value
        60    // default to middle C
    ));

    return { params.begin(), params.end() };
}

void MidiVolumeGateAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    state.debugInfo = juce::String::formatted(
        "Audio Setup:\n"
        "Sample Rate: %.1f Hz\n"
        "Block Size: %d samples\n"
        "Channels: %d\n",
        sampleRate,
        samplesPerBlock,
        getTotalNumInputChannels());
}

void MidiVolumeGateAudioProcessor::releaseResources()
{
}

void MidiVolumeGateAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, 
                                              juce::MidiBuffer& midiMessages)
{
    state.isAudioRunning = true;
    
    // Store incoming MIDI messages for the editor
    this->midiMessages.clear();
    for (const auto metadata : midiMessages)
    {
        this->midiMessages.addEvent(metadata.getMessage(),
                                  metadata.samplePosition);
    }
    
    // Create a new MIDI buffer for output
    juce::MidiBuffer processedMidi;
    
    // Process MIDI for gate control
    int triggerNote = static_cast<int>(*parameters.getRawParameterValue("triggerNote"));
    
    for (const auto metadata : midiMessages)
    {
        auto message = metadata.getMessage();
        auto samplePosition = metadata.samplePosition;
        
        // Add the message to our output buffer regardless of whether it's our trigger note
        processedMidi.addEvent(message, samplePosition);
        
        if (message.isNoteOn() && message.getNoteNumber() == triggerNote)
        {
            gateLevel = message.getVelocity() / 127.0f;
            state.lastNote = message.getNoteNumber();
            state.wasNoteOn = true;
            state.timestamp = juce::Time::currentTimeMillis();
        }
        else if (message.isNoteOff() && message.getNoteNumber() == triggerNote)
        {
            gateLevel = 0.0f;
            state.wasNoteOn = false;
            state.timestamp = juce::Time::currentTimeMillis();
        }
    }

    // Replace the input MIDI buffer with our processed one
    midiMessages.swapWith(processedMidi);

    // Debug MIDI buffer state
    state.debugInfo = juce::String::formatted(
        "Block: %d\n"
        "MIDI Events: %d\n"
        "Audio Channels: %d\n"
        "Buffer Size: %d\n"
        "Gate Level: %.2f\n"
        "Trigger Note: %d\n",
        processBlockCallCount++,
        midiMessages.getNumEvents(),
        buffer.getNumChannels(),
        buffer.getNumSamples(),
        gateLevel,
        static_cast<int>(*parameters.getRawParameterValue("triggerNote")));

    // Log every MIDI message
    for (const auto metadata : midiMessages)
    {
        auto message = metadata.getMessage();
        state.debugInfo += juce::String::formatted("\nMIDI: [%02X", message.getRawData()[0]);
        for (int i = 1; i < message.getRawDataSize(); ++i)
            state.debugInfo += juce::String::formatted(" %02X", message.getRawData()[i]);
        state.debugInfo += "]";
    }

    // Apply gate to audio
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        float* channelData = buffer.getWritePointer(channel);
        if (gateLevel > 0.0f)
        {
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                channelData[sample] *= gateLevel;
        }
        else
        {
            buffer.clear(channel, 0, buffer.getNumSamples());
        }
    }

    state.currentGateLevel = gateLevel;
    state.messageCount = midiMessages.getNumEvents();
}

void MidiVolumeGateAudioProcessor::handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message)
{
    DBG("Got MIDI message directly: " << message.getDescription());
}

juce::AudioProcessorEditor* MidiVolumeGateAudioProcessor::createEditor()
{
    return new MidiVolumeGateAudioProcessorEditor(*this);
}

void MidiVolumeGateAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void MidiVolumeGateAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
}

bool MidiVolumeGateAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo() ||
        layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MidiVolumeGateAudioProcessor();
} 
