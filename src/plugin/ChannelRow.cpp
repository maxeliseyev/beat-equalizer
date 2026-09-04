#include "ChannelRow.h"

ChannelColumns ChannelColumns::from(juce::Rectangle<int> row,
                                    bool withMonitor,
                                    ChannelTableMode mode)
{
    ChannelColumns columns;
    columns.enable = row.removeFromLeft(ChannelRow::kEnableWidth);
    if (mode == ChannelTableMode::advanced || withMonitor)
    {
        columns.solo = row.removeFromLeft(ChannelRow::kSoloWidth);
        columns.mute = row.removeFromLeft(ChannelRow::kMuteWidth);
    }
    columns.name = row.removeFromLeft(ChannelRow::kNameWidth);

    if (mode == ChannelTableMode::advanced)
        columns.role = row.removeFromLeft(ChannelRow::kRoleWidth);

    if (mode == ChannelTableMode::advanced && withMonitor)
    {
        columns.level = row.removeFromLeft(ChannelRow::kLevelWidth);
        columns.pan = row.removeFromLeft(ChannelRow::kPanWidth);
    }

    if (mode == ChannelTableMode::advanced)
    {
        columns.delay = row.removeFromLeft(ChannelRow::kDelayWidth);
        columns.rotator = row.removeFromLeft(ChannelRow::kRotatorWidth);
        columns.polarity = row.removeFromLeft(ChannelRow::kPolarityWidth);
        columns.corr = row.removeFromLeft(ChannelRow::kCorrWidth);
        columns.phase = row.removeFromLeft(ChannelRow::kPhaseWidth);
        columns.perHit = row.removeFromLeft(ChannelRow::kPerHitWidth);
    }

    // Осциллограмма забирает весь остаток: она одна тянется по ширине окна.
    columns.scope = row;
    return columns;
}

int ChannelRow::controlsWidth(ChannelTableMode mode, bool withMonitor)
{
    int width = kEnableWidth + kNameWidth;

    if (mode == ChannelTableMode::advanced || withMonitor)
        width += kSoloWidth + kMuteWidth;

    if (mode == ChannelTableMode::advanced)
    {
        if (withMonitor)
            width += kLevelWidth + kPanWidth;

        width += kRoleWidth + kDelayWidth + kRotatorWidth + kPolarityWidth + kCorrWidth
                 + kPhaseWidth + kPerHitWidth;
    }

    return width;
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

    // Все четыре регулятора — полосы со значением внутри: ручку с отдельным
    // текстовым полем строка себе позволить не может, окно и так шире экрана.
    // Тянется мышью, двойной клик даёт ввод числа.
    const auto compact = [](juce::Slider& slider)
    {
        slider.setSliderStyle(juce::Slider::LinearBar);
        slider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 10, 10);
        slider.setColour(juce::Slider::trackColourId, juce::Colour(0xff2a3340));
    };

    compact(levelSlider);
    addChildComponent(levelSlider);

    compact(panSlider);
    addChildComponent(panSlider);

    compact(delaySlider);
    delaySlider.setNumDecimalPlacesToDisplay(3);
    addAndMakeVisible(delaySlider);

    // Проценты приходят из самого параметра: attachment перетирает
    // textFromValueFunction слайдера своей версией.
    compact(rotatorSlider);
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

    perHitLabel.setJustificationType(juce::Justification::centredRight);
    perHitLabel.setFont(juce::FontOptions(12.0f));
    perHitLabel.setText("-", juce::dontSendNotification);
    perHitLabel.setColour(juce::Label::textColourId, juce::Colour(0xff6b7280));
    addAndMakeVisible(perHitLabel);
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
    const auto columns = ChannelColumns::from(getLocalBounds(), monitorVisible, tableMode);

    // Строка выше ручек: их держим по центру, а высоту отдаём осциллограмме.
    const auto centred = [](juce::Rectangle<int> cell, int trimX)
    {
        return cell.withSizeKeepingCentre(cell.getWidth() - 2 * trimX, kControlHeight);
    };

    enabledButton.setBounds(centred(columns.enable, 6));
    if (!columns.solo.isEmpty())
        soloButton.setBounds(centred(columns.solo, 2));
    if (!columns.mute.isEmpty())
        muteButton.setBounds(centred(columns.mute, 2));
    nameLabel.setBounds(centred(columns.name, 4));
    if (!columns.role.isEmpty())
        roleBox.setBounds(centred(columns.role, 2));
    if (!columns.level.isEmpty())
    {
        levelSlider.setBounds(centred(columns.level, 4));
        panSlider.setBounds(centred(columns.pan, 4));
    }

    if (!columns.delay.isEmpty())
        delaySlider.setBounds(centred(columns.delay, 4));
    if (!columns.rotator.isEmpty())
        rotatorSlider.setBounds(centred(columns.rotator, 4));
    if (!columns.polarity.isEmpty())
        polarityBox.setBounds(centred(columns.polarity, 2));
    if (!columns.corr.isEmpty())
        corrLabel.setBounds(centred(columns.corr, 0));
    if (!columns.phase.isEmpty())
        phaseLabel.setBounds(centred(columns.phase, 4));
    if (!columns.perHit.isEmpty())
        perHitLabel.setBounds(centred(columns.perHit, 4));
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
    updateColumnVisibility();
    resized();
}

void ChannelRow::setTableMode(ChannelTableMode mode)
{
    if (tableMode == mode)
        return;

    tableMode = mode;
    updateColumnVisibility();
    resized();
}

void ChannelRow::updateColumnVisibility()
{
    const bool advanced = tableMode == ChannelTableMode::advanced;
    const bool showMonitorButtons = advanced || monitorVisible;

    soloButton.setVisible(showMonitorButtons);
    muteButton.setVisible(showMonitorButtons);
    roleBox.setVisible(advanced);
    levelSlider.setVisible(advanced && monitorVisible);
    panSlider.setVisible(advanced && monitorVisible);
    delaySlider.setVisible(advanced);
    rotatorSlider.setVisible(advanced);
    polarityBox.setVisible(advanced);
    corrLabel.setVisible(advanced);
    phaseLabel.setVisible(advanced);
    perHitLabel.setVisible(advanced);
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

void ChannelRow::setPerHitDelay(double medianMs, double spreadMs, int observations)
{
    if (observations <= 0)
    {
        perHitLabel.setText("-", juce::dontSendNotification);
        perHitLabel.setColour(juce::Label::textColourId, juce::Colour(0xff6b7280));
        return;
    }

    perHitLabel.setText(juce::String(medianMs, 2) + " ±" + juce::String(spreadMs, 2),
                        juce::dontSendNotification);

    // Разброс в десятую миллиметра звука — задержка от удара к удару не
    // гуляет, и статического выравнивания хватает. Крупный разброс значит либо
    // что двигать надо по каждому удару, либо что сверка мерила разные удары;
    // отличить одно от другого можно только глазами по маркерам.
    const auto colour = (spreadMs < 0.10) ? juce::Colour(0xff7ddc9a)
                        : (spreadMs < 0.50) ? juce::Colour(0xffc5cad3)
                                            : juce::Colour(0xffe0a45d);
    perHitLabel.setColour(juce::Label::textColourId, colour);
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
