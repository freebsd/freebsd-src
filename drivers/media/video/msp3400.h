#ifndef MSP3400_H
#define MSP3400_H

/* ---------------------------------------------------------------------- */

struct msp_dfpreg {
    int reg;
    int value;
};

#define MSP_SET_DFPREG     _IOW('m',15,struct msp_dfpreg)
#define MSP_GET_DFPREG     _IOW('m',16,struct msp_dfpreg)

#endif /* MSP3400_H */
