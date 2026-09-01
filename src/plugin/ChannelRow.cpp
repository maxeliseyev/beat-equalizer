#include "ChannelRow.h"

ChannelColumns ChannelColumns::from(juce::Rectangle<int> row, bool withMonitor)
{
    ChannelColumns columns;
    columns.enable = row.removeFromLeft(ChannelRow::kEnableWidth);
    columns.solo = row.removeFromLeft(ChannelRow::kSoloWidth);
    columns.mute = row.removeFromLeft(ChannelRow::kMuteWidth);
    columns.name = row.removeFromLeft(ChannelRow::kNameWidth);
    columns.role = row.removeFromLeft(ChannelRow::kRoleWidth);

    if (withMonitor)
    {
        columns.level = row.removeFromLeft(ChannelRow::kLevelWidth);
        columns.pan = row.removeFromLeft(ChannelRow::kPanWidth);
    }

    columns.delay = row.removeFromLeft(ChannelRow::kDelayWidth);
    columns.rotator = row.removeFromLeft(ChannelRow::kRotatorWidth);
    columns.polarity = row.removeFromLeft(ChannelRow::kPolarityWidth);
    columns.corr = row.removeFromLeft(ChannelRow::kCorrWidth);
    columns.phase = row.removeFromLeft(ChannelRow::kPhaseWidth);
    // Осциллограмма забирает весь остаток: она одна тянется по ширине окна.
    columns.scope = row;
    return columns;
}

ChannelRow::ChannelRow(juce::AudioProcessorValueTreeState& state, int channelIndex)
    : index(channelIndex),
      scope(channelIndex)
{
    enabledButton.setClickingTogglesState(true);
    enabledButton.setButtonText({});
    addAndMakeVisible(enabledButton);

    // Solo жёлтым, Mute красным — как на пульте, читается боковым зрением.
    soloButton.setClickingTogglesState(true);
    soloButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffe8c547));
    soloButton.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff14161b));
    addAndMakeVisible(soloButton);

    muteButton.setClickingTogglesState(true);
    muteButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffe06c75));
    muteButton.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff14161b));
    addAndMakeVisible(muteButton);

    nameLabel.setJustificationType(juce::Justification::centredLeft);
    nameLabel.setFont(juce::FontOptions(13.0f));
    addAndMakeVisible(nameLabel);
    setChannelName({});

    roleBox.addItem("-", 1);
    roleBox.addItem("Close", 2);
    roleBox.addItem("OH", 3);
    roleBox.addItem("Room", 4);
    roleBox.addItem("Hats", 5);
    addAndMakeVisible(roleBox);

    levelSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    levelSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 54, 20);
    addChildComponent(levelSlider);

    // Ползунок посередине читается как центр панорамы; подпись рядом говорит,
    // насколько и куда уведён канал.
    panSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    panSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, 20);
    addChildComponent(panSlider);

    delaySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    delaySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 64, 20);
    delaySlider.setNumDecimalPlacesToDisplay(2);
    addAndMakeVisible(delaySlider);

    rotatorSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    // Проценты приходят из самого параметра: attachment перетирает
    // textFromValueFunction слайдера своей версией.
    rotatorSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 20);
    addAndMakeVisible(rotatorSlider);

    polarityBox.addItem("Auto", 1);
    polarityBox.addItem("Positive", 2);
    polarityBox.addItem("Invert", 3);
    addAndMakeVisible(polarityBox);

    corrLabel.setJustificationType(juce::Justification::centredRight);
    corrLabel.setFont(juce::FontOptions(12.0f));
    addAndMakeVisible(corrLabel);

    phaseLabel.setJustificationType(juce::Justification::centredRight);
    phaseLabel.setFont(juce::FontOptions(12.0f));
    addAndMakeVisible(phaseLabel);
    setPhaseMatch(0.0f, 0.0f, false);

    // Номер канала уже стоит в колонке Ch, второй раз внутри осциллограммы не нужен.
    scope.setShowIndex(false);
    addAndMakeVisible(scope);

    enabledAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        state, beat::channelParamId(index, "enabled"), enabledButton);
    soloAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        state, beat::channelParamId(index, "solo"), soloButton);
    muteAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        state, beat::channelParamId(index, "mute"), muteButton);
    roleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        state, beat::channelParamId(index, "role"), roleBox);
    levelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, beat::channelParamId(index, "levelDb"), levelSlider);
    panAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, beat::channelParamId(index, "pan"), panSlider);
    delayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, beat::channelParamId(index, "delayMs"), delaySlider);
    rotatorAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, beat::channelParamId(index, "rotatorAmount"), rotatorSlider);
    polarityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        state, beat::channelParamId(index, "polarity"), polarityBox);

    setActive(false);
    setCorrelation(0.0f);
}

void ChannelRow::resized()
{
    const auto columns = ChannelColumns::from(getLocalBounds(), monitorVisible);

    // Строка выше ручек: их держим по центру, а высоту отдаём осциллограмме.
    const auto centred = [](juce::Rectangle<int> cell, int trimX)
    {
        return cell.withSizeKeepingCentre(cell.getWidth() - 2 * trimX, kControlHeight);
    };

    enabledButton.setBounds(centred(columns.enable, 6));
    soloButton.setBounds(centred(columns.solo, 2));
    muteButton.setBounds(centred(columns.mute, 2));
    nameLabel.setBounds(centred(columns.name, 4));
    roleBox.setBounds(centred(columns.role, 2));

    if (monitorVisible)
    {
        levelSlider.setBounds(centred(columns.level, 4));
        panSlider.setBounds(centred(columns.pan, 4));
    }

    delaySlider.setBounds(centred(columns.delay, 4));
    rotatorSlider.setBounds(centred(columns.rotator, 4));
    polarityBox.setBounds(centred(columns.polarity, 2));
    corrLabel.setBounds(centred(columns.corr, 0));
    phaseLabel.setBounds(centred(columns.phase, 4));
    scope.setBounds(columns.scope);
}

void ChannelRow::paint(juce::Graphics& g)
{
    g.setColour(active ? juce::Colour(0xff1e222a) : juce::Colour(0xff14161b));
    g.fillRect(getLocalBounds());
}

void ChannelRow::setActive(bool shouldBeActive)
{
    active = shouldBeActive;
    setVisible(shouldBeActive);
    setEnabled(shouldBeActive);
    scope.setActive(shouldBeActive);
    nameLabel.setColour(juce::Label::textColourId,
                        shouldBeActive ? juce::Colours::white : juce::Colour(0xff6b7280));
}

void ChannelRow::setMonitorVisible(bool shouldBeVisible)
{
    if (monitorVisible == shouldBeVisible)
        return;

    monitorVisible = shouldBeVisible;
    levelSlider.setVisible(shouldBeVisible);
    panSlider.setVisible(shouldBeVisible);
    resized();
}

void ChannelRow::setGrid(const beat::grid::Line* lines, int count)
{
    scope.setGrid(lines, count);
}

void ChannelRow::setPhaseMatch(float before, float after, bool measured)
{
    if (!measured)
    {
        phaseLabel.setText("-", juce::dontSendNotification);
        phaseLabel.setColour(juce::Label::textColourId, juce::Colour(0xff6b7280));
        return;
    }

    const auto percent = [](float value)
    { return juce::String(juce::roundToInt(100.0f * juce::jlimit(0.0f, 1.0f, value))); };

    phaseLabel.setText(percent(before) + " -> " + percent(after), juce::dontSendNotification);

    // Зелёное — выравнивание помогло, красное — стало хуже: оценке на этом
    // канале верить нельзя, смотреть руками.
    const float delta = after - before;
    const auto colour = (delta > 0.02f)    ? juce::Colour(0xff7ddc9a)
                        : (delta < -0.02f) ? juce::Colour(0xffe06c75)
                                           : juce::Colour(0xffc5cad3);
    phaseLabel.setColour(juce::Label::textColourId, colour);
}

void ChannelRow::setChannelName(const juce::String& name)
{
    const auto number = juce::String::formatted("%02d", index + 1);
    nameLabel.setText(name.isEmpty() ? number : number + "  " + name,
                      juce::dontSendNotification);
}

void ChannelRow::setCorrelation(float value)
{
    const float clamped = juce::jlimit(-1.0f, 1.0f, value);
    corrLabel.setText(juce::String(clamped, 2), juce::dontSendNotification);

    // Отрицательная корреляция с опорой — это либо полярность, либо промах
    // выравнивания; красным, чтобы бросалось в глаза без чтения цифр.
    corrLabel.setColour(juce::Label::textColourId,
                        clamped < -0.05f ? juce::Colour(0xffe06c75) : juce::Colour(0xffc5cad3));
}

void ChannelRow::setIsReference(bool isReference)
{
    scope.setReference(isReference);

    if (!isReference)
        return;

    corrLabel.setText("ref", juce::dontSendNotification);
    corrLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8b919c));
}

void ChannelRow::setWaveform(const float* samples, int count)
{
    scope.setWaveform(samples, count);
}
