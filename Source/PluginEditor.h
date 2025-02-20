#pragma once

#include "PluginProcessor.h"

class MidiVolumeGateAudioProcessorEditor : public juce::AudioProcessorEditor,
                                          public juce::Timer,
                                          public juce::MidiKeyboardState::Listener
{
public:
    explicit MidiVolumeGateAudioProcessorEditor(MidiVolumeGateAudioProcessor&);
    ~MidiVolumeGateAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
    
    void handleNoteOn(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;
    void handleNoteOff(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;

private:
    MidiVolumeGateAudioProcessor& processor;

    // Add keyboard components
    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardComponent keyboardComponent;

    // Controls
    juce::Slider triggerNoteSlider;
    juce::Label triggerNoteLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> triggerNoteAttachment;

    // Status display
    juce::Label gateStatusLabel;
    juce::Label audioStatusLabel;
    juce::Label midiStatusLabel;

    // Debug display
    juce::TextEditor debugDisplay;
    
    // Styling
    juce::Colour backgroundColour = juce::Colour(40, 40, 40);
    juce::Colour textColour = juce::Colours::white;
    juce::Colour accentColour = juce::Colour(0, 149, 168);
    
    int lastProcessBlockCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiVolumeGateAudioProcessorEditor)
}; 