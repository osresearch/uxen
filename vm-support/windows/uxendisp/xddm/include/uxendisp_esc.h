
#ifndef _UXENDISP_ESC_H_
#define _UXENDISP_ESC_H_

/* Escape code: GDI->display driver */
enum {
    UXENDISP_ESCAPE_SET_CUSTOM_MODE = 0x10001,
    UXENDISP_ESCAPE_SET_VIRTUAL_MODE = 0x10002,
    UXENDISP_ESCAPE_IS_VIRT_MODE_ENABLED = 0x10003,
};

typedef struct {
    int esc_code;
    unsigned long width;
    unsigned long height;
    /* bpp ? */
} UXENDISPCustomMode;

#endif /* _UXENDISP_ESC_H_ */
