#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/hid.h>
#include <linux/usb.h>
#include <linux/power_supply.h>

#define SCE_USB_VENDOR_ID 0x12BA
#define SCE_USB_PLATINUM_PRODUCT_ID 0x0050
#define SCE_USB_GOLD_PRODUCT_ID 0x0035

#define REPORT_SIZE 8

struct hid_sce_headset_battery {
    struct hid_device *hdev;
    struct power_supply *psy;
    struct power_supply_desc psy_desc;

    int present;
    int capacity;
    uint8_t flags01;
};

static enum power_supply_property hid_sce_headset_props[] = {
    POWER_SUPPLY_PROP_PRESENT,
    POWER_SUPPLY_PROP_CAPACITY,
    POWER_SUPPLY_PROP_CAPACITY_LEVEL,
    POWER_SUPPLY_PROP_MANUFACTURER,
    POWER_SUPPLY_PROP_MODEL_NAME,
    POWER_SUPPLY_PROP_SCOPE,
    POWER_SUPPLY_PROP_STATUS
};

const char* oem = "Sony";
const char* deviceNamePlatinum = "PlayStation Platinum Wireless Headset";
const char* deviceNameGold = "PlayStation Gold Wireless Headset";

static int hid_sce_headset_get_property(struct power_supply *psy,
                                enum power_supply_property psp,
                                union power_supply_propval *val)
{
    struct hid_sce_headset_battery *bat = power_supply_get_drvdata(psy);

    switch (psp) {
    case POWER_SUPPLY_PROP_PRESENT:
        val->intval = bat->present ? 1 : 0;
        return 0;

    case POWER_SUPPLY_PROP_CAPACITY:
        val->intval = bat->capacity == 0x80 ? 0 : bat->capacity;
        return 0;
    case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
        val->intval = 
            bat->capacity == 0x80 ? POWER_SUPPLY_CAPACITY_LEVEL_UNKNOWN
            : POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
        return 0;
    case POWER_SUPPLY_PROP_MANUFACTURER:
        val->strval = oem;
        return 0;
    case POWER_SUPPLY_PROP_MODEL_NAME:
        val->strval = 
            bat->flags01 == 0b01 ? deviceNameGold
            : deviceNamePlatinum;
        return 0;
    case POWER_SUPPLY_PROP_SCOPE:
        val->intval = POWER_SUPPLY_SCOPE_DEVICE;
        return 0;
    case POWER_SUPPLY_PROP_STATUS:
        val->intval = bat->capacity == 0x80 ? POWER_SUPPLY_STATUS_CHARGING : POWER_SUPPLY_STATUS_DISCHARGING;
        return 0;

    default:
        return -EINVAL;
    }
}

static int hid_sce_headset_raw_event(struct hid_device *hdev,
                             struct hid_report *report,
                             u8 *data, int size)
{
    struct hid_sce_headset_battery *bat = hid_get_drvdata(hdev);

    if (!bat)
        return 0;

    if (size < REPORT_SIZE)
        return 0;

    if (data[0] == 0xB0) {
        int level = data[3];
        uint8_t flags = data[4];

        const uint8_t FLAG_SCE_HEADSET_VSS_ENABLED = 0b1;
        const uint8_t FLAG_SCE_HEADSET_MUTE_ENABLED = 0b10;
        const uint8_t FLAG_SCE_HEADSET_DEVICE_CONNECTED = 0b1000;
        bat->flags01 = (flags & 0b11000000) >> 6;

        bat->present = (flags & FLAG_SCE_HEADSET_DEVICE_CONNECTED) != 0;

        //if (level <= 0x99) {
            bat->capacity = level;
        //}
        power_supply_changed(bat->psy);
    }

    return 0;
}

static int hid_sce_headset_probe(struct hid_device *hdev,
                         const struct hid_device_id *id)
{
    int ret = 0;
    struct hid_sce_headset_battery *bat;
    struct power_supply_config psy_cfg = {};

    bat = devm_kzalloc(&hdev->dev, sizeof(*bat), GFP_KERNEL);
    if (!bat)
        return -ENOMEM;

    hid_set_drvdata(hdev, bat);
    bat->hdev = hdev;
    bat->capacity = 0;

    ret = hid_parse(hdev);
    if (ret)
        return ret;

    ret = hid_hw_start(hdev, HID_CONNECT_DEFAULT);
    if (ret)
        return ret;

    bat->psy_desc.name = "hid-playstation-headset";
    bat->psy_desc.type = POWER_SUPPLY_TYPE_BATTERY;
    bat->psy_desc.properties = hid_sce_headset_props;
    bat->psy_desc.num_properties = ARRAY_SIZE(hid_sce_headset_props);
    bat->psy_desc.get_property = hid_sce_headset_get_property;

    psy_cfg.drv_data = bat;

    bat->psy = devm_power_supply_register(&hdev->dev,
                                          &bat->psy_desc,
                                          &psy_cfg);
    if (IS_ERR(bat->psy)) {
        hid_hw_stop(hdev);
        return PTR_ERR(bat->psy);
    }

    dev_info(&hdev->dev, "hid-playstation-headset loaded\n");

    return 0;
}

static void hid_sce_headset_remove(struct hid_device *hdev)
{
    hid_hw_stop(hdev);
}

static const struct hid_device_id hid_sce_headset_devices[] = {
    { HID_USB_DEVICE(SCE_USB_VENDOR_ID, SCE_USB_PLATINUM_PRODUCT_ID) },
    { HID_USB_DEVICE(SCE_USB_VENDOR_ID, SCE_USB_GOLD_PRODUCT_ID) },
    { }
};
MODULE_DEVICE_TABLE(hid, hid_sce_headset_devices);

static struct hid_driver hid12ba_driver = {
    .name = "hid_playstation_headset",
    .id_table = hid_sce_headset_devices,
    .probe = hid_sce_headset_probe,
    .remove = hid_sce_headset_remove,
    .raw_event = hid_sce_headset_raw_event,
};

module_hid_driver(hid12ba_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("cntrpl");
MODULE_DESCRIPTION("Battery indicator driver for PlayStation wireless headsets"); 
