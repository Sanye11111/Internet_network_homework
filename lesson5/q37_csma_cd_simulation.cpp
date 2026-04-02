#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <random>
#include <thread>

namespace {

std::mutex start_mutex;
std::atomic<bool> channel_busy(false);
std::atomic<int> current_senders(0);

void send_frame(int station_id) {
    std::mt19937 rng(std::random_device{}());
    int collision_count = 0;
    const int max_retry = 16;

    while (collision_count < max_retry) {
        while (channel_busy.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        {
            std::lock_guard<std::mutex> lock(start_mutex);
            if (channel_busy.load()) {
                continue;
            }
            channel_busy.store(true);
            current_senders.store(1);
        }

        std::cout << "站点 " << station_id << " 监听到信道空闲，开始发送数据帧。\n";

        std::this_thread::sleep_for(std::chrono::milliseconds(3));

        bool forced_collision = (collision_count == 0);
        if (forced_collision) {
            current_senders.store(2);
        }

        if (current_senders.load() > 1) {
            std::cout << "站点 " << station_id
                      << " 检测到冲突，发送 Jam 信号并停止发送。\n";

            channel_busy.store(false);
            current_senders.store(0);

            int k = (collision_count < 10) ? collision_count : 10;
            int max_slot = (1 << k) - 1;
            std::uniform_int_distribution<int> dist(0, max_slot > 0 ? max_slot : 0);
            int backoff_slots = dist(rng);

            std::cout << "站点 " << station_id << " 执行二进制指数退避，等待 "
                      << backoff_slots << " 个时隙后重传。\n";

            std::this_thread::sleep_for(std::chrono::milliseconds(backoff_slots * 5 + 2));
            ++collision_count;
            continue;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(8));
        std::cout << "站点 " << station_id << " 成功发送数据帧。\n";

        channel_busy.store(false);
        current_senders.store(0);
        return;
    }

    std::cout << "站点 " << station_id << " 重传次数过多，发送失败。\n";
}

}  // namespace

int main() {
    std::thread station1(send_frame, 1);
    std::thread station2(send_frame, 2);
    std::thread station3(send_frame, 3);

    station1.join();
    station2.join();
    station3.join();

    return 0;
}
