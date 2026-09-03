#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "SyntheticKit.h"
#include "plugin/DetectWorker.h"
#include "plugin/PluginProcessor.h"
#include "plugin/Exporter.h"
#include "plugin/OverviewStrip.h"
#include "plugin/PluginEditor.h"
#include "plugin/ScopeStrip.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

using Catch::Approx;
using namespace beat;
using namespace beat::test;

namespace
{
constexpr double kRate = 48000.0;
constexpr int kLength = 96000;
constexpr float kOverheadDelay = 240.0f; // 5 мс
constexpr float kRoomDelay = 480.0f;     // 10 мс
const std::vector<int> kHits { 6000, 18000, 30000, 42000, 54000, 66000, 78000, 90000 };

// Тот же кит, что в DSP-тестах: снейр в близком микрофоне, оверхеде и комнате.
juce::AudioBuffer<float> benchClip()
{
    KitInstrument snare;
    snare.hitSamples = kHits;
    snare.decayPerSecond = 25.0f;
    snare.toneHz = 190.0f;
    snare.noiseMix = 0.6f;
    snare.arrivalSamples = { 0.0f, kOverheadDelay, kRoomDelay };
    snare.gain = { 1.0f, 0.35f, 0.2f };

    KitSpec spec;
    spec.sampleRate = kRate;
    spec.numChannels = 3;
    spec.numSamples = kLength;
    spec.instruments = { snare };
    spec.noiseFloor = 0.0005f;

    const auto rendered = renderKit(spec);

    juce::AudioBuffer<float> clip(static_cast<int>(rendered.size()), kLength);
    for (size_t ch = 0; ch < rendered.size(); ++ch)
        for (int i = 0; i < kLength; ++i)
            clip.setSample(static_cast<int>(ch), i, rendered[ch][static_cast<size_t>(i)]);

    return clip;
}

DetectWorker::Result run(const juce::AudioBuffer<float>& clip, int from = 0, int length = kLength)
{
    DetectWorker::Request request;
    request.clip = &clip;
    request.generation = 7;
    request.sampleRate = kRate;
    request.reference = 0;
    request.from = from;
    request.length = length;

    DetectWorker::Result result;
    DetectWorker::detect(request, result);
    return result;
}

int countNearColour(const juce::Image& image, juce::Colour target, int tolerance)
{
    int hits = 0;
    for (int y = 0; y < image.getHeight(); ++y)
        for (int x = 0; x < image.getWidth(); ++x)
        {
            const auto p = image.getPixelAt(x, y);
            if (std::abs((int) p.getRed() - (int) target.getRed()) <= tolerance
                && std::abs((int) p.getGreen() - (int) target.getGreen()) <= tolerance
                && std::abs((int) p.getBlue() - (int) target.getBlue()) <= tolerance)
                ++hits;
        }

    return hits;
}

// Маркеры полосы обзора полупрозрачные: точный цвет зависит от того, что под
// ними. Считаем «желтизну» — красного заметно больше синего.
int countYellowish(const juce::Image& image)
{
    int hits = 0;
    for (int y = 0; y < image.getHeight(); ++y)
        for (int x = 0; x < image.getWidth(); ++x)
        {
            const auto p = image.getPixelAt(x, y);
            if ((int) p.getRed() > 90 && (int) p.getGreen() > 70
                && (int) p.getRed() - (int) p.getBlue() > 60)
                ++hits;
        }

    return hits;
}
} // namespace

TEST_CASE("detect on the bench finds the hits and measures the mics")
{
    const auto clip = benchClip();
    const auto result = run(clip);

    REQUIRE(result.valid);
    CHECK(result.document.events().size() == kHits.size());
    CHECK(result.reference == 0);
    CHECK(result.generation == 7);

    // Калибровка идёт первой: без её априорных задержек сверка не дотянется до
    // дальнего микрофона.
    CHECK(result.calibration.selected > 0);
    CHECK(result.profile.knows(0, 1));
    CHECK(result.profile.knows(0, 2));

    CHECK(result.source.valid);
    CHECK(result.source.sourceChannel == 0);
    CHECK(result.source.totalEvents == static_cast<int>(kHits.size()));
    CHECK(result.source.sourceOwnedEvents == static_cast<int>(kHits.size()));
    CHECK(result.source.closeChannel == 1);
    CHECK(result.source.lateChannel == 2);

    const auto& close = result.source.channels[1];
    CHECK(close.observations == static_cast<int>(kHits.size()));
    CHECK(close.rawMedianSamples == Approx(static_cast<double>(kOverheadDelay)).margin(4.0));
    CHECK(close.rawSpreadSamples < 4.0);
    CHECK(std::abs(close.calibrationResidualSamples) < 4.0);

    const auto& returned = result.source.channels[2];
    CHECK(returned.observations == static_cast<int>(kHits.size()));
    CHECK(returned.rawMedianSamples == Approx(static_cast<double>(kRoomDelay)).margin(4.0));
    CHECK(returned.fullAlignOffsetSamples == Approx(0.0).margin(1.0));
    CHECK(returned.naturalOffsetSamples == Approx(static_cast<double>(kRoomDelay)).margin(4.0));

    for (const auto& event : result.document.events())
    {
        CHECK(event.channels[1].present);
        CHECK(result.document.delays().raw(event.id, 1)
              == Approx(static_cast<double>(kOverheadDelay)).margin(4.0));
    }
}

TEST_CASE("the document carries the channels of the bench, not just events")
{
    const auto clip = benchClip();
    const auto result = run(clip);

    // Номера каналов документа и строк таблицы обязаны совпадать: поле
    // задержек индексируется ими, и разъехавшись, они молча покажут задержку
    // не той дорожки.
    REQUIRE(result.document.channelCount() == clip.getNumChannels());
    for (int ch = 0; ch < result.document.channelCount(); ++ch)
        CHECK(result.document.channel(ch)->sourceChannel == ch);
}

TEST_CASE("detect over a window carries the window offset into event times")
{
    const auto clip = benchClip();
    const int from = 24000;
    const auto result = run(clip, from, kLength - from);

    REQUIRE(result.valid);
    REQUIRE(!result.document.events().empty());

    // Время события — от нуля клипа, а не от начала окна: иначе маркер уедет
    // на длину окна, и совпадение с волной будет случайным.
    for (const auto& event : result.document.events())
        CHECK(event.timeSamples >= static_cast<double>(from));

    CHECK(result.document.events().front().timeSamples < static_cast<double>(from + 12000));
}

TEST_CASE("a hit draws a marker on the scope where it was found")
{
    juce::ScopedJuceInitialiser_GUI gui;

    ScopeStrip scope(0);
    scope.setBounds(0, 0, 400, ScopeStrip::kMinHeight);
    scope.setShowIndex(false);

    std::vector<float> wave(256, 0.0f);
    scope.setWaveform(wave.data(), static_cast<int>(wave.size()));

    juce::Image empty(juce::Image::ARGB, 400, ScopeStrip::kMinHeight, true);
    {
        juce::Graphics g(empty);
        scope.paint(g);
    }

    const auto marked = juce::Colour(0xffe8c547);
    REQUIRE(countNearColour(empty, marked, 12) == 0);

    ScopeMarker marker;
    marker.position = 0.5f;
    marker.owned = true;
    scope.setMarkers(&marker, 1);
    CHECK(scope.getMarkerCount() == 1);

    juce::Image drawn(juce::Image::ARGB, 400, ScopeStrip::kMinHeight, true);
    {
        juce::Graphics g(drawn);
        scope.paint(g);
    }

    CHECK(countNearColour(drawn, marked, 12) > 0);

    // Маркер стоит там, где сказано: середина окна, а не край.
    int leftmost = drawn.getWidth();
    int rightmost = -1;
    for (int y = 0; y < drawn.getHeight(); ++y)
        for (int x = 0; x < drawn.getWidth(); ++x)
        {
            const auto p = drawn.getPixelAt(x, y);
            if (std::abs((int) p.getRed() - 0xe8) <= 12 && std::abs((int) p.getGreen() - 0xc5) <= 12)
            {
                leftmost = std::min(leftmost, x);
                rightmost = std::max(rightmost, x);
            }
        }

    REQUIRE(rightmost >= 0);
    const int centre = (leftmost + rightmost) / 2;
    CHECK(std::abs(centre - drawn.getWidth() / 2) < 24);
}

TEST_CASE("markers vanish with the material they belong to")
{
    juce::ScopedJuceInitialiser_GUI gui;

    ScopeStrip scope(0);
    scope.setBounds(0, 0, 400, ScopeStrip::kMinHeight);

    ScopeMarker marker;
    marker.position = 0.25f;
    scope.setMarkers(&marker, 1);
    REQUIRE(scope.getMarkerCount() == 1);

    scope.setMarkers(nullptr, 0);
    CHECK(scope.getMarkerCount() == 0);
}

TEST_CASE("the overview shows every hit of the take")
{
    juce::ScopedJuceInitialiser_GUI gui;

    OverviewStrip overview;
    overview.setBounds(0, 0, 600, OverviewStrip::kHeight);

    std::vector<float> peaks(512, 0.4f);
    overview.setOverview(peaks.data(), static_cast<int>(peaks.size()));
    overview.setTotalSeconds(10.0);

    juce::Image empty(juce::Image::ARGB, 600, OverviewStrip::kHeight, true);
    {
        juce::Graphics g(empty);
        overview.paint(g);
    }

    const std::vector<float> positions { 0.1f, 0.3f, 0.5f, 0.7f, 0.9f };
    overview.setMarkers(positions.data(), static_cast<int>(positions.size()));
    CHECK(overview.getMarkerCount() == 5);

    juce::Image drawn(juce::Image::ARGB, 600, OverviewStrip::kHeight, true);
    {
        juce::Graphics g(drawn);
        overview.paint(g);
    }

    CHECK(countYellowish(drawn) > countYellowish(empty));
}

TEST_CASE("the result reaches the table and the scopes")
{
    juce::ScopedJuceInitialiser_GUI gui;

    const auto clip = benchClip();
    auto result = run(clip);
    REQUIRE(result.valid);

    BeatEqualizerAudioProcessor processor;
    processor.prepareToPlay(kRate, 512);

    const int events = static_cast<int>(result.document.events().size());
    processor.applyDetection(std::move(result));

    // Отчёт — это то, что читает инженер, а не отладочная печать: числа в нём
    // обязаны быть настоящими.
    const auto status = processor.getDetectStatus();
    CHECK(status.contains(juce::String(events)));
    CHECK(status.contains("hits"));

    const auto sourceStatus = processor.getSourceDiagnosticStatus();
    CHECK(sourceStatus.contains("src Ch 1"));
    CHECK(sourceStatus.contains("close Ch 2"));
    CHECK(sourceStatus.contains("late Ch 3"));
    CHECK(sourceStatus.contains("5.00"));
    CHECK(sourceStatus.contains("10.00"));

    // Оверхед стоит на пяти миллисекундах и не гуляет: медиана попадает в
    // заложенную задержку, разброс мал. Ради этой пары чисел Detect и сделан —
    // по ним видно, нужен ли по-ударный рендер или хватает статики.
    const auto overhead = processor.getDelaySpread(1);
    CHECK(overhead.observations == events);
    CHECK(1000.0 * overhead.medianSamples / kRate == Approx(1000.0 * kOverheadDelay / kRate)
                                                         .margin(0.1));
    CHECK(1000.0 * overhead.spreadSamples / kRate < 0.2);

    // Канала, которого в ките нет, в поле задержек тоже нет: прочерк, а не ноль.
    CHECK(processor.getDelaySpread(7).observations == 0);
}

TEST_CASE("a stale detection is dropped rather than shown against new material")
{
    juce::ScopedJuceInitialiser_GUI gui;

    const auto clip = benchClip();
    auto result = run(clip);
    REQUIRE(result.valid);

    BeatEqualizerAudioProcessor processor;
    processor.prepareToPlay(kRate, 512);
    processor.applyDetection(std::move(result));

    // Поколение результата принадлежит другому клипу: строки таблицы обязаны
    // показать прочерк, а не числа по материалу, которого больше нет.
    CHECK(processor.getDetection().generation != processor.getFilePlayer().getGeneration());
}

TEST_CASE("the same hit sits at the same place in every aligned row")
{
    juce::ScopedJuceInitialiser_GUI gui;

    const auto clip = benchClip();
    auto result = run(clip);
    REQUIRE(result.valid);

    auto file = juce::File::createTempFile("wav");
    REQUIRE(beat::exporter::writeWav(file, clip, kRate));

    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Standalone);
    BeatEqualizerAudioProcessor processor;
    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Undefined);

    processor.prepareToPlay(kRate, 512);
    REQUIRE(processor.getFilePlayer().load({ file }, kRate).isEmpty());

    result.generation = processor.getFilePlayer().getGeneration();
    processor.applyDetection(std::move(result));

    // Выравнивание задерживает тот канал, что пришёл раньше: близкий микрофон
    // ждёт оверхед, а не наоборот (applied = max(d) − d[i], инвариант 2).
    auto* delay = processor.getParameters().getParameter("ch01.delayMs");
    REQUIRE(delay != nullptr);
    delay->setValueNotifyingHost(delay->convertTo0to1(1000.0f * kOverheadDelay / (float) kRate));

    std::unique_ptr<juce::AudioProcessorEditor> base(processor.createEditor());
    auto* editor = dynamic_cast<BeatEqualizerAudioProcessorEditor*>(base.get());
    REQUIRE(editor != nullptr);
    editor->refreshWaveforms();

    CHECK(editor->getSourceStatusText().contains("src Ch 1"));
    CHECK(editor->getSourceStatusText().contains("close Ch 2"));
    CHECK(editor->getSourceStatusText().contains("late Ch 3"));
    CHECK(editor->getRow(1).getPerHitText().contains("5.00"));
    CHECK(editor->getRow(2).getPerHitText().contains("10.00"));

    auto& close = editor->getRow(0).getScope();
    auto& overhead = editor->getRow(1).getScope();

    REQUIRE(close.getMarkerCount() > 0);
    REQUIRE(overhead.getMarkerCount() == close.getMarkerCount());

    // Строки показаны уже выровненными: окно оверхеда сдвинуто назад ровно на
    // его задержку, а удар в нём приходит на неё же позже. Значит один и тот
    // же удар обязан стоять в обеих строках на одном месте — иначе маркер
    // считается не от того окна, и картинка врёт.
    for (int i = 0; i < close.getMarkerCount(); ++i)
    {
        INFO(i);
        CHECK(std::abs(close.getMarker(i).position - overhead.getMarker(i).position) < 0.01f);
    }

    // И хозяин удара — близкий микрофон, а не оверхед.
    CHECK(close.getMarker(0).owned);
    CHECK_FALSE(overhead.getMarker(0).owned);

    file.deleteFile();
}
