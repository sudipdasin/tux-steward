// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QHash>
#include <QVector>
#include <QtGlobal>

struct ProcessTreeIdentity {
    qint64 pid = 0;
    qint64 parentPid = 0;
    quint64 startTicks = 0;
};

// Parent indices for a process snapshot; -1 means a root. A missing,
// newer/reused, duplicate or cyclic parent must never create a false hierarchy.
inline QVector<int> processTreeParents(const QVector<ProcessTreeIdentity> &nodes)
{
    QHash<qint64, int> indices;
    for (int i = 0; i < nodes.size(); ++i) {
        if (indices.contains(nodes[i].pid)) indices[nodes[i].pid] = -1;
        else indices.insert(nodes[i].pid, i);
    }
    QVector<int> parents(nodes.size(), -1);
    for (int i = 0; i < nodes.size(); ++i) {
        const int parent = indices.value(nodes[i].parentPid, -1);
        if (nodes[i].parentPid > 0 && parent >= 0 && parent != i
            && indices.value(nodes[i].pid, -1) == i
            && nodes[parent].startTicks <= nodes[i].startTicks)
            parents[i] = parent;
    }
    QVector<int> state(nodes.size(), 0);
    for (int i = 0; i < nodes.size(); ++i) {
        if (state[i] != 0) continue;
        QVector<int> path;
        int current = i;
        while (current >= 0 && state[current] == 0) {
            state[current] = 1;
            path.append(current);
            current = parents[current];
        }
        if (current >= 0 && state[current] == 1) {
            const int start = path.indexOf(current);
            for (int n = start; n >= 0 && n < path.size(); ++n) parents[path[n]] = -1;
        }
        for (int item : path) state[item] = 2;
    }
    return parents;
}
