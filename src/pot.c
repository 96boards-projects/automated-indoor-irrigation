#################################################################
# Title: pot.c
# Author: Sahaj Sarup
# Copyright (c) 2018 Linaro Limited
#################################################################

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
/* mraa header */
#include "mraa/spi.h"
#include "mraa/gpio.h"
/* upm headers */
#include "lcm1602.h"
#include "upm_utilities.h"

/* SPI declaration */
#define SPI_BUS 0

/* SPI frequency in Hz */
#define SPI_FREQ 10000
#define GPIO_CS 12
#define GPIO_PUMP 23

static uint8_t tx[3],rx[3];

volatile sig_atomic_t flag = 1;

long map(long x, long in_min, long in_max, long out_min, long out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

int main(int argc, char** argv)
{
    mraa_result_t status = MRAA_SUCCESS;
    mraa_spi_context spi;
    mraa_gpio_context gpio_cs, gpio_pump;
    int adc_value, water_level;
    char u_print[17];
    lcm1602_context rc = lcm1602_i2c_init(0, 0x27, true, 16, 2);

    if (!rc)
    {
	printf("Initialization failed; aborting...\n");
	return 0;
    }

    /* initialize mraa for the platform (not needed most of the times) */
    mraa_init();

    gpio_cs = mraa_gpio_init(GPIO_CS);
    if (gpio_cs == NULL) {
        fprintf(stderr, "Failed to initialize GPIO %d\n", GPIO_CS);
        mraa_deinit();
        return EXIT_FAILURE;
    }

    status = mraa_gpio_dir(gpio_cs, MRAA_GPIO_OUT);
    if (status != MRAA_SUCCESS) {
        goto err_exit;
    }

    gpio_pump = mraa_gpio_init(GPIO_PUMP);
    if (gpio_pump == NULL) {
        fprintf(stderr, "Failed to initialize GPIO %d\n", GPIO_PUMP);
        mraa_deinit();
        return EXIT_FAILURE;
    }

    status = mraa_gpio_dir(gpio_pump, MRAA_GPIO_OUT);
    if (status != MRAA_SUCCESS) {
        goto err_exit;
    }


    //! [Interesting]
    /* initialize SPI bus */
    spi = mraa_spi_init(SPI_BUS);
    if (spi == NULL) {
        fprintf(stderr, "Failed to initialize SPI\n");
        mraa_deinit();
        return EXIT_FAILURE;
    }

    /* set SPI frequency */
    status = mraa_spi_frequency(spi, SPI_FREQ);
    if (status != MRAA_SUCCESS)
        goto err_exit;

    /* set big endian mode */
    status = mraa_spi_lsbmode(spi, 0);
    if (status != MRAA_SUCCESS) {
        goto err_exit;
    }

    status = mraa_spi_bit_per_word(spi, 8);
    if (status != MRAA_SUCCESS) {
        fprintf(stdout, "Failed to set SPI Device to 16Bit mode\n");
        goto err_exit;
    }

   tx[0] = 0x01;
   tx[1] = 0x80;
   tx[2] = 0x00;

   while(flag)
   {
	      mraa_gpio_write(gpio_cs, 1);
        usleep(10);
        mraa_gpio_write(gpio_cs, 0);
	      mraa_spi_transfer_buf(spi,tx,rx,3);
        mraa_gpio_write(gpio_cs, 1);

        adc_value = (rx[1]<<8)&0b1100000000 | (rx[2]&0xff);

      	water_level = map(adc_value, 550, 10, 0, 100);
        sprintf(u_print, "Water P/c: %d ", water_level);
        printf("%d \n", water_level);

        lcm1602_set_cursor(rc, 0, 0);
        lcm1602_write(rc, u_print, 16);


        if(water_level < 30)
        {
            mraa_gpio_write(gpio_pump, 1);
	          lcm1602_set_cursor(rc, 1, 0);
	          lcm1602_write(rc, "PUMP ON ", 8);
        }
        else if(water_level > 30){
            mraa_gpio_write(gpio_pump, 0);
            lcm1602_set_cursor(rc, 1, 0);
            lcm1602_write(rc, "PUMP OFF", 8);
        }

        sleep(1);
   }
    /* stop spi */
    mraa_spi_stop(spi);

    //! [Interesting]
    /* deinitialize mraa for the platform (not needed most of the times) */
    mraa_deinit();

    return EXIT_SUCCESS;

err_exit:
    mraa_result_print(status);

    /* stop spi */
    mraa_spi_stop(spi);

    /* deinitialize mraa for the platform (not needed most of the times) */
    mraa_deinit();

    return EXIT_FAILURE;
}
