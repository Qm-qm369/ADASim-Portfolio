/*
给 Planner 协议提供一个本机单调时钟，单位毫秒。

它在返回什么

std::chrono::steady_clock::now()
取当前单调时间。这种钟只往前走，不受系统改时间、NTP 回拨影响。
.time_since_epoch()
从该时钟的起点到现在的时长。起点是任意的，数值本身没有“年月日”意义，只适合做差值。
duration_cast<milliseconds>(...)
把时长收成整毫秒。
.count()
取出那个整数，类型是 qint64（Qt 的 64 位有符号整数）。

所以调用一次就得到：从开机后某个固定起点算起的毫秒数。
*/

#pragma once
#include <chrono>
#include <QtGlobal>

inline qint64 plannerNowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}