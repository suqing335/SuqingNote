#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/wakelock.h>

#define SN7326_KEYDATA		0X10
#define SN7326_KEY_UP_DOWN	0X40
#define SN7326_KEY_NUMBER	0X80
#define SN7326_KEY_CODE		0X3F
#define SN7326_CYCLE_INTERVAL	(2*HZ)

//extern void rk28_send_wakeup_key(void);

// sn7326 key_value0-7
// sn7326 key_value8-15
// sn7326 key_value16-23
// sn7326 key_value24-31
// sn7326 key_value32-41

#define TIME_NUM  (10)
/*
static const unsigned short sn7326_key2code[] = {
        KEY_REDIAL,	KEY_FLASH,	KEY_7,		 KEY_ONOFF,		KEY_REDKEY,		KEY_LEFT,	KEY_0,		KEY_0,
        KEY_DND,	KEY_CLEAR,	KEY_NUMERIC_STAR,KEY_3,			KEY_PLAYPAUSE, 		KEY_OK, 	KEY_0, 		KEY_0,
        KEY_HOLD, 	KEY_2,		KEY_0, 		 KEY_6,			KEY_NEXT, 		KEY_RIGHT, 	KEY_0, 		KEY_0,
        KEY_MUTE, 	KEY_1,		KEY_8, 		 KEY_9,			KEY_DELETE, 		KEY_DOWN, 	KEY_0, 		KEY_0,
        KEY_VOLUMEUP, 	KEY_4,		KEY_5, 		 KEY_NUMERIC_POUND,	KEY_UP, 		KEY_PHONE, 	KEY_0, 		KEY_0,		KEY_VOLUMEDOWN,
};*/
static unsigned short sn7326_key2code[][2] = {
	{0x19, KEY_1},
	{0x11, KEY_2},
	{0x0b, KEY_3},

	{0x21, KEY_4},
	{0x22, KEY_5},
	{0x13, KEY_6},

	{0x02, KEY_7},
	{0x1a, KEY_8},
	{0x1b, KEY_9},

	{0x0a, KEY_NUMERIC_STAR},
	{0x12, KEY_0},
	{0x23, KEY_NUMERIC_POUND}
};

struct sn7326_data {
        struct i2c_client *client;
        struct input_dev *input;
	//struct delayed_work dwork;
	struct tasklet_struct keyboard_tasklet;
	struct timer_list timer;
        //unsigned short keycodes[ARRAY_SIZE(sn7326_key2code)];
	unsigned short (*keycodes)[2];
        u8 last_keys;
	spinlock_t t_lock;
	unsigned int irq;
	unsigned int irq_gpio;
	unsigned int reset;
	unsigned int led_en;
	unsigned int s_count;
	
};

static int sn7326_read(struct i2c_client *client, u8 reg)
{
        int ret;

        ret = i2c_smbus_read_byte_data(client, reg);
        if (ret < 0)
                dev_err(&client->dev,"can not read register, returned %d\n", ret);

        return ret;
}

static int sn7326_write(struct i2c_client *client, unsigned char reg, unsigned char data)
{
        int ret;
	
	ret = i2c_smbus_write_byte_data(client, reg, data);
        if (ret < 0)
                dev_err(&client->dev,"can not write register, returned %d\n", ret);
 
 	return ret; 
	
}

static void sn7326_key_report(int key_value,struct sn7326_data *sn7326)
{
	int i;
	struct input_dev *input = sn7326->input;
	int key_code=key_value & SN7326_KEY_CODE;
	//printk("sn7326_key_report key_code : 0x%x\n", key_code);

	if((key_value & SN7326_KEY_UP_DOWN)==0x40) {
	//key down
		//printk("key down\n");
	
		for (i = 0; i < ARRAY_SIZE(sn7326_key2code); i++) {
			if(sn7326_key2code[i][0] == key_code) {
				input_report_key(input, sn7326_key2code[i][1], 1);
				input_sync(input);
				//msleep(50);
				input_report_key(input, sn7326_key2code[i][1], 0);
				input_sync(input);
				
				spin_lock(&sn7326->t_lock);
				if(sn7326->s_count > 0) {
					sn7326->s_count = TIME_NUM;
				} else {
					gpio_set_value(sn7326->led_en, 1);
					sn7326->s_count = TIME_NUM;
					sn7326->timer.expires = jiffies + 2*HZ;//2s
					add_timer(&sn7326->timer);
				}
				spin_unlock(&sn7326->t_lock);
				break;
			}
		}	
	}	
}

static void sn7326_timer_handle(unsigned long data) {
	struct sn7326_data *sn7326 = (struct sn7326_data *)data;
	spin_lock(&sn7326->t_lock);
	if(sn7326->s_count >= 2) {
		sn7326->s_count -= 2;
		if(sn7326->s_count > 0) {
			sn7326->timer.expires = jiffies + 2*HZ;//2s
			add_timer(&sn7326->timer);
		} else {
			gpio_set_value(sn7326->led_en, 0);
		}
	} 
	spin_unlock(&sn7326->t_lock);
}

static void keyboard_do_something(unsigned long data){
	int keyvalue;
	struct sn7326_data *sn7326 = (struct sn7326_data *)data;
	struct i2c_client *client = sn7326->client;
	
	keyvalue = sn7326_read(client, SN7326_KEYDATA);
	sn7326_key_report(keyvalue, sn7326);	
}

static irqreturn_t sn7326_interrupt(int irq, void *dev_id)
{
	struct sn7326_data *data = dev_id;
        
	tasklet_schedule(&data->keyboard_tasklet);

	return IRQ_HANDLED;
}

static int sn7326_parse_dt(struct device_node *np, struct sn7326_data *sn7326) {
	int ret;
	unsigned int gpio;
	enum of_gpio_flags gpio_flags;
	/**key irq gpio */
	gpio = of_get_named_gpio_flags(np, "irq_gpio", 0, &gpio_flags);
	ret = gpio_request(gpio, "irq_gpio");
	if (ret < 0) {
		printk("request I/O irq_gpio : %d failed\n", gpio);
		return -ENODEV;
	}
	gpio_direction_input(gpio);
	sn7326->irq_gpio = gpio;
	
	sn7326->irq = gpio_to_irq(gpio);
	if (sn7326->irq < 0) {
		ret = sn7326->irq;
		printk("irq for gpio %d error(%d)\n", gpio, ret);
		gpio_free(gpio);
		return ret;
	}
	
	/*******reset gpio******/
	gpio = of_get_named_gpio_flags(np, "reset_gpio", 0, &gpio_flags);
	ret = gpio_request(gpio, "reset_gpio");
	if (ret < 0) {
		printk("request I/O reset_gpio : %d failed\n", gpio);
		return -ENODEV;
	}
	gpio_direction_output(gpio, 1);
	sn7326->reset = gpio;
	
	/*******led en gpio******/
	gpio = of_get_named_gpio_flags(np, "led_en_gpio", 0, &gpio_flags);
	ret = gpio_request(gpio, "led_en_gpio");
	if (ret < 0) {
		printk("request I/O led_en_gpio : %d failed\n", gpio);
		return -ENODEV;
	}
	gpio_direction_output(gpio, 0);
	sn7326->led_en = gpio;
	printk("%s probe success\n", __func__);
	
	/*******led en gpio******/
/*	gpio = of_get_named_gpio_flags(np, "gpiosen", 0, &gpio_flags);
	ret = gpio_request(gpio, "gpiosen");
	if (ret < 0) {
		printk("request I/O gpiosen : %d failed\n", gpio);
		return -ENODEV;
	}
	gpio_direction_output(gpio, 0);
	printk("%s gpio success\n", __func__);
	msleep(2000);
	gpio_direction_output(gpio, 1);
*/
	return ret;
}
static int sn7326_probe(struct i2c_client *client,const struct i2c_device_id *id)
{
	
 	struct input_dev *input;
	struct sn7326_data *sn7326;
	struct device_node *np = client->dev.of_node;
	int ret,i;
	
	sn7326 = kzalloc(sizeof(struct sn7326_data), GFP_KERNEL);
	if(!sn7326) {
		return -1;
	}
        input = input_allocate_device();
        if (!sn7326 || !input) {
                dev_err(&client->dev, "insufficient memory\n");
                goto err_malloc_mem;
        }
	ret = sn7326_parse_dt(np, sn7326);
	if(ret < 0) {
		printk("%s, parse dt faile\n", __func__);
		goto err_parse_dt;
	}
	sn7326->client = client;
	sn7326->input =input;

	input->name = "SN7326 key_board";
        input->dev.parent = &client->dev;
        input->id.bustype = BUS_I2C;

	input->keycode = sn7326->keycodes;
        input->keycodesize = sizeof(sn7326->keycodes[0]);
        input->keycodemax = ARRAY_SIZE(sn7326_key2code);

	sn7326->keycodes = sn7326_key2code;
	__set_bit(EV_KEY, input->evbit);

        for (i = 0; i < ARRAY_SIZE(sn7326_key2code); i++) {
                __set_bit(sn7326_key2code[i][1], input->keybit);
        }	

	ret = input_register_device(sn7326->input);
        if (ret) {
                dev_err(&client->dev, "Failed to register input device\n");
                goto err_register_input;
        }

	gpio_set_value(sn7326->reset, 0);
	msleep(50);
	gpio_set_value(sn7326->reset, 1);
	ret = request_irq(sn7326->irq, sn7326_interrupt, IRQF_TRIGGER_FALLING, client->dev.driver->name, sn7326);
	if (ret) {
                gpio_free(sn7326->irq);
                printk(KERN_ERR "%s: request irq failed,ret is %d\n",__func__,ret);
        	goto err_request_irq;
        }
	disable_irq_nosync(sn7326->irq);
	/*****************/
	tasklet_init(&sn7326->keyboard_tasklet, keyboard_do_something, (unsigned long)sn7326);
	sn7326->s_count = 0;
	/******create time_list*******/
	init_timer(&sn7326->timer);
	//sn7326->timer.expires = jiffies + 2*HZ;
	sn7326->timer.function = &sn7326_timer_handle;
	sn7326->timer.data = (unsigned long)sn7326;
	
	spin_lock_init(&sn7326->t_lock);
	
//set 0x08 0x30.set sn7326_irq times
	
	ret=sn7326_write(client, 0x08, 0x30);
	if(ret<0){
	printk("set sn7326 0x08 0x30 faild-------------!!!");	
	}


	enable_irq(sn7326->irq);

	printk("------sn7326_keyboard------init ok!!!\n");
	return 0;

	//free_irq(sn7326->irq, sn7326);
err_request_irq:
	input_unregister_device(sn7326->input);
err_register_input:
err_parse_dt:      
	input_free_device(input);
err_malloc_mem:   
        kfree(sn7326);
        return -1;
}


static const struct i2c_device_id sn7326_id[] = {
        { "sn7326", 0 },
        { }
};

//MODULE_DEVICE_TABLE(i2c, sn7326_id);

static struct of_device_id sn7326_dt_ids[] = {
    { .compatible = "Jstar-sn7326" },
    { }
};

static struct i2c_driver sn7326_driver = {
        .driver = {
                .name = "sn7326",
		.owner    = THIS_MODULE,
		.of_match_table = of_match_ptr(sn7326_dt_ids),
        },
        .probe = sn7326_probe,
        .id_table = sn7326_id,
};

static int sn7623_init(void)
{
        return i2c_add_driver(&sn7326_driver);
}

static void sn7326_exit(void)
{
        i2c_del_driver(&sn7326_driver);
}

module_init(sn7623_init);

module_exit(sn7326_exit);

MODULE_DESCRIPTION("sn7326 keyboard Driver");
MODULE_LICENSE("GPL");