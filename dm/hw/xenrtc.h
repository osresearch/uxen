
#ifndef _XENRTC_H_
#define _XENRTC_H_

void rtc_set_memory(ISADevice *dev, int addr, int val);

#define RTC_REG_EQUIPMENT_BYTE 0x14

#endif  /* _XENRTC_H_ */
