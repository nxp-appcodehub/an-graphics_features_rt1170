/*
* Copyright 2022 NXP
* All rights reserved.
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "fsl_debug_console.h"
#include "board.h"

#include "fsl_pxp.h"
#include "fsl_lcdifv2.h"
#include "display_support.h"
/*-----------------------------------------------------------*/
#include "vg_lite.h"
#include "vglite_support.h"
#include "Elm.h"

#include "RotatedMap.h"
#include "A8Mask.h"
#include "Thermostat720_PreBuilt.h"
#include "Lights.h"
#include "Cooling.h"
#include "Speaker.h"
#include "Lock.h"
#include "PathAnimation.h"

#include "pin_mux.h"
#include "fsl_soc_src.h"
/*******************************************************************************
* Definitions
******************************************************************************/
#define DEFAULT_SIZE     256.0f;

#define APP_BUFFER_COUNT 2
#define APP_PXP PXP
#define APP_IMG_WIDTH DEMO_BUFFER_WIDTH
#define APP_IMG_HEIGHT DEMO_BUFFER_HEIGHT
typedef uint16_t outputPixel_t;
#define PXP_LAYER_BPP     4U
#define APP_PXP_OUT_FORMAT kPXP_OutputPixelFormatARGB8888
#define APP_SPECIAL_PXP_OUT_FORMAT kPXP_OutputPixelFormatARGB1555
#define APP_PXP_LAYER_FORMAT kVIDEO_PixelFormatXRGB8888
#define APP_SPECIAL_PXP_LAYER_FORMAT kVIDEO_PixelFormatXRGB1555
#define APP_LAYER0_FORMAT kVIDEO_PixelFormatLUT8
#define DEMO_ICON_BUFFER_PIXEL_FORMAT kVIDEO_PixelFormatXRGB4444
#define DEFAULT_SIZE 256.0f;

typedef struct elm_render_buffer
{
  ElmBuffer handle;
  vg_lite_buffer_t *buffer;
} ElmRenderBuffer;

/*******************************************************************************
* Prototypes
******************************************************************************/
static void pxp_task(void *pvParameters);
static int render(vg_lite_buffer_t *buffer, ElmHandle object);

/*******************************************************************************
* Variables
******************************************************************************/
static vg_lite_matrix_t matrix;
vg_lite_buffer_t alphaMask;
vg_lite_buffer_t renderTarget[APP_BUFFER_COUNT];
vg_lite_buffer_t topBar;

static ElmHandle rotatedMapHandle  = ELM_NULL_HANDLE;
static ElmRenderBuffer elmFB[APP_BUFFER_COUNT];

extern unsigned int RotatedMap_evo_len;
extern unsigned char RotatedMap_evo[];

vg_lite_path_t radialPath;

static const uint8_t * GaugePaths[] =
{
  Path000, Path001, Path002, Path003,
  Path004, Path005, Path006, Path007, Path008, Path009, Path010, Path011,
  Path012, Path013, Path014, Path015, Path016, Path017, Path018, Path019,
  Path020, Path021, Path022, Path023, Path024, Path025, Path026, Path027,
  Path028, Path029, Path030, Path031, Path032, Path033, Path034, Path035,
  Path036, Path037, Path038, Path039, Path040, Path041, Path042, Path043,
  Path044, Path045, Path046, Path047, Path048, Path049, Path050, Path051,
  Path052, Path053, Path054, Path055, Path056, Path057, Path058, Path059,
  Path060, Path061, Path062, Path063, Path064, Path065, Path066, Path067,
  Path068, Path069, Path070, Path071, Path072, Path073, Path074, Path075,
  Path076, Path077, Path078, Path079, Path080, Path081, Path082, Path083,
  Path084, Path085, Path086, Path087, Path088, Path089, Path090, Path091,
  Path092, Path093, Path094, Path095, Path096, Path097, Path098, Path099,
  Path100, Path101, Path102, Path103, Path104, Path105, Path106, Path107,
  Path108, Path109, Path110, Path111, Path112, Path113, Path114, Path115,
  Path116, Path117, Path118, Path119, Path120, Path121, Path122, Path123,
  Path124, Path125, Path126, Path127, Path128, Path129, Path130, Path131,
  Path132, Path133, Path134, Path135, Path136, Path137, Path138, Path139,
  Path140, Path141, Path142, Path143, Path144, Path145, Path146, Path147,
  Path148, Path149, Path150, Path151, Path152, Path153, Path154, Path155,
  Path156, Path157, Path158, Path159, Path160, Path161, Path162, Path163,
  Path164, Path165, Path166, Path167, Path168, Path169, Path170, Path171,
  Path172, Path173, Path174, Path175, Path176, Path177, Path178, Path179,
  Path180, Path181, Path182, Path183, Path184, Path185, Path186, Path187,
  Path188, Path189, Path190, Path191, Path192, Path193, Path194, Path195,
  Path196, Path197, Path198, Path199, Path200, Path201, Path202, Path203,
  Path204, Path205, Path206, Path207, Path208, Path209, Path210, Path211,
  Path212, Path213, Path214, Path215, Path216, Path217, Path218, Path219,
  Path220, Path221, Path222, Path223, Path224, Path225, Path226, Path227,
  Path228, Path229, Path230, Path231, Path232, Path233, Path234, Path235,
  Path236, Path237, Path238, Path239, Path240, Path241, Path242, Path243,
  Path244, Path245, Path246, Path247, Path248, Path249, Path250, Path251,
  Path252, Path253, Path254, Path255, Path256, Path257, Path258, Path259,
  Path260, Path261, Path262, Path263, Path264, Path265, Path266, Path267,
  Path268, Path269, Path270, Path271, Path272, Path273, Path274, Path275,
  Path276, Path277, Path278, Path279, Path280, Path281, Path282, Path283,
  Path284, Path285, Path286, Path287, Path288, Path289, Path290, Path291,
  Path292, Path293, Path294, Path295, Path296, Path297, Path298, Path299,
  Path300, Path301, Path302, Path303, Path304, Path305, Path306, Path307,
  Path308, Path309, Path310, Path311, Path312, Path313, Path314, Path315,
  Path316, Path317, Path318, Path319, Path320, Path321, Path322, Path323,
  Path324, Path325, Path326, Path327, Path328, Path329, Path330, Path331,
  Path332, Path333, Path334, Path335, Path336, Path337, Path338, Path339,
  Path340, Path341, Path342, Path343, Path344, Path345, Path346, Path347,
  Path348, Path349, Path350, Path351, Path352, Path353, Path354, Path355,
  Path356, Path357, Path358, Path359
};

static uint32_t GaugePathLengths[] =
{
  Path000_len, Path001_len, Path002_len,
  Path003_len, Path004_len, Path005_len, Path006_len, Path007_len,
  Path008_len, Path009_len, Path010_len, Path011_len, Path012_len,
  Path013_len, Path014_len, Path015_len, Path016_len, Path017_len,
  Path018_len, Path019_len, Path020_len, Path021_len, Path022_len,
  Path023_len, Path024_len, Path025_len, Path026_len, Path027_len,
  Path028_len, Path029_len, Path030_len, Path031_len, Path032_len,
  Path033_len, Path034_len, Path035_len, Path036_len, Path037_len,
  Path038_len, Path039_len, Path040_len, Path041_len, Path042_len,
  Path043_len, Path044_len, Path045_len, Path046_len, Path047_len,
  Path048_len, Path049_len, Path050_len, Path051_len, Path052_len,
  Path053_len, Path054_len, Path055_len, Path056_len, Path057_len,
  Path058_len, Path059_len, Path060_len, Path061_len, Path062_len,
  Path063_len, Path064_len, Path065_len, Path066_len, Path067_len,
  Path068_len, Path069_len, Path070_len, Path071_len, Path072_len,
  Path073_len, Path074_len, Path075_len, Path076_len, Path077_len,
  Path078_len, Path079_len, Path080_len, Path081_len, Path082_len,
  Path083_len, Path084_len, Path085_len, Path086_len, Path087_len,
  Path088_len, Path089_len, Path090_len, Path091_len, Path092_len,
  Path093_len, Path094_len, Path095_len, Path096_len, Path097_len,
  Path098_len, Path099_len, Path100_len, Path101_len, Path102_len,
  Path103_len, Path104_len, Path105_len, Path106_len, Path107_len,
  Path108_len, Path109_len, Path110_len, Path111_len, Path112_len,
  Path113_len, Path114_len, Path115_len, Path116_len, Path117_len,
  Path118_len, Path119_len, Path120_len, Path121_len, Path122_len,
  Path123_len, Path124_len, Path125_len, Path126_len, Path127_len,
  Path128_len, Path129_len, Path130_len, Path131_len, Path132_len,
  Path133_len, Path134_len, Path135_len, Path136_len, Path137_len,
  Path138_len, Path139_len, Path140_len, Path141_len, Path142_len,
  Path143_len, Path144_len, Path145_len, Path146_len, Path147_len,
  Path148_len, Path149_len, Path150_len, Path151_len, Path152_len,
  Path153_len, Path154_len, Path155_len, Path156_len, Path157_len,
  Path158_len, Path159_len, Path160_len, Path161_len, Path162_len,
  Path163_len, Path164_len, Path165_len, Path166_len, Path167_len,
  Path168_len, Path169_len, Path170_len, Path171_len, Path172_len,
  Path173_len, Path174_len, Path175_len, Path176_len, Path177_len,
  Path178_len, Path179_len, Path180_len, Path181_len, Path182_len,
  Path183_len, Path184_len, Path185_len, Path186_len, Path187_len,
  Path188_len, Path189_len, Path190_len, Path191_len, Path192_len,
  Path193_len, Path194_len, Path195_len, Path196_len, Path197_len,
  Path198_len, Path199_len, Path200_len, Path201_len, Path202_len,
  Path203_len, Path204_len, Path205_len, Path206_len, Path207_len,
  Path208_len, Path209_len, Path210_len, Path211_len, Path212_len,
  Path213_len, Path214_len, Path215_len, Path216_len, Path217_len,
  Path218_len, Path219_len, Path220_len, Path221_len, Path222_len,
  Path223_len, Path224_len, Path225_len, Path226_len, Path227_len,
  Path228_len, Path229_len, Path230_len, Path231_len, Path232_len,
  Path233_len, Path234_len, Path235_len, Path236_len, Path237_len,
  Path238_len, Path239_len, Path240_len, Path241_len, Path242_len,
  Path243_len, Path244_len, Path245_len, Path246_len, Path247_len,
  Path248_len, Path249_len, Path250_len, Path251_len, Path252_len,
  Path253_len, Path254_len, Path255_len, Path256_len, Path257_len,
  Path258_len, Path259_len, Path260_len, Path261_len, Path262_len,
  Path263_len, Path264_len, Path265_len, Path266_len, Path267_len,
  Path268_len, Path269_len, Path270_len, Path271_len, Path272_len,
  Path273_len, Path274_len, Path275_len, Path276_len, Path277_len,
  Path278_len, Path279_len, Path280_len, Path281_len, Path282_len,
  Path283_len, Path284_len, Path285_len, Path286_len, Path287_len,
  Path288_len, Path289_len, Path290_len, Path291_len, Path292_len,
  Path293_len, Path294_len, Path295_len, Path296_len, Path297_len,
  Path298_len, Path299_len, Path300_len, Path301_len, Path302_len,
  Path303_len, Path304_len, Path305_len, Path306_len, Path307_len,
  Path308_len, Path309_len, Path310_len, Path311_len, Path312_len,
  Path313_len, Path314_len, Path315_len, Path316_len, Path317_len,
  Path318_len, Path319_len, Path320_len, Path321_len, Path322_len,
  Path323_len, Path324_len, Path325_len, Path326_len, Path327_len,
  Path328_len, Path329_len, Path330_len, Path331_len, Path332_len,
  Path333_len, Path334_len, Path335_len, Path336_len, Path337_len,
  Path338_len, Path339_len, Path340_len, Path341_len, Path342_len,
  Path343_len, Path344_len, Path345_len, Path346_len, Path347_len,
  Path348_len, Path349_len, Path350_len, Path351_len, Path352_len,
  Path353_len, Path354_len, Path355_len, Path356_len, Path357_len,
  Path358_len, Path359_len
};

AT_NONCACHEABLE_SECTION_ALIGN(static uint8_t s_layer0[1][APP_IMG_HEIGHT][APP_IMG_WIDTH], FRAME_BUFFER_ALIGN);
AT_NONCACHEABLE_SECTION_ALIGN(static outputPixel_t s_BufferLcd[2][416][416], FRAME_BUFFER_ALIGN);
AT_NONCACHEABLE_SECTION_ALIGN(static uint8_t icon0_frameBuffer[1][144][144][2], FRAME_BUFFER_ALIGN);
AT_NONCACHEABLE_SECTION_ALIGN(static uint8_t icon1_frameBuffer[1][144][144][2], FRAME_BUFFER_ALIGN);
AT_NONCACHEABLE_SECTION_ALIGN(static uint8_t icon2_frameBuffer[1][128][155][2], FRAME_BUFFER_ALIGN);
AT_NONCACHEABLE_SECTION_ALIGN(static uint8_t icon3_frameBuffer[1][128][103][2], FRAME_BUFFER_ALIGN);
AT_NONCACHEABLE_SECTION_ALIGN(static uint8_t topBar_frameBuffer[1][48][1280][2], FRAME_BUFFER_ALIGN);

/* PXP Output buffer config. */
static pxp_output_buffer_config_t outputBufferConfig;
static uint8_t curLcdBufferIdx = 0;
static uint8_t curVgBufferIdx = 0;
/*
* When new frame buffer sent to display, it might not be shown immediately.
* Application could use callback to get new frame shown notification, at the
* same time, when this flag is set, application could write to the older
* frame buffer.
*/
static volatile bool s_newFrameShown = false;
static volatile bool icon0_newFrameShown = false;
static volatile bool icon1_newFrameShown = false;
static volatile bool icon2_newFrameShown = false;
static volatile bool icon3_newFrameShown = false;
static volatile bool topBar_newFrameShown = false;
static dc_fb_info_t fbInfo;
static dc_fb_info_t pxpFbInfo;
static dc_fb_info_t icon0FbInfo;
static dc_fb_info_t icon1FbInfo;
static dc_fb_info_t icon2FbInfo;
static dc_fb_info_t icon3FbInfo;
static dc_fb_info_t topBarFbInfo;

SemaphoreHandle_t xMutex;
/*******************************************************************************
* Code
******************************************************************************/
static void BOARD_ResetDisplayMix(void)
{
  /*
  * Reset the displaymix, otherwise during debugging, the
  * debugger may not reset the display, then the behavior
  * is not right.
  */
  SRC_AssertSliceSoftwareReset(SRC, kSRC_DisplaySlice);
  while (kSRC_SliceResetInProcess == SRC_GetSliceResetState(SRC, kSRC_DisplaySlice))
  {
  }
}

static void ConfigureOB(int width, int height, void * address)
{
  outputBufferConfig.pixelFormat    = APP_SPECIAL_PXP_OUT_FORMAT;
  outputBufferConfig.interlacedMode = kPXP_OutputProgressive;
  outputBufferConfig.buffer0Addr    = (uint32_t)address;
  outputBufferConfig.buffer1Addr    = 0U;
  outputBufferConfig.pitchBytes     = 416 * 2;
  outputBufferConfig.width          = width;
  outputBufferConfig.height         = height;
  
  PXP_SetOutputBufferConfig(APP_PXP, &outputBufferConfig);
}

static void ConfigurePS(int x0, int y0, int instanceWidth, int instanceHeight, int bufferWidth, int bpp, pxp_ps_pixel_format_t format, void * address)
{
  /* PS configure. */
  const pxp_ps_buffer_config_t psBufferConfig = {
    .pixelFormat = format,
    .swapByte    = false,
    .bufferAddr  = (uint32_t)address,
    .bufferAddrU = 0U,
    .bufferAddrV = 0U,
    .pitchBytes  = bufferWidth * bpp,
  };
  
  PXP_SetProcessSurfaceBackGroundColor(APP_PXP, 0xFFFFFFU);
  PXP_SetProcessSurfaceBufferConfig(APP_PXP, &psBufferConfig);
  PXP_SetProcessSurfacePosition(APP_PXP, x0, y0, x0 + instanceWidth - 1U, y0 + instanceHeight - 1U);
}

static void ConfigureAS(int x0, int y0, int instanceWidth, int instanceHeight, int bufferWidth, int bpp, pxp_as_pixel_format_t format, void * address)
{
  /* AS config. */
  const pxp_as_buffer_config_t asBufferConfig = {
    .pixelFormat = format,
    .bufferAddr  = (uint32_t)address,
    .pitchBytes  = bufferWidth * bpp,
  };
  
  const pxp_as_blend_config_t asBlendConfig = {.alpha       = 0U,    /* Don't care. */
  .invertAlpha = false, /* Don't care. */
  .alphaMode   = kPXP_AlphaEmbedded,
  .ropMode     = kPXP_RopMaskAs};
  
  PXP_SetAlphaSurfaceBufferConfig(APP_PXP, &asBufferConfig);
  PXP_SetAlphaSurfaceBlendConfig(APP_PXP, &asBlendConfig);
  PXP_SetAlphaSurfacePosition(APP_PXP, x0, y0, x0 + instanceWidth - 1U, y0 + instanceHeight - 1U);
}

static void pxpFinish()
{
  /* Wait for process complete. */
  while (!(kPXP_CompleteFlag & PXP_GetStatusFlags(APP_PXP)))
  {
  }
  PXP_ClearStatusFlags(APP_PXP, kPXP_CompleteFlag);
}

static void APP_BufferSwitchOffCallback(void *param, void *switchOffBuffer)
{
  s_newFrameShown = true;
}

static void Icon0SwitchOffCallback(void * param, void * switchOffBuffer)
{
  icon0_newFrameShown = true;
}
static void Icon1SwitchOffCallback(void * param, void * switchOffBuffer)
{
  icon1_newFrameShown = true;
}
static void Icon2SwitchOffCallback(void * param, void * switchOffBuffer)
{
  icon2_newFrameShown = true;
}
static void Icon3SwitchOffCallback(void * param, void * switchOffBuffer)
{
  icon3_newFrameShown = true;
}
static void TopBarSwitchOffCallback(void * param, void * switchOffBuffer)
{
  topBar_newFrameShown = true;
}



static void APP_InitLcdif(void)
{
  status_t status;
    const lcdifv2_blend_config_t blendConfig0 = {
		        .globalAlpha = 255,
		        .alphaMode   = kLCDIFV2_AlphaEmbedded,
		    };
    
    const lcdifv2_blend_config_t blendConfig1 = {
		        .globalAlpha = 255,
		        .alphaMode   = kLCDIFV2_AlphaEmbedded,
		    };
    
    const lcdifv2_blend_config_t blendConfig2 = {
		        .globalAlpha = 255,
		        .alphaMode   = kLCDIFV2_AlphaEmbedded,
		    };
    
    const lcdifv2_blend_config_t blendConfig3 = {
		        .globalAlpha = 255,
		        .alphaMode   = kLCDIFV2_AlphaEmbedded,
		    };
    
    const lcdifv2_blend_config_t blendConfig4 = {
		        .globalAlpha = 255,
		        .alphaMode   = kLCDIFV2_AlphaEmbedded,
		    };
    
    const lcdifv2_blend_config_t blendConfig5 = {
		        .globalAlpha = 255,
		        .alphaMode   = kLCDIFV2_AlphaEmbedded,
		    };
    
    const lcdifv2_blend_config_t blendConfig6 = {
		        .globalAlpha = 255,
		        .alphaMode   = kLCDIFV2_AlphaEmbedded,
		    };
    
    
  
  //Populate the bitmaps for the Layers
  for(uint32_t i = 0; i < 720*1280; i++)
  {
    uint8_t * pData = (uint8_t *)s_layer0[0];
    pData += i;
    *pData = Thermostat720_PreBuilt_Bitmap0[i];
  }
  
  for(uint32_t i = 0; i < 144*144*2; i++)
  {
    uint8_t * pData = (uint8_t *)icon0_frameBuffer[0];
    pData += i;
    *pData = Lights_Bitmap0[i];
  }
  
  for(uint32_t i = 0; i < 144*144*2; i++)
  {
    uint8_t * pData = (uint8_t *)icon1_frameBuffer[0];
    pData += i;
    *pData = Cooling_Bitmap0[i];
  }
  
  for(uint32_t i = 0; i < 128*155*2; i++)
  {
    uint8_t * pData = (uint8_t *)icon2_frameBuffer[0];
    pData += i;
    *pData = Speaker_Bitmap0[i];
  }
  
  for(uint32_t i = 0; i < 128*103*2; i++)
  {
    uint8_t * pData = (uint8_t *)icon3_frameBuffer[0];
    pData += i;
    *pData = Lock_Bitmap0[i];
  }
  
  
  BOARD_PrepareDisplayController();
  
  status = g_dc.ops->init(&g_dc);
  if (kStatus_Success != status)
  {
    PRINTF("Display initialization failed\r\n");
    assert(0);
  }
  
  g_dc.ops->getLayerDefaultConfig(&g_dc, 0, &fbInfo);
  fbInfo.pixelFormat = APP_LAYER0_FORMAT;
  fbInfo.width       = APP_IMG_WIDTH;
  fbInfo.height      = APP_IMG_HEIGHT;
  fbInfo.strideBytes = APP_IMG_WIDTH;
  g_dc.ops->setLayerConfig(&g_dc, 0, &fbInfo);
  
  //g_dc.ops->getLayerDefaultConfig(&g_dc, 1, &pxpFbInfo);
  pxpFbInfo.startX = 152;
  pxpFbInfo.startY = 432;
  pxpFbInfo.pixelFormat = APP_SPECIAL_PXP_LAYER_FORMAT;
  pxpFbInfo.width       = 416;
  pxpFbInfo.height      = 416;
  pxpFbInfo.strideBytes = 416*2;
  g_dc.ops->setLayerConfig(&g_dc, 1, &pxpFbInfo);
  
  icon0FbInfo.startX = 114;
  icon0FbInfo.startY = 114;
  icon0FbInfo.pixelFormat = DEMO_ICON_BUFFER_PIXEL_FORMAT;
  icon0FbInfo.width       = 144;
  icon0FbInfo.height      = 144;
  icon0FbInfo.strideBytes = 144*2;
  g_dc.ops->setLayerConfig(&g_dc, 2, &icon0FbInfo);
  
  icon1FbInfo.startX = 464;
  icon1FbInfo.startY = 114;
  icon1FbInfo.pixelFormat = DEMO_ICON_BUFFER_PIXEL_FORMAT;
  icon1FbInfo.width       = 144;
  icon1FbInfo.height      = 144;
  icon1FbInfo.strideBytes = 144*2;
  g_dc.ops->setLayerConfig(&g_dc, 3, &icon1FbInfo);
  
  icon2FbInfo.startX = 122;
  icon2FbInfo.startY = 1017;
  icon2FbInfo.pixelFormat = DEMO_ICON_BUFFER_PIXEL_FORMAT;
  icon2FbInfo.width       = 128;
  icon2FbInfo.height      = 155;
  icon2FbInfo.strideBytes = 128*2;
  g_dc.ops->setLayerConfig(&g_dc, 4, &icon2FbInfo);
  
  icon3FbInfo.startX = 472;
  icon3FbInfo.startY = 1043;
  icon3FbInfo.pixelFormat = DEMO_ICON_BUFFER_PIXEL_FORMAT;
  icon3FbInfo.width       = 128;
  icon3FbInfo.height      = 103;
  icon3FbInfo.strideBytes = 128*2;
  g_dc.ops->setLayerConfig(&g_dc, 5, &icon3FbInfo);
  
  topBarFbInfo.startX = 672;
  topBarFbInfo.startY = 0;
  topBarFbInfo.pixelFormat = DEMO_ICON_BUFFER_PIXEL_FORMAT;
  topBarFbInfo.width       = 48;
  topBarFbInfo.height      = 1280;
  topBarFbInfo.strideBytes = 48*2;
  g_dc.ops->setLayerConfig(&g_dc, 6, &topBarFbInfo);
  
  g_dc.ops->setCallback(&g_dc, 1, APP_BufferSwitchOffCallback, NULL);
  g_dc.ops->setCallback(&g_dc, 2, Icon0SwitchOffCallback, NULL);
  g_dc.ops->setCallback(&g_dc, 3, Icon1SwitchOffCallback, NULL);
  g_dc.ops->setCallback(&g_dc, 4, Icon2SwitchOffCallback, NULL);
  g_dc.ops->setCallback(&g_dc, 5, Icon3SwitchOffCallback, NULL);
  g_dc.ops->setCallback(&g_dc, 6, TopBarSwitchOffCallback, NULL);
  
  
  s_newFrameShown = false;
  LCDIFV2_SetLut(LCDIFV2, 0, Thermostat720_PreBuilt_CLUT, 255, false);
  LCDIFV2_SetLayerBlendConfig(LCDIFV2, 0, &blendConfig0);
  LCDIFV2_SetLayerBlendConfig(LCDIFV2, 1, &blendConfig1);
  LCDIFV2_SetLayerBlendConfig(LCDIFV2, 2, &blendConfig2);
  LCDIFV2_SetLayerBlendConfig(LCDIFV2, 3, &blendConfig3);
  LCDIFV2_SetLayerBlendConfig(LCDIFV2, 4, &blendConfig4);
  LCDIFV2_SetLayerBlendConfig(LCDIFV2, 5, &blendConfig5);
  LCDIFV2_SetLayerBlendConfig(LCDIFV2, 6, &blendConfig6);
  g_dc.ops->setFrameBuffer(&g_dc, 0, (void *)s_layer0[0]);
  g_dc.ops->setFrameBuffer(&g_dc, 1, (void *)s_BufferLcd[0]);
  g_dc.ops->setFrameBuffer(&g_dc, 2, (void *)icon0_frameBuffer[0]);
  g_dc.ops->setFrameBuffer(&g_dc, 3, (void *)icon1_frameBuffer[0]);
  g_dc.ops->setFrameBuffer(&g_dc, 4, (void *)icon2_frameBuffer[0]);
  g_dc.ops->setFrameBuffer(&g_dc, 5, (void *)icon3_frameBuffer[0]);
  g_dc.ops->setFrameBuffer(&g_dc, 6, (void *)topBar_frameBuffer[0]);
  
  
  /* For the DBI interface display, application must wait for the first
  * frame buffer sent to the panel.
  */
  if ((g_dc.ops->getProperty(&g_dc) & kDC_FB_ReserveFrameBuffer) == 0)
  {
    while (s_newFrameShown == false || 
           icon0_newFrameShown == false ||
           icon1_newFrameShown == false ||
           icon2_newFrameShown == false ||
           icon3_newFrameShown == false ||
           topBar_newFrameShown == false)
    {
    }
  }
  
  s_newFrameShown = true;
  icon0_newFrameShown = true;
  icon1_newFrameShown = true;
  icon2_newFrameShown = true;
  icon3_newFrameShown = true;
  topBar_newFrameShown = true;
  
  g_dc.ops->enableLayer(&g_dc, 0);
  g_dc.ops->enableLayer(&g_dc, 1);
  g_dc.ops->enableLayer(&g_dc, 2);
  g_dc.ops->enableLayer(&g_dc, 3);
  g_dc.ops->enableLayer(&g_dc, 4);
  g_dc.ops->enableLayer(&g_dc, 5);
  g_dc.ops->enableLayer(&g_dc, 6);
}

static void APP_InitPxp(void)
{
  PXP_Init(APP_PXP);
  
  memset(s_BufferLcd, 0x00000000U, sizeof(s_BufferLcd));
  
  ConfigureOB(APP_IMG_WIDTH, APP_IMG_HEIGHT, (void *)&s_BufferLcd[0]);
  
  //Disable both PS and AS for now:
  PXP_SetProcessSurfacePosition(APP_PXP, 0xFFFFU, 0xFFFFU, 0U, 0U);
  PXP_SetAlphaSurfacePosition(APP_PXP, 0xFFFFU, 0xFFFFU, 0U, 0U);
  
  /* Disable CSC1, it is enabled by default. */
  PXP_EnableCsc1(APP_PXP, false);
}

vg_lite_error_t DrawRadialBars(vg_lite_buffer_t * target)
{
  vg_lite_error_t error;
  static uint32_t pathIndex = 333;
  vg_lite_init_path(&radialPath, VG_LITE_FP32, VG_LITE_LOW, GaugePathLengths[pathIndex], (void *)GaugePaths[pathIndex],
                    0, 0, 720, 1280);
  
  vg_lite_identity(&matrix);
  vg_lite_translate(-152, -432, &matrix);
  error = vg_lite_draw(target, &radialPath, VG_LITE_FILL_EVEN_ODD, &matrix, VG_LITE_BLEND_SRC_OVER, 0x7CA2FF7C);
  vg_lite_clear_path(&radialPath);
  return error;
}

static void redrawPXP()
{
  int status = 0;
  static float mapAngle = 0.0f;
  bool renderedVGLite = false;
  while (s_newFrameShown == false)
  {
    if(false == renderedVGLite)
    {
      renderedVGLite = true;
      vg_lite_clear(&renderTarget[curVgBufferIdx], 0, 0x00000000);
      ElmReset(rotatedMapHandle, ELM_PROP_TRANSFER_BIT);
      ElmTransfer(rotatedMapHandle, -282, -637);
      ElmTransfer(rotatedMapHandle, 490, 845);
      ElmRotate(rotatedMapHandle, mapAngle);
      ElmScale(rotatedMapHandle, 0.8, 0.8);
      ElmTransfer(rotatedMapHandle, -490, -845);
      status = render(&renderTarget[curVgBufferIdx], rotatedMapHandle);
      if (status == -1)
      {
        PRINTF("ELM Render rotatedMapHandle Failed");
        return;
      }
      DrawRadialBars(&renderTarget[curVgBufferIdx]);
      
      vg_lite_identity(&matrix);
      vg_lite_blit(&renderTarget[curVgBufferIdx], &alphaMask, &matrix, VG_LITE_BLEND_DST_IN, 0, VG_LITE_FILTER_POINT);
      
      mapAngle+=1.0f;
      
      vg_lite_flush();
    }
  }
  
  curLcdBufferIdx ^= 1U;
  outputBufferConfig.buffer0Addr = (uint32_t)s_BufferLcd[curLcdBufferIdx];
  ConfigureOB(416,416, (void *)s_BufferLcd[curLcdBufferIdx]);

  ConfigurePS(0, 0, 416, 416, 416, 2, kPXP_PsPixelFormatRGB555, (void *) (void *)s_BufferLcd[curLcdBufferIdx]);
  ConfigureAS(0, 0, 416, 416, 416, 4, kPXP_AsPixelFormatARGB8888, (void *) renderTarget[curVgBufferIdx^1U].memory);
  PXP_Start(APP_PXP);
  pxpFinish();
       
  vg_lite_finish();
  curVgBufferIdx ^= 1U;

  s_newFrameShown = false;
  g_dc.ops->setFrameBuffer(&g_dc, 1, (void *)s_BufferLcd[curLcdBufferIdx]);
}


int main(void)
{
  /* Init board hardware. */
  BOARD_ConfigMPU();
  BOARD_BootClockRUN();
  BOARD_ResetDisplayMix();
  BOARD_InitLpuartPins();
  BOARD_InitMipiPanelPins();
  BOARD_InitDebugConsole();
  
  xMutex = xSemaphoreCreateMutex();
  if (xTaskCreate(pxp_task, "pxp_task", configMINIMAL_STACK_SIZE + 200, NULL, configMAX_PRIORITIES - 1, NULL) !=
        pdPASS)
  {
        PRINTF("Task creation failed!.\r\n");
        while (1)
            ;
  }
 
  vTaskStartScheduler();
  for (;;)
    ;
}

static ELM_BUFFER_FORMAT _buffer_format_to_Elm(vg_lite_buffer_format_t format)
{
  switch (format)
  {
  case VG_LITE_RGB565:
    return ELM_BUFFER_FORMAT_RGB565;
    break;
  case VG_LITE_BGR565:
    return ELM_BUFFER_FORMAT_BGR565;
    break;
  case VG_LITE_BGRA8888:
    return ELM_BUFFER_FORMAT_BGRA8888;
    break;   
  default:
    return ELM_BUFFER_FORMAT_RGBA8888;
    break;
  }
}

static void cleanup(void)
{
  vg_lite_close();
}

static int load_map()
{
  if (RotatedMap_evo_len != 0)
  {
    rotatedMapHandle =
      ElmCreateObjectFromData(ELM_OBJECT_TYPE_EGO, (void *)RotatedMap_evo, RotatedMap_evo_len);
  }
  
  return (rotatedMapHandle != ELM_NULL_HANDLE);
}

static int load_texture()
{
  int ret = 0;
  ret     = load_map();
  if (ret < 0)
  {
    PRINTF("load_map\r\n");
    return ret;
  }
  return ret;
}

static vg_lite_error_t init_vg_lite(void)
{
  vg_lite_error_t error = VG_LITE_SUCCESS;
  int ret               = 0;
  
  // Initialize the draw.
  ret = ElmInitialize(256, 256);
  if (!ret)
  {
    PRINTF("ElmInitalize failed\n");
    cleanup();
    return VG_LITE_OUT_OF_MEMORY;
  }
  
  // load the texture;
  ret = load_texture();
  if (ret < 0)
  {
    PRINTF("load_texture error\r\n");
    return VG_LITE_OUT_OF_MEMORY;
  }
  
  alphaMask.width = 416;
  alphaMask. height = 416;
  alphaMask.format = VG_LITE_A8;
  error = vg_lite_allocate(&alphaMask);
  if (error)
  {
    PRINTF("vg_lite_allocate for alpha mask failed, error %d\n", error);
    return error;
  }
  //Populate the alpha mask
  uint8_t * pData = (uint8_t *)alphaMask.memory;
  for(int i = 0; i < 416*416; i++)
  {
    *pData = A8Mask_Bitmap0[i];
    pData++;
  }
  
  for (int i = 0; i < 2; i++)
  {
    //Allocate the render target
    renderTarget[i].width = 416;
    renderTarget[i].height = 416;
    renderTarget[i].format = VG_LITE_BGRA8888;
    error = vg_lite_allocate(&renderTarget[i]);
    if (VG_LITE_SUCCESS != error)
    {
      PRINTF("vg_lite_allocate failed: vg_lite_allocate() returned error %d\n", error);
      return error;
    }
  }
  
  topBar.width = 48;
  topBar.height = 1280;
  topBar.format = VG_LITE_ABGR4444;
  topBar.stride = 48*2;
  topBar.memory = (void*)topBar_frameBuffer[0];
  topBar.address = (uint32_t)topBar_frameBuffer[0];
  vg_lite_clear(&topBar, 0, 0x7F7F7F7F);
  vg_lite_finish();
    
  return error;
}

static ElmBuffer get_elm_buffer(vg_lite_buffer_t *buffer)
{
  for (int i = 0; i < APP_BUFFER_COUNT; i++)
  {
    if (elmFB[i].buffer == NULL)
    {
      elmFB[i].buffer = buffer;
      elmFB[i].handle = ElmWrapBuffer(buffer->width, buffer->height, buffer->stride, buffer->memory,
                                      buffer->address, _buffer_format_to_Elm(buffer->format));
      vg_lite_clear(buffer, NULL, 0x0);
      return elmFB[i].handle;
    }
    if (elmFB[i].buffer == buffer)
      return elmFB[i].handle;
  }
  return 0;
}
static int render(vg_lite_buffer_t *buffer, ElmHandle object)
{
  int status                = 0;
  ElmBuffer elmRenderBuffer = get_elm_buffer(buffer);
  status                    = ElmDraw(elmRenderBuffer, object);
  if (!status)
  {
    status = -1;
    return status;
  }
  return status;
}

uint32_t getTime()
{
  return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static void pxp_task(void *pvParameters)
{
  APP_InitLcdif();
  APP_InitPxp();
  
  status_t status;
  vg_lite_error_t error;
  
  status = BOARD_PrepareVGLiteController();
  if (status != kStatus_Success)
  {
    PRINTF("Prepare VGlite contolor error\r\n");
    while (1)
      ;
  }
  
  error = init_vg_lite();
  if (error)
  {
    PRINTF("init_vg_lite failed: init_vg_lite() returned error %d\n", error);
    while (1)
      ;
  }
  
  uint32_t startTime, time, n = 0;
  startTime = getTime();
  while (1)
  {
    redrawPXP();
    n++;
    if (n >= 60)
    {
      time = getTime() - startTime;
      float fTime = (float) time / 1000.0f;
      float fps = (float)n / fTime;
      PRINTF("PXP: %d frames in %d timeUnits: %f fps\r\n", n, time, fps);
      n         = 0;
      startTime = getTime();
    }
  }
}