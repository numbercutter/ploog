#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace {
    juce::String midiNoteNumberToName(int noteNumber) {
        static const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        int octave = (noteNumber / 12) - 2;
        juce::String name = noteNames[noteNumber % 12];
        return name + juce::String(octave);
    }
}

MidiVolumeGateAudioProcessorEditor::MidiVolumeGateAudioProcessorEditor(MidiVolumeGateAudioProcessor& p)
    : AudioProcessorEditor(&p), 
      processor(p),
      keyboardComponent(keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    // Add keyboard
    keyboardState.addListener(this);
    addAndMakeVisible(keyboardComponent);
    keyboardComponent.setKeyWidth(16.0f);
    keyboardComponent.setAvailableRange(36, 96); // C2 to C7
    
    // Make keyboard respond to incoming MIDI
    keyboardComponent.setMidiChannel(0);  // Listen to all MIDI channels
    keyboardComponent.setMidiChannelsToDisplay(0xffff);  // Display all MIDI channels

    // Set up trigger note control
    triggerNoteSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    triggerNoteSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
    triggerNoteSlider.setColour(juce::Slider::textBoxTextColourId, textColour);
    triggerNoteSlider.setColour(juce::Slider::rotarySliderFillColourId, accentColour);
    addAndMakeVisible(triggerNoteSlider);

    triggerNoteLabel.setText("Trigger Note", juce::dontSendNotification);
    triggerNoteLabel.setColour(juce::Label::textColourId, textColour);
    triggerNoteLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(triggerNoteLabel);

    // Create parameter attachment
    triggerNoteAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.parameters, "triggerNote", triggerNoteSlider);

    // Set up status labels
    gateStatusLabel.setColour(juce::Label::textColourId, textColour);
    gateStatusLabel.setJustificationType(juce::Justification::left);
    addAndMakeVisible(gateStatusLabel);

    audioStatusLabel.setColour(juce::Label::textColourId, textColour);
    audioStatusLabel.setJustificationType(juce::Justification::left);
    addAndMakeVisible(audioStatusLabel);

    midiStatusLabel.setColour(juce::Label::textColourId, textColour);
    midiStatusLabel.setJustificationType(juce::Justification::left);
    addAndMakeVisible(midiStatusLabel);

    // Set up debug display
    debugDisplay.setMultiLine(true);
    debugDisplay.setReadOnly(true);
    debugDisplay.setColour(juce::TextEditor::backgroundColourId, backgroundColour.darker());
    debugDisplay.setColour(juce::TextEditor::textColourId, textColour);
    debugDisplay.setFont(juce::Font("Courier New", 12.0f, juce::Font::plain));
    addAndMakeVisible(debugDisplay);

    // Make window taller to accommodate keyboard
    setSize(400, 600);
    startTimerHz(30);
}

MidiVolumeGateAudioProcessorEditor::~MidiVolumeGateAudioProcessorEditor()
{
    keyboardState.removeListener(this);
    stopTimer();
}

void MidiVolumeGateAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(backgroundColour);

    // Draw title
    g.setColour(textColour);
    g.setFont(20.0f);
    g.drawFittedText("MIDI Volume Gate", getLocalBounds().removeFromTop(40),
                     juce::Justification::centred, 1);
}

void MidiVolumeGateAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(10);
    
    // Title area
    area.removeFromTop(40);

    // Controls section
    auto controlsArea = area.removeFromTop(100);
    auto sliderArea = controlsArea.removeFromRight(controlsArea.getWidth() - 100);
    triggerNoteLabel.setBounds(controlsArea);
    triggerNoteSlider.setBounds(sliderArea);

    // Status section
    auto statusArea = area.removeFromTop(80);
    gateStatusLabel.setBounds(statusArea.removeFromTop(25));
    audioStatusLabel.setBounds(statusArea.removeFromTop(25));
    midiStatusLabel.setBounds(statusArea.removeFromTop(25));

    // Add keyboard at bottom
    auto keyboardArea = area.removeFromBottom(80);
    keyboardComponent.setBounds(keyboardArea);

    // Debug area
    debugDisplay.setBounds(area);
}

void MidiVolumeGateAudioProcessorEditor::timerCallback()
{
    // Add this at the start of timerCallback
    if (!processor.midiMessages.isEmpty())
    {
        // Update keyboard state with incoming MIDI
        for (const auto metadata : processor.midiMessages)
        {
            auto message = metadata.getMessage();
            if (message.isNoteOn() || message.isNoteOff())
            {
                keyboardState.processNextMidiEvent(message);
            }
        }
        processor.midiMessages.clear();
    }

    auto state = processor.getState();
    
    // Update gate status
    bool gateOpen = processor.gateLevel > 0.0f;
    gateStatusLabel.setText("Gate: " + juce::String(gateOpen ? "OPEN" : "CLOSED"),
                          juce::dontSendNotification);
    gateStatusLabel.setColour(juce::Label::textColourId,
                             gateOpen ? juce::Colours::green : juce::Colours::red);

    // Update audio status
    bool isProcessing = processor.processBlockCallCount != lastProcessBlockCount;
    lastProcessBlockCount = processor.processBlockCallCount;
    audioStatusLabel.setText("Audio: " + juce::String(isProcessing ? "RUNNING" : "STOPPED"),
                           juce::dontSendNotification);
    audioStatusLabel.setColour(juce::Label::textColourId,
                              isProcessing ? juce::Colours::green : juce::Colours::red);

    // Update MIDI status
    midiStatusLabel.setText("MIDI Events: " + juce::String(state.messageCount),
                          juce::dontSendNotification);

    // Update debug display
    debugDisplay.setText(state.debugInfo);

    repaint();
}

// Add these methods to handle keyboard input
void MidiVolumeGateAudioProcessorEditor::handleNoteOn(juce::MidiKeyboardState*, int midiChannel, 
                                                     int midiNoteNumber, float velocity)
{
    auto message = juce::MidiMessage::noteOn(midiChannel, midiNoteNumber, velocity);
    juce::MidiBuffer buffer;
    buffer.addEvent(message, 0);
    
    // Create a temporary buffer for the audio
    juce::AudioBuffer<float> tempBuffer(2, 512); // 2 channels, 512 samples
    tempBuffer.clear();
    
    // Process the MIDI message
    processor.processBlock(tempBuffer, buffer);
}

void MidiVolumeGateAudioProcessorEditor::handleNoteOff(juce::MidiKeyboardState*, int midiChannel, 
                                                      int midiNoteNumber, float velocity)
{
    auto message = juce::MidiMessage::noteOff(midiChannel, midiNoteNumber);
    juce::MidiBuffer buffer;
    buffer.addEvent(message, 0);
    
    // Create a temporary buffer for the audio
    juce::AudioBuffer<float> tempBuffer(2, 512); // 2 channels, 512 samples
    tempBuffer.clear();
    
    // Process the MIDI message
    processor.processBlock(tempBuffer, buffer);
} 