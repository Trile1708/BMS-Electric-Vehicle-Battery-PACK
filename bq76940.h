#ifndef BQ76940_H
#define BQ76940_H

#include "main.h" //_Keo_thu_vien_loi_cua_STM32_vao
#include <stdint.h>
//_--------------------------------------------------------
//_1_DIA_CHI_I2C_CUA_IC_BQ76940_(Dich_trai_1_bit_cho_STM32_HAL)
//_--------------------------------------------------------
#define BQ_I2C_ADDR             (0x08 << 1) //_Hoac_0x18

//_--------------------------------------------------------
//_2_BAN_DO_THANH_GHI_(REGISTER_MAP)
//_--------------------------------------------------------
/*_Cac_thanh_ghi_trang_thai_va_dieu_khien_*/
#define BQ_SYS_STAT             0x00  //_Thanh_ghi_bao_loi_(Trai_tim_he_thong)
#define BQ_CELLBAL1             0x01  //_Can_bang_Cell_1_den_5
#define BQ_CELLBAL2             0x02  //_Can_bang_Cell_6_den_10
#define BQ_CELLBAL3             0x03  //_Can_bang_Cell_11_den_15
#define BQ_SYS_CTRL1            0x04  //_Dieu_khien_ADC_(Do_ap)
#define BQ_SYS_CTRL2            0x05  //_Dieu_khien_CC_(Do_dong)_&_Bat_tat_FET

/*_Cac_thanh_ghi_bao_ve_(Protection)_*/
#define BQ_PROTECT1             0x06  //_Cau_hinh_bao_ve_Ngan_mach_(SCD)
#define BQ_PROTECT2             0x07  //_Cau_hinh_bao_ve_Qua_dong_xa_(OCD)
#define BQ_PROTECT3             0x08  //_Cau_hinh_bao_ve_Qua_ap_(OV)_va_Tut_ap_(UV)
#define BQ_OV_TRIP              0x09  //_Nguong_dien_ap_OV
#define BQ_UV_TRIP              0x0A  //_Nguong_dien_ap_UV
#define BQ_CC_CFG               0x0B  //_Cau_hinh_Coulomb_Counter_(Luon_phai_ghi_0x19)

/*_Cac_thanh_ghi_Du_lieu_Dien_ap_(Chi_Doc)_*/
#define BQ_VC1_HI               0x0C  //_Byte_cao_ap_Cell_1
#define BQ_VC1_LO               0x0D  //_Byte_thap_ap_Cell_1
//_..._(No_se_keo_dai_den_VC15_o_0x29)
#define BQ_BAT_HI               0x2A  //_Tong_ap_ca_pack_(Byte_cao)
#define BQ_BAT_LO               0x2B  //_Tong_ap_ca_pack_(Byte_thap)

/*_Cac_thanh_ghi_Du_lieu_Dong_dien_va_Nhiet_do_*/
#define BQ_TS1_HI               0x2C  //_Nhiet_do_vung_1
#define BQ_TS1_LO               0x2D
#define BQ_CC_HI                0x32  //_Dong_dien_(Coulomb_Counter)
#define BQ_CC_LO                0x33
/*read_gain_offset_register*/
#define BQ_ADCGAIN1  0x50
#define BQ_ADCOFFSET 0x51
//_--------------------------------------------------------
//_3_CAU_TRUC_DU_LIEU_BMS_(DATA_STRUCT)
//_--------------------------------------------------------
typedef enum {
    SYS_OK = 0x00,
    SYS_ERR_I2C_TIMEOUT  = 0x01,  // Bi nhieu, mat giao tiep I2C
    SYS_ERR_I2C_NACK     = 0x02,  // Chip BQ không phan hoi (có the do chua wake-up)
    SYS_ERR_WIRE_BROKEN  = 0x03,  // Ðut dây do áp cell ho?c long giac cam
    SYS_ERR_DATA_CORRUPT = 0x04,  // Du lieu doc ve vô lý (ADC nhay loan)
	  SYS_ERR_OVER_TEMP    = 0x05,   //Loi qua nhiet
	  SYS_ERR_UNDER_TEMP   = 0x06,
} Software_Fault_t;
typedef enum
 {
    HW_FAULT_NONE           = 0x00,
    HW_FAULT_OCD            = 0x01,
    HW_FAULT_SCD            = 0x02,
    HW_FAULT_OV             = 0x04,
    HW_FAULT_UV             = 0x08,
    HW_FAULT_OVRD_ALERT     = 0x10,
    HW_FAULT_DEVICE_XREADY  = 0x20
 } Hardware_Fault_t;
typedef enum
{
    BMS_STATE_INIT = 0,
    BMS_STATE_IDLE,
    BMS_STATE_CHARGING,
    BMS_STATE_DISCHARGING,
    BMS_STATE_FAULT,
    BMS_STATE_SLEEP

} BMS_SystemState_t;
#pragma pack(push,1)
typedef struct {
    uint16_t CellVoltage[15];   //_Mang_luu_dien_ap_15_cell_(mV)
	  int16_t  PackCurrent;       //_Dong_dien_hien_tai_(mA)_Co_am_duong_(Sac_Xa)
    uint32_t TotalVoltage;      //_Tong_dien_ap_pack_pin_(mV)
    float    Temperature[3];    //_Nhiet_do_3_cam_bien_NTC_(do_C)
	  uint8_t  I2C_Error_Count;   // Bien dem so lan loi I2C liên tiep
	  uint8_t  Hardware_Fault;    
	  uint8_t  ChargeFET_On;      //_Trang_thai_FET_Sac_(1_=_mo_0_=_Dong)
    uint8_t  DischargeFET_On;   //_Trang_thai_FET_Xa
    Software_Fault_t Software_Fault; // Thêm co báo loi phan mem
    BMS_SystemState_t SystemState;
	// --- CÁC BIEN CHO THUAT TOÁN SOC ---
    float    SOC;                    // Phan tram pin 
    float    RemainingCapacity_mAh;  // Dung luong dien thuc te còn lai (mAh)
    float    DesignCapacity_mAh;     // Dung luong Toi da cua khoi pin (mAh)
    uint32_t Last_Tick_ms;           // Moc thoi gian luu lai o lan tính truoc
} BMS_State_t;
//_Khai_bao_cac_ham_se_viet_ben_file_.c
void BQ_WriteReg(uint8_t regAddr, uint8_t data);
uint8_t BQ_ReadReg(uint8_t regAddr);
uint8_t BQ_CRC8(uint8_t *data, uint8_t len);
uint8_t BMS_Is_System_Safe(BMS_State_t *bms);
void BQ_Init(void);
void BQ_Read_All_Voltages(BMS_State_t *bms);
void BQ_ClearFaults(void);
void BQ_UpdateData(BMS_State_t *bms);
void BQ_TurnOn_FETs(void);
void BQ_TurnOff_FETs(void);
void BQ_Read_Current(BMS_State_t *bms);
void BQ_Set_Protection_Limits(uint16_t OVP_mV, uint16_t UVP_mV);
void BMS_Calculate_SOC(BMS_State_t *bms);
void BMS_Check_Full_Charge(BMS_State_t *bms);
void BQ_Read_Temperature(BMS_State_t *bms);
void BMS_Update_LCD(BMS_State_t *bms);
void BMS_Update_State(BMS_State_t *bms);
void BQ_TurnOff_ChargeFET_Only(void);
#endif
