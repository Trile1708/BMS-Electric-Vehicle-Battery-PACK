#include "bq76940.h"
#include "i2c_lcd.h"
#include <stdio.h>
#include "debug.h"
#define BQ_SHUNT_MOHM  1.5f
uint16_t BQ_Gain_uV = 380;
int16_t  BQ_Offset_mV = 0;
//======================OFFSET============================
#define USER_CELL_OFFSET_MV   28
#define USER_CURRENT_OFFSET_MA      28
#define USER_CURRENT_DEADBAND_MA    50
#define STATE_CHARGE_THRESHOLD_MA       500
#define STATE_DISCHARGE_THRESHOLD_MA   -500

//========================================================
//_Bien_I2C_
extern I2C_HandleTypeDef hi2c1; 
volatile uint8_t System_Locked = 0;
//=================FUNCTION PROTOTYPE=================================
void BQ_Read_Factory_Calibration(void);
void BQ_Read_Temperature(BMS_State_t *bms);
static uint8_t BQ_CRC8(uint8_t *data, uint8_t len);
static HAL_StatusTypeDef BQ_WriteReg_CRC(uint8_t regAddr, uint8_t data);
static HAL_StatusTypeDef BQ_ReadBlock_CRC(uint8_t startReg, uint8_t *data, uint8_t len);
static uint8_t BMS_Check_Wire_Broken(BMS_State_t *bms);
//======================================================================
//H�M CHUYEN DOI GIA TRI
static uint16_t BQ_Convert_Cell_mV(uint16_t raw_adc)
{
    int32_t mv;

    /*
       C�ng thuc goc:
       Vcell = ADC * Gain / 1000 + Offset
    */
    mv = (int32_t)(((float)raw_adc * BQ_Gain_uV) / 1000.0f + (float)BQ_Offset_mV + 0.5f);

    
    mv += USER_CELL_OFFSET_MV;

    if (mv < 0)
        mv = 0;

    if (mv > 5000)
        mv = 5000;

    return (uint16_t)mv;
}
//H�m doc CRC8
static uint8_t BQ_CRC8(uint8_t *data, uint8_t len)
{
    uint8_t crc = 0x00;

    for (uint8_t i = 0; i < len; i++)
    {
        crc ^= data[i];

        for (uint8_t bit = 0; bit < 8; bit++)
        {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x07;
            else
                crc <<= 1;
        }
    }

    return crc;
}

//Ham DOC CRC
static HAL_StatusTypeDef BQ_ReadBlock_CRC(uint8_t startReg, uint8_t *data, uint8_t len)
{
    uint8_t rx[64];

    if (len == 0 || len > 32)
        return HAL_ERROR;

    HAL_StatusTypeDef status =
        HAL_I2C_Mem_Read(&hi2c1,
                         BQ_I2C_ADDR,
                         startReg,
                         I2C_MEMADD_SIZE_8BIT,
                         rx,
                         len * 2,
                         100);

    if (status != HAL_OK)
        return status;

    for (uint8_t i = 0; i < len; i++)
    {
        uint8_t d   = rx[i * 2];
        uint8_t crc = rx[i * 2 + 1];
        uint8_t calc;

        if (i == 0)
        {
            uint8_t crc_input[2];

            /*
               V?i read, address byte = 0x10 | 1 = 0x11
            */
            crc_input[0] = BQ_I2C_ADDR | 0x01;
            crc_input[1] = d;

            calc = BQ_CRC8(crc_input, 2);
        }
        else
        {
            calc = BQ_CRC8(&d, 1);
        }

        if (calc != crc)
            return HAL_ERROR;

        data[i] = d;
    }

    return HAL_OK;
}
//HAM DOC THANH GHI
void BQ_WriteReg(uint8_t regAddr, uint8_t data)
{
    HAL_StatusTypeDef status;

    status = BQ_WriteReg_CRC(regAddr, data);

    Debug_I2C_Write_OK = (status == HAL_OK);
}
//_Ham_GHI_(Phan_cung_I2C_se_duoc_chen_vao_day)
static HAL_StatusTypeDef BQ_WriteReg_CRC(uint8_t regAddr, uint8_t data)
{
    uint8_t crc_input[3];
    uint8_t tx[3];

    /*
       BQ_I2C_ADDR trong HAL d� l� d?a ch? 8-bit left-shift:
       0x08 << 1 = 0x10
       V?i write, R/W = 0 n�n address byte = 0x10
    */
    crc_input[0] = BQ_I2C_ADDR;
    crc_input[1] = regAddr;
    crc_input[2] = data;

    tx[0] = regAddr;
    tx[1] = data;
    tx[2] = BQ_CRC8(crc_input, 3);

    HAL_StatusTypeDef status =
        HAL_I2C_Master_Transmit(&hi2c1, BQ_I2C_ADDR, tx, 3, 100);

    Debug_I2C_Write_OK = (status == HAL_OK);

    return status;
}

//_Ham_DOC
uint8_t BQ_ReadReg(uint8_t regAddr)
{
    uint8_t data = 0;

    if (BQ_ReadBlock_CRC(regAddr, &data, 1) != HAL_OK)
    {
        return 0xFF;
    }

    return data;
}

//_Ham_XOA_RAC_KHOI_DONG_
void BQ_ClearFaults(void)
{
    uint8_t stat = BQ_ReadReg(BQ_SYS_STAT);

    /*
       Bit7 = CC_READY      -> kh�ng ph?i l?i
       Bit6 = RSVD
       Bit5 = DEVICE_XREADY
       Bit4 = OVRD_ALERT
       Bit3 = UV
       Bit2 = OV
       Bit1 = SCD
       Bit0 = OCD

       Ch? clear c�c bit l?i/th�ng b�o t? bit0 d?n bit5.
    */
    uint8_t fault_bits = stat & 0x3F;

    if(fault_bits != 0x00)
    {
        BQ_WriteReg(BQ_SYS_STAT, fault_bits);
    }
}

//_HAM_KHOI_TAO_(Se_goi_1_lan_trong_main)
void BQ_Init(void) {
    //_1_Xoa_rac_SYS_STAT
    BQ_ClearFaults();
	  //Read_Gain_offset
    BQ_Read_Factory_Calibration();
	
    //_2_Bat_ADC_de_do_dien_ap_(Ghi_bit_ADC_EN_=_1_vao_SYS_CTRL1)
    BQ_WriteReg(BQ_SYS_CTRL1, 0x18); 
    
    //_3_Cau_hinh_phan_cung_CC_(Datasheet_ep_buoc_luon_phai_ghi_0x19_vao_thanh_ghi_CC_CFG)
    BQ_WriteReg(BQ_CC_CFG, 0x19);
    
    //_4_Bat_CC_de_do_dong_dien_(Ghi_bit_CC_EN_=_1_vao_SYS_CTRL2)
    //_Luu_y_Chua_bat_FET_sac_xa_o_buoc_nay_cho_an_toan
    BQ_WriteReg(BQ_SYS_CTRL2, 0x40); 
	//_=================================================
    //_THIET_LAP_BAO_VE_PHAN_CUNG_(Dua_tren_xe_48V_20A)
    //_=================================================
     //_Bao_ve_Ngan_mach_(SCD)_-_Cat_dien_cuc_nhanh_khi_chap_day_(dong_>_74A)
    BQ_WriteReg(BQ_PROTECT1, 0x93);
    //_Bao_ve_Qua_dong_Xa_(OCD)_-_Cat_dien_khi_dong_vuot_~40A
    //_Tra_bang_Datasheet:_0x05_tuong_duong_nguong_60mV_va_tre_320ms
    BQ_WriteReg(BQ_PROTECT2, 0x58);
    //_Nap_thong_so_LFP_cho_BQ76940
    //_OVP_=_3750mV_(3.75V)_|_UVP_=_2700mV_(2.70V)
    BQ_Set_Protection_Limits(3750, 2700);
    //_Sau_khi_cai_dat_an_toan_xong_xuoi_moi_cho_phep_mo_FET
    BQ_TurnOn_FETs();
    BQ_WriteReg_CRC(BQ_CELLBAL1, 0x00);
    BQ_WriteReg_CRC(BQ_CELLBAL2, 0x00);
    BQ_WriteReg_CRC(BQ_CELLBAL3, 0x00);
}
void BQ_Read_Factory_Calibration(void)
{
    uint8_t gain1 = 0;
    uint8_t gain2 = 0;
    uint8_t offset_u8 = 0;

    if (BQ_ReadBlock_CRC(0x50, &gain1, 1) != HAL_OK)
        return;

    if (BQ_ReadBlock_CRC(0x51, &offset_u8, 1) != HAL_OK)
        return;

    if (BQ_ReadBlock_CRC(0x59, &gain2, 1) != HAL_OK)
        return;

    uint8_t adc_gain_bits =
        (((gain1 >> 2) & 0x03) << 3) |
        ((gain2 >> 5) & 0x07);

    BQ_Gain_uV = 365 + adc_gain_bits;
    BQ_Offset_mV = (int8_t)offset_u8;
}

//_HAM_CAP_NHAT_DU_LIEU_(Se_goi_lien_tuc_trong_vong_lap_while)
void BQ_UpdateData(BMS_State_t *bms)
 {
    uint8_t sys_stat;
    Software_Fault_t voltage_fault;

    /*
       1. Doc SYS_STAT mot lan dau chu ky
       Khong clear fault ngay lap tuc.
    */
    sys_stat = BQ_ReadReg(BQ_SYS_STAT);
    Debug_SYS1 = sys_stat;

    /*
       Neu doc SYS_STAT loi, BQ_ReadReg() dang tra ve 0xFF.
       Khong duoc hieu 0xFF la hardware fault that.
    */
    if (sys_stat == 0xFF)
    {
        bms->I2C_Error_Count++;

        if (bms->I2C_Error_Count > 5)
        {
            bms->Software_Fault = SYS_ERR_I2C_TIMEOUT;
            BQ_TurnOff_FETs();
        }

        return;
    }

    /*
       2. Luu loi phan cung cua BQ76940
       Bit7 CC_READY khong phai loi.
       Bit6 reserved.
       Lay bit0..bit5.
    */
    bms->Hardware_Fault = sys_stat & 0x3F;
    Debug_HardwareFault = bms->Hardware_Fault;

    /*
       3. Doc du lieu chinh
    */
    BQ_Read_All_Voltages(bms);

    /*
       Luu lai loi do dien ap tao ra:
       SYS_ERR_WIRE_BROKEN, SYS_ERR_DATA_CORRUPT, hoac SYS_OK.
    */
    voltage_fault = bms->Software_Fault;

    BQ_Read_Current(bms);
    BQ_Read_Temperature(bms);

    /*
       4. Xu ly Software_Fault theo muc uu tien
    */
    if (bms->I2C_Error_Count > 5)
    {
        bms->Software_Fault = SYS_ERR_I2C_TIMEOUT;
        BQ_TurnOff_FETs();
    }
    else if (voltage_fault == SYS_ERR_WIRE_BROKEN ||
             voltage_fault == SYS_ERR_DATA_CORRUPT)
    {
        bms->Software_Fault = voltage_fault;
        BQ_TurnOff_FETs();
    }
    else if (bms->Temperature[0] > 55.0f)
    {
        bms->Software_Fault = SYS_ERR_OVER_TEMP;
       BQ_TurnOff_FETs();
    }
    else if (bms->Temperature[0] < 0.0f)
    {
        bms->Software_Fault = SYS_ERR_UNDER_TEMP;
        BQ_TurnOff_FETs();
    }
    else
    {
        bms->Software_Fault = SYS_OK;
    }

    /*
       5. Neu BQ bao loi phan cung thi tat FET
       Vi du: OV, UV, OCD, SCD, DEVICE_XREADY.
    */
    if (bms->Hardware_Fault != 0x00)
     {
       BQ_TurnOff_FETs();
     }

    if (bms->Hardware_Fault != 0x00)
    {
        BQ_WriteReg(BQ_SYS_STAT, bms->Hardware_Fault);
    }
//=============================DEBUG===================================  
    Debug_SoftwareFault = bms->Software_Fault;
    Debug_PackVoltage = bms->TotalVoltage;
    Debug_Current = bms->PackCurrent;
    Debug_Temp = bms->Temperature[0];

    for (int i = 0; i < 15; i++)
    {
        Debug_Cell[i] = bms->CellVoltage[i];
    }
    Debug_SYS2 = BQ_ReadReg(BQ_SYS_STAT);
    Debug_SYSCTRL1 = BQ_ReadReg(BQ_SYS_CTRL1);
    Debug_SYSCTRL2 = BQ_ReadReg(BQ_SYS_CTRL2);
    Debug_PROTECT1 = BQ_ReadReg(BQ_PROTECT1);
    Debug_PROTECT2 = BQ_ReadReg(BQ_PROTECT2);
    Debug_PROTECT3 = BQ_ReadReg(BQ_PROTECT3);
    Debug_OV_TRIP  = BQ_ReadReg(BQ_OV_TRIP);
    Debug_UV_TRIP  = BQ_ReadReg(BQ_UV_TRIP);
    Debug_CC_CFG   = BQ_ReadReg(BQ_CC_CFG);
}
//_Ham_BAT_cho_phep_dien_chay_qua_mach
void BQ_TurnOn_FETs(void) {
    //_0x43_doi_ra_nhi_phan_la_0100_0011
    //_Tuc_la_Bat_do_dong_(Bit_6)_+_Bat_FET_Xa_(Bit_1)_+_Bat_FET_Sac_(Bit_0)
    BQ_WriteReg(BQ_SYS_CTRL2, 0x43);
}
void BQ_TurnOff_ChargeFET_Only(void)
  {
    /*
       SYS_CTRL2:
       Bit6 = CC_EN
       Bit1 = DSG_ON
       Bit0 = CHG_ON

       0x42 = 0100 0010
       Gi? do d�ng + gi? FET x? + t?t FET s?c
    */
    BQ_WriteReg(BQ_SYS_CTRL2, 0x42);
  }

//_Ham_TAT_khan_cap_khi_co_su_co
void BQ_TurnOff_FETs(void) {
    //_0x40=0100_0000
    //_Tuc_la_Van_giu_do_dong_(Bit_6)_nhung_Tat_het_FET_(Bit_1_va_Bit_0_=_0)
    BQ_WriteReg(BQ_SYS_CTRL2, 0x40);
}
//_Khai_bao_2_bien_toan_cuc_de_luu_thong_so_nha_may


//_Ham_doc_va_giai_ma_dien_ap
#define BMS_CELL_COUNT      15
#define CELL_MIN_VALID_MV   2000
#define CELL_MAX_VALID_MV   4500

static HAL_StatusTypeDef BQ_Read_5_Cell_Group(uint8_t startReg, uint16_t *cellBuf)
{
    uint8_t data[10];

    HAL_StatusTypeDef status = BQ_ReadBlock_CRC(startReg, data, 10);

    if (status != HAL_OK)
        return status;

    for (int j = 0; j < 5; j++)
    {
        uint16_t adc;

        adc = ((uint16_t)(data[j * 2] & 0x3F) << 8) |
              ((uint16_t)data[j * 2 + 1]);

        cellBuf[j] = BQ_Convert_Cell_mV(adc);
    }

    return HAL_OK;
}

void BQ_Read_All_Voltages(BMS_State_t *bms)
{
    uint16_t cell_temp[BMS_CELL_COUNT];
    uint32_t total_temp = 0;
    HAL_StatusTypeDef status;

    status = BQ_Read_5_Cell_Group(0x0C, &cell_temp[0]);
    if (status != HAL_OK)
        goto I2C_READ_ERROR;

    status = BQ_Read_5_Cell_Group(0x16, &cell_temp[5]);
    if (status != HAL_OK)
        goto I2C_READ_ERROR;

    status = BQ_Read_5_Cell_Group(0x20, &cell_temp[10]);
    if (status != HAL_OK)
        goto I2C_READ_ERROR;

    for (int i = 0; i < BMS_CELL_COUNT; i++)
    {
        total_temp += cell_temp[i];

    }

  
    for (int i = 0; i < BMS_CELL_COUNT; i++)
    {
        bms->CellVoltage[i] = cell_temp[i];
    }

    bms->TotalVoltage = total_temp;
    bms->I2C_Error_Count = 0;

   
    if (BMS_Check_Wire_Broken(bms))
      {
       bms->Software_Fault = SYS_ERR_WIRE_BROKEN;
      }
    else
      {
       if (bms->Software_Fault == SYS_ERR_WIRE_BROKEN ||
           bms->Software_Fault == SYS_ERR_DATA_CORRUPT)
         {
          bms->Software_Fault = SYS_OK;
         }
      }

    return;

I2C_READ_ERROR:
    bms->I2C_Error_Count++;

  
    if (bms->I2C_Error_Count >= 3)
    {
        bms->Software_Fault = SYS_ERR_DATA_CORRUPT;
    }
}
//H�M �OC D�NG �IEN
void BQ_Read_Current(BMS_State_t *bms)
{
    uint8_t data[2];

    if(BQ_ReadBlock_CRC(BQ_CC_HI, data, 2) != HAL_OK)
    {
        bms->I2C_Error_Count++;
        return;
    }

    int16_t cc_raw = ((int16_t)data[0] << 8) | data[1];

    float current_ma = ((float)cc_raw * 8.44f) / BQ_SHUNT_MOHM;

    /*
       Hi?u chu?n d�ng zero-current.
       Khi th?c t? kh�ng s?c/x? nhung BMS d?c kho?ng +309mA,
       ta tr? offset kho?ng 310mA.
    */
    current_ma += USER_CURRENT_OFFSET_MA;

    /*
       V�ng ch?t d? tr�nh h? th?ng nh?y CHG/DSG khi d�ng nh?.
    */
    if(current_ma > -USER_CURRENT_DEADBAND_MA &&
       current_ma <  USER_CURRENT_DEADBAND_MA)
    {
        current_ma = 0;
    }
		

    if(current_ma > 50000)
        current_ma = 50000;

    if(current_ma < -50000)
        current_ma = -50000;

    bms->PackCurrent = (int16_t)current_ma;
    bms->I2C_Error_Count = 0;
}
//_Ham_Cai_dat_Nguong_Bao_ve_Ap_(OVP_va_UVP)
void BQ_Set_Protection_Limits(uint16_t OVP_mV, uint16_t UVP_mV) {
    //_1_Tinh_toan_gia_tri_Hex_cho_nguong_Qua_Ap_(OV_TRIP)
    //_Cong_thuc:_ADC_tho_=_((Dien_ap_-_Offset)_*_1000)_/_Gain
    uint32_t adc_ov = ((uint32_t)OVP_mV - BQ_Offset_mV) * 1000 / BQ_Gain_uV;
    
    //_Dich_phai_4_bit_(>>_4)_de_cat_bo_4_bit_duoi
    //_AND_voi_0xFF_de_lay_dung_1_Byte_nh�t_vao_thanh_ghi
    uint8_t ov_trip_hex = (adc_ov >> 4) & 0xFF;
    BQ_WriteReg(BQ_OV_TRIP, ov_trip_hex);
    
    //_2_Tinh_toan_gia_tri_Hex_cho_nguong_Tut_Ap_(UV_TRIP)
    uint32_t adc_uv = ((uint32_t)UVP_mV - BQ_Offset_mV) * 1000 / BQ_Gain_uV;
    uint8_t uv_trip_hex = (adc_uv >> 4) & 0xFF;
    BQ_WriteReg(BQ_UV_TRIP, uv_trip_hex);
}
void BMS_Calculate_SOC(BMS_State_t *bms) 
	{
    uint32_t current_tick = HAL_GetTick();
    uint32_t delta_t_ms = current_tick - bms->Last_Tick_ms;
    
    bms->Last_Tick_ms = current_tick;
    
    // 4. T�nh to�n luong dien (mAh) vua chay qua mach
    // LUU �: PackCurrent duong (+) l� Sac v�o, �m (-) l� Xa ra
	if (bms->SystemState != BMS_STATE_FAULT) 
		{
    float delta_capacity_mAh = ((float)bms->PackCurrent * (float)delta_t_ms) / 3600000.0f;
    
    // 5. Cong don v�o Dung luong hien tai (T�ch ph�n)
    bms->RemainingCapacity_mAh += delta_capacity_mAh;
    
    if (bms->RemainingCapacity_mAh > bms->DesignCapacity_mAh)
			{
        bms->RemainingCapacity_mAh = bms->DesignCapacity_mAh;
      }
		else if (bms->RemainingCapacity_mAh < 0.0f) 
			{
        bms->RemainingCapacity_mAh = 0.0f;
      }
    // 7. T�nh xuat ra so phan tram (%) cho M�n h�nh hien thi
    bms->SOC = (bms->RemainingCapacity_mAh / bms->DesignCapacity_mAh) * 100.0f;
    }
	}
//==========================CHECK FULL-CHARGE=====================
#define FULL_CELL_MV          3550
#define FULL_TAIL_CURRENT_MA  500
#define FULL_MIN_CURRENT_MA   50
#define FULL_CONFIRM_TIME_MS  5000

void BMS_Check_Full_Charge(BMS_State_t *bms)
{
    static uint32_t full_start_tick = 0;

    uint16_t max_cell_mv = 0;

    for (int i = 0; i < 15; i++)
    {
        if (bms->CellVoltage[i] > max_cell_mv)
        {
            max_cell_mv = bms->CellVoltage[i];
        }
    }

    /*
       Dieu kien sac gan day:
       - Dang co dong sac vao
       - Dong sac da nho lai
       - Cell cao nhat dat nguong day
    */
    if ((bms->PackCurrent > FULL_MIN_CURRENT_MA) &&
        (bms->PackCurrent < FULL_TAIL_CURRENT_MA) &&
        (max_cell_mv >= FULL_CELL_MV))
    {
        if (full_start_tick == 0)
        {
            full_start_tick = HAL_GetTick();
        }

        if ((HAL_GetTick() - full_start_tick) >= FULL_CONFIRM_TIME_MS)
        {
            bms->RemainingCapacity_mAh = bms->DesignCapacity_mAh;
            bms->SOC = 100.0f;

            /*
               Sac day: chi tat FET sac, khong tat FET xa.
            */
            BQ_TurnOff_ChargeFET_Only();

            bms->ChargeFET_On = 0;
            bms->DischargeFET_On = 1;
        }
    }
    else
    {
        full_start_tick = 0;
    }
}
// Bang tra cuu NTC 10k B=3950 (�on vi: Ohm)
const float NTC_Table_R[17] = {
    33200.0f, 25580.0f, 20000.0f, 15760.0f, 12510.0f, 10000.0f, 8048.0f, 
    6518.0f,  5312.0f,  4354.0f,  3588.0f,  2974.0f,  2476.0f,  2072.0f, 
    1743.0f,  1473.0f,  1250.0f
};

// Mang nhiet do tuong ung tu 0�C den 80�C 
const float NTC_Table_T[17] = {
    0.0f,  5.0f,  10.0f, 15.0f, 20.0f, 25.0f, 30.0f, 
    35.0f, 40.0f, 45.0f, 50.0f, 55.0f, 60.0f, 65.0f, 
    70.0f, 75.0f, 80.0f
};

float Get_Temp_From_LUT(float r_measured) {
    // 1. Chan gioi han tr�n/duoi 
    if (r_measured >= NTC_Table_R[0]) return NTC_Table_T[0];   
    if (r_measured <= NTC_Table_R[16]) return NTC_Table_T[16]; 

    // 2. T�m vi tr� cua dien tro trong bang
    for (int i = 0; i < 16; i++) {
        if (r_measured <= NTC_Table_R[i] && r_measured > NTC_Table_R[i+1]) {
            float r_diff = NTC_Table_R[i] - NTC_Table_R[i+1];
            float t_diff = NTC_Table_T[i+1] - NTC_Table_T[i];
            float r_offset = NTC_Table_R[i] - r_measured;
            
					  float cal_temp = NTC_Table_T[i] + (r_offset / r_diff) * t_diff;
            return cal_temp;
        }
    }
    return 25.0f; // Tra ve nhiet do ph�ng neu c� loi logic
}
void BQ_Read_Temperature(BMS_State_t *bms)
{
    uint8_t data[2];

    /*
       BQ7694003 c� CRC, n�n ph?i d?c b?ng BQ_ReadBlock_CRC.
       data[0] = TS1_HI
       data[1] = TS1_LO
    */
    if(BQ_ReadBlock_CRC(BQ_TS1_HI, data, 2) != HAL_OK)
    {
        bms->I2C_Error_Count++;
        return;
    }

    uint16_t ts1_adc = ((data[0] & 0x3F) << 8) | data[1];

    if(ts1_adc == 0)
        return;

    /*
       Theo datasheet:
       VTSX = ADC * 382 uV
       RTS = 10000 * VTSX / (3.3 - VTSX)
    */
    float v_tsx = (float)ts1_adc * 0.000382f;

    if(v_tsx > 0.05f && v_tsx < 3.25f)
    {
        float r_ntc = (10000.0f * v_tsx) / (3.3f - v_tsx);
        bms->Temperature[0] = Get_Temp_From_LUT(r_ntc);
    }

    bms->I2C_Error_Count = 0;
}
	void BMS_Update_State(BMS_State_t *bms)
{
    if(System_Locked == 1 ||
       bms->Hardware_Fault != 0x00 ||
       bms->Software_Fault != SYS_OK)
    {
        bms->SystemState = BMS_STATE_FAULT;
        return;
    }

    if(bms->PackCurrent > STATE_CHARGE_THRESHOLD_MA)
    {
        bms->SystemState = BMS_STATE_CHARGING;
        return;
    }

    if(bms->PackCurrent < STATE_DISCHARGE_THRESHOLD_MA)
    {
        bms->SystemState = BMS_STATE_DISCHARGING;
        return;
    }

    bms->SystemState = BMS_STATE_IDLE;
}
//=====================================LCD==========================================
#define LCD_BUTTON_GPIO_PORT    GPIOA
#define LCD_BUTTON_PIN          GPIO_PIN_0

#define LCD_TOTAL_PAGE          4
#define LCD_UPDATE_PERIOD_MS    200
#define LCD_DEBOUNCE_MS         50
static const char* BMS_LCD_StateString(BMS_SystemState_t state)
{
    switch (state)
    {
        case BMS_STATE_IDLE:
            return "IDL";

        case BMS_STATE_CHARGING:
            return "CHG";

        case BMS_STATE_DISCHARGING:
            return "DSG";

        case BMS_STATE_FAULT:
            return "FLT";

        case BMS_STATE_SLEEP:
            return "SLP";

        default:
            return "INI";
    }
}

/*
   LCD 20x04 HD44780 co dia chi DDRAM:
   Row 0: 0x00
   Row 1: 0x40
   Row 2: 0x14
   Row 3: 0x54

   Khong dung lcd_goto_XY() cu vi ham do chi ho tro 2 dong.
*/
static void BMS_LCD20x4_SetCursor(uint8_t row, uint8_t col)
{
    static const uint8_t row_addr[4] = {0x00, 0x40, 0x14, 0x54};

    if (row > 3)
        row = 0;

    if (col > 19)
        col = 19;

    lcd_send_cmd((char)(0x80 | (row_addr[row] + col)));
}

/*
   In dung 20 ky tu moi dong.
   Neu chuoi ngan hon 20 ky tu thi them khoang trang de xoa ky tu cu.
*/
static void BMS_LCD20x4_PrintLine(uint8_t row, const char *text)
{
    char line[21];

    for (int i = 0; i < 20; i++)
    {
        if (text[i] != '\0')
            line[i] = text[i];
        else
        {
            for (int j = i; j < 20; j++)
                line[j] = ' ';
            break;
        }
    }

    line[20] = '\0';

    BMS_LCD20x4_SetCursor(row, 0);
    lcd_send_string(line);
}
static const char* BMS_LCD_SoftwareFaultString(Software_Fault_t fault)
{
    switch (fault)
    {
        case SYS_OK:
            return "SYSTEM NORMAL";

        case SYS_ERR_I2C_TIMEOUT:
            return "ERR:I2C TIMEOUT";

        case SYS_ERR_DATA_CORRUPT:
            return "ERR:DATA CORRUPT";

        case SYS_ERR_OVER_TEMP:
            return "ERR:OVER TEMP";

        case SYS_ERR_UNDER_TEMP:
            return "ERR:UNDER TEMP";
				case SYS_ERR_WIRE_BROKEN:
            return "ERR:WIRE BROKEN";
        default:
            return "ERR:UNKNOWN";
    }
}
static const char* BMS_LCD_HardwareFaultString(uint8_t hw)
 {
    if (hw & HW_FAULT_DEVICE_XREADY)
        return "HW:DEVICE_XREADY";

    if (hw & HW_FAULT_OVRD_ALERT)
        return "HW:OVRD_ALERT";

    if (hw & HW_FAULT_UV)
        return "HW:UV";

    if (hw & HW_FAULT_OV)
        return "HW:OV";

    if (hw & HW_FAULT_SCD)
        return "HW:SCD";

    if (hw & HW_FAULT_OCD)
        return "HW:OCD";

    return "HW:NORMAL";
 }
extern volatile uint8_t lcd_next_page_request;
 void BMS_Update_LCD(BMS_State_t *bms)
{
    static uint8_t lcd_page = 0;
    static uint8_t last_page = 255;
    static uint32_t last_update_tick = 0;

    char lcd_buf[32];
    uint32_t now = HAL_GetTick();

    /*
       Nhan PA0 de doi trang
       PA0 dung Pull-up:
       - Khong nhan = GPIO_PIN_SET
       - Nhan       = GPIO_PIN_RESET
    */
    if (lcd_next_page_request)
     {
      lcd_next_page_request = 0;

      lcd_page++;

    if (lcd_page >= LCD_TOTAL_PAGE)
        lcd_page = 0;

      last_page = 255;
     }

    /*
       Khong cap nhat LCD qua nhanh
    */
    if ((now - last_update_tick) < LCD_UPDATE_PERIOD_MS &&
        lcd_page == last_page)
    {
        return;
    }

    last_update_tick = now;

    if (lcd_page != last_page)
    {
        lcd_clear();
        last_page = lcd_page;
    }

    switch (lcd_page)
    {
        case 0:
        {
            int32_t current = bms->PackCurrent;
            uint32_t abs_current;
            char sign;

            if (current < 0)
            {
                sign = '-';
                abs_current = (uint32_t)(-current);
            }
            else
            {
                sign = '+';
                abs_current = (uint32_t)current;
            }

            snprintf(lcd_buf,
                     sizeof(lcd_buf),
                     "BMS:%s SOC:%03d%%",
                     BMS_LCD_StateString(bms->SystemState),
                     (int)bms->SOC);
            BMS_LCD20x4_PrintLine(0, lcd_buf);

            snprintf(lcd_buf,
                     sizeof(lcd_buf),
                     "Pack:%lu.%03luV",
                     (unsigned long)(bms->TotalVoltage / 1000),
                     (unsigned long)(bms->TotalVoltage % 1000));
            BMS_LCD20x4_PrintLine(1, lcd_buf);

            snprintf(lcd_buf,
                     sizeof(lcd_buf),
                     "I:%c%lu.%03luA T:%02dC",
                     sign,
                     (unsigned long)(abs_current / 1000),
                     (unsigned long)(abs_current % 1000),
                     (int)bms->Temperature[0]);
            BMS_LCD20x4_PrintLine(2, lcd_buf);

            snprintf(lcd_buf,
                     sizeof(lcd_buf),
                     "HW:%u SW:%d I2C:%u",
                     (unsigned int)bms->Hardware_Fault,
                     (int)bms->Software_Fault,
                     (unsigned int)bms->I2C_Error_Count);
            BMS_LCD20x4_PrintLine(3, lcd_buf);
            break;
        }

        case 1:
        {
            for (int row = 0; row < 4; row++)
            {
                int c1 = row * 2;
                int c2 = c1 + 1;

                snprintf(lcd_buf,
                         sizeof(lcd_buf),
                         "%02d:%u.%03u %02d:%u.%03u",
                         c1 + 1,
                         (unsigned int)(bms->CellVoltage[c1] / 1000),
                         (unsigned int)(bms->CellVoltage[c1] % 1000),
                         c2 + 1,
                         (unsigned int)(bms->CellVoltage[c2] / 1000),
                         (unsigned int)(bms->CellVoltage[c2] % 1000));

                BMS_LCD20x4_PrintLine((uint8_t)row, lcd_buf);
            }
            break;
        }

        case 2:
        {
            for (int row = 0; row < 3; row++)
            {
                int c1 = 8 + row * 2;
                int c2 = c1 + 1;

                snprintf(lcd_buf,
                         sizeof(lcd_buf),
                         "%02d:%u.%03u %02d:%u.%03u",
                         c1 + 1,
                         (unsigned int)(bms->CellVoltage[c1] / 1000),
                         (unsigned int)(bms->CellVoltage[c1] % 1000),
                         c2 + 1,
                         (unsigned int)(bms->CellVoltage[c2] / 1000),
                         (unsigned int)(bms->CellVoltage[c2] % 1000));

                BMS_LCD20x4_PrintLine((uint8_t)row, lcd_buf);
            }

            {
                uint16_t max_cell = bms->CellVoltage[0];
                uint16_t min_cell = bms->CellVoltage[0];
                uint16_t delta_cell = 0;

                for (int i = 1; i < 15; i++)
                {
                    if (bms->CellVoltage[i] > max_cell)
                        max_cell = bms->CellVoltage[i];

                    if (bms->CellVoltage[i] < min_cell)
                        min_cell = bms->CellVoltage[i];
                }

                if (max_cell >= min_cell)
                    delta_cell = max_cell - min_cell;

                snprintf(lcd_buf,
                         sizeof(lcd_buf),
                         "15:%u.%03u D:%u.%03u",
                         (unsigned int)(bms->CellVoltage[14] / 1000),
                         (unsigned int)(bms->CellVoltage[14] % 1000),
                         (unsigned int)(delta_cell / 1000),
                         (unsigned int)(delta_cell % 1000));

                BMS_LCD20x4_PrintLine(3, lcd_buf);
            }
            break;
        }

        case 3:
         default:
          {
            snprintf(lcd_buf,
                     sizeof(lcd_buf),
                    "STATE:%s",
                     BMS_LCD_StateString(bms->SystemState));
            BMS_LCD20x4_PrintLine(0, lcd_buf);

            snprintf(lcd_buf,
                     sizeof(lcd_buf),
                    "HW:%u SW:%d I2C:%u",
                    (unsigned int)bms->Hardware_Fault,
                    (int)bms->Software_Fault,
                    (unsigned int)bms->I2C_Error_Count);
           BMS_LCD20x4_PrintLine(1, lcd_buf);

           snprintf(lcd_buf,
                    sizeof(lcd_buf),
                   "CHG:%u DSG:%u",
                   (unsigned int)bms->ChargeFET_On,
                   (unsigned int)bms->DischargeFET_On);
           BMS_LCD20x4_PrintLine(2, lcd_buf);

           if (bms->Hardware_Fault != 0)
              {
                BMS_LCD20x4_PrintLine(3,
                                       BMS_LCD_HardwareFaultString(bms->Hardware_Fault));
              }
           else
              {
                BMS_LCD20x4_PrintLine(3,
                                      BMS_LCD_SoftwareFaultString(bms->Software_Fault));
              }
            break;
           }
    }
}
  uint8_t BMS_Is_System_Safe(BMS_State_t *bms)
    {
     if (bms->Software_Fault != SYS_OK)
        return 0;

     if (bms->Hardware_Fault != 0x00)
        return 0;

    for (int i = 0; i < 15; i++)
     {
        if (bms->CellVoltage[i] < 2800)
            return 0;

        if (bms->CellVoltage[i] > 3650)
            return 0;
     }

    if (bms->Temperature[0] > 50)
        return 0;

    return 1;
}
//====================================
#define CELL_WIRE_MIN_MV        2500
#define CELL_WIRE_MAX_MV        3900
#define CELL_PAIR_SUM_MIN_MV    6200
#define CELL_PAIR_SUM_MAX_MV    7000
#define CELL_PAIR_DIFF_MV       500

static uint8_t BMS_Check_Wire_Broken(BMS_State_t *bms)
{
    /*
       1. Bat loi cell qua thap / qua cao theo mien dien ap LFP thuc te.
    */
    for (int i = 0; i < 15; i++)
    {
        if (bms->CellVoltage[i] < CELL_WIRE_MIN_MV ||
            bms->CellVoltage[i] > CELL_WIRE_MAX_MV)
        {
            return 1;
        }
    }

    /*
       2. Bat loi dut day sense giua 2 cell.
       Mau loi thuong gap:
       - Mot cell bi thap
       - Cell ke ben bi cao
       - Tong hai cell van gan dung
    */
    for (int i = 0; i < 14; i++)
    {
        uint16_t c1 = bms->CellVoltage[i];
        uint16_t c2 = bms->CellVoltage[i + 1];
        uint16_t sum = c1 + c2;
        uint16_t diff = (c1 > c2) ? (c1 - c2) : (c2 - c1);

        if (sum >= CELL_PAIR_SUM_MIN_MV &&
            sum <= CELL_PAIR_SUM_MAX_MV &&
            diff >= CELL_PAIR_DIFF_MV)
        {
            return 1;
        }
    }

    return 0;
}

