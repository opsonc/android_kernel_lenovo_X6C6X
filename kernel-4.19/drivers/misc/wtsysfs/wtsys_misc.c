#include "wtsys_misc.h"

MISC_INFO(MISC_EMMC_SIZE,emmc_size);
MISC_INFO(MISC_RAM_SIZE,ram_size);

extern unsigned int msdc_get_capacity(int get_emmc_total);
extern char *get_emmc_name(void);

unsigned int round_kbytes_to_readable_mbytes(unsigned int k){
	unsigned int r_size_m = 0;
	unsigned int in_mega = k/1024;

	if (in_mega > 64*1024) { //if memory is larger than 64G
		r_size_m = 128*1024; // It should be 128G
	} else if (in_mega > 32*1024) {  //larger than 32G
		r_size_m = 64*1024; //should be 64G
	} else if (in_mega > 16*1024) {
		r_size_m = 32*1024;
	} else if (in_mega > 8*1024) {
		r_size_m = 16*1024;
	} else if (in_mega > 6*1024) {
		r_size_m = 8*1024;
	} else if (in_mega > 4*1024) {
		r_size_m = 6*1024;  //RAM may be 6G
	} else if (in_mega > 3*1024) {
		r_size_m = 4*1024;
	} else if (in_mega > 2*1024) {
		r_size_m = 3*1024; //RAM may be 3G
	} else if (in_mega > 1024) {
		r_size_m = 2*1024;
	} else if (in_mega > 512) {
		r_size_m = 1024;
	} else if (in_mega > 256) {
		r_size_m = 512;
	} else if (in_mega > 128) {
		r_size_m = 256;
	} else {
		k = 0;
	}
	return r_size_m;
}

ssize_t wt_emmcinfo(char * buf)
{
	ssize_t count = -1;
	struct file *pfile = NULL;
	mm_segment_t old_fs;
	loff_t pos;
	ssize_t ret = 0;

	unsigned long long Size_buf=0;
	char buf_size[mtk_emmc_len];
	memset(buf_size,0,sizeof(buf_size));

	pfile = filp_open(mtk_emmc,O_RDONLY,0);
	if (IS_ERR(pfile)) {
		goto ERR_0;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;

	ret = vfs_read(pfile, buf_size, mtk_emmc_len, &pos);
	if ( ret <= 0 ) {
		goto ERR_1;
	}

	Size_buf = simple_strtoull(buf_size,NULL,0);

	Size_buf>>=1; //Switch to KB


	count = sprintf(buf,"%dGB\n",round_kbytes_to_readable_mbytes((unsigned int)Size_buf)/1024);

ERR_1:

	filp_close(pfile, NULL);

	set_fs(old_fs);

	return count;

ERR_0:
	return count;

}

static struct attribute *wt_misc_attrs[] = {
	&misc_info_emmc_size.attr,
	&misc_info_ram_size.attr,
	NULL
};

#if 0
extern int wt_read_sn_from_otp(char *sn);
extern int wt_write_sn_to_otp(char *sn,unsigned int len);
#endif
#define SN_LEN (12) //for B6H Nikeh

static ssize_t wt_misc_show(struct kobject *kobj, struct attribute *a, char *buf){
	ssize_t count = 0;

	struct misc_info *mi = container_of(a, struct misc_info , attr);

	switch (mi->m_id) {
		case MISC_RAM_SIZE:
			{
#define K(x) ((x) << (PAGE_SHIFT - 10))
				struct sysinfo i;
				si_meminfo(&i);
				//count=sprintf(buf,"%u",(unsigned int)K(i.totalram));

				if(round_kbytes_to_readable_mbytes(K(i.totalram)) >= 1024) {
					count = sprintf(buf,"%dGB\n",round_kbytes_to_readable_mbytes(K(i.totalram))/1024);
				} else {
					count = sprintf(buf,"%dMB\n",round_kbytes_to_readable_mbytes(K(i.totalram)));
				}

			}
			break;
		case MISC_EMMC_SIZE:
			//count = sprintf(buf,"%dGB",round_kbytes_to_readable_mbytes(msdc_get_capacity(1)/2)/1024);
			count = wt_emmcinfo(buf);
			break;
#if 0    //#ifdef CONFIG_MTK_EMMC_SUPPORT_OTP
		case MISC_OTP_SN:
			{
				char temp[SN_LEN+1] = {0};
				int result = 0;

				int i=0;

				result = wt_read_sn_from_otp(temp);

				if(0 == result){
					//#if 0
					//check if alpha and num
					for(i=0; i<SN_LEN; i++){
						if(!isalnum(temp[i])){
							count = sprintf(buf,"Not Valid SN\n");
							goto r_error;
						}
					}
					//#endif
					count = sprintf(buf,"%s",temp);
				}else{
					count = sprintf(buf,"Read SN in OTP error %d\n",result);
				}
			}

			count = sprintf(buf,"SN in OTP not enabled\n");
#endif
			break;
		default:
			count = sprintf(buf,"Not support");
			break;
	}

	//r_error:
	return count;
}

static ssize_t wt_misc_store(struct kobject *kobj, struct attribute *a, const char *buf, size_t count){

	struct misc_info *mi = container_of(a, struct misc_info , attr);

	switch (mi->m_id) {
#if 0     //#ifdef CONFIG_MTK_EMMC_SUPPORT_OTP
		case MISC_OTP_SN:
			{
				char temp[SN_LEN+1] = {0};
				int result = 0;
				int i = 0;

				if(0 != strncmp(buf,"SN:=",4)){
					printk("[%s] invalid write sn command\n",__func__);
					break;
				}
				for(i=0;i<SN_LEN;i++){
					temp[i] = buf[i+4];
					if(('\n' == buf[i+4]) || ('\r' == buf[i+4])){
						temp[i] = 0;
						break;
					}
				}


				result = wt_write_sn_to_otp(temp,strlen(temp));
				if(0 != result)
					printk("[%s] called write error %d\n",__func__,result);

			}
			break;
#endif
		default:
			break;
	}
	return count;
}

/* wt_misc object */
static struct kobject wt_misc_kobj;
static const struct sysfs_ops wt_misc_sysfs_ops = {
	.show = wt_misc_show,
	.store = wt_misc_store,
};

/* wt_misc type */
static struct kobj_type wt_misc_ktype = {
	.sysfs_ops = &wt_misc_sysfs_ops,
	.default_attrs = wt_misc_attrs
};

static struct class  *wt_class;
static struct device *wt_hw_device;

int register_kboj_under_wtsysfs(struct kobject *kobj, struct kobj_type *ktype, const char *fmt, ...){
	return kobject_init_and_add(kobj, ktype, &(wt_hw_device->kobj), fmt);
}


static int __init create_misc(void){
	int ret;

	/* add kobject */
	ret = register_kboj_under_wtsysfs(&wt_misc_kobj, &wt_misc_ktype, WT_MISC_NAME);
	if (ret < 0) {
		pr_err("%s fail to add wt_misc_kobj\n",__func__);
		return ret;
	}
	return 0;
}

static int __init create_sysfs(void)
{
	/*  create class (device model) */
	wt_class = class_create(THIS_MODULE, WT_CLASS_NAME);
	if (IS_ERR(wt_class)) {
		pr_err("%s fail to create class\n",__func__);
		return -1;
	}

	wt_hw_device = device_create(wt_class, NULL, MKDEV(0, 0), NULL, WT_INTERFACE_NAME);
	if (IS_ERR(wt_hw_device)) {
		pr_warn("fail to create device\n");
		return -1;
	}

	return 0;
}

#define SDC_DETECT_STATUS "sd_tray_gpio_value"
//#define SDC_DETECT_GPIO  330
extern unsigned int cd_gpio;

static struct proc_dir_entry *sdc_detect_status = NULL;

static int sdc_detect_proc_show(struct seq_file *file, void* data)
{
	int gpio_value = -1;

	gpio_value = gpio_get_value(cd_gpio);
	seq_printf(file, "%d\n", gpio_value ? 1 : 0);
	return 0;
}

static int sdc_detect_proc_open (struct inode* inode, struct file* file)
{
	return single_open(file, sdc_detect_proc_show, inode->i_private);
}

static const struct file_operations sdc_detect_proc_fops =
{
	.open = sdc_detect_proc_open,
	.read = seq_read,
};

static int sdc_detect_init(void)
{
	sdc_detect_status = proc_create(SDC_DETECT_STATUS, 0644, NULL, &sdc_detect_proc_fops);
	if (sdc_detect_status == NULL)
	{
		pr_err("[%s]: create_proc_entry sdc_detect_status failed\n", __func__);
		return -1;
	}

	return 0;
}

static int __init wt_misc_sys_init(void)
{
	create_sysfs();
	/* create sysfs entry at /sys/class/wt_misc/interface/misc */
	create_misc();

	if (sdc_detect_init() < 0)
		pr_err("[%s]: create_proc_entry sdc_detect_proc failed\n", __func__);

	return 0;
}


late_initcall(wt_misc_sys_init);
MODULE_DESCRIPTION("wt Hardware Info Driver");
MODULE_LICENSE("GPL");
