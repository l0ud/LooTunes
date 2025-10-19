.syntax	unified
.arch	armv6-m

.global Pre_Reset_Handler
.type Pre_Reset_Handler, %function

Pre_Reset_Handler:
    /* Workaround to py32 mcu sometimes setting stack incorrectly after watchdog reset */
    ldr     r0, =_estack
    mov     sp, r0

    bl      Reset_Handler
    b       .