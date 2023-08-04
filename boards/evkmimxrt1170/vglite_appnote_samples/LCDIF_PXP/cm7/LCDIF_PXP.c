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
#include "display_support.h"

#include "PXPLayer0Background.h"
#include "PXPLayer0Blit1.h"
#include "PXPLayer0Blit2.h"
#include "PXPLayer0Blit3.h"
#include "PXPLayer1Blit1.h"
/*-----------------------------------------------------------*/
#include "pin_mux.h"
#include "fsl_soc_src.h"
/*******************************************************************************
* Definitions
******************************************************************************/
#define DEFAULT_SIZE     256.0f;

#define APP_BUFFER_COUNT 2
#define APP_PXP PXP
#define APP_LAYER0_WIDTH 720
#define APP_LAYER0_HEIGHT 640
#define APP_LAYER1_WIDTH 144
#define APP_LAYER1_HEIGHT 394
typedef uint32_t outputPixel_t;
#define DISPLAY_BPP     4U
#define APP_PXP_PS_FORMAT kPXP_PsPixelFormatRGB888
#define APP_PXP_AS_FORMAT kPXP_AsPixelFormatARGB8888
#define APP_PXP_OUT_FORMAT kPXP_OutputPixelFormatARGB8888
#define APP_DC_FORMAT kVIDEO_PixelFormatXRGB8888
/*******************************************************************************
* Prototypes
******************************************************************************/
static void pxp_task(void *pvParameters);
/*******************************************************************************
* Variables
******************************************************************************/

AT_NONCACHEABLE_SECTION_ALIGN(static outputPixel_t Layer0Buffer[2][APP_LAYER0_HEIGHT][APP_LAYER0_WIDTH], FRAME_BUFFER_ALIGN);
AT_NONCACHEABLE_SECTION_ALIGN(static outputPixel_t Layer1Buffer[1][APP_LAYER1_HEIGHT][APP_LAYER1_WIDTH], FRAME_BUFFER_ALIGN);


/* PXP Output buffer config. */
static pxp_output_buffer_config_t outputBufferConfig;
static uint8_t curLcdBufferIdx = 0;
/*
* When new frame buffer sent to display, it might not be shown immediately.
* Application could use callback to get new frame shown notification, at the
* same time, when this flag is set, application could write to the older
* frame buffer.
*/
static volatile bool Layer0FrameShown = false;
static volatile bool Layer1FrameShown = false;

static dc_fb_info_t layer0FbInfo;
static dc_fb_info_t layer1FbInfo;

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

static void ConfigureOB(int width, int height, int pitch, void * address)
{
  outputBufferConfig.pixelFormat    = APP_PXP_OUT_FORMAT;
  outputBufferConfig.interlacedMode = kPXP_OutputProgressive;
  outputBufferConfig.buffer0Addr    = (uint32_t)address;
  outputBufferConfig.buffer1Addr    = 0U;
  outputBufferConfig.pitchBytes     = pitch;
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

static void Layer0SwitchOffCallback(void *param, void *switchOffBuffer)
{
  Layer0FrameShown = true;
}

static void Layer1SwitchOffCallback(void *param, void *switchOffBuffer)
{
  Layer1FrameShown = true;
}

static void APP_InitLcdif(void)
{
  status_t status;
  
  BOARD_PrepareDisplayController();
  
  status = g_dc.ops->init(&g_dc);
  if (kStatus_Success != status)
  {
    PRINTF("Display initialization failed\r\n");
    assert(0);
  }
  
    //LAYER 0: THIS WILL BE USED AS A STATIC LAYER
  g_dc.ops->getLayerDefaultConfig(&g_dc, 0, &layer0FbInfo);
  layer0FbInfo.pixelFormat = APP_DC_FORMAT;
  layer0FbInfo.startX = 0;
  layer0FbInfo.startY = 0;
  layer0FbInfo.width       = APP_LAYER0_WIDTH;
  layer0FbInfo.height      = APP_LAYER0_HEIGHT;
  layer0FbInfo.strideBytes = APP_LAYER0_WIDTH*4;
  g_dc.ops->setLayerConfig(&g_dc, 0, &layer0FbInfo);
  
  //LAYER 1: THIS LAYER WILL BE USED FOR VGLite
  layer1FbInfo.startX = 288;
  layer1FbInfo.startY = 720;
  layer1FbInfo.pixelFormat = APP_DC_FORMAT;
  layer1FbInfo.width       = APP_LAYER1_WIDTH;
  layer1FbInfo.height      = APP_LAYER1_HEIGHT;
  layer1FbInfo.strideBytes = APP_LAYER1_WIDTH*4;
  g_dc.ops->setLayerConfig(&g_dc, 1, &layer1FbInfo);
  
  g_dc.ops->setCallback(&g_dc, 0, Layer0SwitchOffCallback, NULL);
  g_dc.ops->setCallback(&g_dc, 1, Layer1SwitchOffCallback, NULL);
  
  Layer0FrameShown = false;
  Layer1FrameShown = false;
  g_dc.ops->setFrameBuffer(&g_dc, 0, (void *)Layer0Buffer[curLcdBufferIdx]);
  g_dc.ops->setFrameBuffer(&g_dc, 1, (void *)Layer1Buffer[0]);
  
  /* For the DBI interface display, application must wait for the first
  * frame buffer sent to the panel.
  */
  if ((g_dc.ops->getProperty(&g_dc) & kDC_FB_ReserveFrameBuffer) == 0)
  {
    while ((Layer0FrameShown == false) ||
           (Layer1FrameShown == false))
    {
    }
  }
  
  Layer0FrameShown = true;
  Layer1FrameShown = true;
  
  g_dc.ops->enableLayer(&g_dc, 0);
  g_dc.ops->enableLayer(&g_dc, 1);
}

static void APP_InitPxp(void)
{
  PXP_Init(APP_PXP);
  
  memset(Layer0Buffer, 0xFFFFFFFFU, sizeof(Layer0Buffer));
  memset(Layer1Buffer, 0U, sizeof(Layer1Buffer));
  
  //Copy to Layer 1 Using PXP
  ConfigureOB(APP_LAYER1_WIDTH,APP_LAYER1_HEIGHT, APP_LAYER1_WIDTH*4, (void *)Layer1Buffer[0]);
  ConfigurePS(0, 0, APP_LAYER1_WIDTH, APP_LAYER1_HEIGHT, APP_LAYER1_WIDTH, 4, kPXP_PsPixelFormatRGB888, (void *) Layer1Buffer[0]);
  ConfigureAS(0, 0, APP_LAYER1_WIDTH, APP_LAYER1_HEIGHT, APP_LAYER1_WIDTH, 4, kPXP_AsPixelFormatARGB8888, (void *) PXPLayer1Blit1_Bitmap0);
  PXP_Start(APP_PXP);
  pxpFinish();
  
  /* Disable CSC1, it is enabled by default. */
  PXP_EnableCsc1(APP_PXP, false);
}

static void redrawPXP()
{
  while (Layer0FrameShown == false)
  {
  }
  
  curLcdBufferIdx ^= 1U;
  ConfigureOB(APP_LAYER0_WIDTH,APP_LAYER0_HEIGHT, APP_LAYER0_WIDTH*4, (void *)Layer0Buffer[curLcdBufferIdx]);
  ConfigurePS(0, 0, APP_LAYER0_WIDTH, APP_LAYER0_HEIGHT, APP_LAYER0_WIDTH, 4, kPXP_PsPixelFormatRGB888, (void *) PXPLayer0Background_Bitmap0);
  ConfigureAS(544, 138, 48, 364, 48, 4, kPXP_AsPixelFormatARGB8888, (void *)PXPLayer0Blit1_Bitmap0);
  PXP_Start(APP_PXP);
  pxpFinish();
    
  uint32_t * outputPixel =  (uint32_t *)Layer0Buffer[curLcdBufferIdx];
     
  void * startingAddress = outputPixel + (138*720 + 336);
  ConfigureOB(48, 364, APP_LAYER0_WIDTH*4, startingAddress);
  ConfigurePS(0, 0, 48, 364, 720, 4, kPXP_PsPixelFormatRGB888, startingAddress);
  ConfigureAS(0, 0, 48, 364, 48, 4, kPXP_AsPixelFormatARGB8888, (void *) PXPLayer0Blit2_Bitmap0);
  PXP_Start(APP_PXP);
  pxpFinish();
  
  startingAddress = outputPixel + (138*720 + 127);
  ConfigureOB(48, 364, APP_LAYER0_WIDTH*4, startingAddress);
  ConfigurePS(0, 0, 48, 364, 720, 4, kPXP_PsPixelFormatRGB888, startingAddress);
  ConfigureAS(0, 0, 48, 364, 48, 4, kPXP_AsPixelFormatARGB8888, (void *) PXPLayer0Blit3_Bitmap0);
  PXP_Start(APP_PXP);
  pxpFinish();
     
  Layer0FrameShown = false;
  g_dc.ops->setFrameBuffer(&g_dc, 0, (void *)Layer0Buffer[curLcdBufferIdx]);
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

uint32_t getTime()
{
  return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static void pxp_task(void *pvParameters)
{
  APP_InitLcdif();
  APP_InitPxp();

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