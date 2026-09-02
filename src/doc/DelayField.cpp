#include "doc/DelayField.h"

#include <algorithm>

namespace beat::doc
{

namespace
{
bool inRange(int channel)
{
    return channel >= 0 && channel < kMaxChannels;
}
} // namespace

void DelayField::setRaw(EventId event, int channel, double tdoaSamples)
{
    if (!inRange(channel))
        return;

    auto& row = rows[event];
    row.tdoa[static_cast<size_t>(channel)] = tdoaSamples;
    row.valid[static_cast<size_t>(channel)] = true;
}

const DelayField::Row* DelayField::find(EventId event) const
{
    const auto it = rows.find(event);
    return it == rows.end() ? nullptr : &it->second;
}

bool DelayField::has(EventId event, int channel) const
{
    if (!inRange(channel))
        return false;

    const auto* row = find(event);
    return row != nullptr && row->valid[static_cast<size_t>(channel)];
}

double DelayField::raw(EventId event, int channel) const
{
    if (!has(event, channel))
        return 0.0;

    return find(event)->tdoa[static_cast<size_t>(channel)];
}

void DelayField::setReturn(int channel, float amount)
{
    if (!inRange(channel))
        return;

    returns[static_cast<size_t>(channel)] = std::clamp(amount, 0.0f, 1.0f);
}

float DelayField::returnFactor(int channel) const
{
    return inRange(channel) ? returns[static_cast<size_t>(channel)] : 0.0f;
}

double DelayField::maxRaw(EventId event) const
{
    const auto* row = find(event);
    if (row == nullptr)
        return 0.0;

    // Ноль в основании: опорный канал ждать самого себя не может, и без него
    // максимум ушёл бы в минус на событии, где все каналы пришли раньше опоры.
    double top = 0.0;
    for (int ch = 0; ch < kMaxChannels; ++ch)
        if (row->valid[static_cast<size_t>(ch)])
            top = std::max(top, row->tdoa[static_cast<size_t>(ch)]);

    return top;
}

double DelayField::applied(EventId event, int channel) const
{
    if (!has(event, channel))
        return 0.0;

    const double d = raw(event, channel);
    const double keep = 1.0 - static_cast<double>(returnFactor(channel));

    return std::max(0.0, maxRaw(event) - keep * d);
}

void DelayField::eraseEvent(EventId event)
{
    rows.erase(event);
}

void DelayField::clear()
{
    rows.clear();
}

int DelayField::eventCount() const
{
    return static_cast<int>(rows.size());
}

} // namespace beat::doc
