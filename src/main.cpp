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
	int heart_rate = 0; // heart rate
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
		thread_sleep_for(80);
		if(pressure>=170 && pressure - last_pressure<0 && !starting_flag) {  // pressure reaches 150, and start to releasing pressure
			starting_flag = true;  // start the experiment
			printf("experiment starts and stop increase the pressure./n");
			thread_sleep_for(2000);
		} else if(pressure<=30 && starting_flag) {  // end phase
			starting_flag = false;
			// Analyze data 
			float SBP_diff = max_pressure_diff*0.55; // using Om to calculate Os https://patents.google.com/patent/CN102018507A/en
			float DBP_diff = max_pressure_diff*0.82; // using Om to calculate Od https://patents.google.com/patent/CN102018507A/en

			float error_gap = 1000.0;
			for(int i=index-1;i>=0;i--) { // staring from MAP, find the index of Os and get the corresponded pressure data
				if(diff_data[i] > 0) { // find the closest index of Os
					float diff = data[i] - SBP_diff;
					if (diff < 0) {
						diff = -diff;
					}
					if (diff < error_gap) {
						SBP = data[i]; // get the corresponded pressure
						error_gap = diff;
					}
				}
			}
			error_gap = 1000.0;
			for(int i=index+1;i<diff_data.size()-1;i++) { // staring from MAP, find the index of Od and get the corresponded pressure data
				if(diff_data[i] > 0) { // find the closest index of Od
					float diff = data[i] - DBP_diff;
					if (diff < 0) {
						diff = -diff;
					}
					if (diff < error_gap) {
						DBP = data[i]; // get the corresponded pressure
						error_gap = diff;
					}
				}
			}
			// heart rate
			int heart_count = 0;
			int heart_index = 0;
			for(int i = index+2;i<diff_data.size()-1;i++) {
				if(diff_data[i] > max_pressure_diff*0.2) { // find the heart beats
					heart_count++;
					heart_index = i;
					if(heart_count>=10) {
						break;
					}
				}
			}
			// calculate the heart rate (beats/min)
			heart_rate = (int) heart_count*60/((heart_index-index)*0.1);
			// print result
			printf("Mean arterial pressure is: %f.\n",MAP);
			printf("Systolic Blood pressure is: %f.\n",SBP);
			printf("Diastolic Blood pressure is: %f.\n",DBP);
			printf("Heart Rate is: %d.\n", heart_rate);

			// Analyze blood pressure, ref:https://www.heart.org/en/health-topics/high-blood-pressure/understanding-blood-pressure-readings
			if(SBP>=180||DBP>=120) {
				printf("This stage of high blood pressure requires medical attention. If your blood pressure readings suddenly exceed 180/120 mm Hg, wait five minutes and then test your blood pressure again. If your readings are still unusually high, contact your doctor immediately. You could be experiencing a hypertensive crisis.\n");
			} else if(SBP>=140||DBP>=90) {
				printf("Hypertension Stage 2 is when blood pressure consistently ranges at 140/90 mm Hg or higher. At this stage of high blood pressure, doctors are likely to prescribe a combination of blood pressure medications and lifestyle changes.\n");
			} else if(SBP>=130||DBP>=80) {
				printf("Hypertension Stage 1 is when blood pressure consistently ranges from 130-139 systolic or 80-89 mm Hg diastolic. At this stage of high blood pressure, doctors are likely to prescribe lifestyle changes and may consider adding blood pressure medication based on your risk of atherosclerotic cardiovascular disease (ASCVD), such as heart attack or stroke.\n");
			} else if(SBP>=120 && DBP<80) {
				printf("Elevated blood pressure is when readings consistently range from 120-129 systolic and less than 80 mm Hg diastolic. People with elevated blood pressure are likely to develop high blood pressure unless steps are taken to control the condition.\n");
			} else {
				printf("Blood pressure numbers of less than 120/80 mm Hg are considered within the normal range. If your results fall into this category, stick with heart-healthy habits like following a balanced diet and getting regular exercise.\n");
			}
			data.clear(); // clear data for the next experitment
			diff_data.clear();
			max_pressure_diff = 0.0;
			MAP = 0.0;
			SBP = 0.0;
			DBP = 0.0;
			heart_rate = 0;
			counter = 0;
			index = 0;
			avg_pressure_diff = 0.0;
			// time for observe the result
			thread_sleep_for(30000);
		} else if(starting_flag) { // start the experiment
			pressure_diff = pressure - last_pressure; // record the pressure difference between current and last
			diff_data.push_back(pressure_diff); // record the difference
			data.push_back(pressure);  // record current pressure
			if(pressure_diff>max_pressure_diff) {
				max_pressure_diff = pressure_diff; // update max pressure diff
				MAP = pressure; // update MAP
				index = counter; // index of MAP
			}
			avg_pressure_diff += pressure_diff;
			counter ++; // update counter
			if(counter%10==0) { // test release rate once per second
				printf("The current Pressure is %f.\n", pressure);

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
	}

}
