#include <kernel/vm.h>
#include <kernel/vpmap.h>
#include <kernel/console.h>
#include <kernel/io.h>
#include <lib/string.h>
#include <kernel/cga.h>

#define REG_INDEX   0x3D4
#define REG_DATA    0x3D5

#define INDEX_CURSOR_HI   0x0E
#define INDEX_CURSOR_LO   0x0F

#define ATB_NORMAL  0x0700
#define RES_X       80
#define RES_Y       25

static uint16_t *cga_mem = 0;

void
cga_init(void)
{
    kassert(kas != 0);
    kassert(kas->vpmap != 0);

    cga_mem = (uint16_t*)kmap_p2v(0xB8000);
}

void
cga_putc(int c)
{
    int cursor;

    if (cga_mem == 0) {
        // CGA not initialized yet
        return;
    }

    // Get current cursor position
    writeb(REG_INDEX, INDEX_CURSOR_LO);
    cursor = readb(REG_DATA);
    writeb(REG_INDEX, INDEX_CURSOR_HI);
    cursor |= readb(REG_DATA) << 8;

    switch (c) {
    case '\n':
        cursor += (RES_X - (cursor % RES_X));
        break;
    case BACKSPACE:
        if(cursor > 0) {
            --cursor;
        }
        break;
    default:
        cga_mem[cursor++] = c | ATB_NORMAL;
    }

    // Page scroll up
    if (cursor >= RES_X * RES_Y) {
        memmove(cga_mem, cga_mem + RES_X, sizeof(cga_mem[0]) * RES_X * (RES_Y - 1));
        cursor -= RES_X;
        memset(cga_mem + cursor, 0, sizeof(cga_mem[0]) * (RES_X * RES_Y - cursor));
    }

    // Update cursor position
    writeb(REG_INDEX, INDEX_CURSOR_LO);
    writeb(REG_DATA, cursor & 0xFF);
    writeb(REG_INDEX, INDEX_CURSOR_HI);
    writeb(REG_DATA, (cursor >> 8) & 0xFF);
}
