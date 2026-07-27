#pragma once

enum class ErrorCodes {
    NO_ERROR = 0x00,
    // SlimeServerEmu 的啟動失敗。
    // 原本 begin() 不論成敗都回 NO_ERROR,失敗時 dongle 會安靜地
    // 「開機成功但永遠收不到任何追蹤器」,使用者只能從沒有 LED 錯誤碼去猜。
    AP_START_FAILED = 0x01,      // WiFi.softAP() 失敗(通道/密碼長度/NVS 異常)
    UDP_LISTEN_FAILED = 0x02,    // AsyncUDP.listen() 失敗(埠被占用/lwIP 資源不足)

    // 哨兵:永遠等於「最大錯誤碼 + 1」,新增錯誤碼時它會自動跟著長大。
    // led.cpp 的 static_assert 檢查的是它,而不是某個具名錯誤碼 ——
    // 盯著具名的那一個,只要有人在它後面補一項就會漏檢,而漏檢的後果是
    // 新錯誤碼的高位被靜默切掉,在 LED 上和某個舊錯誤閃出一模一樣的樣式。
    ERROR_CODE_COUNT
};
