#pragma once

#include "doc/DelayField.h"
#include "doc/EditLog.h"
#include "doc/Event.h"
#include "doc/FeatureCache.h"
#include "doc/Source.h"

#include <string>
#include <vector>

namespace beat::doc
{

// Чем считали. Пишется в проект вместе с результатами: переанализ — явное
// действие пользователя, а не тихое обновление под ним (detector-design 1.6).
struct DetectorStamp
{
    std::string name;
    std::string version;
    std::string parameters;
};

// Модель документа: источники, каналы, события, поле задержек, признаки,
// журнал правок. Без JUCE и без окон — ARA-плагин этапа 4 обязан получиться
// адаптером хоста поверх неё, а не второй такой же моделью.
//
// Звука здесь нет: документ говорит, где он лежит и что про него известно.
class Document
{
public:
    SourceId addSource(Source source);
    const Source* source(SourceId id) const;
    const std::vector<Source>& sources() const { return sourceList; }

    // Возвращает индекс канала документа.
    int addChannel(Channel channel);
    const Channel* channel(int index) const;
    const std::vector<Channel>& channels() const { return channelList; }
    int channelCount() const { return static_cast<int>(channelList.size()); }

    // События хранятся упорядоченными по времени. Указатель живёт до
    // следующего изменения списка; наружу ходить по id.
    EventId addEvent(Event event);
    const Event* event(EventId id) const;
    Event* event(EventId id);
    const std::vector<Event>& events() const { return eventList; }
    bool removeEvent(EventId id);

    // Смена детектора или порогов: решения выбрасываются, признаки остаются.
    // Ради этого признаки и лежат отдельно (detector-design 1.4).
    void clearEvents();

    DelayField& delays() { return delayField; }
    const DelayField& delays() const { return delayField; }

    FeatureCache& features() { return featureCache; }
    const FeatureCache& features() const { return featureCache; }

    EditLog& log() { return editLog; }
    const EditLog& log() const { return editLog; }

    void setDetectorStamp(DetectorStamp stamp) { detector = std::move(stamp); }
    const DetectorStamp& detectorStamp() const { return detector; }

    // Частота источников. Пороги и окна проекта задаются в миллисекундах и
    // метрах (инвариант 4), сюда их переводят только на границе с DSP.
    double sampleRate() const;

    void clear();

private:
    std::vector<Source> sourceList;
    std::vector<Channel> channelList;
    std::vector<Event> eventList;

    DelayField delayField;
    FeatureCache featureCache;
    EditLog editLog;
    DetectorStamp detector;

    SourceId nextSourceId = 0;
    EventId nextEventId = 0;
};

} // namespace beat::doc
