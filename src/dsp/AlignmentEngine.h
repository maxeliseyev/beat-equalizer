#pragma once

#include "AlignmentSnapshot.h"
#include "Coherence.h"
#include "Constants.h"
#include "GccPhat.h"

#include <array>
#include <vector>

namespace beat
{

struct AnalysisRequest
{
    double sampleRate = 48000.0;
    float maxDistanceM = kDefaultMaxDistanceM;
    int reference = 0;
};

enum class AnalysisStatus
{
    idle = 0,
    ok,
    tooQuiet,      // материал есть, но тише порога — «проиграйте материал»
    notEnoughData, // в буфере меньше кадра
    badRequest
};

struct ChannelEstimate
{
    // Сырой TDOA относительно опоры: > 0 — канал позже опоры.
    float tdoaSamples = 0.0f;
    bool invert = false;
    float confidence = 0.0f;
    int framesUsed = 0;
    bool valid = false;

    // Когерентность суммы с опорой на самом громком кадре: до и после того,
    // что предлагает Analyze (задержка, полярность, ротатор).
    float coherenceBefore = 0.0f;
    float coherenceAfter = 0.0f;
    float rotatorHz = kDefaultRotatorHz;
    float rotatorAmount = 0.0f;
};

// Кадры кольцевого буфера → GCC-PHAT на пару (опора, канал) → медиана лага,
// мажоритарная полярность → snapshot с applied >= 0 и моделью latency.
class AlignmentEngine
{
public:
    struct Result
    {
        AnalysisStatus status = AnalysisStatus::idle;
        int numChannels = 0;
        int reference = 0;
        int framesTotal = 0;
        int framesLoud = 0;
        float coherenceBefore = 0.0f;
        float coherenceAfter = 0.0f;
        std::array<ChannelEstimate, kMaxChannels> channels {};
        AlignmentSnapshot snapshot {};
    };

    explicit AlignmentEngine(int fftOrder = kDefaultFftOrder);

    int frameSize() const { return frame; }
    int hopSize() const { return frame / 2; }

    // channels — channel-major, у каждого канала numSamples сэмплов.
    Result analyze(const float* const* channels,
                   int numChannels,
                   int numSamples,
                   const AnalysisRequest& request);

private:
    void measureAndRotate(const float* const* channels,
                          int loudestFrame,
                          const AnalysisRequest& request,
                          Result& result);

    GccPhat gcc;
    Coherence coherence;
    int frame = 0;
    std::array<std::vector<float>, kMaxChannels> lagsPerChannel {};
    std::array<std::vector<float>, kMaxChannels> ratiosPerChannel {};
    std::array<int, kMaxChannels> invertVotes {};
};

} // namespace beat
