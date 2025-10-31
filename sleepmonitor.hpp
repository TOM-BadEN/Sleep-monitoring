#pragma once
#include <switch.h>
#include <functional>

class SleepMonitor {
public:
    SleepMonitor();
    ~SleepMonitor();
    
    // 启动监控
    bool Start();
    
    // 停止监控
    void Stop();
    
    // 设置休眠回调
    void SetSleepCallback(std::function<void()> callback);
    
    // 设置唤醒回调
    void SetWakeupCallback(std::function<void()> callback);

private:
    // 线程入口函数（静态）
    static void MonitorThreadFunc(void* arg);
    
    // 监控循环（实例方法）
    void MonitorLoop();
    
    PscPmModule m_PscModule;                // PSC电源管理模块
    Thread m_thread;                         // 监控线程
    bool m_running;                          // 运行标志
    PscPmState m_last_state;                 // 上次电源状态
    
    std::function<void()> m_on_sleep;       // 休眠回调
    std::function<void()> m_on_wakeup;      // 唤醒回调
};

