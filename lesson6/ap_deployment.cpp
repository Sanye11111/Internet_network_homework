#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>
#include <algorithm>
using namespace std;

// 常量定义
const double FLOOR_LENGTH = 100.0;  // 楼层长度 (米)
const double FLOOR_WIDTH = 80.0;    // 楼层宽度 (米)
const double FLOOR_HEIGHT = 3.5;    // 层高 (米)
const double CORRIDOR_WIDTH = 2.0;  // 走廊宽度 (米)
const double CLASSROOM_LENGTH = 15.0;  // 教室长度 (米)
const double CLASSROOM_WIDTH = 10.0;   // 教室宽度 (米)

// 墙体类型
enum WallType {
    LOAD_BEARING,    // 承重墙 (25cm, 衰减 12dB)
    PARTITION_WALL,  // 普通隔断墙 (10cm, 衰减 6dB)
    GLASS_WALL       // 玻璃幕墙 (5cm, 衰减 7dB)
};

// AP 结构
struct AP {
    int id;
    double x, y, z;          // AP 坐标
    int channel24G;          // 2.4GHz 信道
    int channel5G;           // 5GHz 信道
    double coverageRadius;   // 覆盖半径
};

// 计算信号衰减
double calculateAttenuation(double distance, int wallCount, WallType wallType) {
    // 自由空间衰减 (简化模型)
    double freeSpaceLoss = 20 * log10(distance) + 20 * log10(2400) + 32.44;
    
    // 墙体衰减
    double wallLoss = 0;
    switch (wallType) {
        case LOAD_BEARING:
            wallLoss = wallCount * 12.0;
            break;
        case PARTITION_WALL:
            wallLoss = wallCount * 6.0;
            break;
        case GLASS_WALL:
            wallLoss = wallCount * 7.0;
            break;
    }
    
    return freeSpaceLoss + wallLoss;
}

// 计算信号强度
double calculateSignalStrength(double txPower, double attenuation) {
    return txPower - attenuation;  // dBm
}

// 估算覆盖半径
double estimateCoverageRadius(int wallCount, WallType wallType, bool is24G) {
    double baseRadius = is24G ? 152.5 : 101.5;  // 无墙体时的覆盖半径
    
    // 每穿一堵承重墙，半径减少
    double reduction = 0;
    switch (wallType) {
        case LOAD_BEARING:
            reduction = is24G ? (152.5 - 81.5) : (101.5 - 58.0);
            break;
        case PARTITION_WALL:
            reduction = is24G ? 20.0 : 15.0;
            break;
        case GLASS_WALL:
            reduction = is24G ? 25.0 : 18.0;
            break;
    }
    
    return max(10.0, baseRadius - reduction * wallCount);
}

// 检查位置是否有效 (在楼层内)
bool isValidPosition(double x, double y, double z, int floors) {
    if (x < 0 || x > FLOOR_LENGTH) return false;
    if (y < 0 || y > FLOOR_WIDTH) return false;
    if (z < 0 || z >= floors * FLOOR_HEIGHT) return false;
    return true;
}

// 分配信道 (避免同频干扰)
void assignChannels(vector<AP>& aps) {
    // 2.4GHz 信道：1, 6, 11 (不重叠)
    int channels24G[] = {1, 6, 11};
    // 5GHz 信道：36, 40, 44, 48, 149, 153, 157, 161
    int channels5G[] = {36, 40, 44, 48, 149, 153, 157, 161};
    
    for (size_t i = 0; i < aps.size(); i++) {
        aps[i].channel24G = channels24G[i % 3];
        aps[i].channel5G = channels5G[i % 8];
    }
}

// 生成热力图数据 (简化版本)
void generateHeatmap(const vector<AP>& aps, int floors) {
    cout << "\n=== 信号热力图数据 (简化版) ===" << endl;
    cout << "采样网格：10m x 10m" << endl;
    cout << "信号强度单位：dBm (目标：>= -65dBm)" << endl << endl;
    
    const int gridSize = 10;
    const double txPower = 20.0;  // 发射功率 20dBm
    
    for (int f = 0; f < floors; f++) {
        cout << "\n--- 第 " << (f + 1) << " 层 ---" << endl;
        cout << setw(8) << "X(m)" << setw(8) << "Y(m)" << setw(12) << "信号强度" << setw(10) << "状态" << endl;
        
        for (int x = 0; x <= FLOOR_LENGTH; x += gridSize) {
            for (int y = 0; y <= FLOOR_WIDTH; y += gridSize) {
                double z = f * FLOOR_HEIGHT + FLOOR_HEIGHT / 2;  // 楼层中间高度
                double minDistance = 1e9;
                
                // 找到最近的 AP
                for (const auto& ap : aps) {
                    if (abs(ap.z - z) < FLOOR_HEIGHT / 2) {  // 同一楼层
                        double distance = sqrt(pow(x - ap.x, 2) + pow(y - ap.y, 2));
                        minDistance = min(minDistance, distance);
                    }
                }
                
                if (minDistance < 1e8) {
                    // 计算信号强度 (简化)
                    double attenuation = 20 * log10(minDistance + 1) + 40;
                    double signalStrength = calculateSignalStrength(txPower, attenuation);
                    string status = signalStrength >= -65 ? "良好" : "较弱";
                    
                    cout << setw(8) << x << setw(8) << y 
                         << setw(10) << fixed << setprecision(1) << signalStrength << " dBm"
                         << setw(10) << status << endl;
                }
            }
        }
    }
}

// 优化 AP 位置
vector<AP> optimizeAPPositions(int floors) {
    vector<AP> aps;
    int apId = 1;
    
    // 每层楼布置 AP
    for (int f = 0; f < floors; f++) {
        double z = f * FLOOR_HEIGHT + FLOOR_HEIGHT / 2;
        
        // 走廊中心线布置 AP (贯穿每层中心区域)
        // 走廊宽度 2m，贯穿每层中心区域
        double corridorY = FLOOR_WIDTH / 2;
        
        // 每隔 40 米布置一个 AP (考虑墙体衰减)
        for (double x = 20; x <= FLOOR_LENGTH - 20; x += 40) {
            AP ap;
            ap.id = apId++;
            ap.x = x;
            ap.y = corridorY;
            ap.z = z;
            ap.coverageRadius = estimateCoverageRadius(2, PARTITION_WALL, true);
            aps.push_back(ap);
        }
    }
    
    // 分配信道
    assignChannels(aps);
    
    return aps;
}

// 显示 AP 部署信息
void displayAPInfo(const vector<AP>& aps) {
    cout << "\n=== AP 部署信息 ===" << endl;
    cout << "AP 数量：" << aps.size() << endl << endl;
    
    cout << setw(5) << "ID" 
         << setw(10) << "X(m)" 
         << setw(10) << "Y(m)" 
         << setw(10) << "Z(m)" 
         << setw(12) << "2.4G 信道" 
         << setw(12) << "5G 信道"
         << setw(15) << "覆盖半径 (m)" << endl;
    
    cout << string(75, '-') << endl;
    
    for (const auto& ap : aps) {
        cout << setw(5) << ap.id
             << setw(10) << fixed << setprecision(1) << ap.x
             << setw(10) << ap.y
             << setw(10) << ap.z
             << setw(12) << ap.channel24G
             << setw(12) << ap.channel5G
             << setw(15) << setprecision(1) << ap.coverageRadius << endl;
    }
}

// 显示教学楼结构
void displayBuildingInfo() {
    cout << "=== 教学楼三维结构信息 ===" << endl;
    cout << "尺寸：" << FLOOR_LENGTH << "m x " << FLOOR_WIDTH << "m x 3 层" << endl;
    cout << "层高：" << FLOOR_HEIGHT << "m" << endl;
    cout << "走廊宽度：" << CORRIDOR_WIDTH << "m (贯穿每层中心区域)" << endl;
    cout << "教室尺寸：" << CLASSROOM_LENGTH << "m x " << CLASSROOM_WIDTH << "m (分布两侧)" << endl << endl;
    
    cout << "墙体分类：" << endl;
    cout << "  - 承重墙：钢筋混凝土，厚度 25cm, 衰减 12dB" << endl;
    cout << "  - 普通隔断墙：石膏板/木板，厚度 10cm, 衰减 6dB" << endl;
    cout << "  - 玻璃幕墙：厚度 5cm, 衰减 7dB" << endl << endl;
    
    cout << "目标信号强度阈值：>= -65dBm (保证稳定连接)" << endl;
    cout << "单 AP 覆盖半径估算：" << endl;
    cout << "  2.4GHz: 152.5m (无墙体) / 81.5m (穿透 1 堵承重墙)" << endl;
    cout << "  5GHz:   101.5m (无墙体) / 58m (穿透 1 堵承重墙)" << endl << endl;
}

int main() {
    int floors = 3;
    
    cout << "=== AP 部署模拟程序 ===" << endl << endl;
    
    // 显示教学楼信息
    displayBuildingInfo();
    
    // 优化 AP 位置
    cout << "正在计算 AP 最佳位置..." << endl;
    vector<AP> aps = optimizeAPPositions(floors);
    
    // 显示 AP 部署信息
    displayAPInfo(aps);
    
    // 生成热力图
    generateHeatmap(aps, floors);
    
    cout << "\n=== 部署建议 ===" << endl;
    cout << "1. AP 应安装在走廊天花板中央位置" << endl;
    cout << "2. 2.4GHz 和 5GHz 信道已自动分配以避免同频干扰" << endl;
    cout << "3. 建议定期检测信号强度，必要时调整 AP 位置或增加 AP 数量" << endl;
    cout << "4. 承重墙区域信号衰减较大，可能需要额外部署 AP" << endl;
    
    cout << "\n模拟完成！" << endl;
    
    return 0;
}
