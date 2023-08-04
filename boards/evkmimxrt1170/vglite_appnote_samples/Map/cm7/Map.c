/*
* Copyright 2019 NXP
* All rights reserved.
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"
#include "task.h"

#include "fsl_debug_console.h"
#include "board.h"
/*-----------------------------------------------------------*/
#include "vg_lite.h"
#include "vglite_support.h"
#include "vglite_window.h"
#include "Elm.h"

#include "RotatedMap.h"
#include "A8Mask.h"
#include "Thermostat32bpp.h"
#include "CoolingIconARGB_Arm.h"
#include "LightsIconARGB_Arm.h"
#include "LockIconARGB_Arm.h"
#include "SpeakerIconARGB_Arm.h"
#include "PathAnimation.h"

#include "pin_mux.h"
#include "fsl_soc_src.h"
/*******************************************************************************
* Definitions
******************************************************************************/
#define APP_BUFFER_COUNT 2
#define DEFAULT_SIZE     256.0f;

typedef struct elm_render_buffer
{
  ElmBuffer handle;
  vg_lite_buffer_t *buffer;
} ElmRenderBuffer;

/*******************************************************************************
* Prototypes
******************************************************************************/
static void vglite_task(void *pvParameters);

/*******************************************************************************
* Variables
******************************************************************************/
static vg_lite_display_t display;
static vg_lite_window_t window;

static vg_lite_matrix_t matrix;
vg_lite_buffer_t alphaMask;
vg_lite_buffer_t background;
vg_lite_buffer_t coolingIcon;
vg_lite_buffer_t lightsIcon;
vg_lite_buffer_t lockIcon;
vg_lite_buffer_t speakerIcon;
vg_lite_buffer_t renderTarget;
vg_lite_buffer_t topbar;

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


int main(void)
{
  /* Init board hardware. */
  BOARD_ConfigMPU();
  BOARD_BootClockRUN();
  BOARD_ResetDisplayMix();
  BOARD_InitLpuartPins();
  BOARD_InitMipiPanelPins();
  BOARD_InitDebugConsole();
  
  if (xTaskCreate(vglite_task, "vglite_task", configMINIMAL_STACK_SIZE + 200, NULL, configMAX_PRIORITIES - 1, NULL) !=
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
  
  error = VGLITE_CreateDisplay(&display);
  if (error)
  {
    PRINTF("VGLITE_CreateDisplay failed: VGLITE_CreateDisplay() returned error %d\n", error);
    return error;
  }
  // Initialize the window.
  error = VGLITE_CreateWindow(&display, &window);
  if (error)
  {
    PRINTF("VGLITE_CreateWindow failed: VGLITE_CreateWindow() returned error %d\n", error);
    return error;
  }
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
  
  topbar.width = 48;
  topbar.height = 1280;
  topbar.format = VG_LITE_ABGR4444;
  error = vg_lite_allocate(&topbar);
  if (error)
  {
    PRINTF("vg_lite_allocate for topbar failed, error %d\n", error);
    return error;
  }
  vg_lite_clear(&topbar, 0, 0x40000000);
  
  background.width = 720;
  background.height = 1280;
  background.format = VG_LITE_BGRA8888;
  background.stride = 720*4;
  background.memory = (void *)Thermostat32bpp_Bitmap0;
  background.address = (uint32_t)Thermostat32bpp_Bitmap0;
    
  renderTarget.width = 416;
  renderTarget. height = 416;
  renderTarget.format = VG_LITE_BGRA8888;
  error = vg_lite_allocate(&renderTarget);
  if (error)
  {
    PRINTF("vg_lite_allocate for renderTarget failed, error %d\n", error);
    return error;
  }
  
  coolingIcon.width = 144;
  coolingIcon.height = 144;
  coolingIcon.format = VG_LITE_RGBA4444;
  coolingIcon.stride = 144*2;
  coolingIcon.memory = (void *)CoolingIconARGB_Arm_Bitmap0;
  coolingIcon.address = (uint32_t)CoolingIconARGB_Arm_Bitmap0;
  
  lightsIcon.width = 144;
  lightsIcon.height = 144;
  lightsIcon.format = VG_LITE_RGBA4444;
  lightsIcon.stride = 144*2;
  lightsIcon.memory = (void *)LightsIconARGB_Arm_Bitmap0;
  lightsIcon.address = (uint32_t)LightsIconARGB_Arm_Bitmap0;
  
  speakerIcon.width = 128;
  speakerIcon.height = 155;
  speakerIcon.format = VG_LITE_RGBA4444;
  speakerIcon.stride = 128*2;
  speakerIcon.memory = (void *)SpeakerIconARGB_Arm_Bitmap0;
  speakerIcon.address = (uint32_t)SpeakerIconARGB_Arm_Bitmap0;
  
  lockIcon.width = 128;
  lockIcon.height = 103;
  lockIcon.format = VG_LITE_RGBA4444;
  lockIcon.stride = 128*2;
  lockIcon.memory = (void *)LockIconARGB_Arm_Bitmap0;
  lockIcon.address = (uint32_t)LockIconARGB_Arm_Bitmap0;
  
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

vg_lite_error_t DrawRadialBars(vg_lite_buffer_t * target)
{
  vg_lite_error_t error;
  static uint32_t pathIndex = 333;
  vg_lite_init_path(&radialPath, VG_LITE_FP32, VG_LITE_LOW, GaugePathLengths[pathIndex], (void *)GaugePaths[pathIndex],
                    0, 0, 720, 1280);
  
  vg_lite_identity(&matrix);
  vg_lite_translate(-152, -432, &matrix);
  error = vg_lite_draw(target, &radialPath, VG_LITE_FILL_EVEN_ODD, &matrix, VG_LITE_BLEND_SRC_OVER, 0x7CA2FF7C);
  
  return error;
}

static void redraw()
{
  int status = 0;
  static float mapAngle = 0.0f;
  static float mapScale = 1.0f;
  static float mapScaleIncrement = 0.005f;
  
  vg_lite_buffer_t *rt = VGLITE_GetRenderTarget(&window);
  //vg_lite_buffer_t *rt = VGLITE_GetOffscreenRenderTarget(&window);
  if (rt == NULL)
  {
    PRINTF("vg_lite_get_renderTarget error\r\n");
    while (1)
      ;
  }
  vg_lite_clear(rt, 0, 0x00000000);
  vg_lite_clear(&renderTarget, 0, 0x00000000);
  ElmReset(rotatedMapHandle, ELM_PROP_TRANSFER_BIT);
  ElmTransfer(rotatedMapHandle, -282, -637);
  ElmTransfer(rotatedMapHandle, 490, 845);
  ElmRotate(rotatedMapHandle, mapAngle);
  ElmScale(rotatedMapHandle, 0.8, 0.8);
  ElmTransfer(rotatedMapHandle, -490, -845);
  status = render(&renderTarget, rotatedMapHandle);
  if (status == -1)
  {
    PRINTF("ELM Render rotatedMapHandle Failed");
    return;
  }
  DrawRadialBars(&renderTarget);
  
  vg_lite_identity(&matrix);
  vg_lite_blit(&renderTarget, &alphaMask, &matrix, VG_LITE_BLEND_DST_IN, 0, VG_LITE_FILTER_POINT);
  
  vg_lite_blit(rt, &background, &matrix, VG_LITE_BLEND_NONE, 0, VG_LITE_FILTER_POINT);
 
  vg_lite_identity(&matrix);
  vg_lite_translate(360-208, 640-208, &matrix);
  vg_lite_blit(rt, &renderTarget, &matrix, VG_LITE_BLEND_SRC_OVER, 0, VG_LITE_FILTER_POINT);
  
  vg_lite_identity(&matrix);
  vg_lite_translate(113.5f, 1280-1023.5f-144.0f, &matrix);
  vg_lite_blit(rt, &lightsIcon, &matrix, VG_LITE_BLEND_SRC_OVER, 0, VG_LITE_FILTER_POINT);
  
  vg_lite_identity(&matrix);
  vg_lite_translate(463.5f, 1280-1023.5f-144.0f, &matrix);
  vg_lite_blit(rt, &coolingIcon, &matrix, VG_LITE_BLEND_SRC_OVER, 0, VG_LITE_FILTER_POINT);
  
  vg_lite_identity(&matrix);
  vg_lite_translate(121.5f, 1280-108.160f-155.0f, &matrix);
  vg_lite_blit(rt, &speakerIcon, &matrix, VG_LITE_BLEND_SRC_OVER, 0, VG_LITE_FILTER_POINT);
  
  vg_lite_identity(&matrix);
  vg_lite_translate(471.5f, 1280-134.024f-103.0f, &matrix);
  vg_lite_blit(rt, &lockIcon, &matrix, VG_LITE_BLEND_SRC_OVER, 0, VG_LITE_FILTER_POINT);
  
  vg_lite_identity(&matrix);
  vg_lite_translate(672, 0, &matrix);
  vg_lite_blit(rt, &topbar, &matrix, VG_LITE_BLEND_SRC_OVER, 0, VG_LITE_FILTER_POINT);
  
  mapScale += mapScaleIncrement;
  if(mapScale >= 1.5f)
    mapScaleIncrement = - 0.005f;
  if(mapScale <= 1.0f)
    mapScaleIncrement = 0.005f;
  mapAngle+=1.0f;

  VGLITE_SwapBuffers(&window);
  
  return;
}

uint32_t getTime()
{
  return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static void vglite_task(void *pvParameters)
{
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
    redraw();
    n++;
    if (n >= 60)
    {
      time = getTime() - startTime;
      float fTime = (float) time / 1000.0f;
      float fps = (float)n / fTime;
      PRINTF("%d frames in %d timeUnits: %f fps\r\n", n, time, fps);
      n         = 0;
      startTime = getTime();
    }
  }
}
