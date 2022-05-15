#include <mbed.h>
#include <vector>
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
	thread_sleep_for(6);
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
	thread_sleep_for(6);
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
	float pressure_diff = 0.0;
	float max_pressure_diff = 0.0; // record the max pressure diff, used to determine MAP
	float MAP = 0.0; // mean arterial pressure
	float SBP = 0.0; // systolic blood pressure
	float DBP = 0.0; // diastolic blood pressure
	std::vector<float> diff_data; // record the pressure diff
	std::vector<float> data; // record the pressure
	int counter = 0; // used to calculate release rate
	int index = 0; // index of MAP
	float avg_pressure_diff = 0.0; // the average release rate per second
    // Setup the spi for 8 bit data
    // MPR Series SPI sensors are configured for SPI operation in mode 0 
    // with a 100KHz clock rate
    spi.format(8,0);
    spi.frequency(100'000);
	bool starting_flag = false;
	while(1) {
		// Update the pressure
		last_pressure = pressure; // update last pressure
		pressure = getPressure();
		thread_sleep_for(88);
		if(pressure>=170 && pressure - last_pressure<0 && !starting_flag) {  // pressure reaches 150, and start to releasing pressure
			starting_flag = true;
			printf("experiment starts and stop increase the pressure./n");
			thread_sleep_for(2000);
		} else if(pressure<=30 && starting_flag) {
			starting_flag = false;
			// Analyze data 
			float SBP_diff = max_pressure_diff*0.55; // using Om to calculate Os https://patents.google.com/patent/CN102018507A/en
			float DBP_diff = max_pressure_diff*0.82; // using Om to calculate Od https://patents.google.com/patent/CN102018507A/en
			for(int i=index;i>=0;i--) {
				if((diff_data[i] > 0) && (diff_data[i]<SBP_diff)) {
					SBP = data[i];
					printf("SBP: %d\n",i);
					break;
				}
			}
			for(int i = index;i<diff_data.size()-1;i++) {
				if((diff_data[i] > 0) && (diff_data[i]<DBP_diff)) {
					DBP = data[i];
					printf("DBP: %d\n",i);
					break;
				}
			}
			printf("MAP: %d\n",index);
			//float sbp = 
			printf("Mean arterial pressure is: %f\n",MAP);
			printf("Systolic Blood pressure is: %f\n",SBP);
			printf("Diastolic Blood pressure is: %f\n",DBP);
			printf("Heart Rate is: \n");
			data.clear(); // clear data for the next experitment
			diff_data.clear();
			counter = 0;
			index = 0;
			// time for observe the result
			thread_sleep_for(10000);
		} else if(starting_flag) { // start the experiment
			pressure_diff = pressure - last_pressure;
			diff_data.push_back(pressure_diff);
			data.push_back(pressure);
			if(pressure_diff>max_pressure_diff) {
				max_pressure_diff = pressure_diff; // update max pressure diff
				MAP = pressure; // update MAP
				index = counter;
			}
			avg_pressure_diff += pressure_diff;
			counter ++; // update counter
			if(counter%10==0) { // test release rate once per second
				printf("The current Pressure is %f.\n", avg_pressure_diff);

				if(avg_pressure_diff>=-2) {
					printf("warnning, the release rate is too slow!\n");
				} else if(avg_pressure_diff<=-6) {
					printf("warnning, the release rate is too fast!\n");
				}
				avg_pressure_diff = 0.0;
			}
		} else {
			printf("Below the start pressure! The current pressure is %f.\n", pressure);
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
