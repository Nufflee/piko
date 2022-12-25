#pragma once

#include <string_view>
#include <span>
#include <cstdint>
#include <cstdio>

#include <pico/stdlib.h>
#include <hardware/structs/scb.h>

namespace piko {

// -- Config --
constexpr uint32_t TASK_CAPACITY = 4;
constexpr uint32_t TASK_TICK_INTERVAL_US = 1 * 1000;
constexpr uint32_t TASK_STACK_SIZE = 1024;

constexpr uint32_t CORE_COUNT = 2;

constexpr uint32_t ALARM_NUM = 0;
constexpr uint32_t ALARM_IRQ = TIMER_IRQ_0 + ALARM_NUM;

#define PIKO_SYSVIEW_LOGGING_ENABLED false
// -- Config --

struct Task {
    uint32_t* sp;
    std::string_view name;
    uint32_t sleep_ticks;
};

class Scheduler {
private:
    Task m_tasks[TASK_CAPACITY];
    uint32_t m_task_count = 0;
    uint32_t m_current_task = 1; // Task 0 is the idle task, skip that one

public:
    Scheduler();

    /// @brief Get the scheduler for the current executing core. There is one scheduler per core
    /// @return The scheduler for the current core
    static Scheduler& current();

    /// @brief Add a task to the task scheduler
    /// @param func The task function
    /// @param name The name of the task (used for debugging)
    void add_task(void (*func)(), std::string_view name);

    /// @brief Start the task scheduler and run the first task
    void start();

    /// @brief Sleep for a given number of scheduler ticks and switch to a different task in the meantime
    /// @param ticks Number of ticks to sleep for.
    void sleep(uint32_t ticks);

    __attribute__((always_inline)) inline void yield() {
        // Trigger a PendSV interrupt
        hw_set_bits(&scb_hw->icsr, M0PLUS_ICSR_PENDSVSET_BITS);
    }

    std::span<Task> tasks();

private:
    void tick();
    Task& select_next_task();

    static void alarm_isr();
    __attribute__((naked)) static void task_switch_isr();

    static void idle_task();
};

__attribute__((always_inline)) inline void sleep(uint32_t ticks) {
    auto& scheduler = Scheduler::current();
    // - 1 is a hack to account for the time yield takes to run
    scheduler.sleep(ticks - 1);
    scheduler.yield();
}

// TODO: implement sleep_ms

}