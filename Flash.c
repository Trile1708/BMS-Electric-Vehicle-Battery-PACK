#include "stm32g0xx_hal.h"
#include "Flash.h"
// Dinh nghia dia chi Trang cuoi cung cua STM32G030C8T6 (Page 31)

// =================================================================
// HAM LUU DU LIEU VAO FLASH (Goi truoc khi tat may)
// =================================================================
void Flash_Save_BMS_State(BMS_State_t *bms) {
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t PageError = 0;
    
    // 1. Mo khoa bo nho Flash (Unlock)
    HAL_FLASH_Unlock();
    
    // 2. Xoa sach Trang 31 (Bat buoc truoc khi ghi)
    EraseInitStruct.TypeErase   = FLASH_TYPEERASE_PAGES;
    EraseInitStruct.Page        = FLASH_PAGE_NUMBER;
    EraseInitStruct.NbPages     = 1;
    
    if (HAL_FLASHEx_Erase(&EraseInitStruct, &PageError) != HAL_OK) {
        HAL_FLASH_Lock();
        return; // Loi xoa Flash -> Thoat
    }
    
    // 3. Ghep 2 bien float (32-bit) thanh 1 bien uint64_t (64-bit)
    // Dung ep kieu con tro de lay nguyen ban day bit cua float
    uint32_t data1 = *(uint32_t*)&(bms->RemainingCapacity_mAh);
    uint32_t data2 = *(uint32_t*)&(bms->SOC);
    
    uint64_t double_word_to_write = ((uint64_t)data2 << 32) | data1;
    
    // 4. Ghi 64-bit vao Flash
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, FLASH_STORAGE_ADDR, double_word_to_write);
    
    // 5. Khoa Flash lai de bao ve
    HAL_FLASH_Lock();
}

// =================================================================
// HAM DOC DU LIEU TU FLASH (Goi 1 lan duy nhat khi khoi dong)
// =================================================================
void Flash_Load_BMS_State(BMS_State_t *bms) {
    // 1. Doc vung nho 64-bit tai dia chi Flash
    uint64_t read_data = *(__IO uint64_t*)FLASH_STORAGE_ADDR;
    
    // Kiem tra xem Flash co bi trong khong (0xFFFFFFFFFFFFFFFF la chua ghi gi)
    if (read_data == 0xFFFFFFFFFFFFFFFF) {
        // Lan dau tien bat mach, chua co data -> Mac dinh la 100%
        bms->RemainingCapacity_mAh = bms->DesignCapacity_mAh;
        bms->SOC = 100.0f;
    } else {
        // 2. Tach lai thanh 2 bien float 32-bit
        uint32_t raw_data1 = (uint32_t)(read_data & 0xFFFFFFFF);
        uint32_t raw_data2 = (uint32_t)((read_data >> 32) & 0xFFFFFFFF);
        
        bms->RemainingCapacity_mAh = *(float*)&raw_data1;
        bms->SOC                   = *(float*)&raw_data2;
    }
}
