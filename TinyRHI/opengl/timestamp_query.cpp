#include "command_list.h"
#include "device.h"

#include <algorithm>
#include <utility>

namespace lunalite::rhi {
namespace {

bool timestampQueryRangeValid(const OpenGLTimestampQueryPool& pool, uint32_t first, uint32_t count)
{
    const auto poolSize = static_cast<uint32_t>(pool.queries.size());
    return first <= poolSize && count <= poolSize - first;
}

void markTimestampQueryRangeUnwritten(OpenGLTimestampQueryPool& pool, uint32_t first, uint32_t count)
{
    std::fill(pool.written.begin() + first, pool.written.begin() + first + count, false);
}

} // namespace

TimestampQueryPoolHandle OpenGLDevice::createTimestampQueryPool(const TimestampQueryPoolDesc& desc)
{
    if (desc.count == 0) {
        return {};
    }

    OpenGLTimestampQueryPool pool{};
    pool.queries.resize(desc.count);
    pool.written.resize(desc.count, false);

    if (glCreateQueries != nullptr) {
        glCreateQueries(GL_TIMESTAMP, static_cast<GLsizei>(pool.queries.size()), pool.queries.data());
    } else if (glGenQueries != nullptr) {
        glGenQueries(static_cast<GLsizei>(pool.queries.size()), pool.queries.data());
    } else {
        return {};
    }

    m_timestamp_query_pools.push_back(std::move(pool));
    return makeHandle<TimestampQueryPoolHandle>(m_timestamp_query_pools.size() - 1);
}

void OpenGLDevice::destroyTimestampQueryPool(TimestampQueryPoolHandle pool)
{
    auto* glPool = getTimestampQueryPool(pool);
    if (glPool == nullptr) {
        return;
    }

    glDeleteQueries(static_cast<GLsizei>(glPool->queries.size()), glPool->queries.data());
    glPool->queries.clear();
    glPool->written.clear();
}

bool OpenGLDevice::getTimestampQueryResults(TimestampQueryPoolHandle pool,
                                            uint32_t first,
                                            uint32_t count,
                                            uint64_t* timestamps_ns)
{
    auto* glPool = getTimestampQueryPool(pool);
    if (glPool == nullptr || !timestampQueryRangeValid(*glPool, first, count)) {
        return false;
    }
    if (count == 0) {
        return true;
    }
    if (timestamps_ns == nullptr) {
        return false;
    }

    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t queryIndex = first + i;
        if (!glPool->written[queryIndex]) {
            return false;
        }

        GLuint available = GL_FALSE;
        glGetQueryObjectuiv(glPool->queries[queryIndex], GL_QUERY_RESULT_AVAILABLE, &available);
        if (available != GL_TRUE) {
            return false;
        }
    }

    for (uint32_t i = 0; i < count; ++i) {
        GLuint64 timestamp = 0;
        glGetQueryObjectui64v(glPool->queries[first + i], GL_QUERY_RESULT, &timestamp);
        timestamps_ns[i] = static_cast<uint64_t>(timestamp);
    }

    return true;
}

void OpenGLCommandList::resetTimestampQueries(TimestampQueryPoolHandle pool, uint32_t first, uint32_t count)
{
    auto* glPool = m_device.getTimestampQueryPool(pool);
    if (glPool == nullptr || count == 0 || !timestampQueryRangeValid(*glPool, first, count)) {
        return;
    }

    markTimestampQueryRangeUnwritten(*glPool, first, count);
}

void OpenGLCommandList::writeTimestamp(TimestampQueryPoolHandle pool, uint32_t index)
{
    auto* glPool = m_device.getTimestampQueryPool(pool);
    if (glPool == nullptr || !timestampQueryRangeValid(*glPool, index, 1)) {
        return;
    }

    glQueryCounter(glPool->queries[index], GL_TIMESTAMP);
    glPool->written[index] = true;
}

} // namespace lunalite::rhi
