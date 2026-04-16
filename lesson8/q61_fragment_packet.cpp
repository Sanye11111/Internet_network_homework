#include <iostream>
#include <vector>

using namespace std;

struct Fragment {
    int packetLength;
    int offset;
};

vector<Fragment> fragmentPacket(int packetLength, const vector<int>& pathMTUs) {
    const int headerLength = 20;
    vector<Fragment> fragments;
    fragments.push_back({packetLength, 0});

    for (int mtu : pathMTUs) {
        vector<Fragment> nextFragments;
        int maxData = ((mtu - headerLength) / 8) * 8;

        for (size_t i = 0; i < fragments.size(); ++i) {
            int dataLength = fragments[i].packetLength - headerLength;
            int baseOffset = fragments[i].offset;

            if (fragments[i].packetLength <= mtu) {
                nextFragments.push_back(fragments[i]);
                continue;
            }

            int used = 0;
            while (dataLength - used > maxData) {
                nextFragments.push_back({maxData + headerLength, baseOffset + used / 8});
                used += maxData;
            }

            nextFragments.push_back({dataLength - used + headerLength, baseOffset + used / 8});
        }

        fragments = nextFragments;
    }

    return fragments;
}

int main() {
    int packetLength = 24576;
    vector<int> pathMTUs = {4325, 2346, 1500, 4464, 2346};
    vector<Fragment> result = fragmentPacket(packetLength, pathMTUs);

    for (size_t i = 0; i < result.size(); ++i) {
        cout << "fragment " << i + 1
             << ": length=" << result[i].packetLength
             << ", offset=" << result[i].offset << endl;
    }

    return 0;
}
