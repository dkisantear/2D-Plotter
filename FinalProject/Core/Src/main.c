/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : STM32-based 2D plotter final project using stepper
  *                   drivers, an Ender 3 frame, limit switches, an LCD menu,
  *                   a rotary encoder, and hardcoded drawing functions.
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdint.h>
#include <math.h>
#include "i2c_lcd.h"
#include "stm32f3xx_hal.h"
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* Pin setup and motion calibration */
#define X_STEPS_PER_MM   7.3
#define Z_STEPS_PER_MM   7.4
#define Y_STEPS_PER_MM  10.0

/* Stepper pin aliases */
#define X_STEP_PORT     X_STEP_GPIO_Port
#define X_STEP_PIN      X_STEP_Pin
#define X_DIR_PORT      X_DIR_GPIO_Port
#define X_DIR_PIN       X_DIR_Pin

#define Z_STEP_PORT     Z_STEP_GPIO_Port
#define Z_STEP_PIN      Z_STEP_Pin
#define Z_DIR_PORT      Z_DIR_GPIO_Port
#define Z_DIR_PIN       Z_DIR_Pin

#define Y_STEP_PORT     Y_STEP_GPIO_Port
#define Y_STEP_PIN      Y_STEP_Pin
#define Y_DIR_PORT      Y_DIR_GPIO_Port
#define Y_DIR_PIN       Y_DIR_Pin

/* Limit switch pin aliases */
#define X_LIMIT_PORT    X_LIMIT_GPIO_Port
#define X_LIMIT_PIN     X_LIMIT_Pin

#define Z_LIMIT_PORT    Z_LIMIT_GPIO_Port
#define Z_LIMIT_PIN     Z_LIMIT_Pin

#define Y_LIMIT_PORT    Y_LIMIT_GPIO_Port
#define Y_LIMIT_PIN     Y_LIMIT_Pin

/* Limit switches use pull-ups, so a pressed switch reads LOW. */
#define LIMIT_TRIGGERED  GPIO_PIN_RESET

/* Tuned center position in mm after homing. */
#define X_CENTER_MM   96.0
#define Z_CENTER_MM   89.0

/* Line interpolation resolution for draw_vector(). */
#define GLOBAL_STEP_SIZE  100.0

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim2;

/* USER CODE BEGIN PV */

extern TIM_HandleTypeDef htim2;

uint32_t STEP_DELAY_MS  = 0;   /* 0 = full speed; set higher to slow for debug */

double residualX        = 0.0;
double residualZ        = 0.0;
double residualY        = 0.0;

double global_step_size = GLOBAL_STEP_SIZE;
double font_size        = 1.0;
double char_separation  = 3.0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
static void MX_I2C1_Init(void);
/* USER CODE BEGIN PFP */

void   pulse_step(GPIO_TypeDef *GPIOx, uint16_t stepPin);
void   move_motor_with_residue(GPIO_TypeDef *dirPort,  uint16_t dirPin,
                               GPIO_TypeDef *stepPort, uint16_t stepPin,
                               double mm, double steps_per_mm, double *residue);
void   move_x_mm(double mm);
void   move_z_mm(double mm);
void   move_y_mm(double mm);
void   draw_vector(double distance, double angle);
void   move_to_zero(void);
void   move_to_center(double x_center_mm, double z_center_mm);
void   move_to_zero_then_center(double x_center_mm, double z_center_mm);
void   draw_char(int char_value, double font_scale);

void pen_down(void);
void pen_up(void);
void draw_line(double length);
void square(double length);
void circle(double radius);
void triangle(double side);

void init_lcds(void);
void print_menu(uint32_t counter, char **menu);
void parse_input(uint32_t current_counter_value, uint32_t current_menu);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// LCD menu helpers

I2C_LCD_HandleTypeDef lcd1;
I2C_LCD_HandleTypeDef lcd2;

uint32_t main_menu = 0;
uint32_t print_shape_menu = 1;
uint32_t print_text_menu = 2;

char buf[16];
int menu_flag = 0;

char *menu[3] = {"Reset Printer", "Print Shape", "Print Char", };
char *shape_menu[4] = {"Square", "Circle", "Triangle", "Back"};
char *text_menu[4] = {"Text 1", "Text 2", "Text 3", "Back"};

void init_lcds(void) {
    lcd1.hi2c = &hi2c1;
    lcd1.address = 0x4E;
    lcd_init(&lcd1);
}

void print_menu(uint32_t counter, char **menu)
{
    sprintf(buf, "%s", menu[counter]);

    lcd_gotoxy(&lcd1, 0, 0);
    lcd_puts(&lcd1, buf);
}

void parse_input(uint32_t current_counter_value, uint32_t current_menu) {
    if (current_menu == main_menu)
    {
        switch (current_counter_value) {
            case 0:
                lcd_clear(&lcd1);
                lcd_gotoxy(&lcd1, 0, 0);
                lcd_puts(&lcd1, "Resetting...");
                move_to_zero_then_center(X_CENTER_MM, Z_CENTER_MM);
                break;

            case 1:
                lcd_clear(&lcd1);
                lcd_gotoxy(&lcd1, 0, 0);
                menu_flag = 1;
                break;

            case 2:
                lcd_clear(&lcd1);
                lcd_gotoxy(&lcd1, 0, 0);
                menu_flag = 2;
                break;
        }
        return;
    }
    else if (current_menu == print_shape_menu)
    {
        switch (current_counter_value)
        {
            case 0:
                lcd_clear(&lcd1);
                lcd_gotoxy(&lcd1, 0, 0);
                lcd_puts(&lcd1, "Printing...");
                square(10.0);
                menu_flag = 0;
                current_menu = 0;
                break;

            case 1:
                lcd_clear(&lcd1);
                lcd_gotoxy(&lcd1, 0, 0);
                lcd_puts(&lcd1, "Printing...");
                circle(10.0);
                menu_flag = 0;
                current_menu = 0;
                break;

            case 2:
                lcd_clear(&lcd1);
                lcd_gotoxy(&lcd1, 0, 0);
                lcd_puts(&lcd1, "Printing...");
                triangle(10.0);
                menu_flag = 0;
                current_menu = 0;
                break;
            case 3:
                lcd_clear(&lcd1);
                lcd_gotoxy(&lcd1, 0, 0);
                menu_flag = 0;
                break;
        }
        return;
    }
    else if (current_menu == print_text_menu)
    {
        switch (current_counter_value)
        {
            case 0:
                lcd_clear(&lcd1);
                lcd_gotoxy(&lcd1, 0, 0);
                lcd_puts(&lcd1, "Printing...");
                draw_char(0b1111111, 5);
                draw_char(0b1110011, 5);
                menu_flag = 0;
                current_menu = 0;
                break;

            case 1:
                lcd_clear(&lcd1);
                lcd_gotoxy(&lcd1, 0, 0);
                lcd_puts(&lcd1, "Printing...");
                HAL_Delay(2000);
                menu_flag = 0;
                current_menu = 0;
                break;

            case 2:
                lcd_clear(&lcd1);
                lcd_gotoxy(&lcd1, 0, 0);
                lcd_puts(&lcd1, "Printing...");
                HAL_Delay(2000);
                menu_flag = 0;
                current_menu = 0;
                break;
            case 3:
            	lcd_clear(&lcd1);
            	lcd_gotoxy(&lcd1, 0, 0);
            	menu_flag = 0;
            	break;
        }
        return;
    }
}

// Shape drawing

void pen_down(void) {
    move_y_mm(-12);
    HAL_Delay(200);
}

void pen_up(void) {
    move_y_mm(12);
    HAL_Delay(200);
}

void draw_line(double length) {
    pen_down();

    draw_vector(length, 0);

    pen_up();
}

void square(double side) {
    pen_down();

    draw_vector(side, 0);         // Right
    HAL_Delay(10);
    draw_vector(side, 90);         // Up
    HAL_Delay(10);
    draw_vector(side, 180);        // Left
    HAL_Delay(10);
    draw_vector(side, 270);        // Down

    pen_up();
}

void circle(double radius)
{
    double prev_x;
    double prev_z;

    move_x_mm(radius);
    pen_down();

    prev_x = radius;
    prev_z = 0.0;

    for (int angle = 10; angle <= 360; angle += 10)
    {
        double rad = angle * (3.14159265358979323846 / 180.0);

        double x = radius * cos(rad);
        double z = radius * sin(rad);

        double dx = x - prev_x;
        double dz = z - prev_z;

        double dist = sqrt((dx * dx) + (dz * dz));
        double seg_angle = atan2(dz, dx) * (180.0 / 3.14159265358979323846);

        draw_vector(dist, seg_angle);

        prev_x = x;
        prev_z = z;
    }

    pen_up();
    move_x_mm(-radius);
}

void triangle(double side)
{
    move_x_mm(-(side / 2.0));
    pen_down();

    draw_vector(side, 0);
    draw_vector(side, 120);
    draw_vector(side, 240);

    pen_up();

    move_x_mm(side / 2.0);
}
/* Motor movement */
void pulse_step(GPIO_TypeDef *GPIOx, uint16_t stepPin)
{
    HAL_GPIO_WritePin(GPIOx, stepPin, GPIO_PIN_SET);
    HAL_Delay(STEP_DELAY_MS);
    HAL_GPIO_WritePin(GPIOx, stepPin, GPIO_PIN_RESET);
    HAL_Delay(STEP_DELAY_MS);
}

/* Shared stepper move helper with residue tracking. */
void move_motor_with_residue(GPIO_TypeDef *dirPort,  uint16_t dirPin,
                             GPIO_TypeDef *stepPort, uint16_t stepPin,
                             double mm, double steps_per_mm, double *residue)
{
    double total   = (mm * steps_per_mm) + *residue;
    int    steps   = (int)fabs(total);

    *residue = total - (total > 0 ? steps : -steps);

    HAL_GPIO_WritePin(dirPort, dirPin,
                      (mm < 0) ? GPIO_PIN_RESET : GPIO_PIN_SET);

    for (int i = 0; i < steps; i++)
        pulse_step(stepPort, stepPin);
}

/* Per-axis move helpers */
void move_x_mm(double mm)
{
    move_motor_with_residue(X_DIR_PORT, X_DIR_PIN,
                            X_STEP_PORT, X_STEP_PIN,
                            mm, X_STEPS_PER_MM, &residualX);
}

void move_z_mm(double mm)
{
    move_motor_with_residue(Z_DIR_PORT, Z_DIR_PIN,
                            Z_STEP_PORT, Z_STEP_PIN,
                            mm, Z_STEPS_PER_MM, &residualZ);
}

void move_y_mm(double mm)
{
    move_motor_with_residue(Y_DIR_PORT, Y_DIR_PIN,
                            Y_STEP_PORT, Y_STEP_PIN,
                            mm, Y_STEPS_PER_MM, &residualY);
}

/* Break a line into smaller X/Z moves so diagonal motion stays smooth. */
void draw_vector(double distance, double angle)
{
    double rad      = (angle * 3.14159265358979323846) / 180.0;
    double totalX   = distance * cos(rad);
    double totalZ   = distance * sin(rad);
    double currentX = 0.0;
    double currentZ = 0.0;

    for (int i = 1; i <= (int)global_step_size; i++)
    {
        double nextX = (totalX * i) / global_step_size;
        double nextZ = (totalZ * i) / global_step_size;

        move_x_mm(nextX - currentX);
        move_z_mm(nextZ - currentZ);

        currentX = nextX;
        currentZ = nextZ;

        HAL_Delay(1);
    }
}

/* Homing and centering */
void move_to_zero(void)
{
    int state = 0;

    while (state != 3)
    {
        switch (state)
        {
            case 0:
                while (HAL_GPIO_ReadPin(X_LIMIT_PORT, X_LIMIT_PIN) != LIMIT_TRIGGERED)
                    move_x_mm(1);
                HAL_Delay(200);
                state = 1;
                break;

            case 1:
                while (HAL_GPIO_ReadPin(Z_LIMIT_PORT, Z_LIMIT_PIN) != LIMIT_TRIGGERED)
                    move_z_mm(1);
                HAL_Delay(200);
                state = 2;
                break;

            case 2:
                while (HAL_GPIO_ReadPin(Y_LIMIT_PORT, Y_LIMIT_PIN) != LIMIT_TRIGGERED)
                    move_y_mm(-1);
                HAL_Delay(200);
                move_y_mm(20);
                state = 3;
                break;

            default:
                state = 3;
                break;
        }
    }
}

/* Move from the switch position to the tuned drawing center. */
void move_to_center(double x_center_mm, double z_center_mm)
{
    move_x_mm(-x_center_mm);
    HAL_Delay(200);

    move_z_mm(-z_center_mm);
    HAL_Delay(200);



}

/* Full reset sequence used by the menu reset option. */
void move_to_zero_then_center(double x_center_mm, double z_center_mm)
{
    move_to_zero();
    HAL_Delay(500);

    move_to_center(x_center_mm, z_center_mm);
    HAL_Delay(500);
}

/* Hardcoded 7-segment style character drawing. */
void draw_char(int char_value, double font_scale)
{
    for (int i = 0; i < 7; i++)
    {
        int seg = (char_value >> i) & 0x01;

        if (i == 0)
        {
            if (seg) { move_y_mm(-12); draw_vector(font_size * font_scale,   0); move_y_mm(12); }
            else     {                 draw_vector(font_size * font_scale,   0);                }
        }
        if (i == 1)
        {
            if (seg) { move_y_mm(-12); draw_vector(font_size * font_scale,  90); move_y_mm(12); }
            else     {                 draw_vector(font_size * font_scale,  90);                }
        }
        if (i == 2)
        {
            if (seg) { move_y_mm(-12); draw_vector(font_size * font_scale,  90); move_y_mm(12); }
            else     {                 draw_vector(font_size * font_scale,  90);                }
        }
        if (i == 3)
        {
            if (seg) { move_y_mm(-12); draw_vector(font_size * font_scale, 180); move_y_mm(12); }
            else     {                 draw_vector(font_size * font_scale, 180);                }
        }
        if (i == 4)
        {
            if (seg) { move_y_mm(-12); draw_vector(font_size * font_scale, 270); move_y_mm(12); }
            else     {                 draw_vector(font_size * font_scale, 270);                }
        }
        if (i == 5)
        {
            if (seg) { move_y_mm(-12); draw_vector(font_size * font_scale, 270); move_y_mm(12); }
            else     {                 draw_vector(font_size * font_scale, 270);                }
        }
        if (i == 6)
        {
            if (seg)
            {
                draw_vector(-(font_size * font_scale), 270);
                move_y_mm(-12);
                draw_vector( (font_size * font_scale),   0);
                move_y_mm(12);
                draw_vector(-(font_size * font_scale),   0);
                draw_vector( (font_size * font_scale), 270);
                draw_vector( (font_size * font_scale) + char_separation, 0);
            }
            else
            {
                draw_vector(15, 0);
            }
        }
    }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_TIM2_Init();
  MX_I2C1_Init();
  MX_GPIO_Init();
  /* USER CODE BEGIN 2 */
  HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);
  /* Encoder and LCD startup */
  //move_to_zero_then_center(X_CENTER_MM, Z_CENTER_MM);

  init_lcds();
  HAL_Delay(200);

  lcd_clear(&lcd1);
  lcd_gotoxy(&lcd1, 0, 0);

  uint32_t current_count = 0;
  menu_flag = 0;

  /* USER CODE END 2 */


  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  uint32_t count = __HAL_TIM_GET_COUNTER(&htim2);

	  uint32_t counter = 0;

	  if(menu_flag == 0) {
		  counter = ((count / 4)) % 3;
	  }
	  else {
		  counter = ((count / 4)) % 4;
	  }

	  if (current_count != counter) {
	      lcd_clear(&lcd1);
	  }

	  current_count = counter;

	  switch (menu_flag) {
	      case 0:
	          print_menu(counter, menu);
	          break;

	      case 1:
	          print_menu(counter, shape_menu);
	          break;

	      case 2:
	          print_menu(counter, text_menu);
	          break;

	      default:
	          print_menu(counter, menu);
	          break;
	  }

	  if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4) == GPIO_PIN_RESET) {
	      parse_input(counter, menu_flag);
	      __HAL_TIM_SET_COUNTER(&htim2, 0);
	      count = 0;
	      counter = 0;
	      HAL_Delay(10);

	      while (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4) == GPIO_PIN_RESET) {
	          HAL_Delay(10);
	      }
	  }

	  HAL_Delay(10);
    /* Quick test blocks kept here for bench testing.
    move_x_mm(10);   HAL_Delay(1000);
    move_x_mm(-10);  HAL_Delay(1000);
    move_z_mm(10);   HAL_Delay(1000);
    move_z_mm(-10);  HAL_Delay(1000);
    move_y_mm(10);   HAL_Delay(1000);
    move_y_mm(-10);  HAL_Delay(2000);
	*/
    //move_y_mm(10);
    //draw_char(0b0111111, 5);
    //draw_char(0b0000110, 5);
    //draw_char(0b1011011, 5);
    //draw_char(0b1001111, 5);
    //draw_char(0b1100110, 5);
    //draw_char(0b1011001, 5);
    //draw_char(0b1111101, 5);
    //draw_char(0b0000111, 5);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C1;
  PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_HSI;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x00201D2B;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 65536;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  sConfig.EncoderMode = TIM_ENCODERMODE_TI12;
  sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter = 0;
  sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter = 0;
  if (HAL_TIM_Encoder_Init(&htim2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */
  /* GPIO pin setup for steppers, limits, and the encoder button */
  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* Default output levels */
  HAL_GPIO_WritePin(GPIOA, X_STEP_Pin|X_DIR_Pin|Z_STEP_Pin|Z_DIR_Pin
                          |Y_STEP_Pin, GPIO_PIN_RESET);

  HAL_GPIO_WritePin(Y_DIR_GPIO_Port, Y_DIR_Pin, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = X_STEP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = X_DIR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = Z_STEP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = Z_DIR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = Y_STEP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : Y_DIR_Pin */
  GPIO_InitStruct.Pin = Y_DIR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(Y_DIR_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : X_LIMIT_Pin Y_LIMIT_Pin */
  GPIO_InitStruct.Pin = X_LIMIT_Pin|Y_LIMIT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : Z_LIMIT_Pin Encoder_Button_Pin */
  GPIO_InitStruct.Pin = Z_LIMIT_Pin|Encoder_Button_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1) {}
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
