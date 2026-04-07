#include <iostream>
#include <map>
#include <string>
#include <iomanip>
using namespace std;

// 网桥转发表
class BridgeTable {
private:
    map<string, int> macTable; // MAC 地址 -> 端口映射
    
public:
    // 学习 MAC 地址
    void learn(const string& macAddress, int port) {
        macTable[macAddress] = port;
    }
    
    // 查找 MAC 地址对应的端口
    int lookup(const string& macAddress) {
        if (macTable.find(macAddress) != macTable.end()) {
            return macTable[macAddress];
        }
        return -1; // 未找到
    }
    
    // 显示转发表
    void displayTable() {
        cout << "\n=== 网桥转发表 ===" << endl;
        cout << "MAC 地址\t\t端口" << endl;
        cout << "------------------------" << endl;
        for (auto& entry : macTable) {
            cout << entry.first << "\t" << entry.second << endl;
        }
        cout << "====================\n" << endl;
    }
};

// 帧结构
struct Frame {
    string srcAddress;  // 源地址
    int srcPort;        // 源端口
    string dstAddress;  // 目的地址
};

// 将 4bit 地址转换为字符串格式
string addressToString(int address) {
    char buffer[8];
    sprintf(buffer, "0x%X", address);
    return string(buffer);
}

// 从字符串解析地址
int stringToAddress(const string& str) {
    int addr;
    sscanf(str.c_str(), "0x%X", &addr);
    return addr;
}

int main() {
    BridgeTable bridge;
    int frameCount;
    
    cout << "=== 网桥自学习功能模拟 ===" << endl;
    cout << "说明：地址长度为 4bit (0x0-0xF)，0xF 表示广播地址" << endl;
    cout << "网桥有 2 个端口：端口 1 和端口 2" << endl << endl;
    
    cout << "请输入帧的数量：";
    cin >> frameCount;
    
    cout << "\n请输入 " << frameCount << " 组通信帧信息：" << endl;
    cout << "格式：源地址 源端口 目的地址 (例如：0x1 1 0x2)" << endl << endl;
    
    for (int i = 0; i < frameCount; i++) {
        Frame frame;
        
        cout << "第 " << (i + 1) << " 帧：";
        cin >> frame.srcAddress >> frame.srcPort >> frame.dstAddress;
        
        // 学习源 MAC 地址
        bridge.learn(frame.srcAddress, frame.srcPort);
        
        // 查找目的地址
        int dstPort = bridge.lookup(frame.dstAddress);
        
        // 判断是否为广播地址
        int dstAddrValue = stringToAddress(frame.dstAddress);
        bool isBroadcast = (dstAddrValue == 0xF);
        
        // 输出转发决策
        cout << "  -> 目的端口：";
        if (isBroadcast) {
            cout << "广播到所有端口 (1 和 2)" << endl;
        } else if (dstPort == -1) {
            cout << "未知地址，泛洪到所有端口 (1 和 2)" << endl;
        } else if (dstPort == frame.srcPort) {
            cout << "过滤 (源和目的在同一端口)" << endl;
        } else {
            cout << "端口 " << dstPort << endl;
        }
    }
    
    // 显示最终的转发表
    bridge.displayTable();
    
    cout << "模拟完成！" << endl;
    
    return 0;
}
