#include "doc/Event.h"

namespace beat::doc
{

int observationCount(const Event& event)
{
    int count = 0;
    for (const auto& observation : event.channels)
        if (observation.present)
            ++count;

    return count;
}

} // namespace beat::doc
