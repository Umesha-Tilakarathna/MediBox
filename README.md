Under the module In20-S4-EN2853 - Embedded Systems and Applications, developed a Medibox using an ESP32 to remind users to take their medicine on time. Did microcontroller programming, and implemented IoT through NodeRED. The project was implemented in Wokwi. 

The simulation includes the following functionality.

 1. A menu that provides the following options.
 (a) Set time zone by taking the offset from UTC as input.
 (b) Set 3 alarms.
 (c) Disable all alarms.
 2. Fetch the time in the selected time zone from the NTP server over Wi-Fi. Display the current time
 on the OLED.
 3. Ring the alarm with proper indication when the set alarm times have been reached.
 4. Stop the alarm using a push button.
 5. Monitor temperature and humidity levels and provide warnings using proper indication when either
 or both temperature and humidity have exceeded healthy limits.
 Note: Healthy Temperature : 26 ◦C ≤ Temperature ≤ 32 ◦C
 Note: Healthy Humidity : 60% ≤ Humidity ≤ 80%


 • Reading the intensity of light using an LDR and displaying in the Node-RED dashboard.
 • A shaded sliding window has been installed to prevent excessive light from entering the Medibox.– The shaded sliding window
 is connected to a servo motor responsible for adjusting the light
 intensity entering the Medibox. The motor can adjust its angle between 0-180 degrees based
 on the lighting conditions. This enables the system to dynamically regulate the amount of
 light entering the Medibox to ensure optimal storage conditions for sensitive medicines.

Next stage:
 To adjust the system’s minimum angle and the controlling factor, two slider controls 
 were used in the Node-RED dashboard. The first slider control ranges from 0
 to 120, allowing the user to adjust the minimum angle of the shaded sliding window as needed.
 The second slider control should ranges from 0 to 1, enabling the user to adjust the controlling
 factor used to calculate the motor angle.
