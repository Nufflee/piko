/**
 * This one is interesting.. SEGGER SystemView (https://www.segger.com/products/development-tools/systemview/) is a really neat program used for
 * debugging and profiling RTOS's, and this module is a thin wrapper on top of their C API.
 * Additionally, we "hijack" their code to log all SystemView data to UART instead of proprietary RTT, as I cannot afford an expensive J-Link
 * probe required for it, and it makes debugging more accessible in general.
 * To view the logged data in SystemView, simply dump all UART output to a .bin file and open it in the software (make sure there is no other
 * data being written to UART at the same time).
*/

#pragma once

#include <cstdint>

#include <hardware/clocks.h>
#include <hardware/structs/systick.h>

#if PIKO_SYSVIEW_LOGGING_ENABLED
#include <SEGGER_SYSVIEW.h>
#endif

#include "piko.hpp"

namespace piko::sysview {

#if PIKO_SYSVIEW_LOGGING_ENABLED
static void sysview_system_desc_cb() {
    SEGGER_SYSVIEW_SendSysDesc("N=" SEGGER_SYSVIEW_APP_NAME ",O=piko,D=" SEGGER_SYSVIEW_DEVICE_NAME);
}

static uint64_t sysview_get_time() {
    return time_us_64();
}

static void sysview_send_task_list() {
    std::span<Task> tasks = Scheduler::current().tasks();

    for (uint32_t i = 0; i < tasks.size(); i++) {
        SEGGER_SYSVIEW_TASKINFO info = {
            .TaskID = i,
            .sName = tasks[i].name.data(),
            .Prio = 0,
            // TODO: Stack base
            .StackBase = 0,
            .StackSize = TASK_STACK_SIZE
        };

        SEGGER_SYSVIEW_SendTaskInfo(&info);
    }
}

static SEGGER_SYSVIEW_OS_API sysview_api = {
    sysview_get_time,
    sysview_send_task_list,
};
#endif

void init() {
#if PIKO_SYSVIEW_LOGGING_ENABLED
    SEGGER_SYSVIEW_Init(clock_get_hz(clk_sys), clock_get_hz(clk_sys), &sysview_api, sysview_system_desc_cb);
#endif
}

void start() {
#if PIKO_SYSVIEW_LOGGING_ENABLED
    SEGGER_SYSVIEW_Start();
#endif
}

void log_task_create([[maybe_unused]] uint32_t task_id) {
#if PIKO_SYSVIEW_LOGGING_ENABLED
    SEGGER_SYSVIEW_OnTaskCreate(task_id);
#endif
}

void log_task_idle() {
#if PIKO_SYSVIEW_LOGGING_ENABLED
    SEGGER_SYSVIEW_OnIdle();
#endif
}

void log_task_start([[maybe_unused]] uint32_t task_id) {
#if PIKO_SYSVIEW_LOGGING_ENABLED
    SEGGER_SYSVIEW_OnTaskStartExec(task_id);
#endif
}

void log_task_stop() {
#if PIKO_SYSVIEW_LOGGING_ENABLED
    SEGGER_SYSVIEW_OnTaskStopExec();
#endif
}

}

#if PIKO_SYSVIEW_LOGGING_ENABLED
extern "C" {

uint32_t SEGGER_RTT_WriteSkipNoLock(uint32_t buffer_index, const void* buffer, uint32_t byte_count) {
    (void) buffer_index;
    assert(buffer_index == 0);

    uart_write_blocking(uart0, (const uint8_t*) buffer, byte_count);
    return 1;
}

uint32_t SEGGER_SYSVIEW_X_GetTimestamp() {
    return systick_hw->rvr - systick_hw->cvr;
}

}
#endif


