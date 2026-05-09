#include <algorithm>
#include <iostream>
#include <limits>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

struct Edge {
    string to;
    int weight;
};

struct NodeState {
    int dist;
    string node;

    bool operator<(const NodeState& other) const {
        return dist > other.dist;
    }
};

unordered_map<string, vector<Edge>> buildGraph(int edgeCount) {
    unordered_map<string, vector<Edge>> graph;
    for (int i = 0; i < edgeCount; ++i) {
        string u, v;
        int w;
        cin >> u >> v >> w;
        graph[u].push_back({v, w});
        graph[v].push_back({u, w});
    }
    return graph;
}

vector<string> restorePath(
    const unordered_map<string, string>& prev,
    const string& start,
    const string& end) {
    vector<string> path;
    string cur = end;

    while (!cur.empty()) {
        path.push_back(cur);
        auto it = prev.find(cur);
        if (it == prev.end()) {
            break;
        }
        cur = it->second;
    }

    reverse(path.begin(), path.end());
    if (path.empty() || path.front() != start) {
        return {};
    }
    return path;
}

int main() {
    int edgeCount;
    cout << "请输入边数量: ";
    cin >> edgeCount;
    cout << "每条边输入格式: r1 r2 weight" << endl;

    auto graph = buildGraph(edgeCount);

    string start, end;
    cout << "请输入起点: ";
    cin >> start;
    cout << "请输入终点: ";
    cin >> end;

    unordered_map<string, int> dist;
    unordered_map<string, string> prev;
    for (const auto& item : graph) {
        dist[item.first] = numeric_limits<int>::max();
    }

    priority_queue<NodeState> pq;
    dist[start] = 0;
    pq.push({0, start});

    while (!pq.empty()) {
        NodeState current = pq.top();
        pq.pop();

        if (current.dist != dist[current.node]) {
            continue;
        }

        for (const auto& edge : graph[current.node]) {
            if (dist[current.node] == numeric_limits<int>::max()) {
                continue;
            }

            int newDist = dist[current.node] + edge.weight;
            if (!dist.count(edge.to) || newDist < dist[edge.to]) {
                dist[edge.to] = newDist;
                prev[edge.to] = current.node;
                pq.push({newDist, edge.to});
            }
        }
    }

    vector<string> path = restorePath(prev, start, end);
    if (!dist.count(end) || dist[end] == numeric_limits<int>::max() || path.empty()) {
        cout << "两点之间不可达" << endl;
        return 0;
    }

    cout << "最短距离: " << dist[end] << endl;
    cout << "最短路径: ";
    for (size_t i = 0; i < path.size(); ++i) {
        if (i > 0) {
            cout << " -> ";
        }
        cout << path[i];
    }
    cout << endl;
    return 0;
}
