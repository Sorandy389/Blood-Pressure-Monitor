#include <mbed.h>

// Transfer Function B, 2.5% - 22.5% of 2^24
float output_max = 3774873.6;	// 22.5%
float output_min = 419430.4;	// 2.5%

// Gage sensor with 0 mmHg to 300 mmHg 
uint32_t P_max = 300;
uint32_t P_min = 0;

SPI spi(PF_9, PF_8, PF_7); // mosi, miso, sclk
DigitalOut cs(PC_1);

float getPressure(void) {
	uint32_t raw_data[4];
	uint32_t output = 0;
	// using an Output Measurement Command of “0xAA”, followed by “0x00”, “0x00”
	// Select the device by seting chip select low 
	cs = 0;
	spi.write(0xAA);
	spi.write(0x00);
	spi.write(0x00);
	// wait for at least 5ms
	thread_sleep_for(10);
	// deselect the sensor
	cs = 1;

	// To read the 24-bit pressure output along with the 8-bit Status Byte
	// Select the device by seting chip select low 
	cs = 0;
	raw_data[0] = spi.write(0xF0);
	raw_data[1] = spi.write(0x00);
	raw_data[2] = spi.write(0x00);
	raw_data[3] = spi.write(0x00);
	// wait for at least 5ms
	thread_sleep_for(10);
	// deselect the sensor
	cs = 1;

	output = (raw_data[1]<<16) | (raw_data[2]<<8) | (raw_data[3]);

	// Pressure Output Function
	return (output - output_min)*(P_max - P_min)/(output_max - output_min) + P_min;
}

int main() {
    // Chip must be deselected
    cs = 1;
    
	// Calculated pressure value
	float pressure = 0.0;
	float last_pressure = 0.0; // record the last pressure
	float pressure_diff = 0,0;
    // Setup the spi for 8 bit data
    // MPR Series SPI sensors are configured for SPI operation in mode 0 
    // with a 100KHz clock rate
    spi.format(8,0);
    spi.frequency(100'000);
	bool starting_flag = false;
	vector<float> data; // record the blood pressure change
	while(1) {
		// Update the pressure
		last_pressure = pressure; // update last pressure
		pressure = getPressure();
		printf("The current Pressure is %f\n", pressure);
		thread_sleep_for(1000);
		if(pressure>=150) {
			starting_flag = true;
		}
		if(pressure<=30 & starting_flag) {
			starting_flag = false;
			// Analyse data 


			printf("Systolic Blood pressure is: ");
			printf("Diastolic Blood pressure is: ");
			printf("Heart Rate is: ");
			data.erase(); // erase all the date for the next experiment
		}
		if(starting_flag) { // start the experiment
			pressure_diff = pressure - pressure;
			if (pressure_diff>=5) {
				printf("warnning, the release rate is too fast!");
			} else if(pressure_diff<=3) {
				printf("warnning, the release rate is too slow!");
			}
			data.push_back(pressure_diff);
		}
		// if(pressure < 150) {
		// 	// wait for 1000ms if the pressure does not meet the threshold
		// 	thread_sleep_for(1000);
		// 	continue;
		// } else {
		// 	// start sampling after reaching 150mmHg
		// }
	}

}
