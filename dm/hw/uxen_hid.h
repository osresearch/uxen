#ifndef _UXEN_HID_
#define _UXEN_HID_

int uxenhid_send_mouse_report(uint8_t buttons, uint16_t x, uint16_t y,
                              int8_t wheel, int8_t hwheel);
int uxenhid_send_pen_report(uint16_t x, uint16_t y, uint8_t flags,
                            uint16_t pressure);
int uxenhid_send_touch_report(uint8_t contact_count, uint16_t contact_id,
                              uint16_t x, uint16_t y,
                              uint16_t width, uint16_t height,
                              uint8_t flags);

#endif /* _UXEN_HID_ */
