/*
* Copyright 2022-2023 NXP
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
#include <math.h>
#include "pin_mux.h"
#include "fsl_soc_src.h"

/*******************************************************************************
* Definitions
******************************************************************************/
#define APP_BUFFER_COUNT 2
#define DEFAULT_SIZE     256.0f;

#define DEG2RAD(x)	(0.01745329252 * (x))

#define SIMPLE_BLIT             0
#define NO_BANDING              1
#define BLENDING                2
#define SIMPLE_TRANSFORMATION   3
#define PERSPECTIVE_2_5_D       4
#define VECTOR_AND_RASTER       5

#define TEST_STEP       PERSPECTIVE_2_5_D

#if(TEST_STEP == SIMPLE_BLIT)
#include "Dial4444.h"
#elif((TEST_STEP == NO_BANDING) || (TEST_STEP == BLENDING))
#include "Dial8888.h"
#endif

#if((TEST_STEP == SIMPLE_TRANSFORMATION) || (TEST_STEP == PERSPECTIVE_2_5_D))
#include "Square.h"
#endif

#if(TEST_STEP == VECTOR_AND_RASTER)
#include "A8Mask.h"
#include "Landscape.h"
#endif

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
static vg_lite_buffer_t dial;
static vg_lite_buffer_t album1;
static vg_lite_buffer_t alphaMask;
static vg_lite_buffer_t renderTarget;

vg_lite_path_t lsBackground;
vg_lite_path_t lsMiddleground;
vg_lite_path_t lsForeground;


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

static vg_lite_error_t init_vg_lite(void)
{
  vg_lite_error_t error = VG_LITE_SUCCESS;
  
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
  
  error = vg_lite_init(256, 256);
  if (error)
  {
    PRINTF("vg_lite_init failed: vg_lite_init() returned error %d\n", error);
    return error;
  }
  
  
#if(TEST_STEP == SIMPLE_BLIT)
  dial.width = 720;
  dial.height = 720;
  dial.stride = 720*2;
  dial.format = VG_LITE_RGBA4444;
  dial.memory = (void *)Dial4444_Bitmap;
  dial.address = (uint32_t)Dial4444_Bitmap;
#elif((TEST_STEP == NO_BANDING) || (TEST_STEP == BLENDING))
  dial.width = 720;
  dial.height = 720;
  dial.stride = 720*4;
  dial.format = VG_LITE_RGBA8888;
  dial.memory = (void *)Dial8888_Bitmap;
  dial.address = (uint32_t)Dial8888_Bitmap;
#endif
  
#if((TEST_STEP == SIMPLE_TRANSFORMATION) || (TEST_STEP == PERSPECTIVE_2_5_D))
  album1.width = 384;
  album1.height = 384;
  album1.stride = 384*4;
  album1.format = VG_LITE_RGBA8888;
  album1.memory = (void*)Square_Bitmap0;
  album1.address = (uint32_t)Square_Bitmap0;
#endif  
  
#if(TEST_STEP == VECTOR_AND_RASTER)
  renderTarget.width = 416;
  renderTarget. height = 416;
  renderTarget.format = VG_LITE_RGBA8888;
  error = vg_lite_allocate(&renderTarget);
  if (error)
  {
    PRINTF("vg_lite_allocate for renderTarget failed, error %d\n", error);
    return error;
  }
  
  alphaMask.width = 416;
  alphaMask.height = 416;
  alphaMask.stride = 416;
  alphaMask.format = VG_LITE_A8;
  alphaMask.memory = (void *)A8Mask_Bitmap0;
  alphaMask.address = (uint32_t)A8Mask_Bitmap0;
  
  error = vg_lite_init_path(&lsBackground, VG_LITE_FP32, VG_LITE_LOW, LandscapeBackground_len, (void *)LandscapeBackground,
                            0, 0, 720, 1280);
  if (error)
  {
    PRINTF("vg_lite_init_path for background failed, error %d\n", error);
    return error;
  }
  
  error = vg_lite_init_path(&lsMiddleground, VG_LITE_FP32, VG_LITE_LOW, LandscapeMiddleground_len, (void *)LandscapeMiddleground,
                            0, 0, 720, 1280);
  if (error)
  {
    PRINTF("vg_lite_init_path for middleground failed, error %d\n", error);
    return error;
  }
  
  error = vg_lite_init_path(&lsForeground, VG_LITE_FP32, VG_LITE_LOW, LandscapeForeground_len, (void *)LandscapeForeground,
                            0, 0, 720, 1280);
  if (error)
  {
    PRINTF("vg_lite_init_path for foreground failed, error %d\n", error);
    return error;
  }
#endif
  
  return error;
}

static void Render(vg_lite_buffer_t * rt)
{
  vg_lite_identity(&matrix);
  vg_lite_blit(rt, &dial, &matrix, VG_LITE_BLEND_NONE, 0, VG_LITE_FILTER_POINT);
}

static void RenderBlend(vg_lite_buffer_t * rt)
{
  vg_lite_identity(&matrix);
  vg_lite_blit(rt, &dial, &matrix, VG_LITE_BLEND_SRC_OVER, 0, VG_LITE_FILTER_POINT);
}

static void RenderSimpleTransformation(vg_lite_buffer_t * rt)
{
  vg_lite_identity(&matrix);
  vg_lite_translate(168.0f, 168.0f, &matrix);
  vg_lite_translate(192.0f, 192.0f, &matrix);
  vg_lite_rotate(45.0f, &matrix);
  vg_lite_translate(-192.0f, -192.0f, &matrix);
  vg_lite_blit(rt, &album1, &matrix, VG_LITE_BLEND_SRC_OVER, 0, VG_LITE_FILTER_POINT);
}

void CreatePerspectiveMatrix(float width, float height, float angle, float FOV, vg_lite_matrix_t *outMatrix)
{
  float newWidth; 
  float newheight; 
  vg_lite_point4_t src, dst;

  newWidth = cos(angle) * width;
  newheight = sin(angle) * (height/2) * FOV;
  
  dst[0].x = 0;         dst[0].y = height;
  dst[1].x = newWidth;  dst[1].y = height - newheight;  
  dst[2].x = newWidth;  dst[2].y = newheight;  
  dst[3].x = 0;         dst[3].y = 0;  
  
  src[0].x = 0;         src[0].y = height;
  src[1].x = width;     src[1].y = height;  
  src[2].x = width;     src[2].y = 0;  
  src[3].x = 0;         src[3].y = 0;  
  
  vg_lite_get_transform_matrix(src, dst, outMatrix) ;
}

static void RenderPerspectiveTransformation(vg_lite_buffer_t *rt)
{
  static float angle = 0.0f;
  static int direction = 1;
  vg_lite_identity(&matrix);  
  CreatePerspectiveMatrix(384.0f, 384.0f, DEG2RAD(angle), 0.4f, &matrix);
  matrix.m[0][2] += 168;
  matrix.m[1][2] += 168;
  vg_lite_blit(rt, &album1, &matrix, VG_LITE_BLEND_SRC_OVER, 0, VG_LITE_FILTER_POINT);
  
  if(0 == direction)
  {
    angle += 0.5;
    if(angle>=60.0f)
      direction = 1;
  }
  else
  {
    angle -= 0.5;
    if(angle<=-60.0f)
      direction = 0;
  }
}

void RenderVectorAndRaster(vg_lite_buffer_t * rt)
{
  vg_lite_clear(&renderTarget, NULL, 0xFFDCF1FF);
  
  static int frontX = -350;
  static int frontIncrement = 1;
  
  frontX += frontIncrement;
  
  if(frontX >= 0)
    frontIncrement = -1;
  if(frontX <= -350)
    frontIncrement = 1;
  
  float middleX = (float)frontX * 0.5;
  float backgroundX = (float)frontX * 0.1;
  
  vg_lite_identity(&matrix);
  vg_lite_translate(0, backgroundX , &matrix);
  vg_lite_draw(&renderTarget, &lsBackground, VG_LITE_FILL_EVEN_ODD, &matrix, VG_LITE_BLEND_NONE, 0xFFFBE6F1);
  
  vg_lite_identity(&matrix);
  vg_lite_translate(0, middleX, &matrix);
  vg_lite_draw(&renderTarget, &lsMiddleground, VG_LITE_FILL_EVEN_ODD, &matrix, VG_LITE_BLEND_NONE, 0xFFE7DCED);
  
  vg_lite_identity(&matrix);
  vg_lite_translate(0, frontX , &matrix);
  vg_lite_draw(&renderTarget, &lsForeground, VG_LITE_FILL_EVEN_ODD, &matrix, VG_LITE_BLEND_NONE, 0xFF696890);
  
  vg_lite_identity(&matrix);
  vg_lite_blit(&renderTarget, &alphaMask, &matrix, VG_LITE_BLEND_DST_IN, 0, VG_LITE_FILTER_POINT);
  
  vg_lite_identity(&matrix);
  //Now render to the render target
  vg_lite_translate(360-208, 640-208, &matrix);
  vg_lite_blit(rt, &renderTarget, &matrix, VG_LITE_BLEND_SRC_OVER, 0, VG_LITE_FILTER_POINT);
}

static void redraw()
{
  vg_lite_buffer_t *rt = VGLITE_GetRenderTarget(&window);
  if (rt == NULL)
  {
    PRINTF("vg_lite_get_renderTarget error\r\n");
    while (1)
      ;
  }
  vg_lite_clear(rt, NULL, 0xFF404040);
#if(TEST_STEP == SIMPLE_BLIT)
  Render(rt);  
#elif(TEST_STEP == NO_BANDING)
  Render(rt);
#elif(TEST_STEP == BLENDING)
  RenderBlend(rt);
#elif(TEST_STEP == SIMPLE_TRANSFORMATION)
  RenderSimpleTransformation(rt);
#elif(TEST_STEP == PERSPECTIVE_2_5_D)
  RenderPerspectiveTransformation(rt);
#elif(TEST_STEP == VECTOR_AND_RASTER)
  RenderVectorAndRaster(rt);
#endif
  
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
    if (n > 60)
    {
      time = getTime() - startTime;
      PRINTF("%d frames in %d seconds: %d fps\r\n", n, time / 1000, n * 1000 / time);
      n         = 0;
      startTime = getTime();
    }
  }
}
