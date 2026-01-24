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
    STATE_HIGHSCORES
} SystemState;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define FLASH_ACCOUNTS_PAGE      511
#define FLASH_ACCOUNTS_ADDR      0x080FF800U
#define FLASH_ACCOUNTS_NBPAGES      1
#define CMD_BUF_SIZE            40
#define MAX_ACCOUNTS             3
#define ACCOUNT_SIZE             sizeof(account_t)
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
typedef struct {
    int32_t credit;           // 4 bytes
    char    username[16];     // 16 bytes
    uint32_t crc;             // optional safety
} account_t;

account_t accounts[MAX_ACCOUNTS];
int active_user = -1;

//BAZA DANYCH (Wspólna)
volatile int credits = 100;
char user_name[16] = "";

//ZMIENNE GRY
int symbols_credits[7] = {1000,500,20,300,250,20,100};
int reel_state[3] = {0, 0, 0};
volatile uint8_t game_active = 0;
uint32_t start_time = 0;
uint32_t duration = 0;
volatile uint8_t no_money = 0;
int speed = 4;

//ZMIENNE SYSTEMOWE
volatile SystemState currentState = STATE_MENU;
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
bool CheckCommand(const char* cmd);
void DisplayTerminalPrompt(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
int __io_putchar(int ch) {
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
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
bool Flash_SaveAccounts(void)
{
    HAL_FLASH_Unlock();

    FLASH_EraseInitTypeDef erase = {
        .TypeErase = FLASH_TYPEERASE_PAGES,
        .Banks     = FLASH_BANK_2,
        .Page      = FLASH_ACCOUNTS_PAGE,
        .NbPages   = 1
    };

    uint32_t error;
    if (HAL_FLASHEx_Erase(&erase, &error) != HAL_OK)
        return false;

    uint64_t *src = (uint64_t *)accounts;
    uint32_t addr = FLASH_ACCOUNTS_ADDR;

    for (int i = 0; i < (sizeof(accounts) / 8); i++)
    {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr, src[i]) != HAL_OK)
            return false;

        addr += 8;
    }

    HAL_FLASH_Lock();
    return true;
}

bool Account_Create(const char *name)
{
    for (int i = 0; i < MAX_ACCOUNTS; i++)
    {
        if (accounts[i].username[0] == '\0')
        {
            strncpy(accounts[i].username, name, 15);
            accounts[i].credit = 100;
            return Flash_SaveAccounts();
        }
    }
    return false; // full
}

int Account_Login(const char *name)
{
    for (int i = 0; i < MAX_ACCOUNTS; i++)
    {
        if (strcmp(accounts[i].username, name) == 0)
            return i;
    }
    return -1;
}

static void SaveActiveUser(void)
{
    if (active_user >= 0)
    {
        accounts[active_user].credit = credits;
        Flash_SaveAccounts();
    }
}

static bool IsLoggedIn(void)
{
    if (active_user < 0)
    {
        printf("Not logged in. Use: login <name>\r\n");
        return false;
    }
    return true;
}

void Flash_LoadAccounts(void)
{
    const account_t *flash = (const account_t *)FLASH_ACCOUNTS_ADDR;
    for (int i = 0; i < MAX_ACCOUNTS; i++)
    {
    	if ((uint32_t)flash[i].credit == 0xFFFFFFFF)
    	{
    	    accounts[i].credit = 0;
    	    accounts[i].username[0] = '\0';
    	}
    	else
    	{
    	    accounts[i] = flash[i];
    	}
    }
}

void DisplayTerminalPrompt(void)
{
    printf("𝓬𝓪𝓼𝓲𝓷𝓸@𝓒𝓪𝓼𝓲𝓷𝓸𝓞𝓢:~$ ");
    fflush(stdout);
}



//Parser komend
bool CheckCommand(const char* cmd)
{
    /* ---------------- HELP ---------------- */
    if(strcmp(cmd, "help") == 0)
    {
        printf(
            "Commands:\r\n"
            " help\r\n"
            " users               - list accounts\r\n"
            " create <name>       - create account\r\n"
            " login <name>        - login\r\n"
            " logout              - logout\r\n"
            " whoami              - show user\r\n"
            " balance             - show credits\r\n"
            " deposit <num>       - add credits\r\n"
            " withdraw <num>      - remove credits\r\n"
        );
        return true;
    }
    /* ---------------- USERS LIST ---------------- */
    if(strcmp(cmd, "users") == 0)
    {
        for(uint32_t i = 0; i < MAX_ACCOUNTS; i++)
        {
            if(accounts[i].username[0])
                printf("%ld. %s (%ld)\r\n", i+1, accounts[i].username, accounts[i].credit);
        }
        return true;
    }
    /* ---------------- CREATE ---------------- */
    if(strncmp(cmd, "create ", 7) == 0)
    {
        const char *name = cmd + 7;
        if(strlen(name) == 0 || strlen(name) >= 16)
        {
            printf("Invalid username\r\n");
            return true;
        }
        if(Account_Create(name))
            printf("Account created: %s\r\n", name);
        else
            printf("Account list full\r\n");

        return true;
    }
    /* ---------------- LOGIN ---------------- */
    if(strncmp(cmd, "login ", 6) == 0)
    {
        const char *name = cmd + 6;
        int idx = Account_Login(name);

        if(idx < 0)
        {
            printf("User not found\r\n");
            return true;
        }

        active_user = idx;
        credits = accounts[idx].credit;
        strncpy(user_name, accounts[idx].username, 16);

        printf("Logged in as %s\r\n", user_name);
        return true;
    }

    /* ---------------- LOGOUT ---------------- */
    if(strcmp(cmd, "logout") == 0)
    {
        if(IsLoggedIn())
        {
            SaveActiveUser();
            printf("Logged out: %s\r\n", user_name);
            active_user = -1;
            credits = 0;
            strcpy(user_name, "guest");
        }
        return true;
    }

    /* ---------------- WHOAMI ---------------- */
    if(strcmp(cmd, "whoami") == 0)
    {
        if(active_user >= 0)
            printf("%s\r\n", user_name);
        else
            printf("guest\r\n");
        return true;
    }

    /* ---------------- BALANCE ---------------- */
    if(strcmp(cmd, "balance") == 0)
    {
        if(IsLoggedIn())
            printf("Balance: %d\r\n", credits);
        return true;
    }

    /* ---------------- DEPOSIT ---------------- */
    if(strncmp(cmd, "deposit ", 8) == 0)
    {
        if(!IsLoggedIn()) return true;

        int amount = atoi(cmd + 8);
        if(amount <= 0)
        {
            printf("Invalid amount\r\n");
            return true;
        }
        credits += amount;
        printf("Deposited %d\r\n", amount);
        SaveActiveUser();
        return true;
    }

    /* ---------------- WITHDRAW ---------------- */
    if(strncmp(cmd, "withdraw ", 9) == 0)
    {
        if(!IsLoggedIn()) return true;

        int amount = atoi(cmd + 9);
        if(amount <= 0 || amount > credits)
        {
            printf("Invalid amount\r\n");
            return true;
        }

        credits -= amount;
        printf("Withdrawn %d\r\n", amount);
        SaveActiveUser();
        return true;
    }

    /* ---------------- UNKNOWN ---------------- */
    printf("Command not found: '%s'\r\n", cmd);
    return false;
}


// Funkcja do zaznaczenia wybranej opcji w menu
void DrawMenuLine(int y, int index, char* text){
	if(menu_position == index){
		ssd1306_FillRectangle(0, 1, 128, 10, White);
		ssd1306_SetCursor(20, 2);
		ssd1306_WriteString("LITTLE LAS VEGAS", Font_6x8, Black);

		ssd1306_SetCursor(10, y);

		ssd1306_WriteString(text, Font_6x8, Black);
	}else{
		ssd1306_SetCursor(10, y);

		ssd1306_WriteString(text, Font_6x8, White);
	}
}

//Obsługa przerwań
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin){
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

        if(currentState == STATE_GAME && game_active == 0){
            if(credits > 0){
                credits--;

                if(active_user >= 0) {
                	SaveActiveUser();
                }

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

static uint8_t esc_ignore = 0;

//PRZERWANIE OD UART (Sterowanie)
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        char c = (char)rx_data[0];

        if (esc_ignore) {
            if (c >= 'A' && c <= 'Z') esc_ignore = 0;
            HAL_UART_Receive_IT(&huart2, rx_data, 1);
            return;
        }
        if (c == 0x1B) {
            esc_ignore = 1;
            HAL_UART_Receive_IT(&huart2, rx_data, 1);
            return;
        }

        uint8_t is_nav_command = 0;

        // 1. STEROWANIE MENU
        if (cmd_index == 0) {
            if (c == 'q') {
                currentState = STATE_MENU;
                game_active = 0;
                is_nav_command = 1;
            }
            else if (currentState == STATE_MENU) {
                if (c == 'a') {
                    menu_position--;
                    if(menu_position < 0) menu_position = 3;
                    is_nav_command = 1;
                }
                else if (c == 'd') {
                    menu_position++;
                    if(menu_position > 3) menu_position = 0;
                    is_nav_command = 1;
                }
                else if (c == 'e' || c == '\r') {
                    switch(menu_position) {
                        case 0: currentState = STATE_GAME; break;
                        case 1: currentState = STATE_AUTHORS; break;
                        case 2: currentState = STATE_DESC; break;
                        case 3: currentState = STATE_HIGHSCORES; break;
                    }
                    is_nav_command = 1;
                }
            }
        }

        // 2. Terminal
        if (is_nav_command == 0)
        {
            // ENTER
            if (c == '\r' || c == '\n') {
                HAL_UART_Transmit(&huart2, (uint8_t*)"\r\n", 2, 10);

                cmd_buffer[cmd_index] = '\0';
                if (cmd_index > 0) {
                    CheckCommand(cmd_buffer);
                    DisplayTerminalPrompt();
                } else {
                    DisplayTerminalPrompt();
                }
                cmd_index = 0;
            }
            else if (c == 0x08 || c == 127) {
                if (cmd_index > 0) {
                    cmd_index--;
                    uint8_t erase_seq[] = {0x08, ' ', 0x08};
                    HAL_UART_Transmit(&huart2, erase_seq, 3, 10);
                }
            }
            else {
                if (cmd_index < CMD_BUF_SIZE) {
                    cmd_buffer[cmd_index++] = c;
                    HAL_UART_Transmit(&huart2, (uint8_t*)&c, 1, 10);
                }
            }
        }
        HAL_UART_Receive_IT(&huart2, rx_data, 1);
    }
}

// Funkcja do odwracania kolorów (Inwersja)
void SetDisplayInverse(uint8_t inverse) {
    if (inverse) {
        ssd1306_WriteCommand(0xA7);
    } else {
        ssd1306_WriteCommand(0xA6);
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
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_I2C1_Init();
  MX_SPI1_Init();
  /* USER CODE BEGIN 2 */
  setvbuf(stdout, NULL, _IONBF, 0);

    char buffor[40];

    ssd1306_Init();
    ssd1306_Fill(Black);
    ssd1306_UpdateScreen();

    // Wczytanie stanu konta
    Flash_LoadAccounts();
    credits = 0;
    strcpy(user_name, "guest");

    HAL_UART_Transmit(&huart2, (uint8_t*)"\r\nSYSTEM START\r\n", 14, 100);

    // POWITANIE W TERMINALU
    printf("\033[2J\033[H"); // Clear screen
    printf("                     CasinoOS Terminal v1.0.7 \r\n");
    printf("            In luck we trust, in probability we pray \r\n");
    printf("------------------------------------------------------- \r\n");
    printf("System status  : ONLINE \r\n");
    printf("RNG engine     : SEEDED \r\n");
    printf("Credit balance : %06d \r\n", credits);
    printf("User access    : %s \r\n", user_name);
    printf("------------------------------------------------------- \r\n");
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
	  	  	  	                DrawMenuLine(16,0,"1. Game");
	  	  	  	                DrawMenuLine(27,1,"2. Authors");
	  	  	  	                DrawMenuLine(38, 2,"3. About game");
	  	  	  	                DrawMenuLine(49,3,"4. Accounts");

	  	  	  	                ssd1306_DrawRectangle(0, 13, 127, 63, White);
	  	  	  	                break;

	  	  	  	            case STATE_GAME:
	  	  	  	                ssd1306_SetCursor(0, 0);

	  	  	  	                if(credits > 99999){
	  	  	  	                	sprintf(buffor, "Credits:%dk", credits / 1000);
	  	  	  	          	  	} else {
	  	  	  	          	  		sprintf(buffor, "Credits:%d", credits);
	  	  	  	          	  	}

	  	  	  	                ssd1306_WriteString(buffor, Font_6x8, White);

	  	  	  	                ssd1306_SetCursor(80, 0);
	  	  	  	                ssd1306_WriteString("[q]Exit", Font_6x8, White);

	  	  	  	                for(int x=0; x < 128; x++){ ssd1306_DrawPixel(x, 15, White); }
	  	  	  	                for(int x=0; x < 128; x++){ ssd1306_DrawPixel(x, 48, White); }
	  	  	  	                ssd1306_SetCursor(0, 27);   ssd1306_WriteString(">", Font_7x10, White);
	  	  	  	                ssd1306_SetCursor(120, 27); ssd1306_WriteString("<", Font_7x10, White);

	  	  	  	                for(int i=0; i < 3; i++){
	  	  	  	                    if(reel_state[i] == 1){
	  	  	  	                        uint32_t extra_delay = i * 1000;
	  	  	  	                        if(HAL_GetTick() - start_time > (duration + extra_delay)){
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
	  	  	  	                    int current_symbol_y = reel[i].pixel_offset + 16;
	  	  	  	                    int next_symbol_y = current_symbol_y - 32;
	  	  	  	                    ssd1306_DrawBitmap(reel[i].x_pos, current_symbol_y, epd_bitmap_allArray[reel[i].current_symbol], 32, 32, White);
	  	  	  	                    ssd1306_DrawBitmap(reel[i].x_pos, next_symbol_y, epd_bitmap_allArray[reel[i].next_symbol], 32, 32, White);
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
	  	  	  	                    	if(active_user >= 0) {
	  	  	  	                    		SaveActiveUser();
	  	  	  	                  	  	}

	  	  	  	                        for(int k=0; k<3; k++){
	  	  	  	                            SetDisplayInverse(1);
	  	  	  	                            HAL_Delay(100);
	  	  	  	                            SetDisplayInverse(0);
	  	  	  	                            HAL_Delay(100);
	  	  	  	                        }

	  	  	  	                        ssd1306_FillRectangle(14, 15, 100, 40, White);


	  	  	  	                        ssd1306_DrawRectangle(16, 17, 96, 36, Black);

	  	  	  	                        ssd1306_SetCursor(38, 20);
	  	  	  	                        ssd1306_WriteString("BIG WIN!", Font_6x8, Black);

	  	  	  	                        sprintf(buffor, "%d", win);

	  	  	  	                        int text_width = (win > 999) ? 44 : (win > 99) ? 33 : 22;
	  	  	  	                        int x_pos = 64 - (text_width / 2);

	  	  	  	                        ssd1306_SetCursor(x_pos, 32);
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
	  	  	  	                    ssd1306_SetCursor(25, 20);
	  	  	  	                    ssd1306_WriteString("OUT OF MONEY!", Font_7x10, Black);
	  	  	  	                    ssd1306_SetCursor(20, 35);
	  	  	  	                    ssd1306_WriteString("Deposit money :)", Font_6x8, Black);
	  	  	  	                    ssd1306_UpdateScreen();
	  	  	  	                    HAL_Delay(2500);
	  	  	  	                    no_money = 0;
	  	  	  	                }
	  	  	  	                break;

	  	  	  	            case STATE_AUTHORS:
	  	  	  	                ssd1306_SetCursor(0, 0); ssd1306_WriteString("Authors:", Font_7x10, White);
	  	  	  	                ssd1306_SetCursor(0, 20); ssd1306_WriteString("- Jakub Matusiewicz", Font_6x8, White);
	  	  	  	                ssd1306_SetCursor(0, 30); ssd1306_WriteString("- Mateusz Treda", Font_6x8, White);
	  	  	  	                ssd1306_SetCursor(2, 55);
	  	  	  	          	  	ssd1306_WriteString("[q] Back to menu", Font_6x8, White);
	  	  	  	                break;

	  	  	  	            case STATE_DESC:
	  	  	  	      				ssd1306_FillRectangle(0, 0, 128, 12, White);
	  	  	  	      				ssd1306_SetCursor(35, 2);
	  	  	  	      				ssd1306_WriteString("GAME RULES:", Font_6x8, Black);

	  	  	  	      				ssd1306_SetCursor(2, 16);
	  	  	  	      				ssd1306_WriteString("Press BLUE to SPIN", Font_6x8, White);

	  	  	  	      				ssd1306_SetCursor(2, 30);
	  	  	  	      				ssd1306_WriteString("3 symbols = MAX WIN", Font_6x8, White);

	  	  	  	      				ssd1306_SetCursor(2, 42);
	  	  	  	      				ssd1306_WriteString("2 symbols = 33% WIN", Font_6x8, White);

	  	  	  	      				ssd1306_FillRectangle(80, 54, 128, 64, Black);
	  	  	  	      				ssd1306_SetCursor(2, 55);
	  	  	  	      				ssd1306_WriteString("[q] Back to menu", Font_6x8, White);
	  	  	  	      				break;

	  	  	  	            case STATE_HIGHSCORES:
	  	  	  	      	  	  	  	ssd1306_FillRectangle(0, 0, 128, 12, White);
	  	  	  	      	  	  	  	ssd1306_SetCursor(40, 2);
	  	  	  	      	  	  	  	ssd1306_WriteString("ACCOUNTS", Font_6x8, Black);

	  	  	  	      	  	  	  	int y_pos = 20;

	  	  	  	      	  	  	  	for(int i = 0; i < MAX_ACCOUNTS; i++) {
	  	  	  	      	  	  	  	    if(accounts[i].username[0] != '\0') {
	  	  	  	      	  	  	  	       sprintf(buffor, "%d. %s - %ld", i+1, accounts[i].username, accounts[i].credit);

	  	  	  	      	  	  	  	       ssd1306_SetCursor(0, y_pos);
	  	  	  	      	  	  	  	       ssd1306_WriteString(buffor, Font_6x8, White);

	  	  	  	      	  	  	  	       y_pos += 10;
	  	  	  	      	  	  	  	       }
	  	  	  	      	  	  	  	    else {
	  	  	  	      	  	  	  	        sprintf(buffor, "%d. [Empty]", i+1);
	  	  	  	      	  	  	  	        ssd1306_SetCursor(0, y_pos);
	  	  	  	      	  	  	  	        ssd1306_WriteString(buffor, Font_6x8, White);
	  	  	  	      	  	  	  	        y_pos += 10;
	  	  	  	      	  	  	  	       }
	  	  	  	      	  	  	  	 }
	  	  	  	      	  	  	  	 ssd1306_FillRectangle(80, 54, 128, 64, Black);
	  	  	  	      	  	  	  	 ssd1306_SetCursor(2, 55);
	  	  	  	      	  		  	 ssd1306_WriteString("[q] Back to menu", Font_6x8, White);
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
