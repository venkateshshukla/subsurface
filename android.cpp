/* implements Android specific functions */
#include "dive.h"
#include "display.h"
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>

#include <QtAndroidExtras/QtAndroidExtras>
#include <QtAndroidExtras/QAndroidJniObject>
#include <QtAndroid>

#define FTDI_VID 0x0403
#define USB_SERVICE "usb"

extern "C" {

const char system_divelist_default_font[] = "Roboto";
const int system_divelist_default_font_size = 8;

const char *system_default_filename(void)
{
	/* Replace this when QtCore/QStandardPaths getExternalStorageDirectory landed */
	QAndroidJniObject externalStorage = QAndroidJniObject::callStaticObjectMethod("android/os/Environment", "getExternalStorageDirectory", "()Ljava/io/File;");
	QAndroidJniObject externalStorageAbsolute = externalStorage.callObjectMethod("getAbsolutePath", "()Ljava/lang/String;");
	QString system_default_filename = externalStorageAbsolute.toString() + "/subsurface.xml";
	QAndroidJniEnvironment env;
	if (env->ExceptionCheck()) {
		// FIXME: Handle exception here.
		env->ExceptionClear();
		return strdup("/sdcard/subsurface.xml");
	}
	return strdup(system_default_filename.toUtf8().data());
}

int enumerate_devices(device_callback_t callback, void *userdata, int dc_type)
{
	/* FIXME: we need to enumerate in some other way on android */
	/* qtserialport maybee? */
	return -1;
}

/**
 * Get the file descriptor of first available ftdi device attached to usb in android.
 * This is needed for dc_device_open on android.
 *
 * return
 *	-1 : No Usb Device is attached.
 *	-2 : No ftdi device found.
 *	-3 : No permission for using the device
 *	-4 : Error while opening the device.
 * +ve num : Successfully extracted file descriptor is returned.
 * */
int get_usb_fd()
{
	int i;
	jint fd, vendorid, productid;
	QAndroidJniObject usbName, usbDevice;

	// Get the current main activity of the application.
	QAndroidJniObject activity = QtAndroid::androidActivity();

	QAndroidJniObject usb_service = QAndroidJniObject::fromString(USB_SERVICE);

	// Get UsbManager from activity
	QAndroidJniObject usbManager = activity.callObjectMethod("getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;", usb_service.object());

	// Get a HashMap<Name, UsbDevice> of all USB devices attached to Android
	QAndroidJniObject deviceMap = usbManager.callObjectMethod("getDeviceList", "()Ljava/util/HashMap;");
	jint num_devices = deviceMap.callMethod<jint>("size", "()I");
	if (num_devices == 0) {
		// No USB device is attached.
		return -1;
	}

	// Iterate over all the devices and find the first available FTDI device.
	QAndroidJniObject keySet = deviceMap.callObjectMethod("keySet", "()Ljava/util/Set;");
	QAndroidJniObject iterator = keySet.callObjectMethod("iterator", "()Ljava/util/Iterator;");

	for (i = 0; i < num_devices; i++) {
		usbName = iterator.callObjectMethod("next", "()Ljava/lang/Object;");
		usbDevice = deviceMap.callObjectMethod ("get", "(Ljava/lang/Object;)Ljava/lang/Object;", usbName.object());
		vendorid = usbDevice.callMethod<jint>("getVendorId", "()I");
		productid = usbDevice.callMethod<jint>("getProductId", "()I");
		if(vendorid == FTDI_VID) // Found a FTDI device. Break.
			break;
	}
	if (i == num_devices) {
		// No ftdi device found.
		return -2;
	}

	jboolean hasPermission = usbManager.callMethod<jboolean>("hasPermission", "(Landroid/hardware/usb/UsbDevice;)Z", usbDevice.object());
	if (!hasPermission) {
		// You do not have permission to use the usbDevice.
		// Please remove and reinsert the USB device.
		// Could also give an dialogbox asking for permission.
		return -3;
	}

	// An FTDI device is present and we also have permission to use the device.
	// Open the device and get its file descriptor.
	QAndroidJniObject usbDeviceConnection = usbManager.callObjectMethod("openDevice", "(Landroid/hardware/usb/UsbDevice;)Landroid/hardware/usb/UsbDeviceConnection;", usbDevice.object());
	if (usbDeviceConnection.object() == NULL) {
		// Some error occurred while opening the device. Exit.
		return -4;
	}

	// Finally get the required file descriptor.
	fd = usbDeviceConnection.callMethod<jint>("getFileDescriptor", "()I");
	if (fd == -1) {
		// The device is not opened. Some error.
		return -4;
	}
	return fd;
}

/* NOP wrappers to comform with windows.c */
int subsurface_rename(const char *path, const char *newpath)
{
	return rename(path, newpath);
}

int subsurface_open(const char *path, int oflags, mode_t mode)
{
	return open(path, oflags, mode);
}

FILE *subsurface_fopen(const char *path, const char *mode)
{
	return fopen(path, mode);
}

void *subsurface_opendir(const char *path)
{
	return (void *)opendir(path);
}

struct zip *subsurface_zip_open_readonly(const char *path, int flags, int *errorp)
{
	return zip_open(path, flags, errorp);
}

int subsurface_zip_close(struct zip *zip)
{
	return zip_close(zip);
}

/* win32 console */
void subsurface_console_init(bool dedicated)
{
	/* NOP */
}

void subsurface_console_exit(void)
{
	/* NOP */
}
}
