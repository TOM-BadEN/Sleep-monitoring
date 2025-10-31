#include "sleepmonitor.hpp"
#include <cstring>

// 静态线程栈定义
alignas(0x1000) static char monitor_thread_stack[4 * 1024];

SleepMonitor::SleepMonitor() : m_running(false), m_last_state(PscPmState_ReadyAwaken) {
    memset(&m_PscModule, 0, sizeof(PscPmModule));
    memset(&m_thread, 0, sizeof(Thread));
}

SleepMonitor::~SleepMonitor() {
    Stop();
}

bool SleepMonitor::Start() {

    // 设置线程创建标志
    bool thread_created = false;
    
    // 初始化 PSC 模块
    Result rc = pscmGetPmModule(&m_PscModule, (PscPmModuleId)200, nullptr, 0, false);
    if (R_FAILED(rc)) goto error;
    
    // 设置运行标志
    m_running = true;
    
    // 创建监控线程（-2表示自适应核心分配）
    rc = threadCreate(&m_thread, MonitorThreadFunc, this,
                     monitor_thread_stack, sizeof(monitor_thread_stack), 44, -2);
    if (R_FAILED(rc)) goto error;
    
    // 线程创建成功标志
    thread_created = true;
    
    // 启动线程
    rc = threadStart(&m_thread);
    if (R_FAILED(rc)) goto error;
    
    return true;

error:
    if (thread_created) threadClose(&m_thread);
    if (m_running) {
        m_running = false;
        pscPmModuleClose(&m_PscModule);
    }
    return false;
}

void SleepMonitor::Stop() {
    if (!m_running) return;
    
    m_running = false;
    
    // 等待线程退出
    threadWaitForExit(&m_thread);
    threadClose(&m_thread);
    
    // 关闭 PSC 模块
    pscPmModuleClose(&m_PscModule);
}

void SleepMonitor::SetSleepCallback(std::function<void()> callback) {
    m_on_sleep = callback;
}

void SleepMonitor::SetWakeupCallback(std::function<void()> callback) {
    m_on_wakeup = callback;
}

void SleepMonitor::MonitorThreadFunc(void* arg) {
    SleepMonitor* self = static_cast<SleepMonitor*>(arg);
    self->MonitorLoop();
}

void SleepMonitor::MonitorLoop() {
    while (m_running) {
        // 等待 PSC 事件（100ms超时，定期检查退出标志）
        Result rc = eventWait(&m_PscModule.event, 100000000ULL);
        // 超时或失败，继续循环检查 m_running
        if (R_FAILED(rc)) continue;
        PscPmState state;
        pscPmModuleGetRequest(&m_PscModule, &state, nullptr);
        // 只在状态变化时触发回调
        if (state != m_last_state) {
            // 休眠事件
            if (state == PscPmState_ReadySleep && m_on_sleep) m_on_sleep();
            // 唤醒事件
            else if (state == PscPmState_ReadyAwaken && m_on_wakeup) m_on_wakeup();
            // 更新上次状态
            m_last_state = state;
        }
        // 确认事件
        pscPmModuleAcknowledge(&m_PscModule, state);
    }
}

