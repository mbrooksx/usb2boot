#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <ctype.h>

#include <linux/usbdevice_fs.h>
#include <linux/usbdevice_fs.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 20)
#include <linux/usb/ch9.h>
#else
#include <linux/usb_ch9.h>
#endif
#include <asm/byteorder.h>

#include "usb.h"

//#define TRACE_USB 1
#if TRACE_USB
#define DBG1(x...) fprintf(stderr, x)
#define DBG(x...) fprintf(stderr, x)
#else
#define DBG(x...)
#define DBG1(x...)
#endif

struct usb_handle 
{
    char fname[64];
    int desc;
    unsigned char ep_in;
    unsigned char ep_out;
};

static inline int badname(const char *name)
{
    while(*name) {
        if(!isdigit(*name++)) return 1;
    }
    return 0;
}

static int check(void *_desc, int len, unsigned type, int size)
{
    unsigned char *desc = _desc;
    
    if(len < size) return -1;
    if(desc[0] < size) return -1;
    if(desc[0] > len) return -1;
    if(desc[1] != type) return -1;
    
    return 0;
}

static int filter_usb_device(char *ptr, int len, ifc_match_func callback)
{
    struct usb_device_descriptor *dev;
    struct usb_ifc_info info;
    
    if(check(ptr, len, USB_DT_DEVICE, USB_DT_DEVICE_SIZE))
        return 0;
    dev = (void*) ptr;
    
    info.dev_vendor = dev->idVendor;
    info.dev_product = dev->idProduct;
    info.dev_class = dev->bDeviceClass;
    info.dev_subclass = dev->bDeviceSubClass;
    info.dev_protocol = dev->bDeviceProtocol;

    if(callback(&info)) {
       return 1;
    }

    return 0;
}

static int find_usb_device(const char *base, ifc_match_func callback)
{
    int ret = 0;
    int n;
    char busname[64], devname[64];
    char desc[1024];
    
    DIR *busdir, *devdir;
    struct dirent *de;
    int fd;
    
    busdir = opendir(base);
    if(busdir == 0) return 0;

    while((de = readdir(busdir)) && (ret == 0)) {
        if(badname(de->d_name)) continue;
        
        sprintf(busname, "%s/%s", base, de->d_name);
        devdir = opendir(busname);
        if(devdir == 0) continue;
        
        DBG("[ scanning %s ]\n", busname);
        while((de = readdir(devdir)) && (ret == 0)) {
            
            if(badname(de->d_name)) continue;
            sprintf(devname, "%s/%s", busname, de->d_name);

            DBG("[ scanning %s ]\n", devname);
            if((fd = open(devname, O_RDONLY)) < 0) {
                continue;
            }

            n = read(fd, desc, sizeof(desc));
            
            if(filter_usb_device(desc, n, callback)){
                ret = 1;
                continue;
            } else {
                close(fd);
            }
        }
        closedir(devdir);
    }
    closedir(busdir);
    return ret;
}

int usb_close(usb_handle *h)
{
    int fd;
    
    fd = h->desc;
    h->desc = -1;
    if(fd >= 0) {
        close(fd);
        DBG("[ usb closed %d ]\n", fd);
    }

    return 0;
}

int usb_find(ifc_match_func callback)
{
    return find_usb_device("/dev/bus/usb", callback);
}


