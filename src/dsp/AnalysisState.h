#pragma once

#include "AlignmentEngine.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace beat
{

// Версия формата блоба последних оценок. Поднимать при любой правке полей:
// чужой блоб лучше не читать вовсе, чем прочитать со сдвигом.
inline constexpr std::int32_t kAnalysisStateVersion = 1;

// Имя узла в состоянии плагина. Хост хранит его вместе с параметрами.
inline constexpr const char* kAnalysisStateTag = "analysis";

std::vector<std::uint8_t> serializeAnalysis(const AlignmentEngine::Result& result,
                                            double sampleRate);

// Возвращает false, если версия чужая, буфер обрезан или каналов стало другое
// количество: тогда состояние молча не восстанавливается, а не врёт.
bool deserializeAnalysis(const std::uint8_t* data,
                         std::size_t size,
                         int expectedChannels,
                         AlignmentEngine::Result& result,
                         double& sampleRate);

} // namespace beat
