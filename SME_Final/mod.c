#include "ssd1306.h"
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/types.h>

/*
 ** This function writes the data into the I2C client
 **
 **  Arguments:
 **      buff -> buffer to be sent
 **      len  -> Length of the data
 **   
 */

static struct i2c_adapter * ldr_i2c_adapter = NULL; // I2C Adapter Structure
static struct i2c_client * ldr_i2c_client_oled = NULL; // I2C Cient Structure (In our case it is OLED)

static int  count = 0,curr_value1=0;

static struct timer_list my_timer;
static int i2c_write(unsigned char * buf, unsigned int len) {
    /*
     ** Sending Start condition, Slave address with R/W bit, 
     ** ACK/NACK and Stop condtions will be handled internally.
     */
    int ret = i2c_master_send(ldr_i2c_client_oled, buf, len);

    return ret;
}

/*
 ** This function is specific to the SSD_1306 OLED.
 ** This function sends the command/data to the OLED.
 **
 **  Arguments:
 **      is_cmd -> true = command, flase = data
 **      data   -> data to be written
 ** 
 */
static void ssd1306_write(bool is_cmd, unsigned char data) {
    unsigned char buf[2] = {
        0
    };

    /*
     ** First byte is always control byte. Data is followed after that.
     **
     ** There are two types of data in SSD_1306 OLED.
     ** 1. Command
     ** 2. Data
     **
     ** Control byte decides that the next byte is, command or data.
     **
     ** -------------------------------------------------------                        
     ** |              Control byte's | 6th bit  |   7th bit  |
     ** |-----------------------------|----------|------------|    
     ** |   Command                   |   0      |     0      |
     ** |-----------------------------|----------|------------|
     ** |   data                      |   1      |     0      |
     ** |-----------------------------|----------|------------|
     ** 
     ** Please refer the datasheet for more information. 
     **    
     */
    if (is_cmd == true) {
        buf[0] = 0x00;
    } else {
        buf[0] = 0x40;
    }

    buf[1] = data;

    ret = i2c_write(buf, 2);
}

/*
 ** This function is specific to the SSD_1306 OLED.
 **
 **  Arguments:
 **      lineNo    -> Line Number
 **      cursorPos -> Cursor Position
 **   
 */
static void ssd1306_set_cursor(uint8_t lineNo, uint8_t cursorPos) {
    /* Move the Cursor to specified position only if it is in range */
    if ((lineNo <= SSD1306_MAX_LINE) && (cursorPos < SSD1306_MAX_SEG)) {
        line_num = lineNo; // Save the specified line number
        Cursor_pos = cursorPos; // Save the specified cursor position

        ssd1306_write(true, 0x21); // cmd for the column start and end address
        ssd1306_write(true, cursorPos); // column start addr
        ssd1306_write(true, SSD1306_MAX_SEG - 1); // column end addr

        ssd1306_write(true, 0x22); // cmd for the page start and end address
        ssd1306_write(true, lineNo); // page start addr
        ssd1306_write(true, SSD1306_MAX_LINE); // page end addr
    }
}

/*
 ** This function is specific to the SSD_1306 OLED.
 ** This function move the cursor to the next line.
 **
 **  Arguments:
 **      none
 ** 
 */
static void ssd1306_goto_next_line(void) {
    /*
     ** Increment the current line number.
     ** roll it back to first line, if it exceeds the limit. 
     */
    line_num++;
    line_num = (line_num & SSD1306_MAX_LINE);

    ssd1306_set_cursor(line_num, 0); /* Finally move it to next line */
}

/*
 ** This function is specific to the SSD_1306 OLED.
 ** This function sends the single char to the OLED.
 **
 **  Arguments:
 **      c   -> character to be written
 ** 
 */
static void ssd1306_print_char(unsigned char c) {
    uint8_t data_byte;
    uint8_t temp = 0;

    /*
     ** If we character is greater than segment len or we got new line charcter
     ** then move the cursor to the new line
     */
    if (((Cursor_pos + SSD1306_FontSize) >= SSD1306_MAX_SEG) ||
        (c == '\n')
    ) {
        ssd1306_goto_next_line();
    }

    // print charcters other than new line
    if (c != '\n') {

        /*
         ** In our font array (SSD1306_font), space starts in 0th index.
         ** But in ASCII table, Space starts from 32 (0x20).
         ** So we need to match the ASCII table with our font table.
         ** We can subtract 32 (0x20) in order to match with our font table.
         */
        c -= 0x20; //or c -= ' ';

        do {
            data_byte = SSD1306_font[c][temp]; // Get the data to be displayed from LookUptable

            ssd1306_write(false, data_byte); // write data to the OLED
            Cursor_pos++;

            temp++;

        } while (temp < SSD1306_FontSize);
        ssd1306_write(false, 0x00); //Display the data
        Cursor_pos++;
    }
}

/*
 ** This function is specific to the SSD_1306 OLED.
 ** This function sends the string to the OLED.
 **
 **  Arguments:
 **      str   -> string to be written
 ** 
 */
static void ssd1306_string_display(unsigned char * str) {
    while ( * str) {
        ssd1306_print_char( * str++);
    }
}

static int ssd1306_init(void) {
    // delay

    /*
     ** Commands to initialize the SSD_1306 OLED Display
     */
    ssd1306_write(true, 0xAE); // Entire Display OFF
    ssd1306_write(true, 0xD5); // Set Display Clock Divide Ratio and Oscillator Frequency
    ssd1306_write(true, 0x80); // Default Setting for Display Clock Divide Ratio and Oscillator Frequency that is recommended
    ssd1306_write(true, 0xA8); // Set Multiplex Ratio
    ssd1306_write(true, 0x3F); // 64 COM lines
    ssd1306_write(true, 0xD3); // Set display offset
    ssd1306_write(true, 0x00); // 0 offset
    ssd1306_write(true, 0x40); // Set first line as the start line of the display
    ssd1306_write(true, 0x8D); // Charge pump
    ssd1306_write(true, 0x14); // Enable charge dump during display on
    ssd1306_write(true, 0x20); // Set memory addressing mode
    ssd1306_write(true, 0x00); // Horizontal addressing mode
    ssd1306_write(true, 0xA1); // Set segment remap with column address 127 mapped to segment 0
    ssd1306_write(true, 0xC8); // Set com output scan direction, scan from com63 to com 0
    ssd1306_write(true, 0xDA); // Set com pins hardware configuration
    ssd1306_write(true, 0x12); // Alternative com pin configuration, disable com left/right remap
    ssd1306_write(true, 0x81); // Set contrast control
    ssd1306_write(true, 0x80); // Set Contrast to 128
    ssd1306_write(true, 0xD9); // Set pre-charge period
    ssd1306_write(true, 0xF1); // Phase 1 period of 15 DCLK, Phase 2 period of 1 DCLK
    ssd1306_write(true, 0xDB); // Set Vcomh deselect level
    ssd1306_write(true, 0x20); // Vcomh deselect level ~ 0.77 Vcc
    ssd1306_write(true, 0xA4); // Entire display ON, resume to RAM content display
    ssd1306_write(true, 0xA6); // Set Display in Normal Mode, 1 = ON, 0 = OFF
    ssd1306_write(true, 0x2E); // Deactivate scroll
    ssd1306_write(true, 0xAF); // Display ON in normal mode

   // ssd1306_write(true, 0x81); // Contrast command
    //Clear the display
    ssd1306_fill(0x00);
    return 0;
}

/*
 ** This function Fills the complete OLED with this data byte.
 **
 **  Arguments:
 **      data  -> Data to be filled in the OLED
 ** 
 */
static void ssd1306_fill(unsigned char data) {
    unsigned int total = 128 * 8; // 8 pages x 128 segments x 8 bits of data
    unsigned int i = 0;

    //Fill the Display
    for (i = 0; i < total; i++) {
        ssd1306_write(false, data);
    }
}

static void SSD1306_SetBrightness(uint8_t brightnessValue)
{
    ssd1306_write(true, 0x81); // Contrast command
    ssd1306_write(true, brightnessValue); // Contrast value (default value = 0x7F)
}

/*
 ** This function getting called when the slave has been found
 ** Note : This will be called only once when we load the driver.
 */
static int oled_probe(struct i2c_client * client,const struct i2c_device_id * id) {

    //Write String to OLED
    ssd1306_init();
  
    //Set cursor
    ssd1306_set_cursor(0,0); 
    
    //Write String to OLED
    ssd1306_string_display("WELCOME\nTO\nSYAMDETECT\n\n");
    
    return 0;
}

/*
 ** This function getting called when the slave has been removed
 ** Note : This will be called only once when we unload the driver.
 */
static void oled_remove(struct i2c_client * client) {
    //Set cursor
    //ssd1306_set_cursor(2,0);  
    //Write String to OLED
    ssd1306_string_display("THANK YOU!!!");

    //Set cursor
    ssd1306_set_cursor(0, 0);
    //clear the display
    ssd1306_fill(0x00);

    ssd1306_write(true, 0xAE); // Entire Display OFF

    pr_info("OLED Removed!!!\n");
}

static const struct i2c_device_id etx_oled_id[] = {
    {
        SLAVE_DEVICE_NAME,
        0
    },
    {}
};
MODULE_DEVICE_TABLE(i2c, etx_oled_id);

/*
 ** I2C driver Structure that has to be added to linux
 */
static struct i2c_driver etx_oled_driver = {
    .driver = {
        .name = SLAVE_DEVICE_NAME,
        .owner = THIS_MODULE,
    },
    .probe = oled_probe,
    .remove         = oled_remove,
    .id_table = etx_oled_id,
};

/*
 ** I2C Board Info strucutre
 */
static struct i2c_board_info oled_i2c_board_info = {
    I2C_BOARD_INFO(SLAVE_DEVICE_NAME, SSD1306_SLAVE_ADDR)
};



unsigned long old_jiffie = 0;
static irqreturn_t ldr_sensor_irq_handler(int irq, void * dev_id) {

     unsigned long diff = jiffies - old_jiffie;
     if (diff < 500) {
         return IRQ_HANDLED;
    }

    // old_jiffie = jiffies;

    // Read the value from the GPIO pin
    curr_value1 = gpio_get_value(SENSOR1_PIN);
   pr_info("Data = %d\n",curr_value1); 
    if(curr_value1==1)
    {
        count=0;
        pr_info("No light detected...!\n"); //For Darkness
    }
    else
    {
        count=1;
        pr_info("Intruder detected...!\n"); //For Intruder in Darkness (not dark)
    }




    return IRQ_WAKE_THREAD;
}
static int brightness = 5;      // Global variable for changing contract settings
static irqreturn_t interrupt_thread_fn(int irq, void * dev_id) {
    /*
        Function that handles the thread created by IRQ
    */
    ssd1306_init();

    //Set cursor
    ssd1306_set_cursor(0, 0);

    
	SSD1306_SetBrightness(brightness);
      if(count == 1)
      //Write String to OLED
      
      {
        ssd1306_string_display("Intruder detected.\n");    //Day outside/ Intruder present
        brightness = 3;     //Set low brightness when intruder is detected
      }
      else
      {
        ssd1306_string_display("No light detected.\n");     //Its dark/ Empty room
	    brightness = 149;   //Set high brightness when no light is detected
      }
    
    return IRQ_HANDLED;
}

static int __init etx_driver_init(void) {

    //ssd1306_write(true, 0x81); // Contrast command
    ldr_i2c_adapter = i2c_get_adapter(I2C_BUS_AVAILABLE);

    if (ldr_i2c_adapter != NULL) {
        ldr_i2c_client_oled = i2c_new_client_device(ldr_i2c_adapter, & oled_i2c_board_info);

        if (ldr_i2c_client_oled != NULL) {
            i2c_add_driver( & etx_oled_driver);
            ret = 0;
        }

        i2c_put_adapter(ldr_i2c_adapter);
    }

    // Request GPIO pin for LDR sensor
    ret = gpio_request(SENSOR1_PIN, "ldr_sensor");
    if (ret < 0) {
        printk(KERN_ERR "Failed to request GPIO %d for LDR sensor\n", SENSOR1_PIN);
        return ret;
    }

    
    // Set GPIO pin as input
    ret = gpio_direction_input(SENSOR1_PIN);
    if (ret < 0) {
        printk(KERN_ERR "Failed to set GPIO %d as input for LDR sensor\n", SENSOR1_PIN);
        gpio_free(SENSOR1_PIN);
        return ret;
    }
    ret = request_threaded_irq(gpio_to_irq(SENSOR1_PIN), ldr_sensor_irq_handler, interrupt_thread_fn, IRQF_TRIGGER_RISING, "ldr_sensor", NULL);
    if (ret < 0) {
        printk(KERN_ERR "Failed to request interrupt for LDR sensor\n");
        gpio_free(SENSOR1_PIN);
        return ret;
    }

    
    printk(KERN_INFO "sensor driver initialized\n");

    return ret;
}

/*
 ** Module Exit function
 */
static void __exit etx_driver_exit(void) {
    i2c_unregister_device(ldr_i2c_client_oled);
    i2c_del_driver( & etx_oled_driver);
    
    free_irq(gpio_to_irq(SENSOR1_PIN), NULL);
    gpio_free(SENSOR1_PIN);

   
    del_timer_sync( & my_timer);

    printk(KERN_INFO "LDR sensor driver exited\n");
}
module_init(etx_driver_init);
module_exit(etx_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SYAMDETECT");
MODULE_DESCRIPTION("LDR (Light Dependent Resistor) Sensor Driver");
MODULE_VERSION("1.0");