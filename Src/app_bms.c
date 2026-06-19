#include "app_bms.h"

static uint32_t fault_timer_start = 0;
static uint8_t is_counting_fault = 0;
static uint8_t retry_count = 0; 
#define MAX_RETRY 3

void BMS_Update_LED_Status(BMS_State_t *bms) {
    uint32_t currentMillis = HAL_GetTick();
    
    // Khai báo các bien dem thoi gian doc lap cho tung LED
    static uint32_t led_sys_timer = 0;
    static uint8_t  led_sys_state = 0;
    static uint32_t led_bal_timer = 0;

    // ==============================================================
    // LED 1 (PC6): TRANG THÁI HE THONG (SYSTEM STATUS)
    // ==============================================================
    if (bms->SystemState == BMS_STATE_FAULT) {
        // Uu tiên 1: LOI HE THONG -> Nháy liên tuc chop nhoáng (100ms) báo dong
        if (currentMillis - led_sys_timer >= 100) {
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_6);
            led_sys_timer = currentMillis;
        }
    } 
    else if (bms->SystemState == BMS_STATE_CHARGING) {
        // Uu tiên 2: ÐANG SAC -> Nháy cham deu  500ms (nhu dang hít tho)
        if (currentMillis - led_sys_timer >= 500) {
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_6);
            led_sys_timer = currentMillis;
        }
    }
    else {
        // Uu tiên 3: BÌNH THUONG (IDLE / DISCHARGING) -> Nháy nhip tim (Heartbeat)
        // Chop lóe lên 1 cái 100ms roi tat 2900ms
        if (led_sys_state == 0 && (currentMillis - led_sys_timer >= 2900)) {
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_SET); 
            led_sys_state = 1;
            led_sys_timer = currentMillis;
        } 
        else if (led_sys_state == 1 && (currentMillis - led_sys_timer >= 100)) {
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_RESET); 
            led_sys_state = 0;
            led_sys_timer = currentMillis;
        }
    }

    // ==============================================================
    // LED 2 (PC7): TRANG THÁI CÂN BANG (BALANCING STATUS)
    // ==============================================================
    uint16_t v_max = 0;
    uint16_t v_min = 5000; 
    
    // Tìm Cell cao nhat và thap nhat
    if (bms->TotalVoltage > 0) {
        for (int i = 0; i < 15; i++) {
            if (bms->CellVoltage[i] > v_max) v_max = bms->CellVoltage[i];
            if (bms->CellVoltage[i] < v_min) v_min = bms->CellVoltage[i];
        }
    }
    uint16_t delta_v = v_max - v_min;

    // ÐIEU KIEN CÂN BANG: Lech áp >= 100mV VÀ Áp tong phai lon hon 45V 
    if (delta_v >= 100 && bms->TotalVoltage > 45000) { 
        // Mach ETA3000 dang làm viec -> Nháy deu 250ms 
        if (currentMillis - led_bal_timer >= 250) {
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_7);
            led_bal_timer = currentMillis;
        }
    } else {
        // Ðã cân bang xong hoac pin dang can -> Tat han LED
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);
    }
}
// =================================================================
// 2. HÀM AUTO-RECOVERY
// =================================================================
void BMS_Auto_Recovery(BMS_State_t *bms) {
    // Neu he thong dang có loi phan cung (ví du: SCD, OCD do BQ76940 báo lên)
    if (bms->Hardware_Fault != 0x00) {
      if (retry_count >= MAX_RETRY) {
            return; 
        }  
        if (is_counting_fault == 0) {
            fault_timer_start = HAL_GetTick();
            is_counting_fault = 1;
        }

        // Kiem tra xem dã qua 30 giây (30000 ms) cách ly chua
        if (HAL_GetTick() - fault_timer_start >= 30000) {
            BQ_ClearFaults();

            bms->Hardware_Fault = 0x00;
            
            // Reset loi bo dem thoi gian
            is_counting_fault = 0;
					retry_count++;
        }
    }  
    else {
        // Mach vua het báo loi -> Bat dau dem thoi gian an toàn
        if (is_counting_fault == 1) {
            is_counting_fault = 0;
            fault_timer_start = HAL_GetTick(); // Ðat lai moc thoi gian an toàn
        }
        
        // Neu dã chay tron tru duoc 10 giây mà không có loi moi ->  xóa án tích
        if (retry_count > 0 && (HAL_GetTick() - fault_timer_start >= 10000)) {
             retry_count = 0;
        }
    }
}

