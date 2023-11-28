/**
 * @file    homeoffice.c
 * @brief   Homeoffice device communication
 * @details This file contains the SPI communication with the homeoffice device.
 *              It is possible to read the voltage, current and power, as well as
 *              the relay status. It is also possible to set the relay on or off.
 * @author  Klaus Becker (doklauss@gmail.com)
*/

/* ****************
 * INCLUDED FILES *
 * ****************/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <string.h>

/* *****************
 * PRIVATE DEFINES *
 * *****************/

#ifdef __DEBUG__
	#define printd(fmt, args...) { printf("%s:%d::%s: " fmt "\n", __FILE__, __LINE__, __FUNCTION__, ##args); fflush(stdout); }
#else
	#define printd(args...)
#endif

#define SPI_DEVICE "/dev/spidev0.0" /* SPI device path */
#define SPI_MODE SPI_MODE_0         /* SPI communication mode */
#define SPI_BITS_PER_WORD 8         /* SPI bits per word */
#define SPI_SPEED_HZ 100000           /* SPI speed in Hz */

#define CMD_READ_VOLTAGE 0x01       /* SPI Read Voltage command */
#define CMD_READ_CURRENT 0x02       /* SPI Read Current command */
#define CMD_READ_POWER 0x03         /* SPI Read Power command */
#define CMD_READ_RELAY 0x04         /* SPI Read Relay command */
#define CMD_READ_ALL 0x05           /* SPI Read All command */
#define CMD_SET_RELAY_ON 0x06       /* SPI Set Relay On command */
#define CMD_SET_RELAY_OFF 0x07      /* SPI Set Relay Off command */

/* **************************
 * PRIVATE TYPES DEFINITION *
 * **************************/

/* Homeoffice device struct data */
typedef struct homeoffice_data{
    float voltage;
    float current;
    float power;
    uint8_t relay;
}__attribute__((__packed__)) __attribute__((aligned(4))) homeoffice_data;

/* *********************************
 * PROTOTYPES OF PRIVATE FUNCTIONS *
 * *********************************/

static char *get_cmd_str(uint8_t cmd);
static void spi_init();
static void spi_transfer(void *tx_buf, void *rx_buf, size_t len);
static void spi_write(uint8_t cmd);
static void spi_read(void *rx_buf, size_t len);
static void spi_read_voltage();
static void spi_read_current();
static void spi_read_power();
static void spi_read_all();
static void spi_read_relay();
static void spi_set_relay(uint8_t state);

/* *************************************
 * PRIVATE GLOBAL VARIABLES DEFINITION *
 * *************************************/

static int gs_spi_fd = -1; /* SPI file descriptor */

static homeoffice_data gs_homeoffice_data; /* Homeoffice data */

/* *********************************
 * DEFINITION OF PRIVATE FUNCTIONS *
 * *********************************/

/**
 * @brief Get the SPI command string
 * 
 * @param cmd SPI Command
 * @return char* 
 */
static char *get_cmd_str(uint8_t cmd)
{
    switch (cmd)
    {
    case CMD_READ_POWER:
        return "READ POWER";
    case CMD_READ_CURRENT:
        return "READ CURRENT";
    case CMD_READ_VOLTAGE:
        return "READ VOLTAGE";
    case CMD_READ_RELAY:
        return "READ RELAY";
    case CMD_READ_ALL:
        return "READ ALL";
    case CMD_SET_RELAY_ON:
        return "SET RELAY ON";
    case CMD_SET_RELAY_OFF:
        return "SET RELAY OFF";
    default:
        return "UNKNOWN";
    }
}

/**
 * @brief Initialize SPI communication
 * 
 */
static void spi_init()
{
    gs_spi_fd = open(SPI_DEVICE, O_RDWR);
    if (gs_spi_fd < 0)
    {
        perror("Error opening SPI device");
        exit(1);
    }

    uint8_t mode = SPI_MODE;
    if (ioctl(gs_spi_fd, SPI_IOC_WR_MODE, &mode) < 0)
    {
        perror("Error setting SPI mode");
        exit(1);
    }

    uint8_t bits_per_word = SPI_BITS_PER_WORD;
    if (ioctl(gs_spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits_per_word) < 0)
    {
        perror("Error setting SPI bits per word");
        exit(1);
    }

    uint32_t speed_hz = SPI_SPEED_HZ;
    if (ioctl(gs_spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed_hz) < 0)
    {
        perror("Error setting SPI speed");
        exit(1);
    }
}

/**
 * @brief SPI transfer data
 * 
 * @param tx_buf Transmit buffer
 * @param rx_buf Receive buffer
 * @param len Length of the buffer
 */
static void spi_transfer(void *tx_buf, void *rx_buf, size_t len)
{
    uint8_t sendbuf[20] = {0};
    uint8_t recvbuf[20] = {0};

    memset(sendbuf, 0, sizeof(sendbuf));
    memset(recvbuf, 0, sizeof(recvbuf));


    uint8_t data;

    if (tx_buf != NULL){
        memcpy(sendbuf, tx_buf, len);
        data = sendbuf[0];

        printd("==== SENDING ====\n");
        printd(" CMD: %s (%d)\n", get_cmd_str(data), data);
        for(int i = 1; i < len; i++)
        {   
            data = sendbuf[i];
            if(data != 0){
                printd(" %02d: %d\n", i, data);
            }
        }
    }

    struct spi_ioc_transfer transfer = {
        .tx_buf = (unsigned long)sendbuf,
        .rx_buf = (unsigned long)recvbuf,
        .len = 20,
        .speed_hz = SPI_SPEED_HZ,
        .bits_per_word = SPI_BITS_PER_WORD,
    };

    if (ioctl(gs_spi_fd, SPI_IOC_MESSAGE(1), &transfer) < 0)
    {
        perror("Error transferring SPI data");
        exit(1);
    }

    if (rx_buf != NULL)
    {
        data = recvbuf[2];
        printd("=== RECEIVING ===\n");
        printd(" CMD: %s (%d)\n", get_cmd_str(data), data);
        for(int i = 3; i < 20; i++)
        {   
            data = recvbuf[i];
            if(data != 0){
                printd(" %02d: %d\n", i, data);
            }
        }
        printd("=================\n");

        memcpy(rx_buf,&recvbuf[3], len);
    }
}

/**
 * @brief SPI write data
 * 
 * @param cmd SPI Command
 */
static void spi_write(uint8_t cmd)
{
    spi_transfer(&cmd, NULL, 1);
}

/**
 * @brief SPI read data
 * 
 * @param rx_buf Receive buffer
 * @param len Length of the buffer
 */
static void spi_read(void *rx_buf, size_t len)
{
    spi_transfer(NULL, rx_buf, len);
}

/**
 * @brief SPI read voltage data
 * 
 */
static void spi_read_voltage()
{   
    spi_write(CMD_READ_VOLTAGE);
    spi_read(&gs_homeoffice_data.voltage, sizeof(float));
    printf("Voltage: %05.2f V\n", gs_homeoffice_data.voltage);
}

/**
 * @brief SPI read current data
 * 
 */
static void spi_read_current()
{
    spi_write(CMD_READ_CURRENT);
    spi_read(&gs_homeoffice_data.current, sizeof(float));
    printf("Current: %05.2f mA\n", gs_homeoffice_data.current * 1000);
}

/**
 * @brief SPI read power data
 * 
 */

static void spi_read_power()
{
    spi_write(CMD_READ_POWER);
    spi_read(&gs_homeoffice_data.power, sizeof(float));
    printf("Power: %05.2f mW\n", gs_homeoffice_data.power * 1000);
}

/**
 * @brief SPI read all data
 * 
 */
static void spi_read_all()
{
    spi_write(CMD_READ_ALL);
    spi_read(&gs_homeoffice_data, sizeof(homeoffice_data));
    printf("Voltage: %05.2f mV\n", gs_homeoffice_data.voltage);
    printf("Current: %05.2f mA\n", gs_homeoffice_data.current *1000);
    printf("Power: %05.2f mW\n", gs_homeoffice_data.power * 1000);
    printf("Relay: %s\n", gs_homeoffice_data.relay ? "ON" : "OFF");
}

/**
 * @brief SPI read relay data
 * 
 */
static void spi_read_relay()
{
    spi_write(CMD_READ_RELAY);
    spi_read(&gs_homeoffice_data.relay, sizeof(uint8_t));
    printf("Relay: %s\n", gs_homeoffice_data.relay ? "ON" : "OFF");
}

/**
 * @brief SPI set relay state
 * 
 */
static void spi_set_relay(uint8_t state)
{
    spi_write(state ? CMD_SET_RELAY_ON : CMD_SET_RELAY_OFF);
    spi_read(&gs_homeoffice_data.relay, sizeof(uint8_t));
    printf("Relay: %s\n", gs_homeoffice_data.relay ? "ON" : "OFF");
}

/* ********************************
 * DEFINITION OF PUBLIC FUNCTIONS *
 * ********************************/

int main(int argc, char **argv)
{
    spi_init();

    int choice = 0, c;
    while (choice != 8)
    {
        system("clear");

        memset(&gs_homeoffice_data, 0, sizeof(gs_homeoffice_data));

        printf("Choices:\n");
        printf(" 1: Read Power\n");
        printf(" 2: Read Current\n");
        printf(" 3: Read Voltage\n");
        printf(" 4: Read Relay\n");
        printf(" 5: Read All\n");
        printf(" 6: Set Relay On\n");
        printf(" 7: Set Relay Off\n");
        printf(" 8: Exit\n\n");
        printf("Enter your choice: ");
        scanf("%d", &choice);
        printf("\n");

        switch (choice)
        {
            case 1:
                spi_read_power();
                break;
            case 2:
                spi_read_current();
                break;
            case 3:
                spi_read_voltage();
                break;
            case 4:
                spi_read_relay();
                break;
            case 5:
                spi_read_all();
                break;
            case 6:
                spi_set_relay(1);
                break;
            case 7:
                spi_set_relay(0);
                break;
            case 8:
                printf("Exiting...\n");
                break;
            default:
                printf("Invalid choice\n");
                break;
        }

        /* Flush the STDIN */
        do c = getchar(); while (c != '\n' && c != EOF);

        /* Pause the execution of the code */
        printf("\nPress [ENTER] to continue....");
        c = getchar();

    }

    close(gs_spi_fd);

    return 0;
}
