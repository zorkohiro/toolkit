/*
 * Standalone temperature logger
 */
#include <stdio.h>
#include <time.h>
#include "pcsensor.h"

/* Calibration adjustments */
/* See http://www.pitt-pladdy.com/blog/_20110824-191017_0100_TEMPer_under_Linux_perl_with_Cacti/ */
static float scale = 1.0287;
static float offset = -0.85;

int main(){
	int success = 0;
	float tempc = 0.0000;
    int i;

	for (i = 0; i < 5; i++) {
		usb_dev_handle *lvr_winusb = pcsensor_open();

		if (!lvr_winusb) {
			/* Open fails sometime, sleep and try again */
			sleep(1);
		} else {
			tempc = pcsensor_get_temperature(lvr_winusb);
			pcsensor_close(lvr_winusb);
	        /* Read fails silently with a 0.0 return */
	        if (tempc > -0.0001 && tempc < 0.0001) {
                sleep(1);
                continue;
            }
            success = 1;
            break;
		}
	}
    if (success == 0)
        return 1;

    /* Apply calibrations */
    tempc = (tempc * scale) + offset;

    time_t t = time(NULL);

    printf("%lu %4.2f %4.2f\n", t, tempc, (tempc * 9.0/5.0) + 32.0);
    return 0;
}
