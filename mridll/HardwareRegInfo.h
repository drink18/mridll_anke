#pragma once
#define GRADIENT_OFFSET_LW_X (0x132*2)
#define GRADIENT_OFFSET_HI_X (0x133*2)
#define GRADIENT_OFFSET_LW_Y (0x172*2)
#define GRADIENT_OFFSET_HI_Y (0x173*2) 
#define GRADIENT_OFFSET_LW_Z (0x1B2*2)
#define GRADIENT_OFFSET_HI_Z (0x1B3*2) 

#define GRADIENT_OFFSET_LW_B0 (0x1F0*2)
#define GRADIENT_OFFSET_HI_B0 (0x1F1*2) 

#define MAIN_CTRL_SEQ_CLR     (0x80*2)


//#define SYS_CTRL_REG1      (0x1*2)
#define TX_B1_CTRL_REG    (0x301*2)
#define TX_B2_CTRL_REG    (0x581*2)
#define RX_B1_CTRL_REG    (0x1001*2)
#define RX_B2_CTRL_REG    (0x1201*2)
#define RX_B3_CTRL_REG    (0x1401*2)
#define RX_B4_CTRL_REG    (0x1601*2)
#define SYS_READY_LED     (0x7*2)
#define LINK_STATUS_LED   (0x9*2)
//1:Start,0:Pause
#define MAIN_CTRL_CONTROL_REG (0x84*2)

#define MAINCTRL_ID_VALUE0 (0x90*2)
//1:Setup mode 0:RUn
#define MAIN_SETUP_RUN_MODE   (0x9A*2)

//judge if the MAINCTRL is END
#define MAINCTRL_SEQ_END_REG  (0x50000000+0x92*2)

#define GRADIENT_CONTROL_REG (0x117*2)
#define GRADIENT_SEQ_CLR_REG (0x113*2)

#define TX1_CONTROL_REG       (0x313*2)
#define TX1_SEQ_CLR_REG       (0x30F*2)


#define TX2_CONTROL_REG       (0x593*2)
#define TX2_SEQ_CLR_REG       (0x58F*2)

#define RX1_CONTROL_REG       (0x1014*2)
#define RX1_SEQ_CLR_REG       (0x1010*2)

#define RX2_CONTROL_REG       (0x1214*2)
#define RX2_SEQ_CLR_REG       (0x1210*2)

#define RX3_CONTROL_REG       (0x1414*2)
#define RX3_SEQ_CLR_REG       (0x1410*2)

#define RX4_CONTROL_REG       (0x1614*2)
#define RX4_SEQ_CLR_REG       (0x1610*2)

#define  MAIN_CTRL_COM_FIFO_CONFIG1  (0x85*2)
#define  MAIN_CTRL_COM_FIFO_CONFIG2  (0x86*2)
#define  MAIN_CTRL_COM_FIFO_CONFIG3  (0x87*2)
#define  MAIN_CTRL_COM_FIFO_CONFIG4  (0x88*2)
#define  MAIN_CTRL_COM_FIFO_CONFIG5  (0x89*2)

//RAM addr M
#define MAINCTRL_CTRL_RAM_DATA_1  0x114
#define MAINCTRL_CTRL_RAM_DATA_2  0x116
#define MAINCTRL_CTRL_RAM_DATA_3  0x118
#define MAINCTRL_CTRL_RAM_DATA_4  0x11A
#define MAINCTRL_CTRL_RAM_ADDR	  0x11C

#define  GRADP_COM_FIFO_CONFIG1  (0x247*2)
#define  GRADP_COM_FIFO_CONFIG2  (0x248*2)
#define  GRADP_COM_FIFO_CONFIG3  (0x249*2)
#define  GRADP_COM_FIFO_CONFIG4  (0x24A*2)
#define  GRADP_COM_FIFO_CONFIG5  (0x24B*2)

#define GRADP_RAM_DATA_1 	0x49C
#define GRADP_RAM_DATA_2 	0x49E
#define GRADP_RAM_DATA_3 	0x4A0
#define GRADP_RAM_DATA_4 	0x4A2
#define GRADP_RAM_ADDR 		0x4A4


#define  GRADS_COM_FIFO_CONFIG1  (0x118*2)
#define  GRADS_COM_FIFO_CONFIG2  (0x119*2)
#define  GRADS_COM_FIFO_CONFIG3  (0x11A*2)
#define  GRADS_COM_FIFO_CONFIG4  (0x11B*2)
#define  GRADS_COM_FIFO_CONFIG5  (0x11C*2)


#define GRADS_RAM_DATA_1 	0x23E
#define GRADS_RAM_DATA_2 	0x240
#define GRADS_RAM_DATA_3 	0x242
#define GRADS_RAM_DATA_4 	0x244
#define GRADS_RAM_ADDR 		0x246

#define  GRADR_COM_FIFO_CONFIG1  (0x227*2)
#define  GRADR_COM_FIFO_CONFIG2  (0x228*2)
#define  GRADR_COM_FIFO_CONFIG3  (0x229*2)
#define  GRADR_COM_FIFO_CONFIG4  (0x22A*2)
#define  GRADR_COM_FIFO_CONFIG5  (0x22B*2)


#define GRADR_RAM_DATA_1 	0x45C
#define GRADR_RAM_DATA_2 	0x45E
#define GRADR_RAM_DATA_3 	0x460
#define GRADR_RAM_DATA_4 	0x462
#define GRADR_RAM_ADDR 		0x464



#define  TX_B1_COM_FIFO_CONFIG1  (0x314*2)
#define  TX_B1_COM_FIFO_CONFIG2  (0x315*2)
#define  TX_B1_COM_FIFO_CONFIG3  (0x316*2)
#define  TX_B1_COM_FIFO_CONFIG4  (0x317*2)
#define  TX_B1_COM_FIFO_CONFIG5  (0x318*2)

#define TX1_RAM_DATA_1 	0x636
#define TX1_RAM_DATA_2 	0x638
#define TX1_RAM_DATA_3 	0x63A
#define TX1_RAM_DATA_4 	0x63C
#define TX1_RAM_ADDR 	0x63E

#define  TX_B2_COM_FIFO_CONFIG1  (0x594*2)
#define  TX_B2_COM_FIFO_CONFIG2  (0x595*2)
#define  TX_B2_COM_FIFO_CONFIG3  (0x596*2)
#define  TX_B2_COM_FIFO_CONFIG4  (0x597*2)
#define  TX_B2_COM_FIFO_CONFIG5  (0x598*2)

#define TX2_RAM_DATA_1 	(0x59b*2)
#define TX2_RAM_DATA_2 	(0x59c*2)
#define TX2_RAM_DATA_3 	(0x59d*2)
#define TX2_RAM_DATA_4 	(0x59e*2)
#define TX2_RAM_ADDR 	(0x59f*2)

#define  RX_B1_COM_FIFO_CONFIG1  (0x1015*2)
#define  RX_B1_COM_FIFO_CONFIG2  (0x1016*2)
#define  RX_B1_COM_FIFO_CONFIG3  (0x1017*2)
#define  RX_B1_COM_FIFO_CONFIG4  (0x1018*2)
#define  RX_B1_COM_FIFO_CONFIG5  (0x1019*2)

#define RX1_RAM_DATA_1 	(0x101c*2)
#define RX1_RAM_DATA_2 	(0x101d*2)
#define RX1_RAM_DATA_3 	(0x101e*2)
#define RX1_RAM_DATA_4 	(0x101f*2)
#define RX1_RAM_ADDR 	(0x1020*2)

#define  RX_B2_COM_FIFO_CONFIG1  (0x1215*2)
#define  RX_B2_COM_FIFO_CONFIG2  (0x1216*2)
#define  RX_B2_COM_FIFO_CONFIG3  (0x1217*2)
#define  RX_B2_COM_FIFO_CONFIG4  (0x1218*2)
#define  RX_B2_COM_FIFO_CONFIG5  (0x1219*2)

#define RX2_RAM_DATA_1 	(0x121c*2)
#define RX2_RAM_DATA_2 	(0x121d*2)
#define RX2_RAM_DATA_3 	(0x121e*2)
#define RX2_RAM_DATA_4 	(0x121f*2)
#define RX2_RAM_ADDR 	(0x1220*2)

#define  RX_B3_COM_FIFO_CONFIG1  (0x1415*2)
#define  RX_B3_COM_FIFO_CONFIG2  (0x1416*2)
#define  RX_B3_COM_FIFO_CONFIG3  (0x1417*2)
#define  RX_B3_COM_FIFO_CONFIG4  (0x1418*2)
#define  RX_B3_COM_FIFO_CONFIG5  (0x1419*2)

#define RX3_RAM_DATA_1 	(0x141c*2)
#define RX3_RAM_DATA_2 	(0x141d*2)
#define RX3_RAM_DATA_3 	(0x141e*2)
#define RX3_RAM_DATA_4 	(0x141f*2)
#define RX3_RAM_ADDR 	(0x1420*2)

#define  RX_B4_COM_FIFO_CONFIG1  (0x1615*2)
#define  RX_B4_COM_FIFO_CONFIG2  (0x1616*2)
#define  RX_B4_COM_FIFO_CONFIG3  (0x1617*2)
#define  RX_B4_COM_FIFO_CONFIG4  (0x1618*2)
#define  RX_B4_COM_FIFO_CONFIG5  (0x1619*2)

#define RX4_RAM_DATA_1 	(0x161c*2)
#define RX4_RAM_DATA_2 	(0x161d*2)
#define RX4_RAM_DATA_3 	(0x161e*2)
#define RX4_RAM_DATA_4 	(0x161f*2)
#define RX4_RAM_ADDR 	(0x1620*2)

#define  GRADIENT_PEF_X          0x13a
#define  GRADIENT_PEF_Y          0x17a
#define  GRADIENT_PEF_Z          0x1Ba
#define  GRADIENT_PEF_B0         0x1F2

#define  GRAIDENT_ROTA_X         0x134
#define  GRAIDENT_ROTA_Y         0x174
#define  GRAIDENT_ROTA_Z         0x1B4

#define  GRAIDENT_SCALE_X         0x152
#define  GRAIDENT_SCALE_Y         0x192
#define  GRAIDENT_SCALE_Z         0x1D2

#define  GRAIDENT_CLR_REG         0x100
#define  GRAIDENT_WAVE_RAM_CLR    0x1
#define  GRAIDENT_PARAM_RAM_CLR   0x3f
#define  GRAIDENT_MATRIX_RAM_CLR  0x4
#define  GRAIDENT_SCALE_X_RAM_CLR 0x8
#define  GRAIDENT_SCALE_Y_RAM_CLR 0x10
#define  GRAIDENT_SCALE_Z_RAM_CLR 0x20

#define  GRAIDENT_PARAM_RAM_REG1  (0x10D*2)
#define  GRAIDENT_PARAM_RAM_REG2  (0x10E*2)
#define  GRAIDENT_PARAM_RAM_REG3  (0x10F*2)
#define  GRAIDENT_PARAM_RAM_REG4  (0x110*2)

#define  GRADIENT_WAVE_RAM_LOW_REG (0x10B*2)
#define  GRADIENT_WAVE_RAM_HIGH_REG (0x10C*2)

#define  TX1_BRAM_WR_ADDR_CLR          (0x380*2)
#define  TX2_BRAM_WR_ADDR_CLR          (0x600*2)
#define  TX1_BRAM_WR_SEL              (0x381*2)
#define  TX1_BRAM_WDATA_I             (0x382*2)
#define  TX1_BRAM_WDATA_H             (0x383*2)
#define  TX1_C1_TYPE                  0x387
#define  TX1_C1_BRAM_RD_POINT         0x3DB



#define  TX2_BRAM_WR_SEL              (0x601*2)
#define  TX2_BRAM_WDATA_I             (0x602*2)
#define  TX2_BRAM_WDATA_H             (0x603*2)
#define  TX2_C1_TYPE                  0x607
#define  TX2_C1_BRAM_RD_POINT         0x65B

#define  TX1_C1_NCO_FREQ_I            0x3FB

#define  TX2_C1_NCO_FREQ_I            0x67B

#define  TX1_C1_NCO_PHASE_I            0x413

#define  TX2_C1_NCO_PHASE_I            0x693

#define  TX1_C1_GAIN_I            0x42B

#define  TX2_C1_GAIN_I            0x6AB

#define  TX1_C1_NCO_CENTER_FREQ_I   0x43c

#define  TX2_C1_NCO_CENTER_FREQ_I   0x6BC

#define  RX1_CH1_RX_POINT_L    0x106D
#define  RX2_CH1_RX_POINT_L    0x126D
#define  RX3_CH1_RX_POINT_L    0x146D
#define  RX4_CH1_RX_POINT_L    0x166D
#define  RX1_ADC_OFF_EN        0x1024
#define  RX2_ADC_OFF_EN        0x1224

#define  RX3_ADC_OFF_EN        0x1424

#define  RX4_ADC_OFF_EN        0x1624
#define  RX1_RECEIVE_BYPASS    0x1086
#define  RX2_RECEIVE_BYPASS    0x1286
#define  RX3_RECEIVE_BYPASS    0x1486
#define  RX4_RECEIVE_BYPASS    0x1686
#define  RX1_RECEIVE_START     0x107D
#define  RX2_RECEIVE_START     0x127D
#define  RX3_RECEIVE_START     0x147D
#define  RX4_RECEIVE_START     0x167D
#define  RX1_C1_TYPE            0x1044
#define  RX2_C1_TYPE            0x1244
#define  RX3_C1_TYPE            0x1444
#define  RX4_C1_TYPE            0x1644

#define  RX1_C1_NCO_FREQ_I            0x1088

#define  RX2_C1_NCO_FREQ_I            0x1288

#define  RX3_C1_NCO_FREQ_I            0x1488

#define  RX4_C1_NCO_FREQ_I            0x1688

#define  RX1_C1_NCO_PHASE_I            0x10A0

#define  RX2_C1_NCO_PHASE_I            0x12A0

#define  RX3_C1_NCO_PHASE_I            0x14A0

#define  RX4_C1_NCO_PHASE_I            0x16A0


#define  GRADIENT_X_SPI_CMD            (0x1E*2)

#define  GRADIENT_X_SPI_DATA            (0x1F*2)

#define  GRADIENT_X_SPI_ADDR            (0x20*2)

#define  GRADIENT_Y_SPI_CMD            (0x21*2)

#define  GRADIENT_Y_SPI_DATA            (0x22*2)

#define  GRADIENT_Y_SPI_ADDR            (0x23*2)

#define  GRADIENT_Z_SPI_CMD            (0x24*2)

#define  GRADIENT_Z_SPI_DATA            (0x25*2)

#define  GRADIENT_Z_SPI_ADDR            (0x26*2)

#define  GRADIENT_B0_SPI_CMD            (0x27*2)

#define  GRADIENT_B0_SPI_DATA            (0x28*2)

#define  GRADIENT_B0_SPI_ADDR            (0x29*2)

#define  GRADIENT_DAC_CFG                (0x2A*2)


#define  TX1_DATABUS_CMD_REG_ADDR (0x308*2)
#define  TX1_DATABUS_DATA_REG_ADDR (0x309*2)
#define  TX1_DATABUS_ADDR_REG_ADDR (0x30A*2)


///TX cali
#define  TX1_GAIN_SEL             (0x3FA*2)
#define  TX1_C1_DUC_GAIN_DB        0x3C8
#define  TX1_C1_COMP_GAIN_DB       0x3C9

#define  TX2_GAIN_SEL             (0x67A*2)
#define  TX2_C1_DUC_GAIN_DB        0x648
#define  TX2_C1_COMP_GAIN_DB       0x649

#define		TX1_PWD        (0x320*2)
#define		TX2_PWD        (0x5A0*2)
#define     WRITE_END      (0x48*2)
#define  TX_DAC_CH1_LW  20
#define  TX_DAC_CH1_HI  21

#define  TX_DAC_CH2_LW  22
#define  TX_DAC_CH2_HI  23

#define  TX1_DAC_CFG            (0x30E*2) 
 
#define  TX2_DAC_CFG            (0x58E*2)

#define  RX1_C1_NCO_CENTER_FREQ_L (0x10B9)

#define  RX2_C1_NCO_CENTER_FREQ_L (0x12B9)

#define  RX3_C1_NCO_CENTER_FREQ_L (0x14B9)

#define  RX4_C1_NCO_CENTER_FREQ_L (0x16B9*2)
//RX Cali
#define  RX1_C1_GAIN_DB     0x107e
#define  RX1_C1_G1          0x10d1
#define  RX1_C1_G2          0x10d9
#define  RX1_C1_G3          0x10e1
#define  RX1_C1_ATT0        0x10e9
#define  RX1_C1_AMP_CTRL    0x1030

#define  SYS_CTRL_REG       (0x1*2)

#define  CTRL_BR_CHIP_ID    (0x0)
#define  TX1_B1_CHIP_ID      (0x300*2)
#define  TX2_B1_CHIP_ID      (0x580*2)
#define  RX1_B1_CHIP_ID      (0x1000*2)
#define  RX2_B1_CHIP_ID      (0x1200*2)
#define  RX3_B1_CHIP_ID      (0x1400*2)
#define  RX4_B1_CHIP_ID      (0x1600*2)

#define  CTRL_FPGA_MONITOR_ALARM (0x60*2)
#define  TX1_FPGA_MONITOR_ALARM (0x360*2)
#define  TX2_FPGA_MONITOR_ALARM (0x5E0*2)
#define  RX1_FPGA_MONITOR_ALARM (0x11F0*2)
#define  RX2_FPGA_MONITOR_ALARM (0x13F0*2)
#define  RX3_FPGA_MONITOR_ALARM (0x15F0*2)
#define  RX4_FPGA_MONITOR_ALARM (0x17F0*2)

#define  CARD_IDENTIFY_REG     (0xF*2)
#define  TX1_NCO_GAIN_CONFG_CLR (0x3F8*2)
#define  TX2_NCO_GAIN_CONFG_CLR (0x678*2)
#define  TX_FREQ_CLR            (1<<1)
#define  TX_PH_CLR            (1<<2)
#define  TX_GAIN_CLR            1

#define  RX1_FRE_PH_CONFG_CLR	(0x103F*2)
#define  RX2_FRE_PH_CONFG_CLR	(0x123F*2)
#define  RX3_FRE_PH_CONFG_CLR	(0x143F*2)
#define  RX4_FRE_PH_CONFG_CLR	(0x163F*2)
#define  RX_FREQ_CLR            1
#define  RX_PH_CLR            2
#define  DAC_TEST_NCO_FREQ_I 0x3e3
#define  DAC_TEST_NCO_FREQ_Q 0x3e4

#define  GRAD_DELAY_X  (0x15a*2)
#define  GRAD_DELAY_Y  (0x19a*2)
#define  GRAD_DELAY_Z  (0x1da*2)
#define  GRAD_DELAY_B0  (0x216*2)
//filter
#define  RX_CIC_GAIN_L			     (0x1039*2)		
#define  RX_CIC_GAIN_M			     (0x103A*2)		
#define  RX_CIC_GAIN_H			     (0x103B*2)		

#define  RX_CIC_DECIMIMATIN_FACOR    (0x103C*2)
#define  RX_FIR_DECIMIMATIN_FACOR    (0x103D*2)
#define  RX_IIR_DECIMIMATIN_FACOR    (0x103E*2)
#define  RX_FIR_IIR_BYPASS           (0x10F1*2)
#define  COEF_WDATA_L    (0x1040*2)
#define  COEF_WDATA_H    (0x1041*2)
#define  RX_FIR_RESET    (0x1043*2)

#define TX1_C1_ATT_PHASE_RAM_DATA_L (0x46D*2)
#define TX1_C1_ATT_PHASE_RAM_DATA_H (0x46E*2)

#define TX1_C2_ATT_PHASE_RAM_DATA_L (0x46F*2)
#define TX1_C2_ATT_PHASE_RAM_DATA_H (0x470*2)

#define TX2_C1_ATT_PHASE_RAM_DATA_L (0x6ED*2)
#define TX2_C1_ATT_PHASE_RAM_DATA_H (0x6EE*2)

#define TX2_C2_ATT_PHASE_RAM_DATA_L (0x6EF*2)
#define TX2_C2_ATT_PHASE_RAM_DATA_H (0x6F0*2)
#define RX1_DC_CALI    0x10F2
#define RX2_DC_CALI    0x12F2
#define RX3_DC_CALI    0x14F2
#define RX4_DC_CALI    0x16F2
#define RX1_C1_ADC_DC_ADJ 0x10C9
#define RX2_C1_ADC_DC_ADJ 0x12C9

#define RX3_C1_ADC_DC_ADJ 0x14C9

#define RX4_C1_ADC_DC_ADJ 0x16C9
#define RX1_B1_WR_CMD      0x1002
#define RX2_B1_WR_CMD      0x1202
#define RX3_B1_WR_CMD      0x1402
#define RX4_B1_WR_CMD      0x1602

#define CS2_OFFSET 0x2000


#define  TX_B1_SPI2_CMD  (0x30B*2)
#define  TX_B1_SPI2_DATA  (0x30C*2)
#define  TX_B1_SPI2_ADDR  (0x30D*2)

#define  RX_B1_SPI1_CMD  (CS2_OFFSET+0x8*2)
#define  RX_B1_SPI1_DATA  (CS2_OFFSET+0x9*2)
#define  RX_B1_SPI1_ADDR  (CS2_OFFSET+0xa*2)

#define  RX_B1_SPI2_CMD  (CS2_OFFSET+0xb*2)
#define  RX_B1_SPI2_DATA  (CS2_OFFSET+0xc*2)
#define  RX_B1_SPI2_ADDR  (CS2_OFFSET+0xd*2)

#define  RX_B2_SPI1_CMD  (CS2_OFFSET+0x208*2)
#define  RX_B2_SPI1_DATA  (CS2_OFFSET+0x209*2)
#define  RX_B2_SPI1_ADDR  (CS2_OFFSET+0x20a*2)

#define  RX_B2_SPI2_CMD  (CS2_OFFSET+0x20b*2)
#define  RX_B2_SPI2_DATA  (CS2_OFFSET+0x20c*2)
#define  RX_B2_SPI2_ADDR  (CS2_OFFSET+0x20d*2)

#define  RX_B3_SPI1_CMD  (CS2_OFFSET+0x408*2)
#define  RX_B3_SPI1_DATA  (CS2_OFFSET+0x409*2)
#define  RX_B3_SPI1_ADDR  (CS2_OFFSET+0x40a*2)

#define  RX_B3_SPI2_CMD  (CS2_OFFSET+0x40b*2)
#define  RX_B3_SPI2_DATA  (CS2_OFFSET+0x40c*2)
#define  RX_B3_SPI2_ADDR  (CS2_OFFSET+0x40d*2)

#define  RX_B4_SPI1_CMD  (CS2_OFFSET+0x608*2)
#define  RX_B4_SPI1_DATA  (CS2_OFFSET+0x609*2)
#define  RX_B4_SPI1_ADDR  (CS2_OFFSET+0x60a*2)

#define  RX_B4_SPI2_CMD  (CS2_OFFSET+0x60b*2)
#define  RX_B4_SPI2_DATA  (CS2_OFFSET+0x60c*2)
#define  RX_B4_SPI2_ADDR  (CS2_OFFSET+0x60d*2)


#define  RX1_ADC_CFG      (CS2_OFFSET+0xE*2)
#define  RX2_ADC_CFG      (CS2_OFFSET+0x20E*2)
#define  RX3_ADC_CFG      (CS2_OFFSET+0x40E*2)
#define  RX4_ADC_CFG      (CS2_OFFSET+0x60E*2)
#define  RX1_AMP_CFG_START      (CS2_OFFSET+0x30*2)
#define  RX2_AMP_CFG_START      (CS2_OFFSET+0x230*2)
#define  RX3_AMP_CFG_START      (CS2_OFFSET+0x430*2)
#define  RX4_AMP_CFG_START      (CS2_OFFSET+0x630*2)


#define MB_SPI1_CMD    (0x1E*2)
#define MB_SPI1_DATA   (0x1F*2)
#define MB_SPI1_ADDR   (0x20*2)

#define MB_SPI2_CMD    (0x21*2)
#define MB_SPI2_DATA   (0x22*2)
#define MB_SPI2_ADDR   (0x23*2)

#define MB_SPI3_CMD    (0x24*2)
#define MB_SPI3_DATA   (0x25*2)
#define MB_SPI3_ADDR   (0x26*2)

#define MB_SPI4_CMD    (0x27*2)
#define MB_SPI4_DATA   (0x28*2)
#define MB_SPI4_ADDR   (0x29*2)


#define  TX2_DATABUS_CMD_REG_ADDR (0x588*2)
#define  TX2_DATABUS_DATA_REG_ADDR (0x589*2)
#define  TX2_DATABUS_ADDR_REG_ADDR (0x58A*2)


#define  DAC1_SEL  0x00
#define  DAC2_SEL  0x20
#define  DAC3_SEL  0x40
#define  DAC4_SEL  0x60




#define  ATT1_SEL  0x00
#define  ATT2_SEL  0x20
#define  ATT3_SEL  0x40
#define  ATT4_SEL  0x60
#define  ATT5_SEL  0x80
#define  ATT6_SEL  0xA0
#define  ATT7_SEL  0xC0
#define  ATT8_SEL  0xE0

#define  RX_ATT1_SEL  0x00
#define  RX_ATT2_SEL  0x18
#define  RX_ATT3_SEL  0x30
#define  RX_ATT4_SEL  0x48
#define  RX_ATT5_SEL  0x60
#define  RX_ATT6_SEL  0x78
#define  RX_ATT7_SEL  0x90
#define  RX_ATT8_SEL  0xA8


#define  MAIN_CTRL_ID0_ADDR  (0x50000000+0x120) //for test 
#define  MAIN_CTRL_WAITE_STATE_REG (0x50000000+0x11e) //for test 

#define MAINCTRL_TRIGGER_SEL_REG (0x91*2)
