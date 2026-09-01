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
    juce::Rectangle<int> solo;
    juce::Rectangle<int> mute;
    juce::Rectangle<int> name;
    juce::Rectangle<int> role;
    juce::Rectangle<int> level;
    juce::Rectangle<int> pan;
    juce::Rectangle<int> delay;
    juce::Rectangle<int> rotator;
    juce::Rectangle<int> polarity;
    juce::Rectangle<int> corr;
    juce::Rectangle<int> phase;
    juce::Rectangle<int> scope;

    // Монитор-микс есть только у стенда: внутри хоста уровень и панорама ничего
    // не делают (инвариант 3), поэтому их колонки не занимают места вовсе.
    static ChannelColumns from(juce::Rectangle<int> row, bool withMonitor = true);
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
    static constexpr int kSoloWidth = 28;
    static constexpr int kMuteWidth = 28;
    static constexpr int kNameWidth = 126;
    static constexpr int kRoleWidth = 84;
    static constexpr int kLevelWidth = 150;
    static constexpr int kPanWidth = 132;
    static constexpr int kDelayWidth = 210;
    static constexpr int kRotatorWidth = 132;
    static constexpr int kPolarityWidth = 92;
    static constexpr int kCorrWidth = 56;
    static constexpr int kPhaseWidth = 92;
    static constexpr int kControlsWidth = kEnableWidth + kSoloWidth + kMuteWidth + kNameWidth
                                          + kRoleWidth + kLevelWidth + kPanWidth + kDelayWidth
                                          + kRotatorWidth + kPolarityWidth + kCorrWidth
                                          + kPhaseWidth;
    static constexpr int kMinScopeWidth = 280;

    ChannelRow(juce::AudioProcessorValueTreeState& state, int channelIndex);

    void resized() override;
    void paint(juce::Graphics&) override;

    void setActive(bool shouldBeActive);
    void setCorrelation(float value);
    void setIsReference(bool isReference);
    void setWaveform(const float* samples, int count);
    void setGrid(const beat::grid::Line* lines, int count);
    // Когерентность пары «канал + опора» из последнего Analyze, до и после.
    void setPhaseMatch(float before, float after, bool measured);
    // Пусто — в строке остаётся один номер: в хосте имён дорожек нет.
    void setChannelName(const juce::String& name);
    // Уровень и панорама показываются только когда в стенде есть материал.
    void setMonitorVisible(bool shouldBeVisible);

    juce::Rectangle<int> getScopeBounds() const { return scope.getBounds(); }
    juce::String getLabelText() const { return nameLabel.getText(); }
    juce::String getPhaseText() const { return phaseLabel.getText(); }

private:
    int index = 0;
    bool active = false;
    bool monitorVisible = false;

    juce::ToggleButton enabledButton;
    juce::TextButton soloButton { "S" };
    juce::TextButton muteButton { "M" };
    juce::Label nameLabel;
    juce::ComboBox roleBox;
    juce::Slider levelSlider;
    juce::Slider panSlider;
    juce::Slider delaySlider;
    juce::Slider rotatorSlider;
    juce::ComboBox polarityBox;
    juce::Label corrLabel;
    juce::Label phaseLabel;
    ScopeStrip scope;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enabledAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> soloAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> muteAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> roleAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> levelAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> panAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> delayAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rotatorAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> polarityAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChannelRow)
};
