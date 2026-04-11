#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <iomanip>

using namespace std;

// ============================================
// 程序设计题 27：Nyquist 采样定理模拟实现
// ============================================

// 信号类 - 表示连续的正弦信号
class Signal {
private:
    double frequency;   // 信号频率 (Hz)
    double amplitude;   // 信号振幅
    double phase;       // 信号相位
    
public:
    Signal(double freq, double amp, double p = 0) 
        : frequency(freq), amplitude(amp), phase(p) {}
    
    // 计算信号在时间 t 的值
    double getValue(double t) const {
        return amplitude * sin(2 * M_PI * frequency * t + phase);
    }
    
    double getFrequency() const { return frequency; }
    double getAmplitude() const { return amplitude; }
};

// 采样器类 - 对信号进行离散化采样
class Sampler {
private:
    double samplingRate;  // 采样率 (Hz)
    
public:
    Sampler(double rate) : samplingRate(rate) {}
    
    // 对信号进行采样，返回 (时间，幅值) 的采样点序列
    vector<pair<double, double>> sample(const Signal& signal, double duration) {
        vector<pair<double, double>> samples;
        double interval = 1.0 / samplingRate;
        
        for (double t = 0; t < duration; t += interval) {
            samples.push_back({t, signal.getValue(t)});
        }
        
        return samples;
    }
    
    double getSamplingRate() const { return samplingRate; }
};

// 信号重建器类 - 使用 sinc 插值法从采样点重建连续信号
class SignalReconstructor {
public:
    // 使用 sinc 函数插值重建信号在时间 t 的值
    static double reconstruct(double t, const vector<pair<double, double>>& samples, 
                             double samplingRate) {
        double value = 0.0;
        double T = 1.0 / samplingRate;
        
        for (size_t i = 0; i < samples.size(); i++) {
            double ti = samples[i].first;
            double xi = samples[i].second;
            
            if (abs(t - ti) < 1e-10) {
                value += xi;
            } else {
                double arg = M_PI * (t - ti) / T;
                value += xi * sin(arg) / arg;
            }
        }
        
        return value;
    }
    
    // 生成重建信号的时间序列
    static vector<pair<double, double>> generateReconstructedSignal(
        const vector<pair<double, double>>& samples, 
        double samplingRate,
        double duration,
        double resolution) {
        
        vector<pair<double, double>> reconstructed;
        
        for (double t = 0; t < duration; t += resolution) {
            double value = reconstruct(t, samples, samplingRate);
            reconstructed.push_back({t, value});
        }
        
        return reconstructed;
    }
};

// 计算均方误差 (Mean Square Error)
double calculateMSE(const vector<pair<double, double>>& original,
                   const vector<pair<double, double>>& reconstructed) {
    double mse = 0.0;
    size_t n = min(original.size(), reconstructed.size());
    
    for (size_t i = 0; i < n; i++) {
        double diff = original[i].second - reconstructed[i].second;
        mse += diff * diff;
    }
    
    return mse / n;
}

// 验证 Nyquist 采样定理的主函数
void verifyNyquistTheorem() {
    cout << "========== Nyquist 采样定理验证实验 ==========" << endl;
    cout << endl;
    
    double maxFrequency = 100.0;    // 信号最高频率 (Hz)
    double signalDuration = 0.1;     // 信号持续时间 (秒)
    
    Signal testSignal(maxFrequency, 1.0);
    
    cout << "测试信号参数:" << endl;
    cout << "  最高频率：f_max = " << maxFrequency << " Hz" << endl;
    cout << "  Nyquist 采样率：f_Nyquist = 2 * f_max = " << 2 * maxFrequency << " Hz" << endl;
    cout << endl;
    
    // 生成原始信号用于比较
    vector<pair<double, double>> original;
    for (double t = 0; t < signalDuration; t += 0.0001) {
        original.push_back({t, testSignal.getValue(t)});
    }
    
    // 情况 1: 采样率 > 2*f_max (满足 Nyquist 定理)
    cout << "【情况 1】采样率 > 2*f_max (满足 Nyquist 定理)" << endl;
    cout << "-------------------------------------------" << endl;
    double samplingRate1 = 250.0;
    Sampler sampler1(samplingRate1);
    vector<pair<double, double>> samples1 = sampler1.sample(testSignal, signalDuration);
    vector<pair<double, double>> reconstructed1 = 
        SignalReconstructor::generateReconstructedSignal(samples1, samplingRate1, 
                                                         signalDuration, 0.0001);
    double mse1 = calculateMSE(original, reconstructed1);
    
    cout << "  采样率：f_s = " << samplingRate1 << " Hz" << endl;
    cout << "  采样点数：" << samples1.size() << endl;
    cout << "  均方误差 (MSE): " << scientific << setprecision(10) << mse1 << endl;
    cout << "  结论：可以成功重建信号 (MSE 很小)" << endl;
    cout << endl;
    
    // 情况 2: 采样率 < 2*f_max (不满足 Nyquist 定理)
    cout << "【情况 2】采样率 < 2*f_max (不满足 Nyquist 定理)" << endl;
    cout << "-------------------------------------------" << endl;
    double samplingRate2 = 150.0;
    Sampler sampler2(samplingRate2);
    vector<pair<double, double>> samples2 = sampler2.sample(testSignal, signalDuration);
    vector<pair<double, double>> reconstructed2 = 
        SignalReconstructor::generateReconstructedSignal(samples2, samplingRate2, 
                                                         signalDuration, 0.0001);
    double mse2 = calculateMSE(original, reconstructed2);
    
    cout << "  采样率：f_s = " << samplingRate2 << " Hz" << endl;
    cout << "  采样点数：" << samples2.size() << endl;
    cout << "  均方误差 (MSE): " << scientific << setprecision(10) << mse2 << endl;
    cout << "  结论：无法正确重建信号 (MSE 很大，出现混叠现象)" << endl;
    cout << endl;
    
    // 情况 3: 采样率 = 2*f_max (临界情况)
    cout << "【情况 3】采样率 = 2*f_max (临界情况)" << endl;
    cout << "-------------------------------------------" << endl;
    double samplingRate3 = 200.0;
    Sampler sampler3(samplingRate3);
    vector<pair<double, double>> samples3 = sampler3.sample(testSignal, signalDuration);
    vector<pair<double, double>> reconstructed3 = 
        SignalReconstructor::generateReconstructedSignal(samples3, samplingRate3, 
                                                         signalDuration, 0.0001);
    double mse3 = calculateMSE(original, reconstructed3);
    
    cout << "  采样率：f_s = " << samplingRate3 << " Hz" << endl;
    cout << "  采样点数：" << samples3.size() << endl;
    cout << "  均方误差 (MSE): " << scientific << setprecision(10) << mse3 << endl;
    cout << "  结论：理论上可以重建，但实际效果取决于相位" << endl;
    cout << endl;
    
    // 实验总结
    cout << "========== 实验总结 ==========" << endl;
    cout << "Nyquist 采样定理：要从样本中无失真地恢复连续信号，" << endl;
    cout << "              采样频率必须大于信号最高频率的 2 倍。" << endl;
    cout << "公式：f_s > 2 * f_max" << endl;
    cout << endl;
    cout << "实验验证结果：" << endl;
    cout << "  - 当 f_s > 2*f_max 时，MSE 很小，信号可以成功重建" << endl;
    cout << "  - 当 f_s < 2*f_max 时，MSE 很大，出现混叠 (aliasing) 现象" << endl;
    cout << "  - 当 f_s = 2*f_max 时，处于临界状态，重建质量不稳定" << endl;
}

int main() {
    verifyNyquistTheorem();
    return 0;
}
