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
#include <math.h>
#include "pin_mux.h"
#include "fsl_soc_src.h"
/*******************************************************************************
* Definitions
******************************************************************************/
#define APP_BUFFER_COUNT 2
#define DEFAULT_SIZE     256.0f;

#define INITIAL_LOGO    0
#define FILL_RULE       1
#define TRANSFORM_INTUITION     2
#define TRANSFORM_VG_LITE        3
#define COLOR           4
#define BLEND           5
#define LINEAR_GRADIENTS        6

#define TEST_STEP       INITIAL_LOGO

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

union opcodeAndCoord {
  float		coord;
  uint32_t	opcode;
};

static union opcodeAndCoord nPathData[] = {
  {.opcode=VLC_OP_MOVE}, {0.0f}, {0.0f},
  {.opcode=VLC_OP_LINE}, {74.9f}, {0.0f},
  {.opcode=VLC_OP_LINE}, {200.66f}, {147.21f},
  {.opcode=VLC_OP_LINE}, {200.66f}, {0.0f},
  {.opcode=VLC_OP_LINE}, {274.81f}, {121.88f},
  {.opcode=VLC_OP_LINE}, {200.66f}, {244.57f},
  {.opcode=VLC_OP_LINE}, {74.9f}, {97.48f},
  {.opcode=VLC_OP_LINE}, {74.9f}, {244.57f},
  {.opcode=VLC_OP_LINE}, {0.0f}, {244.57f},
  {.opcode=VLC_OP_END}    
};

static union opcodeAndCoord xPathData[] = {
  {.opcode=VLC_OP_MOVE}, {218.86f}, {0.0f},
  {.opcode=VLC_OP_LINE}, {307.51f}, {0.0f},
  {.opcode=VLC_OP_LINE}, {357.78f}, {80.51f},
  {.opcode=VLC_OP_LINE}, {408.13f}, {0.0f},
  {.opcode=VLC_OP_LINE}, {495.97f}, {0.0f},
  {.opcode=VLC_OP_LINE}, {422.57f}, {121.88f},
  {.opcode=VLC_OP_LINE}, {495.97f}, {244.57f},
  {.opcode=VLC_OP_LINE}, {408.13f}, {244.57f},
  {.opcode=VLC_OP_LINE}, {357.78f}, {165.04f},
  {.opcode=VLC_OP_LINE}, {307.51f}, {244.57f},
  {.opcode=VLC_OP_LINE}, {218.86f}, {244.57f},
  {.opcode=VLC_OP_LINE}, {292.31f}, {121.88f},    
  {.opcode=VLC_OP_END}    
};

static union opcodeAndCoord pPathData[] = {
  {.opcode=VLC_OP_MOVE}, {514.92f}, {0.0f},
  {.opcode=VLC_OP_LINE}, {648.62f}, {0.0f},
  {.opcode=VLC_OP_CUBIC}, {740.62}, {0.0f}, {740.62f}, {191.07f}, {648.62f}, {191.07f},
  {.opcode=VLC_OP_LINE}, {514.92f}, {191.07f},
  {.opcode=VLC_OP_LINE}, {514.92f}, {244.57f},
  {.opcode=VLC_OP_LINE}, {440.06f}, {121.88f},
  {.opcode=VLC_OP_LINE}, {514.92f}, {0.0f},   
  
  {.opcode=VLC_OP_MOVE}, {514.92f}, {61.14f},
  {.opcode=VLC_OP_LINE}, {613.96f}, {61.14f},
  {.opcode=VLC_OP_CUBIC}, {639.4f}, {61.14f}, {639.4f}, {130.44f}, {613.96f}, {130.44f},
  {.opcode=VLC_OP_LINE}, {514.92f}, {130.44f},
  
  {.opcode=VLC_OP_END}    
};



static vg_lite_path_t nPath, xPath, pPath;
vg_lite_matrix_t * gradientMatrix;
vg_lite_linear_gradient_t nGradient, xGradient, pGradient;

uint32_t nStopColors[] = {0xFFE8B410, 0xFF000000};
uint32_t nStops[] = {0, 255};

uint32_t xStopColors[] = {0xFF000000, 0xFF8BAED9, 0xFF000000};
uint32_t xStops[] = {0, 128, 255};

uint32_t pStopColors[] = {0xFF000000, 0xFFC9D121};
uint32_t pStops[] = {0, 255};

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
  
#if(TEST_STEP == LINEAR_GRADIENTS)
  vg_lite_init_grad(&nGradient);
  gradientMatrix = vg_lite_get_grad_matrix(&nGradient);
  vg_lite_identity(gradientMatrix);
  vg_lite_set_grad(&nGradient, 2, nStopColors, nStops);
  vg_lite_update_grad(&nGradient);
  
  vg_lite_init_grad(&xGradient);
  gradientMatrix = vg_lite_get_grad_matrix(&xGradient);
  vg_lite_identity(gradientMatrix);
  vg_lite_set_grad(&xGradient, 3, xStopColors, xStops);
  vg_lite_update_grad(&xGradient);
  
  vg_lite_init_grad(&pGradient);
  gradientMatrix = vg_lite_get_grad_matrix(&pGradient);
  vg_lite_identity(gradientMatrix);
  vg_lite_set_grad(&pGradient, 2, pStopColors, pStops);
  vg_lite_update_grad(&pGradient);
#endif
  
  return error;
}

static void redraw()
{
  vg_lite_error_t error;
  vg_lite_buffer_t *rt = VGLITE_GetRenderTarget(&window);
  if (rt == NULL)
  {
    PRINTF("vg_lite_get_renderTarget error\r\n");
    while (1)
      ;
  }
  
  error = vg_lite_init_path(&nPath, VG_LITE_FP32, VG_LITE_HIGH, sizeof(nPathData), nPathData, 0, 0, 720, 1280);
  error = vg_lite_init_path(&xPath, VG_LITE_FP32, VG_LITE_HIGH, sizeof(xPathData), xPathData, 0, 0, 720, 1280);
  error = vg_lite_init_path(&pPath, VG_LITE_FP32, VG_LITE_HIGH, sizeof(pPathData), pPathData, 0, 0, 720, 1280);
  if(error != VG_LITE_SUCCESS)
  {
    PRINTF("Error while uploading path\n");
  }
  
  vg_lite_identity(&matrix);
#if(TEST_STEP == INITIAL_LOGO)
  vg_lite_clear(rt, NULL, 0xFFFFFFFF);
  vg_lite_draw(rt, &nPath, VG_LITE_FILL_EVEN_ODD, &matrix, VG_LITE_BLEND_NONE, 0xFF000000);
  vg_lite_draw(rt, &xPath, VG_LITE_FILL_EVEN_ODD, &matrix, VG_LITE_BLEND_NONE, 0xFF000000);
  vg_lite_draw(rt, &pPath, VG_LITE_FILL_EVEN_ODD, &matrix, VG_LITE_BLEND_NONE, 0xFF000000);

#elif(TEST_STEP == FILL_RULE)
  vg_lite_clear(rt, NULL, 0xFFFFFFFF);
  vg_lite_draw(rt, &nPath, VG_LITE_FILL_EVEN_ODD, &matrix, VG_LITE_BLEND_NONE, 0xFF000000);
  vg_lite_draw(rt, &xPath, VG_LITE_FILL_EVEN_ODD, &matrix, VG_LITE_BLEND_NONE, 0xFF000000);
  vg_lite_draw(rt, &pPath, VG_LITE_FILL_NON_ZERO, &matrix, VG_LITE_BLEND_NONE, 0xFF000000);

#elif(TEST_STEP == TRANSFORM_INTUITION)
  vg_lite_clear(rt, NULL, 0xFFFFFFFF);
  
  float userMatrix[9] = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
  
  //Rotate around the origin:
  float localPI = 3.1415926f;
  float angleInRadians = (28.0f * localPI) / 180.0f;
  
  userMatrix[0] = cosf(angleInRadians);
  userMatrix[1] = -sinf(angleInRadians);
  userMatrix[3] = sinf(angleInRadians);
  userMatrix[4] = cosf(angleInRadians);
  
  vg_lite_draw(rt, &nPath, VG_LITE_FILL_EVEN_ODD, (vg_lite_matrix_t *)&userMatrix, VG_LITE_BLEND_NONE, 0xFF000000);
  vg_lite_draw(rt, &xPath, VG_LITE_FILL_EVEN_ODD, (vg_lite_matrix_t *)&userMatrix, VG_LITE_BLEND_NONE, 0xFF000000);
  vg_lite_draw(rt, &pPath, VG_LITE_FILL_EVEN_ODD, (vg_lite_matrix_t *)&userMatrix, VG_LITE_BLEND_NONE, 0xFF000000);

#elif(TEST_STEP == TRANSFORM_VG_LITE)
  vg_lite_clear(rt, NULL, 0xFFFFFFFF);
  
  vg_lite_rotate(28.0f, &matrix);
  
  vg_lite_draw(rt, &nPath, VG_LITE_FILL_EVEN_ODD, &matrix, VG_LITE_BLEND_NONE, 0xFF000000);
  vg_lite_draw(rt, &xPath, VG_LITE_FILL_EVEN_ODD, &matrix, VG_LITE_BLEND_NONE, 0xFF000000);
  vg_lite_draw(rt, &pPath, VG_LITE_FILL_EVEN_ODD, &matrix, VG_LITE_BLEND_NONE, 0xFF000000);

#elif(TEST_STEP == COLOR)
  vg_lite_clear(rt, NULL, 0xFFFFFFFF);
    
  vg_lite_draw(rt, &nPath, VG_LITE_FILL_EVEN_ODD, &matrix, VG_LITE_BLEND_NONE, 0xFF10B4E8);
  vg_lite_draw(rt, &xPath, VG_LITE_FILL_EVEN_ODD, &matrix, VG_LITE_BLEND_NONE, 0xFFD9AE8B);
  vg_lite_draw(rt, &pPath, VG_LITE_FILL_EVEN_ODD, &matrix, VG_LITE_BLEND_NONE, 0xFF21D1C9);
#elif(TEST_STEP == BLEND)    
  vg_lite_clear(rt, NULL, 0xFF0000FF);
  vg_lite_draw(rt, &nPath, VG_LITE_FILL_EVEN_ODD, &matrix, VG_LITE_BLEND_SRC_OVER, 0x7F10B4E8);
  vg_lite_draw(rt, &xPath, VG_LITE_FILL_EVEN_ODD, &matrix, VG_LITE_BLEND_SRC_OVER, 0x7FD9AE8B);
  vg_lite_draw(rt, &pPath, VG_LITE_FILL_EVEN_ODD, &matrix, VG_LITE_BLEND_SRC_OVER, 0xgradientMatrix7F21D1C9);
#elif(TEST_STEP == LINEAR_GRADIENTS)
  vg_lite_clear(rt, NULL, 0xFFFFFFFF);
  
  gradientMatrix = vg_lite_get_grad_matrix(&nGradient);
  vg_lite_identity(gradientMatrix);
  vg_lite_scale(1.073f, 1.0, gradientMatrix);
  vg_lite_draw_gradient(rt, &nPath, VG_LITE_FILL_EVEN_ODD, &matrix, &nGradient, VG_LITE_BLEND_NONE);
  
  gradientMatrix = vg_lite_get_grad_matrix(&xGradient);
  vg_lite_identity(gradientMatrix);
  vg_lite_translate(218.86f, 0.0f, gradientMatrix);
  vg_lite_scale(1.0823f, 1.0f, gradientMatrix);
  vg_lite_draw_gradient(rt, &xPath, VG_LITE_FILL_EVEN_ODD, &matrix, &xGradient, VG_LITE_BLEND_NONE);
  
  gradientMatrix = vg_lite_get_grad_matrix(&pGradient);
  vg_lite_identity(gradientMatrix);
  vg_lite_translate(440.06f, 0.0f, gradientMatrix);
  vg_lite_scale(1.0835f, 1.0f, gradientMatrix);
  vg_lite_draw_gradient(rt, &pPath, VG_LITE_FILL_EVEN_ODD, &matrix, &pGradient, VG_LITE_BLEND_NONE);
  
  //Draw the Image Gradient as a guide:
  vg_lite_identity(&matrix);
  vg_lite_translate(0.0f, 250.0f, &matrix);
  vg_lite_scale(1.073f, 10.0f, &matrix);
  vg_lite_blit(rt, &nGradient.image, &matrix, VG_LITE_BLEND_NONE, 0, VG_LITE_FILTER_POINT);
  
  //Draw the Image Gradient as a guide:
  vg_lite_identity(&matrix);
  vg_lite_translate(218.86f, 260.0f, &matrix);
  vg_lite_scale(1.0823f, 10.0f, &matrix);
  vg_lite_blit(rt, &xGradient.image, &matrix, VG_LITE_BLEND_NONE, 0, VG_LITE_FILTER_POINT);
  
  //Draw the Image Gradient as a guide:
  vg_lite_identity(&matrix);
  vg_lite_translate(440.06f, 250.0f, &matrix);
  vg_lite_scale(1.0835f, 10.0f, &matrix);
  vg_lite_blit(rt, &pGradient.image, &matrix, VG_LITE_BLEND_NONE, 0, VG_LITE_FILTER_POINT);
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
