#pragma once

#include "Parameters.h"
#include "ScopeStrip.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

// Геометрия колонок таблицы живёт в одном месте: и строка, и её шапка
// раскладываются отсюда, иначе они разъезжаются при первой же правке.
struct ChannelColumns
{
    juce::Rectangle<int> enable;
    juce::Rectangle<int> name;
    juce::Rectangle<int> role;
    juce::Rectangle<int> delay;
    juce::Rectangle<int> rotator;
    juce::Rectangle<int> polarity;
    juce::Rectangle<int> corr;
    juce::Rectangle<int> scope;

    static ChannelColumns from(juce::Rectangle<int> row);
};

class ChannelRow final : public juce::Component
{
public:
    // Осциллограмма канала живёт в его же строке, справа от ручек. Поэтому
    // ширина колонок фиксированная (иначе на широком окне разъезжается delay),
    // а окно растёт вниз ровно на kHeight за канал.
    static constexpr int kHeight = 64;
    static constexpr int kControlHeight = 26;
    static constexpr int kEnableWidth = 44;
    static constexpr int kNameWidth = 36;
    static constexpr int kRoleWidth = 84;
    static constexpr int kDelayWidth = 240;
    static constexpr int kRotatorWidth = 148;
    static constexpr int kPolarityWidth = 100;
    static constexpr int kCorrWidth = 56;
    static constexpr int kControlsWidth = kEnableWidth + kNameWidth + kRoleWidth + kDelayWidth
                                          + kRotatorWidth + kPolarityWidth + kCorrWidth;
    static constexpr int kMinScopeWidth = 280;

    ChannelRow(juce::AudioProcessorValueTreeState& state, int channelIndex);

    void resized() override;
    void paint(juce::Graphics&) override;

    void setActive(bool shouldBeActive);
    void setCorrelation(float value);
    void setIsReference(bool isReference);
    void setWaveform(const float* samples, int count);

    juce::Rectangle<int> getScopeBounds() const { return scope.getBounds(); }

private:
    bool active = false;

    juce::ToggleButton enabledButton;
    juce::Label nameLabel;
    juce::ComboBox roleBox;
    juce::Slider delaySlider;
    juce::Slider rotatorSlider;
    juce::ComboBox polarityBox;
    juce::Label corrLabel;
    ScopeStrip scope;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enabledAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> roleAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> delayAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rotatorAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> polarityAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChannelRow)
};
