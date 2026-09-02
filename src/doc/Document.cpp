#include "doc/Document.h"

#include <algorithm>
#include <utility>

namespace beat::doc
{

SourceId Document::addSource(Source source)
{
    source.id = nextSourceId++;
    sourceList.push_back(std::move(source));
    return sourceList.back().id;
}

const Source* Document::source(SourceId id) const
{
    const auto it = std::find_if(sourceList.begin(),
                                 sourceList.end(),
                                 [id](const Source& s) { return s.id == id; });

    return it == sourceList.end() ? nullptr : &*it;
}

int Document::addChannel(Channel channel)
{
    if (channelCount() >= kMaxChannels)
        return kInvalidId;

    channel.index = channelCount();
    channelList.push_back(std::move(channel));
    return channelList.back().index;
}

const Channel* Document::channel(int index) const
{
    if (index < 0 || index >= channelCount())
        return nullptr;

    return &channelList[static_cast<size_t>(index)];
}

EventId Document::addEvent(Event event)
{
    event.id = nextEventId++;

    const auto at = std::upper_bound(eventList.begin(),
                                     eventList.end(),
                                     event.timeSamples,
                                     [](SamplePos t, const Event& e)
                                     { return t < e.timeSamples; });

    const auto inserted = eventList.insert(at, std::move(event));
    return inserted->id;
}

const Event* Document::event(EventId id) const
{
    const auto it = std::find_if(eventList.begin(),
                                 eventList.end(),
                                 [id](const Event& e) { return e.id == id; });

    return it == eventList.end() ? nullptr : &*it;
}

Event* Document::event(EventId id)
{
    return const_cast<Event*>(std::as_const(*this).event(id));
}

bool Document::removeEvent(EventId id)
{
    const auto it = std::find_if(eventList.begin(),
                                 eventList.end(),
                                 [id](const Event& e) { return e.id == id; });

    if (it == eventList.end())
        return false;

    eventList.erase(it);
    delayField.eraseEvent(id);
    return true;
}

void Document::clearEvents()
{
    eventList.clear();
    delayField.clear();
    // Номера не переиспользуются: журнал правок ссылается на них.
}

double Document::sampleRate() const
{
    return sourceList.empty() ? 0.0 : sourceList.front().sampleRate;
}

void Document::clear()
{
    clearEvents();
    sourceList.clear();
    channelList.clear();
    featureCache.clear();
    editLog.clear();
    detector = {};
    nextSourceId = 0;
    nextEventId = 0;
}

} // namespace beat::doc
