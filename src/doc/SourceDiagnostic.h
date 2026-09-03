#pragma once

#include "doc/Document.h"
#include "doc/SessionProfile.h"
#include "dsp/Constants.h"

#include <array>

namespace beat::doc
{

struct SourceChannelDiagnostic
{
    int observations = 0;
    double rawMedianSamples = 0.0;
    double rawSpreadSamples = 0.0;
    double calibrationResidualSamples = 0.0;
    double fullAlignOffsetSamples = 0.0;
    double naturalOffsetSamples = 0.0;
    bool calibrated = false;
};

struct SourceDiagnostic
{
    bool valid = false;
    int sourceChannel = 0;
    int totalEvents = 0;
    int sourceOwnedEvents = 0;
    int sourceObservations = 0;
    int calibratedDelays = 0;
    int closeChannel = -1;
    int lateChannel = -1;
    std::array<SourceChannelDiagnostic, kMaxChannels> channels {};
};

// Source-centric gate перед нарезкой: статистика только по событиям, которыми
// владеет выбранный источник. Так close-пара, bleed и поздние микрофоны не
// смешиваются со средней строкой всех найденных ударов.
SourceDiagnostic buildSourceDiagnostic(const Document& document,
                                       const SessionProfile& profile,
                                       int source,
                                       int channels);

} // namespace beat::doc
