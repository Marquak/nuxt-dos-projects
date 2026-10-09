#ifndef MATRIX_FPU_H
#define MATRIX_FPU_H

#define MATRIX_GLOW_STEPS 64

/* Initializes a responding 8087 or later x87. No WAIT is executed by the
 * probe, so an XT with an empty 8087 socket can safely return zero. */
int matrix_fpu_detect(void);

/* One-time circular glow table: 64 symmetric samples, each in 0..8.
 * Pass use_fpu != 0 only after matrix_fpu_detect() succeeds. The integer
 * implementation gives exactly the same table without x87 instructions. */
void matrix_glow_table(unsigned char out[MATRIX_GLOW_STEPS], int use_fpu);

#endif
