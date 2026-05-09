#include <algorithm>
#include <iostream>
#include <map>
#include <string>

using namespace std;

struct RouteEntry {
    int distance;
    string nextHop;
};

map<string, RouteEntry> readTable(const string& name) {
    int count;
    map<string, RouteEntry> table;
    cout << "请输入" << name << "表项数量: ";
    cin >> count;

    for (int i = 0; i < count; ++i) {
        string network;
        string nextHop;
        int distance;
        cin >> network >> distance >> nextHop;
        table[network] = {distance, nextHop};
    }
    return table;
}

map<string, RouteEntry> updateRipTable(
    const map<string, RouteEntry>& r1Table,
    const map<string, RouteEntry>& r2Table,
    const string& neighborName) {
    map<string, RouteEntry> result = r1Table;

    for (const auto& item : r2Table) {
        const string& network = item.first;
        int newDistance = min(item.second.distance + 1, 16);

        if (!result.count(network)) {
            result[network] = {newDistance, neighborName};
            continue;
        }

        const RouteEntry& oldEntry = result[network];
        if (oldEntry.nextHop == neighborName || newDistance < oldEntry.distance) {
            result[network] = {newDistance, neighborName};
        }
    }

    return result;
}

void printTable(const map<string, RouteEntry>& table) {
    cout << "更新后的 R1 路由表:" << endl;
    for (const auto& item : table) {
        cout << item.first << " " << item.second.distance << " " << item.second.nextHop << endl;
    }
}

int main() {
    cout << "输入 R1 路由表，每行格式: 网络 跳数 下一跳" << endl;
    map<string, RouteEntry> r1Table = readTable("R1");

    cout << "输入 R2 发来的路由表，每行格式: 网络 跳数 下一跳" << endl;
    map<string, RouteEntry> r2Table = readTable("R2");

    string neighborName;
    cout << "请输入邻居名称，例如 R2: ";
    cin >> neighborName;

    map<string, RouteEntry> result = updateRipTable(r1Table, r2Table, neighborName);
    printTable(result);
    return 0;
}
