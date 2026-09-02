#include "doc/EditLog.h"

namespace beat::doc
{

std::int64_t EditLog::append(EditEntry entry)
{
    entry.sequence = nextSequence++;
    items.push_back(std::move(entry));
    return items.back().sequence;
}

void EditLog::clear()
{
    items.clear();
    // Номера не переиспользуются: журнал ссылается на них извне.
}

std::vector<EditEntry> EditLog::mistakes() const
{
    std::vector<EditEntry> out;
    for (const auto& entry : items)
        if (entry.verdict == EditVerdict::detectorWasWrong)
            out.push_back(entry);

    return out;
}

} // namespace beat::doc
