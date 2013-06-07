/*
 * Copyright (C) 2013 Jolla Mobile <andrew.den.exter@jollamobile.com>
 *
 * You may use this file under the terms of the BSD license as follows:
 *
 * "Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Nemo Mobile nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
 */

#ifndef SYNCHRONIZELISTS_P_H
#define SYNCHRONIZELISTS_P_H

#include <QVector>
#include <QtDebug>

// Helper utility to synchronize a cached list with some reference list with correct
// QAbstractItemModel signals and filtering.

// If the reference list is populated incrementally this can be called multiple times with the
// same variables c and r to progressively synchronize the lists.  If after the final call either
// the c or r index is not equal to the length of the cache or reference lists respectively then
// the remaining items can be synchronized manually by removing the remaining items from the
// cache list before (filtering and) appending the remaining reference items.

template <typename Agent, typename ValueType, typename ReferenceList>
class SynchronizeList
{
public:
    SynchronizeList(
            Agent *agent,
            const QVector<ValueType> &cache,
            int &c,
            const ReferenceList &reference,
            int &r)
        : agent(agent), cache(cache), c(c), reference(reference), r(r)
    {
        while (c < cache.count() && r < reference.count()) {
            if (cache.at(c) == reference.at(r)) {
                ++c;
                ++r;
                continue;
            }

            bool match = false;

            // Iterate through both the reference and cache lists in parallel looking for first
            // point of commonality, when that is found resolve the differences and continue
            // looking.
            int count = 1;
            for (; !match && c + count < cache.count() && r + count < reference.count(); ++count) {
                const ValueType cacheValue = cache.at(c + count);
                const ValueType referenceValue = reference.at(r + count);

                for (int i = 0; i <= count; ++i) {
                    if (cacheMatch(i, count, referenceValue) || referenceMatch(i, count, cacheValue)) {
                        match = true;
                        break;
                    }
                }
            }

            // Continue scanning the reference list if the cache has been exhausted.
            for (int re = r + count; !match && re < reference.count(); ++re) {
                const ValueType referenceValue = reference.at(re);
                for (int i = 0; i < count; ++i) {
                    if (cacheMatch(i, re - r, referenceValue)) {
                        match = true;
                        break;
                    }
                }
            }

            // Continue scanning the cache if the reference list has been exhausted.
            for (int ce = c + count; !match && ce < cache.count(); ++ce) {
                const ValueType cacheValue = cache.at(ce);
                for (int i = 0; i < count; ++i) {
                    if (referenceMatch(i, ce - c, cacheValue)) {
                        match = true;
                        break;
                    }
                }
            }

            if (!match)
                return;
        }
    }

private:
    // Tests if the cached value at i matches a referenceValue.
    // If there is a match removes all items traversed in the cache since the previous match
    // and inserts any items in the reference set found to to not be in the cache.
    bool cacheMatch(int i, int count, ValueType referenceValue)
    {
        if (cache.at(c + i) == referenceValue) {
            if (i > 0)
                c += agent->removeRange(c, i);
            c += agent->insertRange(c, count, reference, r) + 1;
            r += count + 1;
            return true;
        } else {
            return false;
        }
    }

    // Tests if the reference value at i matches a cacheValue.
    // If there is a match inserts all items traversed in the reference set since the
    // previous match and removes any items from the cache that were not found in the
    // reference list.
    bool referenceMatch(int i, int count, ValueType cacheValue)
    {
        if (reference.at(r + i) == cacheValue) {
            c += agent->removeRange(c, count);
            if (i > 0)
                c += agent->insertRange(c, i, reference, r);
            c += 1;
            r += i + 1;
            return true;
        } else {
            return false;
        }
    }

    Agent * const agent;
    const QVector<ValueType> &cache;
    int &c;
    const ReferenceList &reference;
    int &r;
};

template <typename Agent, typename ValueType, typename ReferenceList>
void synchronizeList(
        Agent *agent,
        const QVector<ValueType> &cache,
        int &c,
        const ReferenceList &reference,
        int &r)
{
    SynchronizeList<Agent, ValueType, ReferenceList>(agent, cache, c, reference, r);
}

template <typename Agent, typename ValueType, typename ReferenceList>
class SynchronizeFilteredList
{
public:
    SynchronizeFilteredList(
            Agent *agent,
            const QVector<ValueType> &cache,
            int &c,
            const ReferenceList &reference,
            int &r)
        : cache(cache)
        , agent(agent)
        , previousIndex(0)
        , removeCount(0)
    {
        synchronizeList(this, cache, c, reference, r);

        if (filteredValues.count() > 0) {
            c += filteredValues.count();
            agent->insertRange(previousIndex, filteredValues.count(), filteredValues, 0);
        } else if (removeCount > 0) {
            c -= removeCount;
            agent->removeRange(previousIndex, removeCount);
        }

        for (; previousIndex < c; ++previousIndex) {
            int filterCount = 0;
            for (int i; (i = previousIndex + filterCount) < c; ++filterCount) {
                if (agent->filterValue(cache.at(i)))
                    break;
            }
            if (filterCount > 0) {
                agent->removeRange(previousIndex, filterCount);
                c -= filterCount;
            }
        }
    }

    int insertRange(int index, int count, const QVector<ValueType> &source, int sourceIndex)
    {
        int adjustedIndex = index;

        if (removeCount > 0) {
            adjustedIndex -= removeCount;
            agent->removeRange(previousIndex, removeCount);
            removeCount = 0;
        } else if (filteredValues.count() > 0 && index > previousIndex) {
            agent->insertRange(previousIndex, filteredValues.count(), filteredValues, 0);
            adjustedIndex += filteredValues.count();
            previousIndex += filteredValues.count();
            filteredValues.resize(0);
        }

        if (filteredValues.isEmpty()) {
            for (; previousIndex < adjustedIndex;) {
                int filterCount = 0;
                for (int i; (i = previousIndex + filterCount) < adjustedIndex; ++filterCount) {
                    if (agent->filterValue(cache.at(i)))
                        break;
                }
                if (filterCount > 0) {
                    agent->removeRange(previousIndex, filterCount);
                    adjustedIndex -= filterCount;
                } else {
                    ++previousIndex;
                }
            }
        }

        for (int i = 0; i < count; ++i) {
            const ValueType sourceValue = source.at(sourceIndex + i);
            if (agent->filterValue(sourceValue))
                filteredValues.append(sourceValue);
        }

        return adjustedIndex - index;
    }

    int removeRange(int index, int count)
    {
        int adjustedIndex = index;
        if (filteredValues.count() > 0) {
            adjustedIndex += filteredValues.count();
            agent->insertRange(previousIndex, filteredValues.count(), filteredValues, 0);
            filteredValues.resize(0);
        } else if (removeCount > 0 && adjustedIndex > previousIndex + removeCount) {
            adjustedIndex -= removeCount;
            agent->removeRange(previousIndex, removeCount);
            removeCount = 0;
        }

        if (removeCount == 0) {
            for (; previousIndex < adjustedIndex;) {
                int filterCount = 0;
                for (int i; (i = previousIndex + filterCount) < adjustedIndex; ++filterCount) {
                    if (agent->filterValue(cache.at(i)))
                        break;
                }
                if (previousIndex + filterCount == adjustedIndex) {
                    removeCount += filterCount;
                    break;
                } else if (filterCount > 0) {
                    agent->removeRange(previousIndex, filterCount);
                    adjustedIndex -= filterCount;
                } else {
                    ++previousIndex;
                }
            }
        }

        removeCount += count;

        return adjustedIndex - index + count;
    }

    QVector<ValueType> filteredValues;
    const QVector<ValueType> &cache;
    Agent *agent;
    int previousIndex;
    int removeCount;
};

template <typename Agent, typename ValueType, typename ReferenceList>
void synchronizeFilteredList(
        Agent *agent,
        const QVector<ValueType> &cache,
        int &c,
        const ReferenceList &reference,
        int &r)
{
    SynchronizeFilteredList<Agent, ValueType, ReferenceList>(agent, cache, c, reference, r);
}

#endif
