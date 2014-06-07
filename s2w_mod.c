#undef __KERNEL__
#define __KERNEL__
 
#undef MODULE
#define MODULE

#include <linux/module.h>
#include <linux/kernel.h>    
#include <linux/init.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/kallsyms.h>

#define DRIVER_AUTHOR "flar2 (asegaert at gmail.com)"
#define DRIVER_DESCRIPTION "sweep2sleep module, modified for 720p by CNexus (me at cnexus.co)"
#define DRIVER_VERSION "1.0"

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESCRIPTION);
MODULE_VERSION(DRIVER_VERSION);
MODULE_LICENSE("GPL");


/* get existing functions */

void (*my_input_set_capability)(struct input_dev *dev, unsigned int type, unsigned int code);

struct input_dev *(*my_input_allocate_device)(void);

int *(*my_input_register_device)(struct input_dev *dev);

int *(*my_input_register_handler)(struct input_handler *handler);

int *(*my_input_register_handle)(struct input_handle *handle);

int *(*my_input_open_device)(struct input_handle *handle);

void (*my_input_event)(struct input_dev *dev, unsigned int type, unsigned int code, int value);


//sweep2sleep
#define S2W_PWRKEY_DUR          60
#define S2W_Y_MAX               1920
#define S2W_X_MAX               1280
#define S2W_Y_LIMIT             S2W_Y_MAX-120
#define S2W_X_B1                466
#define S2W_X_B2                800
#define S2W_X_FINAL             266

static int touch_x = 0, touch_y = 0;
static bool touch_x_called = false, touch_y_called = false;
static bool scr_on_touch = false, barrier[2] = {false, false};
static bool exec_count = true;
static struct input_dev * sweep2wake_pwrdev;
static DEFINE_MUTEX(pwrkeyworklock);
static struct workqueue_struct *s2w_input_wq;
static struct work_struct s2w_input_work;

/* PowerKey work func */
static void sweep2wake_presspwr(struct work_struct * sweep2wake_presspwr_work) {
	if (!mutex_trylock(&pwrkeyworklock))
                return;
	my_input_event(sweep2wake_pwrdev, EV_KEY, KEY_POWER, 1);
	my_input_event(sweep2wake_pwrdev, EV_SYN, 0, 0);
	msleep(S2W_PWRKEY_DUR);
	my_input_event(sweep2wake_pwrdev, EV_KEY, KEY_POWER, 0);
	my_input_event(sweep2wake_pwrdev, EV_SYN, 0, 0);
	msleep(S2W_PWRKEY_DUR);
        mutex_unlock(&pwrkeyworklock);
	return;
}
static DECLARE_WORK(sweep2wake_presspwr_work, sweep2wake_presspwr);

/* PowerKey trigger */
static void sweep2wake_pwrtrigger(void) {
	schedule_work(&sweep2wake_presspwr_work);
        return;
}

/* reset on finger release */
static void sweep2wake_reset(void) {
	exec_count = true;
	barrier[0] = false;
	barrier[1] = false;
	scr_on_touch = false;
}

/* Sweep2wake main function */
static void detect_sweep2wake(int x, int y, bool st)
{
        int prevx = 0, nextx = 0;
        bool single_touch = st;

	if (single_touch) {
		scr_on_touch=true;
		prevx = (S2W_X_MAX - S2W_X_FINAL);
		nextx = S2W_X_B2;
		if ((barrier[0] == true) ||
		   ((x < prevx) &&
		    (x > nextx) &&
		    (y > S2W_Y_LIMIT))) {
			prevx = nextx;
			nextx = S2W_X_B1;
			barrier[0] = true;
			if ((barrier[1] == true) ||
			   ((x < prevx) &&
			    (x > nextx) &&
			    (y > S2W_Y_LIMIT))) {
				prevx = nextx;
				barrier[1] = true;
				if ((x < prevx) &&
				    (y > S2W_Y_LIMIT)) {
					if (x < S2W_X_FINAL) {
						if (exec_count) {
							sweep2wake_pwrtrigger();
							exec_count = false;
						}
					}
				}
			}
		}
	}
}


static void s2w_input_callback(struct work_struct *unused) {

	detect_sweep2wake(touch_x, touch_y, true);

	return;
}

static void s2w_input_event(struct input_handle *handle, unsigned int type,
				unsigned int code, int value) {

	if (code == ABS_MT_SLOT) {
		sweep2wake_reset();
		return;
	}

	if (code == ABS_MT_TRACKING_ID && value == -1) {
		sweep2wake_reset();
		return;
	}

	if (code == ABS_MT_POSITION_X) {
		touch_x = value;
		touch_x_called = true;
	}

	if (code == ABS_MT_POSITION_Y) {
		touch_y = value;
		touch_y_called = true;
	}

	if (touch_x_called && touch_y_called) {
		touch_x_called = false;
		touch_y_called = false;
		queue_work_on(0, s2w_input_wq, &s2w_input_work);
	}
}

static int input_dev_filter(struct input_dev *dev) {
	if (strstr(dev->name, "touch")) {
		return 0;
	} else {
		return 1;
	}
}

static int s2w_input_connect(struct input_handler *handler,
				struct input_dev *dev, const struct input_device_id *id) {
	struct input_handle *handle;
	int error;

	if (input_dev_filter(dev))
		return -ENODEV;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "s2w";

	error = *my_input_register_handle(handle);

	error = *my_input_open_device(handle);

	return 0;

}

static const struct input_device_id s2w_ids[] = {
	{ .driver_info = 1 },
	{ },
};

static struct input_handler s2w_input_handler = {
	.event		= s2w_input_event,
	.connect	= s2w_input_connect,
	.name		= "s2w_inputreq",
	.id_table	= s2w_ids,
};


static void lookups(void)
{
	my_input_set_capability = (void *)kallsyms_lookup_name("input_set_capability");
	my_input_register_device = (void *)kallsyms_lookup_name("input_register_device");
	my_input_event = (void *)kallsyms_lookup_name("input_event");
	my_input_allocate_device = (void *)kallsyms_lookup_name("input_allocate_device");
	my_input_register_handler = (void *)kallsyms_lookup_name("input_register_handler");
	my_input_register_handle = (void *)kallsyms_lookup_name("input_register_handle");
	my_input_open_device = (void *)kallsyms_lookup_name("input_open_device");
}


static int __init sweep2wake_init(void)
{
	int rc = 0;

	lookups();

	sweep2wake_pwrdev = my_input_allocate_device();

	my_input_set_capability(sweep2wake_pwrdev, EV_KEY, KEY_POWER);

	sweep2wake_pwrdev->name = "s2w_pwrkey";
	sweep2wake_pwrdev->phys = "s2w_pwrkey/input0";

	rc = *my_input_register_device(sweep2wake_pwrdev);

	s2w_input_wq = create_workqueue("s2wiwq");

	INIT_WORK(&s2w_input_work, s2w_input_callback);
	rc = *my_input_register_handler(&s2w_input_handler);

	return 0;
}

static void __exit sweep2wake_exit(void)
{
}

module_init(sweep2wake_init);
module_exit(sweep2wake_exit);
