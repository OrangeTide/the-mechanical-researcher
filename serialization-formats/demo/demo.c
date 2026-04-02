#include <stdio.h>
#include "sensor.h"

static void annotate(const uint8_t *buf, int len)
{
    static const char *wname[] = {"8-bit", "16-bit", "32-bit", "64-bit"};
    static const int wsz[] = {1, 2, 4, 8};
    int pos, end, field, wire, sz, i;
    uint32_t val;

    if (len < 2) return;
    end = (buf[0] | (buf[1] << 8)) + 2;
    printf("  %02x %02x              length = %d\n", buf[0], buf[1], end - 2);

    pos = 2;
    while (pos < end) {
        field = buf[pos] >> 3;
        wire = buf[pos] & 7;
        if (wire > 3) break;
        sz = wsz[wire];
        printf("  %02x", buf[pos]);
        val = 0;
        for (i = 0; i < sz; i++) {
            printf(" %02x", buf[pos + 1 + i]);
            if (i < 4)
                val |= (uint32_t)buf[pos + 1 + i] << (i * 8);
        }
        for (i = sz; i < 4; i++)
            printf("   ");
        printf("   field %d (%s) = %u\n", field, wname[wire], val);
        pos += 1 + sz;
    }
}

int main(void)
{
    uint8_t buf[64];
    int n;

    /* encode a combo sensor reading (temperature + humidity) */
    struct sensor_reading tx = {
        .sensor_id = 7,
        .timestamp = 1712000000,
        .kind = SENSOR_TYPE_COMBO,
        .temperature = 234,   /* 23.4 C in tenths */
        .humidity = 655,      /* 65.5% in tenths */
    };

    n = sensor_reading_encode(&tx, buf, sizeof(buf));
    printf("SensorReading (%d bytes on wire):\n", n);
    annotate(buf, n);

    /* decode it back */
    struct sensor_reading rx = {0};
    sensor_reading_decode(&rx, buf, n);
    printf("  decoded: id=%d ts=%u kind=%d temp=%d hum=%d\n\n",
        rx.sensor_id, rx.timestamp, rx.kind,
        rx.temperature, rx.humidity);

    /* encode a device info message */
    struct device_info info = {
        .device_id = 0x1234,
        .firmware_major = 2,
        .firmware_minor = 1,
        .uptime_secs = 86400,
    };

    n = device_info_encode(&info, buf, sizeof(buf));
    printf("DeviceInfo (%d bytes on wire):\n", n);
    annotate(buf, n);

    struct device_info info2 = {0};
    device_info_decode(&info2, buf, n);
    printf("  decoded: id=0x%04x fw=%d.%d uptime=%u\n",
        info2.device_id, info2.firmware_major,
        info2.firmware_minor, info2.uptime_secs);

    return 0;
}
