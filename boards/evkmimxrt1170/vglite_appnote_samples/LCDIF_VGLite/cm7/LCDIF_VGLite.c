/*
* Copyright 2019 NXP
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

#include "fsl_lcdifv2.h"
#include "display_support.h"
/*-----------------------------------------------------------*/
#include "vg_lite.h"
#include "vglite_support.h"
#include "pin_mux.h"
#include "fsl_soc_src.h"

#include "Dial720Indexed8bpp.h"
#include "A8Mask.h"
/*******************************************************************************
* Definitions
******************************************************************************/
#define DEFAULT_SIZE     256.0f;

#define APP_BUFFER_COUNT 2

#define APP_LAYER0_WIDTH 720
#define APP_LAYER0_HEIGHT 720
#define APP_LAYER0_FORMAT kVIDEO_PixelFormatLUT8


#define APP_LAYER1_WIDTH        416
#define APP_LAYER1_HEIGHT       416
#define APP_LAYER1_FORMAT kVIDEO_PixelFormatXRGB8888

/*******************************************************************************
* Prototypes
******************************************************************************/
static void main_task(void *pvParameters);

/*******************************************************************************
* Variables
******************************************************************************/
static vg_lite_matrix_t matrix;
vg_lite_buffer_t alphaMask;
vg_lite_buffer_t renderTarget[APP_BUFFER_COUNT];

AT_NONCACHEABLE_SECTION_ALIGN(static uint8_t s_layer0[1][APP_LAYER0_HEIGHT][APP_LAYER0_WIDTH], FRAME_BUFFER_ALIGN);
AT_NONCACHEABLE_SECTION_ALIGN(static uint32_t vgLiteFrameBuffers[2][416][416], FRAME_BUFFER_ALIGN);

static uint8_t currentVGLiteBufferIndex = 0;
/*
* When new frame buffer sent to display, it might not be shown immediately.
* Application could use callback to get new frame shown notification, at the
* same time, when this flag is set, application could write to the older
* frame buffer.
*/
static volatile bool Layer0NewFrameShown = false;
static volatile bool Layer1NewFrameShown = false;

static dc_fb_info_t fbInfo;
static dc_fb_info_t vgLiteFbInfo;

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

static void Layer0CallBack(void *param, void *switchOffBuffer)
{
  Layer0NewFrameShown = true;
}

static void Layer1CallBack(void * param, void * switchOffBuffer)
{
  Layer1NewFrameShown = true;
}

static void InitLCDIFV2(void)
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

  //Populate the bitmaps for the Layers
  for(uint32_t i = 0; i < 720*720; i++)
  {
    uint8_t * pData = (uint8_t *)s_layer0[0];
    pData += i;
    *pData = Dial720Indexed8bpp_Bitmap0[i];
  }
  
  BOARD_PrepareDisplayController();
  
  status = g_dc.ops->init(&g_dc);
  if (kStatus_Success != status)
  {
    PRINTF("Display initialization failed\r\n");
    assert(0);
  }
  
  //LAYER 0: THIS WILL BE USED AS A STATIC LAYER
  g_dc.ops->getLayerDefaultConfig(&g_dc, 0, &fbInfo);
  fbInfo.pixelFormat = APP_LAYER0_FORMAT;
  fbInfo.startX = 0;
  fbInfo.startY = 0;
  fbInfo.width       = APP_LAYER0_WIDTH;
  fbInfo.height      = APP_LAYER0_HEIGHT;
  fbInfo.strideBytes = APP_LAYER0_WIDTH;
  g_dc.ops->setLayerConfig(&g_dc, 0, &fbInfo);
  
  //LAYER 1: THIS LAYER WILL BE USED FOR VGLite
  vgLiteFbInfo.startX = 152;
  vgLiteFbInfo.startY = 152;
  vgLiteFbInfo.pixelFormat = APP_LAYER1_FORMAT;
  vgLiteFbInfo.width       = 416;
  vgLiteFbInfo.height      = 416;
  vgLiteFbInfo.strideBytes = 416*4;
  g_dc.ops->setLayerConfig(&g_dc, 1, &vgLiteFbInfo);

  g_dc.ops->setCallback(&g_dc, 0, Layer0CallBack, NULL);
  g_dc.ops->setCallback(&g_dc, 1, Layer1CallBack, NULL);
  
  Layer0NewFrameShown = false;
  Layer1NewFrameShown = false;
  LCDIFV2_SetLut(LCDIFV2, 0, Dial720Indexed8bpp_CLUT, 255, false);
  LCDIFV2_SetLayerBlendConfig(LCDIFV2, 0, &blendConfig0);
  LCDIFV2_SetLayerBlendConfig(LCDIFV2, 1, &blendConfig1);
 
  g_dc.ops->setFrameBuffer(&g_dc, 0, (void *)s_layer0[0]);
  g_dc.ops->setFrameBuffer(&g_dc, 1, (void *)vgLiteFrameBuffers[0]);
  
  /* For the DBI interface display, application must wait for the first
  * frame buffer sent to the panel.
  */
  if ((g_dc.ops->getProperty(&g_dc) & kDC_FB_ReserveFrameBuffer) == 0)
  {
    while (Layer0NewFrameShown == false || 
           Layer1NewFrameShown == false)
    {
    }
  }
  
  Layer0NewFrameShown = true;
  Layer1NewFrameShown = true;
  
  g_dc.ops->enableLayer(&g_dc, 0);
  g_dc.ops->enableLayer(&g_dc, 1);
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
  if (xTaskCreate(main_task, "main_task", configMINIMAL_STACK_SIZE + 200, NULL, configMAX_PRIORITIES - 1, NULL) !=
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

static void cleanup(void)
{
  vg_lite_close();
}

static vg_lite_error_t init_vg_lite(void)
{
  vg_lite_error_t error = VG_LITE_SUCCESS;
  
  // Initialize the draw.
  error = vg_lite_init(256, 256);
  if (error != VG_LITE_SUCCESS)
  {
    PRINTF("vg_lite_init for alpha mask failed, error %d\n", error);
    return error;
  }
  
  alphaMask.width = APP_LAYER1_WIDTH;
  alphaMask. height = APP_LAYER1_HEIGHT;
  alphaMask.format = VG_LITE_A8;
  alphaMask.stride = APP_LAYER1_WIDTH;
  alphaMask.memory = (void *)A8Mask_Bitmap0;
  alphaMask.address = (uint32_t)A8Mask_Bitmap0;
  
  renderTarget[0].width = APP_LAYER1_WIDTH;
  renderTarget[0].height = APP_LAYER1_HEIGHT;
  renderTarget[0].stride = APP_LAYER1_WIDTH*4;
  renderTarget[0].format = VG_LITE_BGRA8888;
  renderTarget[0].memory = (void *)vgLiteFrameBuffers[0];
  renderTarget[0].address = (uint32_t)vgLiteFrameBuffers[0];
  
  renderTarget[1].width = APP_LAYER1_WIDTH;
  renderTarget[1].height = APP_LAYER1_HEIGHT;
  renderTarget[1].stride = APP_LAYER1_WIDTH*4;
  renderTarget[1].format = VG_LITE_BGRA8888;
  renderTarget[1].memory = (void *)vgLiteFrameBuffers[1];
  renderTarget[1].address = (uint32_t)vgLiteFrameBuffers[1];
    
  return error;
}

uint32_t getTime()
{
  return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static void redrawVGLite(void)
{
  vg_lite_error_t error;
  
  while (Layer1NewFrameShown == false)
  {
  }
  currentVGLiteBufferIndex ^= 1U;
  vg_lite_clear(&renderTarget[currentVGLiteBufferIndex], NULL, 0xFFFF0000);
  vg_lite_identity(&matrix);
  vg_lite_blit(&renderTarget[currentVGLiteBufferIndex], &alphaMask, &matrix, VG_LITE_BLEND_DST_IN, 0, VG_LITE_FILTER_POINT);
  vg_lite_finish();
  Layer1NewFrameShown = false;
  g_dc.ops->setFrameBuffer(&g_dc, 1, (void *)vgLiteFrameBuffers[currentVGLiteBufferIndex]);
}

static void main_task(void *pvParameters)
{
  InitLCDIFV2();
  
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
    redrawVGLite();
    n++;
    if (n >= 60)
    {
      time = getTime() - startTime;
      float fTime = (float) time / 1000.0f;
      float fps = (float)n / fTime;
      PRINTF("VGLite: %d frames in %d timeUnits: %f fps\r\n", n, time, fps);
      n         = 0;
      startTime = getTime();
    }
  }
}