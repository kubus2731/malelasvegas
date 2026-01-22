/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include "stdio.h"
#include "stdlib.h"
#include "stdbool.h"
#include "string.h"
#include "symbols.h"
#include <ctype.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum {
    STATE_MENU,
    STATE_GAME,
    STATE_AUTHORS,
    STATE_DESC,
    STATE_HIGHSCORES,
	STATE_NONE
} SystemState;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define MAX_USERS               8
#define USERNAME_LEN            16
#define FLASH_USERS_ADDR        0x080E0000U
#define FLASH_USER_START_PAGE   511
#define FLASH_USER_NBPAGES      1
#define FLASH_USER_START_ADDR   0x080FF800U
#define CMD_BUF_SIZE            40
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart2;

//@@
static void MX_USART2_UART_Init(void);

#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#define GETCHAR_PROTOTYPE int __io_getchar(void)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#define GETCHAR_PROTOTYPE int fgetc(FILE *f)
#endif

PUTCHAR_PROTOTYPE
{
	HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
	return ch;
}

GETCHAR_PROTOTYPE
{
	uint8_t ch = 0;
	__HAL_UART_CLEAR_OREFLAG(&huart2);
	HAL_UART_Receive(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
	HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
	return ch;
}

#define MAX_USERS               8
#define USERNAME_LEN            16
#define FLASH_USERS_ADDR        0x080E0000U
#define FLASH_USER_START_PAGE   511
#define FLASH_USER_NBPAGES      1
#define FLASH_USER_START_ADDR   0x080FF800U
#define CMD_BUF_SIZE            40

char cmd_buffer[CMD_BUF_SIZE + 1];
char* user[] = {"defUser"};
unsigned int credit_balance = 0;
unsigned short tab[] = {0,1,2,3,4};//zapisac tabele do flash memeory
//@@
/* USER CODE BEGIN PV */

//BAZA DANYCH (Wspólna)
volatile int credits = 100;
char user_name[16] = "defUser";

//ZMIENNE GRY
int symbols_credits[7] = {1000,500,20,300,250,20,100};
int reel_state[3] = {0, 0, 0};
volatile uint8_t game_active = 0;
uint32_t start_time = 0;
uint32_t duration = 0;
volatile uint8_t no_money = 0;
int speed = 4;

//ZMIENNE SYSTEMOWE
volatile SystemState currentState = STATE_NONE;
volatile int menu_position = 0;

//ZMIENNE TERMINALA
uint8_t rx_data[1];
char cmd_buffer[CMD_BUF_SIZE + 1];
volatile uint8_t cmd_index = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_SPI1_Init(void);
/* USER CODE BEGIN PFP */
bool Flash_WriteBalance(uint32_t balance);
uint32_t Flash_ReadBalance(void);
bool CheckCommand(const char* cmd);
void DisplayTerminalPrompt(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
typedef struct {
    int obecny_symbol; // Indeks symbolu, który jest teraz na środku (0-6)
    int nas_symbol;    // Indeks symbolu, który wjeżdża z góry (0-6)
    int pixel_offset;   // Przesunięcie w pikselach (od 0 do 32)
    int x_poz;          // Pozycja X tego bębna na ekranie (8, 48 lub 88)
} bebenki;

// Inicjalizacja trzech bębnów
bebenki beben[3] = {
    {0, 1, 0, 8},  // Bęben 1
    {0, 1, 0, 48}, // Bęben 2
    {0, 1, 0, 88}  // Bęben 3
};

int fputc(int ch, FILE *f) {
    return __io_putchar(ch);
}
typedef struct {
    int current_symbol; // Indeks symbolu, który jest teraz na środku (0-6)
    int next_symbol;    // Indeks symbolu, który wjeżdża z góry (0-6)
    int pixel_offset;   // Przesunięcie w pikselach (od 0 do 32)
    int x_pos;          // Pozycja X tego bębna na ekranie (8, 48 lub 88)
} reels;

// Inicjalizacja trzech bębnów
reels reel[3] = {
    {0, 1, 0, 8},  // Bęben 1
    {0, 1, 0, 48}, // Bęben 2
    {0, 1, 0, 88}  // Bęben 3
};

//Obsługa pamięci flash
bool Flash_WriteBalance(uint32_t balance)
{
    HAL_FLASH_Unlock();

    // Wymaż stronę
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t PageError = 0;

    EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
    EraseInitStruct.Banks     = FLASH_BANK_2;
    EraseInitStruct.Page      = FLASH_USER_START_PAGE;
    EraseInitStruct.NbPages   = FLASH_USER_NBPAGES;

    if(HAL_FLASHEx_Erase(&EraseInitStruct, &PageError) != HAL_OK)
    {
        HAL_FLASH_Lock();
        return false;
    }
    // Flash wymaga zapisu double word = 64-bit
    uint64_t data64 = (uint64_t)balance; // dolne 32-bit = saldo, górne 32-bit = 0
    if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, FLASH_USER_START_ADDR, data64) != HAL_OK)
    {
        HAL_FLASH_Lock();
        return false;
    }
    HAL_FLASH_Lock();
    return true;
}

void DisplayTerminalPrompt(void)
{
    printf("𝓬𝓪𝓼𝓲𝓷𝓸@𝓒𝓪𝓼𝓲𝓷𝓸𝓞𝓢:~$ ");
    fflush(stdout);
}

//Parser komend
bool CheckCommand(const char* cmd)
{
    if(strcmp(cmd, "help") == 0)
    {
        printf("Usefull commands:\r\n");
        printf("\r\n");
        printf("help - print a list of commands used on software\r\n");
        printf("balance - prints an actual cash stored in Flash memory\r\n");
        printf("deposit <num> - deposits an number of cash to user\r\n");
        printf("withdraw <num> - withdraw an number of cash from user\r\n");
        printf("whoami - prints an user name\r\n");
        printf("setuser <name> - change and save username\r\n");
        return true;
    }

    if(strcmp(cmd, "balance") == 0)
    {
        printf("User cash balance: %d\r\n", credits);
        return true;
    }

    if(strncmp(cmd, "deposit ", 8) == 0)
    {
        const char* num_str = cmd + 8;
        int amount = atoi(num_str);
        if(amount > 0)
        {
            credits += amount;
            if (Flash_WriteBalance(credits))
            {
                printf("Deposited: %d\r\n", amount);
            }
            else
            {
                printf("Flash write error!\r\n");
            }
        }
        else
        {
            printf("Invalid value: '%s'\r\n", num_str);
        }
        return true;
    }

    if(strncmp(cmd, "withdraw", 8) == 0)
        {
            const char* num_str = cmd + 8;
            int amount = atoi(num_str);
            if(amount > 0)
            {
                credits -= amount;
                if (Flash_WriteBalance(credits))
                {
                    printf("Withdrawed: %d\r\n", amount);
                }
                else
                {
                    printf("Flash write error!\r\n");
                }
            }
            else
            {
                printf("Invalid value: '%s'\r\n", num_str);
            }
            return true;
        }

    if(strcmp(cmd, "whoami") == 0)
    {
        printf("%s\r\n", user_name);
        return true;
    }

    if (strncmp(cmd, "setuser ", 8) == 0)
    {
        const char* name_ptr = cmd + 8;
        if(strlen(name_ptr) > 0 && strlen(name_ptr) < 15) {

        	strncpy(user_name, name_ptr, USERNAME_LEN - 1);

        	user_name[USERNAME_LEN - 1] = '\0';
            // Tutaj symulujemy zapis nazwy (sukces)
            printf("Username saved: %s\r\n", user_name);
        } else {
            printf("Invalid username!\r\n");
        }
        return true;
    }

    printf("command not found : '%s'\r\n", cmd);
    return false;
}

// Funkcja do zaznaczenia wybranej opcji w menu
void DrawMenuLine(int y, int index, char* text){
	if(menu_position == index){
		ssd1306_FillRectangle(0, y - 1, 128, 11, White);

		ssd1306_SetCursor(10, y);

		ssd1306_WriteString(text, Font_6x8, Black);
	}else{
		ssd1306_SetCursor(10, y);

		ssd1306_WriteString(text, Font_6x8, White);
	}
}

//Obsługa przerwań
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	//printf("debug1");fflush(stdout);
	//return;
	// //////////////////////
    if(GPIO_Pin == B1_Pin)
    {
        static uint32_t last_click = 0;

        static uint8_t rng_initialized = 0;

        if(HAL_GetTick() - last_click < 500) return;
        last_click = HAL_GetTick();

        if(rng_initialized == 0) {
            srand(HAL_GetTick());
            rng_initialized = 1;
        }

        if(game_active == 0){
            if(credits > 0){
                credits--;
                no_money = 0;
                game_active = 1;

                for(int i=0; i<3; i++){
                    reel_state[i] = 1;
                }

                start_time = HAL_GetTick();

                duration = (rand() % 4000) + 3000;
            } else {
                no_money = 1;
            }
        }
    }
}

//PRZERWANIE OD UART (Sterowanie)
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	//each time i press the button+=_++++
    if (huart->Instance == USART2)
    {
        char c = (char)rx_data[0];
        uint8_t is_nav_command = 0; // Flaga: czy to był klawisz sterujący?
        // 1. STEROWANIE MENU (tylko gdy nie wpisujemy już komendy)
        if (cmd_index == 0)
        {
            if (c == 'q')
            {
                currentState = STATE_MENU;
                game_active = 0;
                is_nav_command = 1; // Zaznaczamy, że obsłużono jako nawigację
            }
            else if (currentState == STATE_MENU)
            {
                if (c == 'a')
                {
                    menu_position--;
                    if(menu_position < 0) menu_position = 3;
                    is_nav_command = 1;
                }
                else if (c == 'd')
                {
                    menu_position++;
                    if(menu_position > 3) menu_position = 0;
                    is_nav_command = 1;
                }
                else if (c == 'e')
                {
                    switch(menu_position)
                    {
                        case 0: currentState = STATE_GAME; break;
                        case 1: currentState = STATE_AUTHORS; break;
                        case 2: currentState = STATE_DESC; break;
                        case 3: currentState = STATE_HIGHSCORES; break;
                    }
                    is_nav_command = 1;
                }
            }
        }
        // 2. TERMINAL (Wykonujemy TYLKO jeśli to nie była nawigacja)
        if (is_nav_command == 0)
        {
           // HAL_UART_Transmit(&huart2, &rx_data[0], 1, 10); // Echo znaku//@@
            if (c == '\r' || c == '\n')//@@
            {
                printf("\r\n"); // Nowa linia
                cmd_buffer[cmd_index] = '\0';
                if (cmd_index > 0)
                {
                    CheckCommand(cmd_buffer);
                    DisplayTerminalPrompt();
                }
                else
                {
                    DisplayTerminalPrompt();
                }
                cmd_index = 0; // Reset bufora po Enterze
            }

            else if (c == 0x08 || c == 127)//@@
            { // Backspace
                if (cmd_index > 0)
                {
                	cmd_index--;
                	cmd_buffer[cmd_index] = '\0';
                	printf("\b \b");
                	fflush(stdout);
                }
            }
            else//@@
            {
                if (cmd_index < CMD_BUF_SIZE)//@@
                {
                	cmd_buffer[cmd_index++] = c;
                	printf("%c", c);
                	fflush(stdout);
                }
            }
        }
        // Ważne: wznawiamy nasłuchiwanie
        HAL_UART_Receive_IT(&huart2, rx_data, 1);
    }
}


uint32_t Flash_ReadBalance(void)
{
    uint64_t value = *(uint64_t*)FLASH_USER_START_ADDR;
    if(value == 0xFFFFFFFFFFFFFFFF) return 0; // Flash nie była jeszcze zapisana
    return (uint32_t)(value & 0xFFFFFFFF);   // bierzemy dolne 32-bit
}

void GetCommand(void)
{
    uint8_t ch;
    cmd_index = 0;
    memset(cmd_buffer, 0, sizeof(cmd_buffer));
    while(1)
    {
        HAL_UART_Receive(&huart2, &ch, 1, HAL_MAX_DELAY);
        // ESC sequence (strzałki)
        if(ch == 27) // ESC
        {
        	uint8_t seq[2];
            HAL_UART_Receive(&huart2, &seq[0], 1, HAL_MAX_DELAY);
            HAL_UART_Receive(&huart2, &seq[1], 1, HAL_MAX_DELAY);
            continue; // zignoruj escape
        }
        //enter
        if(ch == '\r' || ch == '\n')
        {
            printf("\r\n"); // przejście do nowej linii
            fflush(stdout);
            cmd_buffer[cmd_index] = '\0'; // zakończ string
            break;
        }
        else if(ch == 0x08 || ch == 127) // 0x08 = ASCII backspace, 127 = DEL
        {
            if(cmd_index > 0)
            {
                cmd_index--;
                cmd_buffer[cmd_index] = '\0';
                printf("\b \b");
                fflush(stdout);
            }
        }
        else
        {
            if(cmd_index < CMD_BUF_SIZE)
            {
                cmd_buffer[cmd_index++] = ch;
                printf("%c", ch);
                fflush(stdout);
            }
            else
            {
                //
            }
        }
    }
}

void DisplayTitle()
{
	printf("                     CasinoOS Terminal v1.0.7 \r\n");
	printf("            In luck we trust, in probability we pray \r\n");
}
void DisplaySeparationBorder()
{
	printf("------------------------------------------------------- \r\n");
}

//@@
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
  //@@
  HAL_Init();
  setvbuf(stdin, NULL ,_IONBF, 0);
  //@@
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_I2C1_Init();
  MX_SPI1_Init();
  /* USER CODE BEGIN 2 */

  /*while(1)
      {
    	  DisplayRequireInput();
    	  GetCommand();
    	  if(CheckCommand(cmd_buffer))
    	  {
    	        //fflush(stdout);
    	  }
    	  else
    	  {
    	        // komenda niepoprawna – już wypisane w CheckCommand
    	  }
      }
  //@@*/
  setvbuf(stdout, NULL, _IONBF, 0);

    char buffor[40];

    ssd1306_Init();
    ssd1306_Fill(Black);
    ssd1306_UpdateScreen();

    // Wczytanie stanu konta
    credits = Flash_ReadBalance();

    // 2. WYMUSZENIE PRIORYTETU PRZERWANIA
    HAL_NVIC_SetPriority(USART2_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);

    // 3. Test bezpośredni
    HAL_UART_Transmit(&huart2, (uint8_t*)"\r\nSYSTEM START\r\n", 14, 100);

    // POWITANIE W TERMINALU
    printf("\033[2J\033[H"); // Clear screen
    DisplayTitle();
    DisplaySeparationBorder();
    printf("System status  : ONLINE \r\n");
    printf("RNG engine     : SEEDED \r\n");
    printf("Credit balance : %06d \r\n", credits);
    printf("User access    : %s \r\n", user_name);
    DisplaySeparationBorder();
    DisplayTerminalPrompt();




    HAL_UART_Receive_IT(&huart2, rx_data, 1);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
	  ssd1306_Fill(Black);
	  switch (currentState)
	  {
	  	  case STATE_MENU:
	  		  ssd1306_FillRectangle(0, 0, 128, 12, White);
	  		  ssd1306_SetCursor(20, 2);
	  		  ssd1306_WriteString("MALE LAS VEGAS", Font_6x8, Black);
	  	  	  DrawMenuLine(16,0,"1. Game");
	  	  	  DrawMenuLine(27,1,"2. Authors");
	  	  	  DrawMenuLine(38, 2,"3. About game");
	  	  	  DrawMenuLine(49,3,"4. Accounts");
	  	  	  ssd1306_DrawRectangle(0, 13, 127, 63, White);
	  	  	  break;
	  	  case STATE_GAME:
	  	  	  	                ssd1306_SetCursor(0, 0);
	  	  	  	                sprintf(buffor, "Credits: %d", credits);
	  	  	  	                ssd1306_WriteString(buffor, Font_6x8, White);

	  	  	  	                ssd1306_SetCursor(80, 0);
	  	  	  	                ssd1306_WriteString("[q]Exit", Font_6x8, White);

	  	  	  	                for(int x=0; x < 128; x++){ ssd1306_DrawPixel(x, 15, White); }
	  	  	  	                for(int x=0; x < 128; x++){ ssd1306_DrawPixel(x, 48, White); }
	  	  	  	                ssd1306_SetCursor(0, 27);   ssd1306_WriteString(">", Font_7x10, White);
	  	  	  	                ssd1306_SetCursor(120, 27); ssd1306_WriteString("<", Font_7x10, White);

	  	  	  	                for(int i=0; i < 3; i++){
	  	  	  	                    if(reel_state[i] == 1){
	  	  	  	                        uint32_t dodatkowe_opoznienie = i * 1000;
	  	  	  	                        if(HAL_GetTick() - start_time > (duration + dodatkowe_opoznienie)){
	  	  	  	                            if(reel[i].pixel_offset == 0){
	  	  	  	                                reel_state[i] = 0;
	  	  	  	                            }
	  	  	  	                        }
	  	  	  	                        if(reel_state[i] == 1) {
	  	  	  	                            reel[i].pixel_offset += speed;
	  	  	  	                            if(reel[i].pixel_offset >= 32){
	  	  	  	                                reel[i].pixel_offset = 0;
	  	  	  	                                reel[i].current_symbol = reel[i].next_symbol;
	  	  	  	                                reel[i].next_symbol = rand() % 7;
	  	  	  	                            }
	  	  	  	                        }
	  	  	  	                    }
	  	  	  	                    int symbol_obecny_y = reel[i].pixel_offset + 16;
	  	  	  	                    int symbol_nastepny_y = symbol_obecny_y - 32;
	  	  	  	                    ssd1306_DrawBitmap(reel[i].x_pos, symbol_obecny_y, epd_bitmap_allArray[reel[i].current_symbol], 32, 32, White);
	  	  	  	                    ssd1306_DrawBitmap(reel[i].x_pos, symbol_nastepny_y, epd_bitmap_allArray[reel[i].next_symbol], 32, 32, White);
	  	  	  	                }

	  	  	  	                if(game_active == 1 && reel_state[0] == 0 && reel_state[1] == 0 && reel_state[2] == 0 && no_money == 0){
	  	  	  	                    int s0 = reel[0].current_symbol;
	  	  	  	                    int s1 = reel[1].current_symbol;
	  	  	  	                    int s2 = reel[2].current_symbol;
	  	  	  	                    int win = 0;

	  	  	  	                    if(s0 == s1 && s1 == s2) win = symbols_credits[s0];
	  	  	  	                    else if(s0 == s1) win = (symbols_credits[s0])/3;
	  	  	  	                    else if(s1 == s2) win = (symbols_credits[s1])/3;
	  	  	  	                    else if(s0 == s2) win = (symbols_credits[s0])/3;

	  	  	  	                    if(win > 0){
	  	  	  	                        credits += win;
	  	  	  	                        Flash_WriteBalance(credits);
	  	  	  	                        ssd1306_FillRectangle(10, 18, 108, 48, White);
	  	  	  	                        sprintf(buffor,"WIN %d", win );
	  	  	  	                        int x_position = (win > 99) ? 25 : 35;
	  	  	  	                        ssd1306_SetCursor(x_position, 28);
	  	  	  	                        ssd1306_WriteString(buffor, Font_11x18, Black);
	  	  	  	                        ssd1306_UpdateScreen();
	  	  	  	                        HAL_Delay(2500);
	  	  	  	                    } else {
	  	  	  	                        HAL_Delay(500);
	  	  	  	                    }
	  	  	  	                    game_active = 0;
	  	  	  	                }
	  	  	  	                else if(no_money == 1){
	  	  	  	                    ssd1306_FillRectangle(10, 18, 108, 32, White);
	  	  	  	                    ssd1306_SetCursor(25, 22);
	  	  	  	                    ssd1306_WriteString("OUT OF MONEY!", Font_7x10, Black);
	  	  	  	                    ssd1306_SetCursor(20, 35);
	  	  	  	                    ssd1306_WriteString("Deposit money :)", Font_6x8, Black);
	  	  	  	                }
	  	  	  	                break;

	  	  	  	            case STATE_AUTHORS:
	  	  	  	                ssd1306_SetCursor(0, 0); ssd1306_WriteString("Authors:", Font_7x10, White);
	  	  	  	                ssd1306_SetCursor(0, 20); ssd1306_WriteString("- Jakub Matusiewicz", Font_6x8, White);
	  	  	  	                ssd1306_SetCursor(0, 30); ssd1306_WriteString("- Mateusz Treda", Font_6x8, White);
	  	  	  	                break;

	  	  	  	            case STATE_DESC:
	  	  	  	                ssd1306_SetCursor(0, 0); ssd1306_WriteString("Opis gry...", Font_6x8, White);
	  	  	  	                break;

	  	  	  	            case STATE_HIGHSCORES:
	  	  	  	                ssd1306_SetCursor(0, 0); ssd1306_WriteString("Accounts:", Font_7x10, White);

	  	  	  	                sprintf(buffor, "1. %s - %d", user_name, credits);
	  	  	  	                ssd1306_SetCursor(0, 20); ssd1306_WriteString(buffor, Font_6x8, White);

	  	  	  	                ssd1306_SetCursor(0, 30); ssd1306_WriteString("2. CASINO - 1 MLN", Font_6x8, White);
	  	  	  	                break;
	  	  	  	        }

	  	  	  	        ssd1306_UpdateScreen();
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

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 10;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
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
	GPIO_InitTypeDef GPIO_InitStruct;
	  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
	  /** Initializes the peripherals clock  */
	  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C1;
	  PeriphClkInit.Usart2ClockSelection = RCC_I2C1CLKSOURCE_PCLK1;
	  HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);

	  /* Enable Peripheral clock */
	  __HAL_RCC_I2C1_CLK_ENABLE();

		__HAL_RCC_GPIOB_CLK_ENABLE();
		/**USART2 GPIO Configuration
		PA2     ------> USART2_TX
		PA3     ------> USART2_RX
		*/
		GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_PULLUP;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
		GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
		HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x10D19CE4;
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
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

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
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PC7 */
  GPIO_InitStruct.Pin = GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PA9 */
  GPIO_InitStruct.Pin = GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PB6 */
  GPIO_InitStruct.Pin = GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

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
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
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
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
