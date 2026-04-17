#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/string.h>

#define DEVICE_NAME "ringbuf"
#define CLASS_NAME "ringbuf_class"

static dev_t dev_num;
static struct cdev my_cdev;
static struct class *my_class;
static struct device *my_device;

struct ring_buffer {
	char *data;
	size_t size;
	size_t count;
	size_t read_pos;
	size_t write_pos;
};

static struct ring_buffer rb = {
	.size = 16,
};

static DEFINE_MUTEX(rb_mutex);

static int param_buf_size = 16;

static int buf_size_set(const char *val, const struct kernel_param *kp)
{
	int ret, new_size;
	size_t new_size_z;

	ret = kstrtoint(val, 10, &new_size);
	if (ret < 0)
		return ret;

	if (new_size < 16 || new_size > 65536) {
		pr_err("Buffer size must be between 16 and 65536\n");
		return -EINVAL;
	}
	new_size_z = (size_t)new_size;

	if (mutex_lock_interruptible(&rb_mutex))
		return -ERESTARTSYS;

	if (rb.data) {
		kfree(rb.data);
		rb.data = NULL;
	}

	rb.data = kmalloc(new_size_z, GFP_KERNEL);
	if (!rb.data) {
		mutex_unlock(&rb_mutex);
		return -ENOMEM;
	}

	rb.size = new_size_z;
	rb.count = 0;
	rb.read_pos = 0;
	rb.write_pos = 0;

	mutex_unlock(&rb_mutex);
	param_buf_size = new_size;
	pr_info("Buffer resized to %zu bytes\n", rb.size);
	return 0;
}

static int buf_size_get(char *buffer, const struct kernel_param *kp)
{
	return param_get_int(buffer, kp);
}

static const struct kernel_param_ops buf_size_ops = {
	.set = buf_size_set,
	.get = buf_size_get,
};

module_param_cb(buf_size, &buf_size_ops, &param_buf_size, 0644);
MODULE_PARM_DESC(buf_size, "Size of ring buffer (16..65536)");

static inline bool ringbuf_empty(void)
{
	return rb.count == 0;
}

static inline bool ringbuf_full(void)
{
	return rb.count == rb.size;
}

static inline size_t ringbuf_space(void)
{
	return rb.size - rb.count;
}

/**
 * Запись в буфер: перезаписывает старые данные при нехватке места
 * @param ubuf
 * @param len
 * @return
 */
static size_t ringbuf_write(const char __user *ubuf, size_t len)
{
	if (!rb.data || len == 0)
		return 0;

	if (len > rb.size) {
		ubuf += (len - rb.size);
		len = rb.size;
	}
	if (len > ringbuf_space()) {
		size_t excess = len - ringbuf_space();
		rb.read_pos = (rb.read_pos + excess) % rb.size;
		rb.count -= excess;
	}

	size_t to_write = min(len, rb.size - rb.write_pos);
	if (copy_from_user(rb.data + rb.write_pos, ubuf, to_write))
		return 0;

	size_t part2 = len - to_write;
	if (part2) {
		if (copy_from_user(rb.data, ubuf + to_write, part2))
			return 0;
	}

	rb.write_pos = (rb.write_pos + len) % rb.size;
	rb.count += len;
	return len;
}

/**
 * Чтение из буфера
 * @param ubuf
 * @param len
 * @return
 */
static size_t ringbuf_peek(char __user *ubuf, size_t len)
{
	size_t avail, part1, part2;

	if (!rb.data || ringbuf_empty())
		return 0;

	avail = min(len, rb.count);
	if (avail == 0)
		return 0;

	part1 = min(avail, rb.size - rb.read_pos);
	if (copy_to_user(ubuf, rb.data + rb.read_pos, part1))
		return 0;

	part2 = avail - part1;
	if (part2 && copy_to_user(ubuf + part1, rb.data, part2))
		return 0;

	return avail;
}

/**
 * Удаление прочитанных данных
 * @param len
 */
static void ringbuf_discard(size_t len)
{
	if (!rb.data || len > rb.count)
		return;
	rb.read_pos = (rb.read_pos + len) % rb.size;
	rb.count -= len;
}


static int dev_open(struct inode *inode, struct file *filp)
{
	if (!rb.data) {
		pr_err("Buffer not allocated\n");
		return -ENOMEM;
	}
	pr_debug("Device opened\n");
	return 0;
}

static int dev_release(struct inode *inode, struct file *filp)
{
	pr_debug("Device closed\n");
	return 0;
}

static ssize_t dev_read(struct file *filp, char __user *ubuf,
			size_t len, loff_t *off)
{
	ssize_t ret;

	if (mutex_lock_interruptible(&rb_mutex))
		return -ERESTARTSYS;

	ret = (ssize_t)ringbuf_peek(ubuf, len);
	if (ret > 0) {
		ringbuf_discard(ret);
		*off += ret;
	}

	mutex_unlock(&rb_mutex);
	return ret;
}

static ssize_t dev_write(struct file *filp, const char __user *ubuf,
			 size_t len, loff_t *off)
{
	ssize_t written;

	if (mutex_lock_interruptible(&rb_mutex))
		return -ERESTARTSYS;

	written = (ssize_t)ringbuf_write(ubuf, len);
	mutex_unlock(&rb_mutex);
	return written;
}


static loff_t dev_llseek(struct file *filp, loff_t off, int whence)
{
	loff_t newpos;
	size_t cnt;

	if (mutex_lock_interruptible(&rb_mutex))
		return -ERESTARTSYS;

	cnt = rb.count;
	switch (whence) {
	case SEEK_SET:
		newpos = off;
		break;
	case SEEK_CUR:
		newpos = filp->f_pos + off;
		break;
	case SEEK_END:
		newpos = cnt + off;
		break;
	default:
		mutex_unlock(&rb_mutex);
		return -EINVAL;
	}

	if (newpos < 0) {
		mutex_unlock(&rb_mutex);
		return -EINVAL;
	}
	if ((size_t)newpos > cnt)
		newpos = cnt;

	filp->f_pos = newpos;
	mutex_unlock(&rb_mutex);
	return newpos;
}

static const struct file_operations ringbuf_fops = {
	.owner		= THIS_MODULE,
	.open		= dev_open,
	.release	= dev_release,
	.read		= dev_read,
	.write		= dev_write,
	.llseek		= dev_llseek,
};


static int __init ringbuf_init(void)
{
	int ret;

	/* Выделение буфера */
	rb.data = kmalloc(rb.size, GFP_KERNEL);
	if (!rb.data) {
		pr_err("Failed to allocate buffer\n");
		return -ENOMEM;
	}
	rb.count = 0;
	rb.read_pos = 0;
	rb.write_pos = 0;

	/* Регистрация символьного устройства */
	ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
	if (ret < 0) {
		pr_err("alloc_chrdev_region failed\n");
		goto err_buf;
	}

	cdev_init(&my_cdev, &ringbuf_fops);
	my_cdev.owner = THIS_MODULE;
	ret = cdev_add(&my_cdev, dev_num, 1);
	if (ret < 0) {
		pr_err("cdev_add failed\n");
		goto err_region;
	}

	my_class = class_create(CLASS_NAME);
	if (IS_ERR(my_class)) {
		ret = PTR_ERR(my_class);
		pr_err("class_create failed\n");
		goto err_cdev;
	}

	my_device = device_create(my_class, NULL, dev_num, NULL, DEVICE_NAME);
	if (IS_ERR(my_device)) {
		ret = PTR_ERR(my_device);
		pr_err("device_create failed\n");
		goto err_class;
	}

	pr_info("Ring buffer driver loaded (size=%zu)\n", rb.size);
	return 0;

err_class:
	class_destroy(my_class);
err_cdev:
	cdev_del(&my_cdev);
err_region:
	unregister_chrdev_region(dev_num, 1);
err_buf:
	kfree(rb.data);
	return ret;
}

static void __exit ringbuf_exit(void)
{
	device_destroy(my_class, dev_num);
	class_destroy(my_class);
	cdev_del(&my_cdev);
	unregister_chrdev_region(dev_num, 1);
	kfree(rb.data);
	pr_info("Ring buffer driver unloaded\n");
}

module_init(ringbuf_init);
module_exit(ringbuf_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("AP");
MODULE_DESCRIPTION("Ring buffer character device driver");

